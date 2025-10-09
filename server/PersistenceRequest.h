#pragma once

#include <cstdint>
#include <string>
#include <functional>

// DBTP 워커가 처리해야 할 요청의 종류
enum class PersistenceType
{
    SAVE_CHAT_LOG,
    LOAD_PLAYER_DATA,
    UPDATE_PLAYER_LOCATION
    //... 기타 필요한 DB/Redis 작업
};

//======================================================================
// GLT에서 DBTP로 전달되는 작업 요청 객체
// [20]
//======================================================================
class PersistenceRequest
{
public:
    PersistenceRequest(PersistenceType type) : type_(type) {}

    PersistenceType GetType() const { return type_; }

    // 채팅 로그 저장 데이터
    std::string chatUsername;
    std::string chatMessage;

    // DB 작업 완료 후 GLT로 결과를 통지하기 위한 콜백 함수 (선택 사항)
    // GLT는 PostQueuedCompletionStatus 등을 사용하여 결과를 받을 수 있음
    // std::function<void(bool success)> completionCallback;

private:
    PersistenceType type_;
};