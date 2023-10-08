#---------------------------------------------------------------------------
#
# xc-val-flgs.m4
#
# Copyright (c) Daniel Stenberg <daniel@haxx.se>
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
#
# SPDX-License-Identifier: MIT
#---------------------------------------------------------------------------

# serial 1


dnl _XC_CHECK_VAR_LIBS
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_CHECK_VAR_LIBS], [
  xc_bad_var_libs=no
  for xc_word in $LIBS; do
    case "$xc_word" in
      -l* | --library=*)
        :
        ;;
      *)
        xc_bad_var_libs=yes
        ;;
    esac
  done
  if test $xc_bad_var_libs = yes; then
    AC_MSG_NOTICE([using LIBS: $LIBS])
    AC_MSG_NOTICE([LIBS error: LIBS may only be used to specify libraries (-lname).])
  fi
])


dnl _XC_CHECK_VAR_LDFLAGS
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_CHECK_VAR_LDFLAGS], [
  xc_bad_var_ldflags=no
  for xc_word in $LDFLAGS; do
    case "$xc_word" in
      -D*)
        xc_bad_var_ldflags=yes
        ;;
      -U*)
        xc_bad_var_ldflags=yes
        ;;
      -I*)
        xc_bad_var_ldflags=yes
        ;;
      -l* | --library=*)
        xc_bad_var_ldflags=yes
        ;;
    esac
  done
  if test $xc_bad_var_ldflags = yes; then
    AC_MSG_NOTICE([using LDFLAGS: $LDFLAGS])
    xc_bad_var_msg="LDFLAGS error: LDFLAGS may only be used to specify linker flags, not"
    for xc_word in $LDFLAGS; do
      case "$xc_word" in
        -D*)
          AC_MSG_NOTICE([$xc_bad_var_msg macro definitions. Use CPPFLAGS for: $xc_word])
          ;;
        -U*)
          AC_MSG_NOTICE([$xc_bad_var_msg macro suppressions. Use CPPFLAGS for: $xc_word])
          ;;
        -I*)
          AC_MSG_NOTICE([$xc_bad_var_msg include directories. Use CPPFLAGS for: $xc_word])
          ;;
        -l* | --library=*)
          AC_MSG_NOTICE([$xc_bad_var_msg libraries. Use LIBS for: $xc_word])
          ;;
      esac
    done
  fi
])


dnl _XC_CHECK_VAR_CPPFLAGS
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_CHECK_VAR_CPPFLAGS], [
  xc_bad_var_cppflags=no
  for xc_word in $CPPFLAGS; do
    case "$xc_word" in
      -rpath*)
        xc_bad_var_cppflags=yes
        ;;
      -L* | --library-path=*)
        xc_bad_var_cppflags=yes
        ;;
      -l* | --library=*)
        xc_bad_var_cppflags=yes
        ;;
    esac
  done
  if test $xc_bad_var_cppflags = yes; then
    AC_MSG_NOTICE([using CPPFLAGS: $CPPFLAGS])
    xc_bad_var_msg="CPPFLAGS error: CPPFLAGS may only be used to specify C preprocessor flags, not"
    for xc_word in $CPPFLAGS; do
      case "$xc_word" in
        -rpath*)
          AC_MSG_NOTICE([$xc_bad_var_msg library runtime directories. Use LDFLAGS for: $xc_word])
          ;;
        -L* | --library-path=*)
          AC_MSG_NOTICE([$xc_bad_var_msg library directories. Use LDFLAGS for: $xc_word])
          ;;
        -l* | --library=*)
          AC_MSG_NOTICE([$xc_bad_var_msg libraries. Use LIBS for: $xc_word])
          ;;
      esac
    done
  fi
])


