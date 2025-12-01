#include "Command.h"
#include "RoomManager.h"
#include "GameRoom.h"
#include "PlayerState.h"
#include "Persistence.h"
#include "PersistenceRequest.h"
#include "Server.h"       
#include "ClientSession.h"


extern Server* g_Server;

void MoveCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    // 1. 세션 가져오기
    auto session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    // 2. [최적화] 세션에서 바로 방 가져오기 (RoomManager 검색 비용 절약)
    auto room = session->GetCurrentRoom();
    if (room == nullptr) return;

    // 3. 방에서 플레이어 객체 가져오기
    // (Room 클래스 구현에 따라 다르겠지만, 보통 Room 안에는 PlayerList가 있을 것임)
    auto player = room->GetPlayer(sessionId_);

    if (player)
    {
        player->velocity.x = vx_;
        player->velocity.y = vy_;
    }
}

void ChatCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    // 1. 세션에서 현재 방과 이름을 안전하게 가져옴
    auto room = session->GetCurrentRoom();
    std::string senderName = session->GetName(); // 여기서 미리 가져옴

    // 2. 방이 존재할 때만 방송 + DB 저장
    if (room)
    {
        // (1) 채팅 브로드캐스트
        room->BroadcastChat(senderName, message_);

        // (2) DB 저장 요청 (방에 뿌려졌을 때만 저장하는 게 맞음)
        auto req = std::make_unique<PersistenceRequest>();
        req->type = RequestType::SAVE_CHAT;
        req->sessionId = sessionId_;
        req->userName = senderName; // 이제 사용 가능
        req->message = message_;

        persistence.PostRequest(std::move(req));

        // std::cout << "[Chat] " << senderName << ": " << message_ << std::endl;
    }
    else
    {
        // 방에 없는 상태에서 채팅 시도 -> 에러 처리 or 무시
        std::cout << "[Logic Error] User " << senderName << " is not in any room." << std::endl;
    }
}

void LoginCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    std::shared_ptr<ClientSession> session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    // 1. 이름 설정
    session->SetName(name_);

    std::cout << "[Logic] Trying to Join Room... (Session: " << sessionId_
        << ", TargetRoom: " << roomId_ << ")" << std::endl;

    // 2. [수정] 인자에서 sessionId_ 제거 (session 안에 있음)
    // JoinRoom 내부에서 "방 찾기 -> 입장 -> session->SetCurrentRoom(room)" 까지 다 처리하도록 구현
    bool success = roomManager.JoinRoom(session, roomId_);

    if (success)
    {
        // JoinRoom 안에서 세션에 방 정보를 넣었다면, 여기선 로그만 남기면 됩니다.
        std::cout << "[Logic] User " << name_ << " joined Room " << roomId_ << std::endl;
    }
    else
    {
        std::cout << "[Logic] Failed to join room " << roomId_ << std::endl;
    }
}