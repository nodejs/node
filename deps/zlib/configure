#!/bin/sh
# configure script for zlib.
#
# Normally configure builds both a static and a shared library.
# If you want to build just a static library, use: ./configure --static
#
# To impose specific compiler or flags or install directory, use for example:
#    prefix=$HOME CC=cc CFLAGS="-O4" ./configure
# or for csh/tcsh users:
#    (setenv prefix $HOME; setenv CC cc; setenv CFLAGS "-O4"; ./configure)

# Incorrect settings of CC or CFLAGS may prevent creating a shared library.
# If you have problems, try without defining CC and CFLAGS before reporting
# an error.

# start off configure.log
echo -------------------- >> configure.log
echo $0 $* >> configure.log
date >> configure.log

# set command prefix for cross-compilation
if [ -n "${CHOST}" ]; then
    uname="`echo "${CHOST}" | sed -e 's/^[^-]*-\([^-]*\)$/\1/' -e 's/^[^-]*-[^-]*-\([^-]*\)$/\1/' -e 's/^[^-]*-[^-]*-\([^-]*\)-.*$/\1/'`"
    CROSS_PREFIX="${CHOST}-"
fi

# destination name for static library
STATICLIB=libz.a

# extract zlib version numbers from zlib.h
VER=`sed -n -e '/VERSION "/s/.*"\(.*\)".*/\1/p' < zlib.h`
VER3=`sed -n -e '/VERSION "/s/.*"\([0-9]*\\.[0-9]*\\.[0-9]*\).*/\1/p' < zlib.h`
VER2=`sed -n -e '/VERSION "/s/.*"\([0-9]*\\.[0-9]*\)\\..*/\1/p' < zlib.h`
VER1=`sed -n -e '/VERSION "/s/.*"\([0-9]*\)\\..*/\1/p' < zlib.h`

# establish commands for library building
if "${CROSS_PREFIX}ar" --version >/dev/null 2>/dev/null || test $? -lt 126; then
    AR=${AR-"${CROSS_PREFIX}ar"}
    test -n "${CROSS_PREFIX}" && echo Using ${AR} | tee -a configure.log
else
    AR=${AR-"ar"}
    test -n "${CROSS_PREFIX}" && echo Using ${AR} | tee -a configure.log
fi
ARFLAGS=${ARFLAGS-"rc"}
if "${CROSS_PREFIX}ranlib" --version >/dev/null 2>/dev/null || test $? -lt 126; then
    RANLIB=${RANLIB-"${CROSS_PREFIX}ranlib"}
    test -n "${CROSS_PREFIX}" && echo Using ${RANLIB} | tee -a configure.log
else
    RANLIB=${RANLIB-"ranlib"}
fi
if "${CROSS_PREFIX}nm" --version >/dev/null 2>/dev/null || test $? -lt 126; then
    NM=${NM-"${CROSS_PREFIX}nm"}
    test -n "${CROSS_PREFIX}" && echo Using ${NM} | tee -a configure.log
else
    NM=${NM-"nm"}
fi

# set defaults before processing command line options
LDCONFIG=${LDCONFIG-"ldconfig"}
LDSHAREDLIBC="${LDSHAREDLIBC--lc}"
ARCHS=
prefix=${prefix-/usr/local}
exec_prefix=${exec_prefix-'${prefix}'}
libdir=${libdir-'${exec_prefix}/lib'}
sharedlibdir=${sharedlibdir-'${libdir}'}
includedir=${includedir-'${prefix}/include'}
mandir=${mandir-'${prefix}/share/man'}
shared_ext='.so'
shared=1
solo=0
cover=0
zprefix=0
zconst=0
build64=0
gcc=0
old_cc="$CC"
old_cflags="$CFLAGS"
OBJC='$(OBJZ) $(OBJG)'
PIC_OBJC='$(PIC_OBJZ) $(PIC_OBJG)'

# leave this script, optionally in a bad way
leave()
{
  if test "$*" != "0"; then
    echo "** $0 aborting." | tee -a configure.log
  fi
  rm -f $test.[co] $test $test$shared_ext $test.gcno ./--version
  echo -------------------- >> configure.log
  echo >> configure.log
  echo >> configure.log
  exit $1
}

