#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include "PlayerState.h"
// #include "LockFreeQueue.h"
#include "NetProtocol.h"

class ClientSession;

class GameRoom
{
public:
    GameRoom(int id, const std::string& name);

    int GetId() const { return id_; }
    int GetPlayerCount();

    void Update(float fixedDeltaTime);

    void AddPlayer(std::shared_ptr<PlayerState> player, std::shared_ptr<ClientSession> session);
    void RemovePlayer(uint32_t sessionId);

    void BroadcastStateSnapshot(uint32_t serverTick);
    void BroadcastChat(const std::string& senderName, const std::string& message);

    std::shared_ptr<PlayerState> GetPlayer(uint32_t sessionId);

private:
    int id_;
    std::string name_;
    std::mutex roomMutex_;

    std::map<uint32_t, std::shared_ptr<PlayerState>> players_;
    std::map<uint32_t, std::shared_ptr<ClientSession>> sessions_;
};