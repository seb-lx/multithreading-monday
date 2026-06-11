#pragma once

#include <array>

// A "heavy" payload for benchmarking thread-safe containers.
//
// Key property: move == copy, because std::array stores data inline
// (no heap pointer to swap). Pushing this into a container always
// copies the full 1KB payload.
//
// This is the scenario where storing shared_ptr<T> (TSQv2) should
// outperform storing T directly (TSQ v1): the lock hold time for v1
// includes copying 1KB, while v2 only copies a 16-byte shared_ptr
// inside the lock (the 1KB allocation happens outside the lock).

struct HeavyPayload {
    int id;
    std::array<char, 1024> data;  // ~1 KB, move == copy

    explicit HeavyPayload(int val = 0) : id{val}, data{} {
        data.fill(static_cast<char>(val & 0xFF));
    }

    // needed for std::set in stress tests
    auto operator<(const HeavyPayload& other) const -> bool {
        return id < other.id;
    }
};
