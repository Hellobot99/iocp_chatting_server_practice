#pragma once
#include <queue>
#include <mutex>

template <typename T>
class LockFreeQueue
{
public:
    // 생성자/소멸자 (필요시 추가)
    LockFreeQueue() {}
    ~LockFreeQueue() {}

    // [Push 구현]
    // 데이터를 큐에 넣습니다.
    bool Push(T item)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item)); // 이동 시멘틱으로 효율성 확보
        return true;
    }

    // [Pop 구현]
    // 큐에서 데이터를 꺼냅니다. 비어있으면 false를 반환합니다.
    bool Pop(T& item)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
        {
            return false;
        }

        // front()로 가져오고 pop()으로 제거
        item = std::move(queue_.front());
        queue_.pop();

        return true;
    }

private:
    // 현재는 Mutex를 사용하므로 엄밀히 말하면 'Lock-Free'는 아니지만,
    // 'Thread-Safe Queue'로서 기능은 완벽하게 수행합니다.
    // 나중에 성능 최적화가 필요하면 이곳을 실제 Lock-Free 알고리즘으로 교체하면 됩니다.
    std::mutex mutex_;
    std::queue<T> queue_;
};