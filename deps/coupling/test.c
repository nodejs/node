#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include "coupling.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define CHUNKSIZE 44231

int
main (int argc, char *argv[])
{
  struct coupling *c = coupling_new_push(STDERR_FILENO);
  int stderr_fd = coupling_nonblocking_fd(c);

  int size;
  if (argc == 2) {
    size = atoi(argv[1]);
    printf("size = %d\n", size);
  } else {
    printf("usage: ./test 123 2> stderr; wc -c stderr\n");
    exit(1);
  }

  char *msg = malloc(CHUNKSIZE);
  int i, r;
  for (i = 0; i < CHUNKSIZE; i++) {
    msg[i] = 'A' + i % 26;
  }

  int written = 0;

  fd_set writefds, exceptfds;

  FD_ZERO(&exceptfds);
  FD_SET(stderr_fd, &exceptfds);

  FD_ZERO(&writefds);
  FD_SET(stderr_fd, &writefds);

  while (written < size) {
    r = select(stderr_fd+1, NULL, &writefds, &exceptfds, NULL);
    if (r < 0) {
      printf("test.c select(): %s\n", strerror(errno));
      exit(1);
    }
    
    if (FD_ISSET(stderr_fd, &exceptfds)) {
      printf("exception on stderr fd\n"); 
      exit(1);
    }
    
    if (FD_ISSET(stderr_fd, &writefds)) {
      r = write(stderr_fd, msg, MIN(size - written, CHUNKSIZE));
      if (r < 0 && errno != EAGAIN) {
        printf("test.c write(): %s\n", strerror(errno));
        exit(1);
      } else {
        written += r;
        printf("%d\n", written);
      }
    }
  }

  close(stderr_fd);
  coupling_join(c);
  coupling_destroy(c);

  return 0;
}
