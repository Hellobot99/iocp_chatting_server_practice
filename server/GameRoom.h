#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include "PlayerState.h"
#include "LockFreeQueue.h"

class ClientSession; // 전방 선언

// 게임 내 특정 공간(방)의 상태를 격리하는 컨테이너
// [16]
class GameRoom
{
public:
    GameRoom(int id, const std::string& name);

    int GetId() const { return id_; }

    // 고정 틱마다 호출되어 시뮬레이션을 업데이트합니다.
    void Update(float fixedDeltaTime);

    // 플레이어 추가/제거
    void AddPlayer(std::shared_ptr<PlayerState> player, std::shared_ptr<ClientSession> session);
    void RemovePlayer(uint32_t sessionId);

    // 해당 방의 모든 클라이언트에게 상태 스냅샷을 브로드캐스트합니다. [17, 18]
    void BroadcastStateSnapshot(uint32_t serverTick);

    // 채팅 브로드캐스트 (동일 방 내)
    void BroadcastChat(uint32_t senderId, const std::string& message);

    std::shared_ptr<PlayerState> GetPlayer(uint32_t sessionId);

private:
    int id_;
    std::string name_;
    std::mutex roomMutex_;

    // 방에 속한 플레이어의 권한 있는 상태
    std::map<uint32_t, std::shared_ptr<PlayerState>> players_;

    // 방에 속한 플레이어 세션 (네트워크 전송을 위해 필요)
    std::map<uint32_t, std::shared_ptr<ClientSession>> sessions_;
};