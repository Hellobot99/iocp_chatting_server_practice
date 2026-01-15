#include <iostream>
#include "ClientSession.h"
#include "NetProtocol.h"
#include "Server.h"
#include "Command.h"

extern Server* g_Server;

ClientSession::ClientSession(SOCKET sock, uint32_t sessionId)
    : socket_(sock), sessionId_(sessionId)
{
    writePos_ = 0;
    readPos_ = 0;
}

ClientSession::~ClientSession()
{
    closesocket(socket_);
}

void ClientSession::Disconnect()
{
    if (socket_ == INVALID_SOCKET) return;

    std::string name = GetName();
    if (!name.empty()) {
        auto cmd = std::make_unique<LogoutCommand>(sessionId_, name);
        if (g_Server) Server::GetGLTInputQueue().Push(std::move(cmd));
    }
    
    closesocket(socket_);
    socket_ = INVALID_SOCKET;

    std::cout << "[Session] Disconnected Client: " << sessionId_ << std::endl;
}

void ClientSession::Send(PacketId id, const std::string& serializedData)
{
    const uint16_t dataSize = static_cast<uint16_t>(serializedData.size());
    const uint16_t packetSize = sizeof(GameHeader) + dataSize;

    std::vector<char> sendBuffer(packetSize);

    GameHeader* header = reinterpret_cast<GameHeader*>(sendBuffer.data());
    header->packetSize = packetSize;
    header->packetId = static_cast<uint16_t>(id);

    std::memcpy(sendBuffer.data() + sizeof(GameHeader), serializedData.data(), dataSize);

    PushSendPacket(sendBuffer);
}

void ClientSession::Send(PacketId id, void* ptr, int size)
{
    const uint16_t packetSize = sizeof(GameHeader) + static_cast<uint16_t>(size);
    std::vector<char> sendBuffer(packetSize);

    GameHeader* header = reinterpret_cast<GameHeader*>(sendBuffer.data());
    header->packetSize = packetSize;
    header->packetId = static_cast<uint16_t>(id);

    // string 변환 없이 바로 복사
    std::memcpy(sendBuffer.data() + sizeof(GameHeader), ptr, size);

    PushSendPacket(sendBuffer);
}

void ClientSession::PostRecv(HANDLE hIOCP)
{
    DWORD flags = 0;
    DWORD recvBytes = 0;

    ZeroMemory(&recvIoData_.overlapped, sizeof(OVERLAPPED));

    int freeSize = BUFFER_SIZE - writePos_;

    if (freeSize <= 0)
    {
        // 더 이상 받을 공간이 없음 -> 에러 처리 혹은 강제 연결 종료
        std::cout << "[Error] Recv Buffer Overflow!" << std::endl;
        Disconnect();
        return;
    }

    // WSABUF 설정 (writePos_ 위치부터 남은 공간만큼 받겠다)
    recvIoData_.wsaBuf.buf = &inputBuffer_[writePos_];
    recvIoData_.wsaBuf.len = (ULONG)freeSize;
    recvIoData_.operation = 0;

    int ret = WSARecv(socket_, &recvIoData_.wsaBuf, 1, &recvBytes, &flags, &recvIoData_.overlapped, NULL);

    if (ret == SOCKET_ERROR)
    {
        int err = WSAGetLastError();
        if (err != WSA_IO_PENDING)
        {
            std::cout << "WSARecv Failed: " << err << std::endl;
        }
    }
}

void ClientSession::PushSendPacket(const std::vector<char>& packetData)
{
    auto packet = std::make_shared<std::vector<char>>(packetData);
    outputQueue_.Push(packet);

    bool expected = false;
    if (isSending_.compare_exchange_strong(expected, true))
    {
        FlushSend();
    }
}

void ClientSession::FlushSend()
{
    std::shared_ptr<std::vector<char>> packet;

    if (!outputQueue_.Pop(packet))
    {
        isSending_ = false;
        return;
    }

    currentSendingPacket_ = packet;

    ZeroMemory(&sendIoData_.overlapped, sizeof(OVERLAPPED));
    sendIoData_.operation = 1;

    sendIoData_.wsaBuf.buf = currentSendingPacket_->data();
    sendIoData_.wsaBuf.len = (ULONG)currentSendingPacket_->size();

    DWORD sendBytes = 0;
    DWORD flags = 0;

    if (WSASend(socket_, &sendIoData_.wsaBuf, 1, &sendBytes, flags, &sendIoData_.overlapped, NULL) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            isSending_ = false;
        }
    }
}

bool ClientSession::HasCompletePacket() const
{
    int dataSize = writePos_ - readPos_;

    // 1) 헤더 크기만큼도 안 왔으면 패킷 아님
    if (dataSize < sizeof(GameHeader)) {
        return false;
    }

    // 2) 헤더를 읽어서 전체 패킷 크기 확인
    const GameHeader* header = reinterpret_cast<const GameHeader*>(&inputBuffer_[readPos_]);

    // 3) 버퍼에 있는 유효 데이터 양이 패킷 전체 크기보다 많거나 같으면
    return dataSize >= header->packetSize;
}

