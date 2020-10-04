#include "pch.h"
#include "hook.h"


static constexpr char *REGISTRY_KEY_NAME = "Software\\ProxyCopyHandler";
static constexpr char *REGISTRY_COPIER_PATH_VALUE_NAME = "CopierPath";
static constexpr char *REGISTRY_COPIER_ARGS_VALUE_NAME = "CopierArgs";
static int64_t QUEUE_SOURCES_DELAY_100_NS = -1000000;

char CProxyCopyHook::_quotedPathBuffer[] {};

CProxyCopyHook::CProxyCopyHook()
    : _waitEvent(CreateEvent(nullptr, FALSE, FALSE, nullptr))
    , _waitTp(CreateThreadpoolWait(WaitCallback, this, nullptr))
    , _stopWorker(false) {
    HKEY registryKey;

    if (RegCreateKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY_NAME, 0, nullptr, 0, KEY_QUERY_VALUE, nullptr, &registryKey, nullptr) == ERROR_SUCCESS) {
        char regValueData[1024];
        DWORD regValueSize;

        regValueSize = sizeof(regValueData);
        if (RegGetValue(registryKey, nullptr, REGISTRY_COPIER_PATH_VALUE_NAME, RRF_RT_REG_SZ, nullptr, regValueData, &regValueSize) == ERROR_SUCCESS) {
            // remove the '\0'
            _copierCmdline.assign(regValueData, regValueSize - 1);
        }

        regValueSize = sizeof(regValueData);
        if (RegGetValue(registryKey, nullptr, REGISTRY_COPIER_ARGS_VALUE_NAME, RRF_RT_REG_SZ, nullptr, regValueData, &regValueSize) == ERROR_SUCCESS) {
            _copierCmdline.append(" ").append(regValueData, regValueSize - 1);
        }
        RegCloseKey(registryKey);
    }
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

STDMETHODIMP_(UINT) CProxyCopyHook::CopyCallback(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs) {
    if (_copierCmdline.empty()) {
        return IDYES;
    }

    if (wFunc != FO_MOVE && wFunc != FO_COPY && wFunc != FO_DELETE) {
        return IDYES;
    }

    // Windows can move file without copying within the same volume (ref. MOVEFILE_COPY_ALLOWED)
    // FastCopy move is always copy-then-delete
    if (wFunc == FO_MOVE && PathGetDriveNumber(pszSrcFile) == PathGetDriveNumber(pszDestFile)) {
        return IDYES;
    }

    if (_waitEvent == nullptr || _waitTp == nullptr) {
        return IDYES;
    }

    const ExecutionKey execKey(wFunc, pszDestFile);

    {
        const std::unique_lock lock(_mutex);
        _pendingExecutions[execKey].append(" ").append(QuotePath(pszSrcFile));
    }

    SetThreadpoolWait(_waitTp, _waitEvent, reinterpret_cast<FILETIME *>(&QUEUE_SOURCES_DELAY_100_NS));

    return IDNO;
}

CProxyCopyHook::ExecutionKey::ExecutionKey(UINT func, PCSTR dest)
    : operation(func) {
    if (dest[0] != '\0') {
        char path[MAX_PATH];

        strcpy_s(path, MAX_PATH, dest);
        PathRemoveFileSpec(path);
        PathAddBackslash(path);
        PathQuoteSpaces(path);
        destination = path;
    }
}

bool CProxyCopyHook::ExecutionKey::operator==(const ExecutionKey &other) const {
    return operation == other.operation && destination == other.destination;
}

size_t CProxyCopyHook::ExecutionKey::Hasher::operator()(const ExecutionKey &key) const {
    const size_t h1 = std::hash<UINT> {} (key.operation);
    const size_t h2 = std::hash<std::string> {} (key.destination);
    return h1 ^ (h2 << 1);
}

void CALLBACK CProxyCopyHook::WaitCallback(PTP_CALLBACK_INSTANCE Instance,
                                           PVOID                 Context,
                                           PTP_WAIT              Wait,
                                           TP_WAIT_RESULT        WaitResult) {
    CProxyCopyHook *hook = static_cast<CProxyCopyHook *>(Context);

    if (hook->_workerThread.joinable()) {
        hook->_cv.notify_one();
    } else {
        hook->_workerThread = std::thread(&CProxyCopyHook::WorkerProc, hook);
    }
}

const char *CProxyCopyHook::QuotePath(PCSTR path) {
    if (path == nullptr) {
        return path;
    }

    strcpy_s(_quotedPathBuffer, MAX_PATH, path);
    PathQuoteSpaces(_quotedPathBuffer);
    return _quotedPathBuffer;
}

void CProxyCopyHook::WorkerProc() {
    while (!_stopWorker) {
        std::unique_lock lock(_mutex);

        _cv.wait(lock, [this]() { return _stopWorker || !_pendingExecutions.empty(); });

        if (_stopWorker) {
            continue;
        }

        auto [key, sources] = *_pendingExecutions.cbegin();
        _pendingExecutions.erase(_pendingExecutions.cbegin());
        lock.unlock();

        const char *cmdlineOperation;
        switch (key.operation) {
        case FO_MOVE:
            cmdlineOperation = "move";
            break;
        case FO_COPY:
            cmdlineOperation = "diff";
            break;
        case FO_DELETE:
            cmdlineOperation = "delete";
            break;
        default:
            return;
        }

        std::string cmdline = _copierCmdline;
        cmdline.append(" /cmd=")
            .append(cmdlineOperation);

        if (!key.destination.empty()) {
            cmdline.append(" /to=")
                .append(key.destination);
        }

        cmdline.append(sources);

        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcess(nullptr, const_cast<char *>(cmdline.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
}
