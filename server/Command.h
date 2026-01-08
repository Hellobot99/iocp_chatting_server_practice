#pragma once

#include <winsock2.h>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include "Utility.h"

class Persistence;
class RoomManager;

class ICommand
{
public:
    virtual ~ICommand() = default;
    // 게임 로직과 DB 처리를 모두 접근할 수 있도록 인자를 받음
    virtual void Execute(RoomManager& roomManager, Persistence& persistence) = 0;
};

// [수정] 회원가입 커맨드 (신규 추가)
class RegisterCommand : public ICommand {
public:
    RegisterCommand(uint32_t sessionId, std::string username, std::string password)
        : sessionId_(sessionId), username_(std::move(username)), password_(std::move(password))
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string username_;
    std::string password_;
};

// [수정] 로그인 커맨드 (roomId 제거, password 추가)
class LoginCommand : public ICommand {
public:
    LoginCommand(uint32_t sessionId, std::string username, std::string password)
        : sessionId_(sessionId), username_(std::move(username)), password_(std::move(password))
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string username_;
    std::string password_;
};

// [수정] 방 입장 커맨드 (신규 추가: 로그인 후 호출됨)
class EnterRoomCommand : public ICommand {
public:
    EnterRoomCommand(uint32_t sessionId, int32_t roomId)
        : sessionId_(sessionId), roomId_(roomId)
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    int32_t roomId_;
};

// [수정] 방 퇴장 커맨드 (신규 추가)
class LeaveRoomCommand : public ICommand {
public:
    LeaveRoomCommand(uint32_t sessionId)
        : sessionId_(sessionId)
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
};

class MoveCommand : public ICommand {
public:
    MoveCommand(uint32_t sessionId, float vx, float vy)
        : sessionId_(sessionId), vx_(vx), vy_(vy)
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    float vx_;
    float vy_;
};

class ChatCommand : public ICommand {
public:
    ChatCommand(uint32_t sessionId, std::string message)
        : sessionId_(sessionId), message_(std::move(message))
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string message_;
};

class CreateRoomCommand : public ICommand {
public:
    CreateRoomCommand(uint32_t sessionId, std::string title)
        : sessionId_(sessionId), title_(title) {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string title_;
};

class RoomListCommand : public ICommand {
public:
    RoomListCommand(uint32_t sessionId) : sessionId_(sessionId) {}
    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
};
