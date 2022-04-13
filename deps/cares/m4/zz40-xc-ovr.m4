#---------------------------------------------------------------------------
#
# zz40-xc-ovr.m4
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


dnl The funny name of this file is intentional in order to make it
dnl sort alphabetically after any libtool, autoconf or automake
dnl provided .m4 macro file that might get copied into this same
dnl subdirectory. This allows that macro (re)definitions from this
dnl file may override those provided in other files.


dnl Version macros
dnl -------------------------------------------------
dnl Public macros.

m4_define([XC_CONFIGURE_PREAMBLE_VER_MAJOR],[1])dnl
m4_define([XC_CONFIGURE_PREAMBLE_VER_MINOR],[0])dnl


dnl _XC_CFG_PRE_PREAMBLE
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_CFG_PRE_PREAMBLE],
[
## -------------------------------- ##
@%:@@%:@  [XC_CONFIGURE_PREAMBLE] ver: []dnl
XC_CONFIGURE_PREAMBLE_VER_MAJOR.[]dnl
XC_CONFIGURE_PREAMBLE_VER_MINOR  ##
## -------------------------------- ##

xc_configure_preamble_ver_major='XC_CONFIGURE_PREAMBLE_VER_MAJOR'
xc_configure_preamble_ver_minor='XC_CONFIGURE_PREAMBLE_VER_MINOR'

#
# Set IFS to space, tab and newline.
#

xc_space=' '
xc_tab='	'
xc_newline='
'
IFS="$xc_space$xc_tab$xc_newline"

#
# Set internationalization behavior variables.
#

LANG='C'
LC_ALL='C'
LANGUAGE='C'
export LANG
export LC_ALL
export LANGUAGE

#
# Some useful variables.
#

xc_msg_warn='configure: WARNING:'
xc_msg_abrt='Can not continue.'
xc_msg_err='configure: error:'
])


dnl _XC_CFG_PRE_BASIC_CHK_CMD_ECHO
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'echo' command
dnl is available, otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_CMD_ECHO],
[dnl
AC_REQUIRE([_XC_CFG_PRE_PREAMBLE])dnl
#
# Verify that 'echo' command is available, otherwise abort.
#

xc_tst_str='unknown'
(`echo "$xc_tst_str" >/dev/null 2>&1`) && xc_tst_str='success'
case "x$xc_tst_str" in @%:@ ((
  xsuccess)
    :
    ;;
  *)
    # Try built-in echo, and fail.
    echo "$xc_msg_err 'echo' command not found. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_CMD_TEST
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'test' command
dnl is available, otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_CMD_TEST],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_ECHO])dnl
#
# Verify that 'test' command is available, otherwise abort.
#

xc_tst_str='unknown'
(`test -n "$xc_tst_str" >/dev/null 2>&1`) && xc_tst_str='success'
case "x$xc_tst_str" in @%:@ ((
  xsuccess)
    :
    ;;
  *)
    echo "$xc_msg_err 'test' command not found. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_VAR_PATH
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'PATH' variable
dnl is set, otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_VAR_PATH],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_TEST])dnl
#
# Verify that 'PATH' variable is set, otherwise abort.
#

xc_tst_str='unknown'
(`test -n "$PATH" >/dev/null 2>&1`) && xc_tst_str='success'
case "x$xc_tst_str" in @%:@ ((
  xsuccess)
    :
    ;;
  *)
    echo "$xc_msg_err 'PATH' variable not set. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_CMD_EXPR
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'expr' command
dnl is available, otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_CMD_EXPR],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
#
# Verify that 'expr' command is available, otherwise abort.
#

xc_tst_str='unknown'
xc_tst_str=`expr "$xc_tst_str" : '.*' 2>/dev/null`
case "x$xc_tst_str" in @%:@ ((
  x7)
    :
    ;;
  *)
    echo "$xc_msg_err 'expr' command not found. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_UTIL_SED
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'sed' utility
dnl is found within 'PATH', otherwise aborts execution.
dnl
dnl This 'sed' is required in order to allow configure
dnl script bootstrapping itself. No fancy testing for a
dnl proper 'sed' this early, that should be done later.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_UTIL_SED],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
#
# Verify that 'sed' utility is found within 'PATH', otherwise abort.
#

