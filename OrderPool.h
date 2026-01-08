#pragma once
#include <vector>
#include <cstdint>

enum class Side { BUY, SELL };

struct Order {
    uint64_t id;
    int32_t price;
    uint32_t quantity;
    Side side;

    Order() : id(0), price(0), quantity(0), side(Side::BUY) {}
    Order(uint64_t i, int32_t p, uint32_t q, Side s) 
        : id(i), price(p), quantity(q), side(s) {}
};

// Pre-allocate slab of memory for Orders
class OrderPool {
    std::vector<Order> pool;
    std::vector<size_t> free_indices;

public:
    explicit OrderPool(size_t size) {
        pool.resize(size);
        free_indices.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            free_indices.push_back(i);
        }
    }

    Order* allocate() {
        if (free_indices.empty()) {
            return nullptr;
        }
        size_t idx = free_indices.back();
        free_indices.pop_back();
        return &pool[idx];
    }

    void deallocate(Order* ptr) {
        size_t idx = ptr - &pool[0];
        free_indices.push_back(idx);
    }
    
    void reset() {
        free_indices.clear();
        for (size_t i = 0; i < pool.size(); ++i) {
            free_indices.push_back(i);
        }
    }
};