dnl _XC_CHECK_VAR_CFLAGS
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_CHECK_VAR_CFLAGS], [
  xc_bad_var_cflags=no
  for xc_word in $CFLAGS; do
    case "$xc_word" in
      -D*)
        xc_bad_var_cflags=yes
        ;;
      -U*)
        xc_bad_var_cflags=yes
        ;;
      -I*)
        xc_bad_var_cflags=yes
        ;;
      -rpath*)
        xc_bad_var_cflags=yes
        ;;
      -L* | --library-path=*)
        xc_bad_var_cflags=yes
        ;;
      -l* | --library=*)
        xc_bad_var_cflags=yes
        ;;
    esac
  done
  if test $xc_bad_var_cflags = yes; then
    AC_MSG_NOTICE([using CFLAGS: $CFLAGS])
    xc_bad_var_msg="CFLAGS error: CFLAGS may only be used to specify C compiler flags, not"
    for xc_word in $CFLAGS; do
      case "$xc_word" in
        -D*)
          AC_MSG_NOTICE([$xc_bad_var_msg macro definitions. Use CPPFLAGS for: $xc_word])
          ;;
        -U*)
          AC_MSG_NOTICE([$xc_bad_var_msg macro suppressions. Use CPPFLAGS for: $xc_word])
          ;;
        -I*)
          AC_MSG_NOTICE([$xc_bad_var_msg include directories. Use CPPFLAGS for: $xc_word])
          ;;
        -rpath*)
          AC_MSG_NOTICE([$xc_bad_var_msg library runtime directories. Use LDFLAGS for: $xc_word])
          ;;
        -L* | --library-path=*)
          AC_MSG_NOTICE([$xc_bad_var_msg library directories. Use LDFLAGS for: $xc_word])
          ;;
        -l* | --library=*)
          AC_MSG_NOTICE([$xc_bad_var_msg libraries. Use LIBS for: $xc_word])
          ;;
      esac
    done
  fi
])


dnl XC_CHECK_USER_FLAGS
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl Performs some sanity checks for LIBS, LDFLAGS,
dnl CPPFLAGS and CFLAGS values that the user might
dnl have set. When checks fails, user is noticed
dnl about errors detected in all of them and script
dnl execution is halted.
dnl
dnl Intended to be used early in configure script.

AC_DEFUN([XC_CHECK_USER_FLAGS], [
  AC_PREREQ([2.50])dnl
  AC_BEFORE([$0],[XC_CHECK_PROG_CC])dnl
  dnl check order below matters
  _XC_CHECK_VAR_LIBS
  _XC_CHECK_VAR_LDFLAGS
  _XC_CHECK_VAR_CPPFLAGS
  _XC_CHECK_VAR_CFLAGS
  if test $xc_bad_var_libs = yes ||
     test $xc_bad_var_cflags = yes ||
     test $xc_bad_var_ldflags = yes ||
     test $xc_bad_var_cppflags = yes; then
     AC_MSG_ERROR([Can not continue. Fix errors mentioned immediately above this line.])
  fi
])


dnl XC_CHECK_BUILD_FLAGS
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl Performs some sanity checks for LIBS, LDFLAGS,
dnl CPPFLAGS and CFLAGS values that the configure
dnl script might have set. When checks fails, user
dnl is noticed about errors detected in all of them
dnl but script continues execution.
dnl
dnl Intended to be used very late in configure script.

AC_DEFUN([XC_CHECK_BUILD_FLAGS], [
  AC_PREREQ([2.50])dnl
  dnl check order below matters
  _XC_CHECK_VAR_LIBS
  _XC_CHECK_VAR_LDFLAGS
  _XC_CHECK_VAR_CPPFLAGS
  _XC_CHECK_VAR_CFLAGS
  if test $xc_bad_var_libs = yes ||
     test $xc_bad_var_cflags = yes ||
     test $xc_bad_var_ldflags = yes ||
     test $xc_bad_var_cppflags = yes; then
     AC_MSG_WARN([Continuing even with errors mentioned immediately above this line.])
  fi
])

