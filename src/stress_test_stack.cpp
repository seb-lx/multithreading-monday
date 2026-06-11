/*
 * stress_test_stack.cpp — Correctness stress test for thread-safe stacks
 *
 * Verifies that no elements are lost or duplicated under concurrent access.
 * Run with ThreadSanitizer for best results:
 *
 *   cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON -B build_tsan
 *   cmake --build build_tsan --target stress_test_stack
 *   ./build_tsan/stress_test_stack
 */

#include <cstddef>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "bench_types.hpp"
#include "thread_safe_stack.hpp"
// ── Add new version headers here ─────────────────────────
// #include "thread_safe_stack_v2.hpp"


constexpr std::size_t ITEMS_PER_THREAD = 10'000;
constexpr std::size_t NUM_ITERATIONS   = 10;


// ─────────────────────────────────────────────────────────
//  Test 1: Concurrent push, then sequential verification
// ─────────────────────────────────────────────────────────
//
//  N threads each push a unique range of values [t*M, (t+1)*M).
//  After all threads join, pop everything and verify the
//  exact set of values is present — no loss, no duplicates.
//  T = element type (int or HeavyPayload).

template<typename Stack, typename T = int>
auto test_concurrent_push(const std::string& label, std::size_t num_threads) -> bool {
    std::cout << "  [" << label << "] concurrent push ("
              << num_threads << " threads x " << ITEMS_PER_THREAD << " items)... ";
    std::cout.flush();

    std::size_t passed = 0u;

    for (std::size_t iter = 0u; iter < NUM_ITERATIONS; ++iter) {
        auto stack   = Stack{};
        auto threads = std::vector<std::thread>{};

        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&stack, t]() {
                auto base = static_cast<int>(t * ITEMS_PER_THREAD);
                for (std::size_t i = 0u; i < ITEMS_PER_THREAD; ++i) {
                    stack.push(T{base + static_cast<int>(i)});
                }
            });
        }
        for (auto& th : threads) { th.join(); }

        // pop all and verify
        auto values = std::set<T>{};
        bool ok = true;

        while (!stack.empty()) {
            try {
                auto val = stack.pop();
                if (!values.insert(*val).second) {
                    ok = false;
                    break;
                }
            } catch (...) {
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
//
//  N producers push unique values, N consumers pop concurrently.
//  After all threads finish, verify that every pushed value
//  was either popped by a consumer or remains in the stack.
//  Checks for both data loss and duplication.

template<typename Stack, typename T = int>
auto test_concurrent_push_pop(const std::string& label, std::size_t num_threads) -> bool {
    std::cout << "  [" << label << "] push+pop ("
              << num_threads << "P + " << num_threads << "C)... ";
    std::cout.flush();

    std::size_t passed = 0u;

    for (std::size_t iter = 0u; iter < NUM_ITERATIONS; ++iter) {
        auto stack   = Stack{};
        auto threads = std::vector<std::thread>{};

        // each consumer collects its popped values
        auto consumer_values = std::vector<std::vector<T>>(num_threads);

        // producers: push unique ranges
        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&stack, t]() {
                auto base = static_cast<int>(t * ITEMS_PER_THREAD);
                for (std::size_t i = 0u; i < ITEMS_PER_THREAD; ++i) {
                    stack.push(T{base + static_cast<int>(i)});
                }
            });
        }

        // consumers: each pops ITEMS_PER_THREAD items
        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&stack, &consumer_values, t]() {
                consumer_values[t].reserve(ITEMS_PER_THREAD);
                for (std::size_t i = 0u; i < ITEMS_PER_THREAD; ) {
                    try {
                        auto val = stack.pop();
                        consumer_values[t].push_back(*val);
                        ++i;
                    } catch (...) {
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& th : threads) { th.join(); }

        // drain any remaining items
        auto remaining = std::vector<T>{};
        while (!stack.empty()) {
            try {
                auto val = stack.pop();
                remaining.push_back(*val);
            } catch (...) {
                break;
            }
        }

        // merge all values and verify: no dups, no loss
        auto all = std::set<T>{};
        bool has_dups = false;

        for (const auto& cv : consumer_values) {
            for (const auto& v : cv) {
                if (!all.insert(v).second) { has_dups = true; }
            }
        }
        for (const auto& v : remaining) {
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

    std::cout << "Stack Stress Test\n"
              << std::string(40u, '=') << "\n"
              << "Hardware threads : " << hwc << "\n"
              << "Items per thread : " << ITEMS_PER_THREAD << "\n"
              << "Iterations       : " << NUM_ITERATIONS << "\n\n";

    bool all_passed = true;

    // ── int ──────────────────────────────────────────────
    all_passed &= test_concurrent_push<TSS<int>, int>("TSS v1 (int)", hwc);
    all_passed &= test_concurrent_push_pop<TSS<int>, int>("TSS v1 (int)", hwc);

    // ── HeavyPayload (1KB) ───────────────────────────────
    all_passed &= test_concurrent_push<TSS<HeavyPayload>, HeavyPayload>("TSS v1 (1KB)", hwc);
    all_passed &= test_concurrent_push_pop<TSS<HeavyPayload>, HeavyPayload>("TSS v1 (1KB)", hwc);

    // ── Add new versions here ────────────────────────────
    // all_passed &= test_concurrent_push<TSS_v2<int>, int>("TSS v2 (int)", hwc);

    std::cout << "\n" << (all_passed ? "All tests PASSED" : "Some tests FAILED") << "\n";
    return all_passed ? 0 : 1;
}
