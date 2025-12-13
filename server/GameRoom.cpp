#include "GameRoom.h"
#include "ClientSession.h"
#include "NetProtocol.h"
#include <iostream>
#include <cstring> // memcpy

GameRoom::GameRoom(int id, const std::string& name)
    : id_(id), name_(name)
{
}

int GameRoom::GetPlayerCount() {
    std::lock_guard<std::mutex> lock(roomMutex_);

    return static_cast<int>(players_.size());
}

void GameRoom::Update(float fixedDeltaTime)
{
    // 멀티스레드 환경이므로 Update 중에도 플레이어가 나가거나 들어올 수 있음
    // 안전하게 락을 걸거나, 복사본을 만들어 돌리는 게 좋음
    std::lock_guard<std::mutex> lock(roomMutex_);

    for (auto& pair : players_) {
        auto& player = pair.second;
        player->ApplyMovement(fixedDeltaTime);
    }
}

void GameRoom::AddPlayer(std::shared_ptr<PlayerState> player, std::shared_ptr<ClientSession> session)
{
    std::lock_guard<std::mutex> lock(roomMutex_);
    players_[player->sessionId] = player;
    sessions_[player->sessionId] = session;
    std::cout << "Session " << player->sessionId << " joined Room " << id_ << std::endl;
}

void GameRoom::RemovePlayer(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    auto it = players_.find(sessionId);
    if (it != players_.end())
    {
        PacketLeaveRoom leavePacket;
        leavePacket.playerId = sessionId;

        BroadcastLocked(PacketId::LEAVE_ROOM, leavePacket, sessionId);
    }

    size_t removedCount = players_.erase(sessionId);
    sessions_.erase(sessionId);

    if (removedCount == 0)
    {
        std::cout << "[Error] Player " << sessionId << " Not Found" << std::endl;
    }
    else
    {
        std::cout << "[Success] Player " << sessionId << " Removed." << std::endl;
    }
}

void GameRoom::BroadcastStateSnapshot(uint32_t serverTick)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    if (sessions_.empty()) return;

    struct PlayerPosData {
        uint32_t id;
        float x;
        float y;
    };

    size_t count = players_.size();
    size_t dataSize = sizeof(uint32_t) + (count * sizeof(PlayerPosData)); // Count + Data

    std::string sendBuffer;
    sendBuffer.resize(dataSize);
    char* ptr = &sendBuffer[0];

    // 2. 플레이어 수 기록
    uint32_t count32 = static_cast<uint32_t>(count);
    std::memcpy(ptr, &count32, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    // 3. 각 플레이어 위치 기록
    for (auto& pair : players_) {
        auto& p = pair.second;

        PlayerPosData data;
        data.id = p->sessionId;
        data.x = p->position.x;
        data.y = p->position.y;

        std::memcpy(ptr, &data, sizeof(PlayerPosData));
        ptr += sizeof(PlayerPosData);
    }

    // 4. 전송 (PacketId::MOVE 패킷 재활용하거나 S_SNAPSHOT 패킷 ID를 새로 파야 함)
    // 여기선 일단 MOVE로 가정하거나, NetProtocol에 SNAPSHOT을 추가했다고 가정
    // (주의: PacketId::MOVE를 쓰면 클라가 파싱할 때 헷갈릴 수 있으니 구분 필요)
    for (auto& pair : sessions_) {
        // 임시로 MOVE 패킷 ID 사용 (나중에 SNAPSHOT 추가 권장)
        pair.second->Send(PacketId::SNAPSHOT, sendBuffer);
    }
}


void GameRoom::BroadcastChat(const std::string& senderName, const std::string& message)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    std::string finalMsg = senderName + ": " + message;
    if (finalMsg.size() >= 256) finalMsg = finalMsg.substr(0, 255);

    // 1. 패킷 생성
    PacketChat packetData;
    packetData.playerId = 0;
    std::memset(packetData.msg, 0, sizeof(packetData.msg));
    std::memcpy(packetData.msg, finalMsg.c_str(), finalMsg.size());

    BroadcastLocked(PacketId::CHAT, packetData);
}

std::shared_ptr<PlayerState> GameRoom::GetPlayer(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(roomMutex_);
    auto it = players_.find(sessionId);
    if (it != players_.end()) return it->second;
    return nullptr;
}