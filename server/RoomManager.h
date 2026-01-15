#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <iostream>
#include "GameRoom.h"
#include "NetProtocol.h"

class ClientSession;

class RoomManager
{
public:
    RoomManager();
    static RoomManager* Instance() { static RoomManager instance; return &instance; }

    std::shared_ptr<GameRoom> CreateRoom(const std::string& name);

    void UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick);

    bool JoinRoom(std::shared_ptr<ClientSession> session, int targetRoomId);

    void RemovePlayerFromCurrentRoom(uint32_t sessionId);

    std::shared_ptr<GameRoom> GetRoom(int roomId);
    std::shared_ptr<GameRoom> GetRoomOfPlayer(uint32_t sessionId);

    void SendRoomList(std::shared_ptr<ClientSession> session);

private:
    std::map<int, std::shared_ptr<GameRoom>> rooms_;
    std::mutex roomMutex_;
    std::map<uint32_t, int> playerToRoomMap_;
    std::atomic<int> nextRoomId_ = 1;
};