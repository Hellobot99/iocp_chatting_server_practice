#pragma once

#include <vector>
#include <thread>
#include <memory>
#include <mutex>

// MySQL 및 Redis 라이브러리 (기존 코드 기반)
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <cppconn/connection.h>
// #include <hiredis/hiredis.h>
#include "Utility.h"
#include "PersistenceRequest.h"
// #include "LockFreeQueue.h"

class Persistence
{
public:
    Persistence(int threadCount);
    ~Persistence();

    // [변경] Redis IP/Port 인자 제거
    bool Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass);
    void Stop();

    void PostRequest(std::unique_ptr<PersistenceRequest> request);

private:
    void WorkerLoop();
    void ProcessSaveChat(sql::Connection* con, const PersistenceRequest& req);

private:
    int threadCount_;
    std::vector<std::thread> workers_;
    bool running_ = true;

    // 요청 큐
    std::queue<std::unique_ptr<PersistenceRequest>> requestQueue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;

    // DB 접속 정보
    std::string dbUrl_, dbUser_, dbPass_;

    // MySQL 드라이버
    sql::mysql::MySQL_Driver* driver_;

    // [변경] 복잡한 풀링 대신, 스레드들이 나눠가질 연결들을 보관만 해두는 용도
    std::vector<sql::Connection*> connections_;
    std::mutex connectionMutex_;
};