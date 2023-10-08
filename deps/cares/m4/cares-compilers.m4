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
# serial 75


dnl CARES_CHECK_COMPILER
dnl -------------------------------------------------
dnl Verify if the C compiler being used is known.

AC_DEFUN([CARES_CHECK_COMPILER], [
  #
  compiler_id="unknown"
  compiler_num="0"
  #
  flags_dbg_all="unknown"
  flags_dbg_yes="unknown"
  flags_dbg_off="unknown"
  flags_opt_all="unknown"
  flags_opt_yes="unknown"
  flags_opt_off="unknown"
  #
  flags_prefer_cppflags="no"
  #
  CARES_CHECK_COMPILER_DEC_C
  CARES_CHECK_COMPILER_HPUX_C
  CARES_CHECK_COMPILER_IBM_C
  CARES_CHECK_COMPILER_INTEL_C
  CARES_CHECK_COMPILER_CLANG
  CARES_CHECK_COMPILER_GNU_C
  CARES_CHECK_COMPILER_LCC
  CARES_CHECK_COMPILER_SGI_MIPSPRO_C
  CARES_CHECK_COMPILER_SGI_MIPS_C
  CARES_CHECK_COMPILER_SUNPRO_C
  CARES_CHECK_COMPILER_TINY_C
  CARES_CHECK_COMPILER_WATCOM_C
  #
  if test "$compiler_id" = "unknown"; then
  cat <<_EOF 1>&2
***
*** Warning: This configure script does not have information about the
*** compiler you are using, relative to the flags required to enable or
*** disable generation of debug info, optimization options or warnings.
***
*** Whatever settings are present in CFLAGS will be used for this run.
***
*** If you wish to help the c-ares project to better support your compiler
*** you can report this and the required info on the c-ares development
*** mailing list: http://lists.haxx.se/listinfo/c-ares/
***
_EOF
  fi
])


dnl CARES_CHECK_COMPILER_CLANG
dnl -------------------------------------------------
dnl Verify if compiler being used is clang.

