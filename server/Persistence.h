#pragma once
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "PersistenceRequest.h"

class Persistence
{
public:
    Persistence(int threadCount);
    ~Persistence();

    bool Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass);
    void Stop();
    void PostRequest(std::unique_ptr<PersistenceRequest> request);

    // [신규] 동기식 인증 함수
    int AuthenticateUser(const std::string& username, const std::string& password);

private:
    void WorkerLoop();
    void ProcessSaveChat(sql::Connection* con, const PersistenceRequest& req);
    void ProcessRegister(sql::Connection* con, const PersistenceRequest& req);
    void ProcessLogin(sql::Connection* con, const PersistenceRequest& req); // 기존 비동기 (필요 없다면 제거 가능)

    // [신규] 커넥션 헬퍼 함수
    sql::Connection* GetConnection();
    void ReturnConnection(sql::Connection* con);

private:
    sql::mysql::MySQL_Driver* driver_;
    std::vector<sql::Connection*> connections_; // 커넥션 풀

    std::string dbUrl_;
    std::string dbUser_;
    std::string dbPass_;

    std::vector<std::thread> workers_;
    std::queue<std::unique_ptr<PersistenceRequest>> requestQueue_;

    std::mutex queueMutex_;
    std::mutex connectionMutex_; // 커넥션 풀 보호용
    std::condition_variable cv_;

    int threadCount_;
    bool running_;
};