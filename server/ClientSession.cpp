#include <iostream>
#include "ClientSession.h"
#include "NetProtocol.h"
#include "Server.h"
#include "Command.h"

extern Server* g_Server;

ClientSession::ClientSession(SOCKET sock, uint32_t sessionId)
    : socket_(sock), sessionId_(sessionId) // [수정] currentRoomId_ 초기화 제거
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

    // 보내는 버퍼는 일회성이므로 vector 써도 무방 (혹은 SendBuffer pooling 사용)
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

    // [수정됨] vector resize 로직 제거
    // RegisterRecv에서 이미 공간 정리가 끝난 상태라고 가정합니다.

    // 남은 공간 계산
    int freeSize = BUFFER_SIZE - writePos_;

    // 혹시라도 공간이 너무 없으면 (RegisterRecv를 안 거치고 왔거나 패킷이 너무 큰 경우)
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
            // 여기서 Disconnect 호출 고려
        }
    }
}

// 1. 외부에서 호출하는 함수 (큐에 넣고 전송 시도)
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

// 2. 실제 WSASend를 호출하는 내부 함수
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

// [수정됨] 버퍼 크기 체크 로직 변경
bool ClientSession::HasCompletePacket() const
{
    // 현재 유효한 데이터 크기 계산
    int dataSize = writePos_ - readPos_;

    // 1) 헤더 크기만큼도 안 왔으면 패킷 아님
    if (dataSize < sizeof(GameHeader)) {
        return false;
    }

    // 2) 헤더를 읽어서 전체 패킷 크기 확인
    // inputBuffer_는 배열이므로 포인터 연산 가능
    const GameHeader* header = reinterpret_cast<const GameHeader*>(&inputBuffer_[readPos_]);

    // 3) 버퍼에 있는 유효 데이터 양이 패킷 전체 크기보다 많거나 같으면 OK
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
        // [수정] 로그인 요청 처리
    case PacketId::LOGIN_REQ:
    {
        // 1. 사이즈 체크 (새 구조체 크기)
        if (bodySize < sizeof(PacketLoginReq)) return nullptr;

        // 2. 캐스팅 (새 구조체)
        const PacketLoginReq* pkt = reinterpret_cast<const PacketLoginReq*>(bodyPtr);

        // 3. 데이터 추출 (char[] -> string)
        std::string username(pkt->username);
        std::string password(pkt->password); // 비밀번호 추가됨

        std::cout << "[RECV] LOGIN_REQ / ID: " << username << std::endl;

        // 4. 커맨드 생성 (roomId 제거됨, password 추가됨)
        command = std::make_unique<LoginCommand>(sessionId_, username, password);
        break;
    }

    // [추가] 회원가입 요청 처리 (새로 만들어야 함)
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

    // [추가] 방 입장 요청 (로그인 후 따로 들어가는 구조로 변경됨)
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
        // 고정 배열 char[] -> std::string 자동 변환
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
        // 보낼 데이터(Body)가 없는 패킷이므로 사이즈 체크 불필요(혹은 0인지 체크)
        std::cout << "[RECV] ROOM_LIST_REQ" << std::endl;
        command = std::make_unique<RoomListCommand>(sessionId_);
        break;
    }

    case PacketId::LOGOUT_REQ:
    {
        std::cout << "[RECV] LOGOUT_REQ" << std::endl;
        // 현재 세션의 유저네임을 가져와서 커맨드 생성
        std::string myName = name_; // ClientSession에 저장된 이름
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
        // [수정됨] vector.size() 대신 writePos_ - readPos_ 사용
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

    // 다 처리했으면 커서 초기화 (최적화)
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
    // [수정됨] 버퍼 공간 관리 로직
    // capacity() 대신 상수 BUFFER_SIZE 사용

    // 남은 공간이 1KB 미만이면 앞으로 땡김 (Compaction)
    if (BUFFER_SIZE - writePos_ < 1024)
    {
        int dataSize = writePos_ - readPos_;

        // 주의: dataSize 자체가 버퍼를 꽉 채우고 있다면(패킷이 64KB에 육박) 답이 없음
        if (dataSize > 0)
        {
            // memmove는 메모리 겹침 현상(overlap)도 안전하게 처리해줌
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