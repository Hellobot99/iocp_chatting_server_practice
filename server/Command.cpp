#include "Command.h"
#include "RoomManager.h"
#include "GameRoom.h"
#include "PlayerState.h"
#include "Persistence.h"
#include "PersistenceRequest.h"

void MoveCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    // 1. 플레이어가 현재 어떤 방에 있는지 확인
    auto room = roomManager.GetRoomOfPlayer(sessionId_);

    if (room == nullptr) return; // 방이 없으면 무시

    // 2. 방에서 플레이어 객체 가져오기
    auto player = room->GetPlayer(sessionId_);
    if (player == nullptr) return;

    // 3. 속도 변경 적용 (검증 로직 추가 가능: 최대 속도 제한 등)
    player->velocity.x = vx_;
    player->velocity.y = vy_;

    // 로그 (디버깅용)
    // std::cout << "Player " << sessionId_ << " moved velocity to " << vx_ << ", " << vy_ << std::endl;
}

void ChatCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    // 요청 객체 생성 (스마트 포인터)
    auto req = std::make_unique<PersistenceRequest>();
    
    req->type = RequestType::SAVE_CHAT;
    req->sessionId = sessionId_;
    req->userName = "User_" + std::to_string(sessionId_); // 임시 이름
    req->message = message_;

    // 큐에 전송 (move)
    persistence.PostRequest(std::move(req));

    std::cout << "[Chat] " << message_ << " (Saved via DB Worker)" << std::endl;
}