AC_DEFUN([CARES_CHECK_COMPILER_CLANG], [
  AC_BEFORE([$0],[CARES_CHECK_COMPILER_GNU_C])dnl
  AC_MSG_CHECKING([if compiler is clang])
  CURL_CHECK_DEF([__clang__], [], [silent])
  if test "$curl_cv_have_def___clang__" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="CLANG"
    clangver=`$CC -dumpversion`
    clangvhi=`echo $clangver | cut -d . -f1`
    clangvlo=`echo $clangver | cut -d . -f2`
    compiler_num=`(expr $clangvhi "*" 100 + $clangvlo) 2>/dev/null`
    flags_dbg_all="-g -g0 -g1 -g2 -g3"
    flags_dbg_all="$flags_dbg_all -ggdb"
    flags_dbg_all="$flags_dbg_all -gstabs"
    flags_dbg_all="$flags_dbg_all -gstabs+"
    flags_dbg_all="$flags_dbg_all -gcoff"
    flags_dbg_all="$flags_dbg_all -gxcoff"
    flags_dbg_all="$flags_dbg_all -gdwarf-2"
    flags_dbg_all="$flags_dbg_all -gvms"
    flags_dbg_yes="-g"
    flags_dbg_off="-g0"
    flags_opt_all="-O -O0 -O1 -O2 -Os -O3 -O4"
    flags_opt_yes="-Os"
    flags_opt_off="-O0"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_DEC_C
dnl -------------------------------------------------
dnl Verify if compiler being used is DEC C.

AC_DEFUN([CARES_CHECK_COMPILER_DEC_C], [
  AC_MSG_CHECKING([if compiler is DEC/Compaq/HP C])
  CURL_CHECK_DEF([__DECC], [], [silent])
  CURL_CHECK_DEF([__DECC_VER], [], [silent])
  if test "$curl_cv_have_def___DECC" = "yes" &&
    test "$curl_cv_have_def___DECC_VER" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="DEC_C"
    flags_dbg_all="-g -g0 -g1 -g2 -g3"
    flags_dbg_yes="-g2"
    flags_dbg_off="-g0"
    flags_opt_all="-O -O0 -O1 -O2 -O3 -O4"
    flags_opt_yes="-O1"
    flags_opt_off="-O0"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_GNU_C
dnl -------------------------------------------------
dnl Verify if compiler being used is GNU C.

AC_DEFUN([CARES_CHECK_COMPILER_GNU_C], [
  AC_REQUIRE([CARES_CHECK_COMPILER_INTEL_C])dnl
  AC_REQUIRE([CARES_CHECK_COMPILER_CLANG])dnl
  AC_MSG_CHECKING([if compiler is GNU C])
  CURL_CHECK_DEF([__GNUC__], [], [silent])
  if test "$curl_cv_have_def___GNUC__" = "yes" &&
    test "$compiler_id" = "unknown"; then
    AC_MSG_RESULT([yes])
    compiler_id="GNU_C"
    gccver=`$CC -dumpversion`
    gccvhi=`echo $gccver | cut -d . -f1`
    gccvlo=`echo $gccver | cut -d . -f2`
    compiler_num=`(expr $gccvhi "*" 100 + $gccvlo) 2>/dev/null`
    flags_dbg_all="-g -g0 -g1 -g2 -g3"
    flags_dbg_all="$flags_dbg_all -ggdb"
    flags_dbg_all="$flags_dbg_all -gstabs"
    flags_dbg_all="$flags_dbg_all -gstabs+"
    flags_dbg_all="$flags_dbg_all -gcoff"
    flags_dbg_all="$flags_dbg_all -gxcoff"
    flags_dbg_all="$flags_dbg_all -gdwarf-2"
    flags_dbg_all="$flags_dbg_all -gvms"
    flags_dbg_yes="-g"
    flags_dbg_off="-g0"
    flags_opt_all="-O -O0 -O1 -O2 -O3 -Os"
    flags_opt_yes="-O2"
    flags_opt_off="-O0"
    CURL_CHECK_DEF([_WIN32], [], [silent])
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_HPUX_C
dnl -------------------------------------------------
dnl Verify if compiler being used is HP-UX C.

AC_DEFUN([CARES_CHECK_COMPILER_HPUX_C], [
  AC_MSG_CHECKING([if compiler is HP-UX C])
  CURL_CHECK_DEF([__HP_cc], [], [silent])
  if test "$curl_cv_have_def___HP_cc" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="HP_UX_C"
    flags_dbg_all="-g -s"
    flags_dbg_yes="-g"
    flags_dbg_off="-s"
    flags_opt_all="-O +O0 +O1 +O2 +O3 +O4"
    flags_opt_yes="+O2"
    flags_opt_off="+O0"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_IBM_C
dnl -------------------------------------------------
dnl Verify if compiler being used is IBM C.

AC_DEFUN([CARES_CHECK_COMPILER_IBM_C], [
  AC_MSG_CHECKING([if compiler is IBM C])
  CURL_CHECK_DEF([__IBMC__], [], [silent])
  if test "$curl_cv_have_def___IBMC__" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="IBM_C"
    flags_dbg_all="-g -g0 -g1 -g2 -g3"
    flags_dbg_yes="-g"
    flags_dbg_off=""
    flags_opt_all="-O -O0 -O1 -O2 -O3 -O4 -O5"
    flags_opt_all="$flags_opt_all -qnooptimize"
    flags_opt_all="$flags_opt_all -qoptimize=0"
    flags_opt_all="$flags_opt_all -qoptimize=1"
    flags_opt_all="$flags_opt_all -qoptimize=2"
    flags_opt_all="$flags_opt_all -qoptimize=3"
    flags_opt_all="$flags_opt_all -qoptimize=4"
    flags_opt_all="$flags_opt_all -qoptimize=5"
    flags_opt_yes="-O2"
    flags_opt_off="-qnooptimize"
    flags_prefer_cppflags="yes"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_INTEL_C
dnl -------------------------------------------------
dnl Verify if compiler being used is Intel C.

AC_DEFUN([CARES_CHECK_COMPILER_INTEL_C], [
  AC_BEFORE([$0],[CARES_CHECK_COMPILER_GNU_C])dnl
  AC_MSG_CHECKING([if compiler is Intel C])
  CURL_CHECK_DEF([__INTEL_COMPILER], [], [silent])
  if test "$curl_cv_have_def___INTEL_COMPILER" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_num="$curl_cv_def___INTEL_COMPILER"
    CURL_CHECK_DEF([__unix__], [], [silent])
    if test "$curl_cv_have_def___unix__" = "yes"; then
      compiler_id="INTEL_UNIX_C"
      flags_dbg_all="-g -g0"
      flags_dbg_yes="-g"
      flags_dbg_off="-g0"
      flags_opt_all="-O -O0 -O1 -O2 -O3 -Os"
      flags_opt_yes="-O2"
      flags_opt_off="-O0"
    else
      compiler_id="INTEL_WINDOWS_C"
      flags_dbg_all="/ZI /Zi /zI /zi /ZD /Zd /zD /zd /Z7 /z7 /Oy /Oy-"
      flags_dbg_all="$flags_dbg_all /debug"
      flags_dbg_all="$flags_dbg_all /debug:none"
      flags_dbg_all="$flags_dbg_all /debug:minimal"
      flags_dbg_all="$flags_dbg_all /debug:partial"
      flags_dbg_all="$flags_dbg_all /debug:full"
      flags_dbg_all="$flags_dbg_all /debug:semantic_stepping"
      flags_dbg_all="$flags_dbg_all /debug:extended"
      flags_dbg_yes="/Zi /Oy-"
      flags_dbg_off="/debug:none /Oy-"
      flags_opt_all="/O /O0 /O1 /O2 /O3 /Od /Og /Og- /Oi /Oi-"
      flags_opt_yes="/O2"
      flags_opt_off="/Od"
    fi
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_LCC
dnl -------------------------------------------------
dnl Verify if compiler being used is LCC.

AC_DEFUN([CARES_CHECK_COMPILER_LCC], [
  AC_MSG_CHECKING([if compiler is LCC])
  CURL_CHECK_DEF([__LCC__], [], [silent])
  if test "$curl_cv_have_def___LCC__" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="LCC"
    flags_dbg_all="-g"
    flags_dbg_yes="-g"
    flags_dbg_off=""
    flags_opt_all=""
    flags_opt_yes=""
    flags_opt_off=""
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_SGI_MIPS_C
dnl -------------------------------------------------
dnl Verify if compiler being used is SGI MIPS C.

AC_DEFUN([CARES_CHECK_COMPILER_SGI_MIPS_C], [
  AC_REQUIRE([CARES_CHECK_COMPILER_SGI_MIPSPRO_C])dnl
  AC_MSG_CHECKING([if compiler is SGI MIPS C])
  CURL_CHECK_DEF([__GNUC__], [], [silent])
  CURL_CHECK_DEF([__sgi], [], [silent])
  if test "$curl_cv_have_def___GNUC__" = "no" &&
    test "$curl_cv_have_def___sgi" = "yes" &&
    test "$compiler_id" = "unknown"; then
    AC_MSG_RESULT([yes])
    compiler_id="SGI_MIPS_C"
    flags_dbg_all="-g -g0 -g1 -g2 -g3"
    flags_dbg_yes="-g"
    flags_dbg_off="-g0"
    flags_opt_all="-O -O0 -O1 -O2 -O3 -Ofast"
    flags_opt_yes="-O2"
    flags_opt_off="-O0"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_SGI_MIPSPRO_C
dnl -------------------------------------------------
dnl Verify if compiler being used is SGI MIPSpro C.

AC_DEFUN([CARES_CHECK_COMPILER_SGI_MIPSPRO_C], [
  AC_BEFORE([$0],[CARES_CHECK_COMPILER_SGI_MIPS_C])dnl
  AC_MSG_CHECKING([if compiler is SGI MIPSpro C])
  CURL_CHECK_DEF([__GNUC__], [], [silent])
  CURL_CHECK_DEF([_COMPILER_VERSION], [], [silent])
  CURL_CHECK_DEF([_SGI_COMPILER_VERSION], [], [silent])
  if test "$curl_cv_have_def___GNUC__" = "no" &&
    (test "$curl_cv_have_def__SGI_COMPILER_VERSION" = "yes" ||
     test "$curl_cv_have_def__COMPILER_VERSION" = "yes"); then
    AC_MSG_RESULT([yes])
    compiler_id="SGI_MIPSPRO_C"
    flags_dbg_all="-g -g0 -g1 -g2 -g3"
    flags_dbg_yes="-g"
    flags_dbg_off="-g0"
    flags_opt_all="-O -O0 -O1 -O2 -O3 -Ofast"
    flags_opt_yes="-O2"
    flags_opt_off="-O0"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_SUNPRO_C
dnl -------------------------------------------------
dnl Verify if compiler being used is SunPro C.

AC_DEFUN([CARES_CHECK_COMPILER_SUNPRO_C], [
  AC_MSG_CHECKING([if compiler is SunPro C])
  CURL_CHECK_DEF([__SUNPRO_C], [], [silent])
  if test "$curl_cv_have_def___SUNPRO_C" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="SUNPRO_C"
    flags_dbg_all="-g -s"
    flags_dbg_yes="-g"
    flags_dbg_off="-s"
    flags_opt_all="-O -xO -xO1 -xO2 -xO3 -xO4 -xO5"
    flags_opt_yes="-xO2"
    flags_opt_off=""
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_TINY_C
dnl -------------------------------------------------
dnl Verify if compiler being used is Tiny C.

AC_DEFUN([CARES_CHECK_COMPILER_TINY_C], [
  AC_MSG_CHECKING([if compiler is Tiny C])
  CURL_CHECK_DEF([__TINYC__], [], [silent])
  if test "$curl_cv_have_def___TINYC__" = "yes"; then
    AC_MSG_RESULT([yes])
    compiler_id="TINY_C"
    flags_dbg_all="-g -b"
    flags_dbg_yes="-g"
    flags_dbg_off=""
    flags_opt_all=""
    flags_opt_yes=""
    flags_opt_off=""
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_WATCOM_C
dnl -------------------------------------------------
dnl Verify if compiler being used is Watcom C.

AC_DEFUN([CARES_CHECK_COMPILER_WATCOM_C], [
  AC_MSG_CHECKING([if compiler is Watcom C])
  CURL_CHECK_DEF([__WATCOMC__], [], [silent])
  if test "$curl_cv_have_def___WATCOMC__" = "yes"; then
    AC_MSG_RESULT([yes])
    CURL_CHECK_DEF([__UNIX__], [], [silent])
    if test "$curl_cv_have_def___UNIX__" = "yes"; then
      compiler_id="WATCOM_UNIX_C"
      flags_dbg_all="-g1 -g1+ -g2 -g3"
      flags_dbg_yes="-g2"
      flags_dbg_off=""
      flags_opt_all="-O0 -O1 -O2 -O3"
      flags_opt_yes="-O2"
      flags_opt_off="-O0"
    else
      compiler_id="WATCOM_WINDOWS_C"
      flags_dbg_all=""
      flags_dbg_yes=""
      flags_dbg_off=""
      flags_opt_all=""
      flags_opt_yes=""
      flags_opt_off=""
    fi
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CONVERT_INCLUDE_TO_ISYSTEM
dnl -------------------------------------------------
dnl Changes standard include paths present in CFLAGS
dnl and CPPFLAGS into isystem include paths. This is
dnl done to prevent GNUC from generating warnings on
dnl headers from these locations, although on ancient
dnl GNUC versions these warnings are not silenced.

AC_DEFUN([CARES_CONVERT_INCLUDE_TO_ISYSTEM], [
  AC_REQUIRE([CARES_SHFUNC_SQUEEZE])dnl
  AC_REQUIRE([CARES_CHECK_COMPILER])dnl
  if test "$compiler_id" = "GNU_C" ||
    test "$compiler_id" = "CLANG"; then
    tmp_has_include="no"
    tmp_chg_FLAGS="$CFLAGS"
    for word1 in $tmp_chg_FLAGS; do
      case "$word1" in
        -I*)
          tmp_has_include="yes"
          ;;
      esac
    done
    if test "$tmp_has_include" = "yes"; then
      tmp_chg_FLAGS=`echo "$tmp_chg_FLAGS" | "$SED" 's/^-I/ -isystem /g'`
      tmp_chg_FLAGS=`echo "$tmp_chg_FLAGS" | "$SED" 's/ -I/ -isystem /g'`
      CFLAGS="$tmp_chg_FLAGS"
      squeeze CFLAGS
    fi
    tmp_has_include="no"
    tmp_chg_FLAGS="$CPPFLAGS"
    for word1 in $tmp_chg_FLAGS; do
      case "$word1" in
        -I*)
          tmp_has_include="yes"
          ;;
      esac
    done
    if test "$tmp_has_include" = "yes"; then
      tmp_chg_FLAGS=`echo "$tmp_chg_FLAGS" | "$SED" 's/^-I/ -isystem /g'`
      tmp_chg_FLAGS=`echo "$tmp_chg_FLAGS" | "$SED" 's/ -I/ -isystem /g'`
      CPPFLAGS="$tmp_chg_FLAGS"
      squeeze CPPFLAGS
    fi
  fi
])


dnl CARES_COMPILER_WORKS_IFELSE ([ACTION-IF-WORKS], [ACTION-IF-NOT-WORKS])
dnl -------------------------------------------------
dnl Verify if the C compiler seems to work with the
dnl settings that are 'active' at the time the test
dnl is performed.

AC_DEFUN([CARES_COMPILER_WORKS_IFELSE], [
  dnl compilation capability verification
  tmp_compiler_works="unknown"
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
      int i = 1;
      return i;
    ]])
  ],[
    tmp_compiler_works="yes"
  ],[
    tmp_compiler_works="no"
    echo " " >&6
    sed 's/^/cc-fail: /' conftest.err >&6
    echo " " >&6
  ])
  dnl linking capability verification
  if test "$tmp_compiler_works" = "yes"; then
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([[
      ]],[[
        int i = 1;
        return i;
      ]])
    ],[
      tmp_compiler_works="yes"
    ],[
      tmp_compiler_works="no"
      echo " " >&6
      sed 's/^/link-fail: /' conftest.err >&6
      echo " " >&6
    ])
  fi
  dnl only do runtime verification when not cross-compiling
  if test "x$cross_compiling" != "xyes" &&
    test "$tmp_compiler_works" = "yes"; then
    AC_RUN_IFELSE([
      AC_LANG_PROGRAM([[
#       ifdef __STDC__
#         include <stdlib.h>
#       endif
      ]],[[
        int i = 0;
        exit(i);
      ]])
    ],[
      tmp_compiler_works="yes"
    ],[
      tmp_compiler_works="no"
      echo " " >&6
      echo "run-fail: test program exited with status $ac_status" >&6
      echo " " >&6
    ])
  fi
  dnl branch upon test result
  if test "$tmp_compiler_works" = "yes"; then
  ifelse($1,,:,[$1])
  ifelse($2,,,[else
    $2])
  fi
])


dnl CARES_SET_COMPILER_BASIC_OPTS
dnl -------------------------------------------------
dnl Sets compiler specific options/flags which do not
dnl depend on configure's debug, optimize or warnings
dnl options.

AC_DEFUN([CARES_SET_COMPILER_BASIC_OPTS], [
  AC_REQUIRE([CARES_CHECK_COMPILER])dnl
  AC_REQUIRE([CARES_SHFUNC_SQUEEZE])dnl
  #
  if test "$compiler_id" != "unknown"; then
    #
    if test "$compiler_id" = "GNU_C" ||
      test "$compiler_id" = "CLANG"; then
      CARES_CONVERT_INCLUDE_TO_ISYSTEM
    fi
    #
    tmp_save_CPPFLAGS="$CPPFLAGS"
    tmp_save_CFLAGS="$CFLAGS"
    tmp_CPPFLAGS=""
    tmp_CFLAGS=""
    #
    case "$compiler_id" in
        #
      CLANG)
        #
        dnl Disable warnings for unused arguments, otherwise clang will
        dnl warn about compile-time arguments used during link-time, like
        dnl -O and -g and -pedantic.
        tmp_CFLAGS="$tmp_CFLAGS -Qunused-arguments"
        ;;
        #
      DEC_C)
        #
        dnl Select strict ANSI C compiler mode
        tmp_CFLAGS="$tmp_CFLAGS -std1"
        dnl Turn off optimizer ANSI C aliasing rules
        tmp_CFLAGS="$tmp_CFLAGS -noansi_alias"
        dnl Generate warnings for missing function prototypes
        tmp_CFLAGS="$tmp_CFLAGS -warnprotos"
        dnl Change some warnings into fatal errors
        tmp_CFLAGS="$tmp_CFLAGS -msg_fatal toofewargs,toomanyargs"
        ;;
        #
      GNU_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      HP_UX_C)
        #
        dnl Disallow run-time dereferencing of null pointers
        tmp_CFLAGS="$tmp_CFLAGS -z"
        dnl Disable some remarks
        dnl #4227: padding struct with n bytes to align member
        dnl #4255: padding size of struct with n bytes to alignment boundary
        tmp_CFLAGS="$tmp_CFLAGS +W 4227,4255"
        ;;
        #
      IBM_C)
        #
        dnl Ensure that compiler optimizations are always thread-safe.
        tmp_CPPFLAGS="$tmp_CPPFLAGS -qthreaded"
        dnl Disable type based strict aliasing optimizations, using worst
        dnl case aliasing assumptions when compiling. Type based aliasing
        dnl would restrict the lvalues that could be safely used to access
        dnl a data object.
        tmp_CPPFLAGS="$tmp_CPPFLAGS -qnoansialias"
        dnl Force compiler to stop after the compilation phase, without
        dnl generating an object code file when compilation has errors.
        tmp_CPPFLAGS="$tmp_CPPFLAGS -qhalt=e"
        ;;
        #
      INTEL_UNIX_C)
        #
        dnl On unix this compiler uses gcc's header files, so
        dnl we select ANSI C89 dialect plus GNU extensions.
        tmp_CFLAGS="$tmp_CFLAGS -std=gnu89"
        dnl Change some warnings into errors
        dnl #140: too many arguments in function call
        dnl #147: declaration is incompatible with 'previous one'
        dnl #165: too few arguments in function call
        dnl #266: function declared implicitly
        tmp_CPPFLAGS="$tmp_CPPFLAGS -diag-error 140,147,165,266"
        dnl Disable some remarks
        dnl #279: controlling expression is constant
        dnl #981: operands are evaluated in unspecified order
        dnl #1469: "cc" clobber ignored
        tmp_CPPFLAGS="$tmp_CPPFLAGS -diag-disable 279,981,1469"
        ;;
        #
      INTEL_WINDOWS_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      LCC)
        #
        dnl Disallow run-time dereferencing of null pointers
        tmp_CFLAGS="$tmp_CFLAGS -n"
        ;;
        #
      SGI_MIPS_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      SGI_MIPSPRO_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      SUNPRO_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      TINY_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      WATCOM_UNIX_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      WATCOM_WINDOWS_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
    esac
    #
    squeeze tmp_CPPFLAGS
    squeeze tmp_CFLAGS
    #
    if test ! -z "$tmp_CFLAGS" || test ! -z "$tmp_CPPFLAGS"; then
      AC_MSG_CHECKING([if compiler accepts some basic options])
      CPPFLAGS="$tmp_save_CPPFLAGS $tmp_CPPFLAGS"
      CFLAGS="$tmp_save_CFLAGS $tmp_CFLAGS"
      squeeze CPPFLAGS
      squeeze CFLAGS
      CARES_COMPILER_WORKS_IFELSE([
        AC_MSG_RESULT([yes])
        AC_MSG_NOTICE([compiler options added: $tmp_CFLAGS $tmp_CPPFLAGS])
      ],[
        AC_MSG_RESULT([no])
        AC_MSG_WARN([compiler options rejected: $tmp_CFLAGS $tmp_CPPFLAGS])
        dnl restore initial settings
        CPPFLAGS="$tmp_save_CPPFLAGS"
        CFLAGS="$tmp_save_CFLAGS"
      ])
    fi
    #
  fi
])


dnl CARES_SET_COMPILER_DEBUG_OPTS
dnl -------------------------------------------------
dnl Sets compiler specific options/flags which depend
dnl on configure's debug option.

AC_DEFUN([CARES_SET_COMPILER_DEBUG_OPTS], [
  AC_REQUIRE([CARES_CHECK_OPTION_DEBUG])dnl
  AC_REQUIRE([CARES_CHECK_COMPILER])dnl
  AC_REQUIRE([CARES_SHFUNC_SQUEEZE])dnl
  #
  if test "$compiler_id" != "unknown"; then
    #
    tmp_save_CFLAGS="$CFLAGS"
    tmp_save_CPPFLAGS="$CPPFLAGS"
    #
    tmp_options=""
    tmp_CFLAGS="$CFLAGS"
    tmp_CPPFLAGS="$CPPFLAGS"
    CARES_VAR_STRIP([tmp_CFLAGS],[$flags_dbg_all])
    CARES_VAR_STRIP([tmp_CPPFLAGS],[$flags_dbg_all])
    #
    if test "$want_debug" = "yes"; then
      AC_MSG_CHECKING([if compiler accepts debug enabling options])
      tmp_options="$flags_dbg_yes"
    fi
    if test "$want_debug" = "no"; then
      AC_MSG_CHECKING([if compiler accepts debug disabling options])
      tmp_options="$flags_dbg_off"
    fi
    #
    if test "$flags_prefer_cppflags" = "yes"; then
      CPPFLAGS="$tmp_CPPFLAGS $tmp_options"
      CFLAGS="$tmp_CFLAGS"
    else
      CPPFLAGS="$tmp_CPPFLAGS"
      CFLAGS="$tmp_CFLAGS $tmp_options"
    fi
    squeeze CPPFLAGS
    squeeze CFLAGS
    CARES_COMPILER_WORKS_IFELSE([
      AC_MSG_RESULT([yes])
      AC_MSG_NOTICE([compiler options added: $tmp_options])
    ],[
      AC_MSG_RESULT([no])
      AC_MSG_WARN([compiler options rejected: $tmp_options])
      dnl restore initial settings
      CPPFLAGS="$tmp_save_CPPFLAGS"
      CFLAGS="$tmp_save_CFLAGS"
    ])
    #
  fi
])


dnl CARES_SET_COMPILER_OPTIMIZE_OPTS
dnl -------------------------------------------------
dnl Sets compiler specific options/flags which depend
dnl on configure's optimize option.

AC_DEFUN([CARES_SET_COMPILER_OPTIMIZE_OPTS], [
  AC_REQUIRE([CARES_CHECK_OPTION_OPTIMIZE])dnl
  AC_REQUIRE([CARES_CHECK_COMPILER])dnl
  AC_REQUIRE([CARES_SHFUNC_SQUEEZE])dnl
  #
  if test "$compiler_id" != "unknown"; then
    #
    tmp_save_CFLAGS="$CFLAGS"
    tmp_save_CPPFLAGS="$CPPFLAGS"
    #
    tmp_options=""
    tmp_CFLAGS="$CFLAGS"
    tmp_CPPFLAGS="$CPPFLAGS"
    honor_optimize_option="yes"
    #
    dnl If optimization request setting has not been explicitly specified,
    dnl it has been derived from the debug setting and initially assumed.
    dnl This initially assumed optimizer setting will finally be ignored
    dnl if CFLAGS or CPPFLAGS already hold optimizer flags. This implies
    dnl that an initially assumed optimizer setting might not be honored.
    #
    if test "$want_optimize" = "assume_no" ||
       test "$want_optimize" = "assume_yes"; then
      AC_MSG_CHECKING([if compiler optimizer assumed setting might be used])
      CARES_VAR_MATCH_IFELSE([tmp_CFLAGS],[$flags_opt_all],[
        honor_optimize_option="no"
      ])
      CARES_VAR_MATCH_IFELSE([tmp_CPPFLAGS],[$flags_opt_all],[
        honor_optimize_option="no"
      ])
      AC_MSG_RESULT([$honor_optimize_option])
      if test "$honor_optimize_option" = "yes"; then
        if test "$want_optimize" = "assume_yes"; then
          want_optimize="yes"
        fi
        if test "$want_optimize" = "assume_no"; then
          want_optimize="no"
        fi
      fi
    fi
    #
    if test "$honor_optimize_option" = "yes"; then
      CARES_VAR_STRIP([tmp_CFLAGS],[$flags_opt_all])
      CARES_VAR_STRIP([tmp_CPPFLAGS],[$flags_opt_all])
      if test "$want_optimize" = "yes"; then
        AC_MSG_CHECKING([if compiler accepts optimizer enabling options])
        tmp_options="$flags_opt_yes"
      fi
      if test "$want_optimize" = "no"; then
        AC_MSG_CHECKING([if compiler accepts optimizer disabling options])
        tmp_options="$flags_opt_off"
      fi
      if test "$flags_prefer_cppflags" = "yes"; then
        CPPFLAGS="$tmp_CPPFLAGS $tmp_options"
        CFLAGS="$tmp_CFLAGS"
      else
        CPPFLAGS="$tmp_CPPFLAGS"
        CFLAGS="$tmp_CFLAGS $tmp_options"
      fi
      squeeze CPPFLAGS
      squeeze CFLAGS
      CARES_COMPILER_WORKS_IFELSE([
        AC_MSG_RESULT([yes])
        AC_MSG_NOTICE([compiler options added: $tmp_options])
      ],[
        AC_MSG_RESULT([no])
        AC_MSG_WARN([compiler options rejected: $tmp_options])
        dnl restore initial settings
        CPPFLAGS="$tmp_save_CPPFLAGS"
        CFLAGS="$tmp_save_CFLAGS"
      ])
    fi
    #
  fi
])


dnl CARES_SET_COMPILER_WARNING_OPTS
dnl -------------------------------------------------
dnl Sets compiler options/flags which depend on
dnl configure's warnings given option.

AC_DEFUN([CARES_SET_COMPILER_WARNING_OPTS], [
  AC_REQUIRE([CARES_CHECK_OPTION_WARNINGS])dnl
  AC_REQUIRE([CARES_CHECK_COMPILER])dnl
  AC_REQUIRE([CARES_SHFUNC_SQUEEZE])dnl
  #
  if test "$compiler_id" != "unknown"; then
    #
    tmp_save_CPPFLAGS="$CPPFLAGS"
    tmp_save_CFLAGS="$CFLAGS"
    tmp_CPPFLAGS=""
    tmp_CFLAGS=""
    #
    case "$compiler_id" in
        #
      CLANG)
        #
        if test "$want_warnings" = "yes"; then
          tmp_CFLAGS="$tmp_CFLAGS -pedantic"
          tmp_CFLAGS="$tmp_CFLAGS -Wall -Wextra"
          tmp_CFLAGS="$tmp_CFLAGS -Wpointer-arith -Wwrite-strings"
          tmp_CFLAGS="$tmp_CFLAGS -Wshadow"
          tmp_CFLAGS="$tmp_CFLAGS -Winline -Wnested-externs"
          tmp_CFLAGS="$tmp_CFLAGS -Wmissing-declarations"
          tmp_CFLAGS="$tmp_CFLAGS -Wmissing-prototypes"
          tmp_CFLAGS="$tmp_CFLAGS -Wno-long-long"
          tmp_CFLAGS="$tmp_CFLAGS -Wfloat-equal"
          tmp_CFLAGS="$tmp_CFLAGS -Wno-multichar -Wsign-compare"
          tmp_CFLAGS="$tmp_CFLAGS -Wundef"
          tmp_CFLAGS="$tmp_CFLAGS -Wno-format-nonliteral"
          tmp_CFLAGS="$tmp_CFLAGS -Wendif-labels -Wstrict-prototypes"
          tmp_CFLAGS="$tmp_CFLAGS -Wdeclaration-after-statement"
          tmp_CFLAGS="$tmp_CFLAGS -Wcast-align"
          tmp_CFLAGS="$tmp_CFLAGS -Wno-system-headers"
          tmp_CFLAGS="$tmp_CFLAGS -Wshorten-64-to-32"
          #
          dnl Only clang 1.1 or later
          if test "$compiler_num" -ge "101"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wunused"
          fi
        fi
        ;;
        #
      DEC_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Select a higher warning level than default level2
          tmp_CFLAGS="$tmp_CFLAGS -msg_enable level3"
        fi
        ;;
        #
      GNU_C)
        #
        if test "$want_warnings" = "yes"; then
          #
          dnl Do not enable -pedantic when cross-compiling with a gcc older
          dnl than 3.0, to avoid warnings from third party system headers.
          if test "x$cross_compiling" != "xyes" ||
            test "$compiler_num" -ge "300"; then
            tmp_CFLAGS="$tmp_CFLAGS -pedantic"
          fi
          #
          dnl Set of options we believe *ALL* gcc versions support:
          tmp_CFLAGS="$tmp_CFLAGS -Wall -W"
          #
          dnl Only gcc 1.4 or later
          if test "$compiler_num" -ge "104"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wpointer-arith -Wwrite-strings"
            dnl If not cross-compiling with a gcc older than 3.0
            if test "x$cross_compiling" != "xyes" ||
              test "$compiler_num" -ge "300"; then
              tmp_CFLAGS="$tmp_CFLAGS -Wunused -Wshadow"
            fi
          fi
          #
          dnl Only gcc 2.7 or later
          if test "$compiler_num" -ge "207"; then
            tmp_CFLAGS="$tmp_CFLAGS -Winline -Wnested-externs"
            dnl If not cross-compiling with a gcc older than 3.0
            if test "x$cross_compiling" != "xyes" ||
              test "$compiler_num" -ge "300"; then
              tmp_CFLAGS="$tmp_CFLAGS -Wmissing-declarations"
              tmp_CFLAGS="$tmp_CFLAGS -Wmissing-prototypes"
            fi
          fi
          #
          dnl Only gcc 2.95 or later
          if test "$compiler_num" -ge "295"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wno-long-long"
          fi
          #
          dnl Only gcc 2.96 or later
          if test "$compiler_num" -ge "296"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wfloat-equal"
            tmp_CFLAGS="$tmp_CFLAGS -Wno-multichar -Wsign-compare"
            dnl -Wundef used only if gcc is 2.96 or later since we get
            dnl lots of "`_POSIX_C_SOURCE' is not defined" in system
            dnl headers with gcc 2.95.4 on FreeBSD 4.9
            tmp_CFLAGS="$tmp_CFLAGS -Wundef"
          fi
          #
          dnl Only gcc 2.97 or later
          if test "$compiler_num" -ge "297"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wno-format-nonliteral"
          fi
          #
          dnl Only gcc 3.0 or later
          if test "$compiler_num" -ge "300"; then
            dnl -Wunreachable-code seems totally unreliable on my gcc 3.3.2 on
            dnl on i686-Linux as it gives us heaps with false positives.
            dnl Also, on gcc 4.0.X it is totally unbearable and complains all
            dnl over making it unusable for generic purposes. Let's not use it.
            tmp_CFLAGS="$tmp_CFLAGS"
          fi
          #
          dnl Only gcc 3.3 or later
          if test "$compiler_num" -ge "303"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wendif-labels -Wstrict-prototypes"
          fi
          #
          dnl Only gcc 3.4 or later
          if test "$compiler_num" -ge "304"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wdeclaration-after-statement"
          fi
          #
          dnl Only gcc 4.0 or later
          if test "$compiler_num" -ge "400"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wstrict-aliasing=3"
          fi
          #
          dnl Only gcc 4.2 or later
          if test "$compiler_num" -ge "402"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wcast-align"
          fi
          #
          dnl Only gcc 4.3 or later
          if test "$compiler_num" -ge "403"; then
            tmp_CFLAGS="$tmp_CFLAGS -Wtype-limits -Wold-style-declaration"
            tmp_CFLAGS="$tmp_CFLAGS -Wmissing-parameter-type -Wempty-body"
            tmp_CFLAGS="$tmp_CFLAGS -Wclobbered -Wignored-qualifiers"
            tmp_CFLAGS="$tmp_CFLAGS -Wconversion -Wno-sign-conversion -Wvla"
          fi
          #
          dnl Only gcc 4.5 or later
          if test "$compiler_num" -ge "405"; then
            dnl Only windows targets
            if test "$curl_cv_have_def__WIN32" = "yes"; then
              tmp_CFLAGS="$tmp_CFLAGS -Wno-pedantic-ms-format"
            fi
          fi
          #
        fi
        #
        dnl Do not issue warnings for code in system include paths.
        if test "$compiler_num" -ge "300"; then
          tmp_CFLAGS="$tmp_CFLAGS -Wno-system-headers"
        else
          dnl When cross-compiling with a gcc older than 3.0, disable
          dnl some warnings triggered on third party system headers.
          if test "x$cross_compiling" = "xyes"; then
            if test "$compiler_num" -ge "104"; then
              dnl gcc 1.4 or later
              tmp_CFLAGS="$tmp_CFLAGS -Wno-unused -Wno-shadow"
            fi
            if test "$compiler_num" -ge "207"; then
              dnl gcc 2.7 or later
              tmp_CFLAGS="$tmp_CFLAGS -Wno-missing-declarations"
              tmp_CFLAGS="$tmp_CFLAGS -Wno-missing-prototypes"
            fi
          fi
        fi
        ;;
        #
      HP_UX_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Issue all warnings
          tmp_CFLAGS="$tmp_CFLAGS +w1"
        fi
        ;;
        #
      IBM_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      INTEL_UNIX_C)
        #
        if test "$want_warnings" = "yes"; then
          if test "$compiler_num" -gt "600"; then
            dnl Show errors, warnings, and remarks
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wall -w2"
            dnl Perform extra compile-time code checking
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wcheck"
            dnl Warn on nested comments
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wcomment"
            dnl Show warnings relative to deprecated features
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wdeprecated"
            dnl Enable warnings for missing prototypes
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wmissing-prototypes"
            dnl Enable warnings for 64-bit portability issues
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wp64"
            dnl Enable warnings for questionable pointer arithmetic
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wpointer-arith"
            dnl Check for function return typw issues
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wreturn-type"
            dnl Warn on variable declarations hiding a previous one
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wshadow"
            dnl Warn when a variable is used before initialized
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wuninitialized"
            dnl Warn if a declared function is not used
            tmp_CPPFLAGS="$tmp_CPPFLAGS -Wunused-function"
          fi
        fi
        dnl Disable using EBP register in optimizations
        tmp_CFLAGS="$tmp_CFLAGS -fno-omit-frame-pointer"
        dnl Disable use of ANSI C aliasing rules in optimizations
        tmp_CFLAGS="$tmp_CFLAGS -fno-strict-aliasing"
        dnl Value-safe optimizations on floating-point data
        tmp_CFLAGS="$tmp_CFLAGS -fp-model precise"
        dnl Only icc 10.0 or later
        if test "$compiler_num" -ge "1000"; then
          dnl Disable vectorizer diagnostic information
          tmp_CFLAGS="$tmp_CFLAGS -vec-report0"
        fi
        ;;
        #
      INTEL_WINDOWS_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
      LCC)
        #
        if test "$want_warnings" = "yes"; then
          dnl Highest warning level is double -A, next is single -A.
          dnl Due to the big number of warnings these trigger on third
          dnl party header files it is impractical for us to use any of
          dnl them here. If you want them simply define it in CPPFLAGS.
          tmp_CFLAGS="$tmp_CFLAGS"
        fi
        ;;
        #
      SGI_MIPS_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Perform stricter semantic and lint-like checks
          tmp_CFLAGS="$tmp_CFLAGS -fullwarn"
        fi
        ;;
        #
      SGI_MIPSPRO_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Perform stricter semantic and lint-like checks
          tmp_CFLAGS="$tmp_CFLAGS -fullwarn"
          dnl Disable some remarks
          dnl #1209: controlling expression is constant
          tmp_CFLAGS="$tmp_CFLAGS -woff 1209"
        fi
        ;;
        #
      SUNPRO_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Perform stricter semantic and lint-like checks
          tmp_CFLAGS="$tmp_CFLAGS -v"
        fi
        ;;
        #
      TINY_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Activate all warnings
          tmp_CFLAGS="$tmp_CFLAGS -Wall"
          dnl Make string constants be of type const char *
          tmp_CFLAGS="$tmp_CFLAGS -Wwrite-strings"
          dnl Warn use of unsupported GCC features ignored by TCC
          tmp_CFLAGS="$tmp_CFLAGS -Wunsupported"
        fi
        ;;
        #
      WATCOM_UNIX_C)
        #
        if test "$want_warnings" = "yes"; then
          dnl Issue all warnings
          tmp_CFLAGS="$tmp_CFLAGS -Wall -Wextra"
        fi
        ;;
        #
      WATCOM_WINDOWS_C)
        #
        dnl Placeholder
        tmp_CFLAGS="$tmp_CFLAGS"
        ;;
        #
    esac
    #
    squeeze tmp_CPPFLAGS
    squeeze tmp_CFLAGS
    #
    if test ! -z "$tmp_CFLAGS" || test ! -z "$tmp_CPPFLAGS"; then
      AC_MSG_CHECKING([if compiler accepts strict warning options])
      CPPFLAGS="$tmp_save_CPPFLAGS $tmp_CPPFLAGS"
      CFLAGS="$tmp_save_CFLAGS $tmp_CFLAGS"
      squeeze CPPFLAGS
      squeeze CFLAGS
      CARES_COMPILER_WORKS_IFELSE([
        AC_MSG_RESULT([yes])
        AC_MSG_NOTICE([compiler options added: $tmp_CFLAGS $tmp_CPPFLAGS])
      ],[
        AC_MSG_RESULT([no])
        AC_MSG_WARN([compiler options rejected: $tmp_CFLAGS $tmp_CPPFLAGS])
        dnl restore initial settings
        CPPFLAGS="$tmp_save_CPPFLAGS"
        CFLAGS="$tmp_save_CFLAGS"
      ])
    fi
    #
  fi
])


dnl CARES_SHFUNC_SQUEEZE
dnl -------------------------------------------------
dnl Declares a shell function squeeze() which removes
dnl redundant whitespace out of a shell variable.

AC_DEFUN([CARES_SHFUNC_SQUEEZE], [
squeeze() {
  _sqz_result=""
  eval _sqz_input=\[$][$]1
  for _sqz_token in $_sqz_input; do
    if test -z "$_sqz_result"; then
      _sqz_result="$_sqz_token"
    else
      _sqz_result="$_sqz_result $_sqz_token"
    fi
  done
  eval [$]1=\$_sqz_result
  return 0
}
])


dnl CARES_CHECK_COMPILER_HALT_ON_ERROR
dnl -------------------------------------------------
dnl Verifies if the compiler actually halts after the
dnl compilation phase without generating any object
dnl code file, when the source compiles with errors.

AC_DEFUN([CARES_CHECK_COMPILER_HALT_ON_ERROR], [
  AC_MSG_CHECKING([if compiler halts on compilation errors])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
      force compilation error
    ]])
  ],[
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([compiler does not halt on compilation errors.])
  ],[
    AC_MSG_RESULT([yes])
  ])
])


