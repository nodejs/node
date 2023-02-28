# generated automatically by aclocal 1.16.3 -*- Autoconf -*-

# Copyright (C) 1996-2020 Free Software Foundation, Inc.

# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

m4_ifndef([AC_CONFIG_MACRO_DIRS], [m4_defun([_AM_CONFIG_MACRO_DIRS], [])m4_defun([AC_CONFIG_MACRO_DIRS], [_AM_CONFIG_MACRO_DIRS($@)])])
m4_ifndef([AC_AUTOCONF_VERSION],
  [m4_copy([m4_PACKAGE_VERSION], [AC_AUTOCONF_VERSION])])dnl
m4_if(m4_defn([AC_AUTOCONF_VERSION]), [2.69],,
[m4_warning([this file was generated for autoconf 2.69.
You have another version of autoconf.  It may work, but is not guaranteed to.
If you have problems, you may need to regenerate the build system entirely.
To do so, use the procedure documented by the package, typically 'autoreconf'.])])

# ltdl.m4 - Configure ltdl for the target system. -*-Autoconf-*-
#
#   Copyright (C) 1999-2008, 2011-2015 Free Software Foundation, Inc.
#   Written by Thomas Tanner, 1999
#
# This file is free software; the Free Software Foundation gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.

# serial 20 LTDL_INIT

# LT_CONFIG_LTDL_DIR(DIRECTORY, [LTDL-MODE])
# ------------------------------------------
# DIRECTORY contains the libltdl sources.  It is okay to call this
# function multiple times, as long as the same DIRECTORY is always given.
AC_DEFUN([LT_CONFIG_LTDL_DIR],
[AC_BEFORE([$0], [LTDL_INIT])
_$0($*)
])# LT_CONFIG_LTDL_DIR

