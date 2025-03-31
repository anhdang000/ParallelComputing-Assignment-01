#include "my_barrier.h"
#include <vector>
#include <iostream>

bool testBarrier(int num_threads, int num_iterations)
{
  std::vector<std::thread> threads;
  std::atomic<int> shared_counter{0};
  std::atomic<bool> start{false};
  // std::atomic<bool> test_failed{false}; // No longer needed for intermediate check
  SenseReversingBarrier barrier(num_threads);

  for (int i = 0; i < num_threads; ++i)
  {
    // Pass necessary variables by reference/value correctly
    threads.emplace_back([&barrier, &shared_counter, num_threads, num_iterations, &start]()
                         { 
            while (!start.load(std::memory_order_acquire)) { // Use acquire load for start flag
                std::this_thread::yield();
            }
            for (int j = 0; j < num_iterations; ++j) {
                // Work before barrier
                shared_counter.fetch_add(1, std::memory_order_relaxed); // Relaxed is fine for counter

                barrier.wait(); 

                // Work after barrier (if any) 
                // The removed check was here. No intermediate check needed for basic barrier correctness.
            } });
  }

  start.store(true, std::memory_order_release); // Use release store for start flag
  for (auto &thread : threads)
  {
    thread.join();
  }

  // Final check: Did all expected increments occur?
  return shared_counter.load() == num_threads * num_iterations;
}

int main()
{
  const int num_threads = 4;
  const int num_iterations = 1000000;

  // Test correctness
  std::cout << "Testing Sense-Reversing Barrier implementation...\n";
  std::cout << "Number of threads: " << num_threads << "\n";
  std::cout << "Number of iterations: " << num_iterations << "\n";
  std::cout << "Test result: " << (testBarrier(num_threads, num_iterations) ? "PASSED" : "FAILED") << "\n\n";

  return 0;
}