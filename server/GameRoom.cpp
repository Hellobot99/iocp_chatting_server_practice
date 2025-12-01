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

    // erase는 "지운 개수"를 반환합니다. (1이면 성공, 0이면 실패/없음)
    size_t removedCount = players_.erase(sessionId);
    sessions_.erase(sessionId);

    if (removedCount == 0)
    {
        // 여기에 걸린다면, 로직 어딘가에서 이미 지워졌거나 
        // 애초에 이 방에 들어온 적이 없는 것입니다.
        std::cout << "[Error] Player " << sessionId << " was NOT found in this room!" << std::endl;
    }
    else
    {
        std::cout << "[Success] Player " << sessionId << " removed. Remaining: " << players_.size() << std::endl;
    }
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


void GameRoom::BroadcastChat(const std::string& senderName, const std::string& message)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    // 방법 1: "이름: 메시지" 형태의 단순 문자열로 합쳐서 보냄 (제일 간단)
    // 클라이언트는 이걸 그대로 채팅창에 띄우면 됨.
    std::string finalMsg = senderName + ": " + message;

    // 패킷 본문 크기 체크 (PacketChat의 msg 배열 크기가 256이라 가정)
    if (finalMsg.size() >= 256) {
        finalMsg = finalMsg.substr(0, 255); // 자르기
    }

    // PacketChat 구조체에 맞춰서 데이터 직렬화
    PacketChat packetData;
    std::memset(&packetData, 0, sizeof(PacketChat));
    std::memcpy(packetData.msg, finalMsg.c_str(), finalMsg.size());

    // string으로 변환해서 Send 호출 (Send 함수가 string을 받으므로)
    std::string serializedData(reinterpret_cast<char*>(&packetData), sizeof(PacketChat));

    for (auto& pair : sessions_) {
        // 모든 세션에게 전송
        pair.second->Send(PacketId::CHAT, serializedData);
    }
}

std::shared_ptr<PlayerState> GameRoom::GetPlayer(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(roomMutex_);
    auto it = players_.find(sessionId);
    if (it != players_.end()) return it->second;
    return nullptr;
}