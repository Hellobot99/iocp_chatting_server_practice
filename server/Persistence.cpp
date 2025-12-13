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

bool Persistence::Initialize(const std::string& dbUrl, const std::string& dbUser, const std::string& dbPass)
{
    dbUrl_ = dbUrl;
    dbUser_ = dbUser;
    dbPass_ = dbPass;

    try {
        // 일단 스레드 개수만큼 커넥션을 만들어둡니다.
        for (int i = 0; i < threadCount_; ++i) {
            sql::Connection* con = driver_->connect(dbUrl_, dbUser_, dbPass_);
            con->setSchema("chatdb"); // DB 이름 확인
            connections_.push_back(con);
        }
    }
    catch (sql::SQLException& e) {
        std::cerr << "[Persistence] MySQL Init Error: " << e.what() << std::endl;
        return false;
    }

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
        // 1. 커넥션 빌려오기
        conn = GetConnection();
        if (conn == nullptr) return -1;

        // 2. 쿼리 준비
        std::unique_ptr<sql::PreparedStatement> pstmt(
            conn->prepareStatement("SELECT id FROM User WHERE username = ? AND password = ?")
        );

        pstmt->setString(1, username);
        pstmt->setString(2, password);

        // 3. 실행
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        // 4. 결과 확인
        if (res->next())
        {
            dbId = res->getInt("id");
        }

        // 5. 커넥션 반납
        ReturnConnection(conn);
    }
    catch (sql::SQLException& e)
    {
        std::cout << "[DB Error] AuthenticateUser: " << e.what() << std::endl;
        // 에러난 커넥션은 재사용하지 않고 삭제하는 것이 안전 (여기서는 간단히 처리)
        if (conn) delete conn;
        return -1;
    }

    return dbId;
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

// [구버전] 비동기 로그인 (이제 AuthenticateUser를 쓴다면 안 써도 됨)
void Persistence::ProcessLogin(sql::Connection* con, const PersistenceRequest& req)
{
    // ... (기존 코드 유지) ...
    // 만약 LoginCommand에서 AuthenticateUser만 쓴다면 이 함수는 호출되지 않습니다.
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