xc_tst_str='unknown'
xc_tst_str=`echo "$xc_tst_str" 2>/dev/null \
  | sed -e 's:unknown:success:' 2>/dev/null`
case "x$xc_tst_str" in @%:@ ((
  xsuccess)
    :
    ;;
  *)
    echo "$xc_msg_err 'sed' utility not found in 'PATH'. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_UTIL_GREP
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'grep' utility
dnl is found within 'PATH', otherwise aborts execution.
dnl
dnl This 'grep' is required in order to allow configure
dnl script bootstrapping itself. No fancy testing for a
dnl proper 'grep' this early, that should be done later.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_UTIL_GREP],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
#
# Verify that 'grep' utility is found within 'PATH', otherwise abort.
#

xc_tst_str='unknown'
(`echo "$xc_tst_str" 2>/dev/null \
  | grep 'unknown' >/dev/null 2>&1`) && xc_tst_str='success'
case "x$xc_tst_str" in @%:@ ((
  xsuccess)
    :
    ;;
  *)
    echo "$xc_msg_err 'grep' utility not found in 'PATH'. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_UTIL_TR
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'tr' utility
dnl is found within 'PATH', otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_UTIL_TR],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
#
# Verify that 'tr' utility is found within 'PATH', otherwise abort.
#

xc_tst_str="${xc_tab}98s7u6c5c4e3s2s10"
xc_tst_str=`echo "$xc_tst_str" 2>/dev/null \
  | tr -d "0123456789$xc_tab" 2>/dev/null`
case "x$xc_tst_str" in @%:@ ((
  xsuccess)
    :
    ;;
  *)
    echo "$xc_msg_err 'tr' utility not found in 'PATH'. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_UTIL_WC
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'wc' utility
dnl is found within 'PATH', otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_UTIL_WC],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_TR])dnl
#
# Verify that 'wc' utility is found within 'PATH', otherwise abort.
#

xc_tst_str='unknown unknown unknown unknown'
xc_tst_str=`echo "$xc_tst_str" 2>/dev/null \
  | wc -w 2>/dev/null | tr -d "$xc_space$xc_tab" 2>/dev/null`
case "x$xc_tst_str" in @%:@ ((
  x4)
    :
    ;;
  *)
    echo "$xc_msg_err 'wc' utility not found in 'PATH'. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_BASIC_CHK_UTIL_CAT
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that verifies that 'cat' utility
dnl is found within 'PATH', otherwise aborts execution.

AC_DEFUN([_XC_CFG_PRE_BASIC_CHK_UTIL_CAT],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_WC])dnl
#
# Verify that 'cat' utility is found within 'PATH', otherwise abort.
#

xc_tst_str='unknown'
xc_tst_str=`cat <<_EOT 2>/dev/null \
  | wc -l 2>/dev/null | tr -d "$xc_space$xc_tab" 2>/dev/null
unknown
unknown
unknown
_EOT`
case "x$xc_tst_str" in @%:@ ((
  x3)
    :
    ;;
  *)
    echo "$xc_msg_err 'cat' utility not found in 'PATH'. $xc_msg_abrt" >&2
    exit 1
    ;;
esac
])


dnl _XC_CFG_PRE_CHECK_PATH_SEPARATOR
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl Emits shell code that computes the path separator
dnl and stores the result in 'PATH_SEPARATOR', unless
dnl the user has already set it with a non-empty value.
dnl
dnl This path separator is the symbol used to separate
dnl or diferentiate paths inside the 'PATH' environment
dnl variable.
dnl
dnl Non-empty user provided 'PATH_SEPARATOR' always
dnl overrides the auto-detected one.

