#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include "lock.h"

// Configuration parameters
#define NUM_OF_THREADS 4
#define NUM_OF_ITERS   1000
#define OUTSIDE_WORK   0
#define N              1000

#if defined(USE_MCS_LOCK)
MCSLock lock;
#define LOCK_ACQUIRE lock.acquire()
#define LOCK_RELEASE lock.release()
#elif defined(USE_TAS_LOCK)
TASLock lock;
#define LOCK_ACQUIRE lock.acquire()
#define LOCK_RELEASE lock.release() 
#elif defined(USE_TTAS_LOCK)
TTASLock lock;
#define LOCK_ACQUIRE lock.acquire()
#define LOCK_RELEASE lock.release()
#elif defined(USE_MUTEX)
std::mutex lock;
#define LOCK_ACQUIRE lock.lock()
#define LOCK_RELEASE lock.unlock()
#endif

// Shared variable
int counter = 0;  // Counts critical section executions

// Primality test function (work simulation)
int is_prime(int p) {
    if (p <= 1) return 0;  // Handle non-prime cases
    for (int d = 2; d < p; d++) {
        if (p % d == 0) return 0;
    }
    return 1;
}

// Thread function
void thread_entry() {
    for (int i = 0; i < NUM_OF_ITERS; ++i) {
        LOCK_ACQUIRE;       // Enter critical section
        is_prime(N);        // Simulate work
        counter++;          // Update shared counter
        LOCK_RELEASE;       // Exit critical section

        // Non-critical section work
        for (int j = 0; j < OUTSIDE_WORK; ++j) {
            is_prime(N);
        }
    }
}

int main() {
    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Create threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_OF_THREADS; ++i) {
        threads.emplace_back(thread_entry);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Stop timing
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Output results
    std::cout << "Total time: " << elapsed.count() << " seconds" << std::endl;
    int expected = NUM_OF_THREADS * NUM_OF_ITERS;
    if (counter == expected) {
        std::cout << "Counter correct: " << counter << std::endl;
    } else {
        std::cout << "Counter incorrect: " << counter << " (expected " << expected << ")" << std::endl;
    }

    return 0;
}