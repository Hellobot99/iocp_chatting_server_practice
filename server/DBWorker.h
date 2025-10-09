#pragma once

#include <windows.h>

// DB 워커 스레드의 진입점 함수
// [21, 22]
DWORD WINAPI DBWorkerThread(LPVOID arg);