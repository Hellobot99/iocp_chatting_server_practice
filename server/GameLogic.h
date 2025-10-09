#pragma once

#include <thread>
#include <chrono>
#include <memory>
#include "Utility.h"
#include "RoomManager.h"
#include "Persistence.h"
#include "Command.h"

//======================================================================
// 게임 로직 스레드 (GLT) 클래스
// 모든 게임 상태 변경의 권한을 가지며 단일 스레드로 실행됨
//======================================================================
class GameLogic
{
public:
    GameLogic(LockFreeQueue<std::unique_ptr<ICommand>>& inputQueue, RoomManager& roomManager, Persistence& persistence);

    // GLT 메인 루프 진입점
    void Run();
    void Stop() { running_ = false; }

private:
    bool running_ = true;

    // 고정 시간 단계 (60 Tps) [19]
    static constexpr float FIXED_DT = 1.0f / 60.0f;

    // I/O 워커로부터 커맨드를 받는 큐
    LockFreeQueue<std::unique_ptr<ICommand>>& inputQueue_;

    // 게임 세계 관리자
    RoomManager& roomManager_;

    // 비동기 DB 처리를 위한 지속성 시스템
    Persistence& persistence_;

    uint32_t currentTick_ = 0;

    // 핵심 게임 업데이트 로직 (Fixed Timestep)
    void GameLogicUpdate(float fixedDeltaTime);

    // 입력 큐에서 모든 명령을 꺼내 처리합니다.
    void ProcessAllInputs();
};

// GLT 스레드의 진입점 함수
DWORD WINAPI GameLogicThread(LPVOID arg);