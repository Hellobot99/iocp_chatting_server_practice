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
            // 현재 Recv 버퍼에 bytesTransferred만큼 데이터가 들어왔습니다.

            // 2-1. 네트워크 스트림 처리 및 패킷 통합
            // 이 로직은 TCP 스트림이 여러 WSARecv에 걸쳐 쪼개져 오거나, 
            // 하나의 Recv에 여러 패킷이 합쳐져 오는 경우를 처리합니다. [4]
            pSession->AppendToInputBuffer(pIoData->buffer, bytesTransferred);

            // 2-2. 패킷 파싱 및 GLT 위임 루프
            // 버퍼에 완전한 패킷이 남아있는 동안 반복합니다.
            while (pSession->HasCompletePacket())
            {
                // 패킷 데이터를 추출하고 ICommand 객체로 역직렬화합니다. [5, 6]
                std::unique_ptr<ICommand> command = pSession->DeserializeCommand();

                if (command) {
                    // 2-3. 커맨드를 GLT 큐에 푸시하고 즉시 반환 (핵심!) [7]
                    // 이 큐는 GLT가 소비할 Lock-Free Queue여야 합니다.
                    Server::GetGLTInputQueue().Push(std::move(command));
                }
            }

            // 2-4. Recv 재요청 (다음 데이터를 받기 위해 반드시 필요)
            // Recv 요청이 걸려있지 않으면 해당 소켓은 더 이상 데이터를 수신할 수 없습니다.
            pSession->PostRecv(hIOCP);

            // pIoData는 Recv 재요청에서 재사용되거나 (PostRecv 내부에서 관리)
            // 또는 Session 내부에 고정되어 사용되는 것이 일반적입니다.
        }
        else if (pIoData->operation == 1) // Send 완료
        {
            // 2-5. Send 완료 처리
            // 이 Send 요청에 사용된 pIoData 메모리를 해제합니다.
            // WSASend는 논블로킹이므로, 완료 후 메모리 정리가 필요합니다.
            delete pIoData;

            // (선택 사항) 세션의 출력 큐를 확인하여 다음 Send 작업을 걸 수도 있습니다.
            // pSession->TryPostNextSend();
        }
    }
    return 0;
}