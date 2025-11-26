#include <iostream>
#include "ClientSession.h"
#include "NetProtocol.h"
#include "Server.h"
#include "Command.h"

extern Server* g_Server;

ClientSession::ClientSession(SOCKET sock, uint32_t sessionId)
    : socket_(sock), sessionId_(sessionId), currentRoomId_(-1)

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
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    std::cout << "[Session] Disconnected Client: " << sessionId_ << std::endl;

    if (g_Server)
        g_Server->RemoveSession(sessionId_);
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

void ClientSession::PostRecv(HANDLE hIOCP)
{
    DWORD flags = 0;
    DWORD recvBytes = 0;

    ZeroMemory(&recvIoData_.overlapped, sizeof(OVERLAPPED));

    // 1. 버퍼 공간 확보 (꽉 찼으면 늘림)
    // vector의 size()가 아니라 '총 용량(capacity)' 대비 '현재 쓰고 있는 위치(writePos)'를 봐야 함
    if (inputBuffer_.capacity() - writePos_ < 1024)
    {
        // 여유 공간이 1024 미만이면 늘림
        // resize를 해서 실제 물리 메모리를 확보해야 &inputBuffer_[writePos_] 접근이 안전함
        inputBuffer_.resize(inputBuffer_.capacity() + 4096);
    }

    // 2. WSABUF 설정 (writePos_ 위치부터 받겠다!)
    recvIoData_.wsaBuf.buf = &inputBuffer_[writePos_];
    recvIoData_.wsaBuf.len = (ULONG)(inputBuffer_.capacity() - writePos_);
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
// 1. 외부에서 호출하는 함수 (큐에 넣고 전송 시도)
void ClientSession::PushSendPacket(const std::vector<char>& packetData)
{
    // 데이터를 스마트 포인터로 만들어서 큐에 넣음 (복사 비용 최소화)
    auto packet = std::make_shared<std::vector<char>>(packetData);
    outputQueue_.Push(packet);

    // "지금 아무도 안 보내고 있나요?" 체크
    // isSending_이 false라면 true로 바꾸고 if문 진입 (Atomic 연산)
    bool expected = false;
    if (isSending_.compare_exchange_strong(expected, true))
    {
        // 내가 총대 메고 전송 시작
        FlushSend();
    }
    // 누군가 보내고 있다면(else), 큐에 넣어만 두고 리턴. 
    // (앞선 전송이 끝나면 워커 스레드가 이어서 보내줄 것임)
}

// 2. 실제 WSASend를 호출하는 내부 함수
void ClientSession::FlushSend()
{
    std::shared_ptr<std::vector<char>> packet;

    // 큐에서 하나 꺼내옴
    if (!outputQueue_.Pop(packet))
    {
        // 더 이상 보낼 게 없으면 전송 상태 해제
        isSending_ = false;
        return;
    }

    // 중요: 비동기 작업이 끝날 때까지 메모리가 살아있어야 하므로 멤버 변수에 보관
    currentSendingPacket_ = packet;

    // OVERLAPPED 초기화 및 설정
    ZeroMemory(&sendIoData_.overlapped, sizeof(OVERLAPPED));
    sendIoData_.operation = 1; // 1 = SEND (IOCPWorker에서 구분용)

    // WSABUF 설정
    sendIoData_.wsaBuf.buf = currentSendingPacket_->data();
    sendIoData_.wsaBuf.len = (ULONG)currentSendingPacket_->size();

    DWORD sendBytes = 0;
    DWORD flags = 0;

    // 비동기 전송 요청
    if (WSASend(socket_, &sendIoData_.wsaBuf, 1, &sendBytes, flags, &sendIoData_.overlapped, NULL) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSA_IO_PENDING)
        {
            // 진짜 에러 발생 (로그 출력 or Disconnect)
            isSending_ = false;
            // Disconnect();
        }
    }
}

// 2. 버퍼에 최소 하나의 완전한 패킷이 있는지 확인
bool ClientSession::HasCompletePacket() const
{
    // 1) 헤더 크기만큼도 안 왔으면 패킷 아님
    if (inputBuffer_.size() < sizeof(GameHeader)) {
        return false;
    }

    // 2) 헤더를 읽어서 전체 패킷 크기 확인
    const GameHeader* header = reinterpret_cast<const GameHeader*>(inputBuffer_.data());

    // 3) 버퍼에 있는 데이터 양이 패킷 전체 크기보다 많거나 같으면 OK
    return inputBuffer_.size() >= header->packetSize;
}

std::unique_ptr<ICommand> ClientSession::DeserializeCommand()
{
    std::lock_guard<std::mutex> lock(lock_);

    // 1. 헤더 읽기
    GameHeader* header = reinterpret_cast<GameHeader*>(&inputBuffer_[readPos_]);

    // 2. 바디 포인터 계산
    const char* bodyPtr = reinterpret_cast<const char*>(&inputBuffer_[readPos_]) + sizeof(GameHeader);
    int bodySize = header->packetSize - sizeof(GameHeader);

    std::unique_ptr<ICommand> command = nullptr;

    // 패킷 ID (enum class 형변환)
    PacketId pktId = static_cast<PacketId>(header->packetId);

    switch (pktId)
    {
    case PacketId::LOGIN:
    {
        // 로그인 패킷 처리 (지금은 데이터가 없으므로 로그만)
        std::cout << "[RECV] LOGIN Request" << std::endl;
        // 필요하다면 Command 생성
    }
    break;

    case PacketId::CHAT:
    {
        const PacketChat* pkt = reinterpret_cast<const PacketChat*>(bodyPtr);

        std::string msg = pkt->msg;

        command = std::make_unique<ChatCommand>(sessionId_, msg);
    }
    break;

    case PacketId::MOVE:
    {
        // 이동 패킷: 구조체로 캐스팅
        // 데이터 크기 체크 (안전장치)
        if (bodySize < sizeof(PacketMove))
        {
            std::cout << "[Error] Invalid Move Packet Size" << std::endl;
            return nullptr;
        }

        const PacketMove* pkt = reinterpret_cast<const PacketMove*>(bodyPtr);

        // 구조체의 vx, vy를 꺼내서 Command 생성
        command = std::make_unique<MoveCommand>(sessionId_, pkt->vx, pkt->vy);
    }
    break;

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
            // [에러 처리]
            // 헤더는 읽혔는데(ID는 알겠는데) 파싱이 실패했거나, 알 수 없는 ID인 경우
            // 여기서 연결을 끊어야 안전합니다.
            // (Login 패킷처럼 커맨드를 안 만드는 경우는 예외 처리 필요)

            PacketId pktId = static_cast<PacketId>(header->packetId);
            if (pktId == PacketId::LOGIN) {
                // 로그인 패킷은 커맨드를 안 만들었으므로 정상 처리로 간주하고 스킵
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
    // 1. 커서가 너무 뒤로 갔나? (남은 공간이 별로 없을 때)
    if (inputBuffer_.capacity() - writePos_ < 1024) // 예: 1KB 미만 남음
    {
        // 2. 읽어야 할 유효 데이터 크기
        int dataSize = writePos_ - readPos_;

        if (dataSize > 0)
        {
            // 3. 유효 데이터만 맨 앞으로 복사 (이때만 복사 비용 발생!)
            // memmove는 메모리 영역이 겹쳐도 안전하게 복사해줍니다.
            std::memmove(&inputBuffer_[0], &inputBuffer_[readPos_], dataSize);
        }

        // 4. 커서 재조정
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