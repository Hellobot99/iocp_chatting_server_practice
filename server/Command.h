#pragma once

#include <memory>
#include <string>
#include "Utility.h"

class GameRoom; // 전방 선언
class PlayerState; // 전방 선언
class Persistence; // 전방 선언

//======================================================================
// 커맨드 패턴 (Command Pattern) 구현
// I/O 스레드와 GLT 로직을 분리 [14, 15]
//======================================================================

// 1. ICommand 추상 클래스
class ICommand
{
public:
    virtual ~ICommand() = default;

    // 모든 명령은 이 메서드를 통해 GLT에서 실행됩니다.
    // RoomManager와 Persistence 객체를 통해 필요한 상태를 변경합니다.
    virtual void Execute(GameRoom& room, Persistence& persistence) = 0;
};

// 2. 구체적인 명령 클래스: Move
class MoveCommand : public ICommand
{
public:
    MoveCommand(uint32_t sessionId, Vector2 desiredVelocity)
        : sessionId_(sessionId), desiredVelocity_(desiredVelocity) {
    }

    void Execute(GameRoom& room, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    Vector2 desiredVelocity_;
    // 여기에 클라이언트가 보낸 순서 번호(Sequence Number)도 저장해야 함
};

// 3. 구체적인 명령 클래스: Chat
class ChatCommand : public ICommand
{
public:
    ChatCommand(uint32_t sessionId, const std::string& msg)
        : sessionId_(sessionId), message_(msg) {
    }

    void Execute(GameRoom& room, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string message_;
};