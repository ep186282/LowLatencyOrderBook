#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <atomic>

#include "BookNaive.h"
#include "BookCacheOptimized.h"
#include "BinaryProtocol.h"
#include "SPSCQueue.h"

// Generate binary-encoded order messages
std::vector<std::vector<char>> generateBinaryData(int count) {
    std::vector<std::vector<char>> data;
    data.reserve(count);
    std::mt19937 rng(28);
    std::uniform_int_distribution<int> price(90, 110);
    
    for (int i = 0; i < count; ++i) {
        Order order;
        order.id = i;
        order.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        order.quantity = 100;
        order.price = price(rng);
        
        std::vector<char> buffer(BINARY_MSG_SIZE);
        BinaryProtocol::encode(order, buffer.data());
        data.push_back(std::move(buffer));
    }
    return data;
}

int main() {
    const int NUM_ORDERS = 100000;
    std::cout << "Order Book Benchmark (" << NUM_ORDERS << " orders)\n" << std::endl;
    auto data = generateBinaryData(NUM_ORDERS);

    // Benchmark 1: Naive (map + list)
    {
        std::cout << "Naive (map + list)" << std::endl;
        BookNaive book;
        Order temp; 
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& buf : data) {
            BinaryProtocol::decode(buf.data(), temp);
            book.addOrder(temp.id, temp.price, temp.quantity, temp.side);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "  " << ms << " ms  |  " << (int)(NUM_ORDERS / ms * 1000) << " orders/sec\n" << std::endl;
    }

    // Benchmark 2: Cache-Optimized (vector + pool)
    {
        std::cout << "Cache-Optimized (vector + pool)" << std::endl;
        OrderPool pool(NUM_ORDERS * 2);
        BookCacheOptimized book(pool);
        Order temp;

        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& buf : data) {
            BinaryProtocol::decode(buf.data(), temp);
            book.addOrder(temp.id, temp.price, temp.quantity, temp.side);
            // std::cout << "order " << temp.id << " processed\n";
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "  " << ms << " ms  |  " << (int)(NUM_ORDERS / ms * 1000) << " orders/sec" << std::endl;
        std::cout << std::endl;
    }

    // Benchmark 3: Cache-Optimized + SPSC pipeline
    {
        std::cout << "Cache-Optimized + SPSC Queue (2 threads)" << std::endl;
        
        constexpr size_t QUEUE_CAPACITY = 65536; // 2^16
        SPSCQueue<Order, QUEUE_CAPACITY> queue;
        
        std::atomic<bool> producer_done{false};
        std::atomic<int64_t> orders_processed{0};
        
        // Pre-decode orders to isolate queue overhead measurement
        std::vector<Order> parsed_orders;
        parsed_orders.reserve(NUM_ORDERS);
        for (const auto& buf : data) {
            Order temp;
            BinaryProtocol::decode(buf.data(), temp);
            parsed_orders.push_back(temp);
        }
        
        OrderPool pool(NUM_ORDERS * 2);
        BookCacheOptimized book(pool);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::thread producer([&]() {
            for (const auto& order : parsed_orders) {
                while (!queue.push(order)) {
                    std::this_thread::yield();
                }
            }
            producer_done.store(true, std::memory_order_release);
        });
        
        std::thread consumer([&]() {
            Order order;
            int64_t count = 0;
            
            while (true) {
                if (queue.pop(order)) {
                    book.addOrder(order.id, order.price, order.quantity, order.side);
                    count++;
                } else if (producer_done.load(std::memory_order_acquire)) {
                    // Drain remaining items
                    while (queue.pop(order)) {
                        book.addOrder(order.id, order.price, order.quantity, order.side);
                        count++;
                    }
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
            orders_processed.store(count, std::memory_order_relaxed);
        });
        
        producer.join();
        consumer.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::cout << "  " << ms << " ms  |  " << (int)(NUM_ORDERS / ms * 1000) << " orders/sec" << std::endl;
        std::cout << std::endl;
    }

    std::cout << "Benchmark completed" << std::endl;
    return 0;
}
