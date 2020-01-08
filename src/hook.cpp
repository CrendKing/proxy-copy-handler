#include "pch.h"
#include "hook.h"
#include <shellapi.h>
#include <shlwapi.h>
#include <string>

STDMETHODIMP_(UINT) CProxyCopyHook::CopyCallback(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs) {
    char srcQuoted[MAX_PATH];
    char destParent[MAX_PATH];
    std::string cmdlineOperation;
    std::string cmdlineDest;

    switch (wFunc) {
    case FO_MOVE:
    case FO_COPY:
        if (wFunc == FO_MOVE) {
            // Windows can move file without copying within the same volume (ref. MOVEFILE_COPY_ALLOWED)
            // FastCopy move is always copy-then-delete
            if (PathGetDriveNumber(pszSrcFile) == PathGetDriveNumber(pszDestFile)) {
                return IDYES;
            }

            cmdlineOperation = "move";
        } else {
            cmdlineOperation = "diff";
        }

        strcpy_s(destParent, MAX_PATH, pszDestFile);
        PathRemoveFileSpec(destParent);
        PathAddBackslash(destParent);
        PathQuoteSpaces(destParent);
        cmdlineDest = std::string(" /to=").append(destParent);
        break;
    case FO_DELETE:
        cmdlineOperation = "delete";
        cmdlineDest = "";
        break;
    default:
        return IDYES;
    }

    strcpy_s(srcQuoted, MAX_PATH, pszSrcFile);
    PathQuoteSpaces(srcQuoted);

    std::string cmdline = "\"FastCopy.exe\" /force_close /no_confirm_del /error_stop /cmd=";
    cmdline.append(cmdlineOperation)
        .append(" ")
        .append(srcQuoted)
        .append(cmdlineDest);

    HANDLE hMutex = CreateMutex(nullptr, FALSE, "ProxyCopyHandler");
    if (hMutex == nullptr) {
        return IDYES;
    }

    WaitForSingleObject(hMutex, INFINITE);

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL created = CreateProcess(nullptr, const_cast<char *>(cmdline.c_str()), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (created) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return created ? IDNO : IDYES;
}
