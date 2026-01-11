#pragma once
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <hiredis/hiredis.h>
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

    bool Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass,
        const std::string& redisHost, int redisPort);
    void Stop();
    void PostRequest(std::unique_ptr<PersistenceRequest> request);

    // [신규] 동기식 인증 함수
    int AuthenticateUser(const std::string& username, const std::string& password);

    std::vector<std::string> GetRecentChats(int roomId);
    void SaveAndCacheChat(int roomId, uint32_t sessionId, const std::string& user, const std::string& msg);

    void RemoveActiveUser(const std::string& username);

private:
    void WorkerLoop();
    void ProcessSaveChat(sql::Connection* con, const PersistenceRequest& req);
    void ProcessRegister(sql::Connection* con, const PersistenceRequest& req);

    // [Redis 추가] 세션 관리 (중복 로그인 방지용)
    bool CheckAndRegisterSession(const std::string& username);

    // [신규] 커넥션 헬퍼 함수
    sql::Connection* GetConnection();
    void ReturnConnection(sql::Connection* con);

private:
    sql::mysql::MySQL_Driver* driver_;
    std::vector<sql::Connection*> connections_;
    redisContext* redisCtx_ = nullptr;

    std::string dbUrl_;
    std::string dbUser_;
    std::string dbPass_;
    std::string redisHost_;
    std::string redisPort_;

    std::vector<std::thread> workers_;
    std::queue<std::unique_ptr<PersistenceRequest>> requestQueue_;

    std::mutex queueMutex_;
    std::mutex connectionMutex_;
    std::mutex redisMutex_;
    std::condition_variable cv_;

    int threadCount_;
    bool running_;

    void InternalCacheChat(int roomId, const std::string& user, const std::string& msg);
};