dnl  Copyright (C) 2009 Sun Microsystems
dnl This file is free software; Sun Microsystems
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl ---------------------------------------------------------------------------
dnl Macro: PANDORA_ENABLE_DTRACE
dnl ---------------------------------------------------------------------------
AC_DEFUN([PANDORA_ENABLE_DTRACE],[
  AC_ARG_ENABLE([dtrace],
    [AS_HELP_STRING([--disable-dtrace],
            [enable DTrace USDT probes. @<:@default=yes@:>@])],
    [ac_cv_enable_dtrace="$enableval"],
    [ac_cv_enable_dtrace="yes"])

  AS_IF([test "$ac_cv_enable_dtrace" = "yes"],[
    AC_CHECK_PROGS([DTRACE], [dtrace])
    AS_IF([test "x$ac_cv_prog_DTRACE" = "xdtrace"],[

      AC_CACHE_CHECK([if dtrace works],[ac_cv_dtrace_works],[
        cat >conftest.d <<_ACEOF
provider Example {
  probe increment(int);
};
_ACEOF
        $DTRACE -h -o conftest.h -s conftest.d 2>/dev/zero
        AS_IF([test $? -eq 0],[ac_cv_dtrace_works=yes],
          [ac_cv_dtrace_works=no])
        rm -f conftest.h conftest.d
      ])
      AS_IF([test "x$ac_cv_dtrace_works" = "xyes"],[
        AC_DEFINE([HAVE_DTRACE], [1], [Enables DTRACE Support])
        AC_CACHE_CHECK([if dtrace should instrument object files],
          [ac_cv_dtrace_needs_objects],[
            dnl DTrace on MacOSX does not use -G option
            cat >conftest.d <<_ACEOF
provider Example {
  probe increment(int);
};
_ACEOF
          cat > conftest.c <<_ACEOF
#include "conftest.h"
void foo() {
  EXAMPLE_INCREMENT(1);
}
_ACEOF
            $DTRACE -h -o conftest.h -s conftest.d 2>/dev/zero
            $CC -c -o conftest.o conftest.c
            $DTRACE -G -o conftest.d.o -s conftest.d conftest.o 2>/dev/zero
            AS_IF([test $? -eq 0],[ac_cv_dtrace_needs_objects=yes],
              [ac_cv_dtrace_needs_objects=no])
            rm -f conftest.d.o conftest.d conftest.h conftest.o conftest.c
        ])
      ])
      AC_SUBST(DTRACEFLAGS) dnl TODO: test for -G on OSX
      ac_cv_have_dtrace=yes
    ])])

AM_CONDITIONAL([HAVE_DTRACE], [test "x$ac_cv_dtrace_works" = "xyes"])
AM_CONDITIONAL([DTRACE_NEEDS_OBJECTS],
               [test "x$ac_cv_dtrace_needs_objects" = "xyes"])

])
dnl ---------------------------------------------------------------------------
dnl End Macro: PANDORA_ENABLE_DTRACE
dnl ---------------------------------------------------------------------------
