#include "pch.h"
#include "hook.h"


static constexpr const WCHAR *REGISTRY_KEY_NAME = LR"(Software\ProxyCopyHandler)";
static constexpr const WCHAR *REGISTRY_COPIER_PATH_VALUE_NAME = L"CopierPath";
static constexpr const WCHAR *REGISTRY_COPIER_ARGS_VALUE_NAME = L"CopierArgs";
static int64_t QUEUE_SOURCES_DELAY_100_NS = -1000000;

static std::array<WCHAR, MAX_PATH> g_quotedPathBuffer {};

OBJECT_ENTRY_AUTO(__uuidof(CProxyCopyHook), CProxyCopyHook)

CProxyCopyHook::CProxyCopyHook() {
    HKEY registryKey;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY_NAME, 0, nullptr, 0, KEY_QUERY_VALUE, nullptr, &registryKey, nullptr) != ERROR_SUCCESS) {
        return;
    }

    std::array<WCHAR, 1024> regValueData;
    DWORD regValueSize;

    regValueSize = static_cast<DWORD>(regValueData.size());
    if (RegGetValueW(registryKey, nullptr, REGISTRY_COPIER_PATH_VALUE_NAME, RRF_RT_REG_SZ, nullptr, regValueData.data(), &regValueSize) != ERROR_SUCCESS) {
        return;
    }

    _copierCmdline = QuotePath(regValueData.data());

    regValueSize = static_cast<DWORD>(regValueData.size());
    if (RegGetValueW(registryKey, nullptr, REGISTRY_COPIER_ARGS_VALUE_NAME, RRF_RT_REG_SZ, nullptr, regValueData.data(), &regValueSize) != ERROR_SUCCESS) {
        return;
    }

    _copierCmdline.append(L" ").append(regValueData.data(), regValueSize / sizeof(WCHAR) - 1);  /* remove the '\0' */

    RegCloseKey(registryKey);
}

CProxyCopyHook::~CProxyCopyHook() {
    if (_workerThread.joinable()) {
        _stopWorker = true;
        _cv.notify_all();
        _workerThread.join();
    }

    if (_waitTp != nullptr) {
        CloseThreadpoolWait(_waitTp);
    }

    if (_waitEvent != nullptr) {
        CloseHandle(_waitEvent);
    }
}

auto STDMETHODCALLTYPE CProxyCopyHook::CopyCallback(HWND hwnd, UINT wFunc, UINT wFlags, PCWSTR pszSrcFile, DWORD dwSrcAttribs, PCWSTR pszDestFile, DWORD dwDestAttribs) -> UINT {
    if (_waitEvent == nullptr || _waitTp == nullptr) {
        return IDYES;
    }

    if (_copierCmdline.empty()) {
        return IDYES;
    }

    if (wFunc != FO_MOVE && wFunc != FO_COPY && wFunc != FO_DELETE) {
        return IDYES;
    }

    // Windows can move file without copying within the same volume (ref. MOVEFILE_COPY_ALLOWED)
    // FastCopy move is always copy-then-delete
    if (wFunc == FO_MOVE && PathGetDriveNumberW(pszSrcFile) == PathGetDriveNumberW(pszDestFile)) {
        return IDYES;
    }

    {
        const std::unique_lock lock(_mutex);

        const ExecutionKey execKey(wFunc, pszDestFile);
        _pendingExecutions[execKey].emplace(pszSrcFile);
    }

    SetThreadpoolWait(_waitTp, _waitEvent, reinterpret_cast<FILETIME *>(&QUEUE_SOURCES_DELAY_100_NS));

    return IDNO;
}

CProxyCopyHook::ExecutionKey::ExecutionKey(UINT func, PCWSTR dest)
    : operation(func) {
    if (dest[0] != L'\0') {
        std::array<WCHAR, MAX_PATH> path {};

        wcscpy_s(path.data(), path.size(), dest);
        PathRemoveFileSpecW(path.data());
        PathAddBackslashW(path.data());
        PathQuoteSpacesW(path.data());
        destination = path.data();
    }
}

auto CProxyCopyHook::ExecutionKey::Hasher::operator()(const ExecutionKey &key) const -> size_t {
    const size_t h1 = std::hash<UINT> {} (key.operation);
    const size_t h2 = std::hash<std::wstring> {} (key.destination);
    return h1 ^ (h2 << 1);
}

auto CALLBACK CProxyCopyHook::WaitCallback(PTP_CALLBACK_INSTANCE Instance,
                                           PVOID                 Context,
                                           PTP_WAIT              Wait,
                                           TP_WAIT_RESULT        WaitResult) -> void {
    if (CProxyCopyHook *hook = static_cast<CProxyCopyHook *>(Context); hook->_workerThread.joinable()) {
        hook->_cv.notify_one();
    } else {
        hook->_workerThread = std::thread(&CProxyCopyHook::WorkerProc, hook);
    }
}

auto CProxyCopyHook::QuotePath(PCWSTR path) -> const WCHAR * {
    if (path == nullptr) {
        return path;
    }

    wcscpy_s(g_quotedPathBuffer.data(), g_quotedPathBuffer.size(), path);
    PathQuoteSpacesW(g_quotedPathBuffer.data());
    return g_quotedPathBuffer.data();
}

auto CProxyCopyHook::WorkerProc() -> void {
    while (!_stopWorker) {
        std::unique_lock lock(_mutex);

        _cv.wait(lock, [this]() -> bool {
            return _stopWorker || !_pendingExecutions.empty();
        });

        if (_stopWorker) {
            continue;
        }

        auto [key, sources] = *_pendingExecutions.cbegin();
        _pendingExecutions.erase(_pendingExecutions.cbegin());
        lock.unlock();

        std::wstring sourceArg;
        for (const std::wstring &s : sources) {
            if (PathFileExistsW(s.c_str())) {
                sourceArg = std::format(L"{} {}", sourceArg, QuotePath(s.c_str()));
            }
        }

        if (sourceArg.empty()) {
            continue;
        }

        const WCHAR *cmdlineOperation;
        switch (key.operation) {
        case FO_MOVE:
            cmdlineOperation = L"move";
            break;
        case FO_COPY:
            cmdlineOperation = L"diff";
            break;
        case FO_DELETE:
            cmdlineOperation = L"delete";
            break;
        default:
            return;
        }

        std::wstring destinationArg;
        if (!key.destination.empty()) {
            destinationArg = std::format(L" /to={}", key.destination);
        }

        std::wstring cmdline = std::format(L"{} /cmd={}{} {}", _copierCmdline, cmdlineOperation, destinationArg, sourceArg);

        STARTUPINFOW si = { .cb = sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            if (key.operation == FO_DELETE) {
                for (const std::wstring &s : sources) {
                    SHChangeNotify(SHCNE_DELETE, SHCNF_PATHW, s.c_str(), nullptr);
                }
            } else {
                SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATHW, key.destination.c_str(), nullptr);
            }
        }
    }
}
