#include "pch.h"
#include "hook.h"

constexpr static int EXECUTION_DELAY = 500;

CProxyCopyHook::ExectionMap CProxyCopyHook::_pendingExecutions;
char CProxyCopyHook::_tempPath[MAX_PATH] { 0 };

HRESULT CProxyCopyHook::FinalConstruct() {
    if (_tempPath[0] == '\0' && GetTempPath(MAX_PATH, _tempPath) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

DWORD WINAPI CProxyCopyHook::ExecutionThreadProc(LPVOID parameter) {
    Sleep(EXECUTION_DELAY);

    ExecutionParameter *param = (ExecutionParameter *) parameter;
    const ExecutionKey &key = param->iter->first;

    char srcFilename[MAX_PATH];
    GetTempFileName(_tempPath, "PCH", 0, srcFilename);

    param->hook->Lock();

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

    std::ofstream srcFile(srcFilename);
    std::ostream_iterator<std::string> output_iterator(srcFile, "\n");
    std::copy(param->iter->second.begin(), param->iter->second.end(), output_iterator);
    srcFile.close();

    std::string cmdline = "\"FastCopy.exe\" /force_close /no_confirm_del /error_stop /cmd=";
    cmdline.append(cmdlineOperation)
        .append(" /srcfile=")
        .append(srcFilename);

    if (key.destination[0] != '\0') {
        cmdline.append(" /to=")
            .append(key.destination);
    }

    _pendingExecutions.erase(param->iter);
    param->hook->Unlock();
    delete param;

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcess(nullptr, const_cast<char *>(cmdline.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    DeleteFile(srcFilename);

    return 0;
}

STDMETHODIMP_(UINT) CProxyCopyHook::CopyCallback(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs) {
    if (wFunc != FO_MOVE && wFunc != FO_COPY && wFunc != FO_DELETE) {
        return IDYES;
    }

    // Windows can move file without copying within the same volume (ref. MOVEFILE_COPY_ALLOWED)
    // FastCopy move is always copy-then-delete
    if (wFunc == FO_MOVE && PathGetDriveNumber(pszSrcFile) == PathGetDriveNumber(pszDestFile)) {
        return IDYES;
    }

    ExecutionKey key(wFunc, pszDestFile);

    Lock();

    ExectionMap::iterator iter = _pendingExecutions.find(key);
    UINT ret = IDNO;

    if (iter == _pendingExecutions.end()) {
        std::list<std::string> sources { pszSrcFile };
        ExecutionParameter *param = new ExecutionParameter {
            this,
            _pendingExecutions.insert({ key, sources }).first
        };

        if (CreateThread(nullptr, 0, ExecutionThreadProc, param, 0, nullptr) == nullptr) {
            ret = IDYES;
        }
    } else {
        iter->second.push_back(pszSrcFile);
    }

    Unlock();
    return ret;
}

CProxyCopyHook::ExecutionKey::ExecutionKey(UINT func, PCSTR dest) :
    operation(func) {
    strcpy_s(destination, MAX_PATH, dest);

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