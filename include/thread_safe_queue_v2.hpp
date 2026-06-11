/*
    Based on the book "C++ Concurrency in Action" by Anthony Williams
*/

#pragma once

#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>


template<typename T>
class TSQv2 {
private:
    std::queue<std::shared_ptr<T>> data_; // now stores shared pointer to data
    std::condition_variable cv_;
    mutable std::mutex m_;

public:
    TSQv2(): data_{}, cv_{}, m_{} {}

    auto push(T new_value) -> void {
        // now allocation can be done outside of lock,
        // NOTE: now has heap allocation, which is certainly slower for 
        //       fundamental types like int and should only have benefits
        //       for larger custom types that are expensive to copy (or non-
        //       movable)
        auto new_value_ptr = std::make_shared<T>(std::move(new_value));

        auto lock = std::scoped_lock<std::mutex>(m_);

        data_.push(new_value_ptr);
        cv_.notify_one(); // or notify_all
    } 

    auto wait_and_pop(T& value) -> void {
        auto lock = std::unique_lock<std::mutex>(m_);

        cv_.wait(lock, [this]() -> bool { return !data_.empty(); });
        value = std::move(*data_.front());
        data_.pop();
    }

    auto wait_and_pop() -> std::shared_ptr<T> {
        auto lock = std::unique_lock<std::mutex>(m_);

        cv_.wait(lock, [this]() -> bool { return !data_.empty(); });
        auto result = data_.front();
        data_.pop();

        return result;
    }

    auto try_pop(T& value) -> bool {
        auto lock = std::scoped_lock<std::mutex>(m_);

        if (data_.empty()) return false;

        value = std::move(*data_.front());
        data_.pop();

        return true;
    }

    auto try_pop() -> std::shared_ptr<T> {
        auto lock = std::scoped_lock<std::mutex>(m_);

        if (data_.empty()) return std::shared_ptr<T>();
        auto result = data_.front();
        data_.pop();

        return result;
    }

    auto empty() const -> bool {
        auto lock = std::scoped_lock<std::mutex>(m_);

        return data_.empty();
    }
};
