## Process this file with autoconf to produce configure.
## In general, the safest way to proceed is to run ./autogen.sh

AC_PREREQ(2.59)

# Note:  If you change the version, you must also update it in:
# * Protobuf.podspec
# * csharp/Google.Protobuf.Tools.nuspec
# * csharp/src/*/AssemblyInfo.cs
# * csharp/src/Google.Protobuf/Google.Protobuf.nuspec
# * java/*/pom.xml
# * python/google/protobuf/__init__.py
# * protoc-artifacts/pom.xml
# * src/google/protobuf/stubs/common.h
# * src/Makefile.am (Update -version-info for LDFLAGS if needed)
#
# In the SVN trunk, the version should always be the next anticipated release
# version with the "-pre" suffix.  (We used to use "-SNAPSHOT" but this pushed
# the size of one file name in the dist tarfile over the 99-char limit.)
AC_INIT([Protocol Buffers],[3.9.0],[protobuf@googlegroups.com],[protobuf])

AM_MAINTAINER_MODE([enable])

AC_CONFIG_SRCDIR(src/google/protobuf/message.cc)
# The config file is generated but not used by the source code, since we only
# need very few of them, e.g. HAVE_PTHREAD and HAVE_ZLIB. Those macros are
# passed down in CXXFLAGS manually in src/Makefile.am
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_VAR(DIST_LANG, [language to include in the distribution package (i.e., make dist)])
case "$DIST_LANG" in
  "") DIST_LANG=all ;;
  all | cpp | csharp | java | python | javanano | objectivec | ruby | js | php) ;;
  *) AC_MSG_FAILURE([unknown language: $DIST_LANG]) ;;
esac
AC_SUBST(DIST_LANG)

# autoconf's default CXXFLAGS are usually "-g -O2".  These aren't necessarily
# the best choice for libprotobuf.
AS_IF([test "x${ac_cv_env_CFLAGS_set}" = "x"],
      [CFLAGS=""])
AS_IF([test "x${ac_cv_env_CXXFLAGS_set}" = "x"],
      [CXXFLAGS=""])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.9 tar-ustar subdir-objects])

# Silent rules enabled: the output is minimal but informative.
# In particular, the warnings from the compiler stick out very clearly.
# To see all logs, use the --disable-silent-rules on configure or via make V=1
AM_SILENT_RULES([yes])

AC_ARG_WITH([zlib],
  [AS_HELP_STRING([--with-zlib],
    [include classes for streaming compressed data in and out @<:@default=check@:>@])],
  [],[with_zlib=check])

AC_ARG_WITH([zlib-include],
  [AS_HELP_STRING([--with-zlib-include=PATH],
    [zlib include directory])],
  [CPPFLAGS="-I$withval $CPPFLAGS"])

AC_ARG_WITH([zlib-lib],
  [AS_HELP_STRING([--with-zlib-lib=PATH],
    [zlib lib directory])],
  [LDFLAGS="-L$withval $LDFLAGS"])

AC_ARG_WITH([protoc],
  [AS_HELP_STRING([--with-protoc=COMMAND],
    [use the given protoc command instead of building a new one when building tests (useful for cross-compiling)])],
  [],[with_protoc=no])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CXX_FOR_BUILD
AC_LANG([C++])
ACX_USE_SYSTEM_EXTENSIONS
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AM_CONDITIONAL(GCC, test "$GCC" = yes)   # let the Makefile know if we're gcc
AC_PROG_OBJC

# test_util.cc takes forever to compile with GCC and optimization turned on.
AC_MSG_CHECKING([C++ compiler flags...])
AS_IF([test "x${ac_cv_env_CXXFLAGS_set}" = "x"],[
  AS_IF([test "$GCC" = "yes"],[
    PROTOBUF_OPT_FLAG="-O2"
    CXXFLAGS="${CXXFLAGS} -g"
  ])

  # Protocol Buffers contains several checks that are intended to be used only
  # for debugging and which might hurt performance.  Most users are probably
  # end users who don't want these checks, so add -DNDEBUG by default.
  CXXFLAGS="$CXXFLAGS -std=c++11 -DNDEBUG"

  AC_MSG_RESULT([use default: $PROTOBUF_OPT_FLAG $CXXFLAGS])
],[
  AC_MSG_RESULT([use user-supplied: $CXXFLAGS])
])

AC_SUBST(PROTOBUF_OPT_FLAG)

ACX_CHECK_SUNCC

# Have to do libtool after SUNCC, other wise it "helpfully" adds Crun Cstd
# to the link
AC_PROG_LIBTOOL

