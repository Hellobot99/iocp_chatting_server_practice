#pragma once
#include <cstdint>

// 모든 패킷의 기본이 되는 공통 헤더
// 명시적 바이트 오더링 및 패킹 처리가 필수적 [5]
#pragma pack(push, 1) 
struct GameHeader
{
    uint16_t packetSize;      // 전체 패킷 크기 (바이트)
    uint16_t packetId;     // 패킷의 종류 [6]
};
#pragma pack(pop)