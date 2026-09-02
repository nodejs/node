#include <assert.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

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
  char* platform;
  int is_aix_or_os400;
  int is_win;

  platform = getenv("NODE_PLATFORM");
  is_aix_or_os400 = platform != NULL && (0 == strcmp(platform, "aix") || 0 == strcmp(platform, "os400"));
  is_win = platform != NULL && 0 == strcmp(platform, "win32");

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

  // V8 has a bug that allows unsupported parts of this test to run,
  // causing the test to fail. poll_win.c is a workaround.
  // https://github.com/nodejs/node/issues/51822
  // The rest of the test is unsupported on Windows.
  if (is_win)
    return 0;

  fds[0] = (struct pollfd){.fd = 1, .events = POLLOUT, .revents = 0};
  fds[1] = (struct pollfd){.fd = 2, .events = POLLOUT, .revents = 0};

  ret = poll(fds, 2, -1);
  assert(ret == 2);
  assert(fds[0].revents == POLLOUT);
  assert(fds[1].revents == POLLOUT);

  // Make a poll() call with duplicate file descriptors.
  fds[0] = (struct pollfd){.fd = 1, .events = POLLOUT, .revents = 0};
  fds[1] = (struct pollfd){.fd = 2, .events = POLLOUT, .revents = 0};
  fds[2] = (struct pollfd){.fd = 1, .events = POLLOUT, .revents = 0};
  fds[3] = (struct pollfd){.fd = 1, .events = POLLIN, .revents = 0};

  ret = poll(fds, 2, -1);
  assert(ret == 2);
  assert(fds[0].revents == POLLOUT);
  assert(fds[1].revents == POLLOUT);
  assert(fds[2].revents == 0);
  assert(fds[3].revents == 0);

  // The original version of this test expected a timeout and return value of
  // zero. In the Node test suite, STDIN is not a TTY, and poll() returns one,
  // with revents = POLLHUP | POLLIN, except on AIX whose poll() does not
  // support POLLHUP.
  fds[0] = (struct pollfd){.fd = 0, .events = POLLIN, .revents = 0};
  ret = poll(fds, 1, 2000);
  assert(ret == 1);

  if (is_aix_or_os400)
    assert(fds[0].revents == POLLIN);
  else
    assert(fds[0].revents == (POLLHUP | POLLIN));

  return 0;
}
