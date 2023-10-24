#***************************************************************************
#
# Copyright (C) Daniel Stenberg et al
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided
# that the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation, and that the name of M.I.T. not be used in advertising or
# publicity pertaining to distribution of the software without specific,
# written prior permission.  M.I.T. makes no representations about the
# suitability of this software for any purpose.  It is provided "as is"
# without express or implied warranty.
#
# SPDX-License-Identifier: MIT
#***************************************************************************

# File version for 'aclocal' use. Keep it a single number.
# serial 11


dnl CARES_CHECK_OPTION_DEBUG
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-debug or --disable-debug, and set shell
dnl variable want_debug value as appropriate.

AC_DEFUN([CARES_CHECK_OPTION_DEBUG], [
  AC_BEFORE([$0],[CARES_CHECK_OPTION_WARNINGS])dnl
  AC_BEFORE([$0],[XC_CHECK_PROG_CC])dnl
  AC_MSG_CHECKING([whether to enable debug build options])
  OPT_DEBUG_BUILD="default"
  AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],[Enable debug build options])
AS_HELP_STRING([--disable-debug],[Disable debug build options]),
  OPT_DEBUG_BUILD=$enableval)
  case "$OPT_DEBUG_BUILD" in
    no)
      dnl --disable-debug option used
      want_debug="no"
      ;;
    default)
      dnl configure option not specified
      want_debug="no"
      ;;
    *)
      dnl --enable-debug option used
      want_debug="yes"
      ;;
  esac
  AC_MSG_RESULT([$want_debug])
])


dnl CARES_CHECK_OPTION_NONBLOCKING
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-nonblocking or --disable-nonblocking, and
dnl set shell variable want_nonblocking as appropriate.

AC_DEFUN([CARES_CHECK_OPTION_NONBLOCKING], [
  AC_BEFORE([$0],[CARES_CHECK_NONBLOCKING_SOCKET])dnl
  AC_MSG_CHECKING([whether to enable non-blocking communications])
  OPT_NONBLOCKING="default"
  AC_ARG_ENABLE(nonblocking,
AS_HELP_STRING([--enable-nonblocking],[Enable non-blocking communications])
AS_HELP_STRING([--disable-nonblocking],[Disable non-blocking communications]),
  OPT_NONBLOCKING=$enableval)
  case "$OPT_NONBLOCKING" in
    no)
      dnl --disable-nonblocking option used
      want_nonblocking="no"
      ;;
    default)
      dnl configure option not specified
      want_nonblocking="yes"
      ;;
    *)
      dnl --enable-nonblocking option used
      want_nonblocking="yes"
      ;;
  esac
  AC_MSG_RESULT([$want_nonblocking])
])


dnl CARES_CHECK_OPTION_OPTIMIZE
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-optimize or --disable-optimize, and set
dnl shell variable want_optimize value as appropriate.

AC_DEFUN([CARES_CHECK_OPTION_OPTIMIZE], [
  AC_REQUIRE([CARES_CHECK_OPTION_DEBUG])dnl
  AC_BEFORE([$0],[XC_CHECK_PROG_CC])dnl
  AC_MSG_CHECKING([whether to enable compiler optimizer])
  OPT_COMPILER_OPTIMIZE="default"
  AC_ARG_ENABLE(optimize,
AS_HELP_STRING([--enable-optimize(=OPT)],[Enable compiler optimizations (default=-O2)])
AS_HELP_STRING([--disable-optimize],[Disable compiler optimizations]),
  OPT_COMPILER_OPTIMIZE=$enableval)
  case "$OPT_COMPILER_OPTIMIZE" in
    no)
      dnl --disable-optimize option used. We will handle this as
      dnl a request to disable compiler optimizations if possible.
      dnl If the compiler is known CFLAGS and CPPFLAGS will be
      dnl overridden, otherwise this can not be honored.
      want_optimize="no"
      AC_MSG_RESULT([no])
      ;;
    default)
      dnl configure's optimize option not specified. Initially we will
      dnl handle this as a a request contrary to configure's setting
      dnl for --enable-debug. IOW, initially, for debug-enabled builds
      dnl this will be handled as a request to disable optimizations if
      dnl possible, and for debug-disabled builds this will be handled
      dnl initially as a request to enable optimizations if possible.
      dnl Finally, if the compiler is known and CFLAGS and CPPFLAGS do
      dnl not have any optimizer flag the request will be honored, in
      dnl any other case the request can not be honored.
      dnl IOW, existing optimizer flags defined in CFLAGS or CPPFLAGS
      dnl will always take precedence over any initial assumption.
      if test "$want_debug" = "yes"; then
        want_optimize="assume_no"
        AC_MSG_RESULT([not specified (assuming no)])
      else
        want_optimize="assume_yes"
        AC_MSG_RESULT([not specified (assuming yes)])
      fi
      ;;
    *)
      dnl --enable-optimize option used. We will handle this as
      dnl a request to enable compiler optimizations if possible.
      dnl If the compiler is known CFLAGS and CPPFLAGS will be
      dnl overridden, otherwise this can not be honored.
      want_optimize="yes"
      AC_MSG_RESULT([yes])
      ;;
  esac
])


