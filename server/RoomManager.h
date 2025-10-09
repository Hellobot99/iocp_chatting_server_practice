#pragma once

#include <map>
#include <memory>
#include <mutex>
#include "GameRoom.h"
#include "PlayerState.h"

// 모든 게임 룸을 관리하고 플레이어의 룸 이동을 처리합니다.
class RoomManager
{
public:
    RoomManager();

    // 새 룸 생성
    std::shared_ptr<GameRoom> CreateRoom(int id, const std::string& name);

    // GLT 틱에서 모든 룸을 업데이트
    void UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick);

    // 플레이어를 지정된 룸으로 이동 (상태 변경)
    bool JoinRoom(uint32_t sessionId, std::shared_ptr<ClientSession> session, int targetRoomId);

    // 플레이어 퇴장/연결 해제 처리
    void RemovePlayerFromCurrentRoom(uint32_t sessionId);

    std::shared_ptr<GameRoom> GetRoom(int roomId);

private:
    std::map<int, std::shared_ptr<GameRoom>> rooms_;

    // GLT는 단일 스레드이므로 룸 목록에 대한 동시 접근 문제가 적지만,
    // 필요에 따라 뮤텍스를 사용하여 룸 생성/삭제를 보호할 수 있습니다.
    std::mutex roomMutex_;

    // 플레이어가 현재 어떤 룸에 있는지 빠르게 조회하기 위한 맵
    std::map<uint32_t, int> playerToRoomMap_;
};