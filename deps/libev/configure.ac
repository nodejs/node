AC_INIT
AC_CONFIG_SRCDIR([ev_epoll.c])

AM_INIT_AUTOMAKE(libev,3.8) dnl also update ev.h!
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AC_PROG_INSTALL
AC_PROG_LIBTOOL

if test "x$GCC" = xyes ; then
  CFLAGS="$CFLAGS -O3"
fi

m4_include([libev.m4])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
