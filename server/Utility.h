#pragma once

#include <winsock2.h>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstdint>

// IOCP 오버랩 I/O 데이터를 위한 구조체 (기존 코드 기반 유지)
struct PER_IO_DATA {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[1024];
    int operation; // 0: recv, 1: send
};

struct Vector2
{
    float x;
    float y;
};