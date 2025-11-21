#include "IOCPWorker.h"
#include "ClientSession.h"
#include "Command.h"
#include "Server.h"

DWORD WINAPI IOCPWorkerThread(LPVOID arg)
{
    HANDLE hIOCP = (HANDLE)arg; // Server::Start()에서 넘겨받은 IOCP 핸들
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    PER_IO_DATA* pIoData = nullptr; // 오버랩 구조체 (Recv/Send 결과)

    // IOCP 워커 스레드는 무한 루프를 돌며 OS로부터 I/O 완료 알림을 기다립니다. [1, 2]
    while (true)
    {
        // GetQueuedCompletionStatus 호출 시점에서 스레드는 블로킹 상태로 진입합니다.
        BOOL ok = GetQueuedCompletionStatus(
            hIOCP,
            &bytesTransferred,
            &completionKey,
            (LPOVERLAPPED*)&pIoData,
            INFINITE
        );

        // CompletionKey는 ClientSession* 포인터입니다. [3]
        // ULONG_PTR -> ClientSession* 캐스팅은 매번 필요합니다.
        ClientSession* pSession = reinterpret_cast<ClientSession*>(completionKey);

        // 1. I/O 실패, 연결 해제 또는 서버 종료 처리
        if (!ok || bytesTransferred == 0)
        {
            // bytesTransferred == 0은 정상적인 연결 해제 또는 에러를 의미합니다.
            // pIoData가 Send 작업 완료로 인한 것이 아니라면, 여기서 pIoData를 삭제해야 합니다.
            // (pIoData가 nullptr이 아닌 경우에만 삭제 로직 필요)
            if (pIoData) {
                // pIoData 삭제 또는 재활용 로직 (복잡하므로 간략화)
                // 현재 세션의 pIoData가 아니라면 여기서 메모리 해제를 시도합니다.
            }

            // 클라이언트 세션 정리 로직 (Server::sessions_에서 제거, 소켓 닫기)
            // closesocket(pSession->GetSocket());
            // Server::RemoveSession(pSession->GetSessionId());

            continue; // 다음 I/O 완료를 기다립니다.
        }

        // 2. I/O 작업 타입 분류 (Recv 완료 또는 Send 완료)
        if (pIoData->operation == 0) // Recv 완료
        {
            pSession->OnRecv(bytesTransferred);
        }
        else if (pIoData->operation == 1) // Send 완료
        {
            pSession->OnSendCompleted(bytesTransferred);
        }
    }
    return 0;
}