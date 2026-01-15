#pragma once

#include <winsock2.h>
#include <mutex>
#include <queue>
#include <atomic>
#include <cstdint>

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