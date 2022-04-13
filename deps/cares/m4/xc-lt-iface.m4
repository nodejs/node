#---------------------------------------------------------------------------
#
# xc-lt-iface.m4
#
# Copyright (c) 2013 Daniel Stenberg <daniel@haxx.se>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#---------------------------------------------------------------------------

# serial 1


dnl _XC_LIBTOOL_PREAMBLE
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks some configure script options related with
dnl libtool and customizes its default behavior before
dnl libtool code is actually used in script.

m4_define([_XC_LIBTOOL_PREAMBLE],
[dnl
# ------------------------------------ #
#  Determine libtool default behavior  #
# ------------------------------------ #

#
# Default behavior is to enable shared and static libraries on systems
# where libtool knows how to build both library versions, and does not
# require separate configuration and build runs for each flavor.
#

xc_lt_want_enable_shared='yes'
xc_lt_want_enable_static='yes'

#
# User may have disabled shared or static libraries.
#
case "x$enable_shared" in @%:@ (
  xno)
    xc_lt_want_enable_shared='no'
    ;;
esac
case "x$enable_static" in @%:@ (
  xno)
    xc_lt_want_enable_static='no'
    ;;
esac
if test "x$xc_lt_want_enable_shared" = 'xno' &&
  test "x$xc_lt_want_enable_static" = 'xno'; then
  AC_MSG_ERROR([can not disable shared and static libraries simultaneously])
fi

#
# Default behavior on systems that require independent configuration
# and build runs for shared and static is to enable shared libraries
# and disable static ones. On these systems option '--disable-shared'
# must be used in order to build a proper static library.
#

if test "x$xc_lt_want_enable_shared" = 'xyes' &&
  test "x$xc_lt_want_enable_static" = 'xyes'; then
  case $host_os in @%:@ (
    mingw* | pw32* | cegcc* | os2* | aix*)
      xc_lt_want_enable_static='no'
      ;;
  esac
fi

#
# Make libtool aware of current shared and static library preferences
# taking in account that, depending on host characteristics, libtool
# may modify these option preferences later in this configure script.
#

enable_shared=$xc_lt_want_enable_shared
enable_static=$xc_lt_want_enable_static

#
# Default behavior is to build PIC objects for shared libraries and
# non-PIC objects for static libraries.
#

xc_lt_want_with_pic='default'

#
# User may have specified PIC preference.
#

case "x$with_pic" in @%:@ ((
  xno)
    xc_lt_want_with_pic='no'
    ;;
  xyes)
    xc_lt_want_with_pic='yes'
    ;;
esac

#
# Default behavior on some systems where building a shared library out
# of non-PIC compiled objects will fail with following linker error
# "relocation R_X86_64_32 can not be used when making a shared object"
# is to build PIC objects even for static libraries. This behavior may
# be overriden using 'configure --disable-shared --without-pic'.
#

if test "x$xc_lt_want_with_pic" = 'xdefault'; then
  case $host_cpu in @%:@ (
    x86_64 | amd64 | ia64)
      case $host_os in @%:@ (
        linux* | freebsd*)
          xc_lt_want_with_pic='yes'
          ;;
      esac
      ;;
  esac
fi

#
# Make libtool aware of current PIC preference taking in account that,
# depending on host characteristics, libtool may modify PIC default
# behavior to fit host system idiosyncrasies later in this script.
#

with_pic=$xc_lt_want_with_pic
dnl
m4_define([$0],[])dnl
])


dnl _XC_LIBTOOL_BODY
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl This macro performs embedding of libtool code into
dnl configure script, regardless of libtool version in
dnl use when generating configure script.

m4_define([_XC_LIBTOOL_BODY],
[dnl
## ----------------------- ##
##  Start of libtool code  ##
## ----------------------- ##
m4_ifdef([LT_INIT],
[dnl
LT_INIT([win32-dll])
],[dnl
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
])dnl
## --------------------- ##
##  End of libtool code  ##
## --------------------- ##
dnl
m4_define([$0], [])[]dnl
])


dnl _XC_CHECK_LT_BUILD_LIBRARIES
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks wether libtool shared and static libraries
dnl are finally built depending on user input, default
dnl behavior and knowledge that libtool has about host
dnl characteristics.
dnl Results stored in following shell variables:
dnl   xc_lt_build_shared
dnl   xc_lt_build_static

