#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>
#include <algorithm>

using namespace std;

// Base counter interface
class Counter
{
public:
  virtual void increment() = 0;
  virtual int64_t get() const = 0;
  virtual ~Counter() {}
};

// Mutex-based counter
class MutexCounter : public Counter
{
private:
  int64_t value;
  mutable std::mutex mtx;

public:
  MutexCounter() : value(0) {}

  void increment()
  {
    // lock_guard is = mutex.lock() and mutex.unlock() when it goes out of scope
    // std::lock_guard<std::mutex> lock(mtx);
    mtx.lock();
    value++;
    mtx.unlock();
  }

  int64_t get() const
  {
    std::lock_guard<std::mutex> lock(mtx);
    return value;
  }
};

// CAS-based counter with exponential backoff
class CompareSwapCounter : public Counter
{
private:
  std::atomic<int64_t> value{0};

  void backoff(int &delay)
  {
    std::this_thread::sleep_for(std::chrono::microseconds(delay));
    delay = std::min(delay * 2, 1000); // Cap at 1ms
  }

public:
  void increment() override
  {
    int delay = 1;
    while (true)
    {
      int64_t current = value.load();
      if (value.compare_exchange_strong(current, current + 1))
      {
        break;
      }
      backoff(delay);
    }
  }

  int64_t get() const override
  {
    return value.load();
  }
};

// Fetch-and-add based counter
class FetchAddCounter : public Counter
{
private:
  std::atomic<int64_t> value{0};

public:
  void increment() override
  {
    value.fetch_add(1);
  }

  int64_t get() const override
  {
    return value.load();
  }
};

// Test function to verify counter correctness
bool testCounter(Counter &counter, int num_threads, int operations_per_thread)
{
  std::vector<std::thread> threads;
  std::atomic<bool> start{false};

  // Create threads
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&counter, operations_per_thread, &start]()
                         {
      while (!start) {
          std::this_thread::yield();
      }
      for (int j = 0; j < operations_per_thread; ++j) {
          counter.increment();
      } });
  }

  // Start all threads simultaneously
  start = true;

  // Wait for all threads to complete
  for (auto &thread : threads)
  {
    thread.join();
  }

  // Verify final count
  return counter.get() == num_threads * operations_per_thread;
}

// Benchmark function
double benchmarkCounter(Counter &counter, int num_threads, int operations_per_thread)
{
  std::vector<std::thread> threads;
  std::atomic<bool> start{false};

  auto start_time = std::chrono::high_resolution_clock::now();

  // Create threads
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&counter, operations_per_thread, &start]()
                         {
            while (!start) {
                std::this_thread::yield();
            }
            for (int j = 0; j < operations_per_thread; ++j) {
                counter.increment();
            } });
  }

  // Start all threads simultaneously
  start = true;

  // Wait for all threads to complete
  for (auto &thread : threads)
  {
    thread.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

  return duration.count() / 1000000.0; // Convert to seconds
}

int main()
{
  const int num_threads = 4;
  const int operations_per_thread = 1000000;

  // Create counter instances
  MutexCounter mutex_counter;
  CompareSwapCounter compare_swap_counter;
  FetchAddCounter fetch_add_counter;

  // Test correctness
  std::cout << "Testing counter implementations...\n";
  std::cout << "Mutex Counter: " << (testCounter(mutex_counter, num_threads, operations_per_thread) ? "PASSED" : "FAILED") << "\n";
  std::cout << "Compare-Swap Counter: " << (testCounter(compare_swap_counter, num_threads, operations_per_thread) ? "PASSED" : "FAILED") << "\n";
  std::cout << "Fetch-Add Counter: " << (testCounter(fetch_add_counter, num_threads, operations_per_thread) ? "PASSED" : "FAILED") << "\n\n";

  // Benchmark performance
  std::cout << "Benchmarking counter implementations...\n";
  std::cout << "Number of threads: " << num_threads << "\n";
  std::cout << "Operations per thread: " << operations_per_thread << "\n\n";

  std::cout << "Mutex Counter time: " << benchmarkCounter(mutex_counter, num_threads, operations_per_thread) << " seconds\n";
  std::cout << "Compare-Swap Counter time: " << benchmarkCounter(compare_swap_counter, num_threads, operations_per_thread) << " seconds\n";
  std::cout << "Fetch-Add Counter time: " << benchmarkCounter(fetch_add_counter, num_threads, operations_per_thread) << " seconds\n";

  return 0;
}
