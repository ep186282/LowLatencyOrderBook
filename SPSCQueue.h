#pragma once
#include <atomic>
#include <vector>

template<typename T, size_t N>
class SPSCQueue {
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::vector<T> data_ = std::vector<T>(N);

public:
    bool push(const T& item) {
        size_t h = head_.load();
        if ((h + 1) % N == tail_.load()) {
            return false;
        }
        data_[h] = item;
        head_.store((h + 1) % N);
        return true;
    }

    bool pop(T& item) {
        size_t t = tail_.load();
        if (t == head_.load()) {
            return false;
        }
        item = data_[t];
        tail_.store((t + 1) % N);
        return true;
    }

    bool empty() const {
        return head_.load() == tail_.load();
    }
};
