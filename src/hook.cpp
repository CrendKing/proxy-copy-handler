#include "pch.h"
#include "hook.h"


static constexpr char *REGISTRY_KEY_NAME = "Software\\ProxyCopyHandler";
static constexpr char *REGISTRY_COPIER_PATH_VALUE_NAME = "CopierPath";
static constexpr char *COPIER_CMDLINE_OPTIONS = " /auto_close /open_window /no_confirm_del";
static constexpr int EXECUTION_DELAY = 100;

CProxyCopyHook::CProxyCopyHook()
    : _quotedPathBuffer { 0 } {
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

    ExecutionDetail detail(wFunc, pszDestFile);

    EnterCriticalSection(&_cs);

    ExecutionList::iterator iter = std::find_if(_pendingExecutions.begin(), _pendingExecutions.end(),
                                                [&detail](const ExecutionDetail &other) -> bool {
        return detail.operation == other.operation && strncmp(detail.destination, other.destination, MAX_PATH) == 0;
    });
    UINT ret = IDNO;

    if (iter == _pendingExecutions.end()) {
        detail.sources.append(" ").append(QuotePath(pszSrcFile));
        iter = _pendingExecutions.insert(_pendingExecutions.end(), detail);
        ExecutionParameter *param = new ExecutionParameter { this, iter };

        if (CreateThread(nullptr, 0, ExecutionThreadProc, param, 0, nullptr) == nullptr) {
            ret = IDYES;
        }
    } else {
        iter->sources.append(" ").append(QuotePath(pszSrcFile));
    }

    LeaveCriticalSection(&_cs);
    return ret;
}

CProxyCopyHook::ExecutionDetail::ExecutionDetail(UINT func, PCSTR dest)
    : operation(func) {
    strcpy_s(destination, MAX_PATH, dest);

    if (destination[0] != '\0') {
        PathRemoveFileSpec(destination);
        PathAddBackslash(destination);
        PathQuoteSpaces(destination);
    }
}

DWORD WINAPI CProxyCopyHook::ExecutionThreadProc(LPVOID parameter) {
    const ExecutionParameter *param = (const ExecutionParameter *) parameter;
    const ExecutionDetail &detail = *param->iter;

    size_t prevSourceSize = 0;
    while (prevSourceSize != detail.sources.size()) {
        prevSourceSize = detail.sources.size();
        Sleep(EXECUTION_DELAY);
    }

    const char *cmdlineOperation;
    switch (detail.operation) {
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
        return 1;
    }

    std::string cmdline = param->instance->_copierCmdline;
    cmdline.append(" /cmd=")
        .append(cmdlineOperation);

    if (detail.destination[0] != '\0') {
        cmdline.append(" /to=")
            .append(detail.destination);
    }

    EnterCriticalSection(&param->instance->_cs);

    cmdline.append(detail.sources);
    param->instance->_pendingExecutions.erase(param->iter);

    LeaveCriticalSection(&param->instance->_cs);
    delete param;

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcess(nullptr, const_cast<char *>(cmdline.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
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