#include <iostream>
#include "Persistence.h"
#include "PersistenceRequest.h"

Persistence::Persistence(int threadCount)
    : threadCount_(threadCount), running_(false)
{
    driver_ = sql::mysql::get_mysql_driver_instance();
}

Persistence::~Persistence()
{
    Stop();
}

bool Persistence::Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass)
{
    dbUrl_ = dbUrl;
    dbUser_ = dbUser;
    dbPass_ = dbPass;

    try {
        // 1. 스레드 개수만큼 DB 연결 생성
        for (int i = 0; i < threadCount_; ++i) {
            sql::Connection* con = driver_->connect(dbUrl_, dbUser_, dbPass_);
            con->setSchema("chatdb"); // 스키마 설정
            connections_.push_back(con);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Persistence] MySQL Init Error: " << e.what() << std::endl;
        return false;
    }

    // 2. 워커 스레드 시작
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
    cv_.notify_all(); // 자고 있는 스레드 다 깨움

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }

    // 연결 해제
    for (auto* con : connections_) {
        delete con;
    }
    connections_.clear();
}

void Persistence::PostRequest(std::unique_ptr<PersistenceRequest> request)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        requestQueue_.push(std::move(request));
    }
    cv_.notify_one();
}

// [수정됨] 테이블 스키마(session_id, user_name, message)에 맞게 쿼리 수정
void Persistence::ProcessSaveChat(sql::Connection* con, const PersistenceRequest& req)
{
    if (!con) return;

    try {
        // MySQL 저장 수행
        // 테이블 구조: id(auto), session_id, user_name, message, created_at(auto)
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO chat_logs(session_id, user_name, message) VALUES(?, ?, ?)")
        );

        // 파라미터 바인딩 (순서 중요)
        pstmt->setInt(1, req.sessionId);      // 1번 ?: session_id
        pstmt->setString(2, req.userName);    // 2번 ?: user_name
        pstmt->setString(3, req.message);     // 3번 ?: message

        pstmt->execute();
    }
    catch (sql::SQLException& e) {
        std::cerr << "[DB Error] " << e.what() << std::endl;
    }
}

void Persistence::WorkerLoop()
{
    // 1. 내 전용 DB 연결 가져오기
    sql::Connection* myCon = nullptr;
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        if (!connections_.empty()) {
            myCon = connections_.back();
            connections_.pop_back();
        }
    }

    // 연결을 못 가져왔으면 스레드 종료 (안전장치)
    if (myCon == nullptr) {
        std::cerr << "[Persistence] Worker failed to get DB connection!" << std::endl;
        return;
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
            switch (req->type)
            {
            case RequestType::SAVE_CHAT:
                ProcessSaveChat(myCon, *req);
                break;
            }
        }
    }

    // 스레드 종료 시 연결 정리 (선택사항: 다시 풀에 넣거나 여기서 해제)
    // 현재 구조에서는 Stop()에서 일괄 삭제하므로 그냥 둡니다.
}