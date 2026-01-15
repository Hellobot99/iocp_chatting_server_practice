#include <iostream>
#include "Server.h"
#include "IOCPWorker.h"
#include "GameLogic.h"
#include "Persistence.h"

extern DWORD WINAPI IOCPWorkerThread(LPVOID arg);
LockFreeQueue<std::unique_ptr<ICommand>> Server::s_gltInputQueue;

Server::Server(int iocpThreadCount, int dbThreadCount)
    : hIOCP_(NULL),
    listenSock_(INVALID_SOCKET),
    nextSessionId_(1)
{
    persistence_ = std::make_unique<Persistence>(dbThreadCount);
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
    gameLogicThread_ = std::thread(&GameLogic::Run, gameLogic_.get());

    // 4. 클라이언트 수락 루프 시작
    acceptThread_ = std::thread(&Server::AcceptLoop, this);

    return true; // 성공 시
}

// Stop: 모든 스레드 종료 및 자원 정리
void Server::Stop()
{
    if (isStopped_) return;
    isStopped_ = true;

    std::cout << "Stopping server..." << std::endl;
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

    if (hIOCP_ != NULL)
    {
        CloseHandle(hIOCP_);
        hIOCP_ = NULL;
    }

    WSACleanup();
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

        if (clientSock == INVALID_SOCKET)
        {
            // 서버 종료 과정에서 발생한 에러라면 루프 종료
            if (!accepting_.load()) break;

            // 진짜 에러라면 로그 출력 후 계속 대기
            int err = WSAGetLastError();
            std::cout << "[Error] Accept Failed: " << err << std::endl;
            continue;
        }

        std::cout << "New Client connected! Socket: " << clientSock << "\n";
        HandleNewClient(clientSock);
    }
}

// 새 클라이언트 처리 로직 구현
void Server::HandleNewClient(SOCKET clientSock)
{
    // ClientSession에 클라이언트 소켓 등록
    uint32_t newId = nextSessionId_.fetch_add(1);
    auto newSession = std::make_shared<ClientSession>(clientSock, newId);

    std::lock_guard<std::mutex> lock(sessionMutex_);
    sessions_.emplace(newId, newSession);

    // IOCP에 클라이언트 소켓 등록
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

std::shared_ptr<ClientSession> Server::GetSession(uint32_t id)
{
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = sessions_.find(id);

    if (it == sessions_.end())
        return nullptr;

    return it->second;
}

// 해당 유저가 접속중인지 확인하는 함수
bool Server::IsUserConnected(const std::string& username)
{
    std::lock_guard<std::mutex> lock(sessionMutex_);

    for (auto& pair : sessions_)
    {
        auto session = pair.second;
        if (session && session->GetName() == username)
        {
            return true;
        }
    }
    return false;
}