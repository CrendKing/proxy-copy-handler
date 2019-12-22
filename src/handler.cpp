#include "pch.h"
#include "hook.h"

class ProxyCopyHandlerModule : public ATL::CAtlDllModuleT<ProxyCopyHandlerModule> {
public:
	DECLARE_LIBID(LIBID_ProxyCopyHandlerLib)
} _module;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved) {
	return _module.DllMain(dwReason, lpReserved);
}

// Used to determine whether the DLL can be unloaded by OLE.
_Use_decl_annotations_
STDAPI DllCanUnloadNow(void) {
	return _module.DllCanUnloadNow();
}

// Returns a class factory to create an object of the requested type.
_Use_decl_annotations_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR *ppv) {
	return _module.DllGetClassObject(rclsid, riid, ppv);
}

// DllRegisterServer - Adds entries to the system registry.
_Use_decl_annotations_
STDAPI DllRegisterServer(void) {
	// registers object, typelib and all interfaces in typelib
	return _module.DllRegisterServer();
}

// DllUnregisterServer - Removes entries from the system registry.
_Use_decl_annotations_
STDAPI DllUnregisterServer(void) {
	return _module.DllUnregisterServer();
}

// DllInstall - Adds/Removes entries to the system registry per user per machine.
_Use_decl_annotations_
STDAPI DllInstall(BOOL bInstall, _In_opt_ PCWSTR pszCmdLine) {
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