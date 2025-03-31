#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <cassert>
#include "lock.h"


class Counter {
private:
    int value = 0;

public:
    void increment() {
        value++;
    }

    int get() const {
        return value;
    }
};

template<typename LockType>
void test_correctness(LockType& lock, int num_threads, int iterations_per_thread) {
    Counter counter;
    std::vector<std::thread> threads;

    auto worker = [&](int id) {
        for (int i = 0; i < iterations_per_thread; i++) {
            if constexpr (std::is_same_v<LockType, MCSLock>) {
                MCSLock::Node node;
                lock.lock(node);
                counter.increment();
                lock.unlock(node);
            } else {
                lock.lock();
                counter.increment();
                lock.unlock();
            }
        }
    };

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // verify correctness
    int expected = num_threads * iterations_per_thread;
    int actual = counter.get();
    
    std::cout << "Correctness test for " 
              << typeid(LockType).name() << ": "
              << (expected == actual ? "PASSED" : "FAILED")
              << " (Expected: " << expected << ", Actual: " << actual << ")"
              << std::endl;

    assert(expected == actual);
}

// Benchmark
template<typename LockType>
double benchmark(LockType& lock, int num_threads, int iterations_per_thread, int critical_section_work) {
    std::vector<std::thread> threads;
    Counter counter;

    auto worker = [&](int id) {
        for (int i = 0; i < iterations_per_thread; i++) {
            if constexpr (std::is_same_v<LockType, MCSLock>) {
                MCSLock::Node node;
                lock.lock(node);
                counter.increment();
                for (volatile int j = 0; j < critical_section_work; j++) {}
                
                lock.unlock(node);
            } else {
                lock.lock();
                
                counter.increment();
                for (volatile int j = 0; j < critical_section_work; j++) {}
                
                lock.unlock();
            }
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    return duration.count();
}

int main() {
    const int NUM_THREADS[] = {1, 2, 4, 8, 16};
    const int ITERATIONS_PER_THREAD = 100000;
    const int CRITICAL_SECTION_WORK = 10;
    
    std::cout << "Testing Spin Lock Implementations" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Test correctness of each lock implementation
    std::cout << "\nCorrectness Tests:" << std::endl;
    std::cout << "-----------------" << std::endl;
    
    {
        TASLock tas_lock;
        test_correctness(tas_lock, 4, 10000);
    }
    
    {
        TTASLock ttas_lock;
        test_correctness(ttas_lock, 4, 10000);
    }
    
    {
        MCSLock mcs_lock;
        test_correctness(mcs_lock, 4, 10000);
    }
    
    // Performance benchmarks
    std::cout << "\nPerformance Benchmarks (milliseconds):" << std::endl;
    std::cout << "------------------------------------" << std::endl;
    
    // Print header
    std::cout << std::setw(10) << "Threads";
    std::cout << std::setw(15) << "TAS";
    std::cout << std::setw(15) << "TTAS";
    std::cout << std::setw(15) << "MCS" << std::endl;
    
    // Run benchmarks for different thread counts
    for (int num_threads : NUM_THREADS) {
        TASLock tas_lock;
        TTASLock ttas_lock;
        MCSLock mcs_lock;
        
        double tas_time = benchmark(tas_lock, num_threads, ITERATIONS_PER_THREAD, CRITICAL_SECTION_WORK);
        double ttas_time = benchmark(ttas_lock, num_threads, ITERATIONS_PER_THREAD, CRITICAL_SECTION_WORK);
        double mcs_time = benchmark(mcs_lock, num_threads, ITERATIONS_PER_THREAD, CRITICAL_SECTION_WORK);
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(10) << num_threads;
        std::cout << std::setw(15) << tas_time;
        std::cout << std::setw(15) << ttas_time;
        std::cout << std::setw(15) << mcs_time << std::endl;
    }
    return 0;
}