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

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/types.h>

#define UV__O_NONBLOCK        0x800
#define UV__O_CLOEXEC         0x80000

#define UV__EFD_CLOEXEC       UV__O_CLOEXEC
#define UV__EFD_NONBLOCK      UV__O_NONBLOCK

#define UV__IN_CLOEXEC        UV__O_CLOEXEC
#define UV__IN_NONBLOCK       UV__O_NONBLOCK

#define UV__SOCK_CLOEXEC      UV__O_CLOEXEC
#define UV__SOCK_NONBLOCK     UV__O_NONBLOCK

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
  __s32 wd;
  __u32 mask;
  __u32 cookie;
  __u32 len;
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
int uv__inotify_add_watch(int fd, const char* path, __u32 mask);
int uv__inotify_rm_watch(int fd, __s32 wd);
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
int uv__utimesat(int dirfd,
                 const char* path,
                 const struct timespec times[2],
                 int flags);

#endif /* UV_LINUX_SYSCALL_H_ */