AC_DEFUN([_XC_CFG_PRE_CHECK_PATH_SEPARATOR],
[dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_EXPR])dnl
#
# Auto-detect and set 'PATH_SEPARATOR', unless it is already non-empty set.
#

# Directory count in 'PATH' when using a colon separator.
xc_tst_dirs_col='x'
xc_tst_prev_IFS=$IFS; IFS=':'
for xc_tst_dir in $PATH; do
  IFS=$xc_tst_prev_IFS
  xc_tst_dirs_col="x$xc_tst_dirs_col"
done
IFS=$xc_tst_prev_IFS
xc_tst_dirs_col=`expr "$xc_tst_dirs_col" : '.*'`

# Directory count in 'PATH' when using a semicolon separator.
xc_tst_dirs_sem='x'
xc_tst_prev_IFS=$IFS; IFS=';'
for xc_tst_dir in $PATH; do
  IFS=$xc_tst_prev_IFS
  xc_tst_dirs_sem="x$xc_tst_dirs_sem"
done
IFS=$xc_tst_prev_IFS
xc_tst_dirs_sem=`expr "$xc_tst_dirs_sem" : '.*'`

if test $xc_tst_dirs_sem -eq $xc_tst_dirs_col; then
  # When both counting methods give the same result we do not want to
  # chose one over the other, and consider auto-detection not possible.
  if test -z "$PATH_SEPARATOR"; then
    # Stop dead until user provides 'PATH_SEPARATOR' definition.
    echo "$xc_msg_err 'PATH_SEPARATOR' variable not set. $xc_msg_abrt" >&2
    exit 1
  fi
else
  # Separator with the greater directory count is the auto-detected one.
  if test $xc_tst_dirs_sem -gt $xc_tst_dirs_col; then
    xc_tst_auto_separator=';'
  else
    xc_tst_auto_separator=':'
  fi
  if test -z "$PATH_SEPARATOR"; then
    # Simply use the auto-detected one when not already set.
    PATH_SEPARATOR=$xc_tst_auto_separator
  elif test "x$PATH_SEPARATOR" != "x$xc_tst_auto_separator"; then
    echo "$xc_msg_warn 'PATH_SEPARATOR' does not match auto-detected one." >&2
  fi
fi
xc_PATH_SEPARATOR=$PATH_SEPARATOR
AC_SUBST([PATH_SEPARATOR])dnl
])


dnl _XC_CFG_PRE_POSTLUDE
dnl -------------------------------------------------
dnl Private macro.

AC_DEFUN([_XC_CFG_PRE_POSTLUDE],
[dnl
AC_REQUIRE([_XC_CFG_PRE_PREAMBLE])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_ECHO])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_TEST])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_EXPR])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_SED])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_GREP])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_TR])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_WC])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_CAT])dnl
AC_REQUIRE([_XC_CFG_PRE_CHECK_PATH_SEPARATOR])dnl
dnl
xc_configure_preamble_result='yes'
])


dnl XC_CONFIGURE_PREAMBLE
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl This macro emits shell code which does some
dnl very basic checks related with the availability
dnl of some commands and utilities needed to allow
dnl configure script bootstrapping itself when using
dnl these to figure out other settings. Also emits
dnl code that performs PATH_SEPARATOR auto-detection
dnl and sets its value unless it is already set with
dnl a non-empty value.
dnl
dnl These basic checks are intended to be placed and
dnl executed as early as possible in the resulting
dnl configure script, and as such these must be pure
dnl and portable shell code.
dnl
dnl This macro may be used directly, or indirectly
dnl when using other macros that AC_REQUIRE it such
dnl as XC_CHECK_PATH_SEPARATOR.
dnl
dnl Currently the mechanism used to ensure that this
dnl macro expands early enough in generated configure
dnl script is making it override autoconf and libtool
dnl PATH_SEPARATOR check.

