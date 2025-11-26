#pragma once

#include <map>
#include <memory>
#include <mutex>
#include "GameRoom.h"

class RoomManager
{
public:
    RoomManager();
    std::shared_ptr<GameRoom> CreateRoom(int id, const std::string& name);
    void UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick);
    bool JoinRoom(uint32_t sessionId, std::shared_ptr<ClientSession> session, int targetRoomId);
    void RemovePlayerFromCurrentRoom(uint32_t sessionId);
    std::shared_ptr<GameRoom> GetRoom(int roomId);
    std::shared_ptr<GameRoom> GetRoomOfPlayer(uint32_t sessionId);

private:
    std::map<int, std::shared_ptr<GameRoom>> rooms_;
    std::mutex roomMutex_;
    std::map<uint32_t, int> playerToRoomMap_;
};