#include <iostream>
#include <thread>
#include <vector>

#include "thread_safe_stack.hpp"



auto main() -> int {

    std::cout << "[Master thread] Hello!\n";

    auto tss = TSS<std::size_t>{};

    auto threads = std::vector<std::thread>{};

    auto hwt = std::thread::hardware_concurrency();
    std::cout << "[Master thread] Threads available: " << hwt << "\n";

    auto add = [&tss](std::size_t id) {
        for (std::size_t i = 0; i < 10; ++i) {
            auto e = i + (10 * id);
            std::cout << "[Thread " << id << " ] pushing element " << e << "\n";
            tss.push(e);
            //std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    for (std::size_t i = 0; i < hwt; ++i) {
        threads.emplace_back(std::thread(add, i));
    }

    for (auto& thread: threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    while(!tss.empty()) {
        auto result = std::size_t{};
        tss.pop(result);
        std::cout << "[Master thread] Popped element: " << result << "\n";
    }

    return 0;
}