# process command line options
while test $# -ge 1
do
case "$1" in
    -h* | --help)
      echo 'usage:' | tee -a configure.log
      echo '  configure [--const] [--zprefix] [--prefix=PREFIX]  [--eprefix=EXPREFIX]' | tee -a configure.log
      echo '    [--static] [--64] [--libdir=LIBDIR] [--sharedlibdir=LIBDIR]' | tee -a configure.log
      echo '    [--includedir=INCLUDEDIR] [--archs="-arch i386 -arch x86_64"]' | tee -a configure.log
        exit 0 ;;
    -p*=* | --prefix=*) prefix=`echo $1 | sed 's/.*=//'`; shift ;;
    -e*=* | --eprefix=*) exec_prefix=`echo $1 | sed 's/.*=//'`; shift ;;
    -l*=* | --libdir=*) libdir=`echo $1 | sed 's/.*=//'`; shift ;;
    --sharedlibdir=*) sharedlibdir=`echo $1 | sed 's/.*=//'`; shift ;;
    -i*=* | --includedir=*) includedir=`echo $1 | sed 's/.*=//'`;shift ;;
    -u*=* | --uname=*) uname=`echo $1 | sed 's/.*=//'`;shift ;;
    -p* | --prefix) prefix="$2"; shift; shift ;;
    -e* | --eprefix) exec_prefix="$2"; shift; shift ;;
    -l* | --libdir) libdir="$2"; shift; shift ;;
    -i* | --includedir) includedir="$2"; shift; shift ;;
    -s* | --shared | --enable-shared) shared=1; shift ;;
    -t | --static) shared=0; shift ;;
    --solo) solo=1; shift ;;
    --cover) cover=1; shift ;;
    -z* | --zprefix) zprefix=1; shift ;;
    -6* | --64) build64=1; shift ;;
    -a*=* | --archs=*) ARCHS=`echo $1 | sed 's/.*=//'`; shift ;;
    --sysconfdir=*) echo "ignored option: --sysconfdir" | tee -a configure.log; shift ;;
    --localstatedir=*) echo "ignored option: --localstatedir" | tee -a configure.log; shift ;;
    -c* | --const) zconst=1; shift ;;
    *)
      echo "unknown option: $1" | tee -a configure.log
      echo "$0 --help for help" | tee -a configure.log
      leave 1;;
    esac
done

# temporary file name
test=ztest$$

# put arguments in log, also put test file in log if used in arguments
show()
{
  case "$*" in
    *$test.c*)
      echo === $test.c === >> configure.log
      cat $test.c >> configure.log
      echo === >> configure.log;;
  esac
  echo $* >> configure.log
}

# check for gcc vs. cc and set compile and link flags based on the system identified by uname
cat > $test.c <<EOF
extern int getchar();
int hello() {return getchar();}
EOF

test -z "$CC" && echo Checking for ${CROSS_PREFIX}gcc... | tee -a configure.log
cc=${CC-${CROSS_PREFIX}gcc}
cflags=${CFLAGS-"-O3"}
# to force the asm version use: CFLAGS="-O3 -DASMV" ./configure
case "$cc" in
  *gcc*) gcc=1 ;;
  *clang*) gcc=1 ;;
esac
case `$cc -v 2>&1` in
  *gcc*) gcc=1 ;;
esac