# We break this out into a separate macro, so that we can call it safely
# internally without being caught accidentally by the sed scan in libtoolize.
m4_defun([_LT_CONFIG_LTDL_DIR],
[dnl remove trailing slashes
m4_pushdef([_ARG_DIR], m4_bpatsubst([$1], [/*$]))
m4_case(_LTDL_DIR,
	[], [dnl only set lt_ltdl_dir if _ARG_DIR is not simply '.'
	     m4_if(_ARG_DIR, [.],
	             [],
		 [m4_define([_LTDL_DIR], _ARG_DIR)
	          _LT_SHELL_INIT([lt_ltdl_dir=']_ARG_DIR['])])],
    [m4_if(_ARG_DIR, _LTDL_DIR,
	    [],
	[m4_fatal([multiple libltdl directories: ']_LTDL_DIR[', ']_ARG_DIR['])])])
m4_popdef([_ARG_DIR])
])# _LT_CONFIG_LTDL_DIR

# Initialise:
m4_define([_LTDL_DIR], [])


# _LT_BUILD_PREFIX
# ----------------
# If Autoconf is new enough, expand to '$(top_build_prefix)', otherwise
# to '$(top_builddir)/'.
m4_define([_LT_BUILD_PREFIX],
[m4_ifdef([AC_AUTOCONF_VERSION],
   [m4_if(m4_version_compare(m4_defn([AC_AUTOCONF_VERSION]), [2.62]),
	  [-1], [m4_ifdef([_AC_HAVE_TOP_BUILD_PREFIX],
			  [$(top_build_prefix)],
			  [$(top_builddir)/])],
	  [$(top_build_prefix)])],
   [$(top_builddir)/])[]dnl
])


# LTDL_CONVENIENCE
# ----------------
# sets LIBLTDL to the link flags for the libltdl convenience library and
# LTDLINCL to the include flags for the libltdl header and adds
# --enable-ltdl-convenience to the configure arguments.  Note that
# AC_CONFIG_SUBDIRS is not called here.  LIBLTDL will be prefixed with
# '$(top_build_prefix)' if available, otherwise with '$(top_builddir)/',
# and LTDLINCL will be prefixed with '$(top_srcdir)/' (note the single
# quotes!).  If your package is not flat and you're not using automake,
# define top_build_prefix, top_builddir, and top_srcdir appropriately
# in your Makefiles.
AC_DEFUN([LTDL_CONVENIENCE],
[AC_BEFORE([$0], [LTDL_INIT])dnl
dnl Although the argument is deprecated and no longer documented,
dnl LTDL_CONVENIENCE used to take a DIRECTORY orgument, if we have one
dnl here make sure it is the same as any other declaration of libltdl's
dnl location!  This also ensures lt_ltdl_dir is set when configure.ac is
dnl not yet using an explicit LT_CONFIG_LTDL_DIR.
m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])dnl
_$0()
])# LTDL_CONVENIENCE

# AC_LIBLTDL_CONVENIENCE accepted a directory argument in older libtools,
# now we have LT_CONFIG_LTDL_DIR:
AU_DEFUN([AC_LIBLTDL_CONVENIENCE],
[_LT_CONFIG_LTDL_DIR([m4_default([$1], [libltdl])])
_LTDL_CONVENIENCE])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBLTDL_CONVENIENCE], [])


# _LTDL_CONVENIENCE
# -----------------
# Code shared by LTDL_CONVENIENCE and LTDL_INIT([convenience]).
m4_defun([_LTDL_CONVENIENCE],
[case $enable_ltdl_convenience in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
esac
LIBLTDL='_LT_BUILD_PREFIX'"${lt_ltdl_dir+$lt_ltdl_dir/}libltdlc.la"
LTDLDEPS=$LIBLTDL
LTDLINCL='-I$(top_srcdir)'"${lt_ltdl_dir+/$lt_ltdl_dir}"

AC_SUBST([LIBLTDL])
AC_SUBST([LTDLDEPS])
AC_SUBST([LTDLINCL])

# For backwards non-gettext consistent compatibility...
INCLTDL=$LTDLINCL
AC_SUBST([INCLTDL])
])# _LTDL_CONVENIENCE


# LTDL_INSTALLABLE
# ----------------
# sets LIBLTDL to the link flags for the libltdl installable library
# and LTDLINCL to the include flags for the libltdl header and adds
# --enable-ltdl-install to the configure arguments.  Note that
# AC_CONFIG_SUBDIRS is not called from here.  If an installed libltdl
# is not found, LIBLTDL will be prefixed with '$(top_build_prefix)' if
# available, otherwise with '$(top_builddir)/', and LTDLINCL will be
# prefixed with '$(top_srcdir)/' (note the single quotes!).  If your
# package is not flat and you're not using automake, define top_build_prefix,
# top_builddir, and top_srcdir appropriately in your Makefiles.
# In the future, this macro may have to be called after LT_INIT.
AC_DEFUN([LTDL_INSTALLABLE],
[AC_BEFORE([$0], [LTDL_INIT])dnl
dnl Although the argument is deprecated and no longer documented,
dnl LTDL_INSTALLABLE used to take a DIRECTORY orgument, if we have one
dnl here make sure it is the same as any other declaration of libltdl's
dnl location!  This also ensures lt_ltdl_dir is set when configure.ac is
dnl not yet using an explicit LT_CONFIG_LTDL_DIR.
m4_ifval([$1], [_LT_CONFIG_LTDL_DIR([$1])])dnl
_$0()
])# LTDL_INSTALLABLE

# AC_LIBLTDL_INSTALLABLE accepted a directory argument in older libtools,
# now we have LT_CONFIG_LTDL_DIR:
AU_DEFUN([AC_LIBLTDL_INSTALLABLE],
[_LT_CONFIG_LTDL_DIR([m4_default([$1], [libltdl])])
_LTDL_INSTALLABLE])

dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIBLTDL_INSTALLABLE], [])


# _LTDL_INSTALLABLE
# -----------------
# Code shared by LTDL_INSTALLABLE and LTDL_INIT([installable]).
m4_defun([_LTDL_INSTALLABLE],
[if test -f "$prefix/lib/libltdl.la"; then
  lt_save_LDFLAGS=$LDFLAGS
  LDFLAGS="-L$prefix/lib $LDFLAGS"
  AC_CHECK_LIB([ltdl], [lt_dlinit], [lt_lib_ltdl=yes])
  LDFLAGS=$lt_save_LDFLAGS
  if test yes = "${lt_lib_ltdl-no}"; then
    if test yes != "$enable_ltdl_install"; then
      # Don't overwrite $prefix/lib/libltdl.la without --enable-ltdl-install
      AC_MSG_WARN([not overwriting libltdl at $prefix, force with '--enable-ltdl-install'])
      enable_ltdl_install=no
    fi
  elif test no = "$enable_ltdl_install"; then
    AC_MSG_WARN([libltdl not installed, but installation disabled])
  fi
fi

# If configure.ac declared an installable ltdl, and the user didn't override
# with --disable-ltdl-install, we will install the shipped libltdl.
case $enable_ltdl_install in
  no) ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
      LIBLTDL=-lltdl
      LTDLDEPS=
      LTDLINCL=
      ;;
  *)  enable_ltdl_install=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-install"
      LIBLTDL='_LT_BUILD_PREFIX'"${lt_ltdl_dir+$lt_ltdl_dir/}libltdl.la"
      LTDLDEPS=$LIBLTDL
      LTDLINCL='-I$(top_srcdir)'"${lt_ltdl_dir+/$lt_ltdl_dir}"
      ;;
esac

AC_SUBST([LIBLTDL])
AC_SUBST([LTDLDEPS])
AC_SUBST([LTDLINCL])

# For backwards non-gettext consistent compatibility...
INCLTDL=$LTDLINCL
AC_SUBST([INCLTDL])
])# LTDL_INSTALLABLE


# _LTDL_MODE_DISPATCH
# -------------------
m4_define([_LTDL_MODE_DISPATCH],
[dnl If _LTDL_DIR is '.', then we are configuring libltdl itself:
m4_if(_LTDL_DIR, [],
	[],
    dnl if _LTDL_MODE was not set already, the default value is 'subproject':
    [m4_case(m4_default(_LTDL_MODE, [subproject]),
	  [subproject], [AC_CONFIG_SUBDIRS(_LTDL_DIR)
			  _LT_SHELL_INIT([lt_dlopen_dir=$lt_ltdl_dir])],
	  [nonrecursive], [_LT_SHELL_INIT([lt_dlopen_dir=$lt_ltdl_dir; lt_libobj_prefix=$lt_ltdl_dir/])],
	  [recursive], [],
	[m4_fatal([unknown libltdl mode: ]_LTDL_MODE)])])dnl
dnl Be careful not to expand twice:
m4_define([$0], [])
])# _LTDL_MODE_DISPATCH


# _LT_LIBOBJ(MODULE_NAME)
# -----------------------
# Like AC_LIBOBJ, except that MODULE_NAME goes into _LT_LIBOBJS instead
# of into LIBOBJS.
AC_DEFUN([_LT_LIBOBJ], [
  m4_pattern_allow([^_LT_LIBOBJS$])
  _LT_LIBOBJS="$_LT_LIBOBJS $1.$ac_objext"
])# _LT_LIBOBJS


# LTDL_INIT([OPTIONS])
# --------------------
# Clients of libltdl can use this macro to allow the installer to
# choose between a shipped copy of the ltdl sources or a preinstalled
# version of the library.  If the shipped ltdl sources are not in a
# subdirectory named libltdl, the directory name must be given by
# LT_CONFIG_LTDL_DIR.
AC_DEFUN([LTDL_INIT],
[dnl Parse OPTIONS
_LT_SET_OPTIONS([$0], [$1])

dnl We need to keep our own list of libobjs separate from our parent project,
dnl and the easiest way to do that is redefine the AC_LIBOBJs macro while
dnl we look for our own LIBOBJs.
m4_pushdef([AC_LIBOBJ], m4_defn([_LT_LIBOBJ]))
m4_pushdef([AC_LIBSOURCES])

dnl If not otherwise defined, default to the 1.5.x compatible subproject mode:
m4_if(_LTDL_MODE, [],
        [m4_define([_LTDL_MODE], m4_default([$2], [subproject]))
        m4_if([-1], [m4_bregexp(_LTDL_MODE, [\(subproject\|\(non\)?recursive\)])],
                [m4_fatal([unknown libltdl mode: ]_LTDL_MODE)])])

AC_ARG_WITH([included_ltdl],
    [AS_HELP_STRING([--with-included-ltdl],
                    [use the GNU ltdl sources included here])])

if test yes != "$with_included_ltdl"; then
  # We are not being forced to use the included libltdl sources, so
  # decide whether there is a useful installed version we can use.
  AC_CHECK_HEADER([ltdl.h],
      [AC_CHECK_DECL([lt_dlinterface_register],
	   [AC_CHECK_LIB([ltdl], [lt_dladvise_preload],
	       [with_included_ltdl=no],
	       [with_included_ltdl=yes])],
	   [with_included_ltdl=yes],
	   [AC_INCLUDES_DEFAULT
	    #include <ltdl.h>])],
      [with_included_ltdl=yes],
      [AC_INCLUDES_DEFAULT]
  )
fi

dnl If neither LT_CONFIG_LTDL_DIR, LTDL_CONVENIENCE nor LTDL_INSTALLABLE
dnl was called yet, then for old times' sake, we assume libltdl is in an
dnl eponymous directory:
AC_PROVIDE_IFELSE([LT_CONFIG_LTDL_DIR], [], [_LT_CONFIG_LTDL_DIR([libltdl])])

AC_ARG_WITH([ltdl_include],
    [AS_HELP_STRING([--with-ltdl-include=DIR],
                    [use the ltdl headers installed in DIR])])

if test -n "$with_ltdl_include"; then
  if test -f "$with_ltdl_include/ltdl.h"; then :
  else
    AC_MSG_ERROR([invalid ltdl include directory: '$with_ltdl_include'])
  fi
else
  with_ltdl_include=no
fi

AC_ARG_WITH([ltdl_lib],
    [AS_HELP_STRING([--with-ltdl-lib=DIR],
                    [use the libltdl.la installed in DIR])])

if test -n "$with_ltdl_lib"; then
  if test -f "$with_ltdl_lib/libltdl.la"; then :
  else
    AC_MSG_ERROR([invalid ltdl library directory: '$with_ltdl_lib'])
  fi
else
  with_ltdl_lib=no
fi

case ,$with_included_ltdl,$with_ltdl_include,$with_ltdl_lib, in
  ,yes,no,no,)
	m4_case(m4_default(_LTDL_TYPE, [convenience]),
	    [convenience], [_LTDL_CONVENIENCE],
	    [installable], [_LTDL_INSTALLABLE],
	  [m4_fatal([unknown libltdl build type: ]_LTDL_TYPE)])
	;;
  ,no,no,no,)
	# If the included ltdl is not to be used, then use the
	# preinstalled libltdl we found.
	AC_DEFINE([HAVE_LTDL], [1],
	  [Define this if a modern libltdl is already installed])
	LIBLTDL=-lltdl
	LTDLDEPS=
	LTDLINCL=
	;;
  ,no*,no,*)
	AC_MSG_ERROR(['--with-ltdl-include' and '--with-ltdl-lib' options must be used together])
	;;
  *)	with_included_ltdl=no
	LIBLTDL="-L$with_ltdl_lib -lltdl"
	LTDLDEPS=
	LTDLINCL=-I$with_ltdl_include
	;;
esac
INCLTDL=$LTDLINCL

# Report our decision...
AC_MSG_CHECKING([where to find libltdl headers])
AC_MSG_RESULT([$LTDLINCL])
AC_MSG_CHECKING([where to find libltdl library])
AC_MSG_RESULT([$LIBLTDL])

_LTDL_SETUP

dnl restore autoconf definition.
m4_popdef([AC_LIBOBJ])
m4_popdef([AC_LIBSOURCES])

AC_CONFIG_COMMANDS_PRE([
    _ltdl_libobjs=
    _ltdl_ltlibobjs=
    if test -n "$_LT_LIBOBJS"; then
      # Remove the extension.
      _lt_sed_drop_objext='s/\.o$//;s/\.obj$//'
      for i in `for i in $_LT_LIBOBJS; do echo "$i"; done | sed "$_lt_sed_drop_objext" | sort -u`; do
        _ltdl_libobjs="$_ltdl_libobjs $lt_libobj_prefix$i.$ac_objext"
        _ltdl_ltlibobjs="$_ltdl_ltlibobjs $lt_libobj_prefix$i.lo"
      done
    fi
    AC_SUBST([ltdl_LIBOBJS], [$_ltdl_libobjs])
    AC_SUBST([ltdl_LTLIBOBJS], [$_ltdl_ltlibobjs])
])

# Only expand once:
m4_define([LTDL_INIT])
])# LTDL_INIT

# Old names:
AU_DEFUN([AC_LIB_LTDL], [LTDL_INIT($@)])
AU_DEFUN([AC_WITH_LTDL], [LTDL_INIT($@)])
AU_DEFUN([LT_WITH_LTDL], [LTDL_INIT($@)])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LIB_LTDL], [])
dnl AC_DEFUN([AC_WITH_LTDL], [])
dnl AC_DEFUN([LT_WITH_LTDL], [])


# _LTDL_SETUP
# -----------
# Perform all the checks necessary for compilation of the ltdl objects
#  -- including compiler checks and header checks.  This is a public
# interface  mainly for the benefit of libltdl's own configure.ac, most
# other users should call LTDL_INIT instead.
AC_DEFUN([_LTDL_SETUP],
[AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([LT_SYS_MODULE_EXT])dnl
AC_REQUIRE([LT_SYS_MODULE_PATH])dnl
AC_REQUIRE([LT_SYS_DLSEARCH_PATH])dnl
AC_REQUIRE([LT_LIB_DLLOAD])dnl
AC_REQUIRE([LT_SYS_SYMBOL_USCORE])dnl
AC_REQUIRE([LT_FUNC_DLSYM_USCORE])dnl
AC_REQUIRE([LT_SYS_DLOPEN_DEPLIBS])dnl
AC_REQUIRE([LT_FUNC_ARGZ])dnl

m4_require([_LT_CHECK_OBJDIR])dnl
m4_require([_LT_HEADER_DLFCN])dnl
m4_require([_LT_CHECK_DLPREOPEN])dnl
m4_require([_LT_DECL_SED])dnl

dnl Don't require this, or it will be expanded earlier than the code
dnl that sets the variables it relies on:
_LT_ENABLE_INSTALL

dnl _LTDL_MODE specific code must be called at least once:
_LTDL_MODE_DISPATCH

# In order that ltdl.c can compile, find out the first AC_CONFIG_HEADERS
# the user used.  This is so that ltdl.h can pick up the parent projects
# config.h file, The first file in AC_CONFIG_HEADERS must contain the
# definitions required by ltdl.c.
# FIXME: Remove use of undocumented AC_LIST_HEADERS (2.59 compatibility).
AC_CONFIG_COMMANDS_PRE([dnl
m4_pattern_allow([^LT_CONFIG_H$])dnl
m4_ifset([AH_HEADER],
    [LT_CONFIG_H=AH_HEADER],
    [m4_ifset([AC_LIST_HEADERS],
	    [LT_CONFIG_H=`echo "AC_LIST_HEADERS" | $SED 's|^[[      ]]*||;s|[[ :]].*$||'`],
	[])])])
AC_SUBST([LT_CONFIG_H])

AC_CHECK_HEADERS([unistd.h dl.h sys/dl.h dld.h mach-o/dyld.h dirent.h],
	[], [], [AC_INCLUDES_DEFAULT])

AC_CHECK_FUNCS([closedir opendir readdir], [], [AC_LIBOBJ([lt__dirent])])
AC_CHECK_FUNCS([strlcat strlcpy], [], [AC_LIBOBJ([lt__strl])])

m4_pattern_allow([LT_LIBEXT])dnl
AC_DEFINE_UNQUOTED([LT_LIBEXT],["$libext"],[The archive extension])

name=
eval "lt_libprefix=\"$libname_spec\""
m4_pattern_allow([LT_LIBPREFIX])dnl
AC_DEFINE_UNQUOTED([LT_LIBPREFIX],["$lt_libprefix"],[The archive prefix])

name=ltdl
eval "LTDLOPEN=\"$libname_spec\""
AC_SUBST([LTDLOPEN])
])# _LTDL_SETUP


# _LT_ENABLE_INSTALL
# ------------------
m4_define([_LT_ENABLE_INSTALL],
[AC_ARG_ENABLE([ltdl-install],
    [AS_HELP_STRING([--enable-ltdl-install], [install libltdl])])

case ,$enable_ltdl_install,$enable_ltdl_convenience in
  *yes*) ;;
  *) enable_ltdl_convenience=yes ;;
esac

m4_ifdef([AM_CONDITIONAL],
[AM_CONDITIONAL(INSTALL_LTDL, test no != "${enable_ltdl_install-no}")
 AM_CONDITIONAL(CONVENIENCE_LTDL, test no != "${enable_ltdl_convenience-no}")])
])# _LT_ENABLE_INSTALL


# LT_SYS_DLOPEN_DEPLIBS
# ---------------------
AC_DEFUN([LT_SYS_DLOPEN_DEPLIBS],
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_CACHE_CHECK([whether deplibs are loaded by dlopen],
  [lt_cv_sys_dlopen_deplibs],
  [# PORTME does your system automatically load deplibs for dlopen?
  # or its logical equivalent (e.g. shl_load for HP-UX < 11)
  # For now, we just catch OSes we know something about -- in the
  # future, we'll try test this programmatically.
  lt_cv_sys_dlopen_deplibs=unknown
  case $host_os in
  aix3*|aix4.1.*|aix4.2.*)
    # Unknown whether this is true for these versions of AIX, but
    # we want this 'case' here to explicitly catch those versions.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  aix[[4-9]]*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  amigaos*)
    case $host_cpu in
    powerpc)
      lt_cv_sys_dlopen_deplibs=no
      ;;
    esac
    ;;
  bitrig*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  darwin*)
    # Assuming the user has installed a libdl from somewhere, this is true
    # If you are looking for one http://www.opendarwin.org/projects/dlcompat
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  freebsd* | dragonfly*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  gnu* | linux* | k*bsd*-gnu | kopensolaris*-gnu)
    # GNU and its variants, using gnu ld.so (Glibc)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  hpux10*|hpux11*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  interix*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  irix[[12345]]*|irix6.[[01]]*)
    # Catch all versions of IRIX before 6.2, and indicate that we don't
    # know how it worked for any of those versions.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  irix*)
    # The case above catches anything before 6.2, and it's known that
    # at 6.2 and later dlopen does load deplibs.
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  netbsd*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  openbsd*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  osf[[1234]]*)
    # dlopen did load deplibs (at least at 4.x), but until the 5.x series,
    # it did *not* use an RPATH in a shared library to find objects the
    # library depends on, so we explicitly say 'no'.
    lt_cv_sys_dlopen_deplibs=no
    ;;
  osf5.0|osf5.0a|osf5.1)
    # dlopen *does* load deplibs and with the right loader patch applied
    # it even uses RPATH in a shared library to search for shared objects
    # that the library depends on, but there's no easy way to know if that
    # patch is installed.  Since this is the case, all we can really
    # say is unknown -- it depends on the patch being installed.  If
    # it is, this changes to 'yes'.  Without it, it would be 'no'.
    lt_cv_sys_dlopen_deplibs=unknown
    ;;
  osf*)
    # the two cases above should catch all versions of osf <= 5.1.  Read
    # the comments above for what we know about them.
    # At > 5.1, deplibs are loaded *and* any RPATH in a shared library
    # is used to find them so we can finally say 'yes'.
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  qnx*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  solaris*)
    lt_cv_sys_dlopen_deplibs=yes
    ;;
  sysv5* | sco3.2v5* | sco5v6* | unixware* | OpenUNIX* | sysv4*uw2*)
    libltdl_cv_sys_dlopen_deplibs=yes
    ;;
  esac
  ])
if test yes != "$lt_cv_sys_dlopen_deplibs"; then
 AC_DEFINE([LTDL_DLOPEN_DEPLIBS], [1],
    [Define if the OS needs help to load dependent libraries for dlopen().])
fi
])# LT_SYS_DLOPEN_DEPLIBS

# Old name:
AU_ALIAS([AC_LTDL_SYS_DLOPEN_DEPLIBS], [LT_SYS_DLOPEN_DEPLIBS])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYS_DLOPEN_DEPLIBS], [])


# LT_SYS_MODULE_EXT
# -----------------
AC_DEFUN([LT_SYS_MODULE_EXT],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([what extension is used for runtime loadable modules],
  [libltdl_cv_shlibext],
[
module=yes
eval libltdl_cv_shlibext=$shrext_cmds
module=no
eval libltdl_cv_shrext=$shrext_cmds
  ])
if test -n "$libltdl_cv_shlibext"; then
  m4_pattern_allow([LT_MODULE_EXT])dnl
  AC_DEFINE_UNQUOTED([LT_MODULE_EXT], ["$libltdl_cv_shlibext"],
    [Define to the extension used for runtime loadable modules, say, ".so".])
fi
if test "$libltdl_cv_shrext" != "$libltdl_cv_shlibext"; then
  m4_pattern_allow([LT_SHARED_EXT])dnl
  AC_DEFINE_UNQUOTED([LT_SHARED_EXT], ["$libltdl_cv_shrext"],
    [Define to the shared library suffix, say, ".dylib".])
fi
if test -n "$shared_archive_member_spec"; then
  m4_pattern_allow([LT_SHARED_LIB_MEMBER])dnl
  AC_DEFINE_UNQUOTED([LT_SHARED_LIB_MEMBER], ["($shared_archive_member_spec.o)"],
    [Define to the shared archive member specification, say "(shr.o)".])
fi
])# LT_SYS_MODULE_EXT

# Old name:
AU_ALIAS([AC_LTDL_SHLIBEXT], [LT_SYS_MODULE_EXT])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SHLIBEXT], [])


# LT_SYS_MODULE_PATH
# ------------------
AC_DEFUN([LT_SYS_MODULE_PATH],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([what variable specifies run-time module search path],
  [lt_cv_module_path_var], [lt_cv_module_path_var=$shlibpath_var])
if test -n "$lt_cv_module_path_var"; then
  m4_pattern_allow([LT_MODULE_PATH_VAR])dnl
  AC_DEFINE_UNQUOTED([LT_MODULE_PATH_VAR], ["$lt_cv_module_path_var"],
    [Define to the name of the environment variable that determines the run-time module search path.])
fi
])# LT_SYS_MODULE_PATH

# Old name:
AU_ALIAS([AC_LTDL_SHLIBPATH], [LT_SYS_MODULE_PATH])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SHLIBPATH], [])


# LT_SYS_DLSEARCH_PATH
# --------------------
AC_DEFUN([LT_SYS_DLSEARCH_PATH],
[m4_require([_LT_SYS_DYNAMIC_LINKER])dnl
AC_CACHE_CHECK([for the default library search path],
  [lt_cv_sys_dlsearch_path],
  [lt_cv_sys_dlsearch_path=$sys_lib_dlsearch_path_spec])
if test -n "$lt_cv_sys_dlsearch_path"; then
  sys_dlsearch_path=
  for dir in $lt_cv_sys_dlsearch_path; do
    if test -z "$sys_dlsearch_path"; then
      sys_dlsearch_path=$dir
    else
      sys_dlsearch_path=$sys_dlsearch_path$PATH_SEPARATOR$dir
    fi
  done
  m4_pattern_allow([LT_DLSEARCH_PATH])dnl
  AC_DEFINE_UNQUOTED([LT_DLSEARCH_PATH], ["$sys_dlsearch_path"],
    [Define to the system default library search path.])
fi
])# LT_SYS_DLSEARCH_PATH

# Old name:
AU_ALIAS([AC_LTDL_SYSSEARCHPATH], [LT_SYS_DLSEARCH_PATH])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYSSEARCHPATH], [])


# _LT_CHECK_DLPREOPEN
# -------------------
m4_defun([_LT_CHECK_DLPREOPEN],
[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
AC_CACHE_CHECK([whether libtool supports -dlopen/-dlpreopen],
  [libltdl_cv_preloaded_symbols],
  [if test -n "$lt_cv_sys_global_symbol_pipe"; then
    libltdl_cv_preloaded_symbols=yes
  else
    libltdl_cv_preloaded_symbols=no
  fi
  ])
if test yes = "$libltdl_cv_preloaded_symbols"; then
  AC_DEFINE([HAVE_PRELOADED_SYMBOLS], [1],
    [Define if libtool can extract symbol lists from object files.])
fi
])# _LT_CHECK_DLPREOPEN


# LT_LIB_DLLOAD
# -------------
AC_DEFUN([LT_LIB_DLLOAD],
[m4_pattern_allow([^LT_DLLOADERS$])
LT_DLLOADERS=
AC_SUBST([LT_DLLOADERS])

AC_LANG_PUSH([C])
lt_dlload_save_LIBS=$LIBS

LIBADD_DLOPEN=
AC_SEARCH_LIBS([dlopen], [dl],
	[AC_DEFINE([HAVE_LIBDL], [1],
		   [Define if you have the libdl library or equivalent.])
	if test "$ac_cv_search_dlopen" != "none required"; then
	  LIBADD_DLOPEN=-ldl
	fi
	libltdl_cv_lib_dl_dlopen=yes
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"],
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([[#if HAVE_DLFCN_H
#  include <dlfcn.h>
#endif
    ]], [[dlopen(0, 0);]])],
	    [AC_DEFINE([HAVE_LIBDL], [1],
		       [Define if you have the libdl library or equivalent.])
	    libltdl_cv_func_dlopen=yes
	    LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"],
	[AC_CHECK_LIB([svld], [dlopen],
		[AC_DEFINE([HAVE_LIBDL], [1],
			 [Define if you have the libdl library or equivalent.])
	        LIBADD_DLOPEN=-lsvld libltdl_cv_func_dlopen=yes
		LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dlopen.la"])])])
if test yes = "$libltdl_cv_func_dlopen" || test yes = "$libltdl_cv_lib_dl_dlopen"
then
  lt_save_LIBS=$LIBS
  LIBS="$LIBS $LIBADD_DLOPEN"
  AC_CHECK_FUNCS([dlerror])
  LIBS=$lt_save_LIBS
fi
AC_SUBST([LIBADD_DLOPEN])

LIBADD_SHL_LOAD=
AC_CHECK_FUNC([shl_load],
	[AC_DEFINE([HAVE_SHL_LOAD], [1],
		   [Define if you have the shl_load function.])
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}shl_load.la"],
    [AC_CHECK_LIB([dld], [shl_load],
	    [AC_DEFINE([HAVE_SHL_LOAD], [1],
		       [Define if you have the shl_load function.])
	    LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}shl_load.la"
	    LIBADD_SHL_LOAD=-ldld])])
AC_SUBST([LIBADD_SHL_LOAD])

case $host_os in
darwin[[1567]].*)
# We only want this for pre-Mac OS X 10.4.
  AC_CHECK_FUNC([_dyld_func_lookup],
	[AC_DEFINE([HAVE_DYLD], [1],
		   [Define if you have the _dyld_func_lookup function.])
	LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dyld.la"])
  ;;
beos*)
  LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}load_add_on.la"
  ;;
cygwin* | mingw* | pw32*)
  AC_CHECK_DECLS([cygwin_conv_path], [], [], [[#include <sys/cygwin.h>]])
  LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}loadlibrary.la"
  ;;
esac

AC_CHECK_LIB([dld], [dld_link],
	[AC_DEFINE([HAVE_DLD], [1],
		   [Define if you have the GNU dld library.])
		LT_DLLOADERS="$LT_DLLOADERS ${lt_dlopen_dir+$lt_dlopen_dir/}dld_link.la"])
AC_SUBST([LIBADD_DLD_LINK])

m4_pattern_allow([^LT_DLPREOPEN$])
LT_DLPREOPEN=
if test -n "$LT_DLLOADERS"
then
  for lt_loader in $LT_DLLOADERS; do
    LT_DLPREOPEN="$LT_DLPREOPEN-dlpreopen $lt_loader "
  done
  AC_DEFINE([HAVE_LIBDLLOADER], [1],
            [Define if libdlloader will be built on this platform])
fi
AC_SUBST([LT_DLPREOPEN])

dnl This isn't used anymore, but set it for backwards compatibility
LIBADD_DL="$LIBADD_DLOPEN $LIBADD_SHL_LOAD"
AC_SUBST([LIBADD_DL])

LIBS=$lt_dlload_save_LIBS
AC_LANG_POP
])# LT_LIB_DLLOAD

# Old name:
AU_ALIAS([AC_LTDL_DLLIB], [LT_LIB_DLLOAD])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_DLLIB], [])


# LT_SYS_SYMBOL_USCORE
# --------------------
# does the compiler prefix global symbols with an underscore?
AC_DEFUN([LT_SYS_SYMBOL_USCORE],
[m4_require([_LT_CMD_GLOBAL_SYMBOLS])dnl
AC_CACHE_CHECK([for _ prefix in compiled symbols],
  [lt_cv_sys_symbol_underscore],
  [lt_cv_sys_symbol_underscore=no
  cat > conftest.$ac_ext <<_LT_EOF
void nm_test_func(){}
int main(){nm_test_func;return 0;}
_LT_EOF
  if AC_TRY_EVAL(ac_compile); then
    # Now try to grab the symbols.
    ac_nlist=conftest.nm
    if AC_TRY_EVAL(NM conftest.$ac_objext \| $lt_cv_sys_global_symbol_pipe \> $ac_nlist) && test -s "$ac_nlist"; then
      # See whether the symbols have a leading underscore.
      if grep '^. _nm_test_func' "$ac_nlist" >/dev/null; then
        lt_cv_sys_symbol_underscore=yes
      else
        if grep '^. nm_test_func ' "$ac_nlist" >/dev/null; then
	  :
        else
	  echo "configure: cannot find nm_test_func in $ac_nlist" >&AS_MESSAGE_LOG_FD
        fi
      fi
    else
      echo "configure: cannot run $lt_cv_sys_global_symbol_pipe" >&AS_MESSAGE_LOG_FD
    fi
  else
    echo "configure: failed program was:" >&AS_MESSAGE_LOG_FD
    cat conftest.c >&AS_MESSAGE_LOG_FD
  fi
  rm -rf conftest*
  ])
  sys_symbol_underscore=$lt_cv_sys_symbol_underscore
  AC_SUBST([sys_symbol_underscore])
])# LT_SYS_SYMBOL_USCORE

# Old name:
AU_ALIAS([AC_LTDL_SYMBOL_USCORE], [LT_SYS_SYMBOL_USCORE])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_SYMBOL_USCORE], [])


# LT_FUNC_DLSYM_USCORE
# --------------------
AC_DEFUN([LT_FUNC_DLSYM_USCORE],
[AC_REQUIRE([_LT_COMPILER_PIC])dnl	for lt_prog_compiler_wl
AC_REQUIRE([LT_SYS_SYMBOL_USCORE])dnl	for lt_cv_sys_symbol_underscore
AC_REQUIRE([LT_SYS_MODULE_EXT])dnl	for libltdl_cv_shlibext
if test yes = "$lt_cv_sys_symbol_underscore"; then
  if test yes = "$libltdl_cv_func_dlopen" || test yes = "$libltdl_cv_lib_dl_dlopen"; then
    AC_CACHE_CHECK([whether we have to add an underscore for dlsym],
      [libltdl_cv_need_uscore],
      [libltdl_cv_need_uscore=unknown
      dlsym_uscore_save_LIBS=$LIBS
      LIBS="$LIBS $LIBADD_DLOPEN"
      libname=conftmod # stay within 8.3 filename limits!
      cat >$libname.$ac_ext <<_LT_EOF
[#line $LINENO "configure"
#include "confdefs.h"
/* When -fvisibility=hidden is used, assume the code has been annotated
   correspondingly for the symbols needed.  */
#if defined __GNUC__ && (((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)) || (__GNUC__ > 3))
int fnord () __attribute__((visibility("default")));
#endif
int fnord () { return 42; }]
_LT_EOF

      # ltfn_module_cmds module_cmds
      # Execute tilde-delimited MODULE_CMDS with environment primed for
      # $module_cmds or $archive_cmds type content.
      ltfn_module_cmds ()
      {( # subshell avoids polluting parent global environment
          module_cmds_save_ifs=$IFS; IFS='~'
          for cmd in @S|@1; do
            IFS=$module_cmds_save_ifs
            libobjs=$libname.$ac_objext; lib=$libname$libltdl_cv_shlibext
            rpath=/not-exists; soname=$libname$libltdl_cv_shlibext; output_objdir=.
            major=; versuffix=; verstring=; deplibs=
            ECHO=echo; wl=$lt_prog_compiler_wl; allow_undefined_flag=
            eval $cmd
          done
          IFS=$module_cmds_save_ifs
      )}

      # Compile a loadable module using libtool macro expansion results.
      $CC $pic_flag -c $libname.$ac_ext
      ltfn_module_cmds "${module_cmds:-$archive_cmds}"

      # Try to fetch fnord with dlsym().
      libltdl_dlunknown=0; libltdl_dlnouscore=1; libltdl_dluscore=2
      cat >conftest.$ac_ext <<_LT_EOF
[#line $LINENO "configure"
#include "confdefs.h"
#if HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#include <stdio.h>
#ifndef RTLD_GLOBAL
#  ifdef DL_GLOBAL
#    define RTLD_GLOBAL DL_GLOBAL
#  else
#    define RTLD_GLOBAL 0
#  endif
#endif
#ifndef RTLD_NOW
#  ifdef DL_NOW
#    define RTLD_NOW DL_NOW
#  else
#    define RTLD_NOW 0
#  endif
#endif
int main () {
  void *handle = dlopen ("`pwd`/$libname$libltdl_cv_shlibext", RTLD_GLOBAL|RTLD_NOW);
  int status = $libltdl_dlunknown;
  if (handle) {
    if (dlsym (handle, "fnord"))
      status = $libltdl_dlnouscore;
    else {
      if (dlsym (handle, "_fnord"))
        status = $libltdl_dluscore;
      else
	puts (dlerror ());
    }
    dlclose (handle);
  } else
    puts (dlerror ());
  return status;
}]
_LT_EOF
      if AC_TRY_EVAL(ac_link) && test -s "conftest$ac_exeext" 2>/dev/null; then
        (./conftest; exit; ) >&AS_MESSAGE_LOG_FD 2>/dev/null
        libltdl_status=$?
        case x$libltdl_status in
          x$libltdl_dlnouscore) libltdl_cv_need_uscore=no ;;
	  x$libltdl_dluscore) libltdl_cv_need_uscore=yes ;;
	  x*) libltdl_cv_need_uscore=unknown ;;
        esac
      fi
      rm -rf conftest* $libname*
      LIBS=$dlsym_uscore_save_LIBS
    ])
  fi
