# urlparse
#
# Copyright (c) 2024 urlparse contributors
# Copyright (c) 2023 sfparse contributors
# Copyright (c) 2019 nghttp3 contributors
# Copyright (c) 2016 ngtcp2 contributors
# Copyright (c) 2012 nghttp2 contributors
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
AC_PREREQ(2.61)
AC_INIT([urlparse], [0.0.0-DEV], [t-tujikawa@users.sourceforge.net])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

LT_PREREQ([2.2.6])
LT_INIT()

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# See versioning rule:
#  https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST(LT_CURRENT, 0)
AC_SUBST(LT_REVISION, 0)
AC_SUBST(LT_AGE, 0)

# Checks for command-line options
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror],
                    [Turn on compile time warnings])],
    [werror=$enableval], [werror=no])

# Checks for programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P

PKG_PROG_PKG_CONFIG([0.20])

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([ \
  assert.h \
  inttypes.h \
  stddef.h \
  stdint.h \
  stdlib.h \
  string.h \
  sys/types.h
])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# More compiler flags from nghttp2.
save_CFLAGS=$CFLAGS

CFLAGS=

if test "x$werror" != "xno"; then
    # For C compiler
    AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"])
    AX_CHECK_COMPILE_FLAG([-Wextra], [CFLAGS="$CFLAGS -Wextra"])
    AX_CHECK_COMPILE_FLAG([-Werror], [CFLAGS="$CFLAGS -Werror"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-prototypes], [CFLAGS="$CFLAGS -Wmissing-prototypes"])
    AX_CHECK_COMPILE_FLAG([-Wstrict-prototypes], [CFLAGS="$CFLAGS -Wstrict-prototypes"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-declarations], [CFLAGS="$CFLAGS -Wmissing-declarations"])
    AX_CHECK_COMPILE_FLAG([-Wpointer-arith], [CFLAGS="$CFLAGS -Wpointer-arith"])
    AX_CHECK_COMPILE_FLAG([-Wdeclaration-after-statement], [CFLAGS="$CFLAGS -Wdeclaration-after-statement"])
    AX_CHECK_COMPILE_FLAG([-Wformat-security], [CFLAGS="$CFLAGS -Wformat-security"])
    AX_CHECK_COMPILE_FLAG([-Wwrite-strings], [CFLAGS="$CFLAGS -Wwrite-strings"])
    AX_CHECK_COMPILE_FLAG([-Wshadow], [CFLAGS="$CFLAGS -Wshadow"])
    AX_CHECK_COMPILE_FLAG([-Winline], [CFLAGS="$CFLAGS -Winline"])
    AX_CHECK_COMPILE_FLAG([-Wnested-externs], [CFLAGS="$CFLAGS -Wnested-externs"])
    AX_CHECK_COMPILE_FLAG([-Wfloat-equal], [CFLAGS="$CFLAGS -Wfloat-equal"])
    AX_CHECK_COMPILE_FLAG([-Wundef], [CFLAGS="$CFLAGS -Wundef"])
    AX_CHECK_COMPILE_FLAG([-Wendif-labels], [CFLAGS="$CFLAGS -Wendif-labels"])
    AX_CHECK_COMPILE_FLAG([-Wempty-body], [CFLAGS="$CFLAGS -Wempty-body"])
    AX_CHECK_COMPILE_FLAG([-Wcast-align], [CFLAGS="$CFLAGS -Wcast-align"])
    AX_CHECK_COMPILE_FLAG([-Wclobbered], [CFLAGS="$CFLAGS -Wclobbered"])
    AX_CHECK_COMPILE_FLAG([-Wvla], [CFLAGS="$CFLAGS -Wvla"])
    AX_CHECK_COMPILE_FLAG([-Wpragmas], [CFLAGS="$CFLAGS -Wpragmas"])
    AX_CHECK_COMPILE_FLAG([-Wunreachable-code], [CFLAGS="$CFLAGS -Wunreachable-code"])
    AX_CHECK_COMPILE_FLAG([-Waddress], [CFLAGS="$CFLAGS -Waddress"])
    AX_CHECK_COMPILE_FLAG([-Wattributes], [CFLAGS="$CFLAGS -Wattributes"])
    AX_CHECK_COMPILE_FLAG([-Wdiv-by-zero], [CFLAGS="$CFLAGS -Wdiv-by-zero"])
    AX_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [CFLAGS="$CFLAGS -Wshorten-64-to-32"])

    AX_CHECK_COMPILE_FLAG([-Wconversion], [CFLAGS="$CFLAGS -Wconversion"])
    AX_CHECK_COMPILE_FLAG([-Wextended-offsetof], [CFLAGS="$CFLAGS -Wextended-offsetof"])
    AX_CHECK_COMPILE_FLAG([-Wformat-nonliteral], [CFLAGS="$CFLAGS -Wformat-nonliteral"])
    AX_CHECK_COMPILE_FLAG([-Wlanguage-extension-token], [CFLAGS="$CFLAGS -Wlanguage-extension-token"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-field-initializers], [CFLAGS="$CFLAGS -Wmissing-field-initializers"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-noreturn], [CFLAGS="$CFLAGS -Wmissing-noreturn"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-variable-declarations], [CFLAGS="$CFLAGS -Wmissing-variable-declarations"])
    # Not used because we cannot change public structs
    # AX_CHECK_COMPILE_FLAG([-Wpadded], [CFLAGS="$CFLAGS -Wpadded"])
    AX_CHECK_COMPILE_FLAG([-Wsign-conversion], [CFLAGS="$CFLAGS -Wsign-conversion"])
    # Not used because this basically disallows default case
    # AX_CHECK_COMPILE_FLAG([-Wswitch-enum], [CFLAGS="$CFLAGS -Wswitch-enum"])
    AX_CHECK_COMPILE_FLAG([-Wunreachable-code-break], [CFLAGS="$CFLAGS -Wunreachable-code-break"])
    AX_CHECK_COMPILE_FLAG([-Wunused-macros], [CFLAGS="$CFLAGS -Wunused-macros"])
    AX_CHECK_COMPILE_FLAG([-Wunused-parameter], [CFLAGS="$CFLAGS -Wunused-parameter"])
    AX_CHECK_COMPILE_FLAG([-Wredundant-decls], [CFLAGS="$CFLAGS -Wredundant-decls"])
    # Only work with Clang for the moment
    AX_CHECK_COMPILE_FLAG([-Wheader-guard], [CFLAGS="$CFLAGS -Wheader-guard"])
    AX_CHECK_COMPILE_FLAG([-Wsometimes-uninitialized], [CFLAGS="$CFLAGS -Wsometimes-uninitialized"])
    AX_CHECK_COMPILE_FLAG([-Wextra-semi], [CFLAGS="$CFLAGS -Wextra-semi"])

    # Only work with gcc7 for the moment
    AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [CFLAGS="$CFLAGS -Wduplicated-branches"])

    # This is required because we pass format string as "const char*.
    AX_CHECK_COMPILE_FLAG([-Wno-format-nonliteral], [CFLAGS="$CFLAGS -Wno-format-nonliteral"])
fi

WARNCFLAGS=$CFLAGS

CFLAGS=$save_CFLAGS

AC_SUBST([WARNCFLAGS])

AC_CONFIG_FILES([
  Makefile
  doc/Makefile
  doc/source/conf.py
])
AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

    Package version: ${VERSION}
    Library version: $LT_CURRENT:$LT_REVISION:$LT_AGE
    Install prefix:  ${prefix}
    System types:
      Build:         ${build}
      Host:          ${host}
      Target:        ${target}
    Compiler:
      C preprocessor: ${CPP}
      CPPFLAGS:       ${CPPFLAGS}
      C compiler:     ${CC}
      CFLAGS:         ${CFLAGS}
      LDFLAGS:        ${LDFLAGS}
      WARNCFLAGS:     ${WARNCFLAGS}
      LIBS:           ${LIBS}
    Library:
      Shared:         ${enable_shared}
      Static:         ${enable_static}
}')
])
