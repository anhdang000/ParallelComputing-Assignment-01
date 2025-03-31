#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include "lock.h"

volatile int counter = 0;
const int iterations = 100000;

template <typename Lock>
void test_lock(Lock& lock) {
    for (int i = 0; i < iterations; i++) {
        lock.acquire();
        counter++;
        lock.release();
    }
}

void test_std_mutex(std::mutex& mtx) {
    for (int i = 0; i < iterations; i++) {
        std::lock_guard<std::mutex> guard(mtx);
        counter++;
    }
}

int main() {
    const int num_threads = std::thread::hardware_concurrency();
    
    {
        TASLock tasLock;
        counter = 0;
        std::vector<std::thread> threads;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(test_lock<TASLock>, std::ref(tasLock));
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        std::cout << "TASLock - Final counter: " << counter 
                  << ", Time: " << diff.count() << " s\n";
    }

    // Example: Testing TTASLock
    {
        TTASLock ttasLock;
        counter = 0;
        std::vector<std::thread> threads;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(test_lock<TTASLock>, std::ref(ttasLock));
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        std::cout << "TTASLock - Final counter: " << counter 
                  << ", Time: " << diff.count() << " s\n";
    }

    {
        MCSLock mcsLock;
        counter = 0;
        std::vector<std::thread> threads;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([&mcsLock]() {
                for (int j = 0; j < iterations; j++) {
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
        std::cout << "MCSLock - Final counter: " << counter 
                  << ", Time: " << diff.count() << " s\n";
    }

    {
        std::mutex mtx;
        counter = 0;
        std::vector<std::thread> threads;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back(test_std_mutex, std::ref(mtx));
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        std::cout << "std::mutex - Final counter: " << counter 
                  << ", Time: " << diff.count() << " s\n";
    }
    
    return 0;
}
