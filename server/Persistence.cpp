#include "Persistence.h"
#include "PersistenceRequest.h"
#include <iostream>

Persistence::Persistence(int threadCount)
    : threadCount_(threadCount), running_(false)
{
    driver_ = sql::mysql::get_mysql_driver_instance();
}

Persistence::~Persistence()
{
    Stop();
}

bool Persistence::Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass, const std::string& redisHost, int redisPort)
{
    dbUrl_ = dbUrl;
    dbUser_ = dbUser;
    dbPass_ = dbPass;
    redisHost_ = redisHost;
    redisPort_ = redisPort;

    try {
        // 1. 스레드 개수만큼 DB 연결을 미리 생성 (풀링)
        for (int i = 0; i < threadCount_; ++i) {
            sql::Connection* con = driver_->connect(dbUrl_, dbUser_, dbPass_);
            con->setSchema("chatdb");
            mysqlConnectionPool_.push_back(con);

            redisContext* ctx = redisConnect(redisHost_.c_str(), redisPort_);
            if (ctx == nullptr || ctx->err) {
                std::cerr << "[Persistence] Redis Connect Failed\n";
                if (ctx) redisFree(ctx);
                redisContextPool_.push_back(nullptr);
            }
            else {
                redisContextPool_.push_back(ctx);
            }
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Persistence] MySQL Init Error: " << e.what() << std::endl;
        return false;
    }

    // 2. 워커 스레드 시작 (DBWorkerThread 함수 대신 WorkerLoop 멤버 함수 실행)
    running_ = true;
    for (int i = 0; i < threadCount_; ++i) {
        workers_.emplace_back(&Persistence::WorkerLoop, this);
    }

    std::cout << "[Persistence] Initialized with " << threadCount_ << " threads.\n";
    return true;
}

void Persistence::Stop()
{
    if (!running_) return;
    running_ = false;
    cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }

    std::lock_guard<std::mutex> lock(poolMutex_);
    for (auto* con : mysqlConnectionPool_) delete con;
    mysqlConnectionPool_.clear();
    for (auto* ctx : redisContextPool_) if (ctx) redisFree(ctx);
    redisContextPool_.clear();
}

void Persistence::PostRequest(std::unique_ptr<PersistenceRequest> request)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    cv_.notify_one();
}

bool Persistence::TryGetRequest(std::unique_ptr<PersistenceRequest>& request)
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (requestQueue_.empty()) return false;

    request = std::move(requestQueue_.front());
    requestQueue_.pop();
    return true;
}

// ★ 기존 ProcessSaveChat 로직을 여기로 가져옴 (헬퍼 함수)
void Persistence::ProcessSaveChat(sql::Connection* con, redisContext* redis, const PersistenceRequest& req)
{
    try {
        // 1. MySQL 저장
        if (con) {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                con->prepareStatement("INSERT INTO chat_logs(username, message) VALUES(?, ?)")
            );
            pstmt->setString(1, req.userName);
            pstmt->setString(2, req.message);
            pstmt->execute();
        }

        // 2. Redis 캐싱 (Cache-Aside)
        if (redis) {
            std::string val = req.userName + ": " + req.message;
            redisReply* reply = (redisReply*)redisCommand(redis, "LPUSH recent_chat %s", val.c_str());
            freeReplyObject(reply);
            reply = (redisReply*)redisCommand(redis, "LTRIM recent_chat 0 49");
            freeReplyObject(reply);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[DB Error] " << e.what() << std::endl;
    }
}

void Persistence::WorkerLoop()
{
    // 스레드별 전용 연결 가져오기
    sql::Connection* myCon = nullptr;
    redisContext* myRedis = nullptr;

    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        if (!mysqlConnectionPool_.empty()) {
            myCon = mysqlConnectionPool_.back();
            mysqlConnectionPool_.pop_back();
        }
        if (!redisContextPool_.empty()) {
            myRedis = redisContextPool_.back();
            redisContextPool_.pop_back();
        }
    }

    while (running_)
    {
        std::unique_ptr<PersistenceRequest> req = nullptr;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this] { return !requestQueue_.empty() || !running_; });

            if (!running_ && requestQueue_.empty()) break;

            if (!requestQueue_.empty()) {
                req = std::move(requestQueue_.front());
                requestQueue_.pop();
            }
        }

        if (req) {
            // ★ 여기서 요청 타입에 따라 분기 처리
            switch (req->type)
            {
            case RequestType::SAVE_CHAT:
                ProcessSaveChat(myCon, myRedis, *req);
                break;
                // case RequestType::LOAD_USER_DATA: ...
            }
        }
    }
}