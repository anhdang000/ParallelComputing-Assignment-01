#include "my_barrier.h"

// Initialize thread-local variable
thread_local bool SenseReversingBarrier::my_sense = true;

SenseReversingBarrier::SenseReversingBarrier(int n)
    : count(n), sense(true), num_threads(n) {}

void SenseReversingBarrier::backoff(int &delay)
{
  std::this_thread::sleep_for(std::chrono::microseconds(delay));
  delay = std::min(delay * 2, 1000); // Cap at 1ms
}

void SenseReversingBarrier::wait()
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
