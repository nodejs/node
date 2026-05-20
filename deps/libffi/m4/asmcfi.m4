dnl ------------------------------------------------------------------
dnl  Check whether the assembler understands .cfi_* pseudo-ops.
dnl ------------------------------------------------------------------
AC_DEFUN([GCC_AS_CFI_PSEUDO_OP],
[
  AC_CACHE_CHECK([assembler .cfi pseudo-op support],
                 [gcc_cv_as_cfi_pseudo_op],
  [ AC_LANG_PUSH([C])
    AC_COMPILE_IFELSE(
      [AC_LANG_SOURCE([[
        #ifdef _MSC_VER
        Nope.
        #endif
        int foo (void)
        {
          __asm__ (".cfi_remember_state\n\t"
                   ".cfi_restore_state\n\t");
          return 0;
        }
      ]])],
      [gcc_cv_as_cfi_pseudo_op=yes],
      [gcc_cv_as_cfi_pseudo_op=no])
    AC_LANG_POP([C])
  ])

  if test "x$gcc_cv_as_cfi_pseudo_op" = xyes; then
    AC_DEFINE([HAVE_AS_CFI_PSEUDO_OP], [1],
              [Define if your assembler supports .cfi_* directives.])
  fi
])
