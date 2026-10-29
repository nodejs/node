#include <assert.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define NANOS_PER_SECOND 1000000000LL
// Timer granularity can make a timeout expire slightly early.
#define TIMEOUT_TOLERANCE_NS 100000000LL

static int64_t elapsed_nanoseconds(const struct timespec* before,
                                   const struct timespec* after) {
  return (after->tv_sec - before->tv_sec) * NANOS_PER_SECOND +
         after->tv_nsec - before->tv_nsec;
}

int main(void) {
  struct pollfd fds[4];
  struct timespec before, now;
  int ret;

  // Test sleep() behavior.
  assert(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
  sleep(1);
  assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
  assert(elapsed_nanoseconds(&before, &now) >=
         NANOS_PER_SECOND - TIMEOUT_TOLERANCE_NS);

  // Test poll() timeout behavior.
  fds[0] = (struct pollfd){.fd = -1, .events = 0, .revents = 0};
  assert(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
  ret = poll(fds, 1, 2000);
  assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
  assert(ret == 0);
  assert(elapsed_nanoseconds(&before, &now) >=
         2 * NANOS_PER_SECOND - TIMEOUT_TOLERANCE_NS);

  return 0;
}
