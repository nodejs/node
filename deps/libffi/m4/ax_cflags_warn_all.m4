# ===========================================================================
#    http://www.gnu.org/software/autoconf-archive/ax_cflags_warn_all.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CFLAGS_WARN_ALL   [(shellvar [,default, [A/NA]])]
#   AX_CXXFLAGS_WARN_ALL [(shellvar [,default, [A/NA]])]
#   AX_FCFLAGS_WARN_ALL  [(shellvar [,default, [A/NA]])]
#
# DESCRIPTION
#
#   Try to find a compiler option that enables most reasonable warnings.
#
#   For the GNU compiler it will be -Wall (and -ansi -pedantic) The result
#   is added to the shellvar being CFLAGS, CXXFLAGS, or FCFLAGS by default.
#
#   Currently this macro knows about the GCC, Solaris, Digital Unix, AIX,
#   HP-UX, IRIX, NEC SX-5 (Super-UX 10), Cray J90 (Unicos 10.0.0.8), and
#   Intel compilers.  For a given compiler, the Fortran flags are much more
#   experimental than their C equivalents.
#
#    - $1 shell-variable-to-add-to : CFLAGS, CXXFLAGS, or FCFLAGS
#    - $2 add-value-if-not-found : nothing
#    - $3 action-if-found : add value to shellvariable
#    - $4 action-if-not-found : nothing
#
#   NOTE: These macros depend on AX_APPEND_FLAG.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2010 Rhys Ulerich <rhys.ulerich@gmail.com>
#
#   This program is free software; you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation; either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 14

AC_DEFUN([AX_FLAGS_WARN_ALL],[dnl
AS_VAR_PUSHDEF([FLAGS],[_AC_LANG_PREFIX[]FLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_[]_AC_LANG_ABBREV[]flags_warn_all])dnl
AC_CACHE_CHECK([m4_ifval($1,$1,FLAGS) for maximum warnings],
VAR,[VAR="no, unknown"
ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-warn all  % -warn all"   dnl Intel
   "-pedantic  % -Wall"       dnl GCC
   "-xstrconst % -v"          dnl Solaris C
   "-std1      % -verbose -w0 -warnprotos" dnl Digital Unix
   "-qlanglvl=ansi % -qsrcmsg -qinfo=all:noppt:noppc:noobs:nocnd" dnl AIX
   "-ansi -ansiE % -fullwarn" dnl IRIX
   "+ESlit     % +w1"         dnl HP-UX C
   "-Xc        % -pvctl[,]fullmsg" dnl NEC SX-5 (Super-UX 10)
   "-h conform % -h msglevel 2" dnl Cray C (Unicos)
   #
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM],
                     [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
FLAGS="$ac_save_[]FLAGS"
])
AS_VAR_POPDEF([FLAGS])dnl
AC_REQUIRE([AX_APPEND_FLAG])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_default($4,[m4_ifval($2,[AX_APPEND_FLAG([$2], [$1])])]) ;;
   *) m4_default($3,[AX_APPEND_FLAG([$VAR], [$1])]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
])dnl AX_FLAGS_WARN_ALL
dnl  implementation tactics:
dnl   the for-argument contains a list of options. The first part of
dnl   these does only exist to detect the compiler - usually it is
dnl   a global option to enable -ansi or -extrawarnings. All other
dnl   compilers will fail about it. That was needed since a lot of
dnl   compilers will give false positives for some option-syntax
dnl   like -Woption or -Xoption as they think of it is a pass-through
dnl   to later compile stages or something. The "%" is used as a
dnl   delimiter. A non-option comment can be given after "%%" marks
dnl   which will be shown but not added to the respective C/CXXFLAGS.

AC_DEFUN([AX_CFLAGS_WARN_ALL],[dnl
AC_LANG_PUSH([C])
AX_FLAGS_WARN_ALL([$1], [$2], [$3], [$4])
AC_LANG_POP([C])
])

AC_DEFUN([AX_CXXFLAGS_WARN_ALL],[dnl
AC_LANG_PUSH([C++])
AX_FLAGS_WARN_ALL([$1], [$2], [$3], [$4])
AC_LANG_POP([C++])
])

AC_DEFUN([AX_FCFLAGS_WARN_ALL],[dnl
AC_LANG_PUSH([Fortran])
AX_FLAGS_WARN_ALL([$1], [$2], [$3], [$4])
AC_LANG_POP([Fortran])
])
