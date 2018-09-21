/* Copyright libuv project contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#ifndef UV_OS390_SYSCALL_H_
#define UV_OS390_SYSCALL_H_

#include "uv.h"
#include "internal.h"
#include <dirent.h>
#include <poll.h>
#include <pthread.h>

#define EPOLL_CTL_ADD             1
#define EPOLL_CTL_DEL             2
#define EPOLL_CTL_MOD             3
#define MAX_EPOLL_INSTANCES       256
#define MAX_ITEMS_PER_EPOLL       1024

#define UV__O_CLOEXEC             0x80000

struct epoll_event {
  int events;
  int fd;
};

typedef struct {
  QUEUE member;
  struct pollfd* items;
  unsigned long size;
  int msg_queue;
} uv__os390_epoll;

/* epoll api */
uv__os390_epoll* epoll_create1(int flags);
int epoll_ctl(uv__os390_epoll* ep, int op, int fd, struct epoll_event *event);
int epoll_wait(uv__os390_epoll* ep, struct epoll_event *events, int maxevents, int timeout);
int epoll_file_close(int fd);

/* utility functions */
int nanosleep(const struct timespec* req, struct timespec* rem);
int scandir(const char* maindir, struct dirent*** namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **,
            const struct dirent **));
char *mkdtemp(char* path);
ssize_t os390_readlink(const char* path, char* buf, size_t len);
size_t strnlen(const char* str, size_t maxlen);

#endif /* UV_OS390_SYSCALL_H_ */