m4_define([_XC_CHECK_LT_BUILD_LIBRARIES],
[dnl
#
# Verify if finally libtool shared libraries will be built
#

case "x$enable_shared" in @%:@ ((
  xyes | xno)
    xc_lt_build_shared=$enable_shared
    ;;
  *)
    AC_MSG_ERROR([unexpected libtool enable_shared value: $enable_shared])
    ;;
esac

#
# Verify if finally libtool static libraries will be built
#

case "x$enable_static" in @%:@ ((
  xyes | xno)
    xc_lt_build_static=$enable_static
    ;;
  *)
    AC_MSG_ERROR([unexpected libtool enable_static value: $enable_static])
    ;;
esac
dnl
m4_define([$0],[])dnl
])


dnl _XC_CHECK_LT_SHLIB_USE_VERSION_INFO
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks if the -version-info linker flag must be
dnl provided when building libtool shared libraries.
dnl Result stored in xc_lt_shlib_use_version_info.

m4_define([_XC_CHECK_LT_SHLIB_USE_VERSION_INFO],
[dnl
#
# Verify if libtool shared libraries should be linked using flag -version-info
#

AC_MSG_CHECKING([whether to build shared libraries with -version-info])
xc_lt_shlib_use_version_info='yes'
if test "x$version_type" = 'xnone'; then
  xc_lt_shlib_use_version_info='no'
fi
case $host_os in @%:@ (
  amigaos*)
    xc_lt_shlib_use_version_info='yes'
    ;;
esac
AC_MSG_RESULT([$xc_lt_shlib_use_version_info])
dnl
m4_define([$0], [])[]dnl
])


dnl _XC_CHECK_LT_SHLIB_USE_NO_UNDEFINED
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks if the -no-undefined linker flag must be
dnl provided when building libtool shared libraries.
dnl Result stored in xc_lt_shlib_use_no_undefined.

m4_define([_XC_CHECK_LT_SHLIB_USE_NO_UNDEFINED],
[dnl
#
# Verify if libtool shared libraries should be linked using flag -no-undefined
#

AC_MSG_CHECKING([whether to build shared libraries with -no-undefined])
xc_lt_shlib_use_no_undefined='no'
if test "x$allow_undefined" = 'xno'; then
  xc_lt_shlib_use_no_undefined='yes'
elif test "x$allow_undefined_flag" = 'xunsupported'; then
  xc_lt_shlib_use_no_undefined='yes'
fi
case $host_os in @%:@ (
  cygwin* | mingw* | pw32* | cegcc* | os2* | aix*)
    xc_lt_shlib_use_no_undefined='yes'
    ;;
esac
AC_MSG_RESULT([$xc_lt_shlib_use_no_undefined])
dnl
m4_define([$0], [])[]dnl
])


dnl _XC_CHECK_LT_SHLIB_USE_MIMPURE_TEXT
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks if the -mimpure-text linker flag must be
dnl provided when building libtool shared libraries.
dnl Result stored in xc_lt_shlib_use_mimpure_text.

m4_define([_XC_CHECK_LT_SHLIB_USE_MIMPURE_TEXT],
[dnl
#
# Verify if libtool shared libraries should be linked using flag -mimpure-text
#

AC_MSG_CHECKING([whether to build shared libraries with -mimpure-text])
xc_lt_shlib_use_mimpure_text='no'
case $host_os in @%:@ (
  solaris2*)
    if test "x$GCC" = 'xyes'; then
      xc_lt_shlib_use_mimpure_text='yes'
    fi
    ;;
esac
AC_MSG_RESULT([$xc_lt_shlib_use_mimpure_text])
dnl
m4_define([$0], [])[]dnl
])


dnl _XC_CHECK_LT_BUILD_WITH_PIC
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks wether libtool shared and static libraries
dnl would be built with PIC depending on user input,
dnl default behavior and knowledge that libtool has
dnl about host characteristics.
dnl Results stored in following shell variables:
dnl   xc_lt_build_shared_with_pic
dnl   xc_lt_build_static_with_pic

