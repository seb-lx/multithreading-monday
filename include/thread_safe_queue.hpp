#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>


template <typename T>
class TSQ {
private:
    std::queue<T> data_;
    std::condition_variable cv_;
    mutable std::mutex m_; // mutable needed so that const member functions 
                           // such as empty() can still lock mutex

public:

    TSQ(): data_{}, cv_{}, m_{} {}

    TSQ(const TSQ& other) {
        auto lock = std::scoped_lock<std::mutex>(other.m_);

        data_ = other.data_;
    }

    auto push(T new_value) -> void {
        auto lock = std::scoped_lock<std::mutex>(m_);

        data_.push(std::move(new_value));
        cv_.notify_one();
    }

    auto wait_and_pop(T& result) -> void {
        // need unique_lock for dynamic locking and unlocking
        auto lock = std::unique_lock<std::mutex>(m_);
        
        // this wait call will wait until predicate is true
        // i.e. until data queue is not empty anymore and
        // an item can be popped
        cv_.wait(lock, [this](){ return !data_.empty(); });

        result = data_.front();
        data_.pop();
    }

    auto wait_and_pop() -> std::shared_ptr<T> {
        auto lock = std::unique_lock<std::mutex>(m_);
        
        cv_.wait(lock, [this](){ return !data_.empty(); });

        auto result = std::make_shared<T>(data_.front());
        data_.pop();

        return result;
    }

    auto try_pop(T& result) -> bool {
        auto lock = std::scoped_lock<std::mutex>(m_);

        if (data_.empty()) return false;

        result = data_.front();
        data_.pop();

        return true;
    }

    auto try_pop() -> std::shared_ptr<T> {
        auto lock = std::scoped_lock<std::mutex>(m_);

        if (data_.empty()) return std::shared_ptr<T>{};

        auto result = std::make_shared<T>(data_.front());
        data_.pop();

        return result;
    }

    auto empty() const -> bool {
        auto lock = std::scoped_lock<std::mutex>(m_);

        return data_.empty();
    }

};
