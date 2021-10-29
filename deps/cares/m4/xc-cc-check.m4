#---------------------------------------------------------------------------
#
# xc-cc-check.m4
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


dnl _XC_PROG_CC_PREAMBLE
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_PROG_CC_PREAMBLE], [
  xc_prog_cc_prev_IFS=$IFS
  xc_prog_cc_prev_LIBS=$LIBS
  xc_prog_cc_prev_CFLAGS=$CFLAGS
  xc_prog_cc_prev_LDFLAGS=$LDFLAGS
  xc_prog_cc_prev_CPPFLAGS=$CPPFLAGS
])


dnl _XC_PROG_CC_POSTLUDE
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_PROG_CC_POSTLUDE], [
  IFS=$xc_prog_cc_prev_IFS
  LIBS=$xc_prog_cc_prev_LIBS
  CFLAGS=$xc_prog_cc_prev_CFLAGS
  LDFLAGS=$xc_prog_cc_prev_LDFLAGS
  CPPFLAGS=$xc_prog_cc_prev_CPPFLAGS
  AC_SUBST([CC])dnl
  AC_SUBST([CPP])dnl
  AC_SUBST([LIBS])dnl
  AC_SUBST([CFLAGS])dnl
  AC_SUBST([LDFLAGS])dnl
  AC_SUBST([CPPFLAGS])dnl
])


dnl _XC_PROG_CC
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_PROG_CC], [
  AC_REQUIRE([_XC_PROG_CC_PREAMBLE])dnl
  AC_REQUIRE([XC_CHECK_BUILD_FLAGS])dnl
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AM_PROG_CC_C_O])dnl
  AC_REQUIRE([AC_PROG_CPP])dnl
  AC_REQUIRE([_XC_PROG_CC_POSTLUDE])dnl
])


dnl XC_CHECK_PROG_CC
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl Checks for C compiler and C preprocessor programs,
dnl while doing some previous sanity validation on user
dnl provided LIBS, LDFLAGS, CPPFLAGS and CFLAGS values
dnl that must succeed in order to continue execution.
dnl
dnl This sets variables CC and CPP, while preventing
dnl LIBS, LDFLAGS, CFLAGS, CPPFLAGS and IFS from being
dnl unexpectedly changed by underlying macros.

AC_DEFUN([XC_CHECK_PROG_CC], [
  AC_PREREQ([2.50])dnl
  AC_BEFORE([$0],[_XC_PROG_CC_PREAMBLE])dnl
  AC_BEFORE([$0],[AC_PROG_INSTALL])dnl
  AC_BEFORE([$0],[AC_PROG_CC])dnl
  AC_BEFORE([$0],[AM_PROG_CC_C_O])dnl
  AC_BEFORE([$0],[AC_PROG_CPP])dnl
  AC_BEFORE([$0],[AC_PROG_LIBTOOL])dnl
  AC_BEFORE([$0],[AM_INIT_AUTOMAKE])dnl
  AC_BEFORE([$0],[_XC_PROG_CC_POSTLUDE])dnl
  AC_REQUIRE([_XC_PROG_CC])dnl
])

