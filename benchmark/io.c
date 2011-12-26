/**
 * gcc -o iotest io.c
 */

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
 
static int c = 0;
static int tsize = 1000 * 1048576;
static const char path[] = "/tmp/wt.dat";
static char buf[65536];

static uint64_t now(void) {
  struct timeval tv;

  if (gettimeofday(&tv, NULL))
    abort();

  return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static void writetest(int size, size_t bsize)
{
  int i;
  uint64_t start, end;
  double elapsed;
  double mbps;

  assert(bsize <= sizeof buf);

  int fd = open(path, O_CREAT|O_WRONLY, 0644);
  if (fd < 0) {
    perror("open failed");
    exit(254);
  }

  start = now();

  for (i = 0; i < size; i += bsize) {
    int rv = write(fd, buf, bsize);
    if (c++ % 2000 == 0) fprintf(stderr, ".");
    if (rv < 0) {
      perror("write failed");
      exit(254);
    }
  }

#ifndef NSYNC
# ifdef __linux__
  fdatasync(fd);
# else
  fsync(fd);
# endif
#endif /* SYNC */

  close(fd);

  end = now();
  elapsed = (end - start) / 1e6;
  mbps = ((tsize/elapsed)) / 1048576;

  fprintf(stderr, "\nWrote %d bytes in %03fs using %ld byte buffers: %03fmB/s\n", size, elapsed, bsize, mbps);
}

void readtest(int size, size_t bsize)
{
  int i;
  uint64_t start, end;
  double elapsed;
  double mbps;

  assert(bsize <= sizeof buf);

  int fd = open(path, O_RDONLY, 0644);
  if (fd < 0) {
    perror("open failed");
    exit(254);
  }

  start = now();

  for (i = 0; i < size; i += bsize) {
    int rv = read(fd, buf, bsize);
    if (rv < 0) {
      perror("write failed");
      exit(254);
    }
  }
  close(fd);

  end = now();
  elapsed = (end - start) / 1e6;
  mbps = ((tsize/elapsed)) / 1048576;

  fprintf(stderr, "Read %d bytes in %03fs using %ld byte buffers: %03fmB/s\n", size, elapsed, bsize, mbps);
}

void cleanup() {
  unlink(path);
}

int main()
{
  int i;
  int bsizes[] = {1024, 4096, 8192, 16384, 32768, 65536, 0};

  for (i = 0; bsizes[i] != 0; i++) {
    writetest(tsize, bsizes[i]);
  }
  for (i = 0; bsizes[i] != 0; i++) {
    readtest(tsize, bsizes[i]);
  }
  atexit(cleanup);
  return 0;
}
