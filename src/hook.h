#pragma once

#include "proxy_copy_handler_h.h"


class ATL_NO_VTABLE CProxyCopyHook
    : public ATL::CComObjectRoot
    , public ATL::CComCoClass<CProxyCopyHook, &CLSID_ProxyCopyHook>
    , public ICopyHook
    , public ATL::IDispatchImpl<IProxyCopyHook, &IID_IProxyCopyHook, &LIBID_ProxyCopyHandlerLib> {
public:
    CProxyCopyHook();
    ~CProxyCopyHook();

    STDMETHOD_(UINT, CopyCallback)(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs);

    DECLARE_REGISTRY_RESOURCEID(IDS_PROXY_COPY_HOOK)

    BEGIN_COM_MAP(CProxyCopyHook)
        COM_INTERFACE_ENTRY(IProxyCopyHook)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY_IID(IID_IShellCopyHook, CProxyCopyHook)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

private:
    struct ExecutionKey {
        UINT operation;
        char destination[MAX_PATH];

        ExecutionKey(UINT func, PCSTR path);
        bool operator==(const ExecutionKey &other) const;
    };

    struct ExecutionKeyHasher {
        std::size_t operator()(const ExecutionKey &key) const;
    };

    typedef std::unordered_map<ExecutionKey, std::string, ExecutionKeyHasher> ExecutionMap;

    struct ExecutionParameter {
        CProxyCopyHook *instance;
        ExecutionMap::iterator iter;
    };

    CRITICAL_SECTION _cs;
    ExecutionMap _pendingExecutions;
    std::string _copierCmdline;
    char _quotedPathBuffer[MAX_PATH];

    static DWORD WINAPI ExecutionThreadProc(LPVOID hook);
    const char *QuotePath(PCSTR path);
};

OBJECT_ENTRY_AUTO(__uuidof(ProxyCopyHook), CProxyCopyHook)
