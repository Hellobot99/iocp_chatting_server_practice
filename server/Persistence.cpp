#include <iostream>
#include "Persistence.h"
#include "PersistenceRequest.h"
#include "NetProtocol.h"
#include "Server.h"
#include "ClientSession.h"

extern Server* g_Server;

Persistence::Persistence(int threadCount)
    : threadCount_(threadCount), running_(false)
{
    driver_ = sql::mysql::get_mysql_driver_instance();
}

Persistence::~Persistence()
{
    Stop();
}

bool Persistence::Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass,
    const std::string& redisHost, int redisPort)
{
    // 1. 전달받은 정보를 멤버 변수에 저장 (워커 스레드나 다른 함수에서 사용하기 위함)
    dbUrl_ = dbUrl;
    dbUser_ = dbUser;
    dbPass_ = dbPass;
    redisHost_ = redisHost;
    redisPort_ = redisPort;

    try {
        // 2. MySQL 커넥션 풀 초기화
        for (int i = 0; i < threadCount_; ++i) {
            sql::Connection* con = driver_->connect(dbUrl_, dbUser_, dbPass_);
            con->setSchema("chatdb");
            connections_.push_back(con);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Persistence] MySQL Init Error: " << e.what() << std::endl;
        return false;
    }

    // 3. Redis 공용 컨텍스트 초기화 (InternalCacheChat 등에서 사용)
    redisCtx_ = redisConnect(redisHost_.c_str(), redisPort);
    if (redisCtx_ == nullptr || redisCtx_->err) {
        std::cerr << "[Persistence] Redis Connection Failed!" << std::endl;
        // Redis가 필수 요소라면 여기서 false를 리턴하여 서버 실행을 중단할 수 있습니다.
    }

    // 4. 워커 스레드 생성 및 실행
    running_ = true;
    for (int i = 0; i < threadCount_; ++i) {
        workers_.emplace_back(&Persistence::WorkerLoop, this);
    }

    std::cout << "[Persistence] Initialized with " << threadCount_ << " DB threads and Redis connection.\n";
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

    if (redisCtx_) {
        redisReply* reply = (redisReply*)redisCommand(redisCtx_, "DEL active_users");
        if (reply) freeReplyObject(reply);
        std::cout << "[Persistence] Redis active_users cleared." << std::endl;

        redisFree(redisCtx_);
        redisCtx_ = nullptr;
    }

    // 풀에 남아있는 커넥션 삭제
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

// -------------------------------------------------------------
// [Connection Helper] 커넥션 가져오기 / 반납하기
// -------------------------------------------------------------
sql::Connection* Persistence::GetConnection()
{
    std::lock_guard<std::mutex> lock(connectionMutex_);

    if (connections_.empty())
    {
        // 풀이 비어있으면(워커들이 다 가져갔거나 부족하면) 임시로 하나 생성
        try {
            sql::Connection* con = driver_->connect(dbUrl_, dbUser_, dbPass_);
            con->setSchema("chatdb");
            return con;
        }
        catch (sql::SQLException& e) {
            std::cerr << "[DB Error] Create Temp Connection Failed: " << e.what() << std::endl;
            return nullptr;
        }
    }

    sql::Connection* con = connections_.back();
    connections_.pop_back();
    return con;
}

void Persistence::ReturnConnection(sql::Connection* con)
{
    std::lock_guard<std::mutex> lock(connectionMutex_);
    connections_.push_back(con);
}

// -------------------------------------------------------------
// [AuthenticateUser] 동기식 로그인 인증 (메인 로직용)
// -------------------------------------------------------------
int Persistence::AuthenticateUser(const std::string& username, const std::string& password)
{
    sql::Connection* conn = nullptr;
    int dbId = -1;

    try
    {
        // 1. [MySQL] 커넥션 빌려오기 & 쿼리 준비 (기존 코드 유지)
        conn = GetConnection();
        if (conn == nullptr) return -1;

        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT id FROM User WHERE username = ? AND password = ?")
        );
        pstmt->setString(1, username);
        pstmt->setString(2, password);

        // 2. [MySQL] 실행 및 결과 확인
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next())
        {
            dbId = res->getInt("id");
        }

        ReturnConnection(conn);
    }
    catch (sql::SQLException& e)
    {
        std::cout << "[DB Error] AuthenticateUser: " << e.what() << std::endl;
        if (conn) delete conn;
        return -1;
    }

    // 3. [Redis] MySQL 인증에 성공했다면, 중복 로그인 검사 (CheckAndRegisterSession 호출)
    if (dbId != -1)
    {
        // Redis에 등록을 시도합니다. 이미 접속 중이라면 false가 반환되게 구현합니다.
        if (CheckAndRegisterSession(username) == false)
        {
            std::cout << "[Login Fail] User already logged in: " << username << std::endl;
            return -1;
        }
    }

    return dbId;
}

