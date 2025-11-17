#include "Command.h"
#include "RoomManager.h"
#include "Persistence.h"
#include "PlayerState.h"

ICommand::~ICommand() {}

void MoveCommand::Execute(RoomManager& roomManager, Persistence& persistence) {

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