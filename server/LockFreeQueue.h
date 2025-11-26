#pragma once
#include <queue>
#include <mutex>

template <typename T>
class LockFreeQueue
{
public:
    LockFreeQueue() {}
    ~LockFreeQueue() {}

    bool Push(T item)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        return true;
    }

    bool Pop(T& item)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
        {
            return false;
        }

        item = std::move(queue_.front());
        queue_.pop();

        return true;
    }

private:
    std::mutex mutex_;
    std::queue<T> queue_;
};