AC_DEFUN([XC_CONFIGURE_PREAMBLE],
[dnl
AC_PREREQ([2.50])dnl
dnl
AC_BEFORE([$0],[_XC_CFG_PRE_PREAMBLE])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_CMD_ECHO])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_CMD_TEST])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_CMD_EXPR])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_UTIL_SED])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_UTIL_GREP])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_UTIL_TR])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_UTIL_WC])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_BASIC_CHK_UTIL_CAT])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_CHECK_PATH_SEPARATOR])dnl
AC_BEFORE([$0],[_XC_CFG_PRE_POSTLUDE])dnl
dnl
AC_BEFORE([$0],[AC_CHECK_TOOL])dnl
AC_BEFORE([$0],[AC_CHECK_PROG])dnl
AC_BEFORE([$0],[AC_CHECK_TOOLS])dnl
AC_BEFORE([$0],[AC_CHECK_PROGS])dnl
dnl
AC_BEFORE([$0],[AC_PATH_TOOL])dnl
AC_BEFORE([$0],[AC_PATH_PROG])dnl
AC_BEFORE([$0],[AC_PATH_PROGS])dnl
dnl
AC_BEFORE([$0],[AC_PROG_SED])dnl
AC_BEFORE([$0],[AC_PROG_GREP])dnl
AC_BEFORE([$0],[AC_PROG_LN_S])dnl
AC_BEFORE([$0],[AC_PROG_MKDIR_P])dnl
AC_BEFORE([$0],[AC_PROG_INSTALL])dnl
AC_BEFORE([$0],[AC_PROG_MAKE_SET])dnl
AC_BEFORE([$0],[AC_PROG_LIBTOOL])dnl
dnl
AC_BEFORE([$0],[LT_INIT])dnl
AC_BEFORE([$0],[AM_INIT_AUTOMAKE])dnl
AC_BEFORE([$0],[AC_LIBTOOL_WIN32_DLL])dnl
dnl
AC_REQUIRE([_XC_CFG_PRE_PREAMBLE])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_ECHO])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_TEST])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_VAR_PATH])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_CMD_EXPR])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_SED])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_GREP])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_TR])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_WC])dnl
AC_REQUIRE([_XC_CFG_PRE_BASIC_CHK_UTIL_CAT])dnl
AC_REQUIRE([_XC_CFG_PRE_CHECK_PATH_SEPARATOR])dnl
AC_REQUIRE([_XC_CFG_PRE_POSTLUDE])dnl
dnl
m4_pattern_forbid([^_*XC])dnl
m4_define([$0],[])dnl
])


dnl Override autoconf and libtool PATH_SEPARATOR check
dnl -------------------------------------------------
dnl Macros overriding.
dnl
dnl This is done to ensure that the same check is
dnl used across different autoconf versions and to
dnl allow expansion of XC_CONFIGURE_PREAMBLE macro
dnl early enough in the generated configure script.

dnl
dnl Override when using autoconf 2.53 and newer.
dnl

m4_ifdef([_AS_PATH_SEPARATOR_PREPARE],
[dnl
m4_undefine([_AS_PATH_SEPARATOR_PREPARE])dnl
m4_defun([_AS_PATH_SEPARATOR_PREPARE],
[dnl
AC_REQUIRE([XC_CONFIGURE_PREAMBLE])dnl
m4_define([$0],[])dnl
])dnl
])

dnl
dnl Override when using autoconf 2.50 to 2.52
dnl

m4_ifdef([_AC_INIT_PREPARE_FS_SEPARATORS],
[dnl
m4_undefine([_AC_INIT_PREPARE_FS_SEPARATORS])dnl
m4_defun([_AC_INIT_PREPARE_FS_SEPARATORS],
[dnl
AC_REQUIRE([XC_CONFIGURE_PREAMBLE])dnl
ac_path_separator=$PATH_SEPARATOR
m4_define([$0],[])dnl
])dnl
])

dnl
dnl Override when using libtool 1.4.2
dnl

m4_ifdef([_LT_AC_LIBTOOL_SYS_PATH_SEPARATOR],
[dnl
m4_undefine([_LT_AC_LIBTOOL_SYS_PATH_SEPARATOR])dnl
m4_defun([_LT_AC_LIBTOOL_SYS_PATH_SEPARATOR],
[dnl
AC_REQUIRE([XC_CONFIGURE_PREAMBLE])dnl
lt_cv_sys_path_separator=$PATH_SEPARATOR
m4_define([$0],[])dnl
])dnl
])


