dnl Provide AC_USE_SYSTEM_EXTENSIONS for old autoconf machines.
AC_DEFUN([ACX_USE_SYSTEM_EXTENSIONS],[
  ifdef([AC_USE_SYSTEM_EXTENSIONS],[
    AC_USE_SYSTEM_EXTENSIONS
  ],[
    AC_BEFORE([$0], [AC_COMPILE_IFELSE])
    AC_BEFORE([$0], [AC_RUN_IFELSE])

    AC_REQUIRE([AC_GNU_SOURCE])
    AC_REQUIRE([AC_AIX])
    AC_REQUIRE([AC_MINIX])

    AH_VERBATIM([__EXTENSIONS__],
[/* Enable extensions on Solaris.  */
#ifndef __EXTENSIONS__
# undef __EXTENSIONS__
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
# undef _POSIX_PTHREAD_SEMANTICS
#endif
#ifndef _TANDEM_SOURCE
# undef _TANDEM_SOURCE
#endif])
    AC_CACHE_CHECK([whether it is safe to define __EXTENSIONS__],
      [ac_cv_safe_to_define___extensions__],
      [AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([
#           define __EXTENSIONS__ 1
            AC_INCLUDES_DEFAULT])],
         [ac_cv_safe_to_define___extensions__=yes],
         [ac_cv_safe_to_define___extensions__=no])])
    test $ac_cv_safe_to_define___extensions__ = yes &&
      AC_DEFINE([__EXTENSIONS__])
    AC_DEFINE([_POSIX_PTHREAD_SEMANTICS])
    AC_DEFINE([_TANDEM_SOURCE])
  ])
])
