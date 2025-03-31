#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "lock.h"  // Includes TASLock, TTASLock, MCSLock

// Configuration parameters
#define NUM_OF_THREADS 4    // Number of concurrent threads
#define NUM_OF_ITERS   1000 // Iterations per thread
#define OUTSIDE_WORK   0    // Controls contention: 0 = all work in critical section
#define N              1000 // Number for primality test

// Shared variable
std::atomic<int> counter{0};  // Counts critical section executions

// Primality test function (work simulation)
int is_prime(int p) {
    if (p <= 1) return 0;  // Handle non-prime cases
    for (int d = 2; d < p; d++) {
        if (p % d == 0) return 0;
    }
    return 1;
}

// Thread function
template <typename LockType>
void thread_entry(LockType& lock) {
    for (int i = 0; i < NUM_OF_ITERS; ++i) {
        lock.acquire();     // Enter critical section
        is_prime(N);        // Simulate work
        counter.fetch_add(1, std::memory_order_relaxed);  // Update shared counter
        lock.release();     // Exit critical section

        // Non-critical section work
        for (int j = 0; j < OUTSIDE_WORK; ++j) {
            is_prime(N);
        }
    }
}

// Function to run the benchmark for a specific lock type
template <typename LockType>
double run_benchmark() {
    LockType lock;
    counter = 0;  // Reset counter

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OF_THREADS; ++i) {
        threads.emplace_back(thread_entry<LockType>, std::ref(lock));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    return elapsed.count();
}

int main() {
    // Run benchmark for TAS Lock
    double tas_time = run_benchmark<TASLock>();
    std::cout << "TAS Lock - Total time: " << tas_time << " seconds" << std::endl;
    int expected = NUM_OF_THREADS * NUM_OF_ITERS;
    if (counter == expected) {
        std::cout << "TAS Lock - Counter correct: " << counter << std::endl;
    } else {
        std::cout << "TAS Lock - Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    // Run benchmark for TTAS Lock
    double ttas_time = run_benchmark<TTASLock>();
    std::cout << "TTAS Lock - Total time: " << ttas_time << " seconds" << std::endl;
    if (counter == expected) {
        std::cout << "TTAS Lock - Counter correct: " << counter << std::endl;
    } else {
        std::cout << "TTAS Lock - Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    // Run benchmark for MCS Lock
    double mcs_time = run_benchmark<MCSLock>();
    std::cout << "MCS Lock - Total time: " << mcs_time << " seconds" << std::endl;
    if (counter == expected) {
        std::cout << "MCS Lock - Counter correct: " << counter << std::endl;
    } else {
        std::cout << "MCS Lock - Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    return 0;
}