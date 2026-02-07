#ifndef EVENT_QUEUE_HPP
#define EVENT_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <atomic>

template<typename T>
class EventQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{false};

public:
    EventQueue() = default;
    
    // Non-copyable, non-movable
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    // Add event to queue (thread-safe)
    void push(T event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(event));
        }
        cv_.notify_one();
    }

    // Wait and retrieve event (blocks until available or shutdown)
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { 
            return !queue_.empty() || shutdown_.load(); 
        });

        if (shutdown_.load() && queue_.empty()) {
            return std::nullopt;
        }

        T event = std::move(queue_.front());
        queue_.pop();
        return event;
    }

    // Non-blocking try to retrieve
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T event = std::move(queue_.front());
        queue_.pop();
        return event;
    }

    // Signal shutdown - unblocks waiting threads
    void shutdown() {
        shutdown_.store(true);
        cv_.notify_all();
    }

    // Reset shutdown flag (for reuse)
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_.store(false);
        // Clear remaining items
        std::queue<T> empty;
        std::swap(queue_, empty);
    }

    // Check if empty
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    // Get current size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // Check if shutdown was requested
    bool isShutdown() const {
        return shutdown_.load();
    }
};

#endif // EVENT_QUEUE_HPP
