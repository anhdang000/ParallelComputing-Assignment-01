#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <barrier>
#include "my_barrier.h"
// Benchmark function for our barrier
double benchmarkMyBarrier(int num_threads, int num_iterations)
{
  std::vector<std::thread> threads;
  std::atomic<bool> start{false};
  SenseReversingBarrier barrier(num_threads);

  auto start_time = std::chrono::high_resolution_clock::now();

  // Create threads
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&barrier, num_iterations, &start]()
                         {
            while (!start) {
                std::this_thread::yield();
            }
            for (int j = 0; j < num_iterations; ++j) {
                barrier.wait();
            } });
  }

  start = true;
  for (auto &thread : threads)
  {
    thread.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

  return duration.count() / 1000000.0; // Convert to seconds
}

double benchmarkStdBarrier(int num_threads, int num_iterations)
{

  std::vector<std::thread> threads;
  std::atomic<bool> start{false};

  // --- Create the std::barrier ---
  // Initialize std::barrier with the number of threads that will participate.
  // The default completion function (std::noop_completion) is implicitly used.
  try
  {
    // Need at least one thread for std::barrier
    if (num_threads <= 0)
    {
      std::cerr << "Error: Number of threads must be positive for std::barrier." << std::endl;
      return -2.0; // Indicate invalid argument
    }
    std::barrier sync_point{num_threads};

    // --- Start Timing ---
    auto start_time = std::chrono::high_resolution_clock::now();

    // --- Create Threads ---
    for (int i = 0; i < num_threads; ++i)
    {
      // Capture the barrier object by reference [&sync_point]
      threads.emplace_back([&sync_point, num_iterations, &start]()
                           {
                // Wait for the start signal (using acquire for visibility)
                while (!start.load(std::memory_order_acquire)) {
                    std::this_thread::yield(); // Yield CPU to avoid busy-waiting too hard
                }

                // --- Barrier Synchronization Loop ---
                for (int j = 0; j < num_iterations; ++j) {
                    // Arrive at the barrier and wait for all other threads.
                    // This is the core operation being benchmarked.
                    sync_point.arrive_and_wait();
                } });
    }

    // --- Signal Threads to Start ---
    // Use release semantics to ensure writes are visible to the acquire loads.
    start.store(true, std::memory_order_release);

    for (auto &thread : threads)
    {
      thread.join();
    }

    // --- Stop Timing ---
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    return duration.count() / 1000000.0;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Exception during std::barrier benchmark: " << e.what() << std::endl;
    return -3.0;
  }
}

int main()
{
  const int num_threads = 4;
  const int num_iterations = 1000000;

  // Benchmark performance
  std::cout << "Benchmarking barrier implementation...\n";
  std::cout << "Sense-Reversing Barrier time: " << benchmarkMyBarrier(num_threads, num_iterations) << " seconds\n";
  std::cout << "Standard Barrier time: " << benchmarkStdBarrier(num_threads, num_iterations) << " seconds\n";

  return 0;
}