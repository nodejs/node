#include <assert.h>
#include <time.h>

int main() {
  struct timespec ts;

  // supported clocks
  assert(clock_getres(CLOCK_REALTIME, &ts) == 0);
  assert(clock_getres(CLOCK_MONOTONIC, &ts) == 0);
  assert(clock_getres(CLOCK_PROCESS_CPUTIME_ID, &ts) == 0);
  assert(clock_getres(CLOCK_THREAD_CPUTIME_ID, &ts) == 0);
}
