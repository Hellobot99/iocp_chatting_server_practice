#include "ClientSession.h"
#include "NetProtocol.h"
#include "Protocol.pb.h"
#include "Command.h"
#include <iostream>

ClientSession::ClientSession(SOCKET sock, uint32_t sessionId)
    : socket_(sock), sessionId_(sessionId), currentRoomId_(-1)
{
}

ClientSession::~ClientSession()
{
    closesocket(socket_);
}

void ClientSession::PostRecv(HANDLE hIOCP)
{
    // TODO: WSARecv 구현 필요 (IOCPWorker에서 호출됨)
}

void ClientSession::PushSendPacket(const std::vector<char>& packetData)
{
    // TODO: 출력 큐에 넣고 WSASend 요청
}

// 1. 수신된 원시 바이트를 벡터 뒤에 붙이기
void ClientSession::AppendToInputBuffer(const char* data, size_t len)
{
    std::lock_guard<std::mutex> lock(lock_);
    inputBuffer_.insert(inputBuffer_.end(), data, data + len);
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

// 3. 버퍼에서 완전한 패킷을 추출하고 변환 (핵심 수정 부분)
std::unique_ptr<ICommand> ClientSession::DeserializeCommand()
{
    std::lock_guard<std::mutex> lock(lock_);

    // 1. 헤더 크기 체크
    if (inputBuffer_.size() < sizeof(GameHeader)) {
        return nullptr;
    }

    // 2. 헤더 읽기 (Peek) - 벡터의 첫 번째 주소를 캐스팅
    GameHeader* header = reinterpret_cast<GameHeader*>(inputBuffer_.data());

    // 3. 패킷 전체 데이터가 다 왔는지 재확인
    if (inputBuffer_.size() < header->packetSize) {
        return nullptr;
    }

    // 4. Protobuf 파싱을 위한 포인터 계산
    // 헤더 바로 뒷부분부터 데이터(Body) 시작
    const char* bodyPtr = inputBuffer_.data() + sizeof(GameHeader);
    int bodySize = header->packetSize - sizeof(GameHeader);

    std::unique_ptr<ICommand> command = nullptr;

    // 5. 패킷 ID에 따라 분기
    switch (header->packetId)
    {
    case Protocol::C_MOVE:
    {
        Protocol::C_Move pkt;
        if (pkt.ParseFromArray(bodyPtr, bodySize)) {
            command = std::make_unique<MoveCommand>(sessionId_, pkt);
        }
        break;
    }
    case Protocol::C_CHAT:
    {
        Protocol::C_Chat pkt;
        if (pkt.ParseFromArray(bodyPtr, bodySize)) {
            command = std::make_unique<ChatCommand>(sessionId_, pkt);
        }
        break;
    }
    // 나중에 로그인 패킷 등 추가
    default:
        std::cout << "Unknown Packet ID: " << header->packetId << std::endl;
        break;
    }

    // 6. 처리 완료된 패킷 데이터 삭제 (중요!)
    // 벡터의 앞에서부터 packetSize만큼 잘라냄
    inputBuffer_.erase(inputBuffer_.begin(), inputBuffer_.begin() + header->packetSize);

    return command;
}