#include "RoomManager.h"
#include "ClientSession.h" // 세션 헤더 필요

RoomManager::RoomManager() {
    // 기본 로비(0번 방) 생성
    CreateRoom(0, "Lobby");
}

std::shared_ptr<GameRoom> RoomManager::CreateRoom(int id, const std::string& name) {
    auto newRoom = std::make_shared<GameRoom>(id, name);

    std::lock_guard<std::mutex> lock(roomMutex_);
    rooms_[id] = newRoom;
    return newRoom;
}

// GameLogic::Run()에서 매 틱마다 호출됨
void RoomManager::UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick) {

    // 1. 룸 업데이트 (물리 등)
    // 2. 스냅샷 브로드캐스트 (매 틱마다 할지, 2~3틱마다 할지는 선택)

    std::lock_guard<std::mutex> lock(roomMutex_);
    for (auto& pair : rooms_) {
        auto& room = pair.second;

        room->Update(fixedDeltaTime);

        // 여기서는 매 틱마다 보낸다고 가정
        room->BroadcastStateSnapshot(serverTick);
    }
}

bool RoomManager::JoinRoom(uint32_t sessionId, std::shared_ptr<ClientSession> session, int targetRoomId) {
    std::lock_guard<std::mutex> lock(roomMutex_);

    // 1. 타겟 방 찾기
    auto it = rooms_.find(targetRoomId);
    if (it == rooms_.end()) return false; // 방 없음

    auto targetRoom = it->second;

    // 2. 기존 방에서 나가기 (이미 방에 있다면)
    if (playerToRoomMap_.count(sessionId)) {
        int currentRoomId = playerToRoomMap_[sessionId];
        if (rooms_.count(currentRoomId)) {
            rooms_[currentRoomId]->RemovePlayer(sessionId);
        }
    }

    // 3. 새 플레이어 상태 생성
    auto newPlayerState = std::make_shared<PlayerState>(sessionId, "Player", targetRoomId);

    // 4. 방에 입장
    targetRoom->AddPlayer(newPlayerState, session);

    // 5. 매핑 정보 갱신
    playerToRoomMap_[sessionId] = targetRoomId;

    return true;
}

void RoomManager::RemovePlayerFromCurrentRoom(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(roomMutex_);

    if (playerToRoomMap_.count(sessionId)) {
        int roomId = playerToRoomMap_[sessionId];
        if (rooms_.count(roomId)) {
            rooms_[roomId]->RemovePlayer(sessionId);
        }
        playerToRoomMap_.erase(sessionId);
    }
}

std::shared_ptr<GameRoom> RoomManager::GetRoom(int roomId) {
    std::lock_guard<std::mutex> lock(roomMutex_);
    auto it = rooms_.find(roomId);
    if (it != rooms_.end()) return it->second;
    return nullptr;
}

// ★ Command에서 쓰기 위해 추가로 필요할 함수 (헤더에도 추가 필요)
std::shared_ptr<GameRoom> RoomManager::GetRoomOfPlayer(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(roomMutex_);
    if (playerToRoomMap_.count(sessionId)) {
        int roomId = playerToRoomMap_[sessionId];
        return rooms_[roomId]; // map[] 연산자는 주의 필요하지만 여기선 존재 확인했으므로 안전
    }
    return nullptr;
}