dnl CARES_CHECK_OPTION_SYMBOL_HIDING
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-symbol-hiding or --disable-symbol-hiding,
dnl setting shell variable want_symbol_hiding value.

AC_DEFUN([CARES_CHECK_OPTION_SYMBOL_HIDING], [
  AC_BEFORE([$0],[CARES_CHECK_COMPILER_SYMBOL_HIDING])dnl
  AC_MSG_CHECKING([whether to enable hiding of library internal symbols])
  OPT_SYMBOL_HIDING="default"
  AC_ARG_ENABLE(symbol-hiding,
AS_HELP_STRING([--enable-symbol-hiding],[Enable hiding of library internal symbols])
AS_HELP_STRING([--disable-symbol-hiding],[Disable hiding of library internal symbols]),
  OPT_SYMBOL_HIDING=$enableval)
  case "$OPT_SYMBOL_HIDING" in
    no)
      dnl --disable-symbol-hiding option used.
      dnl This is an indication to not attempt hiding of library internal
      dnl symbols. Default symbol visibility will be used, which normally
      dnl exposes all library internal symbols.
      want_symbol_hiding="no"
      AC_MSG_RESULT([no])
      ;;
    default)
      dnl configure's symbol-hiding option not specified.
      dnl Handle this as if --enable-symbol-hiding option was given.
      want_symbol_hiding="yes"
      AC_MSG_RESULT([yes])
      ;;
    *)
      dnl --enable-symbol-hiding option used.
      dnl This is an indication to attempt hiding of library internal
      dnl symbols. This is only supported on some compilers/linkers.
      want_symbol_hiding="yes"
      AC_MSG_RESULT([yes])
      ;;
  esac
])


dnl CARES_CHECK_OPTION_EXPOSE_STATICS
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-expose-statics or --disable-expose-statics,
dnl setting shell variable want_expose_statics value.

AC_DEFUN([CARES_CHECK_OPTION_EXPOSE_STATICS], [
  AC_MSG_CHECKING([whether to expose internal static functions for testing])
  OPT_EXPOSE_STATICS="default"
  AC_ARG_ENABLE(expose-statics,
AS_HELP_STRING([--enable-expose-statics],[Enable exposure of internal static functions for testing])
AS_HELP_STRING([--disable-expose-statics],[Disable exposure of internal static functions for testing]),
  OPT_EXPOSE_STATICS=$enableval)
  case "$OPT_EXPOSE_STATICS" in
    no)
      dnl --disable-expose-statics option used.
      want_expose_statics="no"
      AC_MSG_RESULT([no])
      ;;
    default)
      dnl configure's expose-statics option not specified.
      dnl Handle this as if --disable-expose-statics option was given.
      want_expose_statics="no"
      AC_MSG_RESULT([no])
      ;;
    *)
      dnl --enable-expose-statics option used.
      want_expose_statics="yes"
      AC_MSG_RESULT([yes])
      ;;
  esac
  if test "$want_expose_statics" = "yes"; then
    AC_DEFINE_UNQUOTED(CARES_EXPOSE_STATICS, 1,
      [Defined for build that exposes internal static functions for testing.])
  fi
])


dnl CARES_CHECK_OPTION_WARNINGS
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-warnings or --disable-warnings, and set
dnl shell variable want_warnings as appropriate.

AC_DEFUN([CARES_CHECK_OPTION_WARNINGS], [
  AC_REQUIRE([CARES_CHECK_OPTION_DEBUG])dnl
  AC_BEFORE([$0],[CARES_CHECK_OPTION_WERROR])dnl
  AC_BEFORE([$0],[XC_CHECK_PROG_CC])dnl
  AC_MSG_CHECKING([whether to enable strict compiler warnings])
  OPT_COMPILER_WARNINGS="default"
  AC_ARG_ENABLE(warnings,
AS_HELP_STRING([--enable-warnings],[Enable strict compiler warnings])
AS_HELP_STRING([--disable-warnings],[Disable strict compiler warnings]),
  OPT_COMPILER_WARNINGS=$enableval)
  case "$OPT_COMPILER_WARNINGS" in
    no)
      dnl --disable-warnings option used
      want_warnings="no"
      ;;
    default)
      dnl configure option not specified, so
      dnl use same setting as --enable-debug
      want_warnings="$want_debug"
      ;;
    *)
      dnl --enable-warnings option used
      want_warnings="yes"
      ;;
  esac
  AC_MSG_RESULT([$want_warnings])
])

