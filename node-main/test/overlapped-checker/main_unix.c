#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <unistd.h>

static size_t r(char* buf, size_t buf_size) {
  ssize_t read_count;
  do
    read_count = read(0, buf, buf_size);
  while (read_count < 0 && errno == EINTR);
  if (read_count <= 0)
    abort();
  return (size_t)read_count;
}

static void w(const char* buf, size_t count) {
  const char* end = buf + count;

  while (buf < end) {
    ssize_t write_count;
    do
      write_count = write(1, buf, count);
    while (write_count < 0 && errno == EINTR);
    if (write_count <= 0)
      abort();
    buf += write_count;
  }

  fprintf(stderr, "%zu", count);
  fflush(stderr);
}

int main(void) {
  w("0", 1);

  while (1) {
    char buf[256];
    size_t read_count = r(buf, sizeof(buf));
    // The JS part (test-child-process-stdio-overlapped.js) only writes the
    // "exit" string when the buffer is empty, so the read is guaranteed to be
    // atomic due to it being less than PIPE_BUF.
    if (!strncmp(buf, "exit", read_count)) {
      break;
    }
    w(buf, read_count);
  }

  return 0;
}
