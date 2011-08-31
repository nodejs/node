dnl openbsd in it's neverending brokenness requires stdint.h for intptr_t,
dnl but that header isn't very portable...
AC_CHECK_HEADERS([stdint.h sys/syscall.h sys/prctl.h])

AC_SEARCH_LIBS(
   pthread_create,
   [pthread pthreads pthreadVC2],
   ,
   [AC_MSG_ERROR(pthread functions not found)]
)

AC_CACHE_CHECK(for utimes, ac_cv_utimes, [AC_LINK_IFELSE([[
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>
struct timeval tv[2];
int res;
int main (void)
{
   res = utimes ("/", tv);
   return 0;
}
]],ac_cv_utimes=yes,ac_cv_utimes=no)])
test $ac_cv_utimes = yes && AC_DEFINE(HAVE_UTIMES, 1, utimes(2) is available)

AC_CACHE_CHECK(for futimes, ac_cv_futimes, [AC_LINK_IFELSE([[
#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>
struct timeval tv[2];
int res;
int fd;
int main (void)
{
   res = futimes (fd, tv);
   return 0;
}
]],ac_cv_futimes=yes,ac_cv_futimes=no)])
test $ac_cv_futimes = yes && AC_DEFINE(HAVE_FUTIMES, 1, futimes(2) is available)

AC_CACHE_CHECK(for readahead, ac_cv_readahead, [AC_LINK_IFELSE([
#include <fcntl.h>
int main (void)
{
   int fd = 0;
   size_t count = 2;
   ssize_t res;
   res = readahead (fd, 0, count);
   return 0;
}
],ac_cv_readahead=yes,ac_cv_readahead=no)])
test $ac_cv_readahead = yes && AC_DEFINE(HAVE_READAHEAD, 1, readahead(2) is available (linux))

AC_CACHE_CHECK(for fdatasync, ac_cv_fdatasync, [AC_LINK_IFELSE([
#include <unistd.h>
int main (void)
{
   int fd = 0;
   fdatasync (fd);
   return 0;
}
],ac_cv_fdatasync=yes,ac_cv_fdatasync=no)])
test $ac_cv_fdatasync = yes && AC_DEFINE(HAVE_FDATASYNC, 1, fdatasync(2) is available)

AC_CACHE_CHECK(for pread and pwrite, ac_cv_preadwrite, [AC_LINK_IFELSE([
#include <unistd.h>
int main (void)
{
   int fd = 0;
   size_t count = 1;
   char buf;
   off_t offset = 1;
   ssize_t res;
   res = pread (fd, &buf, count, offset);
   res = pwrite (fd, &buf, count, offset);
   return 0;
}
],ac_cv_preadwrite=yes,ac_cv_preadwrite=no)])
test $ac_cv_preadwrite = yes && AC_DEFINE(HAVE_PREADWRITE, 1, pread(2) and pwrite(2) are available)

AC_CACHE_CHECK(for sendfile, ac_cv_sendfile, [AC_LINK_IFELSE([
# include <sys/types.h>
#if __linux
# include <sys/sendfile.h>
#elif __FreeBSD__ || defined __APPLE__
# include <sys/socket.h>
# include <sys/uio.h>
#elif __hpux
# include <sys/socket.h>
#else
# error unsupported architecture
#endif
int main (void)
{
   int fd = 0;
   off_t offset = 1;
   size_t count = 2;
   ssize_t res;
#if __linux
   res = sendfile (fd, fd, offset, count);
#elif __FreeBSD__
   res = sendfile (fd, fd, offset, count, 0, &offset, 0);
#elif __hpux
   res = sendfile (fd, fd, offset, count, 0, 0);
#endif
   return 0;
}
],ac_cv_sendfile=yes,ac_cv_sendfile=no)])
test $ac_cv_sendfile = yes && AC_DEFINE(HAVE_SENDFILE, 1, sendfile(2) is available and supported)

