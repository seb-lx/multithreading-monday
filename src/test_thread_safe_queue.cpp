#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

#include "thread_safe_queue.hpp"


auto main() -> int {

    auto tsq = TSQ<int>{};

    {
        // producer adds element to queue
        auto producer = std::jthread([&tsq]() {
            for (int i = 1; i <= 10; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                std::cout << "[Producer] pushing element: " << i << "\n";
                tsq.push(i);
            }
        });


        // consumers take elements from queue concurrently
        auto consumer_func = [&tsq](int id) {
            for (int i = 0; i < 5; ++i) {
                int value;
                tsq.wait_and_pop(value);
                std::cout << "[Consumer " << id << "] popped element: " << value << "\n";
            }
        };

        auto consumer1 = std::jthread(consumer_func, 1);
        auto consumer2 = std::jthread(consumer_func, 2);
    }
    // automatic join, scope ends


    std::cout << "[Main] queue empty? " << (tsq.empty() ? "Yes" : "No") << "\n";
    
    std::cout << "[Main] pushing fallback element: 42\n";
    tsq.push(42);

    int fallback_res;
    if (tsq.try_pop(fallback_res)) {
        std::cout << "[Main] try_pop(T&) success! Value: " << fallback_res << "\n";
    }

    auto ptr_res = tsq.try_pop();
    if (!ptr_res) {
        std::cout << "[Main] try_pop() safely returned nullptr on empty queue.\n";
    }

    return 0;
}
