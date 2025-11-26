#pragma once

#include <winsock2.h>
#include <mutex>
#include <string>
#include "Utility.h"
#include "Command.h"
#include "LockFreeQueue.h"
#include "NetProtocol.h"

#pragma comment(lib, "Ws2_32.lib")

// IOCP Completion Key로 사용될 클라이언트 세션 객체

class ClientSession
{
public:
    ClientSession(SOCKET sock, uint32_t sessionId);
    ~ClientSession();

    // 네트워크 I/O 관련
    SOCKET GetSocket() const { return socket_; }
    uint32_t GetSessionId() const { return sessionId_; }
    void Disconnect();

    // I/O 완료 시 사용되는 오버랩 데이터
    PER_IO_DATA recvIoData_;

    // TCP 스트림 처리를 위한 입력 버퍼
    std::vector<char> inputBuffer_;
    
    // 게임 상태 관련
    int currentRoomId_;
    std::string username_;

    void Send(PacketId id, const std::string& serializedData);

    // 비동기 Recv를 다시 거는 함수
    void PostRecv(HANDLE hIOCP);

    // GLT가 이 세션에게 패킷 전송을 요청할 때 사용
    void PushSendPacket(const std::vector<char>& packetData);

    // 2. 버퍼에 최소 하나의 완전한 패킷이 있는지 확인합니다.
    bool HasCompletePacket() const;

    // 3. 버퍼에서 완전한 패킷을 추출하고, ICommand 객체로 변환합니다.
    std::unique_ptr<ICommand> DeserializeCommand();

    void FlushSend();

    void OnRecv(DWORD bytesTransferred);

    void OnSendCompleted(DWORD bytesTransferred);

private:
    SOCKET socket_;
    uint32_t sessionId_;
    std::mutex lock_; // 세션 상태 보호용
    PER_IO_DATA sendIoData_;
    int writePos_, readPos_;
    std::shared_ptr<std::vector<char>> currentSendingPacket_;

    // GLT가 생성한 S2C 패킷을 담는 출력 큐 (IOCP 워커가 비동기 WSASend 실행)
    LockFreeQueue<std::shared_ptr<std::vector<char>>> outputQueue_;

    // 중복 전송 방지 플래그
    std::atomic<bool> isSending_ = false;

    void MoveWritePos(DWORD bytes);

    void RegisterRecv();
};