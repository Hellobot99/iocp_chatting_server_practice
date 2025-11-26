#include "IOCPWorker.h"
#include "ClientSession.h"
#include <iostream>

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

        // CompletionKey는 우리가 등록한 ClientSession 포인터
        ClientSession* pSession = reinterpret_cast<ClientSession*>(completionKey);

        // 2. 에러 처리 및 연결 종료 감지
        // - ok가 FALSE면 IOCP 에러
        // - bytesTransferred가 0이면 상대방이 소켓을 닫음 (Graceful Close)
        if (!ok || bytesTransferred == 0)
        {
            // [수정됨] 주석을 지우고 Disconnect 호출
            if (pSession)
            {
                // 세션 내부에서 closesocket() 하고 Server에게 나를 지워달라고 요청함
                pSession->Disconnect();
            }

            // 연결이 끊겼으므로 더 이상 처리하지 않고 다음 대기로 넘어감
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