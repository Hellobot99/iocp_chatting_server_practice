#pragma once

#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <memory>
#include <thread>
#include "ClientSession.h"
#include "Utility.h"

class GameLogic; // 전방 선언
class Persistence; // 전방 선언

class Server
{
public:
    Server(int iocpThreadCount, int dbThreadCount);
    ~Server();

    bool Start(USHORT port);
    void Stop();

private:
    RoomManager roomManager_;

    // IOCP 핸들
    HANDLE hIOCP_;
    SOCKET listenSock_;

    //==================================================================
    // 3계층 분리된 스레드 풀 및 핵심 로직 컴포넌트
    //==================================================================

    // 1. I/O 워커 스레드 풀 (IOCP Worker Threads)
    std::vector<std::thread> iocpWorkerThreads_;

    // 2. 게임 로직 스레드 (GLT)
    std::unique_ptr<GameLogic> gameLogic_;
    std::thread gameLogicThread_;

    // 3. 지속성 스레드 풀 (DBTP)
    std::unique_ptr<Persistence> persistence_;

    //==================================================================

    // 클라이언트 세션 관리 (ID 매핑)
    std::mutex sessionMutex_;
    std::vector<std::shared_ptr<ClientSession>> sessions_;
    std::atomic<uint32_t> nextSessionId_ = 1;

    void AcceptLoop();
    void HandleNewClient(SOCKET clientSock);

    // IOCP 워커 스레드의 진입 함수 (별도 파일에 정의된 함수 포인터)
    void RunIOCPWorker(HANDLE hIOCP);
};