# Check whether the linker supports version scripts
AC_MSG_CHECKING([whether the linker supports version scripts])
save_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -Wl,--version-script=conftest.map"
cat > conftest.map <<EOF
{
  global:
    main;
  local:
    *;
};
EOF
AC_LINK_IFELSE(
  [AC_LANG_SOURCE([int main() { return 0; }])],
  [have_ld_version_script=yes; AC_MSG_RESULT(yes)],
  [have_ld_version_script=no; AC_MSG_RESULT(no)])
LDFLAGS=$save_LDFLAGS
AM_CONDITIONAL([HAVE_LD_VERSION_SCRIPT], [test "$have_ld_version_script" == "yes"])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h stdlib.h unistd.h])

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_STRTOD
AC_CHECK_FUNCS([ftruncate memset mkdir strchr strerror strtol])

# Check for zlib.
HAVE_ZLIB=0
AS_IF([test "$with_zlib" != no], [
  AC_MSG_CHECKING([zlib version])

  # First check the zlib header version.
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([[
        #include <zlib.h>
        #if !defined(ZLIB_VERNUM) || (ZLIB_VERNUM < 0x1204)
        # error zlib version too old
        #endif
        ]], [])], [
    AC_MSG_RESULT([ok (1.2.0.4 or later)])

    # Also need to add -lz to the linker flags and make sure this succeeds.
    AC_SEARCH_LIBS([zlibVersion], [z], [
      AC_DEFINE([HAVE_ZLIB], [1], [Enable classes using zlib compression.])
      HAVE_ZLIB=1
    ], [
      AS_IF([test "$with_zlib" != check], [
        AC_MSG_FAILURE([--with-zlib was given, but no working zlib library was found])
      ])
    ])
  ], [
    AS_IF([test "$with_zlib" = check], [
      AC_MSG_RESULT([headers missing or too old (requires 1.2.0.4)])
    ], [
      AC_MSG_FAILURE([--with-zlib was given, but zlib headers were not present or were too old (requires 1.2.0.4)])
    ])
  ])
])
AM_CONDITIONAL([HAVE_ZLIB], [test $HAVE_ZLIB = 1])

# Add -std=c++11 if necesssary. It is important for us to do this before the
# libatomic check below, since that also depends on C++11.
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])

dnl On some platforms, std::atomic needs a helper library
AC_MSG_CHECKING(whether -latomic is needed)
AC_LINK_IFELSE([AC_LANG_SOURCE([[
  #include <atomic>
  #include <cstdint>
  std::atomic<std::int64_t> v;
  int main() {
    return v;
  }
]])], STD_ATOMIC_NEED_LIBATOMIC=no, STD_ATOMIC_NEED_LIBATOMIC=yes)
AC_MSG_RESULT($STD_ATOMIC_NEED_LIBATOMIC)
if test "x$STD_ATOMIC_NEED_LIBATOMIC" = xyes; then
  LIBATOMIC_LIBS="-latomic"
fi
AC_SUBST([LIBATOMIC_LIBS])

AS_IF([test "$with_protoc" != "no"], [
  PROTOC=$with_protoc
  AS_IF([test "$with_protoc" = "yes"], [
    # No argument given.  Use system protoc.
    PROTOC=protoc
  ])
  AS_IF([echo "$PROTOC" | grep -q '^@<:@^/@:>@.*/'], [
    # Does not start with a slash, but contains a slash.  So, it's a relative
    # path (as opposed to an absolute path or an executable in $PATH).
    # Since it will actually be executed from the src directory, prefix with
    # the current directory.  We also insert $ac_top_build_prefix in case this
    # is a nested package and --with-protoc was actually given on the outer
    # package's configure script.
    PROTOC=`pwd`/${ac_top_build_prefix}$PROTOC
  ])
  AC_SUBST([PROTOC])
])
AM_CONDITIONAL([USE_EXTERNAL_PROTOC], [test "$with_protoc" != "no"])

AX_PTHREAD
AM_CONDITIONAL([HAVE_PTHREAD], [test "x$ax_pthread_ok" = "xyes"])
# We still keep this for improving pbconfig.h for unsupported platforms.
AC_CXX_STL_HASH

# Enable ObjC support for conformance directory on OS X.
OBJC_CONFORMANCE_TEST=0
case "$target_os" in
  darwin*)
    OBJC_CONFORMANCE_TEST=1
    ;;
esac
AM_CONDITIONAL([OBJC_CONFORMANCE_TEST], [test $OBJC_CONFORMANCE_TEST = 1])

# HACK:  Make gmock's configure script pick up our copy of CFLAGS and CXXFLAGS,
#   since the flags added by ACX_CHECK_SUNCC must be used when compiling gmock
#   too.
export CFLAGS
export CXXFLAGS
AC_CONFIG_SUBDIRS([third_party/googletest])

AC_CONFIG_FILES([Makefile src/Makefile benchmarks/Makefile conformance/Makefile protobuf.pc protobuf-lite.pc])
AC_OUTPUT
