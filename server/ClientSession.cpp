#include "ClientSession.h"

ClientSession::ClientSession(SOCKET sock, uint32_t sessionId) {

}

ClientSession::~ClientSession() {

}

// 비동기 Recv를 다시 거는 함수
void ClientSession::PostRecv(HANDLE hIOCP) {

}

// GLT가 이 세션에게 패킷 전송을 요청할 때 사용
void ClientSession::PushSendPacket(const std::vector<char>& packetData) {

}

// 1. 수신된 원시 바이트를 내부 버퍼에 추가합니다.
void ClientSession::AppendToInputBuffer(const char* data, size_t len) {

}

// 2. 버퍼에 최소 하나의 완전한 패킷이 있는지 확인합니다.
bool ClientSession::HasCompletePacket() const {

}

// 3. 버퍼에서 완전한 패킷을 추출하고, ICommand 객체로 변환합니다.
std::unique_ptr<ICommand> ClientSession::DeserializeCommand() {

}