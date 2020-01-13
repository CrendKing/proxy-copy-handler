#pragma once

#include "proxy_copy_handler_h.h"


class ATL_NO_VTABLE CProxyCopyHook
    : public ATL::CComObjectRoot
    , public ATL::CComCoClass<CProxyCopyHook, &CLSID_ProxyCopyHook>
    , public ICopyHook
    , public ATL::IDispatchImpl<IProxyCopyHook, &IID_IProxyCopyHook, &LIBID_ProxyCopyHandlerLib> {
public:
    CProxyCopyHook();

    STDMETHOD_(UINT, CopyCallback)(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs);

    DECLARE_REGISTRY_RESOURCEID(IDS_PROXY_COPY_HOOK)

    BEGIN_COM_MAP(CProxyCopyHook)
        COM_INTERFACE_ENTRY(IProxyCopyHook)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY_IID(IID_IShellCopyHook, CProxyCopyHook)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

private:
    struct ExecutionDetail {
        UINT operation;
        std::string sources;
        char destination[MAX_PATH];

        ExecutionDetail(UINT func, PCSTR dest);
    };

    typedef std::list<ExecutionDetail> ExecutionList;

    void ExecutionThreadProc(const ExecutionList::iterator iter);
    const char *QuotePath(PCSTR path);

    std::mutex _mutex;
    std::string _copierCmdline;
    ExecutionList _pendingExecutions;
    char _quotedPathBuffer[MAX_PATH];
};

OBJECT_ENTRY_AUTO(__uuidof(ProxyCopyHook), CProxyCopyHook)
