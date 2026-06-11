/*
 * stress_test_queue.cpp — Correctness stress test for thread-safe queues
 *
 * Verifies that no elements are lost or duplicated under concurrent access.
 * Run with ThreadSanitizer for best results:
 *
 *   cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -B build_tsan
 *   cmake --build build_tsan --target stress_test_queue
 *   ./build_tsan/stress_test_queue
 */

#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "thread_safe_queue.hpp"
// ── Add new version headers here ─────────────────────────
#include "thread_safe_queue_v2.hpp"


constexpr std::size_t ITEMS_PER_THREAD = 10'000;
constexpr std::size_t NUM_ITERATIONS   = 10;


// ─────────────────────────────────────────────────────────
//  Test 1: Concurrent push, then sequential verification
// ─────────────────────────────────────────────────────────

template<typename Queue>
auto test_concurrent_push(const std::string& label, std::size_t num_threads) -> bool {
    std::cout << "  [" << label << "] concurrent push ("
              << num_threads << " threads x " << ITEMS_PER_THREAD << " items)... ";
    std::cout.flush();

    std::size_t passed = 0u;

    for (std::size_t iter = 0u; iter < NUM_ITERATIONS; ++iter) {
        auto queue   = Queue{};
        auto threads = std::vector<std::thread>{};

        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&queue, t]() {
                auto base = static_cast<int>(t * ITEMS_PER_THREAD);
                for (std::size_t i = 0u; i < ITEMS_PER_THREAD; ++i) {
                    queue.push(base + static_cast<int>(i));
                }
            });
        }
        for (auto& th : threads) { th.join(); }

        // drain and verify
        auto values = std::set<int>{};
        bool ok = true;

        while (!queue.empty()) {
            auto val = queue.try_pop();
            if (val) {
                if (!values.insert(*val).second) {
                    ok = false;
                    break;
                }
            } else {
                break;
            }
        }

        if (ok && values.size() == num_threads * ITEMS_PER_THREAD) {
            ++passed;
        }
    }

    auto success = (passed == NUM_ITERATIONS);
    std::cout << (success ? "PASS" : "FAIL")
              << " (" << passed << "/" << NUM_ITERATIONS << ")\n";
    return success;
}


// ─────────────────────────────────────────────────────────
//  Test 2: Concurrent push + pop simultaneously
// ─────────────────────────────────────────────────────────

template<typename Queue>
auto test_concurrent_push_pop(const std::string& label, std::size_t num_threads) -> bool {
    std::cout << "  [" << label << "] push+pop ("
              << num_threads << "P + " << num_threads << "C)... ";
    std::cout.flush();

    std::size_t passed = 0u;

    for (std::size_t iter = 0u; iter < NUM_ITERATIONS; ++iter) {
        auto queue   = Queue{};
        auto threads = std::vector<std::thread>{};

        auto consumer_values = std::vector<std::vector<int>>(num_threads);

        // producers
        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&queue, t]() {
                auto base = static_cast<int>(t * ITEMS_PER_THREAD);
                for (std::size_t i = 0u; i < ITEMS_PER_THREAD; ++i) {
                    queue.push(base + static_cast<int>(i));
                }
            });
        }

        // consumers: use try_pop() (non-blocking, no exceptions)
        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&queue, &consumer_values, t]() {
                consumer_values[t].reserve(ITEMS_PER_THREAD);
                for (std::size_t i = 0u; i < ITEMS_PER_THREAD; ) {
                    auto val = queue.try_pop();
                    if (val) {
                        consumer_values[t].push_back(*val);
                        ++i;
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& th : threads) { th.join(); }

        // drain remaining
        auto remaining = std::vector<int>{};
        while (true) {
            auto val = queue.try_pop();
            if (!val) { break; }
            remaining.push_back(*val);
        }

        // merge and verify
        auto all = std::set<int>{};
        bool has_dups = false;

        for (const auto& cv : consumer_values) {
            for (auto v : cv) {
                if (!all.insert(v).second) { has_dups = true; }
            }
        }
        for (auto v : remaining) {
            if (!all.insert(v).second) { has_dups = true; }
        }

        if (!has_dups && all.size() == num_threads * ITEMS_PER_THREAD) {
            ++passed;
        }
    }

    auto success = (passed == NUM_ITERATIONS);
    std::cout << (success ? "PASS" : "FAIL")
              << " (" << passed << "/" << NUM_ITERATIONS << ")\n";
    return success;
}


// ─────────────────────────────────────────────────────────

auto main() -> int {
    auto hwc = static_cast<std::size_t>(std::thread::hardware_concurrency());
    if (hwc == 0u) { hwc = 4u; }

    std::cout << "Queue Stress Test\n"
              << std::string(40u, '=') << "\n"
              << "Hardware threads : " << hwc << "\n"
              << "Items per thread : " << ITEMS_PER_THREAD << "\n"
              << "Iterations       : " << NUM_ITERATIONS << "\n\n";

    bool all_passed = true;

    all_passed &= test_concurrent_push<TSQ<int>>("TSQ v1 (mutex, T)", hwc);
    all_passed &= test_concurrent_push_pop<TSQ<int>>("TSQ v1 (mutex, T)", hwc);
    // ── Add new versions here ────────────────────────────
    all_passed &= test_concurrent_push<TSQv2<int>>("TSQ v2 (mutex, shared_ptr<T>)", hwc);
    all_passed &= test_concurrent_push_pop<TSQv2<int>>("TSQ v2 (mutex, shared_ptr<T>)", hwc);

    std::cout << "\n" << (all_passed ? "All tests PASSED" : "Some tests FAILED") << "\n";
    return all_passed ? 0 : 1;
}
