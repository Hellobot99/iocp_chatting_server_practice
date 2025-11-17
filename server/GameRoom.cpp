#include "Protocol/Protocol.pb.h"
#include <iostream>
#include "GameRoom.h"
#include "ClientSession.h"
#include "RoomManager.h"



GameRoom::GameRoom(int id, const std::string& name)
    : id_(id), name_(name)
{
}

// 매 틱(16ms 등)마다 호출되어 물리 연산을 수행
void GameRoom::Update(float fixedDeltaTime)
{
    // 방에 있는 모든 플레이어의 위치 업데이트
    for (auto& pair : players_) {
        auto& player = pair.second;
        player->ApplyMovement(fixedDeltaTime);
    }
}

void GameRoom::AddPlayer(std::shared_ptr<PlayerState> player, std::shared_ptr<ClientSession> session)
{
    std::lock_guard<std::mutex> lock(roomMutex_); // 멀티스레드 안전장치 (필요시 헤더에 mutex 추가)

    players_[player->sessionId] = player;
    sessions_[player->sessionId] = session;

    // 입장 로그
    std::cout << "Session " << player->sessionId << " joined Room " << id_ << std::endl;
}

void GameRoom::RemovePlayer(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    players_.erase(sessionId);
    sessions_.erase(sessionId);
}

// 서버의 '상태(State)'를 클라이언트들에게 전송
void GameRoom::BroadcastStateSnapshot(uint32_t serverTick)
{
    // 1. Protobuf 패킷 생성
    Protocol::S_StateSnapshot pkt;
    pkt.set_server_tick(serverTick);

    {
        std::lock_guard<std::mutex> lock(roomMutex_);

        // 2. 방 안의 모든 플레이어 정보를 패킷에 담음
        for (auto& pair : players_) {
            auto& pState = pair.second;

            auto* pSnapshot = pkt.add_players(); // repeated 필드에 추가
            pSnapshot->set_session_id(pState->sessionId);

            auto* pos = pSnapshot->mutable_authoritative_position();
            pos->set_x(pState->position.x);
            pos->set_y(pState->position.y);
        }
    }

    // 3. 직렬화 (Serialization)
    // (주의: 실제 전송을 위해선 길이 헤더를 붙이는 등의 패킷 포장 과정이 필요할 수 있음.
    //  여기서는 Protobuf SerializeAsString만 예시로 듭니다.)
    std::string sendBuffer;
    if (!pkt.SerializeToString(&sendBuffer)) {
        return; // 직렬화 실패
    }

    // 4. 모든 세션에게 전송
    {
        std::lock_guard<std::mutex> lock(roomMutex_);
        for (auto& pair : sessions_) {
            auto session = pair.second;
            // ClientSession::Send는 구현되어 있다고 가정 (패킷 ID + 데이터 길이 + 데이터)
            session->Send(Protocol::S_STATE_SNAPSHOT, sendBuffer);
        }
    }
}

void GameRoom::BroadcastChat(uint32_t senderId, const std::string& message)
{
    Protocol::S_Chat chatPkt;
    chatPkt.set_username(std::to_string(senderId)); // 닉네임 시스템이 없다면 ID로 대체
    chatPkt.set_message(message);

    std::string sendBuffer;
    chatPkt.SerializeToString(&sendBuffer);

    std::lock_guard<std::mutex> lock(roomMutex_);
    for (auto& pair : sessions_) {
        pair.second->Send(Protocol::S_CHAT, sendBuffer);
    }
}

std::shared_ptr<PlayerState> GameRoom::GetPlayer(uint32_t sessionId)
{
    std::lock_guard<std::mutex> lock(roomMutex_);
    auto it = players_.find(sessionId);
    if (it != players_.end()) {
        return it->second;
    }
    return nullptr;
}