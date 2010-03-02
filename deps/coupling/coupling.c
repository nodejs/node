/* Copyright (c) 2009 Ryan Dahl (ry@tinyclouds.org)
 *
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 */
#include "coupling.h"

#include <sys/types.h> 
#include <sys/uio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <pthread.h>

#ifdef PIPE_BUF
# define BUFSIZE  PIPE_BUF
#else
# define BUFSIZE  4096
#endif

#define MAX(a,b) ((a) > (b) ? (a) : (b))

// ring buffer
typedef struct {
  int head;
  int tail;
  int size;
  char buf[BUFSIZE]; 
} ring_buffer; 

static inline void
ring_buffer_inspect (ring_buffer *ring)
{
  printf("size %5d  head %5d  tail %5d\n", ring->size, ring->head, ring->tail);
}

static inline void
ring_buffer_init (ring_buffer *ring)
{
  ring->head = 0;
  ring->tail = 0;
  ring->size = 0;
}

static inline int
ring_buffer_filled_p (ring_buffer *ring)
{
  assert(BUFSIZE - (long)ring->size >= 0);
  return (BUFSIZE == ring->size);
}

static inline int
ring_buffer_empty_p (ring_buffer *ring)
{
  return 0 == ring->size;
}

static ssize_t
ring_buffer_pull (ring_buffer *ring, int fd)
{
  // DO NOT CALL WHEN FILLED
  assert(!ring_buffer_filled_p(ring));

  struct iovec iov[2];
  int iovcnt = 1;

  // Very tough logic. Can you follow? Barely can I.
  iov[0].iov_base = ring->buf + ring->tail;
  if (ring->tail < ring->head) {
    iov[0].iov_len = ring->head - ring->tail;
  } else {
    iov[0].iov_len = BUFSIZE - ring->tail;
    if (ring->head != 0) {
      iovcnt = 2; 
      iov[1].iov_base = ring->buf;
      iov[1].iov_len = ring->head;
    }
  }

  int r = readv(fd, iov, iovcnt);

  if (r > 0) {
    ring->size += r;
    ring->tail = (ring->tail + r) % BUFSIZE;
  }
  assert(ring->size <= BUFSIZE);

  return r;
}

static ssize_t
ring_buffer_push (ring_buffer *ring, int fd)
{
  // DO NOT CALL WHEN EMPTY
  assert(!ring_buffer_empty_p(ring));

  struct iovec iov[2];
  int iovcnt = 1;

  iov[0].iov_base = ring->buf + ring->head;
  if (ring->head < ring->tail) {
    iov[0].iov_len = ring->tail - ring->head;
  } else {
    iov[0].iov_len = BUFSIZE - ring->head;
    if (ring->tail != 0) {
      iovcnt = 2; 
      iov[1].iov_base = ring->buf;
      iov[1].iov_len = ring->tail;
    }
  }

  int r = writev(fd, iov, iovcnt);

  if (r > 0) {
    ring->size -= r;
    ring->head = (ring->head + r) % BUFSIZE;
  }
  assert(0 <= (long)ring->size);

  return r;
}

/* PULL PUMP
 *
 * This is used to read data from a blocking file descriptor and pump it into
 * a non-blocking pipe (or other non-blocking fd). The algorithm is this:
 *
 *   while (true) {
 *     read(STDIN_FILENO) // blocking
 *
 *     while (!ring.empty) {
 *       write(pipe) // non-blocking
 *       select(pipe, writable) 
 *     }
 *   }
 *
 */
static void
pull_pump (int pullfd, int pushfd)
{
  int r;
  ring_buffer ring;

  fd_set writefds, exceptfds;
  FD_ZERO(&exceptfds);
  FD_ZERO(&writefds);
  FD_SET(pushfd, &exceptfds);
  FD_SET(pushfd, &writefds);

  ring_buffer_init(&ring);

  while (pullfd >= 0) {
    /* Blocking read from STDIN_FILENO */
    r = ring_buffer_pull(&ring, pullfd);

    if (r == 0) {
      /* eof */
      close(pullfd);
      pullfd = -1;
    } else if (r < 0 && errno && errno != EINTR && errno != EAGAIN) {
      /* error */
      perror("pull_pump read()");
      close(pullfd);
      pullfd = -1;
    }

    /* Push all of the data in the ring buffer out. */
    while (!ring_buffer_empty_p(&ring)) {
      /* non-blocking write() to the pipe */
      r = ring_buffer_push(&ring, pushfd);

      if (r < 0 && errno && errno != EAGAIN && errno != EINTR) {
        if (errno == EPIPE) {
          /* This happens if someone closes the other end of the pipe.  This
           * is a normal forced close of STDIN. Hopefully there wasn't data
           * in the ring buffer. Just close both ends and exit.
           */
          close(pushfd);
          close(pullfd);
          pushfd = pullfd = -1;
        } else {
          perror("pull_pump write()");
          close(pushfd);
          close(pullfd);
        }
        return;
      }

      /* Select for writablity on the pipe end. 
       * Very rarely will this stick.
       */
      r = select(pushfd+1, NULL, &writefds, &exceptfds, NULL);

      if (r < 0 || FD_ISSET(pushfd, &exceptfds)) {
        close(pushfd);
        close(pullfd);
        pushfd = pullfd = -1;
        return;
      }
    }
  }
  assert(pullfd < 0);
  assert(ring_buffer_empty_p(&ring));
  close(pushfd);
}

