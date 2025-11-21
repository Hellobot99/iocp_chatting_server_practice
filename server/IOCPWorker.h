#pragma once

#include <winsock2.h>
#include <windows.h>

// IOCP 워커 스레드의 진입점 함수
// [4, 11]
DWORD WINAPI IOCPWorkerThread(LPVOID arg);