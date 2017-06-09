/*
Copyright (c) 2016, Kari Tristan Helgason <kthelgason@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef _UV_PTHREAD_BARRIER_
#define _UV_PTHREAD_BARRIER_
#include <errno.h>
#include <pthread.h>
#if !defined(__MVS__)
#include <semaphore.h> /* sem_t */
#endif

#define PTHREAD_BARRIER_SERIAL_THREAD  0x12345

/*
 * To maintain ABI compatibility with
 * libuv v1.x struct is padded according
 * to target platform
 */
#if defined(__ANDROID__)
# define UV_BARRIER_STRUCT_PADDING \
  sizeof(pthread_mutex_t) + \
  sizeof(pthread_cond_t) + \
  sizeof(unsigned int) - \
  sizeof(void *)
#elif defined(__APPLE__)
# define UV_BARRIER_STRUCT_PADDING \
  sizeof(pthread_mutex_t) + \
  2 * sizeof(sem_t) + \
  2 * sizeof(unsigned int) - \
  sizeof(void *)
#else
# define UV_BARRIER_STRUCT_PADDING 0
#endif

typedef struct {
  pthread_mutex_t  mutex;
  pthread_cond_t   cond;
  unsigned         threshold;
  unsigned         in;
  unsigned         out;
} _uv_barrier;

typedef struct {
  _uv_barrier* b;
  char _pad[UV_BARRIER_STRUCT_PADDING];
} pthread_barrier_t;

int pthread_barrier_init(pthread_barrier_t* barrier,
                         const void* barrier_attr,
                         unsigned count);

int pthread_barrier_wait(pthread_barrier_t* barrier);
int pthread_barrier_destroy(pthread_barrier_t *barrier);

#endif /* _UV_PTHREAD_BARRIER_ */
