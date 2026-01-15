#pragma once

#include <winsock2.h>
#include <mutex>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include "Utility.h"
#include "Command.h"
#include "LockFreeQueue.h"
#include "NetProtocol.h"

#pragma comment(lib, "Ws2_32.lib")

class GameRoom;

class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
    ClientSession(SOCKET sock, uint32_t sessionId);
    ~ClientSession();

    enum { BUFFER_SIZE = 65536 };

    SOCKET GetSocket() const { return socket_; }
    uint32_t GetSessionId() const { return sessionId_; }
    void Disconnect();

    PER_IO_DATA recvIoData_;

    char inputBuffer_[BUFFER_SIZE] = {};

    void Send(PacketId id, const std::string& serializedData);
    void Send(PacketId id, void* ptr, int size);
    void PostRecv(HANDLE hIOCP);
    void PushSendPacket(const std::vector<char>& packetData);

    bool HasCompletePacket() const;
    std::unique_ptr<ICommand> DeserializeCommand();

    void FlushSend();
    void OnRecv(DWORD bytesTransferred);
    void OnSendCompleted(DWORD bytesTransferred);

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

    std::mutex lock_;
    std::string name_ = "Guest";
    std::shared_ptr<GameRoom> currentRoom_ = nullptr;

    PER_IO_DATA sendIoData_;
    int writePos_ = 0, readPos_ = 0;
    std::shared_ptr<std::vector<char>> currentSendingPacket_;

    LockFreeQueue<std::shared_ptr<std::vector<char>>> outputQueue_;

    std::atomic<bool> isSending_ = false;

    void MoveWritePos(DWORD bytes);
    void RegisterRecv();
};