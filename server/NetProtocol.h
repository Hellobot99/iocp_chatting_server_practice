#pragma once
#include <cstdint>

enum class PacketId : uint16_t
{
    LOGIN = 1,
    CHAT = 2,
    MOVE = 3,
    SNAPSHOT =4,
};

#pragma pack(push, 1) 

struct GameHeader
{
    uint16_t packetSize;
    uint16_t packetId;
};

struct PacketLogin
{
    // 예: 유저 ID 등 필요한 데이터
    // (지금은 비워둠)
};

// 3. 이동 패킷 구조체
struct PacketMove
{
    float vx;
    float vy;
};

struct PacketChat
{
    char msg[256];
};

#pragma pack(pop)