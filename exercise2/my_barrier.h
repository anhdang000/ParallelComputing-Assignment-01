#ifndef BARRIER_H
#define BARRIER_H

#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>

class SenseReversingBarrier
{
private:
  std::atomic<int> count;
  std::atomic<bool> sense;
  const int num_threads;
  thread_local static bool my_sense;

  void backoff(int &delay);

public:
  SenseReversingBarrier(int n);
  void wait();
};

#endif