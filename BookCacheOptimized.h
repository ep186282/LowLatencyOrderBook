#pragma once
#include <vector>
#include <algorithm>
#include "OrderPool.h"

struct Level {
    int32_t price;
    std::vector<Order*> orders;
    size_t head = 0;
    bool active = true;
};

// Uses contiguous vectors and memory pool to minimize cache misses and allocations
class BookCacheOptimized {
    std::vector<Level> bids; 
    std::vector<Level> asks;
    OrderPool& pool;

public:
    explicit BookCacheOptimized(OrderPool& p) : pool(p) {
        bids.reserve(10000);
        asks.reserve(10000);
    }

    void addOrder(uint64_t id, int32_t price, uint32_t qty, Side side) {
        Order* ord = pool.allocate();
        if (!ord) {
            return;
        }
        ord->id = id;
        ord->price = price;
        ord->quantity = qty;
        ord->side = side;

        if (side == Side::BUY) {
            match(ord, asks, bids, true);
        } else {
            match(ord, bids, asks, false);
        }
    }

private:
    void match(Order* ord, std::vector<Level>& opp, std::vector<Level>& mine, bool isBuy) {
        // Walk through opposing price levels
        for (size_t i = 0; i < opp.size(); ) {
            if (ord->quantity == 0) {
                break;
            }

            Level& lvl = opp[i];
            if (!lvl.active) {
                i++;
                continue;
            }
            bool canMatch = isBuy ? (lvl.price <= ord->price) : (lvl.price >= ord->price);
            if (!canMatch) {
                break;
            }

            for (size_t j = lvl.head; j < lvl.orders.size(); ) {
                Order* bookOrd = lvl.orders[j];
                uint32_t trade = std::min(ord->quantity, bookOrd->quantity);
                // std::cout << "trade: " << trade << " at " << lvl.price << std::endl;
                ord->quantity -= trade;
                bookOrd->quantity -= trade;

                if (bookOrd->quantity == 0) {
                    pool.deallocate(bookOrd);
                    lvl.head++;
                    j++;
                } else {
                    j++;
                }
                if (ord->quantity == 0) {
                    break;
                }
            }

            if (lvl.head >= lvl.orders.size()) {
                lvl.active = false;
                // TODO: compact inactive levels when this gets too sparse
            }
            i++;
        }

        // Remaining quantity rests on the book
        if (ord->quantity > 0) {
            bool found = false;
            // TODO: improve efficiency of search, current linear search
            for (auto& lvl : mine) {
                if (lvl.price == ord->price) {
                    lvl.orders.push_back(ord);
                    lvl.active = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                Level l;
                l.price = ord->price;
                l.orders.reserve(128);
                l.orders.push_back(ord);
                
                auto pos = isBuy 
                    ? std::lower_bound(mine.begin(), mine.end(), l, 
                        [](const Level& a, const Level& b) { return a.price > b.price; })
                    : std::lower_bound(mine.begin(), mine.end(), l, 
                        [](const Level& a, const Level& b) { return a.price < b.price; });
                mine.insert(pos, std::move(l));
            }
        } else {
            pool.deallocate(ord);
        }
    }
};