std::unique_ptr<ICommand> ClientSession::DeserializeCommand()
{
    std::lock_guard<std::mutex> lock(lock_);

    GameHeader* header = reinterpret_cast<GameHeader*>(&inputBuffer_[readPos_]);
    const char* bodyPtr = reinterpret_cast<const char*>(&inputBuffer_[readPos_]) + sizeof(GameHeader);

    int bodySize = header->packetSize - sizeof(GameHeader);
    std::unique_ptr<ICommand> command = nullptr;
    PacketId pktId = static_cast<PacketId>(header->packetId);

    switch (pktId)
    {
        // 로그인 요청 처리
    case PacketId::LOGIN_REQ:
    {
        // 1. 사이즈 체크
        if (bodySize < sizeof(PacketLoginReq)) return nullptr;

        // 2. 캐스팅
        const PacketLoginReq* pkt = reinterpret_cast<const PacketLoginReq*>(bodyPtr);

        // 3. 데이터 추출
        std::string username(pkt->username);
        std::string password(pkt->password);

        std::cout << "[RECV] LOGIN_REQ / ID: " << username << std::endl;

        // 4. 커맨드 생성
        command = std::make_unique<LoginCommand>(sessionId_, username, password);
        break;
    }

    // 회원가입 요청 처리
    case PacketId::REGISTER_REQ:
    {
        if (bodySize < sizeof(PacketRegisterReq)) return nullptr;

        const PacketRegisterReq* pkt = reinterpret_cast<const PacketRegisterReq*>(bodyPtr);

        std::string username(pkt->username);
        std::string password(pkt->password);

        std::cout << "[RECV] REGISTER_REQ / ID: " << username << std::endl;

        // RegisterCommand 생성
        command = std::make_unique<RegisterCommand>(sessionId_, username, password);
        break;
    }

    // 방 입장 요청
    case PacketId::ENTER_ROOM:
    {
        if (bodySize < sizeof(PacketEnterRoom)) return nullptr;

        const PacketEnterRoom* pkt = reinterpret_cast<const PacketEnterRoom*>(bodyPtr);
        int32_t roomId = pkt->roomId;

        std::cout << "[RECV] ENTER_ROOM / Room: " << roomId << std::endl;

        command = std::make_unique<EnterRoomCommand>(sessionId_, roomId);
        break;
    }

    case PacketId::CHAT:
    {
        if (bodySize < sizeof(PacketChat))
        {
            std::cout << "[Error] Invalid Chat Packet Size" << std::endl;
            return nullptr;
        }
        const PacketChat* pkt = reinterpret_cast<const PacketChat*>(bodyPtr);

        std::string msg(pkt->msg);
        std::cout << "[Debug] Chat Msg: " << msg << std::endl;
        command = std::make_unique<ChatCommand>(sessionId_, msg);
    }
    break;

    case PacketId::MOVE:
    {
        if (bodySize < sizeof(PacketMove))
        {
            std::cout << "[Error] Invalid Move Packet Size" << std::endl;
            return nullptr;
        }
        const PacketMove* pkt = reinterpret_cast<const PacketMove*>(bodyPtr);
        command = std::make_unique<MoveCommand>(sessionId_, pkt->vx, pkt->vy);
    }
    break;

    case PacketId::CREATE_ROOM_REQ:
    {
        if (bodySize < sizeof(PacketCreateRoomReq)) return nullptr;
        const PacketCreateRoomReq* pkt = reinterpret_cast<const PacketCreateRoomReq*>(bodyPtr);

        std::string title(pkt->title);

        std::cout << "[RECV] CREATE_ROOM / Title: " << title << std::endl;
        command = std::make_unique<CreateRoomCommand>(sessionId_, title);
        break;
    }

    case PacketId::ROOM_LIST_REQ:
    {
        std::cout << "[RECV] ROOM_LIST_REQ" << std::endl;
        command = std::make_unique<RoomListCommand>(sessionId_);
        break;
    }

    case PacketId::LOGOUT_REQ:
    {
        std::cout << "[RECV] LOGOUT_REQ" << std::endl;
        std::string myName = name_;
        command = std::make_unique<LogoutCommand>(sessionId_, myName);
        break;
    }

    default:
        std::cout << "[RECV] Unknown Packet ID: " << header->packetId << std::endl;
        return nullptr;
    }

    return command;
}

void ClientSession::OnRecv(DWORD bytesTransferred)
{
    MoveWritePos(bytesTransferred);

    while (true)
    {
        int dataSize = writePos_ - readPos_;
        if (dataSize < sizeof(GameHeader)) break;

        GameHeader* header = reinterpret_cast<GameHeader*>(&inputBuffer_[readPos_]);
        if (dataSize < header->packetSize) break;

        std::unique_ptr<ICommand> command = DeserializeCommand();

        if (command != nullptr) {
            readPos_ += header->packetSize;
            Server::GetGLTInputQueue().Push(std::move(command));
        }
        else {
            PacketId pktId = static_cast<PacketId>(header->packetId);
            if (pktId == PacketId::LOGIN_REQ) {
                readPos_ += header->packetSize;
            }
            else {
                std::cout << "[Session] Error or Unknown Packet. Disconnecting..." << std::endl;
                Disconnect();
                return;
            }
        }
    }

    if (readPos_ == writePos_)
    {
        readPos_ = 0;
        writePos_ = 0;
    }

    RegisterRecv();
}

void ClientSession::MoveWritePos(DWORD bytes)
{
    writePos_ += bytes;
}

void ClientSession::RegisterRecv()
{
    if (BUFFER_SIZE - writePos_ < 1024)
    {
        int dataSize = writePos_ - readPos_;

        if (dataSize > 0)
        {
            std::memmove(&inputBuffer_[0], &inputBuffer_[readPos_], dataSize);
        }

        readPos_ = 0;
        writePos_ = dataSize;
    }

    PostRecv(nullptr);
}

void ClientSession::OnSendCompleted(DWORD bytesTransferred)
{
    currentSendingPacket_ = nullptr;
    FlushSend();
}