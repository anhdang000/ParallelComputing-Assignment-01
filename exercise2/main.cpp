#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <barrier>

class SenseReversingBarrier
{
private:
  std::atomic<int> count;
  std::atomic<bool> sense;
  const int num_threads;
  thread_local static bool my_sense;

  void backoff(int &delay)
  {
    std::this_thread::sleep_for(std::chrono::microseconds(delay));
    delay = std::min(delay * 2, 1000); // Cap at 1ms
  }

public:
  SenseReversingBarrier(int n) : count(n), sense(true), num_threads(n) {}

  void wait()
  {
    // Use relaxed memory order initially for local sense read,
    // acquire semantics will be needed later when checking global sense.
    bool my_sense_local = my_sense;

    // Use acq_rel for fetch_sub: acquire ensures visibility of previous barrier's
    // sense flip, release ensures visibility of this decrement before the potential
    // sense flip by this thread if it's the last one.
    // count = 1 means the last thread to arrive
    if (count.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
      // Reset count. Use relaxed as it's guarded by the sense flip.
      count.store(num_threads, std::memory_order_relaxed);

      // Use release semantics to ensure prior writes (like count reset) are visible
      // before other threads see the flipped sense.
      sense.store(!my_sense_local, std::memory_order_release);
    }
    else
    {
      int delay = 1;
      // Wait for the sense to flip by last thread
      while (sense.load(std::memory_order_acquire) == my_sense_local)
      {
        backoff(delay);
      }
    }

    // Flip local sense for next phase
    my_sense = !my_sense_local;
  }
};

// Initialize thread-local variable
thread_local bool SenseReversingBarrier::my_sense = true;

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

  // Test correctness
  std::cout << "Testing Sense-Reversing Barrier implementation...\n";
  std::cout << "Number of threads: " << num_threads << "\n";
  std::cout << "Number of iterations: " << num_iterations << "\n";
  std::cout << "Test result: " << (testBarrier(num_threads, num_iterations) ? "PASSED" : "FAILED") << "\n\n";

  // Benchmark performance
  std::cout << "Benchmarking barrier implementation...\n";
  std::cout << "Sense-Reversing Barrier time: " << benchmarkMyBarrier(num_threads, num_iterations) << " seconds\n";
  std::cout << "Standard Barrier time: " << benchmarkStdBarrier(num_threads, num_iterations) << " seconds\n";

  return 0;
}