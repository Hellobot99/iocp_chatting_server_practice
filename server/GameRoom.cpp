#include "GameRoom.h"

GameRoom::GameRoom(int id, const std::string& name) {

}

// 고정 틱마다 호출되어 시뮬레이션을 업데이트합니다.
void GameRoom::Update(float fixedDeltaTime) {

}

// 플레이어 추가/제거
void GameRoom::AddPlayer(std::shared_ptr<PlayerState> player, std::shared_ptr<ClientSession> session) {

}

void GameRoom::RemovePlayer(uint32_t sessionId) {

}

// 해당 방의 모든 클라이언트에게 상태 스냅샷을 브로드캐스트합니다. [17, 18]
void GameRoom::BroadcastStateSnapshot(uint32_t serverTick) {

}

// 채팅 브로드캐스트 (동일 방 내)
void GameRoom::BroadcastChat(uint32_t senderId, const std::string& message) {

}

std::shared_ptr<PlayerState> GameRoom::GetPlayer(uint32_t sessionId) {

}