fi

if test yes = "$libltdl_cv_need_uscore"; then
  AC_DEFINE([NEED_USCORE], [1],
    [Define if dlsym() requires a leading underscore in symbol names.])
fi
])# LT_FUNC_DLSYM_USCORE

# Old name:
AU_ALIAS([AC_LTDL_DLSYM_USCORE], [LT_FUNC_DLSYM_USCORE])
dnl aclocal-1.4 backwards compatibility:
dnl AC_DEFUN([AC_LTDL_DLSYM_USCORE], [])

# Copyright (C) 2002-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_AUTOMAKE_VERSION(VERSION)
# ----------------------------
# Automake X.Y traces this macro to ensure aclocal.m4 has been
# generated from the m4 files accompanying Automake X.Y.
# (This private macro should not be called outside this file.)
AC_DEFUN([AM_AUTOMAKE_VERSION],
[am__api_version='1.16'
dnl Some users find AM_AUTOMAKE_VERSION and mistake it for a way to
dnl require some minimum version.  Point them to the right macro.
m4_if([$1], [1.16.3], [],
      [AC_FATAL([Do not call $0, use AM_INIT_AUTOMAKE([$1]).])])dnl
])

# _AM_AUTOCONF_VERSION(VERSION)
# -----------------------------
# aclocal traces this macro to find the Autoconf version.
# This is a private macro too.  Using m4_define simplifies
# the logic in aclocal, which can simply ignore this definition.
m4_define([_AM_AUTOCONF_VERSION], [])

