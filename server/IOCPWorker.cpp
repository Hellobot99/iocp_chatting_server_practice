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
        // 1. I/O 완료 대기
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
            break; // while 루프 탈출 -> 스레드 종료
        }

        // CompletionKey는 우리가 등록한 ClientSession 포인터
        ClientSession* pSession = reinterpret_cast<ClientSession*>(completionKey);

        // 2. 에러 처리 및 연결 종료 감지
        // - ok가 FALSE면 IOCP 에러
        // - bytesTransferred가 0이면 상대방이 소켓을 닫음 (Graceful Close)
        if (!ok || bytesTransferred == 0)
        {
            if (pSession)
            {
                pSession->Disconnect(); // 1. 소켓 닫기 및 정리

                // 2. [★여기서 삭제] IO 작업이 다 끝난 이 시점에 안전하게 삭제합니다.
                if (g_Server)
                    g_Server->RemoveSession(pSession->GetSessionId());
            }
            continue;
        }

        // 3. I/O 작업 종류에 따른 처리
        // pIoData는 ClientSession 멤버 변수의 주소이므로 별도 delete 불필요
        if (pIoData->operation == 0) // RECV 완료
        {
            pSession->OnRecv(bytesTransferred);
        }
        else if (pIoData->operation == 1) // SEND 완료
        {
            pSession->OnSendCompleted(bytesTransferred);
        }
    }
    return 0;
}