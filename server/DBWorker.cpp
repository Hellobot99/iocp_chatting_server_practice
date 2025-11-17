#include "DBWorker.h"
#include "Persistence.h"
#include "PersistenceRequest.h"
#include <iostream>
#include <thread>

// MySQL & Redis 헤더 (main.cpp에 있던 것들)
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <hiredis/hiredis.h>

// 전역 또는 멤버로 관리해야 할 연결 객체들 (여기선 스레드 로컬로 가정하거나 간단히 구현)
// 실제로는 Connection Pool을 쓰는 것이 좋습니다.

void ProcessSaveChat(sql::Connection* con, redisContext* redis, const PersistenceRequest& req) {
    try {
        // 1. MySQL 저장 (영구 보관)
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO chat_logs(username, message) VALUES(?, ?)")
        );
        pstmt->setString(1, req.userName); // username
        pstmt->setString(2, req.message); // message
        pstmt->execute();

        // 2. Redis 캐싱 (최신 채팅 50개 유지 - ZSET 또는 List)
        // Cache-Aside: DB에 쓰고 캐시에도 반영
        if (redis) {
            std::string value = req.userName + ": " + req.message;
            redisReply* reply = (redisReply*)redisCommand(redis, "LPUSH recent_chat %s", value.c_str());
            freeReplyObject(reply);

            reply = (redisReply*)redisCommand(redis, "LTRIM recent_chat 0 49");
            freeReplyObject(reply);
        }

        std::cout << "[DB] Chat saved for " << req.userName << std::endl;
    }
    catch (sql::SQLException& e) {
        std::cerr << "[DB Error] " << e.what() << std::endl;
    }
}
