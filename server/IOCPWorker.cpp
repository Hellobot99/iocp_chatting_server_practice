#include "IOCPWorker.h"
#include "ClientSession.h"
#include <iostream>
#include "Server.h" 
extern Server* g_Server;

DWORD WINAPI IOCPWorkerThread(LPVOID arg)
{
    HANDLE hIOCP = (HANDLE)arg;
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    PER_IO_DATA* pIoData = nullptr;

    while (true)
    {
        BOOL ok = GetQueuedCompletionStatus(
            hIOCP,
            &bytesTransferred,
            &completionKey,
            (LPOVERLAPPED*)&pIoData,
            INFINITE
        );

        if (ok && bytesTransferred == 0 && completionKey == 0 && pIoData == nullptr)
        {
            std::cout << "[Worker] Thread Exiting..." << std::endl;
            break;
        }

        ClientSession* pSession = reinterpret_cast<ClientSession*>(completionKey);

        if (!ok || bytesTransferred == 0)
        {
            if (pSession)
            {
                pSession->Disconnect();

                if (g_Server)
                    g_Server->RemoveSession(pSession->GetSessionId());
            }
            continue;
        }

        if (pIoData->operation == 0)
        {
            pSession->OnRecv(bytesTransferred);
        }
        else if (pIoData->operation == 1)
        {
            pSession->OnSendCompleted(bytesTransferred);
        }
    }
    return 0;
}