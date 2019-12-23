#pragma once

#include "pch.h"
#include "proxy_copy_handler_h.h"

using namespace ATL;

class ATL_NO_VTABLE CProxyCopyHook :
    public CComObjectRootEx<CComSingleThreadModel>,
    public CComCoClass<CProxyCopyHook, &CLSID_ProxyCopyHook>,
    public ICopyHook,
    public IDispatchImpl<IProxyCopyHook, &IID_IProxyCopyHook, &LIBID_ProxyCopyHandlerLib> {
public:
    CProxyCopyHook() {
    }

    STDMETHOD_(UINT, CopyCallback)(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs);

    DECLARE_REGISTRY_RESOURCEID(IDS_PROXY_COPY_HOOK)

    BEGIN_COM_MAP(CProxyCopyHook)
        COM_INTERFACE_ENTRY(IProxyCopyHook)
        COM_INTERFACE_ENTRY(IDispatch)
        COM_INTERFACE_ENTRY_IID(IID_IShellCopyHook, CProxyCopyHook)
    END_COM_MAP()

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    HRESULT FinalConstruct() {
        return S_OK;
    }

    void FinalRelease() {
    }
};

OBJECT_ENTRY_AUTO(__uuidof(ProxyCopyHook), CProxyCopyHook)