# AM_SET_CURRENT_AUTOMAKE_VERSION
# -------------------------------
# Call AM_AUTOMAKE_VERSION and AM_AUTOMAKE_VERSION so they can be traced.
# This function is AC_REQUIREd by AM_INIT_AUTOMAKE.
AC_DEFUN([AM_SET_CURRENT_AUTOMAKE_VERSION],
[AM_AUTOMAKE_VERSION([1.16.3])dnl
m4_ifndef([AC_AUTOCONF_VERSION],
  [m4_copy([m4_PACKAGE_VERSION], [AC_AUTOCONF_VERSION])])dnl
_AM_AUTOCONF_VERSION(m4_defn([AC_AUTOCONF_VERSION]))])

# Figure out how to run the assembler.                      -*- Autoconf -*-

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_PROG_AS
# ----------
AC_DEFUN([AM_PROG_AS],
[# By default we simply use the C compiler to build assembly code.
AC_REQUIRE([AC_PROG_CC])
test "${CCAS+set}" = set || CCAS=$CC
test "${CCASFLAGS+set}" = set || CCASFLAGS=$CFLAGS
AC_ARG_VAR([CCAS],      [assembler compiler command (defaults to CC)])
AC_ARG_VAR([CCASFLAGS], [assembler compiler flags (defaults to CFLAGS)])
_AM_IF_OPTION([no-dependencies],, [_AM_DEPENDENCIES([CCAS])])dnl
])

# AM_AUX_DIR_EXPAND                                         -*- Autoconf -*-

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# For projects using AC_CONFIG_AUX_DIR([foo]), Autoconf sets
# $ac_aux_dir to '$srcdir/foo'.  In other projects, it is set to
# '$srcdir', '$srcdir/..', or '$srcdir/../..'.
#
# Of course, Automake must honor this variable whenever it calls a
# tool from the auxiliary directory.  The problem is that $srcdir (and
# therefore $ac_aux_dir as well) can be either absolute or relative,
# depending on how configure is run.  This is pretty annoying, since
# it makes $ac_aux_dir quite unusable in subdirectories: in the top
# source directory, any form will work fine, but in subdirectories a
# relative path needs to be adjusted first.
#
# $ac_aux_dir/missing
#    fails when called from a subdirectory if $ac_aux_dir is relative
# $top_srcdir/$ac_aux_dir/missing
#    fails if $ac_aux_dir is absolute,
#    fails when called from a subdirectory in a VPATH build with
#          a relative $ac_aux_dir
#
# The reason of the latter failure is that $top_srcdir and $ac_aux_dir
# are both prefixed by $srcdir.  In an in-source build this is usually
# harmless because $srcdir is '.', but things will broke when you
# start a VPATH build or use an absolute $srcdir.
#
# So we could use something similar to $top_srcdir/$ac_aux_dir/missing,
# iff we strip the leading $srcdir from $ac_aux_dir.  That would be:
#   am_aux_dir='\$(top_srcdir)/'`expr "$ac_aux_dir" : "$srcdir//*\(.*\)"`
# and then we would define $MISSING as
#   MISSING="\${SHELL} $am_aux_dir/missing"
# This will work as long as MISSING is not called from configure, because
# unfortunately $(top_srcdir) has no meaning in configure.
# However there are other variables, like CC, which are often used in
# configure, and could therefore not use this "fixed" $ac_aux_dir.
#
# Another solution, used here, is to always expand $ac_aux_dir to an
# absolute PATH.  The drawback is that using absolute paths prevent a
# configured tree to be moved without reconfiguration.

