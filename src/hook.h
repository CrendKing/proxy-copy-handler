#pragma once


class ATL_NO_VTABLE __declspec(uuid("4EB17DC6-A98E-42DE-9F3F-93A6819B08A4")) CProxyCopyHook
    : public ATL::CComObjectRoot
    , public ATL::CComCoClass<CProxyCopyHook, &__uuidof(CProxyCopyHook)>
    , public ICopyHookW {
public:
    DECLARE_REGISTRY_RESOURCEID(IDS_PROXY_COPY_HOOK)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(CProxyCopyHook)
        COM_INTERFACE_ENTRY(ICopyHookW)
    END_COM_MAP()

protected:
    CProxyCopyHook();
    ~CProxyCopyHook();

private:
    auto STDMETHODCALLTYPE CopyCallback(HWND hwnd, UINT wFunc, UINT wFlags, PCWSTR pszSrcFile, DWORD dwSrcAttribs, PCWSTR pszDestFile, DWORD dwDestAttribs) -> UINT override;

    struct ExecutionKey {
        UINT operation;
        std::wstring destination;

        ExecutionKey(UINT func, PCWSTR dest);
        constexpr auto operator==(const ExecutionKey &other) const -> bool { return operation == other.operation && destination == other.destination; }

        struct Hasher {
            auto operator()(const ExecutionKey &key) const -> size_t;
        };
    };

    static auto CALLBACK WaitCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WAIT wait, TP_WAIT_RESULT waitResult) -> void;
    static auto QuotePath(PCWSTR path) -> const WCHAR *;

    auto WorkerProc() -> void;

    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _workerThread;
    HANDLE _waitEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    PTP_WAIT _waitTp = CreateThreadpoolWait(WaitCallback, this, nullptr);

    std::atomic<bool> _stopWorker = false;

    std::wstring _copierCmdline;
    bool _startMinimized = false;
    std::unordered_map<ExecutionKey, std::unordered_set<std::wstring>, ExecutionKey::Hasher> _pendingExecutions;
};
