#include "my_barrier.h"

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
  // acquire semantics will be needed later when checking global sense.
  bool my_sense_local = my_sense;

  // count = 1 means the last thread to arrive
  if (count.fetch_sub(1) == 1)
  {
    // Reset count
    count.store(num_threads);

    // before other threads see the flipped sense.
    sense.store(!my_sense_local);
  }
  else
  {
    int delay = 1;
    // Wait for the sense to flip by last thread
    while (sense.load() == my_sense_local)
    {
      backoff(delay);
    }
  }

  // Flip local sense for next phase
  my_sense = !my_sense_local;
}
