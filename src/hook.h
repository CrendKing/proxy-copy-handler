#pragma once

#include "proxy_copy_handler_h.h"


class ATL_NO_VTABLE CProxyCopyHook
    : public ATL::CComObjectRoot
    , public ATL::CComCoClass<CProxyCopyHook, &CLSID_ProxyCopyHook>
    , public ICopyHookW
    , public ATL::IDispatchImpl<IProxyCopyHook, &IID_IProxyCopyHook, &LIBID_ProxyCopyHandlerLib> {
public:
    CProxyCopyHook();
    virtual ~CProxyCopyHook();

    auto STDMETHODCALLTYPE CopyCallback(HWND hwnd, UINT wFunc, UINT wFlags, PCWSTR pszSrcFile, DWORD dwSrcAttribs, PCWSTR pszDestFile, DWORD dwDestAttribs) -> UINT override;

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
        std::wstring destination;

        ExecutionKey(UINT func, PCWSTR dest);
        constexpr auto operator==(const ExecutionKey &other) const -> bool { return operation == other.operation && destination == other.destination; }

        struct Hasher {
            auto operator()(const ExecutionKey &key) const -> size_t;
        };
    };

    static auto CALLBACK WaitCallback(PTP_CALLBACK_INSTANCE Instance,
                                      PVOID                 Context,
                                      PTP_WAIT              Wait,
                                      TP_WAIT_RESULT        WaitResult) -> void;
    static auto QuotePath(PCWSTR path) -> const WCHAR *;

    auto WorkerProc() -> void;

    static std::array<WCHAR, MAX_PATH> _quotedPathBuffer;

    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _workerThread;
    HANDLE _waitEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    PTP_WAIT _waitTp = CreateThreadpoolWait(WaitCallback, this, nullptr);

    bool _stopWorker = false;

    std::wstring _copierCmdline;
    std::unordered_map<ExecutionKey, std::vector<std::wstring>, ExecutionKey::Hasher> _pendingExecutions;
};

OBJECT_ENTRY_AUTO(__uuidof(ProxyCopyHook), CProxyCopyHook)
