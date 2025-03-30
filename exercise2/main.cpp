#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

class SenseReversingBarrier
{
private:
  std::atomic<int> count;
  int num_threads;
  std::atomic<bool> sense;

public:
  SenseReversingBarrier(int num_threads) : num_threads(num_threads), count(0), sense(false) {}

  void wait(bool &local_sense)
  {
    local_sense = !local_sense;
    if (count.fetch_add(1) == num_threads - 1)
    {
      count.store(0);
      sense.store(local_sense, std::memory_order_release);
    }
    else
    {
      while (sense.load(std::memory_order_acquire) != local_sense)
        ;
    }
  }
};

void thread_function(SenseReversingBarrier &barrier, int thread_id, int iterations)
{
  bool local_sense = false;
  for (int i = 0; i < iterations; ++i)
  {
    barrier.wait(local_sense);
    if (thread_id == 0 && i % 100 == 0)
    {
      std::cout << "Barrier passed at iteration " << i << "\n";
    }
  }
}

int main()
{
  const int num_threads = std::thread::hardware_concurrency();
  const int iterations = 1000;
  std::vector<std::thread> threads;
  SenseReversingBarrier barrier(num_threads);

  auto start_time = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back(thread_function, std::ref(barrier), i, iterations);
  }

  for (auto &t : threads)
  {
    t.join();
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end_time - start_time;

  std::cout << "Execution Time: " << duration.count() << " seconds\n";
  return 0;
}
