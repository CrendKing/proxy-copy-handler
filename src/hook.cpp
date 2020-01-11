#include "pch.h"
#include "hook.h"


constexpr char *REGISTRY_KEY_NAME = "Software\\ProxyCopyHandler";
constexpr char *REGISTRY_COPIER_PATH_VALUE_NAME = "CopierPath";
constexpr char *COPIER_CMDLINE_OPTIONS = " /force_close /no_confirm_del /error_stop";
constexpr static int EXECUTION_DELAY = 100;

CProxyCopyHook::CProxyCopyHook() {
    HKEY registryKey;

    if (RegCreateKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY_NAME, 0, nullptr, 0, KEY_QUERY_VALUE, nullptr, &registryKey, nullptr) == ERROR_SUCCESS) {
        char regValueData[MAX_PATH];
        DWORD regValueSize = MAX_PATH;
        if (RegGetValue(registryKey, nullptr, REGISTRY_COPIER_PATH_VALUE_NAME, RRF_RT_REG_SZ, nullptr, regValueData, &regValueSize) == ERROR_SUCCESS) {
            // remove the '\0'
            _copierCmdline.assign(regValueData, regValueSize - 1)
                .append(COPIER_CMDLINE_OPTIONS);
        }
        RegCloseKey(registryKey);
    }

    InitializeCriticalSection(&_cs);
}

CProxyCopyHook::~CProxyCopyHook() {
    DeleteCriticalSection(&_cs);
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

    const ExecutionKey key(wFunc, pszDestFile);

    EnterCriticalSection(&_cs);

    ExecutionMap::iterator iter = _pendingExecutions.find(key);
    UINT ret = IDNO;

    if (iter == _pendingExecutions.end()) {
        std::string sources;
        sources.append(" ").append(QuotePath(pszSrcFile));

        iter = _pendingExecutions.insert({ key, sources }).first;
        ExecutionParameter *param = new ExecutionParameter { this, iter };

        if (CreateThread(nullptr, 0, ExecutionThreadProc, param, 0, nullptr) == nullptr) {
            ret = IDYES;
        }
    } else {
        iter->second.append(" ").append(QuotePath(pszSrcFile));
    }

    LeaveCriticalSection(&_cs);
    return ret;
}

CProxyCopyHook::ExecutionKey::ExecutionKey(UINT func, PCSTR path)
    : operation(func) {
    strcpy_s(destination, MAX_PATH, path);

    if (destination[0] != '\0') {
        PathRemoveFileSpec(destination);
        PathAddBackslash(destination);
        PathQuoteSpaces(destination);
    }
}

bool CProxyCopyHook::ExecutionKey::operator==(const ExecutionKey &other) const {
    return operation == other.operation && strncmp(destination, other.destination, MAX_PATH) == 0;
}

std::size_t CProxyCopyHook::ExecutionKeyHasher::operator()(const ExecutionKey &key) const {
    return std::hash<UINT>{}(key.operation) ^ (std::hash<std::string>{}(key.destination) << 1);
}

DWORD WINAPI CProxyCopyHook::ExecutionThreadProc(LPVOID parameter) {
    const ExecutionParameter *param = (const ExecutionParameter *) parameter;
    const ExecutionKey &key = param->iter->first;
    const std::string &sources = param->iter->second;

    size_t prevSourceCount = 0;
    while (sources.size() != prevSourceCount) {
        prevSourceCount = sources.size();
        Sleep(EXECUTION_DELAY);
    }

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
        cmdlineOperation = "error";
        break;
    }

    std::string cmdline = param->instance->_copierCmdline;
    cmdline.append(" /cmd=")
        .append(cmdlineOperation);

    if (key.destination[0] != '\0') {
        cmdline.append(" /to=")
            .append(key.destination);
    }

    EnterCriticalSection(&param->instance->_cs);

    cmdline.append(sources);
    param->instance->_pendingExecutions.erase(param->iter);

    LeaveCriticalSection(&param->instance->_cs);
    delete param;

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcess(nullptr, const_cast<char *>(cmdline.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    return 0;
}

const char *CProxyCopyHook::QuotePath(PCSTR path) {
    if (path == '\0') {
        return path;
    }

    strcpy_s(_quotedPathBuffer, MAX_PATH, path);
    PathQuoteSpaces(_quotedPathBuffer);
    return _quotedPathBuffer;
}