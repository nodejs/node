/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#ifndef UV_LINUX_SYSCALL_H_
#define UV_LINUX_SYSCALL_H_

#undef  _GNU_SOURCE
#define _GNU_SOURCE

#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#if defined(__alpha__)
# define UV__O_CLOEXEC        0x200000
#elif defined(__hppa__)
# define UV__O_CLOEXEC        0x200000
#elif defined(__sparc__)
# define UV__O_CLOEXEC        0x400000
#else
# define UV__O_CLOEXEC        0x80000
#endif

#if defined(__alpha__)
# define UV__O_NONBLOCK       0x4
#elif defined(__hppa__)
# define UV__O_NONBLOCK       O_NONBLOCK
#elif defined(__mips__)
# define UV__O_NONBLOCK       0x80
#elif defined(__sparc__)
# define UV__O_NONBLOCK       0x4000
#else
# define UV__O_NONBLOCK       0x800
#endif

#define UV__EFD_CLOEXEC       UV__O_CLOEXEC
#define UV__EFD_NONBLOCK      UV__O_NONBLOCK

#define UV__IN_CLOEXEC        UV__O_CLOEXEC
#define UV__IN_NONBLOCK       UV__O_NONBLOCK

#define UV__SOCK_CLOEXEC      UV__O_CLOEXEC
#if defined(SOCK_NONBLOCK)
# define UV__SOCK_NONBLOCK    SOCK_NONBLOCK
#else
# define UV__SOCK_NONBLOCK    UV__O_NONBLOCK
#endif

/* inotify flags */
#define UV__IN_ACCESS         0x001
#define UV__IN_MODIFY         0x002
#define UV__IN_ATTRIB         0x004
#define UV__IN_CLOSE_WRITE    0x008
#define UV__IN_CLOSE_NOWRITE  0x010
#define UV__IN_OPEN           0x020
#define UV__IN_MOVED_FROM     0x040
#define UV__IN_MOVED_TO       0x080
#define UV__IN_CREATE         0x100
#define UV__IN_DELETE         0x200
#define UV__IN_DELETE_SELF    0x400
#define UV__IN_MOVE_SELF      0x800

struct uv__inotify_event {
  int32_t wd;
  uint32_t mask;
  uint32_t cookie;
  uint32_t len;
  /* char name[0]; */
};

struct uv__mmsghdr {
  struct msghdr msg_hdr;
  unsigned int msg_len;
};

int uv__accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags);
int uv__eventfd(unsigned int count);
int uv__eventfd2(unsigned int count, int flags);
int uv__inotify_init(void);
int uv__inotify_init1(int flags);
int uv__inotify_add_watch(int fd, const char* path, uint32_t mask);
int uv__inotify_rm_watch(int fd, int32_t wd);
int uv__pipe2(int pipefd[2], int flags);
int uv__recvmmsg(int fd,
                 struct uv__mmsghdr* mmsg,
                 unsigned int vlen,
                 unsigned int flags,
                 struct timespec* timeout);
int uv__sendmmsg(int fd,
                 struct uv__mmsghdr* mmsg,
                 unsigned int vlen,
                 unsigned int flags);
ssize_t uv__preadv(int fd, const struct iovec *iov, int iovcnt, int64_t offset);
ssize_t uv__pwritev(int fd, const struct iovec *iov, int iovcnt, int64_t offset);
int uv__dup3(int oldfd, int newfd, int flags);

#endif /* UV_LINUX_SYSCALL_H_ */
