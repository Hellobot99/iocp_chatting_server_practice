#include "GameLogic.h"

GameLogic::GameLogic(LockFreeQueue<std::unique_ptr<ICommand>>& inputQueue, RoomManager& roomManager, Persistence& persistence)
    : inputQueue_(inputQueue),
    roomManager_(roomManager),
    persistence_(persistence)
{
    running_ = true;
}

//GameLogic 메인 처리 함수
void GameLogic::Run() {
    // 60FPS로 고정
    const std::chrono::milliseconds fixedTickDuration(16);
    auto nextTick = std::chrono::steady_clock::now();

    while (running_)
    {
        // 1. 입력 처리
        ProcessAllInputs();

        // 2. 게임 월드 업데이트
        // (고정 틱을 위해 GameLogicUpdate를 여기서 호출)
        GameLogicUpdate(std::chrono::duration<float>(fixedTickDuration).count());

        // 3. 다음 틱까지 대기
        nextTick += fixedTickDuration;
        std::this_thread::sleep_until(nextTick);
    }

}

// RoomManager를 통해 모든 활성화된 방의 상태를 업데이트
void GameLogic::GameLogicUpdate(float fixedDeltaTime) {
    roomManager_.UpdateAllRooms(fixedDeltaTime, FIXED_DT);
}

// 입력 큐에서 모든 명령을 꺼내 처리합니다.
void GameLogic::ProcessAllInputs() {
    std::unique_ptr<ICommand> command;

    while (inputQueue_.Pop(command))
    {
        if (command) {
            command->Execute(roomManager_,persistence_);
        }
    }
}