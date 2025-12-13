#include "RoomManager.h"
#include "ClientSession.h"
#include "PlayerState.h" // PlayerState 생성 위해 필요

RoomManager::RoomManager() {
}

std::shared_ptr<GameRoom> RoomManager::CreateRoom(const std::string& title)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    // 1. 새 방 생성 (ID 자동 증가)
    int roomId = nextRoomId_++;

    // GameRoom 생성자는 (roomId, title)을 받는다고 가정
    auto newRoom = std::make_shared<GameRoom>(roomId, title);

    // 2. 관리 목록에 추가
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

// [핵심 수정] cpp 구현부 인자 맞춤, 로직 통합
bool RoomManager::JoinRoom(std::shared_ptr<ClientSession> session, int targetRoomId)
{
    // [DEBUG] 1. 함수 진입 및 세션 체크
    std::cout << "[Debug] JoinRoom Start. RoomID: " << targetRoomId << std::endl;

    if (session == nullptr)
    {
        std::cout << "[Error] Session is NULL!" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(roomMutex_);

    // [DEBUG] 2. 세션 ID 추출
    uint32_t sessionId = session->GetSessionId();
    std::cout << "[Debug] Session ID extracted: " << sessionId << std::endl;

    // 1. 방 찾기
    std::shared_ptr<GameRoom> targetRoom = nullptr;
    auto it = rooms_.find(targetRoomId);

    if (it == rooms_.end())
    {
        // [DEBUG] 3-A. 방 없음 -> 생성 시도
        std::cout << "[Debug] Room not found. Creating new room..." << std::endl;
        std::string roomName = "Room_" + std::to_string(targetRoomId);
        targetRoom = std::make_shared<GameRoom>(targetRoomId, roomName);
        rooms_[targetRoomId] = targetRoom;
    }
    else
    {
        targetRoom = it->second;
    }

    // [DEBUG] 4. 방 포인터 체크
    if (targetRoom == nullptr)
    {
        return false;
    }

    // 2. 기존 방 나가기 처리
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

    // [DEBUG] 5. 플레이어 이름 가져오기 (여기서 터질 확률 있음)
    std::string playerName = "Unknown";
    try {
        playerName = session->GetName(); // ClientSession에 name_ 변수가 초기화 안됐으면 터짐
        std::cout << "[Debug] Player Name: " << playerName << std::endl;
    }
    catch (...) {
        std::cout << "[Error] Failed to get Player Name!" << std::endl;
    }

    auto newPlayerState = std::make_shared<PlayerState>(sessionId, playerName, targetRoomId);

    // [DEBUG] 6. 방에 플레이어 추가 (AddPlayer 내부 로직 문제일 수 있음)
    std::cout << "[Debug] Calling targetRoom->AddPlayer..." << std::endl;
    targetRoom->AddPlayer(newPlayerState, session);

    playerToRoomMap_[sessionId] = targetRoomId;

    // [DEBUG] 7. 세션에 방 설정 (SetCurrentRoom 내부 문제일 수 있음)
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
        // 맵에 키가 있으면 방도 무조건 있다고 가정 (일관성 유지 필요)
        if (rooms_.count(roomId)) {
            return rooms_[roomId];
        }
    }
    return nullptr;
}

void RoomManager::SendRoomList(std::shared_ptr<ClientSession> session)
{
    std::lock_guard<std::mutex> lock(roomMutex_);

    // 1. 보낼 데이터 전체 크기 계산
    uint16_t roomCount = static_cast<uint16_t>(rooms_.size());
    uint16_t bodySize = sizeof(PacketRoomListRes) + (sizeof(RoomInfo) * roomCount);
    uint16_t packetSize = sizeof(GameHeader) + bodySize;

    // 2. 버퍼 준비
    std::vector<char> sendBuffer(packetSize);
    char* ptr = sendBuffer.data();

    // 3. 헤더 작성
    GameHeader* header = reinterpret_cast<GameHeader*>(ptr);
    header->packetSize = packetSize;
    header->packetId = (uint16_t)PacketId::ROOM_LIST_RES;
    ptr += sizeof(GameHeader);

    // 4. 리스트 헤더(개수) 작성
    PacketRoomListRes* res = reinterpret_cast<PacketRoomListRes*>(ptr);
    res->count = roomCount;
    ptr += sizeof(PacketRoomListRes);

    // 5. 방 정보들 순서대로 복사
    for (auto& pair : rooms_)
    {
        std::shared_ptr<GameRoom> room = pair.second;

        RoomInfo info;
        info.roomId = pair.first;
        info.userCount = room->GetPlayerCount(); // TODO: room->GetUserCount() 구현 필요

        // 방 제목 복사 (안전하게)
        // room->GetTitle()이 없으면 임시로 넣음
        std::string title = "Game Room " + std::to_string(info.roomId);
        strncpy_s(info.title, title.c_str(), 32);

        memcpy(ptr, &info, sizeof(RoomInfo));
        ptr += sizeof(RoomInfo);
    }

    // 6. 전송
    session->PushSendPacket(sendBuffer);
}