m4_define([_XC_CHECK_LT_BUILD_WITH_PIC],
[dnl
#
# Find out wether libtool libraries would be built wit PIC
#

case "x$pic_mode" in @%:@ ((((
  xdefault)
    xc_lt_build_shared_with_pic='yes'
    xc_lt_build_static_with_pic='no'
    ;;
  xyes)
    xc_lt_build_shared_with_pic='yes'
    xc_lt_build_static_with_pic='yes'
    ;;
  xno)
    xc_lt_build_shared_with_pic='no'
    xc_lt_build_static_with_pic='no'
    ;;
  *)
    xc_lt_build_shared_with_pic='unknown'
    xc_lt_build_static_with_pic='unknown'
    AC_MSG_WARN([unexpected libtool pic_mode value: $pic_mode])
    ;;
esac
AC_MSG_CHECKING([whether to build shared libraries with PIC])
AC_MSG_RESULT([$xc_lt_build_shared_with_pic])
AC_MSG_CHECKING([whether to build static libraries with PIC])
AC_MSG_RESULT([$xc_lt_build_static_with_pic])
dnl
m4_define([$0],[])dnl
])


dnl _XC_CHECK_LT_BUILD_SINGLE_VERSION
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Checks wether a libtool shared or static library
dnl is finally built exclusively without the other.
dnl Results stored in following shell variables:
dnl   xc_lt_build_shared_only
dnl   xc_lt_build_static_only

m4_define([_XC_CHECK_LT_BUILD_SINGLE_VERSION],
[dnl
#
# Verify if libtool shared libraries will be built while static not built
#

AC_MSG_CHECKING([whether to build shared libraries only])
if test "$xc_lt_build_shared" = 'yes' &&
  test "$xc_lt_build_static" = 'no'; then
  xc_lt_build_shared_only='yes'
else
  xc_lt_build_shared_only='no'
fi
AC_MSG_RESULT([$xc_lt_build_shared_only])

#
# Verify if libtool static libraries will be built while shared not built
#

AC_MSG_CHECKING([whether to build static libraries only])
if test "$xc_lt_build_static" = 'yes' &&
  test "$xc_lt_build_shared" = 'no'; then
  xc_lt_build_static_only='yes'
else
  xc_lt_build_static_only='no'
fi
AC_MSG_RESULT([$xc_lt_build_static_only])
dnl
m4_define([$0],[])dnl
])


dnl _XC_LIBTOOL_POSTLUDE
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Performs several checks related with libtool that
dnl can not be done unless libtool code has already
dnl been executed. See individual check descriptions
dnl for further info.

m4_define([_XC_LIBTOOL_POSTLUDE],
[dnl
_XC_CHECK_LT_BUILD_LIBRARIES
_XC_CHECK_LT_SHLIB_USE_VERSION_INFO
_XC_CHECK_LT_SHLIB_USE_NO_UNDEFINED
_XC_CHECK_LT_SHLIB_USE_MIMPURE_TEXT
_XC_CHECK_LT_BUILD_WITH_PIC
_XC_CHECK_LT_BUILD_SINGLE_VERSION
dnl
m4_define([$0],[])dnl
])


dnl XC_LIBTOOL
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl This macro embeds libtool machinery into configure
dnl script, regardless of libtool version, and performs
dnl several additional checks whose results can be used
dnl later on.
dnl
dnl Usage of this macro ensures that generated configure
dnl script uses equivalent logic irrespective of autoconf
dnl or libtool version being used to generate configure
dnl script.
dnl
dnl Results stored in following shell variables:
dnl   xc_lt_build_shared
dnl   xc_lt_build_static
dnl   xc_lt_shlib_use_version_info
dnl   xc_lt_shlib_use_no_undefined
dnl   xc_lt_shlib_use_mimpure_text
dnl   xc_lt_build_shared_with_pic
dnl   xc_lt_build_static_with_pic
dnl   xc_lt_build_shared_only
dnl   xc_lt_build_static_only

AC_DEFUN([XC_LIBTOOL],
[dnl
AC_PREREQ([2.50])dnl
dnl
AC_BEFORE([$0],[LT_INIT])dnl
AC_BEFORE([$0],[AC_PROG_LIBTOOL])dnl
AC_BEFORE([$0],[AC_LIBTOOL_WIN32_DLL])dnl
dnl
AC_REQUIRE([XC_CHECK_PATH_SEPARATOR])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_PROG_CC])dnl
dnl
_XC_LIBTOOL_PREAMBLE
_XC_LIBTOOL_BODY
_XC_LIBTOOL_POSTLUDE
dnl
m4_ifdef([AC_LIBTOOL_WIN32_DLL],
  [m4_undefine([AC_LIBTOOL_WIN32_DLL])])dnl
m4_ifdef([AC_PROG_LIBTOOL],
  [m4_undefine([AC_PROG_LIBTOOL])])dnl
m4_ifdef([LT_INIT],
  [m4_undefine([LT_INIT])])dnl
dnl
m4_define([$0],[])dnl
])

