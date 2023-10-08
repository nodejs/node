#---------------------------------------------------------------------------
#
# xc-am-iface.m4
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
# SPDX-License-Identifier: MIT
#---------------------------------------------------------------------------

# serial 1


dnl _XC_AUTOMAKE_BODY
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl This macro performs embedding of automake initialization
dnl code into configure script. When automake version 1.14 or
dnl newer is used at configure script generation time, this
dnl results in 'subdir-objects' automake option being used.
dnl When using automake versions older than 1.14 this option
dnl is not used when generating configure script.
dnl
dnl Existence of automake _AM_PROG_CC_C_O m4 private macro
dnl is used to differentiate automake version 1.14 from older
dnl ones which lack this macro.

m4_define([_XC_AUTOMAKE_BODY],
[dnl
## --------------------------------------- ##
##  Start of automake initialization code  ##
## --------------------------------------- ##
m4_ifdef([_AM_PROG_CC_C_O],
[
AM_INIT_AUTOMAKE([subdir-objects])
],[
AM_INIT_AUTOMAKE
])dnl
## ------------------------------------- ##
##  End of automake initialization code  ##
## ------------------------------------- ##
dnl
m4_define([$0], [])[]dnl
])


dnl XC_AUTOMAKE
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl This macro embeds automake machinery into configure
dnl script regardless of automake version used in order
dnl to generate configure script.
dnl
dnl When using automake version 1.14 or newer, automake
dnl initialization option 'subdir-objects' is used to
dnl generate the configure script, otherwise this option
dnl is not used.

AC_DEFUN([XC_AUTOMAKE],
[dnl
AC_PREREQ([2.50])dnl
dnl
AC_BEFORE([$0],[AM_INIT_AUTOMAKE])dnl
dnl
_XC_AUTOMAKE_BODY
dnl
m4_ifdef([AM_INIT_AUTOMAKE],
  [m4_undefine([AM_INIT_AUTOMAKE])])dnl
dnl
m4_define([$0], [])[]dnl
])


dnl _XC_AMEND_DISTCLEAN_BODY ([LIST-OF-SUBDIRS])
dnl -------------------------------------------------
dnl Private macro.
dnl
dnl This macro performs shell code embedding into
dnl configure script in order to modify distclean
dnl and maintainer-clean targets of makefiles which
dnl are located in given list of subdirs.
dnl
dnl See XC_AMEND_DISTCLEAN comments for details.

