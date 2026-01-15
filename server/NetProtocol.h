#pragma once
#include <cstdint>

enum class PacketId : uint16_t
{
    // [로그인/계정 관련]
    REGISTER_REQ = 1,    // 회원가입 요청
    REGISTER_RES = 2,    // 회원가입 결과
    LOGIN_REQ = 3,       // 로그인 요청
    LOGIN_RES = 4,       // 로그인 결과

    // [인게임/로비 관련]
    ENTER_ROOM = 5,      // 방 입장 요청 (로그인 성공 후)
    LEAVE_ROOM = 6,      // 방 퇴장

    // [게임 플레이]
    CHAT = 7,
    MOVE = 8,
    SNAPSHOT = 9,

    ROOM_LIST_REQ = 10,
    ROOM_LIST_RES = 11,

    CREATE_ROOM_REQ = 12,
    CREATE_ROOM_RES = 13,

    LOGOUT_REQ = 14,
};

#pragma pack(push, 1) 

struct GameHeader
{
    uint16_t packetSize;
    uint16_t packetId;
};

struct PacketRegisterReq
{
    char username[50];
    char password[50];
};

struct PacketRegisterRes
{
    bool success;
};

struct PacketLoginReq
{
    char username[50];
    char password[50];
};

struct PacketLoginRes
{
    bool success;
    uint32_t playerId;
};

struct PacketEnterRoom
{
    int32_t roomId;
};

struct PacketLeaveRoom
{
    uint32_t playerId;
};

struct PacketMove
{
    float vx;
    float vy;
    // float x, y;
};

struct PacketChat
{
    uint32_t playerId;
    char msg[256];
};

struct RoomInfo
{
    int32_t roomId;
    int32_t userCount;
    char title[32];
};

struct PacketRoomListRes
{
    int32_t count;
};

struct PacketCreateRoomReq
{
    char title[32];
};

struct PacketCreateRoomRes
{
    bool success;
    int32_t roomId;
};

#pragma pack(pop)