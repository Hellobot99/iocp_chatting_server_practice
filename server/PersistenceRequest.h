#pragma once
#include <string>

enum class RequestType {
    NONE,
    SAVE_CHAT,
    LOAD_USER_DATA,
};

struct PersistenceRequest {
    RequestType type;
    uint32_t sessionId;
    std::string query;
    std::string userName;
    std::string message;
};