/* PUSH PUMP
 *
 * This is used to push data out to a blocking file descriptor. It pulls
 * data from a non-blocking pipe (pullfd) and pushes to STDOUT_FILENO
 * (pushfd).
 * When the pipe is closed, then the rest of the data is pushed out and then
 * STDOUT_FILENO is closed.
 *
 * The algorithm looks roughly like this:
 *
 *  while (true) {
 *    r = read(pipe) // nonblocking
 *
 *    while (!ring.empty) {
 *      write(STDOUT_FILENO) // blocking
 *    }
 *
 *    select(pipe, readable);
 *  }
 */
static void
push_pump (int pullfd, int pushfd)
{
  int r;
  ring_buffer ring;

  fd_set readfds, exceptfds;
  FD_ZERO(&exceptfds);
  FD_ZERO(&readfds);
  FD_SET(pullfd, &exceptfds);
  FD_SET(pullfd, &readfds);

  ring_buffer_init(&ring);

  /* The pipe is open or there is data left to be pushed out 
   * NOTE: if pushfd (STDOUT_FILENO) ever errors out, then we just exit the
   * loop.
   */
  while (pullfd >= 0 || !ring_buffer_empty_p(&ring)) {

    /* Pull from the non-blocking pipe */
    r = ring_buffer_pull(&ring, pullfd);

    if (r == 0) {
      /* eof */
      close(pullfd);
      pullfd = -1;
    } else if (r < 0 && errno && errno != EINTR && errno != EAGAIN) {
      perror("push_pump read()");
      close(pullfd);
      pullfd = -1;
      return;
    }

    /* Push everything out to STDOUT */
    while (!ring_buffer_empty_p(&ring)) {
      /* Blocking write() to pushfd (STDOUT_FILENO) */
      r = ring_buffer_push(&ring, pushfd);

      /* If there was a problem, just exit the entire function */

      if (r < 0 && errno && errno != EINTR && errno != EAGAIN) {
        close(pushfd);
        close(pullfd);
        pushfd = pullfd = -1;
        return;
      }
    }

    if (pullfd >= 0) {
      /* select for readability on the pullfd */
      r = select(pullfd+1, &readfds, NULL, &exceptfds, NULL);

      if (r < 0 || FD_ISSET(pullfd, &exceptfds)) {
        close(pushfd);
        close(pullfd);
        pushfd = pullfd = -1;
        return;
      }
    }
  }
  /* If we got here then we got eof on pullfd and pushed all the data out.
   * so now just close pushfd */
  assert(pullfd < 0);
  assert(ring_buffer_empty_p(&ring));
  close(pushfd);
}

static inline int
set_nonblock (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;

  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r == -1) return -1;

  return 0;
}

struct coupling {
  int is_pull;
  int pullfd;
  int pushfd;
  int exposedfd;
  pthread_t tid;
};

static void *
pump_thread (void *data)
{
  struct coupling *c = (struct coupling*)data;

  if (c->is_pull) {
    pull_pump(c->pullfd, c->pushfd);
  }  else {
    push_pump(c->pullfd, c->pushfd);
  }

  return NULL;
}

static struct coupling*
create_coupling (int fd, int is_pull)
{
  int pipefd[2];

  struct coupling *c = malloc(sizeof(struct coupling));
  if (!c) return NULL;
  
  int r = pipe(pipefd);
  if (r < 0) return NULL;

  r = set_nonblock(pipefd[0]);
  if (r < 0) return NULL;
  assert(pipefd[0] >= 0);

  r = set_nonblock(pipefd[1]);
  if (r < 0) return NULL;
  assert(pipefd[1] >= 0);

  if (is_pull) {
    c->is_pull = 1;
    c->pullfd = fd;
    c->pushfd = pipefd[1];
    c->exposedfd = pipefd[0];
  } else {
    c->is_pull = 0;
    c->pushfd = fd;
    c->pullfd = pipefd[0];
    c->exposedfd = pipefd[1];
  }

  r = pthread_create(&c->tid, NULL, pump_thread, c);
  if (r < 0) return NULL;

  return c;
}

struct coupling*
coupling_new_pull (int fd)
{
  return create_coupling(fd, 1);
}

struct coupling*
coupling_new_push (int fd)
{
  return create_coupling(fd, 0);
}

int 
coupling_nonblocking_fd (struct coupling *c)
{
  return c->exposedfd;
}

void 
coupling_join (struct coupling *c)
{
  int r = pthread_join(c->tid, NULL);
  assert(r == 0);
}

void
coupling_destroy (struct coupling *c)
{
  close(c->is_pull ? c->pushfd : c->pullfd);
  free(c);
}