dnl CARES_CHECK_COMPILER_ARRAY_SIZE_NEGATIVE
dnl -------------------------------------------------
dnl Verifies if the compiler actually halts after the
dnl compilation phase without generating any object
dnl code file, when the source code tries to define a
dnl type for a constant array with negative dimension.

AC_DEFUN([CARES_CHECK_COMPILER_ARRAY_SIZE_NEGATIVE], [
  AC_REQUIRE([CARES_CHECK_COMPILER_HALT_ON_ERROR])dnl
  AC_MSG_CHECKING([if compiler halts on negative sized arrays])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
      typedef char bad_t[sizeof(char) == sizeof(int) ? -1 : -1 ];
    ]],[[
      bad_t dummy;
    ]])
  ],[
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([compiler does not halt on negative sized arrays.])
  ],[
    AC_MSG_RESULT([yes])
  ])
])


dnl CARES_CHECK_COMPILER_STRUCT_MEMBER_SIZE
dnl -------------------------------------------------
dnl Verifies if the compiler is capable of handling the
dnl size of a struct member, struct which is a function
dnl result, as a compilation-time condition inside the
dnl type definition of a constant array.

AC_DEFUN([CARES_CHECK_COMPILER_STRUCT_MEMBER_SIZE], [
  AC_REQUIRE([CARES_CHECK_COMPILER_ARRAY_SIZE_NEGATIVE])dnl
  AC_MSG_CHECKING([if compiler struct member size checking works])
  tst_compiler_check_one_works="unknown"
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
      struct mystruct {
        int  mi;
        char mc;
        struct mystruct *next;
      };
      struct mystruct myfunc();
      typedef char good_t1[sizeof(myfunc().mi) == sizeof(int)  ? 1 : -1 ];
      typedef char good_t2[sizeof(myfunc().mc) == sizeof(char) ? 1 : -1 ];
    ]],[[
      good_t1 dummy1;
      good_t2 dummy2;
    ]])
  ],[
    tst_compiler_check_one_works="yes"
  ],[
    tst_compiler_check_one_works="no"
    sed 's/^/cc-src: /' conftest.$ac_ext >&6
    sed 's/^/cc-err: /' conftest.err >&6
  ])
  tst_compiler_check_two_works="unknown"
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
      struct mystruct {
        int  mi;
        char mc;
        struct mystruct *next;
      };
      struct mystruct myfunc();
      typedef char bad_t1[sizeof(myfunc().mi) != sizeof(int)  ? 1 : -1 ];
      typedef char bad_t2[sizeof(myfunc().mc) != sizeof(char) ? 1 : -1 ];
    ]],[[
      bad_t1 dummy1;
      bad_t2 dummy2;
    ]])
  ],[
    tst_compiler_check_two_works="no"
  ],[
    tst_compiler_check_two_works="yes"
  ])
  if test "$tst_compiler_check_one_works" = "yes" &&
    test "$tst_compiler_check_two_works" = "yes"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([compiler fails struct member size checking.])
  fi
])


