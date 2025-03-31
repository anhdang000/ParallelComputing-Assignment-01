#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "lock.h"

#define NUM_OF_THREADS 4
#define NUM_OF_ITERS   10000000
#define OUTSIDE_WORK   10
#define N              10000000

std::atomic<int> counter{0};

int is_prime(int p) {
    if (p <= 1) return 0;
    for (int d = 2; d < p; d++) {
        if (p % d == 0) return 0;
    }
    return 1;
}

template <typename LockType>
void thread_entry(LockType& lock) {
    for (int i = 0; i < NUM_OF_ITERS; ++i) {
        lock.acquire();
        is_prime(N);
        counter.fetch_add(1, std::memory_order_relaxed);
        lock.release();

        // Non-critical section work
        for (int j = 0; j < OUTSIDE_WORK; ++j) {
            is_prime(N);
        }
    }
}

template <typename LockType>
double run_benchmark() {
    LockType lock;
    counter = 0;

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
    double tas_time = run_benchmark<TASLock>();
    std::cout << "Outside work: " << OUTSIDE_WORK << std::endl;

    // TAS Lock
    std::cout << "TAS Lock - Total time: " << tas_time << " seconds" << std::endl;
    int expected = NUM_OF_THREADS * NUM_OF_ITERS;
    if (counter == expected) {
        std::cout << "TAS Lock - Counter correct: " << counter << std::endl;
    } else {
        std::cout << "TAS Lock - Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    // TTAS Lock
    double ttas_time = run_benchmark<TTASLock>();
    std::cout << "TTAS Lock - Total time: " << ttas_time << " seconds" << std::endl;
    if (counter == expected) {
        std::cout << "TTAS Lock - Counter correct: " << counter << std::endl;
    } else {
        std::cout << "TTAS Lock - Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    // MCS Lock
    double mcs_time = run_benchmark<MCSLock>();
    std::cout << "MCS Lock - Total time: " << mcs_time << " seconds" << std::endl;
    if (counter == expected) {
        std::cout << "MCS Lock - Counter correct: " << counter << std::endl;
    } else {
        std::cout << "MCS Lock - Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    return 0;
}