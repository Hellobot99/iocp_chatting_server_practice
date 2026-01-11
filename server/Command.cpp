#include "Command.h"
#include "RoomManager.h"
#include "GameRoom.h"
#include "PlayerState.h"
#include "Persistence.h"
#include "PersistenceRequest.h"
#include "Server.h"        
#include "ClientSession.h"

extern Server* g_Server;

// ---------------------------------------------------------
// [1] 회원가입 커맨드 (DB 작업 요청)
// ---------------------------------------------------------
void RegisterCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    // DB 스레드에 작업 넘기기 (비동기)
    auto req = std::make_unique<PersistenceRequest>();
    req->type = RequestType::REGISTER;
    req->sessionId = sessionId_;
    req->username = username_;
    req->password = password_;

    // DB 큐에 넣음 (GameLogic 스레드는 멈추지 않음)
    persistence.PostRequest(std::move(req));
}

// ---------------------------------------------------------
// [2] 로그인 커맨드 (DB 작업 요청)
// ---------------------------------------------------------
void LoginCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (!session) return;

    // ---------------------------------------------------------
    // 1. [실제 로직] DB 인증 시도 (동기식 호출)
    // ---------------------------------------------------------
    // 성공 시: 유저의 DB PK(id) 반환
    // 실패 시: -1 반환
    int dbId = persistence.AuthenticateUser(username_, password_);

    if (dbId != -1) // DB에 유저가 있고 비밀번호도 맞음
    {
        // -----------------------------------------------------
        // 2. [중복 체크] 이미 접속 중인 아이디인지 확인
        // -----------------------------------------------------
        if (g_Server->IsUserConnected(username_))
        {
            std::cout << "[Login] Denied duplicate login: " << username_ << std::endl;

            persistence.RemoveActiveUser(username_);

            PacketLoginRes res;
            res.success = false;
            res.playerId = -1; // 실패 의미

            // (선택) 클라이언트가 "중복 로그인"임을 알 수 있게 별도 에러 코드를 주면 더 좋습니다.

            session->Send(PacketId::LOGIN_RES, &res, sizeof(res));
            return; // ★ 여기서 중단
        }

        // -----------------------------------------------------
        // 3. [로그인 성공] DB 인증 O, 중복 X
        // -----------------------------------------------------

        // [중요] 세션에 유저 정보 저장
        session->SetName(username_);  // 아이디 저장
        // session->SetDbId(dbId);    // (만약 ClientSession에 변수가 있다면 저장 권장)

        PacketLoginRes res;
        res.success = true;
        res.playerId = dbId; // ★ 클라이언트에게 DB 고유 ID(PlayerID)를 전달

        session->Send(PacketId::LOGIN_RES, &res, sizeof(res));

        std::cout << "[Login] Success: " << username_ << " (DB_ID: " << dbId << ")" << std::endl;
    }
    else
    {
        // -----------------------------------------------------
        // 4. [로그인 실패] 아이디가 없거나 비번이 틀림
        // -----------------------------------------------------
        std::cout << "[Login] Failed (Invalid ID or PW): " << username_ << std::endl;

        PacketLoginRes res;
        res.success = false;
        res.playerId = -1;

        session->Send(PacketId::LOGIN_RES, &res, sizeof(res));
    }
}
// ---------------------------------------------------------
// [3] 방 입장 커맨드 (게임 로직)
// ---------------------------------------------------------
void EnterRoomCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    auto session = g_Server->GetSession(sessionId_);
    if (session == nullptr) return;

    // 로그인이 안 된 유저가 방 입장을 시도하면 차단
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
        // (선택) 입장 로그를 DB에 남기고 싶다면 여기서 Persistence 요청 가능
    }
    else
    {
        std::cout << "[Logic] Failed to join room " << roomId_ << std::endl;
    }
}

// ---------------------------------------------------------
// [4] 방 퇴장 커맨드
// ---------------------------------------------------------
void LeaveRoomCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    roomManager.RemovePlayerFromCurrentRoom(sessionId_);
    std::cout << "[Logic] Session " << sessionId_ << " left the room." << std::endl;
}

// ---------------------------------------------------------
// [5] 이동 커맨드 (기존 유지)
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// [6] 채팅 커맨드 (기존 유지 + DB 저장)
// ---------------------------------------------------------
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

    // 1. 방 생성 요청
    auto newRoom = roomManager.CreateRoom(title_);

    // 2. 결과 패킷 전송
    PacketCreateRoomRes res;
    res.success = (newRoom != nullptr);
    res.roomId = (newRoom != nullptr) ? newRoom->GetId() : -1;

    session->Send(PacketId::CREATE_ROOM_RES, &res, sizeof(res));

    // (선택) 여기서 바로 입장시켜도 되지만, 
    // 보통은 클라가 응답 받고 EnterRoom을 다시 보내는 게 흐름상 깔끔함.
}

void RoomListCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{
    // 세션을 찾아서 방 목록 패킷 전송
    auto session = g_Server->GetSession(sessionId_);
    if (session)
    {
        roomManager.SendRoomList(session);
    }
}

void LogoutCommand::Execute(RoomManager& roomManager, Persistence& persistence)
{

    std::cout << "[DEBUG] Try to remove user from Redis: [" << username_ << "]" << std::endl;
    // 1. Redis에서 세션 제거 (필수)
    persistence.RemoveActiveUser(username_);

    // 2. 현재 방에서 나가기 처리 (이미 구현된 로직 활용)
    roomManager.RemovePlayerFromCurrentRoom(sessionId_);

    std::cout << "[Logout] User: " << username_ << " logged out." << std::endl;

    auto session = g_Server->GetSession(sessionId_);
    if (session)
    {
        session->Disconnect();
    }
}