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

#include "linux-syscalls.h"
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <errno.h>

#if defined(__has_feature)
# if __has_feature(memory_sanitizer)
#  define MSAN_ACTIVE 1
#  include <sanitizer/msan_interface.h>
# endif
#endif

#if defined(__i386__)
# ifndef __NR_socketcall
#  define __NR_socketcall 102
# endif
#endif

#if defined(__arm__)
# if defined(__thumb__) || defined(__ARM_EABI__)
#  define UV_SYSCALL_BASE 0
# else
#  define UV_SYSCALL_BASE 0x900000
# endif
#endif /* __arm__ */

#ifndef __NR_accept4
# if defined(__x86_64__)
#  define __NR_accept4 288
# elif defined(__i386__)
   /* Nothing. Handled through socketcall(). */
# elif defined(__arm__)
#  define __NR_accept4 (UV_SYSCALL_BASE + 366)
# endif
#endif /* __NR_accept4 */

#ifndef __NR_eventfd
# if defined(__x86_64__)
#  define __NR_eventfd 284
# elif defined(__i386__)
#  define __NR_eventfd 323
# elif defined(__arm__)
#  define __NR_eventfd (UV_SYSCALL_BASE + 351)
# endif
#endif /* __NR_eventfd */

#ifndef __NR_eventfd2
# if defined(__x86_64__)
#  define __NR_eventfd2 290
# elif defined(__i386__)
#  define __NR_eventfd2 328
# elif defined(__arm__)
#  define __NR_eventfd2 (UV_SYSCALL_BASE + 356)
# endif
#endif /* __NR_eventfd2 */

#ifndef __NR_inotify_init
# if defined(__x86_64__)
#  define __NR_inotify_init 253
# elif defined(__i386__)
#  define __NR_inotify_init 291
# elif defined(__arm__)
#  define __NR_inotify_init (UV_SYSCALL_BASE + 316)
# endif
#endif /* __NR_inotify_init */

#ifndef __NR_inotify_init1
# if defined(__x86_64__)
#  define __NR_inotify_init1 294
# elif defined(__i386__)
#  define __NR_inotify_init1 332
# elif defined(__arm__)
#  define __NR_inotify_init1 (UV_SYSCALL_BASE + 360)
# endif
#endif /* __NR_inotify_init1 */

#ifndef __NR_inotify_add_watch
# if defined(__x86_64__)
#  define __NR_inotify_add_watch 254
# elif defined(__i386__)
#  define __NR_inotify_add_watch 292
# elif defined(__arm__)
#  define __NR_inotify_add_watch (UV_SYSCALL_BASE + 317)
# endif
#endif /* __NR_inotify_add_watch */

#ifndef __NR_inotify_rm_watch
# if defined(__x86_64__)
#  define __NR_inotify_rm_watch 255
# elif defined(__i386__)
#  define __NR_inotify_rm_watch 293
# elif defined(__arm__)
#  define __NR_inotify_rm_watch (UV_SYSCALL_BASE + 318)
# endif
#endif /* __NR_inotify_rm_watch */

#ifndef __NR_pipe2
# if defined(__x86_64__)
#  define __NR_pipe2 293
# elif defined(__i386__)
#  define __NR_pipe2 331
# elif defined(__arm__)
#  define __NR_pipe2 (UV_SYSCALL_BASE + 359)
# endif
#endif /* __NR_pipe2 */

#ifndef __NR_recvmmsg
# if defined(__x86_64__)
#  define __NR_recvmmsg 299
# elif defined(__i386__)
#  define __NR_recvmmsg 337
# elif defined(__arm__)
#  define __NR_recvmmsg (UV_SYSCALL_BASE + 365)
# endif
#endif /* __NR_recvmsg */

#ifndef __NR_sendmmsg
# if defined(__x86_64__)
#  define __NR_sendmmsg 307
# elif defined(__i386__)
#  define __NR_sendmmsg 345
# elif defined(__arm__)
#  define __NR_sendmmsg (UV_SYSCALL_BASE + 374)
# endif
#endif /* __NR_sendmmsg */

#ifndef __NR_utimensat
# if defined(__x86_64__)
#  define __NR_utimensat 280
# elif defined(__i386__)
#  define __NR_utimensat 320
# elif defined(__arm__)
#  define __NR_utimensat (UV_SYSCALL_BASE + 348)
# endif
#endif /* __NR_utimensat */

#ifndef __NR_preadv
# if defined(__x86_64__)
#  define __NR_preadv 295
# elif defined(__i386__)
#  define __NR_preadv 333
# elif defined(__arm__)
#  define __NR_preadv (UV_SYSCALL_BASE + 361)
# endif
#endif /* __NR_preadv */

#ifndef __NR_pwritev
# if defined(__x86_64__)
#  define __NR_pwritev 296
# elif defined(__i386__)
#  define __NR_pwritev 334
# elif defined(__arm__)
#  define __NR_pwritev (UV_SYSCALL_BASE + 362)
# endif
#endif /* __NR_pwritev */

