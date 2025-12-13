#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <iostream>
#include "GameRoom.h"
#include "NetProtocol.h"

class ClientSession; // [수정] 전방 선언 추가 (include 대신 사용해 의존성 줄임)

class RoomManager
{
public:
    RoomManager();
    static RoomManager* Instance() { static RoomManager instance; return &instance; } // 싱글톤으로 쓴다면 필요

    // [수정] 방 생성은 내부에서만 쓰거나, 외부에서 쓸 거면 락 주의가 필요함.
    std::shared_ptr<GameRoom> CreateRoom(const std::string& name);

    void UpdateAllRooms(float fixedDeltaTime, uint32_t serverTick);

    // [수정] sessionId는 session 안에 있으므로 굳이 인자로 안 받아도 됨 (session, roomId 만으로 충분)
    bool JoinRoom(std::shared_ptr<ClientSession> session, int targetRoomId);

    void RemovePlayerFromCurrentRoom(uint32_t sessionId);

    std::shared_ptr<GameRoom> GetRoom(int roomId);
    std::shared_ptr<GameRoom> GetRoomOfPlayer(uint32_t sessionId);

    void SendRoomList(std::shared_ptr<ClientSession> session);

private:
    std::map<int, std::shared_ptr<GameRoom>> rooms_;
    std::mutex roomMutex_;
    std::map<uint32_t, int> playerToRoomMap_;
    std::atomic<int> nextRoomId_ = 1;
};