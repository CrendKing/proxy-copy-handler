#pragma once

#include "proxy_copy_handler_h.h"


class ATL_NO_VTABLE CProxyCopyHook
    : public ATL::CComObjectRoot
    , public ATL::CComCoClass<CProxyCopyHook, &CLSID_ProxyCopyHook>
    , public ICopyHook
    , public ATL::IDispatchImpl<IProxyCopyHook, &IID_IProxyCopyHook, &LIBID_ProxyCopyHandlerLib> {
public:
    CProxyCopyHook();
    virtual ~CProxyCopyHook();

    STDMETHOD_(UINT, CopyCallback)(HWND hwnd, UINT wFunc, UINT wFlags, PCSTR pszSrcFile, DWORD dwSrcAttribs, PCSTR pszDestFile, DWORD dwDestAttribs) override;

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
        std::string destination;

        ExecutionKey(UINT func, PCSTR dest);
        bool operator==(const ExecutionKey &other) const;

        struct Hasher {
            size_t operator()(const ExecutionKey &key) const;
        };
    };

    static void CALLBACK WaitCallback(PTP_CALLBACK_INSTANCE Instance,
                                      PVOID                 Context,
                                      PTP_WAIT              Wait,
                                      TP_WAIT_RESULT        WaitResult);
    static const char *QuotePath(PCSTR path);

    void WorkerProc();

    static char _quotedPathBuffer[MAX_PATH];

    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _workerThread;
    HANDLE _waitEvent;
    PTP_WAIT _waitTp;

    bool _stopWorker;

    std::string _copierCmdline;
    std::unordered_map<ExecutionKey, std::vector<std::string>, ExecutionKey::Hasher> _pendingExecutions;
};

OBJECT_ENTRY_AUTO(__uuidof(ProxyCopyHook), CProxyCopyHook)
