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
#include <hiredis/hiredis.h>
#include "Utility.h"
#include "PersistenceRequest.h"
#include "Protocol.pb.h"
#include "PersistenceRequest.h"

class Persistence
{
public:
    Persistence(int threadCount);
    ~Persistence();

    bool Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass, const std::string& redisHost, int redisPort);
    void Stop();

    // GLT가 비동기 DB 작업을 요청할 때 사용 (생산자)
    void PostRequest(std::unique_ptr<PersistenceRequest> request);

    // DB 워커 스레드가 큐에서 작업을 가져갈 때 사용 (소비자)
    bool TryGetRequest(std::unique_ptr<PersistenceRequest>& request);

    // DB 워커를 위한 스레드 로컬 DB/Redis 연결을 제공하는 함수 (연결 풀링)
    sql::Connection* GetMySqlConnection();
    redisContext* GetRedisContext();

private:
    int threadCount_;
    std::vector<std::thread> workers_;
    bool running_ = true;

    // DB 요청 큐 및 동기화
    std::queue<std::unique_ptr<PersistenceRequest>> requestQueue_;
    std::mutex queueMutex_;
    std::condition_variable cv_;

    // MySQL 및 Redis 연결 정보 (초기화에 사용)
    std::string dbUrl_, dbUser_, dbPass_, redisHost_;
    int redisPort_;

    // DB 워커 스레드가 실행할 실제 작업 루프
    void WorkerLoop();
    void ProcessSaveChat(sql::Connection* con, redisContext* redis, const PersistenceRequest& req);

    // MySQL 드라이버 및 연결 관리
    sql::mysql::MySQL_Driver* driver_;
    std::vector<sql::Connection*> mysqlConnectionPool_;
    std::vector<redisContext*> redisContextPool_;
    std::mutex poolMutex_;
    sql::Connection* GetMySqlConnection();
    // DBTP 워커들은 내부적으로 Connection Pool을 사용하거나 Thread-local Connection을 유지해야 합니다.
};