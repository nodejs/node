#include <assert.h>
#include <time.h>

int main() {
  struct timespec ts;
  int r;

  // supported clocks
  r = clock_getres(CLOCK_REALTIME, &ts);
  assert(r == 0);
  r = clock_getres(CLOCK_MONOTONIC, &ts);
  assert(r == 0);
  r = clock_getres(CLOCK_PROCESS_CPUTIME_ID, &ts);
  assert(r == 0);
  r = clock_getres(CLOCK_THREAD_CPUTIME_ID, &ts);
  assert(r == 0);
}
