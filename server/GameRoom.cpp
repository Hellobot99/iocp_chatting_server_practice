#include "GameRoom.h"
#include "ClientSession.h"
#include "RoomManager.h"
#include "NetProtocol.h"
#include <iostream>
#include <cstring> // memcpy

GameRoom::GameRoom(int id, const std::string& name)
    : id_(id), name_(name)
{
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
    players_.erase(sessionId);
    sessions_.erase(sessionId);
}

void GameRoom::BroadcastStateSnapshot(uint32_t serverTick)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    if (sessions_.empty()) return;

    // 1. 패킷 데이터 크기 계산
    // 데이터 구조: [Count(4byte)] + N * [ID(4) + X(4) + Y(4)]
    // 간단하게 ID, X, Y만 쭉 나열해서 보냅시다.

    // (보낼 데이터 구조체 정의 - 임시)
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

// [핵심 수정 2] 채팅 전송: Protobuf 대신 문자열 포맷팅
void GameRoom::BroadcastChat(uint32_t senderId, const std::string& message)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    // 포맷: "[SenderID] Message" 형태로 단순 문자열 전송
    // 혹은 바이너리로 [ID(4)][MsgLen(4)][MsgBody...] 로 짜도 됨

    // 여기서는 간단하게 문자열 합치기로 구현
    std::string fullMsg = "[" + std::to_string(senderId) + "] " + message;

    for (auto& pair : sessions_) {
        pair.second->Send(PacketId::CHAT, fullMsg);
    }
}

std::shared_ptr<PlayerState> GameRoom::GetPlayer(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(roomMutex_);
    auto it = players_.find(sessionId);
    if (it != players_.end()) return it->second;
    return nullptr;
}