AC_CACHE_CHECK(for sync_file_range, ac_cv_sync_file_range, [AC_LINK_IFELSE([
#include <fcntl.h>
int main (void)
{
   int fd = 0;
   off64_t offset = 1;
   off64_t nbytes = 1;
   unsigned int flags = SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER;
   ssize_t res;
   res = sync_file_range (fd, offset, nbytes, flags);
   return 0;
}
],ac_cv_sync_file_range=yes,ac_cv_sync_file_range=no)])
test $ac_cv_sync_file_range = yes && AC_DEFINE(HAVE_SYNC_FILE_RANGE, 1, sync_file_range(2) is available)

AC_CACHE_CHECK(for fallocate, ac_cv_fallocate, [AC_LINK_IFELSE([
#include <fcntl.h>
int main (void)
{
   int fd = 0;
   int mode = 0;
   off_t offset = 1;
   off_t len = 1;
   int res;
   res = fallocate (fd, mode, offset, len);
   return 0;
}
],ac_cv_fallocate=yes,ac_cv_fallocate=no)])
test $ac_cv_fallocate = yes && AC_DEFINE(HAVE_FALLOCATE, 1, fallocate(2) is available)

AC_CACHE_CHECK(for sys_syncfs, ac_cv_sys_syncfs, [AC_LINK_IFELSE([
#include <unistd.h>
#include <sys/syscall.h>
int main (void)
{
  int res = syscall (__NR_syncfs, (int)0);
}
],ac_cv_sys_syncfs=yes,ac_cv_sys_syncfs=no)])
test $ac_cv_sys_syncfs = yes && AC_DEFINE(HAVE_SYS_SYNCFS, 1, syscall(__NR_syncfs) is available)

AC_CACHE_CHECK(for prctl_set_name, ac_cv_prctl_set_name, [AC_LINK_IFELSE([
#include <sys/prctl.h>
int main (void)
{
  char name[] = "test123";
  int res = prctl (PR_SET_NAME, (unsigned long)name, 0, 0, 0);
}
],ac_cv_prctl_set_name=yes,ac_cv_prctl_set_name=no)])
test $ac_cv_prctl_set_name = yes && AC_DEFINE(HAVE_PRCTL_SET_NAME, 1, prctl(PR_SET_NAME) is available)

dnl #############################################################################
dnl # these checks exist for the benefit of IO::AIO

dnl at least uclibc defines _POSIX_ADVISORY_INFO without *any* of the required
dnl functionality actually being present. ugh.
AC_CACHE_CHECK(for posix_madvise, ac_cv_posix_madvise, [AC_LINK_IFELSE([
#include <sys/mman.h>
int main (void)
{
   int res = posix_madvise ((void *)0, (size_t)0, POSIX_MADV_NORMAL);
   int a = POSIX_MADV_SEQUENTIAL;
   int b = POSIX_MADV_RANDOM;
   int c = POSIX_MADV_WILLNEED;
   int d = POSIX_MADV_DONTNEED;
   return 0;
}
],ac_cv_posix_madvise=yes,ac_cv_posix_madvise=no)])
test $ac_cv_posix_madvise = yes && AC_DEFINE(HAVE_POSIX_MADVISE, 1, posix_madvise(2) is available)

AC_CACHE_CHECK(for posix_fadvise, ac_cv_posix_fadvise, [AC_LINK_IFELSE([
#define _XOPEN_SOURCE 600
#include <fcntl.h>
int main (void)
{
   int res = posix_fadvise ((int)0, (off_t)0, (off_t)0, POSIX_FADV_NORMAL);
   int a = POSIX_FADV_SEQUENTIAL;
   int b = POSIX_FADV_NOREUSE;
   int c = POSIX_FADV_RANDOM;
   int d = POSIX_FADV_WILLNEED;
   int e = POSIX_FADV_DONTNEED;
   return 0;
}
],ac_cv_posix_fadvise=yes,ac_cv_posix_fadvise=no)])
test $ac_cv_posix_fadvise = yes && AC_DEFINE(HAVE_POSIX_FADVISE, 1, posix_fadvise(2) is available)

