#pragma once
#include <string>
#include "Protocol.pb.h"

// ★ RequestType 정의는 무조건 여기에 있어야 합니다!
enum class RequestType {
    NONE,
    SAVE_CHAT,
    LOAD_USER_DATA,
};

// 구조체가 Enum을 사용하므로, Enum이 구조체보다 위에 있어야 합니다.
struct PersistenceRequest {
    RequestType type;
    uint32_t sessionId;
    std::string query;
    std::string userName;
    std::string message;
};