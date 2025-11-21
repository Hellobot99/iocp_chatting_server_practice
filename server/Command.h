// ClientSession.h 맨 윗부분 수정
#pragma once
#include "Protocol.pb.h"
#include <winsock2.h>
#include <mutex>
#include <string>
#include <vector> // vector도 없으면 추가
#include <memory> // <--- 이거 필수! (unique_ptr용)
#include "Utility.h"
#include "Command.h"

class GameRoom; // 전방 선언
class PlayerState; // 전방 선언
class Persistence; // 전방 선언
class RoomManager;

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
    virtual void Execute(RoomManager& roomManager, Persistence& persistence) = 0;
};

// 2. 구체적인 명령 클래스: Move
class MoveCommand : public ICommand {
public:
    // 생성자에서 Protobuf 메시지를 받아 데이터를 꺼내옵니다.
    MoveCommand(uint32_t sessionId, const Protocol::C_Move& pkt)
        : sessionId_(sessionId)
    {
        vx_ = pkt.desired_velocity().x();
        vy_ = pkt.desired_velocity().y();
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    float vx_;
    float vy_;
};

// 3. 구체적인 명령 클래스: Chat
class ChatCommand : public ICommand {
public:
    ChatCommand(uint32_t sessionId, const Protocol::C_Chat& pkt)
        : sessionId_(sessionId)
    {
        // Protobuf string -> std::string 복사
        message_ = pkt.message();
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string message_;
};