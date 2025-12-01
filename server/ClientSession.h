#pragma once

#include <winsock2.h>
#include <mutex>
#include <string>
#include <vector>
#include <memory>       // shared_ptr, enable_shared_from_this
#include <atomic>       // atomic
#include "Utility.h"
#include "Command.h"
#include "LockFreeQueue.h"
#include "NetProtocol.h"

#pragma comment(lib, "Ws2_32.lib")

class GameRoom; // 전방 선언

// [수정 1] enable_shared_from_this 상속 추가 (필수!)
class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
    ClientSession(SOCKET sock, uint32_t sessionId);
    ~ClientSession();

    enum { BUFFER_SIZE = 65536 };

    // 네트워크 I/O 관련
    SOCKET GetSocket() const { return socket_; }
    uint32_t GetSessionId() const { return sessionId_; }
    void Disconnect();

    // I/O 완료 시 사용되는 오버랩 데이터
    PER_IO_DATA recvIoData_;

    // TCP 스트림 처리를 위한 입력 버퍼
    char inputBuffer_[BUFFER_SIZE] = {};

    void Send(PacketId id, const std::string& serializedData);
    void PostRecv(HANDLE hIOCP);
    void PushSendPacket(const std::vector<char>& packetData);

    bool HasCompletePacket() const;
    std::unique_ptr<ICommand> DeserializeCommand();

    void FlushSend();
    void OnRecv(DWORD bytesTransferred);
    void OnSendCompleted(DWORD bytesTransferred);

    // [수정 2] 중복 제거 및 통일된 Getter/Setter
    void SetName(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(lock_);
        name_ = name;
    }

    std::string GetName()
    {
        std::lock_guard<std::mutex> lock(lock_);
        return name_;
    }

    void SetCurrentRoom(std::shared_ptr<GameRoom> room)
    {
        std::lock_guard<std::mutex> lock(lock_);
        currentRoom_ = room;
    }

    std::shared_ptr<GameRoom> GetCurrentRoom()
    {
        std::lock_guard<std::mutex> lock(lock_);
        return currentRoom_;
    }

private:
    SOCKET socket_;
    uint32_t sessionId_;

    std::mutex lock_;       // [수정 3] lock_ 하나로 통일
    std::string name_ = "Guest";
    std::shared_ptr<GameRoom> currentRoom_ = nullptr; // [수정 4] 변수명 통일

    PER_IO_DATA sendIoData_;
    int writePos_ = 0, readPos_ = 0;
    std::shared_ptr<std::vector<char>> currentSendingPacket_;

    // GLT가 생성한 S2C 패킷을 담는 출력 큐
    LockFreeQueue<std::shared_ptr<std::vector<char>>> outputQueue_;

    std::atomic<bool> isSending_ = false;

    void MoveWritePos(DWORD bytes);
    void RegisterRecv();
};