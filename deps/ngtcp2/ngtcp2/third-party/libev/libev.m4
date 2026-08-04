dnl this file is part of libev, do not make local modifications
dnl http://software.schmorp.de/pkg/libev

dnl libev support
AC_CHECK_HEADERS(sys/inotify.h sys/epoll.h sys/event.h port.h poll.h sys/timerfd.h)
AC_CHECK_HEADERS(sys/select.h sys/eventfd.h sys/signalfd.h linux/aio_abi.h linux/fs.h)
 
AC_CHECK_FUNCS(inotify_init epoll_ctl kqueue port_create poll select eventfd signalfd)
 
AC_CHECK_FUNCS(clock_gettime, [], [
   dnl on linux, try syscall wrapper first
   if test $(uname) = Linux; then
      AC_MSG_CHECKING(for clock_gettime syscall)
      AC_LINK_IFELSE([AC_LANG_PROGRAM(
                      [#include <unistd.h>
                       #include <sys/syscall.h>
                       #include <time.h>],
                      [struct timespec ts; int status = syscall (SYS_clock_gettime, CLOCK_REALTIME, &ts)])],
                     [ac_have_clock_syscall=1
                      AC_DEFINE(HAVE_CLOCK_SYSCALL, 1, Define to 1 to use the syscall interface for clock_gettime)
                      AC_MSG_RESULT(yes)],
                     [AC_MSG_RESULT(no)])
   fi
   if test -z "$LIBEV_M4_AVOID_LIBRT" && test -z "$ac_have_clock_syscall"; then
      AC_CHECK_LIB(rt, clock_gettime)
      unset ac_cv_func_clock_gettime
      AC_CHECK_FUNCS(clock_gettime)
   fi
])

AC_CHECK_FUNCS(nanosleep, [], [
   if test -z "$LIBEV_M4_AVOID_LIBRT"; then
      AC_CHECK_LIB(rt, nanosleep)
      unset ac_cv_func_nanosleep
      AC_CHECK_FUNCS(nanosleep)
   fi
])

AC_CHECK_TYPE(__kernel_rwf_t, [
   AC_DEFINE(HAVE_KERNEL_RWF_T, 1, Define to 1 if linux/fs.h defined kernel_rwf_t)
], [], [#include <linux/fs.h>])

if test -z "$LIBEV_M4_AVOID_LIBM"; then
   LIBM=m
fi
AC_SEARCH_LIBS(floor, $LIBM, [AC_DEFINE(HAVE_FLOOR, 1, Define to 1 if the floor function is available)])

