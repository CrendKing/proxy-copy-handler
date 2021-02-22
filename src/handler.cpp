#include "pch.h"
#include "hook.h"


class ProxyCopyHandlerModule : public ATL::CAtlDllModuleT<ProxyCopyHandlerModule> {
public:
    DECLARE_LIBID(LIBID_ProxyCopyHandlerLib)
} _module;

extern "C" auto WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) -> BOOL {
    return _module.DllMain(dwReason, lpReserved);
}

// Used to determine whether the DLL can be unloaded by OLE.
_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllCanUnloadNow(void) -> HRESULT {
    return _module.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type.
_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR *ppv) -> HRESULT {
    return _module.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry.
_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllRegisterServer(void) -> HRESULT {
    // registers object, typelib and all interfaces in typelib
    return _module.DllRegisterServer();
}

// DllUnregisterServer - Removes entries from the system registry.
_Use_decl_annotations_
extern "C" auto STDAPICALLTYPE DllUnregisterServer(void) -> HRESULT {
    return _module.DllUnregisterServer();
}

// DllInstall - Adds/Removes entries to the system registry per user per machine.
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
