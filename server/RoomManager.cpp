#include "RoomManager.h"
#include "ClientSession.h"
#include "PlayerState.h" // PlayerState 생성 위해 필요

RoomManager::RoomManager() {
}

std::shared_ptr<GameRoom> RoomManager::CreateRoom(const std::string& title)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    int roomId = nextRoomId_++;

    auto newRoom = std::make_shared<GameRoom>(roomId, title);

    rooms_[roomId] = newRoom;

    std::cout << "[Room] Created Room " << roomId << ": " << title << std::endl;
    return newRoom;
}

void RoomManager::UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick) {
    std::lock_guard<std::mutex> lock(roomMutex_);
    for (auto& pair : rooms_) {
        auto& room = pair.second;
        room->Update(fixedDeltaTime);
        room->BroadcastStateSnapshot(serverTick);
    }
}

bool RoomManager::JoinRoom(std::shared_ptr<ClientSession> session, int targetRoomId)
{
    std::cout << "[Debug] JoinRoom Start. RoomID: " << targetRoomId << std::endl;

    if (session == nullptr)
    {
        std::cout << "[Error] Session is NULL!" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(roomMutex_);

    uint32_t sessionId = session->GetSessionId();
    std::cout << "[Debug] Session ID extracted: " << sessionId << std::endl;

    std::shared_ptr<GameRoom> targetRoom = nullptr;
    auto it = rooms_.find(targetRoomId);

    if (it == rooms_.end())
    {
        std::cout << "[Debug] Room not found. Creating new room..." << std::endl;
        std::string roomName = "Room_" + std::to_string(targetRoomId);
        targetRoom = std::make_shared<GameRoom>(targetRoomId, roomName);
        rooms_[targetRoomId] = targetRoom;
    }
    else
    {
        targetRoom = it->second;
    }

    if (targetRoom == nullptr)
    {
        return false;
    }

    if (playerToRoomMap_.count(sessionId))
    {
        int oldRoomId = playerToRoomMap_[sessionId];
        std::cout << "[Debug] Leaving old room: " << oldRoomId << std::endl;
        if (oldRoomId == targetRoomId) return true;

        if (rooms_.count(oldRoomId))
        {
            rooms_[oldRoomId]->RemovePlayer(sessionId);
        }
        playerToRoomMap_.erase(sessionId);
    }

    std::string playerName = "Unknown";
    try {
        playerName = session->GetName();
        std::cout << "[Debug] Player Name: " << playerName << std::endl;
    }
    catch (...) {
        std::cout << "[Error] Failed to get Player Name!" << std::endl;
    }

    auto newPlayerState = std::make_shared<PlayerState>(sessionId, playerName, targetRoomId);

    std::cout << "[Debug] Calling targetRoom->AddPlayer..." << std::endl;
    targetRoom->AddPlayer(newPlayerState, session);

    playerToRoomMap_[sessionId] = targetRoomId;

    std::cout << "[Debug] Calling session->SetCurrentRoom..." << std::endl;
    session->SetCurrentRoom(targetRoom);

    std::cout << "[RoomManager] Join Success!" << std::endl;
    return true;
}

void RoomManager::RemovePlayerFromCurrentRoom(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(roomMutex_);

    if (playerToRoomMap_.count(sessionId)) {
        int roomId = playerToRoomMap_[sessionId];

        playerToRoomMap_.erase(sessionId);

        if (rooms_.count(roomId)) {
            auto room = rooms_[roomId];

            room->RemovePlayer(sessionId);

            if (room->GetPlayerCount() == 0) {
                rooms_.erase(roomId);
                std::cout << "[RoomManager] Room " << roomId << " deleted." << std::endl;
            }
        }
    }
}

std::shared_ptr<GameRoom> RoomManager::GetRoom(int roomId) {
    std::lock_guard<std::mutex> lock(roomMutex_);
    auto it = rooms_.find(roomId);
    if (it != rooms_.end()) return it->second;
    return nullptr;
}

std::shared_ptr<GameRoom> RoomManager::GetRoomOfPlayer(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(roomMutex_);
    if (playerToRoomMap_.count(sessionId)) {
        int roomId = playerToRoomMap_[sessionId];
        if (rooms_.count(roomId)) {
            return rooms_[roomId];
        }
    }
    return nullptr;
}

void RoomManager::SendRoomList(std::shared_ptr<ClientSession> session)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    uint16_t roomCount = static_cast<uint16_t>(rooms_.size());
    uint16_t bodySize = sizeof(PacketRoomListRes) + (sizeof(RoomInfo) * roomCount);
    uint16_t packetSize = sizeof(GameHeader) + bodySize;

    std::vector<char> sendBuffer(packetSize);
    char* ptr = sendBuffer.data();

    GameHeader* header = reinterpret_cast<GameHeader*>(ptr);
    header->packetSize = packetSize;
    header->packetId = (uint16_t)PacketId::ROOM_LIST_RES;
    ptr += sizeof(GameHeader);

    PacketRoomListRes* res = reinterpret_cast<PacketRoomListRes*>(ptr);
    res->count = roomCount;
    ptr += sizeof(PacketRoomListRes);

    for (auto& pair : rooms_)
    {
        std::shared_ptr<GameRoom> room = pair.second;

        RoomInfo info;
        info.roomId = pair.first;
        info.userCount = room->GetPlayerCount();

        std::string title = "Game Room " + std::to_string(info.roomId);
        strncpy_s(info.title, title.c_str(), 32);

        memcpy(ptr, &info, sizeof(RoomInfo));
        ptr += sizeof(RoomInfo);
    }

    session->PushSendPacket(sendBuffer);
}