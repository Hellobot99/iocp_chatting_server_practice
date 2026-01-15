#include "Command.h"
#include "RoomManager.h"
#include "GameRoom.h"
#include "PlayerState.h"
#include "Persistence.h"
#include "PersistenceRequest.h"
#include "Server.h"        
#include "ClientSession.h"

extern Server* g_Server;

// [1] 회원가입 커맨드 (DB 작업 요청)
void RegisterCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto req = std::make_unique<PersistenceRequest>();
    req->type = RequestType::REGISTER;
    req->sessionId = sessionId_;
    req->username = username_;
    req->password = password_;

    persistence.PostRequest(std::move(req));
}

// [2] 로그인 커맨드 (DB 작업 요청)
void LoginCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (!session) return;

    int dbId = persistence.AuthenticateUser(username_, password_);

    if (dbId != -1)
    {
        if (g_Server->IsUserConnected(username_))
        {
            std::cout << "[Login] Denied duplicate login: " << username_ << std::endl;

            persistence.RemoveActiveUser(username_);

            PacketLoginRes res;
            res.success = false;
            res.playerId = -1;

            session->Send(PacketId::LOGIN_RES, &res, sizeof(res));
            return;
        }

        session->SetName(username_);

        PacketLoginRes res;
        res.success = true;
        res.playerId = dbId;

        session->Send(PacketId::LOGIN_RES, &res, sizeof(res));

        std::cout << "[Login] Success: " << username_ << " (DB_ID: " << dbId << ")" << std::endl;
    }
    else
    {
        std::cout << "[Login] Failed (Invalid ID or PW): " << username_ << std::endl;

        PacketLoginRes res;
        res.success = false;
        res.playerId = -1;

        session->Send(PacketId::LOGIN_RES, &res, sizeof(res));
    }
}

// [3] 방 입장 커맨드 (게임 로직)
void EnterRoomCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    if (session->GetName().empty())
    {
        std::cout << "[Warning] Unauthenticated user tried to join room." << std::endl;
        return;
    }

    std::cout << "[Logic] Trying to Join Room... (Session: " << sessionId_
        << ", TargetRoom: " << roomId_ << ")" << std::endl;

    bool success = roomManager.JoinRoom(session, roomId_);

    if (success)
    {
        std::cout << "[Logic] User " << session->GetName() << " joined Room " << roomId_ << std::endl;

        std::vector<std::string> history = persistence.GetRecentChats(roomId_);
        std::cout << history.size() << std::endl;

        for (const auto& chatLine : history) {
            PacketChat packet;
            packet.playerId = 0;
            std::memset(packet.msg, 0, sizeof(packet.msg));

            size_t copySize = (std::min)(chatLine.size(), sizeof(packet.msg) - 1);
            std::memcpy(packet.msg, chatLine.c_str(), copySize);

            session->Send(PacketId::CHAT, &packet, sizeof(packet));
        }
    }
    else
    {
        std::cout << "[Logic] Failed to join room " << roomId_ << std::endl;
    }
}

// [4] 방 퇴장 커맨드
void LeaveRoomCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    roomManager.RemovePlayerFromCurrentRoom(sessionId_);
    std::cout << "[Logic] Session " << sessionId_ << " left the room." << std::endl;
}

// [5] 이동 커맨드 (기존 유지)
void MoveCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    auto room = session->GetCurrentRoom();
    if (room == nullptr) return;

    auto player = room->GetPlayer(sessionId_);
    if (player)
    {
        player->velocity.x = vx_;
        player->velocity.y = vy_;
    }
}

// [6] 채팅 커맨드 (기존 유지 + DB 저장)
void ChatCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    auto room = session->GetCurrentRoom();
    if (room == nullptr) return;

    std::string senderName = session->GetName();

    room->BroadcastChat(senderName, message_);

    persistence.SaveAndCacheChat(room->GetId(), sessionId_, senderName, message_);
}

void CreateRoomCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (!session) return;

    auto newRoom = roomManager.CreateRoom(title_);
    PacketCreateRoomRes res;
    res.success = (newRoom != nullptr);
    res.roomId = (newRoom != nullptr) ? newRoom->GetId() : -1;

    session->Send(PacketId::CREATE_ROOM_RES, &res, sizeof(res));
}

void RoomListCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (session)
    {
        roomManager.SendRoomList(session);
    }
}

void LogoutCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{

    std::cout << "[DEBUG] Try to remove user from Redis: [" << username_ << "]" << std::endl;
    persistence.RemoveActiveUser(username_);

    roomManager.RemovePlayerFromCurrentRoom(sessionId_);

    std::cout << "[Logout] User: " << username_ << " logged out." << std::endl;

    auto session = g_Server->GetSession(sessionId_);
    if (session)
    {
        session->Disconnect();
    }
}