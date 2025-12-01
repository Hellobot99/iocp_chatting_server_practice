#pragma once

#include <winsock2.h>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include "Utility.h"
#include "Command.h"

class Persistence;
class RoomManager;

class ICommand
{
public:
    virtual ~ICommand() = default;
    virtual void Execute(RoomManager& roomManager, Persistence& persistence) = 0;
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

class LoginCommand : public ICommand {
public:
    LoginCommand(uint32_t sessionId, std::string name, int32_t roomId)
        : sessionId_(sessionId), name_(name), roomId_(roomId)
    {
    }

    void Execute(RoomManager& roomManager, Persistence& persistence) override;

private:
    uint32_t sessionId_;
    std::string name_;
    int32_t roomId_;
};