AC_DEFUN([AM_AUX_DIR_EXPAND],
[AC_REQUIRE([AC_CONFIG_AUX_DIR_DEFAULT])dnl
# Expand $ac_aux_dir to an absolute path.
am_aux_dir=`cd "$ac_aux_dir" && pwd`
])

# AM_CONDITIONAL                                            -*- Autoconf -*-

# Copyright (C) 1997-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_CONDITIONAL(NAME, SHELL-CONDITION)
# -------------------------------------
# Define a conditional.
AC_DEFUN([AM_CONDITIONAL],
[AC_PREREQ([2.52])dnl
 m4_if([$1], [TRUE],  [AC_FATAL([$0: invalid condition: $1])],
       [$1], [FALSE], [AC_FATAL([$0: invalid condition: $1])])dnl
AC_SUBST([$1_TRUE])dnl
AC_SUBST([$1_FALSE])dnl
_AM_SUBST_NOTMAKE([$1_TRUE])dnl
_AM_SUBST_NOTMAKE([$1_FALSE])dnl
m4_define([_AM_COND_VALUE_$1], [$2])dnl
if $2; then
  $1_TRUE=
  $1_FALSE='#'
else
  $1_TRUE='#'
  $1_FALSE=
fi
AC_CONFIG_COMMANDS_PRE(
[if test -z "${$1_TRUE}" && test -z "${$1_FALSE}"; then
  AC_MSG_ERROR([[conditional "$1" was never defined.
Usually this means the macro was only invoked conditionally.]])
fi])])

# Copyright (C) 1999-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.


# There are a few dirty hacks below to avoid letting 'AC_PROG_CC' be
# written in clear, in which case automake, when reading aclocal.m4,
# will think it sees a *use*, and therefore will trigger all it's
# C support machinery.  Also note that it means that autoscan, seeing
# CC etc. in the Makefile, will ask for an AC_PROG_CC use...


# _AM_DEPENDENCIES(NAME)
# ----------------------
# See how the compiler implements dependency checking.
# NAME is "CC", "CXX", "OBJC", "OBJCXX", "UPC", or "GJC".
# We try a few techniques and use that to set a single cache variable.
#
# We don't AC_REQUIRE the corresponding AC_PROG_CC since the latter was
# modified to invoke _AM_DEPENDENCIES(CC); we would have a circular
# dependency, and given that the user is not expected to run this macro,
# just rely on AC_PROG_CC.
AC_DEFUN([_AM_DEPENDENCIES],
[AC_REQUIRE([AM_SET_DEPDIR])dnl
AC_REQUIRE([AM_OUTPUT_DEPENDENCY_COMMANDS])dnl
AC_REQUIRE([AM_MAKE_INCLUDE])dnl
AC_REQUIRE([AM_DEP_TRACK])dnl

m4_if([$1], [CC],   [depcc="$CC"   am_compiler_list=],
      [$1], [CXX],  [depcc="$CXX"  am_compiler_list=],
      [$1], [OBJC], [depcc="$OBJC" am_compiler_list='gcc3 gcc'],
      [$1], [OBJCXX], [depcc="$OBJCXX" am_compiler_list='gcc3 gcc'],
      [$1], [UPC],  [depcc="$UPC"  am_compiler_list=],
      [$1], [GCJ],  [depcc="$GCJ"  am_compiler_list='gcc3 gcc'],
                    [depcc="$$1"   am_compiler_list=])

AC_CACHE_CHECK([dependency style of $depcc],
               [am_cv_$1_dependencies_compiler_type],
[if test -z "$AMDEP_TRUE" && test -f "$am_depcomp"; then
  # We make a subdir and do the tests there.  Otherwise we can end up
  # making bogus files that we don't know about and never remove.  For
  # instance it was reported that on HP-UX the gcc test will end up
  # making a dummy file named 'D' -- because '-MD' means "put the output
  # in D".
  rm -rf conftest.dir
  mkdir conftest.dir
  # Copy depcomp to subdir because otherwise we won't find it if we're
  # using a relative directory.
  cp "$am_depcomp" conftest.dir
  cd conftest.dir
  # We will build objects and dependencies in a subdirectory because
  # it helps to detect inapplicable dependency modes.  For instance
  # both Tru64's cc and ICC support -MD to output dependencies as a
  # side effect of compilation, but ICC will put the dependencies in
  # the current directory while Tru64 will put them in the object
  # directory.
  mkdir sub

  am_cv_$1_dependencies_compiler_type=none
  if test "$am_compiler_list" = ""; then
     am_compiler_list=`sed -n ['s/^#*\([a-zA-Z0-9]*\))$/\1/p'] < ./depcomp`
  fi
  am__universal=false
  m4_case([$1], [CC],
    [case " $depcc " in #(
     *\ -arch\ *\ -arch\ *) am__universal=true ;;
     esac],
    [CXX],
    [case " $depcc " in #(
     *\ -arch\ *\ -arch\ *) am__universal=true ;;
     esac])

  for depmode in $am_compiler_list; do
    # Setup a source with many dependencies, because some compilers
    # like to wrap large dependency lists on column 80 (with \), and
    # we should not choose a depcomp mode which is confused by this.
    #
    # We need to recreate these files for each test, as the compiler may
    # overwrite some of them when testing with obscure command lines.
    # This happens at least with the AIX C compiler.
    : > sub/conftest.c
    for i in 1 2 3 4 5 6; do
      echo '#include "conftst'$i'.h"' >> sub/conftest.c
      # Using ": > sub/conftst$i.h" creates only sub/conftst1.h with
      # Solaris 10 /bin/sh.
      echo '/* dummy */' > sub/conftst$i.h
    done
    echo "${am__include} ${am__quote}sub/conftest.Po${am__quote}" > confmf

    # We check with '-c' and '-o' for the sake of the "dashmstdout"
    # mode.  It turns out that the SunPro C++ compiler does not properly
    # handle '-M -o', and we need to detect this.  Also, some Intel
    # versions had trouble with output in subdirs.
    am__obj=sub/conftest.${OBJEXT-o}
    am__minus_obj="-o $am__obj"
    case $depmode in
    gcc)
      # This depmode causes a compiler race in universal mode.
      test "$am__universal" = false || continue
      ;;
    nosideeffect)
      # After this tag, mechanisms are not by side-effect, so they'll
      # only be used when explicitly requested.
      if test "x$enable_dependency_tracking" = xyes; then
	continue
      else
	break
      fi
      ;;
    msvc7 | msvc7msys | msvisualcpp | msvcmsys)
      # This compiler won't grok '-c -o', but also, the minuso test has
      # not run yet.  These depmodes are late enough in the game, and
      # so weak that their functioning should not be impacted.
      am__obj=conftest.${OBJEXT-o}
      am__minus_obj=
      ;;
    none) break ;;
    esac
    if depmode=$depmode \
       source=sub/conftest.c object=$am__obj \
       depfile=sub/conftest.Po tmpdepfile=sub/conftest.TPo \
       $SHELL ./depcomp $depcc -c $am__minus_obj sub/conftest.c \
         >/dev/null 2>conftest.err &&
       grep sub/conftst1.h sub/conftest.Po > /dev/null 2>&1 &&
       grep sub/conftst6.h sub/conftest.Po > /dev/null 2>&1 &&
       grep $am__obj sub/conftest.Po > /dev/null 2>&1 &&
       ${MAKE-make} -s -f confmf > /dev/null 2>&1; then
      # icc doesn't choke on unknown options, it will just issue warnings
      # or remarks (even with -Werror).  So we grep stderr for any message
      # that says an option was ignored or not supported.
      # When given -MP, icc 7.0 and 7.1 complain thusly:
      #   icc: Command line warning: ignoring option '-M'; no argument required
      # The diagnosis changed in icc 8.0:
      #   icc: Command line remark: option '-MP' not supported
      if (grep 'ignoring option' conftest.err ||
          grep 'not supported' conftest.err) >/dev/null 2>&1; then :; else
        am_cv_$1_dependencies_compiler_type=$depmode
        break
      fi
    fi
  done

  cd ..
  rm -rf conftest.dir
else
  am_cv_$1_dependencies_compiler_type=none
fi
])
AC_SUBST([$1DEPMODE], [depmode=$am_cv_$1_dependencies_compiler_type])
AM_CONDITIONAL([am__fastdep$1], [
  test "x$enable_dependency_tracking" != xno \
  && test "$am_cv_$1_dependencies_compiler_type" = gcc3])
])


# AM_SET_DEPDIR
# -------------
# Choose a directory name for dependency files.
# This macro is AC_REQUIREd in _AM_DEPENDENCIES.
AC_DEFUN([AM_SET_DEPDIR],
[AC_REQUIRE([AM_SET_LEADING_DOT])dnl
AC_SUBST([DEPDIR], ["${am__leading_dot}deps"])dnl
])


# AM_DEP_TRACK
# ------------
AC_DEFUN([AM_DEP_TRACK],
[AC_ARG_ENABLE([dependency-tracking], [dnl
AS_HELP_STRING(
  [--enable-dependency-tracking],
  [do not reject slow dependency extractors])
AS_HELP_STRING(
  [--disable-dependency-tracking],
  [speeds up one-time build])])
if test "x$enable_dependency_tracking" != xno; then
  am_depcomp="$ac_aux_dir/depcomp"
  AMDEPBACKSLASH='\'
  am__nodep='_no'
fi
AM_CONDITIONAL([AMDEP], [test "x$enable_dependency_tracking" != xno])
AC_SUBST([AMDEPBACKSLASH])dnl
_AM_SUBST_NOTMAKE([AMDEPBACKSLASH])dnl
AC_SUBST([am__nodep])dnl
_AM_SUBST_NOTMAKE([am__nodep])dnl
])

# Generate code to set up dependency tracking.              -*- Autoconf -*-