void Persistence::SaveAndCacheChat(int roomId, uint32_t sessionId, const std::string& user, const std::string& msg) {
    // 1. MySQL 저장은 비동기로 요청 (기존 WorkerLoop 활용)
    auto req = std::make_unique<PersistenceRequest>();
    req->type = RequestType::SAVE_CHAT;
    req->sessionId = sessionId;
    req->username = user;
    req->message = msg;
    PostRequest(std::move(req));

    // 2. Redis 캐싱은 즉각 수행 (속도가 빠르므로 동기 처리 가능)
    InternalCacheChat(roomId, user, msg);
}

void Persistence::InternalCacheChat(int roomId, const std::string& user, const std::string& msg) {
    std::lock_guard<std::mutex> lock(redisMutex_);
    if (!redisCtx_) return;

    std::string key = "room:chat:" + std::to_string(roomId);
    std::string data = user + ":" + msg;

    // Redis List에 저장하고 50개로 제한
    freeReplyObject(redisCommand(redisCtx_, "LPUSH %s %s", key.c_str(), data.c_str()));
    freeReplyObject(redisCommand(redisCtx_, "LTRIM %s 0 49", key.c_str()));
}


// -------------------------------------------------------------
// [Process Functions]
// -------------------------------------------------------------
void Persistence::ProcessSaveChat(sql::Connection* con, const PersistenceRequest& req)
{
    if (!con) return;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO chat_logs(session_id, user_name, message) VALUES(?, ?, ?)")
        );
        pstmt->setInt(1, req.sessionId);
        pstmt->setString(2, req.username);
        pstmt->setString(3, req.message);
        pstmt->execute();
    }
    catch (sql::SQLException& e) {
        std::cerr << "[DB Error/Chat] " << e.what() << std::endl;
    }
}

void Persistence::ProcessRegister(sql::Connection* con, const PersistenceRequest& req)
{
    bool success = false;
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO User(username, password) VALUES(?, ?)")
        );
        pstmt->setString(1, req.username);
        pstmt->setString(2, req.password);
        pstmt->executeUpdate();
        success = true;
        std::cout << "[DB] Registered User: " << req.username << std::endl;
    }
    catch (sql::SQLException& e) {
        if (e.getErrorCode() == 1062) {
            std::cout << "[DB] Register Failed (Duplicate): " << req.username << std::endl;
        }
        else {
            std::cerr << "[DB Error/Register] " << e.what() << std::endl;
        }
        success = false;
    }

    auto session = g_Server->GetSession(req.sessionId);
    if (session) {
        PacketRegisterRes res;
        res.success = success;
        session->Send(PacketId::REGISTER_RES, &res, sizeof(res));
    }
}

// -------------------------------------------------------------
// [Worker Loop]
// -------------------------------------------------------------
void Persistence::WorkerLoop()
{
    // 워커 스레드는 시작할 때 전용 커넥션을 하나 가져갑니다.
    sql::Connection* myCon = nullptr;

    // GetConnection을 써도 되지만, 워커는 죽을 때까지 반납을 안 하므로 
    // 기존 로직(직접 pop)을 유지하거나 GetConnection 호출 후 반납 안 하면 됩니다.
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        if (!connections_.empty()) {
            myCon = connections_.back();
            connections_.pop_back();
        }
        else {
            // 없으면 생성
            try {
                myCon = driver_->connect(dbUrl_, dbUser_, dbPass_);
                myCon->setSchema("chatdb");
            }
            catch (...) {}
        }
    }

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
            case RequestType::REGISTER:
                ProcessRegister(myCon, *req);
                break;
            case RequestType::LOGIN:
                // ProcessLogin(myCon, *req); // [참고] 동기식을 쓰면 이건 주석 처리해도 됨
                break;
            }
        }
    }

    // 워커 종료 시 커넥션 해제
    delete myCon;
}

std::vector<std::string> Persistence::GetRecentChats(int roomId) {
    std::lock_guard<std::mutex> lock(redisMutex_);
    std::vector<std::string> chats;
    if (!redisCtx_) return chats;

    std::string key = "room:chat:" + std::to_string(roomId);
    redisReply* reply = (redisReply*)redisCommand(redisCtx_, "LRANGE %s 0 -1", key.c_str());

    if (reply && reply->type == REDIS_REPLY_ARRAY) {
        for (int i = (int)reply->elements - 1; i >= 0; --i) {
            chats.push_back(reply->element[i]->str);
        }
    }
    freeReplyObject(reply);
    return chats;
}

bool Persistence::CheckAndRegisterSession(const std::string& username)
{
    std::lock_guard<std::mutex> lock(redisMutex_);
    if (!redisCtx_) return true;

    // "active_users"라는 키의 Set에 유저네임을 추가 시도
    redisReply* reply = (redisReply*)redisCommand(redisCtx_, "SADD active_users %s", username.c_str());

    bool isNewLogin = false;
    if (reply)
    {
        // integer가 1이면 신규 등록(성공), 0이면 이미 존재(중복)
        if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1)
        {
            isNewLogin = true;
        }
        freeReplyObject(reply);
    }

    return isNewLogin;
}

void Persistence::RemoveActiveUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(redisMutex_);
    if (!redisCtx_) return;

    freeReplyObject(redisCommand(redisCtx_, "SREM active_users %s", username.c_str()));
    std::cout << "[Redis] Removed active session: " << username << std::endl;
}