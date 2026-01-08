#pragma once
#include <map>
#include <list>
#include <memory>
#include "OrderPool.h"

// Naive order book implementation that uses std::map (price levels) and std::list (order queues)
class BookNaive {
    std::map<int32_t, std::list<std::shared_ptr<Order>>, std::greater<int32_t>> bids; // bids in descending order
    std::map<int32_t, std::list<std::shared_ptr<Order>>, std::less<int32_t>> asks; // asks in ascending order

public:
    void addOrder(uint64_t id, int32_t price, uint32_t qty, Side side) {
        auto ord = std::make_shared<Order>(id, price, qty, side);
        if (side == Side::BUY) {
            match(ord, asks, bids);
        } else {
            match(ord, bids, asks);
        }
    }

    template<typename OppBook, typename MyBook>
    void match(std::shared_ptr<Order>& ord, OppBook& opp, MyBook& mine) {
        auto it = opp.begin();
        while (it != opp.end() && ord->quantity > 0) {
            bool canMatch = (ord->side == Side::BUY) 
                ? (it->first <= ord->price) 
                : (it->first >= ord->price);
            if (!canMatch) {
                break;
            }

            auto& list = it->second;
            while (!list.empty() && ord->quantity > 0) {
                auto& top = list.front();
                uint32_t trade = std::min(ord->quantity, top->quantity);
                ord->quantity -= trade;
                top->quantity -= trade;
                // std::cout << "matched " << trade << " units at price " << it->first << "\n";
                if (top->quantity == 0) {
                    list.pop_front();
                }
            }
            
            if (list.empty()) {
                opp.erase(it++);
            } else {
                ++it;
            }
        }
        
        // Remaining quantity rests on the book
        if (ord->quantity > 0) {
            mine[ord->price].push_back(ord);
        }
    }
};
