#include "Server.h"
#include "IOCPWorker.h"
#include "GameLogic.h"
#include "Persistence.h"

extern DWORD WINAPI IOCPWorkerThread(LPVOID arg);

Server::Server(int iocpThreadCount, int dbThreadCount)
    : hIOCP_(NULL),
    listenSock_(INVALID_SOCKET),
    nextSessionId_(1)
{
    // 큐를 먼저 생성하고, GameLogic과 Persistence에 공유합니다.
    static LockFreeQueue<std::unique_ptr<ICommand>> gltInputQueue;

    persistence_ = std::make_unique<Persistence>(dbThreadCount);
    // (여기서 DB/Redis Initialize를 호출해야 합니다.)

    // GameLogic 생성 (GLT는 입력 큐와 지속성 시스템에 대한 참조를 가집니다.)
    gameLogic_ = std::make_unique<GameLogic>(gltInputQueue, roomManager_, *persistence_);
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

    // 2. IOCP Worker Thread 생성 및 실행
    for (size_t i = 0; i < iocpWorkerThreads_.capacity(); ++i)
    {
        // IOCPWorkerThread 함수 포인터를 사용하여 스레드 생성
        iocpWorkerThreads_.emplace_back(IOCPWorkerThread, hIOCP_);
    }

    // 3. Game Logic Thread (GLT) 실행
    // GameLogic::Run()이 GLT의 진입 함수입니다.
    gameLogicThread_ = std::thread(&GameLogic::Run, gameLogic_.get());

    // 4. 클라이언트 수락 루프 시작 (메인 스레드 또는 별도 스레드에서)
    // AcceptLoop(); // 만약 메인 스레드에서 돌린다면

    return true; // 성공 시
}

// Stop: 모든 스레드 종료 및 자원 정리
void Server::Stop()
{
    // 1. Accept 루프 정지
    //...

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
    //...
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
    //...
}


// 멤버 함수 구현 (반드시 Server:: 스코프 지정)

// 클라이언트 연결 수락 루프 구현
void Server::AcceptLoop()
{
    // listenSock_에서 accept()를 반복적으로 호출
    // 새 클라이언트 연결 시 HandleNewClient 호출
    //...
}

// 새 클라이언트 처리 로직 구현
void Server::HandleNewClient(SOCKET clientSock)
{
    // 1. 새 ClientSession 객체 생성
    // 2. IOCP에 해당 소켓 등록: CreateIoCompletionPort((HANDLE)clientSock, hIOCP_, (ULONG_PTR)newSession, 0);
    // 3. 초기 WSARecv() 요청 걸기
    //...
}

// IOCP 워커 스레드의 진입 함수 (보통 Free Function으로 구현됨)
/*
void RunIOCPWorker(HANDLE hIOCP) {
    //... 이 부분은 Server.cpp가 아닌 IOCPWorker.cpp에 위치해야 합니다.
}
*/