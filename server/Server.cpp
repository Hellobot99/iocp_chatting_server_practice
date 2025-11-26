#include <iostream>
#include "Server.h"
#include "IOCPWorker.h"
#include "GameLogic.h"
#include "Persistence.h"

extern DWORD WINAPI IOCPWorkerThread(LPVOID arg);
LockFreeQueue<std::unique_ptr<ICommand>> Server::s_gltInputQueue;

Server* g_Server = nullptr;

Server::Server(int iocpThreadCount, int dbThreadCount)
    : hIOCP_(NULL),
    listenSock_(INVALID_SOCKET),
    nextSessionId_(1)
{
    persistence_ = std::make_unique<Persistence>(dbThreadCount);
    persistence_->Initialize("tcp://127.0.0.1:3306", "root", "1234");

    gameLogic_ = std::make_unique<GameLogic>(Server::GetGLTInputQueue(), roomManager_, *persistence_);
    iocpThreadCount_ = iocpThreadCount;
}

Server::~Server()
{
    Stop();
}

// Start: 서버 실행 및 스레드 생성
bool Server::Start(USHORT port)
{
    // 1. 네트워킹 및 IOCP 설정 (WSAStartup, CreateIoCompletionPort 등)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    listenSock_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9190);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenSock_, (SOCKADDR*)&addr, sizeof(addr));
    listen(listenSock_, SOMAXCONN);

    hIOCP_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    //메모리 미리 예약
    iocpWorkerThreads_.reserve(iocpThreadCount_);

    // 2. IOCP Worker Thread 생성 및 실행
    for (size_t i = 0; i < iocpThreadCount_; ++i)
    {
        // IOCPWorkerThread 함수 포인터를 사용하여 스레드 생성
        iocpWorkerThreads_.emplace_back(IOCPWorkerThread, hIOCP_);
    }

    // 3. Game Logic Thread (GLT) 실행
    // GameLogic::Run()이 GLT의 진입 함수입니다.
    gameLogicThread_ = std::thread(&GameLogic::Run, gameLogic_.get());

    // 4. 클라이언트 수락 루프 시작
    acceptThread_ = std::thread(&Server::AcceptLoop, this);

    return true; // 성공 시
}

// Stop: 모든 스레드 종료 및 자원 정리
void Server::Stop()
{
    // 1. Accept 루프 정지
    accepting_ = false;

    if (listenSock_ != INVALID_SOCKET) {
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }

    if (acceptThread_.joinable()) 
    {
        acceptThread_.join();
    }   

    // 2. GLT에 정지 신호 전달 및 조인
    if (gameLogic_)
    {
        gameLogic_->Stop();
        if (gameLogicThread_.joinable())
        {
            gameLogicThread_.join();
        }
    }

    // 3. IOCP Worker Thread 정지 신호 전달 (PostQueuedCompletionStatus 등을 이용) 및 조인
    
    if (hIOCP_ != NULL)
    {
        for (size_t i = 0; i < iocpWorkerThreads_.size(); ++i)
        {
            // pIoData=0, CompletionKey=0 (종료 신호로 약속된 값)을 넣습니다.
            PostQueuedCompletionStatus(hIOCP_, 0, 0, NULL);
        }
    }

    for (auto& t : iocpWorkerThreads_)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    // 4. DBTP 정지 (Persistence::Stop())
    if (persistence_)
    {
        persistence_->Stop();
    }

    // 5. 네트워킹 자원 정리 (closesocket, WSACleanup)
    if (hIOCP_ != NULL) CloseHandle(hIOCP_);
    WSACleanup();
    //redisFree(c);
}

LockFreeQueue<std::unique_ptr<ICommand>>& Server::GetGLTInputQueue()
{
    return s_gltInputQueue;
}

// 클라이언트 연결 수락 루프 구현
void Server::AcceptLoop()
{
    while (accepting_.load()) {
        SOCKET clientSock = accept(listenSock_, NULL, NULL);
        std::cout << "New Client connected!\n";

        HandleNewClient(clientSock);
    }
}

// 새 클라이언트 처리 로직 구현
void Server::HandleNewClient(SOCKET clientSock)
{
    // IOCP에 클라이언트 소켓 등록
    uint32_t newId = nextSessionId_.fetch_add(1);
    auto newSession = std::make_shared<ClientSession>(clientSock, newId);

    std::lock_guard<std::mutex> lock(sessionMutex_);
    sessions_.emplace(newId, newSession);

    CreateIoCompletionPort(
        (HANDLE)clientSock,
        hIOCP_,
        (ULONG_PTR)newSession.get(),
        0
    );

    newSession->PostRecv(hIOCP_);
}

void Server::RemoveSession(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    sessions_.erase(sessionId);
}