dnl CARES_CHECK_COMPILER_SYMBOL_HIDING
dnl -------------------------------------------------
dnl Verify if compiler supports hiding library internal symbols, setting
dnl shell variable supports_symbol_hiding value as appropriate, as well as
dnl variables symbol_hiding_CFLAGS and symbol_hiding_EXTERN when supported.

AC_DEFUN([CARES_CHECK_COMPILER_SYMBOL_HIDING], [
  AC_REQUIRE([CARES_CHECK_COMPILER])dnl
  AC_BEFORE([$0],[CARES_CONFIGURE_SYMBOL_HIDING])dnl
  AC_MSG_CHECKING([if compiler supports hiding library internal symbols])
  supports_symbol_hiding="no"
  symbol_hiding_CFLAGS=""
  symbol_hiding_EXTERN=""
  tmp_CFLAGS=""
  tmp_EXTERN=""
  case "$compiler_id" in
    CLANG)
      dnl All versions of clang support -fvisibility=
      tmp_EXTERN="__attribute__ ((__visibility__ (\"default\")))"
      tmp_CFLAGS="-fvisibility=hidden"
      supports_symbol_hiding="yes"
      ;;
    GNU_C)
      dnl Only gcc 3.4 or later
      if test "$compiler_num" -ge "304"; then
        if $CC --help --verbose 2>&1 | grep fvisibility= > /dev/null ; then
          tmp_EXTERN="__attribute__ ((__visibility__ (\"default\")))"
          tmp_CFLAGS="-fvisibility=hidden"
          supports_symbol_hiding="yes"
        fi
      fi
      ;;
    INTEL_UNIX_C)
      dnl Only icc 9.0 or later
      if test "$compiler_num" -ge "900"; then
        if $CC --help --verbose 2>&1 | grep fvisibility= > /dev/null ; then
          tmp_save_CFLAGS="$CFLAGS"
          CFLAGS="$CFLAGS -fvisibility=hidden"
          AC_LINK_IFELSE([
            AC_LANG_PROGRAM([[
#             include <stdio.h>
            ]],[[
              printf("icc fvisibility bug test");
            ]])
          ],[
            tmp_EXTERN="__attribute__ ((__visibility__ (\"default\")))"
            tmp_CFLAGS="-fvisibility=hidden"
            supports_symbol_hiding="yes"
          ])
          CFLAGS="$tmp_save_CFLAGS"
        fi
      fi
      ;;
    SUNPRO_C)
      if $CC 2>&1 | grep flags >/dev/null && $CC -flags | grep xldscope= >/dev/null ; then
        tmp_EXTERN="__global"
        tmp_CFLAGS="-xldscope=hidden"
        supports_symbol_hiding="yes"
      fi
      ;;
  esac
  if test "$supports_symbol_hiding" = "yes"; then
    tmp_save_CFLAGS="$CFLAGS"
    CFLAGS="$tmp_save_CFLAGS $tmp_CFLAGS"
    squeeze CFLAGS
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
        $tmp_EXTERN char *dummy(char *buff);
        char *dummy(char *buff)
        {
         if(buff)
           return ++buff;
         else
           return buff;
        }
      ]],[[
        char b[16];
        char *r = dummy(&b[0]);
        if(r)
          return (int)*r;
      ]])
    ],[
      supports_symbol_hiding="yes"
      if test -f conftest.err; then
        grep 'visibility' conftest.err >/dev/null
        if test "$?" -eq "0"; then
          supports_symbol_hiding="no"
        fi
      fi
    ],[
      supports_symbol_hiding="no"
      echo " " >&6
      sed 's/^/cc-src: /' conftest.$ac_ext >&6
      sed 's/^/cc-err: /' conftest.err >&6
      echo " " >&6
    ])
    CFLAGS="$tmp_save_CFLAGS"
  fi
  if test "$supports_symbol_hiding" = "yes"; then
    AC_MSG_RESULT([yes])
    symbol_hiding_CFLAGS="$tmp_CFLAGS"
    symbol_hiding_EXTERN="$tmp_EXTERN"
  else
    AC_MSG_RESULT([no])
  fi
])


