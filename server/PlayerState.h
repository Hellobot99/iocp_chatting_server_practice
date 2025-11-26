#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include <memory>

#include "Utility.h"
#include "Command.h"
#include "NetProtocol.h"

class PlayerState
{
public:
    uint32_t sessionId;
    std::string username;

    Vector2 position;
    Vector2 velocity;
    float maxSpeed = 5.0f;
    int currentRoomId;

    std::deque<std::unique_ptr<ICommand>> inputBuffer;

    PlayerState(uint32_t id, const std::string& name, int roomId)
        : sessionId(id), username(name), currentRoomId(roomId), position({ 0.0f, 0.0f }), velocity({ 0.0f, 0.0f }) {
    }

    void ApplyMovement(float fixedDeltaTime);
};