m4_define([_XC_AMEND_DISTCLEAN_BODY],
[dnl
## ---------------------------------- ##
##  Start of distclean amending code  ##
## ---------------------------------- ##

for xc_subdir in [$1]
do

if test ! -f "$xc_subdir/Makefile"; then
  echo "$xc_msg_err $xc_subdir/Makefile file not found. $xc_msg_abrt" >&2
  exit 1
fi

# Fetch dependency tracking file list from Makefile include lines.

xc_inc_lines=`grep '^include .*(DEPDIR)' "$xc_subdir/Makefile" 2>/dev/null`
xc_cnt_words=`echo "$xc_inc_lines" | wc -w | tr -d "$xc_space$xc_tab"`

# --disable-dependency-tracking might have been used, consequently
# there is nothing to amend without a dependency tracking file list.

if test $xc_cnt_words -gt 0; then

AC_MSG_NOTICE([amending $xc_subdir/Makefile])

# Build Makefile specific patch hunk.

xc_p="$xc_subdir/xc_patch.tmp"

xc_rm_depfiles=`echo "$xc_inc_lines" \
  | $SED 's%include%	-rm -f%' 2>/dev/null`

xc_dep_subdirs=`echo "$xc_inc_lines" \
  | $SED 's%include[[ ]][[ ]]*%%' 2>/dev/null \
  | $SED 's%(DEPDIR)/.*%(DEPDIR)%' 2>/dev/null \
  | sort | uniq`

echo "$xc_rm_depfiles" >$xc_p

for xc_dep_dir in $xc_dep_subdirs; do
  echo "${xc_tab}@xm_dep_cnt=\`ls $xc_dep_dir | wc -l 2>/dev/null\`; \\"            >>$xc_p
  echo "${xc_tab}if test \$\$xm_dep_cnt -eq 0 && test -d $xc_dep_dir; then \\"      >>$xc_p
  echo "${xc_tab}  rm -rf $xc_dep_dir; \\"                                          >>$xc_p
  echo "${xc_tab}fi"                                                                >>$xc_p
done

# Build Makefile patching sed scripts.

xc_s1="$xc_subdir/xc_script_1.tmp"
xc_s2="$xc_subdir/xc_script_2.tmp"
xc_s3="$xc_subdir/xc_script_3.tmp"

cat >$xc_s1 <<\_EOT
/^distclean[[ ]]*:/,/^[[^	]][[^	]]*:/{
  s/^.*(DEPDIR)/___xc_depdir_line___/
}
/^maintainer-clean[[ ]]*:/,/^[[^	]][[^	]]*:/{
  s/^.*(DEPDIR)/___xc_depdir_line___/
}
_EOT

cat >$xc_s2 <<\_EOT
/___xc_depdir_line___$/{
  N
  /___xc_depdir_line___$/D
}
_EOT

cat >$xc_s3 <<_EOT
/^___xc_depdir_line___/{
  r $xc_p
  d
}
_EOT

# Apply patch to Makefile and cleanup.

$SED -f "$xc_s1" "$xc_subdir/Makefile"      >"$xc_subdir/Makefile.tmp1"
$SED -f "$xc_s2" "$xc_subdir/Makefile.tmp1" >"$xc_subdir/Makefile.tmp2"
$SED -f "$xc_s3" "$xc_subdir/Makefile.tmp2" >"$xc_subdir/Makefile.tmp3"

if test -f "$xc_subdir/Makefile.tmp3"; then
  mv -f "$xc_subdir/Makefile.tmp3" "$xc_subdir/Makefile"
fi

test -f "$xc_subdir/Makefile.tmp1" && rm -f "$xc_subdir/Makefile.tmp1"
test -f "$xc_subdir/Makefile.tmp2" && rm -f "$xc_subdir/Makefile.tmp2"
test -f "$xc_subdir/Makefile.tmp3" && rm -f "$xc_subdir/Makefile.tmp3"

test -f "$xc_p"  && rm -f "$xc_p"
test -f "$xc_s1" && rm -f "$xc_s1"
test -f "$xc_s2" && rm -f "$xc_s2"
test -f "$xc_s3" && rm -f "$xc_s3"

fi

done

## -------------------------------- ##
##  End of distclean amending code  ##
## -------------------------------- ##
dnl
m4_define([$0], [])[]dnl
])


dnl XC_AMEND_DISTCLEAN ([LIST-OF-SUBDIRS])
dnl -------------------------------------------------
dnl Public macro.
dnl
dnl This macro embeds shell code into configure script
dnl that amends, at configure runtime, the distclean
dnl and maintainer-clean targets of Makefiles located
dnl in all subdirs given in the mandatory white-space
dnl separated list argument.
dnl
dnl Embedding only takes place when using automake 1.14
dnl or newer, otherwise amending code is not included
dnl in generated configure script.
dnl
dnl distclean and maintainer-clean targets are modified
dnl to avoid unconditional removal of dependency subdirs
dnl which triggers distclean and maintainer-clean errors
dnl when using automake 'subdir-objects' option along
dnl with per-target objects and source files existing in
dnl multiple subdirs used for different build targets.
dnl
dnl New behavior first removes each dependency tracking
dnl file independently, and only removes each dependency
dnl subdir when it finds out that it no longer holds any
dnl dependency tracking file.
dnl
dnl When configure option --disable-dependency-tracking
dnl is used no amending takes place given that there are
dnl no dependency tracking files.

AC_DEFUN([XC_AMEND_DISTCLEAN],
[dnl
AC_PREREQ([2.50])dnl
dnl
m4_ifdef([_AC_OUTPUT_MAIN_LOOP],
  [m4_provide_if([_AC_OUTPUT_MAIN_LOOP], [],
    [m4_fatal([call to AC_OUTPUT needed before $0])])])dnl
dnl
m4_if([$#], [1], [], [m4_fatal([$0: wrong number of arguments])])dnl
m4_if([$1], [], [m4_fatal([$0: missing argument])])dnl
dnl
AC_REQUIRE([XC_CONFIGURE_PREAMBLE])dnl
dnl
m4_ifdef([_AM_PROG_CC_C_O],
[
_XC_AMEND_DISTCLEAN_BODY([$1])
])dnl
m4_define([$0], [])[]dnl
])