dnl XC_CHECK_PATH_SEPARATOR
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl Usage of this macro ensures that generated configure
dnl script uses the same PATH_SEPARATOR check irrespective
dnl of autoconf or libtool version being used to generate
dnl configure script.
dnl
dnl Emits shell code that computes the path separator
dnl and stores the result in 'PATH_SEPARATOR', unless
dnl the user has already set it with a non-empty value.
dnl
dnl This path separator is the symbol used to separate
dnl or diferentiate paths inside the 'PATH' environment
dnl variable.
dnl
dnl Non-empty user provided 'PATH_SEPARATOR' always
dnl overrides the auto-detected one.
dnl
dnl Strictly speaking the check is done in two steps. The
dnl first, which does the actual check, takes place in
dnl XC_CONFIGURE_PREAMBLE macro and happens very early in
dnl generated configure script. The second one shows and
dnl logs the result of the check into config.log at a later
dnl configure stage. Placement of this second stage in
dnl generated configure script will be done where first
dnl direct or indirect usage of this macro happens.

AC_DEFUN([XC_CHECK_PATH_SEPARATOR],
[dnl
AC_PREREQ([2.50])dnl
dnl
AC_BEFORE([$0],[AC_CHECK_TOOL])dnl
AC_BEFORE([$0],[AC_CHECK_PROG])dnl
AC_BEFORE([$0],[AC_CHECK_TOOLS])dnl
AC_BEFORE([$0],[AC_CHECK_PROGS])dnl
dnl
AC_BEFORE([$0],[AC_PATH_TOOL])dnl
AC_BEFORE([$0],[AC_PATH_PROG])dnl
AC_BEFORE([$0],[AC_PATH_PROGS])dnl
dnl
AC_BEFORE([$0],[AC_PROG_SED])dnl
AC_BEFORE([$0],[AC_PROG_GREP])dnl
AC_BEFORE([$0],[AC_PROG_LN_S])dnl
AC_BEFORE([$0],[AC_PROG_MKDIR_P])dnl
AC_BEFORE([$0],[AC_PROG_INSTALL])dnl
AC_BEFORE([$0],[AC_PROG_MAKE_SET])dnl
AC_BEFORE([$0],[AC_PROG_LIBTOOL])dnl
dnl
AC_BEFORE([$0],[LT_INIT])dnl
AC_BEFORE([$0],[AM_INIT_AUTOMAKE])dnl
AC_BEFORE([$0],[AC_LIBTOOL_WIN32_DLL])dnl
dnl
AC_REQUIRE([XC_CONFIGURE_PREAMBLE])dnl
dnl
#
# Check that 'XC_CONFIGURE_PREAMBLE' has already run.
#

if test -z "$xc_configure_preamble_result"; then
  AC_MSG_ERROR([xc_configure_preamble_result not set (internal problem)])
fi

#
# Check that 'PATH_SEPARATOR' has already been set.
#

if test -z "$xc_PATH_SEPARATOR"; then
  AC_MSG_ERROR([xc_PATH_SEPARATOR not set (internal problem)])
fi
if test -z "$PATH_SEPARATOR"; then
  AC_MSG_ERROR([PATH_SEPARATOR not set (internal or config.site problem)])
fi
AC_MSG_CHECKING([for path separator])
AC_MSG_RESULT([$PATH_SEPARATOR])
if test "x$PATH_SEPARATOR" != "x$xc_PATH_SEPARATOR"; then
  AC_MSG_CHECKING([for initial path separator])
  AC_MSG_RESULT([$xc_PATH_SEPARATOR])
  AC_MSG_ERROR([path separator mismatch (internal or config.site problem)])
fi
dnl
m4_pattern_forbid([^_*XC])dnl
m4_define([$0],[])dnl
])