#ifndef __NR_dup3
# if defined(__x86_64__)
#  define __NR_dup3 292
# elif defined(__i386__)
#  define __NR_dup3 330
# elif defined(__arm__)
#  define __NR_dup3 (UV_SYSCALL_BASE + 358)
# endif
#endif /* __NR_pwritev */

#ifndef __NR_statx
# if defined(__x86_64__)
#  define __NR_statx 332
# elif defined(__i386__)
#  define __NR_statx 383
# elif defined(__aarch64__)
#  define __NR_statx 397
# elif defined(__arm__)
#  define __NR_statx (UV_SYSCALL_BASE + 397)
# elif defined(__ppc__)
#  define __NR_statx 383
# elif defined(__s390__)
#  define __NR_statx 379
# endif
#endif /* __NR_statx */

int uv__accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
#if defined(__i386__)
  unsigned long args[4];
  int r;

  args[0] = (unsigned long) fd;
  args[1] = (unsigned long) addr;
  args[2] = (unsigned long) addrlen;
  args[3] = (unsigned long) flags;

  r = syscall(__NR_socketcall, 18 /* SYS_ACCEPT4 */, args);

  /* socketcall() raises EINVAL when SYS_ACCEPT4 is not supported but so does
   * a bad flags argument. Try to distinguish between the two cases.
   */
  if (r == -1)
    if (errno == EINVAL)
      if ((flags & ~(UV__SOCK_CLOEXEC|UV__SOCK_NONBLOCK)) == 0)
        errno = ENOSYS;

  return r;
#elif defined(__NR_accept4)
  return syscall(__NR_accept4, fd, addr, addrlen, flags);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__eventfd(unsigned int count) {
#if defined(__NR_eventfd)
  return syscall(__NR_eventfd, count);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__eventfd2(unsigned int count, int flags) {
#if defined(__NR_eventfd2)
  return syscall(__NR_eventfd2, count, flags);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__inotify_init(void) {
#if defined(__NR_inotify_init)
  return syscall(__NR_inotify_init);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__inotify_init1(int flags) {
#if defined(__NR_inotify_init1)
  return syscall(__NR_inotify_init1, flags);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__inotify_add_watch(int fd, const char* path, uint32_t mask) {
#if defined(__NR_inotify_add_watch)
  return syscall(__NR_inotify_add_watch, fd, path, mask);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__inotify_rm_watch(int fd, int32_t wd) {
#if defined(__NR_inotify_rm_watch)
  return syscall(__NR_inotify_rm_watch, fd, wd);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__pipe2(int pipefd[2], int flags) {
#if defined(__NR_pipe2)
  int result;
  result = syscall(__NR_pipe2, pipefd, flags);
#if MSAN_ACTIVE
  if (!result)
    __msan_unpoison(pipefd, sizeof(int[2]));
#endif
  return result;
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__sendmmsg(int fd,
                 struct uv__mmsghdr* mmsg,
                 unsigned int vlen,
                 unsigned int flags) {
#if defined(__NR_sendmmsg)
  return syscall(__NR_sendmmsg, fd, mmsg, vlen, flags);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__recvmmsg(int fd,
                 struct uv__mmsghdr* mmsg,
                 unsigned int vlen,
                 unsigned int flags,
                 struct timespec* timeout) {
#if defined(__NR_recvmmsg)
  return syscall(__NR_recvmmsg, fd, mmsg, vlen, flags, timeout);
#else
  return errno = ENOSYS, -1;
#endif
}


ssize_t uv__preadv(int fd, const struct iovec *iov, int iovcnt, int64_t offset) {
#if defined(__NR_preadv)
  return syscall(__NR_preadv, fd, iov, iovcnt, (long)offset, (long)(offset >> 32));
#else
  return errno = ENOSYS, -1;
#endif
}


ssize_t uv__pwritev(int fd, const struct iovec *iov, int iovcnt, int64_t offset) {
#if defined(__NR_pwritev)
  return syscall(__NR_pwritev, fd, iov, iovcnt, (long)offset, (long)(offset >> 32));
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__dup3(int oldfd, int newfd, int flags) {
#if defined(__NR_dup3)
  return syscall(__NR_dup3, oldfd, newfd, flags);
#else
  return errno = ENOSYS, -1;
#endif
}


int uv__statx(int dirfd,
              const char* path,
              int flags,
              unsigned int mask,
              struct uv__statx* statxbuf) {
  /* __NR_statx make Android box killed by SIGSYS.
   * That looks like a seccomp2 sandbox filter rejecting the system call.
   */
#if defined(__NR_statx) && !defined(__ANDROID__)
  return syscall(__NR_statx, dirfd, path, flags, mask, statxbuf);
#else
  return errno = ENOSYS, -1;
#endif
}
