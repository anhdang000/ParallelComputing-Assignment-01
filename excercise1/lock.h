#ifndef LOCK_H
#define LOCK_H

#include <atomic>
#include <thread>

// ---------------------------------------------------------------
// Spin Lock Implementations
// ---------------------------------------------------------------

// TAS (Test-and-Set) Lock
class TASLock {
private:
    std::atomic<bool> locked{false};

public:
    void lock() {
        while (locked.exchange(true, std::memory_order_acquire)) {
            // Spin until lock is acquired
        }
    }

    void unlock() {
        locked.store(false, std::memory_order_release);
    }

    // Alias methods for compatibility with test_mcs.cpp
    void acquire() { lock(); }
    void release() { unlock(); }
};

// TTAS (Test-and-Test-and-Set) Lock
class TTASLock {
private:
    std::atomic<bool> locked{false};

public:
    void lock() {
        while (true) {
            // First test (read)
            while (locked.load(std::memory_order_relaxed)) {
                // Wait until the lock appears to be free
            }

            // Then try to acquire with test-and-set
            if (!locked.exchange(true, std::memory_order_acquire)) {
                // Successfully acquired the lock
                return;
            }
            // If failed, go back to testing
        }
    }

    void unlock() {
        locked.store(false, std::memory_order_release);
    }

    // Alias methods for compatibility with test_mcs.cpp
    void acquire() { lock(); }
    void release() { unlock(); }
};

// MCS Lock
class MCSLock {
public:
    struct Node {
        std::atomic<Node*> next{nullptr};
        std::atomic<bool> locked{true};
        char padding[64 - sizeof(std::atomic<Node*>) - sizeof(std::atomic<bool>)];
    };

private:
    std::atomic<Node*> tail{nullptr};
    
public:
    // Thread-local node for each thread (for simplified API)
    static thread_local Node threadLocalNode;

    void lock(Node& myNode) {
        // Initialize node
        myNode.next.store(nullptr, std::memory_order_relaxed);
        myNode.locked.store(true, std::memory_order_relaxed);

        // Atomically swap the tail
        Node* predecessor = tail.exchange(&myNode, std::memory_order_acq_rel);

        if (predecessor != nullptr) {
            // If there was a previous tail, we need to wait
            predecessor->next.store(&myNode, std::memory_order_release);
            
            // Spin on our own node's locked flag
            while (myNode.locked.load(std::memory_order_acquire)) {
                std::this_thread::yield(); // Yield to avoid excessive CPU usage
            }
        }
    }

    void unlock(Node& myNode) {
        // Check if there's a next node waiting
        Node* next = myNode.next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            // No successor, try to set tail to null
            Node* expected = &myNode;
            if (tail.compare_exchange_strong(expected, nullptr, std::memory_order_release)) {
                // We were the last one in the queue, nothing more to do
                return;
            }
            
            // Wait for our successor to set our next pointer
            do {
                std::this_thread::yield();
                next = myNode.next.load(std::memory_order_acquire);
            } while (next == nullptr);
        }
        
        // Signal the next thread that it can proceed
        next->locked.store(false, std::memory_order_release);
    }

    // Simplified API compatible with test_mcs.cpp
    void acquire() {
        lock(threadLocalNode);
    }

    void release() {
        unlock(threadLocalNode);
    }
};

// Define the thread-local node (inline in header)
inline thread_local MCSLock::Node MCSLock::threadLocalNode;

// MCS Lock wrapper with RAII support (like std::lock_guard)
class MCSLockGuard {
private:
    MCSLock& lock;
    MCSLock::Node node;

public:
    explicit MCSLockGuard(MCSLock& lock) : lock(lock) {
        lock.lock(node);
    }

    ~MCSLockGuard() {
        lock.unlock(node);
    }
};

#endif // LOCK_H