# Copyright (C) 1999-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_OUTPUT_DEPENDENCY_COMMANDS
# ------------------------------
AC_DEFUN([_AM_OUTPUT_DEPENDENCY_COMMANDS],
[{
  # Older Autoconf quotes --file arguments for eval, but not when files
  # are listed without --file.  Let's play safe and only enable the eval
  # if we detect the quoting.
  # TODO: see whether this extra hack can be removed once we start
  # requiring Autoconf 2.70 or later.
  AS_CASE([$CONFIG_FILES],
          [*\'*], [eval set x "$CONFIG_FILES"],
          [*], [set x $CONFIG_FILES])
  shift
  # Used to flag and report bootstrapping failures.
  am_rc=0
  for am_mf
  do
    # Strip MF so we end up with the name of the file.
    am_mf=`AS_ECHO(["$am_mf"]) | sed -e 's/:.*$//'`
    # Check whether this is an Automake generated Makefile which includes
    # dependency-tracking related rules and includes.
    # Grep'ing the whole file directly is not great: AIX grep has a line
    # limit of 2048, but all sed's we know have understand at least 4000.
    sed -n 's,^am--depfiles:.*,X,p' "$am_mf" | grep X >/dev/null 2>&1 \
      || continue
    am_dirpart=`AS_DIRNAME(["$am_mf"])`
    am_filepart=`AS_BASENAME(["$am_mf"])`
    AM_RUN_LOG([cd "$am_dirpart" \
      && sed -e '/# am--include-marker/d' "$am_filepart" \
        | $MAKE -f - am--depfiles]) || am_rc=$?
  done
  if test $am_rc -ne 0; then
    AC_MSG_FAILURE([Something went wrong bootstrapping makefile fragments
    for automatic dependency tracking.  If GNU make was not used, consider
    re-running the configure script with MAKE="gmake" (or whatever is
    necessary).  You can also try re-running configure with the
    '--disable-dependency-tracking' option to at least be able to build
    the package (albeit without support for automatic dependency tracking).])
  fi
  AS_UNSET([am_dirpart])
  AS_UNSET([am_filepart])
  AS_UNSET([am_mf])
  AS_UNSET([am_rc])
  rm -f conftest-deps.mk
}
])# _AM_OUTPUT_DEPENDENCY_COMMANDS


# AM_OUTPUT_DEPENDENCY_COMMANDS
# -----------------------------
# This macro should only be invoked once -- use via AC_REQUIRE.
#
# This code is only required when automatic dependency tracking is enabled.
# This creates each '.Po' and '.Plo' makefile fragment that we'll need in
# order to bootstrap the dependency handling code.
AC_DEFUN([AM_OUTPUT_DEPENDENCY_COMMANDS],
[AC_CONFIG_COMMANDS([depfiles],
     [test x"$AMDEP_TRUE" != x"" || _AM_OUTPUT_DEPENDENCY_COMMANDS],
     [AMDEP_TRUE="$AMDEP_TRUE" MAKE="${MAKE-make}"])])

# Do all the work for Automake.                             -*- Autoconf -*-

# Copyright (C) 1996-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This macro actually does too much.  Some checks are only needed if
# your package does certain things.  But this isn't really a big deal.

dnl Redefine AC_PROG_CC to automatically invoke _AM_PROG_CC_C_O.
m4_define([AC_PROG_CC],
m4_defn([AC_PROG_CC])
[_AM_PROG_CC_C_O
])

# AM_INIT_AUTOMAKE(PACKAGE, VERSION, [NO-DEFINE])
# AM_INIT_AUTOMAKE([OPTIONS])
# -----------------------------------------------
# The call with PACKAGE and VERSION arguments is the old style
# call (pre autoconf-2.50), which is being phased out.  PACKAGE
# and VERSION should now be passed to AC_INIT and removed from
# the call to AM_INIT_AUTOMAKE.
# We support both call styles for the transition.  After
# the next Automake release, Autoconf can make the AC_INIT
# arguments mandatory, and then we can depend on a new Autoconf
# release and drop the old call support.
AC_DEFUN([AM_INIT_AUTOMAKE],
[AC_PREREQ([2.65])dnl
dnl Autoconf wants to disallow AM_ names.  We explicitly allow
dnl the ones we care about.
m4_pattern_allow([^AM_[A-Z]+FLAGS$])dnl
AC_REQUIRE([AM_SET_CURRENT_AUTOMAKE_VERSION])dnl
AC_REQUIRE([AC_PROG_INSTALL])dnl
if test "`cd $srcdir && pwd`" != "`pwd`"; then
  # Use -I$(srcdir) only when $(srcdir) != ., so that make's output
  # is not polluted with repeated "-I."
  AC_SUBST([am__isrc], [' -I$(srcdir)'])_AM_SUBST_NOTMAKE([am__isrc])dnl
  # test to see if srcdir already configured
  if test -f $srcdir/config.status; then
    AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
  fi
fi

# test whether we have cygpath
if test -z "$CYGPATH_W"; then
  if (cygpath --version) >/dev/null 2>/dev/null; then
    CYGPATH_W='cygpath -w'
  else
    CYGPATH_W=echo
  fi
fi
AC_SUBST([CYGPATH_W])

# Define the identity of the package.
dnl Distinguish between old-style and new-style calls.
m4_ifval([$2],
[AC_DIAGNOSE([obsolete],
             [$0: two- and three-arguments forms are deprecated.])
m4_ifval([$3], [_AM_SET_OPTION([no-define])])dnl
 AC_SUBST([PACKAGE], [$1])dnl
 AC_SUBST([VERSION], [$2])],
[_AM_SET_OPTIONS([$1])dnl
dnl Diagnose old-style AC_INIT with new-style AM_AUTOMAKE_INIT.
m4_if(
  m4_ifdef([AC_PACKAGE_NAME], [ok]):m4_ifdef([AC_PACKAGE_VERSION], [ok]),
  [ok:ok],,
  [m4_fatal([AC_INIT should be called with package and version arguments])])dnl
 AC_SUBST([PACKAGE], ['AC_PACKAGE_TARNAME'])dnl
 AC_SUBST([VERSION], ['AC_PACKAGE_VERSION'])])dnl

_AM_IF_OPTION([no-define],,
[AC_DEFINE_UNQUOTED([PACKAGE], ["$PACKAGE"], [Name of package])
 AC_DEFINE_UNQUOTED([VERSION], ["$VERSION"], [Version number of package])])dnl

# Some tools Automake needs.
AC_REQUIRE([AM_SANITY_CHECK])dnl
AC_REQUIRE([AC_ARG_PROGRAM])dnl
AM_MISSING_PROG([ACLOCAL], [aclocal-${am__api_version}])
AM_MISSING_PROG([AUTOCONF], [autoconf])
AM_MISSING_PROG([AUTOMAKE], [automake-${am__api_version}])
AM_MISSING_PROG([AUTOHEADER], [autoheader])
AM_MISSING_PROG([MAKEINFO], [makeinfo])
AC_REQUIRE([AM_PROG_INSTALL_SH])dnl
AC_REQUIRE([AM_PROG_INSTALL_STRIP])dnl
AC_REQUIRE([AC_PROG_MKDIR_P])dnl
# For better backward compatibility.  To be removed once Automake 1.9.x
# dies out for good.  For more background, see:
# <https://lists.gnu.org/archive/html/automake/2012-07/msg00001.html>
# <https://lists.gnu.org/archive/html/automake/2012-07/msg00014.html>
AC_SUBST([mkdir_p], ['$(MKDIR_P)'])
# We need awk for the "check" target (and possibly the TAP driver).  The
# system "awk" is bad on some platforms.
AC_REQUIRE([AC_PROG_AWK])dnl
AC_REQUIRE([AC_PROG_MAKE_SET])dnl
AC_REQUIRE([AM_SET_LEADING_DOT])dnl
_AM_IF_OPTION([tar-ustar], [_AM_PROG_TAR([ustar])],
	      [_AM_IF_OPTION([tar-pax], [_AM_PROG_TAR([pax])],
			     [_AM_PROG_TAR([v7])])])
_AM_IF_OPTION([no-dependencies],,
[AC_PROVIDE_IFELSE([AC_PROG_CC],
		  [_AM_DEPENDENCIES([CC])],
		  [m4_define([AC_PROG_CC],
			     m4_defn([AC_PROG_CC])[_AM_DEPENDENCIES([CC])])])dnl
AC_PROVIDE_IFELSE([AC_PROG_CXX],
		  [_AM_DEPENDENCIES([CXX])],
		  [m4_define([AC_PROG_CXX],
			     m4_defn([AC_PROG_CXX])[_AM_DEPENDENCIES([CXX])])])dnl
AC_PROVIDE_IFELSE([AC_PROG_OBJC],
		  [_AM_DEPENDENCIES([OBJC])],
		  [m4_define([AC_PROG_OBJC],
			     m4_defn([AC_PROG_OBJC])[_AM_DEPENDENCIES([OBJC])])])dnl
AC_PROVIDE_IFELSE([AC_PROG_OBJCXX],
		  [_AM_DEPENDENCIES([OBJCXX])],
		  [m4_define([AC_PROG_OBJCXX],
			     m4_defn([AC_PROG_OBJCXX])[_AM_DEPENDENCIES([OBJCXX])])])dnl
])
AC_REQUIRE([AM_SILENT_RULES])dnl
dnl The testsuite driver may need to know about EXEEXT, so add the
dnl 'am__EXEEXT' conditional if _AM_COMPILER_EXEEXT was seen.  This
dnl macro is hooked onto _AC_COMPILER_EXEEXT early, see below.
AC_CONFIG_COMMANDS_PRE(dnl
[m4_provide_if([_AM_COMPILER_EXEEXT],
  [AM_CONDITIONAL([am__EXEEXT], [test -n "$EXEEXT"])])])dnl

# POSIX will say in a future version that running "rm -f" with no argument
# is OK; and we want to be able to make that assumption in our Makefile
# recipes.  So use an aggressive probe to check that the usage we want is
# actually supported "in the wild" to an acceptable degree.
# See automake bug#10828.
# To make any issue more visible, cause the running configure to be aborted
# by default if the 'rm' program in use doesn't match our expectations; the
# user can still override this though.
if rm -f && rm -fr && rm -rf; then : OK; else
  cat >&2 <<'END'
Oops!

Your 'rm' program seems unable to run without file operands specified
on the command line, even when the '-f' option is present.  This is contrary
to the behaviour of most rm programs out there, and not conforming with
the upcoming POSIX standard: <http://austingroupbugs.net/view.php?id=542>

Please tell bug-automake@gnu.org about your system, including the value
of your $PATH and any error possibly output before this message.  This
can help us improve future automake versions.

END
  if test x"$ACCEPT_INFERIOR_RM_PROGRAM" = x"yes"; then
    echo 'Configuration will proceed anyway, since you have set the' >&2
    echo 'ACCEPT_INFERIOR_RM_PROGRAM variable to "yes"' >&2
    echo >&2
  else
    cat >&2 <<'END'
Aborting the configuration process, to ensure you take notice of the issue.

You can download and install GNU coreutils to get an 'rm' implementation
that behaves properly: <https://www.gnu.org/software/coreutils/>.

If you want to complete the configuration process using your problematic
'rm' anyway, export the environment variable ACCEPT_INFERIOR_RM_PROGRAM
to "yes", and re-run configure.

END
    AC_MSG_ERROR([Your 'rm' program is bad, sorry.])
  fi
fi
dnl The trailing newline in this macro's definition is deliberate, for
dnl backward compatibility and to allow trailing 'dnl'-style comments
dnl after the AM_INIT_AUTOMAKE invocation. See automake bug#16841.
])

dnl Hook into '_AC_COMPILER_EXEEXT' early to learn its expansion.  Do not
dnl add the conditional right here, as _AC_COMPILER_EXEEXT may be further
dnl mangled by Autoconf and run in a shell conditional statement.
m4_define([_AC_COMPILER_EXEEXT],
m4_defn([_AC_COMPILER_EXEEXT])[m4_provide([_AM_COMPILER_EXEEXT])])

# When config.status generates a header, we must update the stamp-h file.
# This file resides in the same directory as the config header
# that is generated.  The stamp files are numbered to have different names.

# Autoconf calls _AC_AM_CONFIG_HEADER_HOOK (when defined) in the
# loop where config.status creates the headers, so we can generate
# our stamp files there.
AC_DEFUN([_AC_AM_CONFIG_HEADER_HOOK],
[# Compute $1's index in $config_headers.
_am_arg=$1
_am_stamp_count=1
for _am_header in $config_headers :; do
  case $_am_header in
    $_am_arg | $_am_arg:* )
      break ;;
    * )
      _am_stamp_count=`expr $_am_stamp_count + 1` ;;
  esac
done
echo "timestamp for $_am_arg" >`AS_DIRNAME(["$_am_arg"])`/stamp-h[]$_am_stamp_count])

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_PROG_INSTALL_SH
# ------------------
# Define $install_sh.
AC_DEFUN([AM_PROG_INSTALL_SH],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
if test x"${install_sh+set}" != xset; then
  case $am_aux_dir in
  *\ * | *\	*)
    install_sh="\${SHELL} '$am_aux_dir/install-sh'" ;;
  *)
    install_sh="\${SHELL} $am_aux_dir/install-sh"
  esac
