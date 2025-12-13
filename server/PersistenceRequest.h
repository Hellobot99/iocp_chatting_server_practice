#pragma once
#include <string>

enum class RequestType {
    NONE,
    SAVE_CHAT,
    LOAD_USER_DATA,
    REGISTER,
    LOGIN,
};

struct PersistenceRequest {
    RequestType type;
    uint32_t sessionId;
    std::string username;
    std::string password;
    std::string message;
};