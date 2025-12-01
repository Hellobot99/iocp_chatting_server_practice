#include "RoomManager.h"
#include "ClientSession.h"
#include "PlayerState.h" // PlayerState 생성 위해 필요

RoomManager::RoomManager() {
    CreateRoom(0, "Lobby");
}

std::shared_ptr<GameRoom> RoomManager::CreateRoom(int id, const std::string& name) {
    std::lock_guard<std::mutex> lock(roomMutex_); // 락

    // 이미 있으면 리턴
    auto it = rooms_.find(id);
    if (it != rooms_.end()) return it->second;

    auto newRoom = std::make_shared<GameRoom>(id, name);
    rooms_[id] = newRoom;
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

// [핵심 수정] cpp 구현부 인자 맞춤, 로직 통합
bool RoomManager::JoinRoom(std::shared_ptr<ClientSession> session, int targetRoomId)
{
    // ★ Lock을 여기서 딱 한 번만 겁니다. 
    // 이 블록 안에서 GetRoom()이나 CreateRoom()을 호출하면 안 됩니다! (Deadlock 발생)
    std::lock_guard<std::mutex> lock(roomMutex_);

    uint32_t sessionId = session->GetSessionId(); // 세션에서 ID 추출

    // 1. 방 찾기 (직접 맵 검색)
    std::shared_ptr<GameRoom> targetRoom = nullptr;
    auto it = rooms_.find(targetRoomId);

    if (it == rooms_.end())
    {
        // [방 없음] 새로 생성 및 등록
        // std::cout << "[Logic] Creating new room " << targetRoomId << std::endl;
        std::string roomName = "Room_" + std::to_string(targetRoomId);
        targetRoom = std::make_shared<GameRoom>(targetRoomId, roomName);
        rooms_[targetRoomId] = targetRoom;
    }
    else
    {
        // [방 있음]
        targetRoom = it->second;
    }

    // 2. 기존 방에서 나가기 처리 (playerToRoomMap_ 활용)
    if (playerToRoomMap_.count(sessionId))
    {
        int oldRoomId = playerToRoomMap_[sessionId];
        if (oldRoomId == targetRoomId)
        {
            // 이미 그 방에 있으면 성공 처리하고 끝
            return true;
        }

        if (rooms_.count(oldRoomId))
        {
            rooms_[oldRoomId]->RemovePlayer(sessionId);
        }
        playerToRoomMap_.erase(sessionId);
    }

    // 3. 새 방 입장 처리
    std::string playerName = session->GetName();
    auto newPlayerState = std::make_shared<PlayerState>(sessionId, playerName, targetRoomId);

    // [Room]에 플레이어 추가
    targetRoom->AddPlayer(newPlayerState, session);

    // [Manager] 관리 맵에 추가
    playerToRoomMap_[sessionId] = targetRoomId;

    // ★★★ [Session] 세션에도 "현재 방" 정보 입력 (이게 빠져 있었음!) ★★★
    // 아까 ClientSession::SetCurrentRoom 구현했으므로 여기서 호출
    session->SetCurrentRoom(targetRoom);

    std::cout << "[RoomManager] Session " << sessionId << " joined Room " << targetRoomId
        << " (Count: " << targetRoom->GetPlayerCount() << ")" << std::endl;

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

std::shared_ptr<GameRoom> RoomManager::GetRoomOfPlayer(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(roomMutex_);
    if (playerToRoomMap_.count(sessionId)) {
        int roomId = playerToRoomMap_[sessionId];
        // 맵에 키가 있으면 방도 무조건 있다고 가정 (일관성 유지 필요)
        if (rooms_.count(roomId)) {
            return rooms_[roomId];
        }
    }
    return nullptr;
}