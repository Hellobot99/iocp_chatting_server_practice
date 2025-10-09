#pragma once

#include <winsock2.h>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstdint>

// IOCP 오버랩 I/O 데이터를 위한 구조체 (기존 코드 기반 유지)
struct PER_IO_DATA
{
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer;
    int operation; // 0: Recv, 1: Send
};

//======================================================================
// 스레드 안전/락-프리 큐 (Lock-Free Queue)
// IOCP 워커 스레드(다중 생산자)가 GLT(단일 소비자)로 명령을 전달하는 통로
// [2, 3]
//======================================================================
template <typename T>
class LockFreeQueue
{
public:
    // 다중 생산자 환경에서 경합을 최소화하는 푸시 (Lock-Free 또는 최소화된 락 사용 가정)
    bool Push(T item);

    // 단일 소비자 환경에서 안전하게 아이템을 가져오는 팝
    bool Pop(T& item);

private:
    // 실제 구현은 C++ <atomic>이나 OS-level primitives를 사용해야 함
    // (여기서는 스켈레톤 코드를 위해 std::mutex를 사용하지만, 실제로는 Lock-Free 구현이 필수적임)
    std::mutex mutex_;
    std::queue<T> queue_;
};

//======================================================================
// 기본 상수 및 Enum
//======================================================================
enum class PacketType : uint16_t
{
    // C2S (Client to Server)
    C2S_LOGIN_REQ,
    C2S_CHAT,
    C2S_MOVE,
    C2S_ROOM_JOIN,

    // S2C (Server to Client)
    S2C_LOGIN_ACK,
    S2C_STATE_SNAPSHOT,
    S2C_CHAT
};

// 위치 벡터 (Fixed Timestep 시뮬레이션을 위해 float 또는 고정 소수점 사용 권장)
struct Vector2
{
    float x;
    float y;
};