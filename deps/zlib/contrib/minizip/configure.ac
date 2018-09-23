#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([minizip], [1.2.8], [bugzilla.redhat.com])
AC_CONFIG_SRCDIR([minizip.c])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

AC_MSG_CHECKING([whether to build example programs])
AC_ARG_ENABLE([demos], AC_HELP_STRING([--enable-demos], [build example programs]))
AM_CONDITIONAL([COND_DEMOS], [test "$enable_demos" = yes])
if test "$enable_demos" = yes
then
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi

case "${host}" in
	*-mingw* | mingw*)
		WIN32="yes"
		;;
	*)
		;;
esac
AM_CONDITIONAL([WIN32], [test "${WIN32}" = "yes"])


AC_SUBST([HAVE_UNISTD_H], [0])
AC_CHECK_HEADER([unistd.h], [HAVE_UNISTD_H=1], [])
AC_CONFIG_FILES([Makefile minizip.pc])
AC_OUTPUT
