/**
 * gcc -o iotest io.c
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
 
int tsize = 1000 * 1048576;
const char *path = "/tmp/wt.dat";

int c = 0;

char* bufit(size_t l)
{
  char *p = malloc(l);
  memset(p, '!', l);
  return p;
}

void writetest(int size, size_t bsize)
{
  int i;
  char *buf = bufit(bsize);
  struct timeval start, end;
  double elapsed;
  double mbps;

  int fd = open(path, O_CREAT|O_WRONLY, 0644);
  if (fd < 0) {
    perror("open failed");
    exit(254);
  }

  assert(0 ==  gettimeofday(&start, NULL));
  for (i = 0; i < size; i += bsize) {
    int rv = write(fd, buf, bsize);
    if (c++ % 2000 == 0) fprintf(stderr, ".");
    if (rv < 0) {
      perror("write failed");
      exit(254);
    }
  }
#ifdef __linux__
  fdatasync(fd);
#else
  fsync(fd);
#endif
  close(fd);
  assert(0 == gettimeofday(&end, NULL));
  elapsed = (end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec))/100000.;
  mbps = ((tsize/elapsed)) / 1048576;
  fprintf(stderr, "\nWrote %d bytes in %03fs using %ld byte buffers: %03fmB/s\n", size, elapsed, bsize, mbps);

  free(buf);
}

void readtest(int size, size_t bsize)
{
  int i;
  char *buf = bufit(bsize);
  struct timeval start, end;
  double elapsed;
  double mbps;

  int fd = open(path, O_RDONLY, 0644);
  if (fd < 0) {
    perror("open failed");
    exit(254);
  }

  assert(0 == gettimeofday(&start, NULL));
  for (i = 0; i < size; i += bsize) {
    int rv = read(fd, buf, bsize);
    if (rv < 0) {
      perror("write failed");
      exit(254);
    }
  }
  close(fd);
  assert(0 == gettimeofday(&end, NULL));
  elapsed = (end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec))/100000.;
  mbps = ((tsize/elapsed)) / 1048576;
  fprintf(stderr, "Read %d bytes in %03fs using %ld byte buffers: %03fmB/s\n", size, elapsed, bsize, mbps);

  free(buf);
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
