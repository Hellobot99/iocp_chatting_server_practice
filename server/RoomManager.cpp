#include "RoomManager.h"

RoomManager::RoomManager() {

}

// 새 룸 생성
std::shared_ptr<GameRoom> RoomManager::CreateRoom(int id, const std::string& name) {

}

// GLT 틱에서 모든 룸을 업데이트
void RoomManager::UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick) {

}

// 플레이어를 지정된 룸으로 이동 (상태 변경)
bool RoomManager::JoinRoom(uint32_t sessionId, std::shared_ptr<ClientSession> session, int targetRoomId) {

}

// 플레이어 퇴장/연결 해제 처리
void RoomManager::RemovePlayerFromCurrentRoom(uint32_t sessionId) {

}

std::shared_ptr<GameRoom> RoomManager::GetRoom(int roomId) {

}