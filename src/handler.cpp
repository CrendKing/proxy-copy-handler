#include "pch.h"
#include "hook.h"


class ProxyCopyHandlerModule : public ATL::CAtlDllModuleT<ProxyCopyHandlerModule> {
public:
    DECLARE_LIBID(LIBID_ProxyCopyHandlerLib)
} g_module;

extern "C" auto WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) -> BOOL {
    return g_module.DllMain(dwReason, lpReserved);
}

_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllCanUnloadNow() -> HRESULT {
    return g_module.DllCanUnloadNow();
}

_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR *ppv) -> HRESULT {
    return g_module.DllGetClassObject(rclsid, riid, ppv);
}

_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllRegisterServer() -> HRESULT {
    return g_module.DllRegisterServer();
}

_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllUnregisterServer() -> HRESULT {
    return g_module.DllUnregisterServer();
}

_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllInstall(BOOL bInstall, _In_opt_ PCWSTR pszCmdLine) -> HRESULT {
    HRESULT hr = E_FAIL;
    static const wchar_t szUserSwitch[] = L"user";

    if (pszCmdLine != nullptr) {
        if (_wcsnicmp(pszCmdLine, szUserSwitch, _countof(szUserSwitch)) == 0) {
            ATL::AtlSetPerUserRegistration(true);
        }
    }

    if (bInstall) {
        hr = DllRegisterServer();
        if (FAILED(hr)) {
            DllUnregisterServer();
        }
    } else {
        hr = DllUnregisterServer();
    }

    return hr;
}
