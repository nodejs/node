dnl also update ev.h!
AC_INIT([libev], [4.33])

orig_CFLAGS="$CFLAGS"

AC_CONFIG_SRCDIR([ev_epoll.c])
AM_INIT_AUTOMAKE

AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE

AC_PROG_CC

dnl Supply default CFLAGS, if not specified
if test -z "$orig_CFLAGS"; then
  if test x$GCC = xyes; then
    CFLAGS="-g -O3"
  fi
fi

AC_PROG_INSTALL
AC_PROG_LIBTOOL

m4_include([libev.m4])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
