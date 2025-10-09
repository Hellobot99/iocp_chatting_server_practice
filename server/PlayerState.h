#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include "Utility.h"
#include "Command.h" // ICommand를 사용하기 위함

// 서버가 관리하는 플레이어의 권한 있는 상태
class PlayerState
{
public:
    uint32_t sessionId;
    std::string username;

    // 권한 있는 위치 및 물리 상태
    Vector2 position;         // 현재 위치 [12]
    Vector2 velocity;         // 현재 속도
    float maxSpeed = 5.0f;
    int currentRoomId;

    // 클라이언트 입력 버퍼 (순서 번호가 매겨진 명령 저장)
    // GLT는 이 버퍼를 고정 틱마다 처리합니다. [7, 13]
    std::deque<std::unique_ptr<ICommand>> inputBuffer;

    PlayerState(uint32_t id, const std::string& name, int roomId)
        : sessionId(id), username(name), currentRoomId(roomId), position({ 0.0f, 0.0f }), velocity({ 0.0f, 0.0f }) {
    }

    // 결정론적 이동 계산 (GameRoom Update에서 사용)
    void ApplyMovement(float fixedDeltaTime);
};