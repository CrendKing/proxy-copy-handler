#pragma once

#include "proxy_copy_handler_h.h"

class ATL_NO_VTABLE CProxyCopyHook :
    public ATL::CComObjectRootEx<ATL::CComMultiThreadModel>,
    public ATL::CComCoClass<CProxyCopyHook, &CLSID_ProxyCopyHook>,
    public ICopyHook,
    public ATL::IDispatchImpl<IProxyCopyHook, &IID_IProxyCopyHook, &LIBID_ProxyCopyHandlerLib> {
public:
    STDMETHOD_(UINT, CopyCallback)(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs);

    DECLARE_REGISTRY_RESOURCEID(IDS_PROXY_COPY_HOOK)

    BEGIN_COM_MAP(CProxyCopyHook)
        COM_INTERFACE_ENTRY(IProxyCopyHook)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY_IID(IID_IShellCopyHook, CProxyCopyHook)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct();

private:
    struct ExecutionKey {
        UINT operation;
        char destination[MAX_PATH];

        ExecutionKey(UINT func, PCSTR dest);
        bool operator==(const ExecutionKey &other) const;
    };

    struct ExecutionKeyHasher {
        std::size_t operator()(const ExecutionKey &key) const;
    };

    typedef std::unordered_map<ExecutionKey, std::list<std::string>, ExecutionKeyHasher> ExectionMap;

    struct ExecutionParameter {
        CProxyCopyHook *hook;
        ExectionMap::iterator iter;
    };

    static ExectionMap _pendingExecutions;
    static char _tempPath[MAX_PATH];

    static DWORD WINAPI ExecutionThreadProc(LPVOID hook);
};

OBJECT_ENTRY_AUTO(__uuidof(ProxyCopyHook), CProxyCopyHook)
