#ifndef LOCK_H
#define LOCK_H

#include <atomic>
#include <thread>

class TASLock {
private:
    std::atomic<bool> locked{false};

public:
    void lock() {
        while (locked.exchange(true, std::memory_order_acquire)) {
            // spin
        }
    }

    void unlock() {
        locked.store(false, std::memory_order_release);
    }

    void acquire() { lock(); }
    void release() { unlock(); }
};

class TTASLock {
private:
    std::atomic<bool> locked{false};

public:
    void lock() {
        while (true) {
            while (locked.load(std::memory_order_relaxed)) {
                // wait
            }

            // test-and-set
            if (!locked.exchange(true, std::memory_order_acquire)) {
                return;
            }
        }
    }

    void unlock() {
        locked.store(false, std::memory_order_release);
    }

    void acquire() { lock(); }
    void release() { unlock(); }
};

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
    static thread_local Node threadLocalNode;

    void lock(Node& myNode) {
        myNode.next.store(nullptr, std::memory_order_relaxed);
        myNode.locked.store(true, std::memory_order_relaxed);

        Node* predecessor = tail.exchange(&myNode, std::memory_order_acq_rel);

        if (predecessor != nullptr) {
            predecessor->next.store(&myNode, std::memory_order_release);
            
            while (myNode.locked.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    }

    void unlock(Node& myNode) {
        Node* next = myNode.next.load(std::memory_order_acquire);
        
        if (next == nullptr) {
            Node* expected = &myNode;
            if (tail.compare_exchange_strong(expected, nullptr, std::memory_order_release)) {
                return;
            }
            
            // wait for next node
            do {
                std::this_thread::yield();
                next = myNode.next.load(std::memory_order_acquire);
            } while (next == nullptr);
        }
        
        next->locked.store(false, std::memory_order_release);
    }

    void acquire() {
        lock(threadLocalNode);
    }

    void release() {
        unlock(threadLocalNode);
    }
};

inline thread_local MCSLock::Node MCSLock::threadLocalNode;

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

#endif