dnl CARES_CHECK_COMPILER_PROTOTYPE_MISMATCH
dnl -------------------------------------------------
dnl Verifies if the compiler actually halts after the
dnl compilation phase without generating any object
dnl code file, when the source code tries to redefine
dnl a prototype which does not match previous one.

AC_DEFUN([CARES_CHECK_COMPILER_PROTOTYPE_MISMATCH], [
  AC_REQUIRE([CARES_CHECK_COMPILER_HALT_ON_ERROR])dnl
  AC_MSG_CHECKING([if compiler halts on function prototype mismatch])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
#     include <stdlib.h>
      int rand(int n);
      int rand(int n)
      {
        if(n)
          return ++n;
        else
          return n;
      }
    ]],[[
      int i[2];
      int j = rand(i[0]);
      if(j)
        return j;
    ]])
  ],[
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([compiler does not halt on function prototype mismatch.])
  ],[
    AC_MSG_RESULT([yes])
  ])
])


dnl CARES_VAR_MATCH (VARNAME, VALUE)
dnl -------------------------------------------------
dnl Verifies if shell variable VARNAME contains VALUE.
dnl Contents of variable VARNAME and VALUE are handled
dnl as whitespace separated lists of words. If at least
dnl one word of VALUE is present in VARNAME the match
dnl is considered positive, otherwise false.

