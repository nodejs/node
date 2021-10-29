#***************************************************************************
# $Id$
#
# Copyright (C) 2008 - 2009 by Daniel Stenberg et al
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
#***************************************************************************

# File version for 'aclocal' use. Keep it a single number.
# serial 6

dnl Note 1
dnl ------
dnl None of the CARES_CHECK_NEED_REENTRANT_* macros shall use HAVE_FOO_H to
dnl conditionally include header files. These macros are used early in the
dnl configure process much before header file availability is known.


dnl CARES_CHECK_NEED_REENTRANT_ERRNO
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes errno available as a preprocessor macro.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_ERRNO], [
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
#include <errno.h>
    ]],[[
      if(0 != errno)
        return 1;
    ]])
  ],[
    tmp_errno="yes"
  ],[
    tmp_errno="no"
  ])
  if test "$tmp_errno" = "yes"; then
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include <errno.h>
      ]],[[
#ifdef errno
        int dummy=1;
#else
        force compilation error
#endif
      ]])
    ],[
      tmp_errno="errno_macro_defined"
    ],[
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM([[
#define _REENTRANT
#include <errno.h>
        ]],[[
#ifdef errno
          int dummy=1;
#else
          force compilation error
#endif
        ]])
      ],[
        tmp_errno="errno_macro_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_GMTIME_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function gmtime_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_GMTIME_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([gmtime_r])
  ],[
    tmp_gmtime_r="yes"
  ],[
    tmp_gmtime_r="no"
  ])
  if test "$tmp_gmtime_r" = "yes"; then
    AC_EGREP_CPP([gmtime_r],[
#include <sys/types.h>
#include <time.h>
    ],[
      tmp_gmtime_r="proto_declared"
    ],[
      AC_EGREP_CPP([gmtime_r],[
#define _REENTRANT
#include <sys/types.h>
#include <time.h>
      ],[
        tmp_gmtime_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_LOCALTIME_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function localtime_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_LOCALTIME_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([localtime_r])
  ],[
    tmp_localtime_r="yes"
  ],[
    tmp_localtime_r="no"
  ])
  if test "$tmp_localtime_r" = "yes"; then
    AC_EGREP_CPP([localtime_r],[
#include <sys/types.h>
#include <time.h>
    ],[
      tmp_localtime_r="proto_declared"
    ],[
      AC_EGREP_CPP([localtime_r],[
#define _REENTRANT
#include <sys/types.h>
#include <time.h>
      ],[
        tmp_localtime_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_STRERROR_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function strerror_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_STRERROR_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strerror_r])
  ],[
    tmp_strerror_r="yes"
  ],[
    tmp_strerror_r="no"
  ])
  if test "$tmp_strerror_r" = "yes"; then
    AC_EGREP_CPP([strerror_r],[
#include <sys/types.h>
#include <string.h>
    ],[
      tmp_strerror_r="proto_declared"
    ],[
      AC_EGREP_CPP([strerror_r],[
#define _REENTRANT
#include <sys/types.h>
#include <string.h>
      ],[
        tmp_strerror_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_STRTOK_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function strtok_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_STRTOK_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([strtok_r])
  ],[
    tmp_strtok_r="yes"
  ],[
    tmp_strtok_r="no"
  ])
  if test "$tmp_strtok_r" = "yes"; then
    AC_EGREP_CPP([strtok_r],[
#include <sys/types.h>
#include <string.h>
    ],[
      tmp_strtok_r="proto_declared"
    ],[
      AC_EGREP_CPP([strtok_r],[
#define _REENTRANT
#include <sys/types.h>
#include <string.h>
      ],[
        tmp_strtok_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_INET_NTOA_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function inet_ntoa_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_INET_NTOA_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([inet_ntoa_r])
  ],[
    tmp_inet_ntoa_r="yes"
  ],[
    tmp_inet_ntoa_r="no"
  ])
  if test "$tmp_inet_ntoa_r" = "yes"; then
    AC_EGREP_CPP([inet_ntoa_r],[
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
    ],[
      tmp_inet_ntoa_r="proto_declared"
    ],[
      AC_EGREP_CPP([inet_ntoa_r],[
#define _REENTRANT
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
      ],[
        tmp_inet_ntoa_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_GETHOSTBYADDR_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function gethostbyaddr_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_GETHOSTBYADDR_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([gethostbyaddr_r])
  ],[
    tmp_gethostbyaddr_r="yes"
  ],[
    tmp_gethostbyaddr_r="no"
  ])
  if test "$tmp_gethostbyaddr_r" = "yes"; then
    AC_EGREP_CPP([gethostbyaddr_r],[
#include <sys/types.h>
#include <netdb.h>
    ],[
      tmp_gethostbyaddr_r="proto_declared"
    ],[
      AC_EGREP_CPP([gethostbyaddr_r],[
#define _REENTRANT
#include <sys/types.h>
#include <netdb.h>
      ],[
        tmp_gethostbyaddr_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_GETHOSTBYNAME_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function gethostbyname_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_GETHOSTBYNAME_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([gethostbyname_r])
  ],[
    tmp_gethostbyname_r="yes"
  ],[
    tmp_gethostbyname_r="no"
  ])
  if test "$tmp_gethostbyname_r" = "yes"; then
    AC_EGREP_CPP([gethostbyname_r],[
#include <sys/types.h>
#include <netdb.h>
    ],[
      tmp_gethostbyname_r="proto_declared"
    ],[
      AC_EGREP_CPP([gethostbyname_r],[
#define _REENTRANT
#include <sys/types.h>
#include <netdb.h>
      ],[
        tmp_gethostbyname_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_GETPROTOBYNAME_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function getprotobyname_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_GETPROTOBYNAME_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([getprotobyname_r])
  ],[
    tmp_getprotobyname_r="yes"
  ],[
    tmp_getprotobyname_r="no"
  ])
  if test "$tmp_getprotobyname_r" = "yes"; then
    AC_EGREP_CPP([getprotobyname_r],[
#include <sys/types.h>
#include <netdb.h>
    ],[
      tmp_getprotobyname_r="proto_declared"
    ],[
      AC_EGREP_CPP([getprotobyname_r],[
#define _REENTRANT
#include <sys/types.h>
#include <netdb.h>
      ],[
        tmp_getprotobyname_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_GETSERVBYPORT_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes function getservbyport_r compiler visible.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_GETSERVBYPORT_R], [
  AC_LINK_IFELSE([
    AC_LANG_FUNC_LINK_TRY([getservbyport_r])
  ],[
    tmp_getservbyport_r="yes"
  ],[
    tmp_getservbyport_r="no"
  ])
  if test "$tmp_getservbyport_r" = "yes"; then
    AC_EGREP_CPP([getservbyport_r],[
#include <sys/types.h>
#include <netdb.h>
    ],[
      tmp_getservbyport_r="proto_declared"
    ],[
      AC_EGREP_CPP([getservbyport_r],[
#define _REENTRANT
#include <sys/types.h>
#include <netdb.h>
      ],[
        tmp_getservbyport_r="proto_needs_reentrant"
        tmp_need_reentrant="yes"
      ])
    ])
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_FUNCTIONS_R
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl makes several _r functions compiler visible.
dnl Internal macro for CARES_CONFIGURE_REENTRANT.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_FUNCTIONS_R], [
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_GMTIME_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_LOCALTIME_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_STRERROR_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_STRTOK_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_INET_NTOA_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_GETHOSTBYADDR_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_GETHOSTBYNAME_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_GETPROTOBYNAME_R
  fi
  if test "$tmp_need_reentrant" = "no"; then
    CARES_CHECK_NEED_REENTRANT_GETSERVBYPORT_R
  fi
])


dnl CARES_CHECK_NEED_REENTRANT_SYSTEM
dnl -------------------------------------------------
dnl Checks if the preprocessor _REENTRANT definition
dnl must be unconditionally done for this platform.
dnl Internal macro for CARES_CONFIGURE_REENTRANT.

AC_DEFUN([CARES_CHECK_NEED_REENTRANT_SYSTEM], [
  case $host_os in
    solaris*)
      tmp_need_reentrant="yes"
      ;;
    *)
      tmp_need_reentrant="no"
      ;;
  esac
])


dnl CARES_CHECK_NEED_THREAD_SAFE_SYSTEM
dnl -------------------------------------------------
dnl Checks if the preprocessor _THREAD_SAFE definition
dnl must be unconditionally done for this platform.
dnl Internal macro for CARES_CONFIGURE_THREAD_SAFE.

AC_DEFUN([CARES_CHECK_NEED_THREAD_SAFE_SYSTEM], [
  case $host_os in
    aix[[123]].* | aix4.[[012]].*)
      dnl aix 4.2 and older
      tmp_need_thread_safe="no"
      ;;
    aix*)
      dnl AIX 4.3 and newer
      tmp_need_thread_safe="yes"
      ;;
    *)
      tmp_need_thread_safe="no"
      ;;
  esac
])


dnl CARES_CONFIGURE_FROM_NOW_ON_WITH_REENTRANT
dnl -------------------------------------------------
dnl This macro ensures that configuration tests done
dnl after this will execute with preprocessor symbol
dnl _REENTRANT defined. This macro also ensures that
dnl the generated config file defines NEED_REENTRANT
dnl and that in turn setup.h will define _REENTRANT.
dnl Internal macro for CARES_CONFIGURE_REENTRANT.

AC_DEFUN([CARES_CONFIGURE_FROM_NOW_ON_WITH_REENTRANT], [
AC_DEFINE(NEED_REENTRANT, 1,
  [Define to 1 if _REENTRANT preprocessor symbol must be defined.])
cat >>confdefs.h <<_EOF
#ifndef _REENTRANT
#  define _REENTRANT
#endif
_EOF
])


dnl CARES_CONFIGURE_FROM_NOW_ON_WITH_THREAD_SAFE
dnl -------------------------------------------------
dnl This macro ensures that configuration tests done
dnl after this will execute with preprocessor symbol
dnl _THREAD_SAFE defined. This macro also ensures that
dnl the generated config file defines NEED_THREAD_SAFE
dnl and that in turn setup.h will define _THREAD_SAFE.
dnl Internal macro for CARES_CONFIGURE_THREAD_SAFE.

AC_DEFUN([CARES_CONFIGURE_FROM_NOW_ON_WITH_THREAD_SAFE], [
AC_DEFINE(NEED_THREAD_SAFE, 1,
  [Define to 1 if _THREAD_SAFE preprocessor symbol must be defined.])
cat >>confdefs.h <<_EOF
#ifndef _THREAD_SAFE
#  define _THREAD_SAFE
#endif
_EOF
])


dnl CARES_CONFIGURE_REENTRANT
dnl -------------------------------------------------
dnl This first checks if the preprocessor _REENTRANT
dnl symbol is already defined. If it isn't currently
dnl defined a set of checks are performed to verify
dnl if its definition is required to make visible to
dnl the compiler a set of *_r functions. Finally, if
dnl _REENTRANT is already defined or needed it takes
dnl care of making adjustments necessary to ensure
dnl that it is defined equally for further configure
dnl tests and generated config file.

AC_DEFUN([CARES_CONFIGURE_REENTRANT], [
  AC_PREREQ([2.50])dnl
  #
  AC_MSG_CHECKING([if _REENTRANT is already defined])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
#ifdef _REENTRANT
      int dummy=1;
#else
      force compilation error
#endif
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tmp_reentrant_initially_defined="yes"
  ],[
    AC_MSG_RESULT([no])
    tmp_reentrant_initially_defined="no"
  ])
  #
  if test "$tmp_reentrant_initially_defined" = "no"; then
    AC_MSG_CHECKING([if _REENTRANT is actually needed])
    CARES_CHECK_NEED_REENTRANT_SYSTEM
    if test "$tmp_need_reentrant" = "no"; then
      CARES_CHECK_NEED_REENTRANT_ERRNO
    fi
    if test "$tmp_need_reentrant" = "no"; then
      CARES_CHECK_NEED_REENTRANT_FUNCTIONS_R
    fi
    if test "$tmp_need_reentrant" = "yes"; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  fi
  #
  AC_MSG_CHECKING([if _REENTRANT is onwards defined])
  if test "$tmp_reentrant_initially_defined" = "yes" ||
    test "$tmp_need_reentrant" = "yes"; then
    CARES_CONFIGURE_FROM_NOW_ON_WITH_REENTRANT
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
  #
])


dnl CARES_CONFIGURE_THREAD_SAFE
dnl -------------------------------------------------
dnl This first checks if the preprocessor _THREAD_SAFE
dnl symbol is already defined. If it isn't currently
dnl defined a set of checks are performed to verify
dnl if its definition is required. Finally, if
dnl _THREAD_SAFE is already defined or needed it takes
dnl care of making adjustments necessary to ensure
dnl that it is defined equally for further configure
dnl tests and generated config file.

AC_DEFUN([CARES_CONFIGURE_THREAD_SAFE], [
  AC_PREREQ([2.50])dnl
  #
  AC_MSG_CHECKING([if _THREAD_SAFE is already defined])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
    ]],[[
#ifdef _THREAD_SAFE
      int dummy=1;
#else
      force compilation error
#endif
    ]])
  ],[
    AC_MSG_RESULT([yes])
    tmp_thread_safe_initially_defined="yes"
  ],[
    AC_MSG_RESULT([no])
    tmp_thread_safe_initially_defined="no"
  ])
  #
  if test "$tmp_thread_safe_initially_defined" = "no"; then
    AC_MSG_CHECKING([if _THREAD_SAFE is actually needed])
    CARES_CHECK_NEED_THREAD_SAFE_SYSTEM
    if test "$tmp_need_thread_safe" = "yes"; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
    fi
  fi
  #
  AC_MSG_CHECKING([if _THREAD_SAFE is onwards defined])
  if test "$tmp_thread_safe_initially_defined" = "yes" ||
    test "$tmp_need_thread_safe" = "yes"; then
    CARES_CONFIGURE_FROM_NOW_ON_WITH_THREAD_SAFE
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
  #
])
