/**
 * gcc -o iotest io.c
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
 
int tsize = 1000 * 1048576;
const char *path = "/tmp/wt.dat";


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
  clock_t start, end;
  double elapsed;
  double mbps;

  int fd = open(path, O_CREAT|O_WRONLY, 0644);
  if (fd < 0) {
    perror("open failed");
    exit(254);
  }

  start = clock();
  for (i = 0; i < size; i += bsize) {
    int rv = write(fd, buf, bsize);
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
  end = clock();
  elapsed = ((double) (end - start)) / CLOCKS_PER_SEC;
  mbps = ((tsize/elapsed)) / 1048576;
  fprintf(stderr, "Wrote %d bytes in %03fs using %d byte buffers: %03fmB/s\n", size, elapsed, bsize, mbps);

  free(buf);
}

void readtest(int size, size_t bsize)
{
  int i;
  char *buf = bufit(bsize);
  clock_t start, end;
  double elapsed;
  double mbps;

  int fd = open(path, O_RDONLY, 0644);
  if (fd < 0) {
    perror("open failed");
    exit(254);
  }

  start = clock();
  for (i = 0; i < size; i += bsize) {
    int rv = read(fd, buf, bsize);
    if (rv < 0) {
      perror("write failed");
      exit(254);
    }
  }
  close(fd);
  end = clock();
  elapsed = ((double) (end - start)) / CLOCKS_PER_SEC;
  mbps = ((tsize/elapsed)) / 1048576;
  fprintf(stderr, "Read %d bytes in %03fs using %d byte buffers: %03fmB/s\n", size, elapsed, bsize, mbps);

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
