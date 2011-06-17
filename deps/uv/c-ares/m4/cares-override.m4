#***************************************************************************
# $Id$
#***************************************************************************

# File version for 'aclocal' use. Keep it a single number.
# serial 3

dnl CARES_OVERRIDE_AUTOCONF
dnl -------------------------------------------------
dnl Placing a call to this macro in configure.ac after
dnl the one to AC_INIT will make macros in this file
dnl visible to the rest of the compilation overriding
dnl those from Autoconf.

AC_DEFUN([CARES_OVERRIDE_AUTOCONF], [
AC_BEFORE([$0],[AC_PROG_LIBTOOL])
# using cares-override.m4
])

dnl Override some Libtool tests
dnl -------------------------------------------------
dnl This is done to prevent Libtool 1.5.X from doing
dnl unnecesary C++, Fortran and Java tests and reduce
dnl resulting configure script by nearly 300 Kb.

m4_define([AC_LIBTOOL_LANG_CXX_CONFIG],[:])
m4_define([AC_LIBTOOL_LANG_F77_CONFIG],[:])
m4_define([AC_LIBTOOL_LANG_GCJ_CONFIG],[:])

dnl Override Autoconf's AC_LANG_PROGRAM (C)
dnl -------------------------------------------------
dnl This is done to prevent compiler warning
dnl 'function declaration isn't a prototype'
dnl in function main. This requires at least
dnl a c89 compiler and does not suport K&R.

m4_define([AC_LANG_PROGRAM(C)],
[$1
int main (void)
{
$2
 ;
 return 0;
}])

dnl Override Autoconf's AC_LANG_CALL (C)
dnl -------------------------------------------------
dnl This is a backport of Autoconf's 2.60 with the
dnl embedded comments that hit the resulting script
dnl removed. This is done to reduce configure size
dnl and use fixed macro across Autoconf versions.

m4_define([AC_LANG_CALL(C)],
[AC_LANG_PROGRAM([$1
m4_if([$2], [main], ,
[
#ifdef __cplusplus
extern "C"
#endif
char $2 ();])], [return $2 ();])])

dnl Override Autoconf's AC_LANG_FUNC_LINK_TRY (C)
dnl -------------------------------------------------
dnl This is a backport of Autoconf's 2.60 with the
dnl embedded comments that hit the resulting script
dnl removed. This is done to reduce configure size
dnl and use fixed macro across Autoconf versions.

m4_define([AC_LANG_FUNC_LINK_TRY(C)],
[AC_LANG_PROGRAM(
[
#define $1 innocuous_$1
#ifdef __STDC__
# include <limits.h>
#else
# include <assert.h>
#endif
#undef $1
#ifdef __cplusplus
extern "C"
#endif
char $1 ();
#if defined __stub_$1 || defined __stub___$1
choke me
#endif
], [return $1 ();])])

dnl Override Autoconf's PATH_SEPARATOR check
dnl -------------------------------------------------
dnl This is done to ensure that the same check is
dnl used across different Autoconf versions and to
dnl allow us to use this macro early enough in the
dnl configure script.

m4_defun([_AS_PATH_SEPARATOR_PREPARE],
[CARES_CHECK_PATH_SEPARATOR
m4_define([$0],[])])

m4_defun([_LT_AC_LIBTOOL_SYS_PATH_SEPARATOR],
[CARES_CHECK_PATH_SEPARATOR
m4_define([$0],[])])

