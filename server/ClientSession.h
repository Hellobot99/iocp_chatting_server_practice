#pragma once

#include <winsock2.h>
#include <mutex>
#include <string>
#include "Utility.h"

// IOCP Completion Key로 사용될 클라이언트 세션 객체
// [4]
class ClientSession
{
public:
    ClientSession(SOCKET sock, uint32_t sessionId);
    ~ClientSession();

    // 네트워크 I/O 관련
    SOCKET GetSocket() const { return socket_; }
    uint32_t GetSessionId() const { return sessionId_; }

    // I/O 완료 시 사용되는 오버랩 데이터
    PER_IO_DATA recvIoData_;

    // TCP 스트림 처리를 위한 입력 버퍼
    std::vector<char> inputBuffer_;

    // GLT가 생성한 S2C 패킷을 담는 출력 큐 (IOCP 워커가 비동기 WSASend 실행)
    LockFreeQueue<std::vector<char>> outputQueue_;

    // 게임 상태 관련
    int currentRoomId_;
    std::string username_;

    // 비동기 Recv를 다시 거는 함수
    void PostRecv(HANDLE hIOCP);

    // GLT가 이 세션에게 패킷 전송을 요청할 때 사용
    void PushSendPacket(const std::vector<char>& packetData);

private:
    SOCKET socket_;
    uint32_t sessionId_;
    std::mutex lock_; // 세션 상태 보호용
};