show $cc -c $test.c
if test "$gcc" -eq 1 && ($cc -c $test.c) >> configure.log 2>&1; then
  echo ... using gcc >> configure.log
  CC="$cc"
  CFLAGS="${CFLAGS--O3} ${ARCHS}"
  SFLAGS="${CFLAGS--O3} -fPIC"
  LDFLAGS="${LDFLAGS} ${ARCHS}"
  if test $build64 -eq 1; then
    CFLAGS="${CFLAGS} -m64"
    SFLAGS="${SFLAGS} -m64"
  fi
  if test "${ZLIBGCCWARN}" = "YES"; then
    if test "$zconst" -eq 1; then
      CFLAGS="${CFLAGS} -Wall -Wextra -Wcast-qual -pedantic -DZLIB_CONST"
    else
      CFLAGS="${CFLAGS} -Wall -Wextra -pedantic"
    fi
  fi
  if test -z "$uname"; then
    uname=`(uname -s || echo unknown) 2>/dev/null`
  fi
  case "$uname" in
  Linux* | linux* | GNU | GNU/* | solaris*)
        LDSHARED=${LDSHARED-"$cc -shared -Wl,-soname,libz.so.1,--version-script,zlib.map"} ;;
  *BSD | *bsd* | DragonFly)
        LDSHARED=${LDSHARED-"$cc -shared -Wl,-soname,libz.so.1,--version-script,zlib.map"}
        LDCONFIG="ldconfig -m" ;;
  CYGWIN* | Cygwin* | cygwin* | OS/2*)
        EXE='.exe' ;;
  MINGW* | mingw*)
# temporary bypass
        rm -f $test.[co] $test $test$shared_ext
        echo "Please use win32/Makefile.gcc instead." | tee -a configure.log
        leave 1
        LDSHARED=${LDSHARED-"$cc -shared"}
        LDSHAREDLIBC=""
        EXE='.exe' ;;
  QNX*)  # This is for QNX6. I suppose that the QNX rule below is for QNX2,QNX4
         # (alain.bonnefoy@icbt.com)
                 LDSHARED=${LDSHARED-"$cc -shared -Wl,-hlibz.so.1"} ;;
  HP-UX*)
         LDSHARED=${LDSHARED-"$cc -shared $SFLAGS"}
         case `(uname -m || echo unknown) 2>/dev/null` in
         ia64)
                 shared_ext='.so'
                 SHAREDLIB='libz.so' ;;
         *)
                 shared_ext='.sl'
                 SHAREDLIB='libz.sl' ;;
         esac ;;
  Darwin* | darwin*)
             shared_ext='.dylib'
             SHAREDLIB=libz$shared_ext
             SHAREDLIBV=libz.$VER$shared_ext
             SHAREDLIBM=libz.$VER1$shared_ext
             LDSHARED=${LDSHARED-"$cc -dynamiclib -install_name $libdir/$SHAREDLIBM -compatibility_version $VER1 -current_version $VER3"}
             if libtool -V 2>&1 | grep Apple > /dev/null; then
                 AR="libtool"
             else
                 AR="/usr/bin/libtool"
             fi
             ARFLAGS="-o" ;;
  *)             LDSHARED=${LDSHARED-"$cc -shared"} ;;
  esac
else
  # find system name and corresponding cc options
  CC=${CC-cc}
  gcc=0
  echo ... using $CC >> configure.log
  if test -z "$uname"; then
    uname=`(uname -sr || echo unknown) 2>/dev/null`
  fi
  case "$uname" in
  HP-UX*)    SFLAGS=${CFLAGS-"-O +z"}
             CFLAGS=${CFLAGS-"-O"}
#            LDSHARED=${LDSHARED-"ld -b +vnocompatwarnings"}
             LDSHARED=${LDSHARED-"ld -b"}
         case `(uname -m || echo unknown) 2>/dev/null` in
         ia64)
             shared_ext='.so'
             SHAREDLIB='libz.so' ;;
         *)
             shared_ext='.sl'
             SHAREDLIB='libz.sl' ;;
         esac ;;
  IRIX*)     SFLAGS=${CFLAGS-"-ansi -O2 -rpath ."}
             CFLAGS=${CFLAGS-"-ansi -O2"}
             LDSHARED=${LDSHARED-"cc -shared -Wl,-soname,libz.so.1"} ;;
  OSF1\ V4*) SFLAGS=${CFLAGS-"-O -std1"}
             CFLAGS=${CFLAGS-"-O -std1"}
             LDFLAGS="${LDFLAGS} -Wl,-rpath,."
             LDSHARED=${LDSHARED-"cc -shared  -Wl,-soname,libz.so -Wl,-msym -Wl,-rpath,$(libdir) -Wl,-set_version,${VER}:1.0"} ;;
  OSF1*)     SFLAGS=${CFLAGS-"-O -std1"}
             CFLAGS=${CFLAGS-"-O -std1"}
             LDSHARED=${LDSHARED-"cc -shared -Wl,-soname,libz.so.1"} ;;
  QNX*)      SFLAGS=${CFLAGS-"-4 -O"}
             CFLAGS=${CFLAGS-"-4 -O"}
             LDSHARED=${LDSHARED-"cc"}
             RANLIB=${RANLIB-"true"}
             AR="cc"
             ARFLAGS="-A" ;;
  SCO_SV\ 3.2*) SFLAGS=${CFLAGS-"-O3 -dy -KPIC "}
             CFLAGS=${CFLAGS-"-O3"}
             LDSHARED=${LDSHARED-"cc -dy -KPIC -G"} ;;
  SunOS\ 5* | solaris*)
         LDSHARED=${LDSHARED-"cc -G -h libz$shared_ext.$VER1"}
         SFLAGS=${CFLAGS-"-fast -KPIC"}
         CFLAGS=${CFLAGS-"-fast"}
         if test $build64 -eq 1; then
             # old versions of SunPRO/Workshop/Studio don't support -m64,
             # but newer ones do.  Check for it.
             flag64=`$CC -flags | egrep -- '^-m64'`
             if test x"$flag64" != x"" ; then
                 CFLAGS="${CFLAGS} -m64"
                 SFLAGS="${SFLAGS} -m64"
             else
                 case `(uname -m || echo unknown) 2>/dev/null` in
                   i86*)
                     SFLAGS="$SFLAGS -xarch=amd64"
                     CFLAGS="$CFLAGS -xarch=amd64" ;;
                   *)
                     SFLAGS="$SFLAGS -xarch=v9"
                     CFLAGS="$CFLAGS -xarch=v9" ;;
                 esac
             fi
         fi
         ;;
  SunOS\ 4*) SFLAGS=${CFLAGS-"-O2 -PIC"}
             CFLAGS=${CFLAGS-"-O2"}
             LDSHARED=${LDSHARED-"ld"} ;;
  SunStudio\ 9*) SFLAGS=${CFLAGS-"-fast -xcode=pic32 -xtarget=ultra3 -xarch=v9b"}
             CFLAGS=${CFLAGS-"-fast -xtarget=ultra3 -xarch=v9b"}
             LDSHARED=${LDSHARED-"cc -xarch=v9b"} ;;
  UNIX_System_V\ 4.2.0)
             SFLAGS=${CFLAGS-"-KPIC -O"}
             CFLAGS=${CFLAGS-"-O"}
             LDSHARED=${LDSHARED-"cc -G"} ;;
  UNIX_SV\ 4.2MP)
             SFLAGS=${CFLAGS-"-Kconform_pic -O"}
             CFLAGS=${CFLAGS-"-O"}
             LDSHARED=${LDSHARED-"cc -G"} ;;
  OpenUNIX\ 5)
             SFLAGS=${CFLAGS-"-KPIC -O"}
             CFLAGS=${CFLAGS-"-O"}
             LDSHARED=${LDSHARED-"cc -G"} ;;
  AIX*)  # Courtesy of dbakker@arrayasolutions.com
             SFLAGS=${CFLAGS-"-O -qmaxmem=8192"}
             CFLAGS=${CFLAGS-"-O -qmaxmem=8192"}
             LDSHARED=${LDSHARED-"xlc -G"} ;;
  # send working options for other systems to zlib@gzip.org
  *)         SFLAGS=${CFLAGS-"-O"}
             CFLAGS=${CFLAGS-"-O"}
             LDSHARED=${LDSHARED-"cc -shared"} ;;
  esac
fi

# destination names for shared library if not defined above
SHAREDLIB=${SHAREDLIB-"libz$shared_ext"}
SHAREDLIBV=${SHAREDLIBV-"libz$shared_ext.$VER"}
SHAREDLIBM=${SHAREDLIBM-"libz$shared_ext.$VER1"}

echo >> configure.log

# define functions for testing compiler and library characteristics and logging the results

cat > $test.c <<EOF
#error error
EOF
if ($CC -c $CFLAGS $test.c) 2>/dev/null; then
  try()
  {
    show $*
    test "`( $* ) 2>&1 | tee -a configure.log`" = ""
  }
  echo - using any output from compiler to indicate an error >> configure.log
else
try()
{
  show $*
  ( $* ) >> configure.log 2>&1
  ret=$?
  if test $ret -ne 0; then
    echo "(exit code "$ret")" >> configure.log
  fi
  return $ret
}
fi

tryboth()
{
  show $*
  got=`( $* ) 2>&1`
  ret=$?
  printf %s "$got" >> configure.log
  if test $ret -ne 0; then
    return $ret
  fi
  test "$got" = ""
}

cat > $test.c << EOF
int foo() { return 0; }
EOF
echo "Checking for obsessive-compulsive compiler options..." >> configure.log
if try $CC -c $CFLAGS $test.c; then
  :
else
  echo "Compiler error reporting is too harsh for $0 (perhaps remove -Werror)." | tee -a configure.log
  leave 1
fi

echo >> configure.log

# see if shared library build supported
cat > $test.c <<EOF
extern int getchar();
int hello() {return getchar();}
EOF
if test $shared -eq 1; then
  echo Checking for shared library support... | tee -a configure.log
  # we must test in two steps (cc then ld), required at least on SunOS 4.x
  if try $CC -w -c $SFLAGS $test.c &&
     try $LDSHARED $SFLAGS -o $test$shared_ext $test.o; then
    echo Building shared library $SHAREDLIBV with $CC. | tee -a configure.log
  elif test -z "$old_cc" -a -z "$old_cflags"; then
    echo No shared library support. | tee -a configure.log
    shared=0;
  else
    echo 'No shared library support; try without defining CC and CFLAGS' | tee -a configure.log
    shared=0;
  fi
fi
if test $shared -eq 0; then
  LDSHARED="$CC"
  ALL="static"
  TEST="all teststatic"
  SHAREDLIB=""
  SHAREDLIBV=""
  SHAREDLIBM=""
  echo Building static library $STATICLIB version $VER with $CC. | tee -a configure.log
else
  ALL="static shared"
  TEST="all teststatic testshared"
fi

# check for underscores in external names for use by assembler code
CPP=${CPP-"$CC -E"}
case $CFLAGS in
  *ASMV*)
    echo >> configure.log
    show "$NM $test.o | grep _hello"
    if test "`$NM $test.o | grep _hello | tee -a configure.log`" = ""; then
      CPP="$CPP -DNO_UNDERLINE"
      echo Checking for underline in external names... No. | tee -a configure.log
    else
      echo Checking for underline in external names... Yes. | tee -a configure.log
    fi ;;
esac

echo >> configure.log

# check for large file support, and if none, check for fseeko()
cat > $test.c <<EOF
#include <sys/types.h>
off64_t dummy = 0;
EOF
if try $CC -c $CFLAGS -D_LARGEFILE64_SOURCE=1 $test.c; then
  CFLAGS="${CFLAGS} -D_LARGEFILE64_SOURCE=1"
  SFLAGS="${SFLAGS} -D_LARGEFILE64_SOURCE=1"
  ALL="${ALL} all64"
  TEST="${TEST} test64"
  echo "Checking for off64_t... Yes." | tee -a configure.log
  echo "Checking for fseeko... Yes." | tee -a configure.log
else
  echo "Checking for off64_t... No." | tee -a configure.log
  echo >> configure.log
  cat > $test.c <<EOF
#include <stdio.h>
int main(void) {
  fseeko(NULL, 0, 0);
  return 0;
}
EOF
  if try $CC $CFLAGS -o $test $test.c; then
    echo "Checking for fseeko... Yes." | tee -a configure.log
  else
    CFLAGS="${CFLAGS} -DNO_FSEEKO"
    SFLAGS="${SFLAGS} -DNO_FSEEKO"
    echo "Checking for fseeko... No." | tee -a configure.log
  fi
fi

echo >> configure.log

# check for strerror() for use by gz* functions
cat > $test.c <<EOF
#include <string.h>
#include <errno.h>
int main() { return strlen(strerror(errno)); }
EOF
if try $CC $CFLAGS -o $test $test.c; then
  echo "Checking for strerror... Yes." | tee -a configure.log
else
  CFLAGS="${CFLAGS} -DNO_STRERROR"
  SFLAGS="${SFLAGS} -DNO_STRERROR"
  echo "Checking for strerror... No." | tee -a configure.log
fi

# copy clean zconf.h for subsequent edits
cp -p zconf.h.in zconf.h

echo >> configure.log

# check for unistd.h and save result in zconf.h
cat > $test.c <<EOF
#include <unistd.h>
int main() { return 0; }
EOF
if try $CC -c $CFLAGS $test.c; then
  sed < zconf.h "/^#ifdef HAVE_UNISTD_H.* may be/s/def HAVE_UNISTD_H\(.*\) may be/ 1\1 was/" > zconf.temp.h
  mv zconf.temp.h zconf.h
  echo "Checking for unistd.h... Yes." | tee -a configure.log
else
  echo "Checking for unistd.h... No." | tee -a configure.log
fi

echo >> configure.log

# check for stdarg.h and save result in zconf.h
cat > $test.c <<EOF
#include <stdarg.h>
int main() { return 0; }
EOF
if try $CC -c $CFLAGS $test.c; then
  sed < zconf.h "/^#ifdef HAVE_STDARG_H.* may be/s/def HAVE_STDARG_H\(.*\) may be/ 1\1 was/" > zconf.temp.h
  mv zconf.temp.h zconf.h
  echo "Checking for stdarg.h... Yes." | tee -a configure.log
else
  echo "Checking for stdarg.h... No." | tee -a configure.log
fi

# if the z_ prefix was requested, save that in zconf.h
if test $zprefix -eq 1; then
  sed < zconf.h "/#ifdef Z_PREFIX.* may be/s/def Z_PREFIX\(.*\) may be/ 1\1 was/" > zconf.temp.h
  mv zconf.temp.h zconf.h
  echo >> configure.log
  echo "Using z_ prefix on all symbols." | tee -a configure.log
fi

# if --solo compilation was requested, save that in zconf.h and remove gz stuff from object lists
if test $solo -eq 1; then
  sed '/#define ZCONF_H/a\
#define Z_SOLO

' < zconf.h > zconf.temp.h
  mv zconf.temp.h zconf.h
OBJC='$(OBJZ)'
PIC_OBJC='$(PIC_OBJZ)'
fi

# if code coverage testing was requested, use older gcc if defined, e.g. "gcc-4.2" on Mac OS X
if test $cover -eq 1; then
  CFLAGS="${CFLAGS} -fprofile-arcs -ftest-coverage"
  if test -n "$GCC_CLASSIC"; then
    CC=$GCC_CLASSIC
  fi
fi

echo >> configure.log

# conduct a series of tests to resolve eight possible cases of using "vs" or "s" printf functions
# (using stdarg or not), with or without "n" (proving size of buffer), and with or without a
# return value.  The most secure result is vsnprintf() with a return value.  snprintf() with a
# return value is secure as well, but then gzprintf() will be limited to 20 arguments.
cat > $test.c <<EOF
#include <stdio.h>
#include <stdarg.h>
#include "zconf.h"
int main()
{
#ifndef STDC
  choke me
#endif
  return 0;
}
EOF
if try $CC -c $CFLAGS $test.c; then
  echo "Checking whether to use vs[n]printf() or s[n]printf()... using vs[n]printf()." | tee -a configure.log

  echo >> configure.log
  cat > $test.c <<EOF
#include <stdio.h>
#include <stdarg.h>
int mytest(const char *fmt, ...)
{
  char buf[20];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return 0;
}
int main()
{
  return (mytest("Hello%d\n", 1));
}
EOF
  if try $CC $CFLAGS -o $test $test.c; then
    echo "Checking for vsnprintf() in stdio.h... Yes." | tee -a configure.log

    echo >> configure.log
    cat >$test.c <<EOF
#include <stdio.h>
#include <stdarg.h>
int mytest(const char *fmt, ...)
{
  int n;
  char buf[20];
  va_list ap;
  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return n;
}
int main()
{
  return (mytest("Hello%d\n", 1));
}
EOF

    if try $CC -c $CFLAGS $test.c; then
      echo "Checking for return value of vsnprintf()... Yes." | tee -a configure.log
    else
      CFLAGS="$CFLAGS -DHAS_vsnprintf_void"
      SFLAGS="$SFLAGS -DHAS_vsnprintf_void"
      echo "Checking for return value of vsnprintf()... No." | tee -a configure.log
      echo "  WARNING: apparently vsnprintf() does not return a value. zlib" | tee -a configure.log
      echo "  can build but will be open to possible string-format security" | tee -a configure.log
      echo "  vulnerabilities." | tee -a configure.log
    fi
  else
    CFLAGS="$CFLAGS -DNO_vsnprintf"
    SFLAGS="$SFLAGS -DNO_vsnprintf"
    echo "Checking for vsnprintf() in stdio.h... No." | tee -a configure.log
    echo "  WARNING: vsnprintf() not found, falling back to vsprintf(). zlib" | tee -a configure.log
    echo "  can build but will be open to possible buffer-overflow security" | tee -a configure.log
    echo "  vulnerabilities." | tee -a configure.log

    echo >> configure.log
    cat >$test.c <<EOF
#include <stdio.h>
#include <stdarg.h>
int mytest(const char *fmt, ...)
{
  int n;
  char buf[20];
  va_list ap;
  va_start(ap, fmt);
  n = vsprintf(buf, fmt, ap);
  va_end(ap);
  return n;
}
int main()
{
  return (mytest("Hello%d\n", 1));
}
EOF

    if try $CC -c $CFLAGS $test.c; then
      echo "Checking for return value of vsprintf()... Yes." | tee -a configure.log
    else
      CFLAGS="$CFLAGS -DHAS_vsprintf_void"
      SFLAGS="$SFLAGS -DHAS_vsprintf_void"
      echo "Checking for return value of vsprintf()... No." | tee -a configure.log
      echo "  WARNING: apparently vsprintf() does not return a value. zlib" | tee -a configure.log
      echo "  can build but will be open to possible string-format security" | tee -a configure.log
      echo "  vulnerabilities." | tee -a configure.log
    fi
  fi
else
  echo "Checking whether to use vs[n]printf() or s[n]printf()... using s[n]printf()." | tee -a configure.log

  echo >> configure.log
  cat >$test.c <<EOF
#include <stdio.h>
int mytest()
{
  char buf[20];
  snprintf(buf, sizeof(buf), "%s", "foo");
  return 0;
}
int main()
{
  return (mytest());
}
EOF

  if try $CC $CFLAGS -o $test $test.c; then
    echo "Checking for snprintf() in stdio.h... Yes." | tee -a configure.log

    echo >> configure.log
    cat >$test.c <<EOF
#include <stdio.h>
int mytest()
{
  char buf[20];
  return snprintf(buf, sizeof(buf), "%s", "foo");
}
int main()
{
  return (mytest());
}
EOF

    if try $CC -c $CFLAGS $test.c; then
      echo "Checking for return value of snprintf()... Yes." | tee -a configure.log
    else
      CFLAGS="$CFLAGS -DHAS_snprintf_void"
      SFLAGS="$SFLAGS -DHAS_snprintf_void"
      echo "Checking for return value of snprintf()... No." | tee -a configure.log
      echo "  WARNING: apparently snprintf() does not return a value. zlib" | tee -a configure.log
      echo "  can build but will be open to possible string-format security" | tee -a configure.log
      echo "  vulnerabilities." | tee -a configure.log
    fi
  else
    CFLAGS="$CFLAGS -DNO_snprintf"
    SFLAGS="$SFLAGS -DNO_snprintf"
    echo "Checking for snprintf() in stdio.h... No." | tee -a configure.log
    echo "  WARNING: snprintf() not found, falling back to sprintf(). zlib" | tee -a configure.log
    echo "  can build but will be open to possible buffer-overflow security" | tee -a configure.log
    echo "  vulnerabilities." | tee -a configure.log

    echo >> configure.log
    cat >$test.c <<EOF
#include <stdio.h>
int mytest()
{
  char buf[20];
  return sprintf(buf, "%s", "foo");
}
int main()
{
  return (mytest());
}
EOF

    if try $CC -c $CFLAGS $test.c; then
      echo "Checking for return value of sprintf()... Yes." | tee -a configure.log
    else
      CFLAGS="$CFLAGS -DHAS_sprintf_void"
      SFLAGS="$SFLAGS -DHAS_sprintf_void"
      echo "Checking for return value of sprintf()... No." | tee -a configure.log
      echo "  WARNING: apparently sprintf() does not return a value. zlib" | tee -a configure.log
      echo "  can build but will be open to possible string-format security" | tee -a configure.log
      echo "  vulnerabilities." | tee -a configure.log
    fi
  fi
fi

# see if we can hide zlib internal symbols that are linked between separate source files
if test "$gcc" -eq 1; then
  echo >> configure.log
  cat > $test.c <<EOF
#define ZLIB_INTERNAL __attribute__((visibility ("hidden")))
int ZLIB_INTERNAL foo;
int main()
{
  return 0;
}
EOF
  if tryboth $CC -c $CFLAGS $test.c; then
    CFLAGS="$CFLAGS -DHAVE_HIDDEN"
    SFLAGS="$SFLAGS -DHAVE_HIDDEN"
    echo "Checking for attribute(visibility) support... Yes." | tee -a configure.log
  else
    echo "Checking for attribute(visibility) support... No." | tee -a configure.log
  fi
fi

# show the results in the log
echo >> configure.log
echo ALL = $ALL >> configure.log
echo AR = $AR >> configure.log
echo ARFLAGS = $ARFLAGS >> configure.log
echo CC = $CC >> configure.log
echo CFLAGS = $CFLAGS >> configure.log
echo CPP = $CPP >> configure.log
echo EXE = $EXE >> configure.log
echo LDCONFIG = $LDCONFIG >> configure.log
echo LDFLAGS = $LDFLAGS >> configure.log
echo LDSHARED = $LDSHARED >> configure.log
echo LDSHAREDLIBC = $LDSHAREDLIBC >> configure.log
echo OBJC = $OBJC >> configure.log
echo PIC_OBJC = $PIC_OBJC >> configure.log
echo RANLIB = $RANLIB >> configure.log
echo SFLAGS = $SFLAGS >> configure.log
echo SHAREDLIB = $SHAREDLIB >> configure.log
echo SHAREDLIBM = $SHAREDLIBM >> configure.log
echo SHAREDLIBV = $SHAREDLIBV >> configure.log
echo STATICLIB = $STATICLIB >> configure.log
echo TEST = $TEST >> configure.log
echo VER = $VER >> configure.log
echo Z_U4 = $Z_U4 >> configure.log
echo exec_prefix = $exec_prefix >> configure.log
echo includedir = $includedir >> configure.log
echo libdir = $libdir >> configure.log
echo mandir = $mandir >> configure.log
echo prefix = $prefix >> configure.log
echo sharedlibdir = $sharedlibdir >> configure.log
echo uname = $uname >> configure.log

# udpate Makefile with the configure results
sed < Makefile.in "
/^CC *=/s#=.*#=$CC#
/^CFLAGS *=/s#=.*#=$CFLAGS#
/^SFLAGS *=/s#=.*#=$SFLAGS#
/^LDFLAGS *=/s#=.*#=$LDFLAGS#
/^LDSHARED *=/s#=.*#=$LDSHARED#
/^CPP *=/s#=.*#=$CPP#
/^STATICLIB *=/s#=.*#=$STATICLIB#
/^SHAREDLIB *=/s#=.*#=$SHAREDLIB#
/^SHAREDLIBV *=/s#=.*#=$SHAREDLIBV#
/^SHAREDLIBM *=/s#=.*#=$SHAREDLIBM#
/^AR *=/s#=.*#=$AR#
/^ARFLAGS *=/s#=.*#=$ARFLAGS#
/^RANLIB *=/s#=.*#=$RANLIB#
/^LDCONFIG *=/s#=.*#=$LDCONFIG#
/^LDSHAREDLIBC *=/s#=.*#=$LDSHAREDLIBC#
/^EXE *=/s#=.*#=$EXE#
/^prefix *=/s#=.*#=$prefix#
/^exec_prefix *=/s#=.*#=$exec_prefix#
/^libdir *=/s#=.*#=$libdir#
/^sharedlibdir *=/s#=.*#=$sharedlibdir#
/^includedir *=/s#=.*#=$includedir#
/^mandir *=/s#=.*#=$mandir#
/^OBJC *=/s#=.*#= $OBJC#
/^PIC_OBJC *=/s#=.*#= $PIC_OBJC#
/^all: */s#:.*#: $ALL#
/^test: */s#:.*#: $TEST#
" > Makefile

