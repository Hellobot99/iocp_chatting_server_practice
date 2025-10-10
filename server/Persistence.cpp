#include "Persistence.h"

bool Persistence::Initialize(const std::string& dbUrl, const std::string& dbUser,
    const std::string& dbPass, const std::string& redisHost, int redisPort){

    this->dbUrl_ = dbUrl;
    this->dbUser_ = dbUser;
    this->dbPass_ = dbPass;
    this->redisHost_ = redisHost;
    this->redisPort_ = redisPort;

    // 1. MySQL 드라이버 초기화
    try {
        driver_ = sql::mysql::get_mysql_driver_instance();
    }
    catch (sql::SQLException& e) {
        //... 오류 처리...
        return false;
    }

    // 2. MySQL 연결 풀 생성 (각 DB 워커 스레드에 할당될 독립적인 연결을 미리 생성)
    for (int i = 0; i < threadCount_; ++i) {
        try {
            // 새 연결 생성
            sql::Connection* con = driver_->connect(dbUrl, dbUser, dbPass);
            con->setSchema("chatdb");

            // 풀에 추가
            mysqlConnectionPool_.push_back(con);

        }
        catch (sql::SQLException& e) {
            //... 오류 처리...
            return false;
        }
    }

    // 3. Redis 컨텍스트 풀 생성
    for (int i = 0; i < threadCount_; ++i) {
        redisContext* c = redisConnect(redisHost.c_str(), redisPort);
        if (c == nullptr || c->err) {
            //... 오류 처리...
            return false;
        }
        redisContextPool_.push_back(c);
    }

    // 이 시점에서 DBTP 워커 스레드가 사용할 모든 연결이 준비됩니다.
    return true;
}

sql::Connection* Persistence::GetMySqlConnection()
{
    // GetCurrentDBThreadIndex() 역할: TLS 변수에서 인덱스를 가져옵니다.
    int workerIndex = g_current_worker_index;

    // 이 인덱스는 이 스레드가 시작될 때 할당받은 0, 1, 2,... 값입니다.
    if (workerIndex >= 0 && workerIndex < mysqlConnectionPool_.size())
    {
        return mysqlConnectionPool_[workerIndex];
    }

    return nullptr;
}

redisContext* Persistence::GetRedisContext() {

    int workerIndex = g_current_worker_index;

    if (workerIndex >= 0 && workerIndex < redisContextPool_.size())
    {
        return redisContextPool_[workerIndex];
    }

    return nullptr;
}