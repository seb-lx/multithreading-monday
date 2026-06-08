#include <mutex>
#include <memory>
#include <stack>


// Thread-safe stack, wrapper of std::stack
//
// basic thread safety ensured by protection access to shared data with mutex
// thus coarse lock granularity
//
// combines top() and pop() operations to avoid race conditions
//
// bad performance since access is serialized, only one thread can access
// shared data at any given time
//
template<typename T>
class TSS {
private:

    // TSS as wrapper around std::stack
    std::stack<T> data_;
    mutable std::mutex m_;

public:

    TSS(): data_{}, m_{} {}

    TSS(const TSS& other) {
        auto lock = std::scoped_lock<std::mutex>(other.m_);

        data_ = other.data_;
    }

    TSS& operator=(const TSS& other) = delete;

    auto push(T new_value) -> void {
        auto lock = std::scoped_lock<std::mutex>(m_);

        data_.push(std::move(new_value));
    }

    // combined top() and pop() operation to avoid data race
    auto pop() -> std::shared_ptr<T> {
        auto lock = std::scoped_lock<std::mutex>(m_);

        if (data_.empty()) throw std::runtime_error("empty stack");

        const auto result = std::make_shared<T>(data_.top());
        data_.pop();
        return result;
    }

    auto pop(T& result) -> void {
        auto lock = std::scoped_lock<std::mutex>(m_);

        if (data_.empty()) throw std::runtime_error("empty stack"); 
        result = data_.top();
        data_.pop();
    }

    auto empty() const -> bool {
        auto lock = std::scoped_lock<std::mutex>(m_);

        return data_.empty();
    }

};