# create zlib.pc with the configure results
sed < zlib.pc.in "
/^CC *=/s#=.*#=$CC#
/^CFLAGS *=/s#=.*#=$CFLAGS#
/^CPP *=/s#=.*#=$CPP#
/^LDSHARED *=/s#=.*#=$LDSHARED#
/^STATICLIB *=/s#=.*#=$STATICLIB#
/^SHAREDLIB *=/s#=.*#=$SHAREDLIB#
/^SHAREDLIBV *=/s#=.*#=$SHAREDLIBV#
/^SHAREDLIBM *=/s#=.*#=$SHAREDLIBM#
/^AR *=/s#=.*#=$AR#
/^ARFLAGS *=/s#=.*#=$ARFLAGS#
/^RANLIB *=/s#=.*#=$RANLIB#
/^EXE *=/s#=.*#=$EXE#
/^prefix *=/s#=.*#=$prefix#
/^exec_prefix *=/s#=.*#=$exec_prefix#
/^libdir *=/s#=.*#=$libdir#
/^sharedlibdir *=/s#=.*#=$sharedlibdir#
/^includedir *=/s#=.*#=$includedir#
/^mandir *=/s#=.*#=$mandir#
/^LDFLAGS *=/s#=.*#=$LDFLAGS#
" | sed -e "
s/\@VERSION\@/$VER/g;
" > zlib.pc

# done
leave 0