AC_DEFUN([CARES_VAR_MATCH], [
  ac_var_match_word="no"
  for word1 in $[$1]; do
    for word2 in [$2]; do
      if test "$word1" = "$word2"; then
        ac_var_match_word="yes"
      fi
    done
  done
])


dnl CARES_VAR_MATCH_IFELSE (VARNAME, VALUE,
dnl                        [ACTION-IF-MATCH], [ACTION-IF-NOT-MATCH])
dnl -------------------------------------------------
dnl This performs a CURL_VAR_MATCH check and executes
dnl first branch if the match is positive, otherwise
dnl the second branch is executed.

AC_DEFUN([CARES_VAR_MATCH_IFELSE], [
  CARES_VAR_MATCH([$1],[$2])
  if test "$ac_var_match_word" = "yes"; then
  ifelse($3,,:,[$3])
  ifelse($4,,,[else
    $4])
  fi
])


dnl CARES_VAR_STRIP (VARNAME, VALUE)
dnl -------------------------------------------------
dnl Contents of variable VARNAME and VALUE are handled
dnl as whitespace separated lists of words. Each word
dnl from VALUE is removed from VARNAME when present.

AC_DEFUN([CARES_VAR_STRIP], [
  AC_REQUIRE([CARES_SHFUNC_SQUEEZE])dnl
  ac_var_stripped=""
  for word1 in $[$1]; do
    ac_var_strip_word="no"
    for word2 in [$2]; do
      if test "$word1" = "$word2"; then
        ac_var_strip_word="yes"
      fi
    done
    if test "$ac_var_strip_word" = "no"; then
      ac_var_stripped="$ac_var_stripped $word1"
    fi
  done
  dnl squeeze whitespace out of result
  [$1]="$ac_var_stripped"
  squeeze [$1]
])