dnl CARES_CHECK_OPTION_WERROR
dnl -------------------------------------------------
dnl Verify if configure has been invoked with option
dnl --enable-werror or --disable-werror, and set
dnl shell variable want_werror as appropriate.

AC_DEFUN([CARES_CHECK_OPTION_WERROR], [
  AC_BEFORE([$0],[CARES_CHECK_COMPILER])dnl
  AC_MSG_CHECKING([whether to enable compiler warnings as errors])
  OPT_COMPILER_WERROR="default"
  AC_ARG_ENABLE(werror,
AS_HELP_STRING([--enable-werror],[Enable compiler warnings as errors])
AS_HELP_STRING([--disable-werror],[Disable compiler warnings as errors]),
  OPT_COMPILER_WERROR=$enableval)
  case "$OPT_COMPILER_WERROR" in
    no)
      dnl --disable-werror option used
      want_werror="no"
      ;;
    default)
      dnl configure option not specified
      want_werror="no"
      ;;
    *)
      dnl --enable-werror option used
      want_werror="yes"
      ;;
  esac
  AC_MSG_RESULT([$want_werror])
])


dnl CARES_CHECK_NONBLOCKING_SOCKET
dnl -------------------------------------------------
dnl Check for how to set a socket into non-blocking state.

AC_DEFUN([CARES_CHECK_NONBLOCKING_SOCKET], [
  AC_REQUIRE([CARES_CHECK_OPTION_NONBLOCKING])dnl
  AC_REQUIRE([CARES_CHECK_FUNC_FCNTL])dnl
  AC_REQUIRE([CARES_CHECK_FUNC_IOCTL])dnl
  AC_REQUIRE([CARES_CHECK_FUNC_IOCTLSOCKET])dnl
  AC_REQUIRE([CARES_CHECK_FUNC_IOCTLSOCKET_CAMEL])dnl
  AC_REQUIRE([CARES_CHECK_FUNC_SETSOCKOPT])dnl
  #
  tst_method="unknown"
  if test "$want_nonblocking" = "yes"; then
    AC_MSG_CHECKING([how to set a socket into non-blocking mode])
    if test "x$ac_cv_func_fcntl_o_nonblock" = "xyes"; then
      tst_method="fcntl O_NONBLOCK"
    elif test "x$ac_cv_func_ioctl_fionbio" = "xyes"; then
      tst_method="ioctl FIONBIO"
    elif test "x$ac_cv_func_ioctlsocket_fionbio" = "xyes"; then
      tst_method="ioctlsocket FIONBIO"
    elif test "x$ac_cv_func_ioctlsocket_camel_fionbio" = "xyes"; then
      tst_method="IoctlSocket FIONBIO"
    elif test "x$ac_cv_func_setsockopt_so_nonblock" = "xyes"; then
      tst_method="setsockopt SO_NONBLOCK"
    fi
    AC_MSG_RESULT([$tst_method])
    if test "$tst_method" = "unknown"; then
      AC_MSG_WARN([cannot determine non-blocking socket method.])
    fi
  fi
  if test "$tst_method" = "unknown"; then
    AC_DEFINE_UNQUOTED(USE_BLOCKING_SOCKETS, 1,
      [Define to disable non-blocking sockets.])
    AC_MSG_WARN([non-blocking sockets disabled.])
  fi
])


dnl CARES_CONFIGURE_SYMBOL_HIDING
dnl -------------------------------------------------
dnl Depending on --enable-symbol-hiding or --disable-symbol-hiding
dnl configure option, and compiler capability to actually honor such
dnl option, this will modify compiler flags as appropriate and also
dnl provide needed definitions for configuration and Makefile.am files.
dnl This macro should not be used until all compilation tests have
dnl been done to prevent interferences on other tests.

AC_DEFUN([CARES_CONFIGURE_SYMBOL_HIDING], [
  AC_MSG_CHECKING([whether hiding of library internal symbols will actually happen])
  CFLAG_CARES_SYMBOL_HIDING=""
  doing_symbol_hiding="no"
  if test x"$ac_cv_native_windows" != "xyes" &&
    test "$want_symbol_hiding" = "yes" &&
    test "$supports_symbol_hiding" = "yes"; then
    doing_symbol_hiding="yes"
    CFLAG_CARES_SYMBOL_HIDING="$symbol_hiding_CFLAGS"
    AC_DEFINE_UNQUOTED(CARES_SYMBOL_SCOPE_EXTERN, $symbol_hiding_EXTERN,
      [Definition to make a library symbol externally visible.])
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
  AM_CONDITIONAL(DOING_CARES_SYMBOL_HIDING, test x$doing_symbol_hiding = xyes)
  AC_SUBST(CFLAG_CARES_SYMBOL_HIDING)
  if test "$doing_symbol_hiding" = "yes"; then
    AC_DEFINE_UNQUOTED(CARES_SYMBOL_HIDING, 1,
      [Defined for build with symbol hiding.])
  fi
])

