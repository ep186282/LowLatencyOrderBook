#pragma once
#include <cstdint>
#include <cstring>
#include "OrderPool.h"

#pragma pack(push, 1)
struct BinaryOrderMsg {
    uint64_t id;
    int32_t price;
    uint32_t quantity;
    int8_t side;
};
#pragma pack(pop)

static constexpr size_t BINARY_MSG_SIZE = sizeof(BinaryOrderMsg);

class BinaryProtocol {
public:
    // Encode Order into binary buffer, returns binary message size
    static size_t encode(const Order& order, char* buffer) {
        BinaryOrderMsg msg;
        msg.id = order.id;
        msg.price = order.price;
        msg.quantity = order.quantity;
        msg.side = (order.side == Side::BUY) ? 1 : 2;
        std::memcpy(buffer, &msg, BINARY_MSG_SIZE);
        return BINARY_MSG_SIZE;
    }

    // Decode binary buffer into Order
    static void decode(const char* buffer, Order& out) {
        BinaryOrderMsg msg;
        std::memcpy(&msg, buffer, BINARY_MSG_SIZE);
        out.id = msg.id;
        out.price = msg.price;
        out.quantity = msg.quantity;
        out.side = (msg.side == 1) ? Side::BUY : Side::SELL;
    }
};