fi
AC_SUBST([install_sh])])

# Copyright (C) 2003-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# Check whether the underlying file-system supports filenames
# with a leading dot.  For instance MS-DOS doesn't.
AC_DEFUN([AM_SET_LEADING_DOT],
[rm -rf .tst 2>/dev/null
mkdir .tst 2>/dev/null
if test -d .tst; then
  am__leading_dot=.
else
  am__leading_dot=_
fi
rmdir .tst 2>/dev/null
AC_SUBST([am__leading_dot])])

# Add --enable-maintainer-mode option to configure.         -*- Autoconf -*-
# From Jim Meyering

# Copyright (C) 1996-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_MAINTAINER_MODE([DEFAULT-MODE])
# ----------------------------------
# Control maintainer-specific portions of Makefiles.
# Default is to disable them, unless 'enable' is passed literally.
# For symmetry, 'disable' may be passed as well.  Anyway, the user
# can override the default with the --enable/--disable switch.
AC_DEFUN([AM_MAINTAINER_MODE],
[m4_case(m4_default([$1], [disable]),
       [enable], [m4_define([am_maintainer_other], [disable])],
       [disable], [m4_define([am_maintainer_other], [enable])],
       [m4_define([am_maintainer_other], [enable])
        m4_warn([syntax], [unexpected argument to AM@&t@_MAINTAINER_MODE: $1])])
AC_MSG_CHECKING([whether to enable maintainer-specific portions of Makefiles])
  dnl maintainer-mode's default is 'disable' unless 'enable' is passed
  AC_ARG_ENABLE([maintainer-mode],
    [AS_HELP_STRING([--]am_maintainer_other[-maintainer-mode],
      am_maintainer_other[ make rules and dependencies not useful
      (and sometimes confusing) to the casual installer])],
    [USE_MAINTAINER_MODE=$enableval],
    [USE_MAINTAINER_MODE=]m4_if(am_maintainer_other, [enable], [no], [yes]))
  AC_MSG_RESULT([$USE_MAINTAINER_MODE])
  AM_CONDITIONAL([MAINTAINER_MODE], [test $USE_MAINTAINER_MODE = yes])
  MAINT=$MAINTAINER_MODE_TRUE
  AC_SUBST([MAINT])dnl
]
)

# Check to see how 'make' treats includes.	            -*- Autoconf -*-

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_MAKE_INCLUDE()
# -----------------
# Check whether make has an 'include' directive that can support all
# the idioms we need for our automatic dependency tracking code.
AC_DEFUN([AM_MAKE_INCLUDE],
[AC_MSG_CHECKING([whether ${MAKE-make} supports the include directive])
cat > confinc.mk << 'END'
am__doit:
	@echo this is the am__doit target >confinc.out
.PHONY: am__doit
END
am__include="#"
am__quote=
# BSD make does it like this.
echo '.include "confinc.mk" # ignored' > confmf.BSD
# Other make implementations (GNU, Solaris 10, AIX) do it like this.
echo 'include confinc.mk # ignored' > confmf.GNU
_am_result=no
for s in GNU BSD; do
  AM_RUN_LOG([${MAKE-make} -f confmf.$s && cat confinc.out])
  AS_CASE([$?:`cat confinc.out 2>/dev/null`],
      ['0:this is the am__doit target'],
      [AS_CASE([$s],
          [BSD], [am__include='.include' am__quote='"'],
          [am__include='include' am__quote=''])])
  if test "$am__include" != "#"; then
    _am_result="yes ($s style)"
    break
  fi
done
rm -f confinc.* confmf.*
AC_MSG_RESULT([${_am_result}])
AC_SUBST([am__include])])
AC_SUBST([am__quote])])

# Fake the existence of programs that GNU maintainers use.  -*- Autoconf -*-

# Copyright (C) 1997-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_MISSING_PROG(NAME, PROGRAM)
# ------------------------------
AC_DEFUN([AM_MISSING_PROG],
[AC_REQUIRE([AM_MISSING_HAS_RUN])
$1=${$1-"${am_missing_run}$2"}
AC_SUBST($1)])

# AM_MISSING_HAS_RUN
# ------------------
# Define MISSING if not defined so far and test if it is modern enough.
# If it is, set am_missing_run to use it, otherwise, to nothing.
AC_DEFUN([AM_MISSING_HAS_RUN],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
AC_REQUIRE_AUX_FILE([missing])dnl
if test x"${MISSING+set}" != xset; then
  MISSING="\${SHELL} '$am_aux_dir/missing'"
fi
# Use eval to expand $SHELL
if eval "$MISSING --is-lightweight"; then
  am_missing_run="$MISSING "
else
  am_missing_run=
  AC_MSG_WARN(['missing' script is too old or missing])
fi
])

# Helper functions for option handling.                     -*- Autoconf -*-

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_MANGLE_OPTION(NAME)
# -----------------------
AC_DEFUN([_AM_MANGLE_OPTION],
[[_AM_OPTION_]m4_bpatsubst($1, [[^a-zA-Z0-9_]], [_])])

# _AM_SET_OPTION(NAME)
# --------------------
# Set option NAME.  Presently that only means defining a flag for this option.
AC_DEFUN([_AM_SET_OPTION],
[m4_define(_AM_MANGLE_OPTION([$1]), [1])])

# _AM_SET_OPTIONS(OPTIONS)
# ------------------------
# OPTIONS is a space-separated list of Automake options.
AC_DEFUN([_AM_SET_OPTIONS],
[m4_foreach_w([_AM_Option], [$1], [_AM_SET_OPTION(_AM_Option)])])

# _AM_IF_OPTION(OPTION, IF-SET, [IF-NOT-SET])
# -------------------------------------------
# Execute IF-SET if OPTION is set, IF-NOT-SET otherwise.
AC_DEFUN([_AM_IF_OPTION],
[m4_ifset(_AM_MANGLE_OPTION([$1]), [$2], [$3])])

# Copyright (C) 1999-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_PROG_CC_C_O
# ---------------
# Like AC_PROG_CC_C_O, but changed for automake.  We rewrite AC_PROG_CC
# to automatically call this.
AC_DEFUN([_AM_PROG_CC_C_O],
[AC_REQUIRE([AM_AUX_DIR_EXPAND])dnl
AC_REQUIRE_AUX_FILE([compile])dnl
AC_LANG_PUSH([C])dnl
AC_CACHE_CHECK(
  [whether $CC understands -c and -o together],
  [am_cv_prog_cc_c_o],
  [AC_LANG_CONFTEST([AC_LANG_PROGRAM([])])
  # Make sure it works both with $CC and with simple cc.
  # Following AC_PROG_CC_C_O, we do the test twice because some
  # compilers refuse to overwrite an existing .o file with -o,
  # though they will create one.
  am_cv_prog_cc_c_o=yes
  for am_i in 1 2; do
    if AM_RUN_LOG([$CC -c conftest.$ac_ext -o conftest2.$ac_objext]) \
         && test -f conftest2.$ac_objext; then
      : OK
    else
      am_cv_prog_cc_c_o=no
      break
    fi
  done
  rm -f core conftest*
  unset am_i])
if test "$am_cv_prog_cc_c_o" != yes; then
   # Losing compiler, so override with the script.
   # FIXME: It is wrong to rewrite CC.
   # But if we don't then we get into trouble of one sort or another.
   # A longer-term fix would be to have automake use am__CC in this case,
   # and then we could set am__CC="\$(top_srcdir)/compile \$(CC)"
   CC="$am_aux_dir/compile $CC"
fi
AC_LANG_POP([C])])

# For backward compatibility.
AC_DEFUN_ONCE([AM_PROG_CC_C_O], [AC_REQUIRE([AC_PROG_CC])])

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_RUN_LOG(COMMAND)
# -------------------
# Run COMMAND, save the exit status in ac_status, and log it.
# (This has been adapted from Autoconf's _AC_RUN_LOG macro.)
AC_DEFUN([AM_RUN_LOG],
[{ echo "$as_me:$LINENO: $1" >&AS_MESSAGE_LOG_FD
   ($1) >&AS_MESSAGE_LOG_FD 2>&AS_MESSAGE_LOG_FD
   ac_status=$?
   echo "$as_me:$LINENO: \$? = $ac_status" >&AS_MESSAGE_LOG_FD
   (exit $ac_status); }])

# Check to make sure that the build environment is sane.    -*- Autoconf -*-

