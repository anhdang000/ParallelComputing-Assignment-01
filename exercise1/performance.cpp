#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <atomic>
#include "lock.h"

// For the benchmark
const int DEFAULT_ITERATIONS = 1000000;
const int MAX_DELAY = 1024; // for backoff
const int INITIAL_DELAY = 1;

// Counter
volatile int counter = 0;
int iterations = DEFAULT_ITERATIONS;

// TASLock with backoff
class TASLockWithBackoff {
private:
    std::atomic<bool> locked{false};

public:
    void acquire() {
        unsigned delay = INITIAL_DELAY;
        while (locked.exchange(true, std::memory_order_acquire)) {
            // Wait a bit before trying again
            std::this_thread::sleep_for(std::chrono::nanoseconds(delay));
            if (delay < MAX_DELAY) {
                delay *= 2;
            }
        }
    }

    void release() {
        locked.store(false, std::memory_order_release);
    }
};

// TTASLock with backoff
class TTASLockWithBackoff {
private:
    std::atomic<bool> locked{false};

public:
    void acquire() {
        unsigned delay = INITIAL_DELAY;
        while (true) {
            while (locked.load(std::memory_order_relaxed)) {
                // Wait
            }
            
            if (!locked.exchange(true, std::memory_order_acquire)) {
                return;
            }
            
            // Wait a bit before trying again
            std::this_thread::sleep_for(std::chrono::nanoseconds(delay));
            if (delay < MAX_DELAY) {
                delay *= 2;
            }
        }
    }

    void release() {
        locked.store(false, std::memory_order_release);
    }
};

// Worker thread function
template <typename Lock>
void test_lock(Lock& lock, int iterations) {
    for (int i = 0; i < iterations; i++) {
        lock.acquire();
        counter++;
        lock.release();
    }
}

void test_std_mutex(std::mutex& mtx, int iterations) {
    for (int i = 0; i < iterations; i++) {
        std::lock_guard<std::mutex> guard(mtx);
        counter++;
    }
}

// Result structure
struct BenchmarkResult {
    std::string lockName;
    int numThreads;
    int totalOperations;
    double executionTime;
    double operationsPerSecond;
    
    static void printHeader() {
        std::cout << std::left << std::setw(25) << "Lock Type" 
                  << std::setw(15) << "Threads" 
                  << std::setw(15) << "Operations" 
                  << std::setw(15) << "Time (s)" 
                  << std::setw(20) << "Ops/second" 
                  << std::endl;
        std::cout << std::string(90, '-') << std::endl;
    }
    
    void print() const {
        std::cout << std::left << std::setw(25) << lockName 
                  << std::setw(15) << numThreads 
                  << std::setw(15) << totalOperations 
                  << std::fixed << std::setprecision(4) << std::setw(15) << executionTime 
                  << std::fixed << std::setprecision(0) << std::setw(20) << operationsPerSecond 
                  << std::endl;
    }
};

// Generic benchmark function
template <typename LockType>
void benchmark(const std::string& lockName, int numThreads, int iterationsPerThread) {
    LockType lock;
    counter = 0;
    std::vector<std::thread> threads;
    
    int totalOperations = numThreads * iterationsPerThread;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Start threads
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(test_lock<LockType>, std::ref(lock), iterationsPerThread);
    }
    
    // Join threads
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    // Results
    BenchmarkResult result;
    result.lockName = lockName;
    result.numThreads = numThreads;
    result.totalOperations = totalOperations;
    result.executionTime = diff.count();
    result.operationsPerSecond = totalOperations / diff.count();
    
    result.print();
    
    // Check if it's correct
    if (counter != totalOperations) {
        std::cout << "ERROR: Counter is " << counter << " but should be " << totalOperations << std::endl;
    }
}

// For std::mutex
void benchmark_std_mutex(int numThreads, int iterationsPerThread) {
    std::mutex mtx;
    counter = 0;
    std::vector<std::thread> threads;
    
    int totalOperations = numThreads * iterationsPerThread;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(test_std_mutex, std::ref(mtx), iterationsPerThread);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    BenchmarkResult result;
    result.lockName = "std::mutex";
    result.numThreads = numThreads;
    result.totalOperations = totalOperations;
    result.executionTime = diff.count();
    result.operationsPerSecond = totalOperations / diff.count();
    
    result.print();
    
    if (counter != totalOperations) {
        std::cout << "ERROR: Counter is " << counter << " but should be " << totalOperations << std::endl;
    }
}

// For MCSLock
void benchmark_mcs_lock(int numThreads, int iterationsPerThread) {
    MCSLock mcsLock;
    counter = 0;
    std::vector<std::thread> threads;
    
    int totalOperations = numThreads * iterationsPerThread;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&mcsLock, iterationsPerThread]() {
            for (int j = 0; j < iterationsPerThread; j++) {
                MCSLockGuard guard(mcsLock);
                counter++;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    BenchmarkResult result;
    result.lockName = "MCSLock";
    result.numThreads = numThreads;
    result.totalOperations = totalOperations;
    result.executionTime = diff.count();
    result.operationsPerSecond = totalOperations / diff.count();
    
    result.print();
    
    if (counter != totalOperations) {
        std::cout << "ERROR: Counter is " << counter << " but should be " << totalOperations << std::endl;
    }
}

// Run tests with different thread counts
void run_scalability_test(int maxThreads, int iterationsPerThread) {
    std::cout << "\n=== Testing with " << iterationsPerThread << " iterations per thread ===" << std::endl;
    BenchmarkResult::printHeader();
    
    // Test different thread counts
    for (int numThreads = 1; numThreads <= maxThreads; numThreads *= 2) {
        benchmark<TASLock>("TASLock", numThreads, iterationsPerThread);
        benchmark<TASLockWithBackoff>("TASLock+Backoff", numThreads, iterationsPerThread);
        benchmark<TTASLock>("TTASLock", numThreads, iterationsPerThread);
        benchmark<TTASLockWithBackoff>("TTASLock+Backoff", numThreads, iterationsPerThread);
        benchmark_mcs_lock(numThreads, iterationsPerThread);
        benchmark_std_mutex(numThreads, iterationsPerThread);
        
        // Add a separator 
        if (numThreads * 2 <= maxThreads) {
            std::cout << std::string(90, '-') << std::endl;
        }
    }
}

int main() {
    // Get how many cores we have
    const int max_threads = std::thread::hardware_concurrency();
    std::cout << "My system has " << max_threads << " threads" << std::endl;
    std::cout << "Using " << iterations << " iterations per thread" << std::endl;
    std::cout << "Backoff settings: start=" << INITIAL_DELAY << "ns, max=" << MAX_DELAY << "ns" << std::endl;
    
    // Run tests
    run_scalability_test(max_threads, iterations / max_threads);
    
    return 0;
}
