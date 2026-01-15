#pragma once

#include <thread>
#include <chrono>
#include <memory>
#include "Utility.h"
#include "RoomManager.h"
#include "Persistence.h"
#include "Command.h"
#include "LockFreeQueue.h"

class GameLogic
{
public:
    GameLogic(LockFreeQueue<std::unique_ptr<ICommand>>& inputQueue, RoomManager& roomManager, Persistence& persistence);

    void Run();
    void Stop() { running_ = false; }

private:
    bool running_ = true;

    static constexpr float FIXED_DT = 1.0f / 60.0f;

    LockFreeQueue<std::unique_ptr<ICommand>>& inputQueue_;

    RoomManager& roomManager_;
    Persistence& persistence_;

    uint32_t currentTick_ = 0;

    void GameLogicUpdate(float fixedDeltaTime);
    void ProcessAllInputs();
};
