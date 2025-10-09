#pragma once

#include <cstdint>
#include <string>
#include "Utility.h"

// 모든 패킷의 기본이 되는 공통 헤더
// 명시적 바이트 오더링 및 패킹 처리가 필수적 [5]
#pragma pack(push, 1) 
struct GameHeader
{
    uint16_t totalLength;      // 전체 패킷 크기 (바이트)
    PacketType packetType;     // 패킷의 종류 [6]
    uint32_t sequenceNumber;   // 클라이언트 입력 예측/조정을 위한 순서 번호 [7, 8]
};

// C2S_MOVE 명령 (클라이언트 -> 서버)
struct C2S_Move
{
    GameHeader header;
    Vector2 desiredVelocity; // 원하는 방향 및 속도 벡터
    // 여기에 클라이언트가 마지막으로 처리한 서버 틱 번호 등을 추가 가능
};

// C2S_CHAT 명령
struct C2S_Chat
{
    GameHeader header;
    char username[1];
    char message;
};

// S2C_STATE_SNAPSHOT 명령 (서버 -> 클라이언트)
// 모든 플레이어의 권한 있는 위치 정보를 포함
struct PlayerSnapshot
{
    uint32_t sessionId;
    Vector2 authoritativePosition; // 서버가 결정한 최종 위치 [9]
    // 델타 압축을 위해 플래그 등을 추가할 수 있음 [10]
};

struct S2C_StateSnapshot
{
    GameHeader header;
    // 현재 틱 번호 (클라이언트 롤백 및 보간에 사용)
    uint32_t serverTick;

    // 이 패킷에 포함된 플레이어 스냅샷 개수 (가변 길이 처리)
    uint16_t playerCount;
    // PlayerSnapshot 배열은 동적으로 처리
};
#pragma pack(pop)