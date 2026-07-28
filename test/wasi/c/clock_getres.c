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
  // don't run these tests until
  // https://github.com/WebAssembly/wasi-libc/issues/266
  // is resolved
  // r = clock_getres(CLOCK_PROCESS_CPUTIME_ID, &ts);
  // assert(r == 0);
  // r = clock_getres(CLOCK_THREAD_CPUTIME_ID, &ts);
  // assert(r == 0);
}
