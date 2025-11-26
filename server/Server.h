#pragma once

#include <winsock2.h>
#include <windows.h>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <map>
#include "ClientSession.h"
#include "Utility.h"
#include "IOCPWorker.h"
#include "RoomManager.h"
#include "Persistence.h"

#pragma comment(lib, "Ws2_32.lib")

class GameLogic;
class Persistence;

class Server
{
public:
    Server(int iocpThreadCount, int dbThreadCount);
    ~Server();

    bool Start(USHORT port);
    void Stop();
    static LockFreeQueue<std::unique_ptr<ICommand>>& GetGLTInputQueue();
    Persistence& GetPersistence() { return *persistence_; }
    void RemoveSession(uint32_t sessionId);

private:
    RoomManager roomManager_;
    std::unique_ptr<Persistence> persistence_;
    static LockFreeQueue<std::unique_ptr<ICommand>> s_gltInputQueue;

    // IOCP 핸들
    HANDLE hIOCP_;
    SOCKET listenSock_;
    std::atomic<bool> accepting_ = true;
    int iocpThreadCount_;

    // 1. I/O 워커 스레드 풀 (IOCP Worker Threads)
    std::vector<std::thread> iocpWorkerThreads_;

    // 2. 게임 로직 스레드 (GLT)
    std::unique_ptr<GameLogic> gameLogic_;
    std::thread gameLogicThread_, acceptThread_;

    // 클라이언트 세션 관리 (ID 매핑)
    std::mutex sessionMutex_;
    std::map<uint32_t, std::shared_ptr<ClientSession>> sessions_;
    std::atomic<uint32_t> nextSessionId_ = 1;

    void AcceptLoop();
    void HandleNewClient(SOCKET clientSock);
};