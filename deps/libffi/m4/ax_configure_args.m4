# ===========================================================================
#     http://www.gnu.org/software/autoconf-archive/ax_configure_args.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CONFIGURE_ARGS
#
# DESCRIPTION
#
#   Helper macro for AX_ENABLE_BUILDDIR.
#
#   The traditional way of starting a subdir-configure is running the script
#   with ${1+"$@"} but since autoconf 2.60 this is broken. Instead we have
#   to rely on eval'ing $ac_configure_args however some old autoconf
#   versions do not provide that. To ensure maximum portability of autoconf
#   extension macros this helper can be AC_REQUIRE'd so that
#   $ac_configure_args will alsways be present.
#
#   Sadly, the traditional "exec $SHELL" of the enable_builddir macros is
#   spoiled now and must be replaced by "eval + exit $?".
#
#   Example:
#
#     AC_DEFUN([AX_ENABLE_SUBDIR],[dnl
#       AC_REQUIRE([AX_CONFIGURE_ARGS])dnl
#       eval $SHELL $ac_configure_args || exit $?
#       ...])
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
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

#serial 9

AC_DEFUN([AX_CONFIGURE_ARGS],[
   # [$]@ is unsable in 2.60+ but earlier autoconf had no ac_configure_args
   if test "${ac_configure_args+set}" != "set" ; then
      ac_configure_args=
      for ac_arg in ${1+"[$]@"}; do
         ac_configure_args="$ac_configure_args '$ac_arg'"
      done
   fi
])