# Copyright (C) 1996-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_SANITY_CHECK
# ---------------
AC_DEFUN([AM_SANITY_CHECK],
[AC_MSG_CHECKING([whether build environment is sane])
# Reject unsafe characters in $srcdir or the absolute working directory
# name.  Accept space and tab only in the latter.
am_lf='
'
case `pwd` in
  *[[\\\"\#\$\&\'\`$am_lf]]*)
    AC_MSG_ERROR([unsafe absolute working directory name]);;
esac
case $srcdir in
  *[[\\\"\#\$\&\'\`$am_lf\ \	]]*)
    AC_MSG_ERROR([unsafe srcdir value: '$srcdir']);;
esac

# Do 'set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   am_has_slept=no
   for am_try in 1 2; do
     echo "timestamp, slept: $am_has_slept" > conftest.file
     set X `ls -Lt "$srcdir/configure" conftest.file 2> /dev/null`
     if test "$[*]" = "X"; then
	# -L didn't work.
	set X `ls -t "$srcdir/configure" conftest.file`
     fi
     if test "$[*]" != "X $srcdir/configure conftest.file" \
	&& test "$[*]" != "X conftest.file $srcdir/configure"; then

	# If neither matched, then we have a broken ls.  This can happen
	# if, for instance, CONFIG_SHELL is bash and it inherits a
	# broken ls alias from the environment.  This has actually
	# happened.  Such a system could not be considered "sane".
	AC_MSG_ERROR([ls -t appears to fail.  Make sure there is not a broken
  alias in your environment])
     fi
     if test "$[2]" = conftest.file || test $am_try -eq 2; then
       break
     fi
     # Just in case.
     sleep 1
     am_has_slept=yes
   done
   test "$[2]" = conftest.file
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
AC_MSG_RESULT([yes])
# If we didn't sleep, we still need to ensure time stamps of config.status and
# generated files are strictly newer.
am_sleep_pid=
if grep 'slept: no' conftest.file >/dev/null 2>&1; then
  ( sleep 1 ) &
  am_sleep_pid=$!
fi
AC_CONFIG_COMMANDS_PRE(
  [AC_MSG_CHECKING([that generated files are newer than configure])
   if test -n "$am_sleep_pid"; then
     # Hide warnings about reused PIDs.
     wait $am_sleep_pid 2>/dev/null
   fi
   AC_MSG_RESULT([done])])
rm -f conftest.file
])

# Copyright (C) 2009-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_SILENT_RULES([DEFAULT])
# --------------------------
# Enable less verbose build rules; with the default set to DEFAULT
# ("yes" being less verbose, "no" or empty being verbose).
AC_DEFUN([AM_SILENT_RULES],
[AC_ARG_ENABLE([silent-rules], [dnl
AS_HELP_STRING(
  [--enable-silent-rules],
  [less verbose build output (undo: "make V=1")])
AS_HELP_STRING(
  [--disable-silent-rules],
  [verbose build output (undo: "make V=0")])dnl
])
case $enable_silent_rules in @%:@ (((
  yes) AM_DEFAULT_VERBOSITY=0;;
   no) AM_DEFAULT_VERBOSITY=1;;
    *) AM_DEFAULT_VERBOSITY=m4_if([$1], [yes], [0], [1]);;
esac
dnl
dnl A few 'make' implementations (e.g., NonStop OS and NextStep)
dnl do not support nested variable expansions.
dnl See automake bug#9928 and bug#10237.
am_make=${MAKE-make}
AC_CACHE_CHECK([whether $am_make supports nested variables],
   [am_cv_make_support_nested_variables],
   [if AS_ECHO([['TRUE=$(BAR$(V))
BAR0=false
BAR1=true
V=1
am__doit:
	@$(TRUE)
.PHONY: am__doit']]) | $am_make -f - >/dev/null 2>&1; then
  am_cv_make_support_nested_variables=yes
else
  am_cv_make_support_nested_variables=no
fi])
if test $am_cv_make_support_nested_variables = yes; then
  dnl Using '$V' instead of '$(V)' breaks IRIX make.
  AM_V='$(V)'
  AM_DEFAULT_V='$(AM_DEFAULT_VERBOSITY)'
else
  AM_V=$AM_DEFAULT_VERBOSITY
  AM_DEFAULT_V=$AM_DEFAULT_VERBOSITY
fi
AC_SUBST([AM_V])dnl
AM_SUBST_NOTMAKE([AM_V])dnl
AC_SUBST([AM_DEFAULT_V])dnl
AM_SUBST_NOTMAKE([AM_DEFAULT_V])dnl
AC_SUBST([AM_DEFAULT_VERBOSITY])dnl
AM_BACKSLASH='\'
AC_SUBST([AM_BACKSLASH])dnl
_AM_SUBST_NOTMAKE([AM_BACKSLASH])dnl
])

# Copyright (C) 2001-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# AM_PROG_INSTALL_STRIP
# ---------------------
# One issue with vendor 'install' (even GNU) is that you can't
# specify the program used to strip binaries.  This is especially
# annoying in cross-compiling environments, where the build's strip
# is unlikely to handle the host's binaries.
# Fortunately install-sh will honor a STRIPPROG variable, so we
# always use install-sh in "make install-strip", and initialize
# STRIPPROG with the value of the STRIP variable (set by the user).
AC_DEFUN([AM_PROG_INSTALL_STRIP],
[AC_REQUIRE([AM_PROG_INSTALL_SH])dnl
# Installed binaries are usually stripped using 'strip' when the user
# run "make install-strip".  However 'strip' might not be the right
# tool to use in cross-compilation environments, therefore Automake
# will honor the 'STRIP' environment variable to overrule this program.
dnl Don't test for $cross_compiling = yes, because it might be 'maybe'.
if test "$cross_compiling" != no; then
  AC_CHECK_TOOL([STRIP], [strip], :)
fi
INSTALL_STRIP_PROGRAM="\$(install_sh) -c -s"
AC_SUBST([INSTALL_STRIP_PROGRAM])])

# Copyright (C) 2006-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_SUBST_NOTMAKE(VARIABLE)
# ---------------------------
# Prevent Automake from outputting VARIABLE = @VARIABLE@ in Makefile.in.
# This macro is traced by Automake.
AC_DEFUN([_AM_SUBST_NOTMAKE])

# AM_SUBST_NOTMAKE(VARIABLE)
# --------------------------
# Public sister of _AM_SUBST_NOTMAKE.
AC_DEFUN([AM_SUBST_NOTMAKE], [_AM_SUBST_NOTMAKE($@)])

# Check how to create a tarball.                            -*- Autoconf -*-

# Copyright (C) 2004-2020 Free Software Foundation, Inc.
#
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# _AM_PROG_TAR(FORMAT)
# --------------------
# Check how to create a tarball in format FORMAT.
# FORMAT should be one of 'v7', 'ustar', or 'pax'.
#
# Substitute a variable $(am__tar) that is a command
# writing to stdout a FORMAT-tarball containing the directory
# $tardir.
#     tardir=directory && $(am__tar) > result.tar
#
# Substitute a variable $(am__untar) that extract such
# a tarball read from stdin.
#     $(am__untar) < result.tar
#
AC_DEFUN([_AM_PROG_TAR],
[# Always define AMTAR for backward compatibility.  Yes, it's still used
# in the wild :-(  We should find a proper way to deprecate it ...
AC_SUBST([AMTAR], ['$${TAR-tar}'])

# We'll loop over all known methods to create a tar archive until one works.
_am_tools='gnutar m4_if([$1], [ustar], [plaintar]) pax cpio none'

m4_if([$1], [v7],
  [am__tar='$${TAR-tar} chof - "$$tardir"' am__untar='$${TAR-tar} xf -'],

  [m4_case([$1],
    [ustar],
     [# The POSIX 1988 'ustar' format is defined with fixed-size fields.
      # There is notably a 21 bits limit for the UID and the GID.  In fact,
      # the 'pax' utility can hang on bigger UID/GID (see automake bug#8343
      # and bug#13588).
      am_max_uid=2097151 # 2^21 - 1
      am_max_gid=$am_max_uid
      # The $UID and $GID variables are not portable, so we need to resort
      # to the POSIX-mandated id(1) utility.  Errors in the 'id' calls
      # below are definitely unexpected, so allow the users to see them
      # (that is, avoid stderr redirection).
      am_uid=`id -u || echo unknown`
      am_gid=`id -g || echo unknown`
      AC_MSG_CHECKING([whether UID '$am_uid' is supported by ustar format])
      if test $am_uid -le $am_max_uid; then
         AC_MSG_RESULT([yes])
      else
         AC_MSG_RESULT([no])
         _am_tools=none
      fi
      AC_MSG_CHECKING([whether GID '$am_gid' is supported by ustar format])
      if test $am_gid -le $am_max_gid; then
         AC_MSG_RESULT([yes])
      else
        AC_MSG_RESULT([no])
        _am_tools=none
      fi],

  [pax],
    [],

  [m4_fatal([Unknown tar format])])

  AC_MSG_CHECKING([how to create a $1 tar archive])

  # Go ahead even if we have the value already cached.  We do so because we
  # need to set the values for the 'am__tar' and 'am__untar' variables.
  _am_tools=${am_cv_prog_tar_$1-$_am_tools}

  for _am_tool in $_am_tools; do
    case $_am_tool in
    gnutar)
      for _am_tar in tar gnutar gtar; do
        AM_RUN_LOG([$_am_tar --version]) && break
      done
      am__tar="$_am_tar --format=m4_if([$1], [pax], [posix], [$1]) -chf - "'"$$tardir"'
      am__tar_="$_am_tar --format=m4_if([$1], [pax], [posix], [$1]) -chf - "'"$tardir"'
      am__untar="$_am_tar -xf -"
      ;;
    plaintar)
      # Must skip GNU tar: if it does not support --format= it doesn't create
      # ustar tarball either.
      (tar --version) >/dev/null 2>&1 && continue
      am__tar='tar chf - "$$tardir"'
      am__tar_='tar chf - "$tardir"'
      am__untar='tar xf -'
      ;;
    pax)
      am__tar='pax -L -x $1 -w "$$tardir"'
      am__tar_='pax -L -x $1 -w "$tardir"'
      am__untar='pax -r'
      ;;
    cpio)
      am__tar='find "$$tardir" -print | cpio -o -H $1 -L'
      am__tar_='find "$tardir" -print | cpio -o -H $1 -L'
      am__untar='cpio -i -H $1 -d'
      ;;
    none)
      am__tar=false
      am__tar_=false
      am__untar=false
      ;;
    esac

    # If the value was cached, stop now.  We just wanted to have am__tar
    # and am__untar set.
    test -n "${am_cv_prog_tar_$1}" && break

    # tar/untar a dummy directory, and stop if the command works.
    rm -rf conftest.dir
    mkdir conftest.dir
    echo GrepMe > conftest.dir/file
    AM_RUN_LOG([tardir=conftest.dir && eval $am__tar_ >conftest.tar])
    rm -rf conftest.dir
    if test -s conftest.tar; then
      AM_RUN_LOG([$am__untar <conftest.tar])
      AM_RUN_LOG([cat conftest.dir/file])
      grep GrepMe conftest.dir/file >/dev/null 2>&1 && break
    fi
  done
  rm -rf conftest.dir

  AC_CACHE_VAL([am_cv_prog_tar_$1], [am_cv_prog_tar_$1=$_am_tool])
  AC_MSG_RESULT([$am_cv_prog_tar_$1])])

AC_SUBST([am__tar])
AC_SUBST([am__untar])
]) # _AM_PROG_TAR

m4_include([m4/asmcfi.m4])
m4_include([m4/ax_append_flag.m4])
m4_include([m4/ax_cc_maxopt.m4])
m4_include([m4/ax_cflags_warn_all.m4])
m4_include([m4/ax_check_compile_flag.m4])
m4_include([m4/ax_compiler_vendor.m4])
m4_include([m4/ax_configure_args.m4])
m4_include([m4/ax_enable_builddir.m4])
m4_include([m4/ax_gcc_archflag.m4])
m4_include([m4/ax_gcc_x86_cpuid.m4])
m4_include([m4/ax_require_defined.m4])
m4_include([m4/libtool.m4])
m4_include([m4/ltoptions.m4])
m4_include([m4/ltsugar.m4])
m4_include([m4/ltversion.m4])
m4_include([m4/lt~obsolete.m4])
m4_include([acinclude.m4])
