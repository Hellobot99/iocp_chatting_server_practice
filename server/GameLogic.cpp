#include "GameLogic.h"

GameLogic::GameLogic(LockFreeQueue<std::unique_ptr<ICommand>>& inputQueue, RoomManager& roomManager, Persistence& persistence)
    : inputQueue_(inputQueue),
    roomManager_(roomManager),
    persistence_(persistence)
{
    running_ = true;
}

void GameLogic::Run() {
    const std::chrono::milliseconds fixedTickDuration(16);
    auto nextTick = std::chrono::steady_clock::now();

    while (running_)
    {
        ProcessAllInputs();

        GameLogicUpdate(std::chrono::duration<float>(fixedTickDuration).count());

        currentTick_++;

        nextTick += fixedTickDuration;
        std::this_thread::sleep_until(nextTick);
    }
}

void GameLogic::GameLogicUpdate(float fixedDeltaTime) {
    roomManager_.UpdateAllRooms(fixedDeltaTime, currentTick_);
}

void GameLogic::ProcessAllInputs() {
    std::unique_ptr<ICommand> command;

    while (inputQueue_.Pop(command))
    {
        if (command) {
            command->Execute(roomManager_, persistence_);
        }
    }
}