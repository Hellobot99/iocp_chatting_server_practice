#include "Server.h"
#include <iostream>

int main()
{
    // 사용할 스레드 개수 설정
    const int iocpThreadCount = 4;
    const int dbThreadCount = 2;

    // 메인 서버 객체 생성
    Server gameServer(iocpThreadCount, dbThreadCount);

    std::cout << "Server starting..." << std::endl;

    // 9190 포트에서 서버 시작
    if (gameServer.Start(9190)) {
        std::cout << "Server is running." << std::endl;

        // 메인 스레드가 바로 종료되지 않도록 대기
        // (실제로는 종료 커맨드를 입력받는 로직 등이 들어갈 수 있습니다)
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    else {
        std::cerr << "Failed to start server." << std::endl;
    }

    return 0;
}

/*
#include "pch.h"
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <hiredis/hiredis.h>

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>

#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 1) // 패딩 방지
struct ChatPacket {
    int packetType;      // 0=채팅, 1=명령
    char username[32];   // 보낸 사람
    char message[256];   // 메시지
};
#pragma pack(pop)

std::vector<SOCKET> clients;
std::mutex client_Mutex;
std::mutex redisMutex;

sql::mysql::MySQL_Driver* driver;
sql::Connection* con;
redisContext* c;

struct PER_IO_DATA {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[1024];
    int operation; // 0: recv, 1: send
};

DWORD WINAPI WorkerThread(LPVOID arg);
void BroadcastMessage(SOCKET sender, const char* msg, int len);
void SaveChatMessage(sql::Connection* con, const std::string& username, const std::string& msg);


int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    try {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect("tcp://127.0.0.1:3306", "root", "1234");
        std::cout << "MYSQL DB Connected!" << std::endl;

        con->setSchema("chatdb");
    }
    catch (sql::SQLException& e) {
        std::cerr << "SQL Error: " << e.what() << std::endl;
        std::cerr << "MySQL error code: " << e.getErrorCode()
            << " (SQLState: " << e.getSQLStateCStr() << ")" << std::endl;
    }

    c = redisConnect("127.0.0.1", 6379);
    if (c == nullptr || c->err) {
        std::cerr << "Redis 연결 실패: "
            << (c ? c->errstr : "context 없음") << std::endl;
        return 1;
    }

    std::cout << "Redis 서버 연결 성공!" << std::endl;

    SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    sockaddr_in addr;
    ZeroMemory(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9190);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listenSock, (SOCKADDR*)&addr, sizeof(addr));
    listen(listenSock, SOMAXCONN);

    // IOCP 생성
    HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // WorkerThread 2개 실행
    for (int i = 0; i < 2; i++) {
        CreateThread(NULL, 0, WorkerThread, hIOCP, 0, NULL);
    }

    std::cout << "IOCP Echo Server running...\n";

    while (true) {
        SOCKET clientSock = accept(listenSock, NULL, NULL);
        std::cout << "Client connected!\n";

        std::lock_guard<std::mutex> lock(client_Mutex);
        clients.push_back(clientSock);

        // IOCP에 클라이언트 소켓 등록
        CreateIoCompletionPort((HANDLE)clientSock, hIOCP, (ULONG_PTR)clientSock, 0);

        // IO 데이터 구조체 생성
        PER_IO_DATA* pIoData = new PER_IO_DATA;
        ZeroMemory(&pIoData->overlapped, sizeof(OVERLAPPED));
        pIoData->wsaBuf.buf = pIoData->buffer;
        pIoData->wsaBuf.len = sizeof(pIoData->buffer);
        pIoData->operation = 0;

        // 첫 번째 Recv 걸기
        DWORD flags = 0;
        WSARecv(clientSock, &pIoData->wsaBuf, 1, NULL, &flags, &pIoData->overlapped, NULL);

    }

    closesocket(listenSock);
    WSACleanup();
    redisFree(c);
    return 0;
}

void BroadcastMessage(SOCKET sender, const char* msg, int len) {
    std::lock_guard<std::mutex> lock(client_Mutex);
    for (SOCKET s : clients) {
        if (s == sender) continue;
        PER_IO_DATA* sendData = new PER_IO_DATA;

        ZeroMemory(&sendData->overlapped, sizeof(OVERLAPPED));
        memcpy(sendData->buffer, msg, len);
        sendData->wsaBuf.buf = sendData->buffer;
        sendData->wsaBuf.len = len;
        sendData->operation = 1;
        WSASend(s, &sendData->wsaBuf, 1, NULL, 0, &sendData->overlapped, NULL);
    }
}

DWORD WINAPI WorkerThread(LPVOID arg) {
    HANDLE hIOCP = (HANDLE)arg;
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    PER_IO_DATA* pIoData;

    while (true) {
        BOOL ok = GetQueuedCompletionStatus(
            hIOCP,
            &bytesTransferred,
            &completionKey,
            (LPOVERLAPPED*)&pIoData,
            INFINITE
        );

        if (!ok || bytesTransferred == 0) {
            std::cout << "Client disconnected\n";
            closesocket((SOCKET)completionKey);
            delete pIoData;
            std::lock_guard<std::mutex> lock(client_Mutex);
            clients.erase(std::remove(clients.begin(), clients.end(), (SOCKET)completionKey));
            ;           continue;
        }

        SOCKET clientSock = (SOCKET)completionKey;

        if (pIoData->operation == 0) { // RECV 완료
            ChatPacket* pkt = reinterpret_cast<ChatPacket*>(pIoData->buffer);

            std::cout << "Recv Packet Type: " << pkt->packetType << "\n";
            std::cout << "From: " << pkt->username << "\n";
            std::cout << "Message: " << pkt->message << "\n";

            SaveChatMessage(con, pkt->username, pkt->message);
            BroadcastMessage(clientSock, pIoData->buffer, bytesTransferred);

            // 다시 Recv 대기
            ZeroMemory(&pIoData->overlapped, sizeof(OVERLAPPED));
            pIoData->wsaBuf.len = sizeof(pIoData->buffer);
            pIoData->operation = 0;
            DWORD flags = 0;
            WSARecv(clientSock, &pIoData->wsaBuf, 1, NULL, &flags, &pIoData->overlapped, NULL);
        }
    }
    return 0;
}

void SaveChatMessage(sql::Connection* con, const std::string& username, const std::string& msg) {
    try {
        // Prepared Statement로 안전하게 INSERT
        sql::PreparedStatement* pstmt;
        pstmt = con->prepareStatement("INSERT INTO chat_logs(username, message) VALUES(?, ?)");
        pstmt->setString(1, username);
        pstmt->setString(2, msg);
        pstmt->execute();
        delete pstmt;

        std::lock_guard<std::mutex> lock(redisMutex);
        redisReply* reply = (redisReply*)redisCommand(c, "LPUSH recent_chat %s", (username + ": " + msg).c_str());
        freeReplyObject(reply);

        reply = (redisReply*)redisCommand(c, "LTRIM recent_chat 0 49");
        freeReplyObject(reply);
    }
    catch (sql::SQLException& e) {
        std::cerr << "DB 저장 실패: " << e.what() << std::endl;
    }
}

*/