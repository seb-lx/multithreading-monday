/*
 * bench.cpp — Throughput benchmark for thread-safe data structures
 *
 * Measures operations/second under varying thread counts.
 * Designed for easy comparison between implementation versions.
 *
 * Usage: ./bench [ops_per_thread]    (default: 200000)
 *
 * To add a new version:
 *   1. #include the new header below
 *   2. Add an entry to the benchmark suite in main()
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "thread_safe_stack.hpp"
#include "thread_safe_queue.hpp"
// ── Add new version headers here ─────────────────────────
// #include "thread_safe_stack_v2.hpp"
#include "thread_safe_queue_v2.hpp"


// ═════════════════════════════════════════════════════════
//  Configuration
// ═════════════════════════════════════════════════════════

constexpr std::size_t DEFAULT_OPS = 200'000;
constexpr std::size_t NUM_RUNS    = 5;   // runs per config, take median


// ═════════════════════════════════════════════════════════
//  Helpers
// ═════════════════════════════════════════════════════════

using Clock = std::chrono::high_resolution_clock;

auto median_of(std::vector<double>& v) -> double {
    std::ranges::sort(v);
    auto n = v.size();
    if (n % 2u == 0u) {
        return (v[n / 2u - 1u] + v[n / 2u]) / 2.0;
    }
    return v[n / 2u];
}

auto format_throughput(double ops_per_sec) -> std::string {
    auto ss = std::ostringstream{};
    ss << std::fixed;
    if (ops_per_sec >= 1e6) {
        ss << std::setprecision(2) << (ops_per_sec / 1e6) << "M ops/s";
    } else if (ops_per_sec >= 1e3) {
        ss << std::setprecision(1) << (ops_per_sec / 1e3) << "K ops/s";
    } else {
        ss << std::setprecision(0) << ops_per_sec << " ops/s";
    }
    return ss.str();
}


// ═════════════════════════════════════════════════════════
//  Benchmark: push-only  (pure write contention)
// ═════════════════════════════════════════════════════════
//
//  Every thread pushes into the container concurrently.
//  Shows how lock contention scales with thread count.

template<typename Container>
auto bench_push_only(std::size_t num_threads, std::size_t ops_per_thread) -> double {
    auto times = std::vector<double>{};
    times.reserve(NUM_RUNS);

    for (std::size_t run = 0u; run < NUM_RUNS; ++run) {
        auto container = Container{};
        auto threads   = std::vector<std::thread>{};
        threads.reserve(num_threads);

        // spin-barrier: all threads wait for the go signal
        auto ready = std::atomic<std::size_t>{0u};
        auto go    = std::atomic<bool>{false};

        for (std::size_t t = 0u; t < num_threads; ++t) {
            threads.emplace_back([&container, &ready, &go, ops_per_thread]() {
                ready.fetch_add(1u, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (std::size_t i = 0u; i < ops_per_thread; ++i) {
                    container.push(static_cast<int>(i));
                }
            });
        }

        while (ready.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }

        auto start = Clock::now();
        go.store(true, std::memory_order_release);
        for (auto& th : threads) { th.join(); }
        auto end = Clock::now();

        times.push_back(
            std::chrono::duration<double, std::milli>(end - start).count()
        );
    }

    return median_of(times);
}


// ═════════════════════════════════════════════════════════
//  Benchmark: producer-consumer  (Stack)
// ═════════════════════════════════════════════════════════
//
//  Half the threads push, half pop concurrently.
//  Most revealing for fine-grained vs coarse-grained locking,
//  because fine-grained locks can allow concurrent push+pop.
//  Uses pop(T&) to avoid shared_ptr allocation overhead
//  so we measure lock contention, not allocator performance.

template<typename Stack>
auto bench_stack_mixed(std::size_t num_threads, std::size_t ops_per_thread) -> double {
    if (num_threads < 2u) { return 0.0; }

    auto num_producers = num_threads / 2u;
    auto num_consumers = num_threads - num_producers;

    auto times = std::vector<double>{};
    times.reserve(NUM_RUNS);

    for (std::size_t run = 0u; run < NUM_RUNS; ++run) {
        auto stack = Stack{};

        // pre-fill so consumers don't starve immediately
        for (std::size_t i = 0u; i < ops_per_thread; ++i) {
            stack.push(static_cast<int>(i));
        }

        auto threads = std::vector<std::thread>{};
        threads.reserve(num_threads);
        auto ready = std::atomic<std::size_t>{0u};
        auto go    = std::atomic<bool>{false};

        // producers
        for (std::size_t t = 0u; t < num_producers; ++t) {
            threads.emplace_back([&stack, &ready, &go, ops_per_thread]() {
                ready.fetch_add(1u, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (std::size_t i = 0u; i < ops_per_thread; ++i) {
                    stack.push(static_cast<int>(i));
                }
            });
        }

        // consumers — pop(T&) avoids make_shared overhead
        for (std::size_t t = 0u; t < num_consumers; ++t) {
            threads.emplace_back([&stack, &ready, &go, ops_per_thread]() {
                ready.fetch_add(1u, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                auto val = int{};
                for (std::size_t i = 0u; i < ops_per_thread; ) {
                    try {
                        stack.pop(val);
                        ++i;
                    } catch (...) {
                        std::this_thread::yield();
                    }
                }
            });
        }

        while (ready.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }

        auto start = Clock::now();
        go.store(true, std::memory_order_release);
        for (auto& th : threads) { th.join(); }
        auto end = Clock::now();

        times.push_back(
            std::chrono::duration<double, std::milli>(end - start).count()
        );
    }

    return median_of(times);
}


// ═════════════════════════════════════════════════════════
//  Benchmark: producer-consumer  (Queue)
// ═════════════════════════════════════════════════════════
//
//  Same pattern but uses try_pop(T&) instead of exceptions.

template<typename Queue>
auto bench_queue_mixed(std::size_t num_threads, std::size_t ops_per_thread) -> double {
    if (num_threads < 2u) { return 0.0; }

    auto num_producers = num_threads / 2u;
    auto num_consumers = num_threads - num_producers;

    auto times = std::vector<double>{};
    times.reserve(NUM_RUNS);

    for (std::size_t run = 0u; run < NUM_RUNS; ++run) {
        auto queue = Queue{};

        for (std::size_t i = 0u; i < ops_per_thread; ++i) {
            queue.push(static_cast<int>(i));
        }

        auto threads = std::vector<std::thread>{};
        threads.reserve(num_threads);
        auto ready = std::atomic<std::size_t>{0u};
        auto go    = std::atomic<bool>{false};

        for (std::size_t t = 0u; t < num_producers; ++t) {
            threads.emplace_back([&queue, &ready, &go, ops_per_thread]() {
                ready.fetch_add(1u, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                for (std::size_t i = 0u; i < ops_per_thread; ++i) {
                    queue.push(static_cast<int>(i));
                }
            });
        }

        for (std::size_t t = 0u; t < num_consumers; ++t) {
            threads.emplace_back([&queue, &ready, &go, ops_per_thread]() {
                ready.fetch_add(1u, std::memory_order_release);
                while (!go.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                auto val = int{};
                for (std::size_t i = 0u; i < ops_per_thread; ) {
                    if (queue.try_pop(val)) {
                        ++i;
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
        }

        while (ready.load(std::memory_order_acquire) < num_threads) {
            std::this_thread::yield();
        }

        auto start = Clock::now();
        go.store(true, std::memory_order_release);
        for (auto& th : threads) { th.join(); }
        auto end = Clock::now();

        times.push_back(
            std::chrono::duration<double, std::milli>(end - start).count()
        );
    }

    return median_of(times);
}


// ═════════════════════════════════════════════════════════
//  Table printer
// ═════════════════════════════════════════════════════════

using BenchFn = std::function<double(std::size_t, std::size_t)>;

auto run_suite(
    const std::string& title,
    const std::vector<std::pair<std::string, BenchFn>>& versions,
    const std::vector<std::size_t>& thread_counts,
    std::size_t ops_per_thread
) -> void {

    constexpr int tcol = 9;
    constexpr int vcol = 18;

    std::cout << "\n"
              << std::string(60u, '=') << "\n"
              << "  " << title << "\n"
              << std::string(60u, '=') << "\n\n";

    // header
    std::cout << std::setw(tcol) << "Threads";
    for (const auto& v : versions) {
        std::cout << " | " << std::setw(vcol) << v.first;
    }
    std::cout << "\n";

    // separator
    std::cout << std::string(static_cast<std::size_t>(tcol), '-');
    for (std::size_t i = 0u; i < versions.size(); ++i) {
        std::cout << "-+-" << std::string(static_cast<std::size_t>(vcol), '-');
    }
    std::cout << "\n";

    // data rows
    for (auto tc : thread_counts) {
        std::cout << std::setw(tcol) << tc;

        for (const auto& v : versions) {
            auto elapsed_ms = v.second(tc, ops_per_thread);
            if (elapsed_ms <= 0.0) {
                std::cout << " | " << std::setw(vcol) << "N/A";
                continue;
            }
            auto total_ops  = static_cast<double>(tc * ops_per_thread);
            auto throughput = total_ops / (elapsed_ms / 1000.0);
            std::cout << " | " << std::setw(vcol) << format_throughput(throughput);
        }

        std::cout << "\n";
        std::cout.flush();  // flush after each row so user sees progress
    }

    std::cout << "\n";
}


// ═════════════════════════════════════════════════════════
//  Main
// ═════════════════════════════════════════════════════════

auto main(int argc, char* argv[]) -> int {

    auto ops = DEFAULT_OPS;
    if (argc > 1) {
        auto parsed = std::strtoul(argv[1], nullptr, 10);
        if (parsed > 0u) {
            ops = static_cast<std::size_t>(parsed);
        }
    }

    auto hwc = static_cast<std::size_t>(std::thread::hardware_concurrency());
    if (hwc == 0u) { hwc = 4u; }

    // build unique, sorted thread counts
    auto tc_set = std::set<std::size_t>{};
    for (auto t : {1u, 2u, 4u, 8u}) {
        tc_set.insert(static_cast<std::size_t>(t));
    }
    tc_set.insert(hwc);
    if (hwc <= 32u) { tc_set.insert(hwc * 2u); }

    auto thread_counts = std::vector<std::size_t>(tc_set.begin(), tc_set.end());

    // mixed benchmarks need at least 2 threads (1 producer + 1 consumer)
    auto mixed_tc = thread_counts;
    std::erase_if(mixed_tc, [](std::size_t t) { return t < 2u; });

    std::cout << "Thread-Safe Data Structure Benchmark\n"
              << std::string(40u, '=') << "\n"
              << "Hardware concurrency : " << hwc << "\n"
              << "Ops per thread       : " << ops << "\n"
              << "Runs per config      : " << NUM_RUNS << " (median)\n";

    // ── Stack ────────────────────────────────────────────

    run_suite("Stack: Push-Only (write contention)", {
        {"TSS v1 (mutex, T)", bench_push_only<TSS<int>>},
        // ── Add new stack versions here ──────────────────
        // {"TSS v2 (...)", bench_push_only<TSS_v2<int>>},
    }, thread_counts, ops);

    run_suite("Stack: Producer-Consumer", {
        {"TSS v1 (mutex, T)", bench_stack_mixed<TSS<int>>},
        // {"TSS v2 (...)", bench_stack_mixed<TSS_v2<int>>},
    }, mixed_tc, ops);

    // ── Queue ────────────────────────────────────────────

    run_suite("Queue: Push-Only (write contention)", {
        {"TSQ v1 (mutex, T)", bench_push_only<TSQ<int>>},
        // ── Add new queue versions here ──────────────────
        {"TSQ v2 (mutex, shared_ptr<T>)", bench_push_only<TSQv2<int>>},
    }, thread_counts, ops);

    run_suite("Queue: Producer-Consumer", {
        {"TSQ v1 (mutex)", bench_queue_mixed<TSQ<int>>},
        {"TSQ v2 (mutex, shared_ptr<T>)", bench_queue_mixed<TSQv2<int>>},
    }, mixed_tc, ops);

    return 0;
}
