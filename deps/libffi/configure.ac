dnl Process this with autoconf to create configure

AC_PREREQ(2.68)

AC_INIT([libffi], [3.0.13], [http://github.com/atgreen/libffi/issues])
AC_CONFIG_HEADERS([fficonfig.h])

AC_CANONICAL_SYSTEM
target_alias=${target_alias-$host_alias}

. ${srcdir}/configure.host

AX_ENABLE_BUILDDIR

AM_INIT_AUTOMAKE

# The same as in boehm-gc and libstdc++. Have to borrow it from there.
# We must force CC to /not/ be precious variables; otherwise
# the wrong, non-multilib-adjusted value will be used in multilibs.
# As a side effect, we have to subst CFLAGS ourselves.
# Also save and restore CFLAGS, since AC_PROG_CC will come up with
# defaults of its own if none are provided.

m4_rename([_AC_ARG_VAR_PRECIOUS],[real_PRECIOUS])
m4_define([_AC_ARG_VAR_PRECIOUS],[])
save_CFLAGS=$CFLAGS
AC_PROG_CC
CFLAGS=$save_CFLAGS
m4_undefine([_AC_ARG_VAR_PRECIOUS])
m4_rename_force([real_PRECIOUS],[_AC_ARG_VAR_PRECIOUS])

AC_SUBST(CFLAGS)

AM_PROG_AS
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

# Test for 64-bit build.
AC_CHECK_SIZEOF([size_t])

AX_COMPILER_VENDOR
AX_CC_MAXOPT
# The AX_CFLAGS_WARN_ALL macro doesn't currently work for sunpro
# compiler.
if test "$ax_cv_c_compiler_vendor" != "sun"; then
  AX_CFLAGS_WARN_ALL
fi

if test "x$GCC" = "xyes"; then
  CFLAGS="$CFLAGS -fexceptions"
  touch local.exp
else
  cat > local.exp <<EOF
set CC_FOR_TARGET "$CC"
EOF
fi

AM_MAINTAINER_MODE

AC_CHECK_HEADERS(sys/mman.h)
AC_CHECK_FUNCS(mmap)
AC_FUNC_MMAP_BLACKLIST

dnl The -no-testsuite modules omit the test subdir.
AM_CONDITIONAL(TESTSUBDIR, test -d $srcdir/testsuite)

TARGETDIR="unknown"
case "$host" in
  aarch64*-*-*)
	TARGET=AARCH64; TARGETDIR=aarch64
	;;

  alpha*-*-*)
	TARGET=ALPHA; TARGETDIR=alpha;
	# Support 128-bit long double, changeable via command-line switch.
	HAVE_LONG_DOUBLE='defined(__LONG_DOUBLE_128__)'
	;;

  arm*-*-*)
	TARGET=ARM; TARGETDIR=arm
	;;

  amd64-*-freebsd* | amd64-*-openbsd*)
	TARGET=X86_64; TARGETDIR=x86
  	;;

  amd64-*-freebsd*)
	TARGET=X86_64; TARGETDIR=x86
  	;;

  amd64-*-freebsd*)
	TARGET=X86_64; TARGETDIR=x86
	;;

  avr32*-*-*)
	TARGET=AVR32; TARGETDIR=avr32
	;;

  bfin*)
  	TARGET=BFIN; TARGETDIR=bfin
  	;;

  cris-*-*)
	TARGET=LIBFFI_CRIS; TARGETDIR=cris
	;;

  frv-*-*)
	TARGET=FRV; TARGETDIR=frv
	;;

  hppa*-*-linux* | parisc*-*-linux* | hppa*-*-openbsd*)
	TARGET=PA_LINUX; TARGETDIR=pa
	;;
  hppa*64-*-hpux*)
	TARGET=PA64_HPUX; TARGETDIR=pa
	;;
  hppa*-*-hpux*)
	TARGET=PA_HPUX; TARGETDIR=pa
	;;

  i?86-*-freebsd* | i?86-*-openbsd*)
	TARGET=X86_FREEBSD; TARGETDIR=x86
	;;
  i?86-win32* | i?86-*-cygwin* | i?86-*-mingw* | i?86-*-os2* | i?86-*-interix*)
	TARGET=X86_WIN32; TARGETDIR=x86
	# All mingw/cygwin/win32 builds require -no-undefined for sharedlib.
	# We must also check with_cross_host to decide if this is a native
	# or cross-build and select where to install dlls appropriately.
	if test -n "$with_cross_host" &&
	   test x"$with_cross_host" != x"no"; then
	  AM_LTLDFLAGS='-no-undefined -bindir "$(toolexeclibdir)"';
	else
	  AM_LTLDFLAGS='-no-undefined -bindir "$(bindir)"';
	fi
	;;
  i?86-*-darwin*)
	TARGET=X86_DARWIN; TARGETDIR=x86
	;;
  i?86-*-solaris2.1[[0-9]]*)
	TARGETDIR=x86
	if test $ac_cv_sizeof_size_t = 4; then
	  TARGET=X86; 
	else
	  TARGET=X86_64; 
	fi	  
	;;

  x86_64-*-darwin*)
	TARGET=X86_DARWIN; TARGETDIR=x86
	;;

  x86_64-*-cygwin* | x86_64-*-mingw*)
	TARGET=X86_WIN64; TARGETDIR=x86
	# All mingw/cygwin/win32 builds require -no-undefined for sharedlib.
	# We must also check with_cross_host to decide if this is a native
	# or cross-build and select where to install dlls appropriately.
	if test -n "$with_cross_host" &&
	   test x"$with_cross_host" != x"no"; then
	  AM_LTLDFLAGS='-no-undefined -bindir "$(toolexeclibdir)"';
	else
	  AM_LTLDFLAGS='-no-undefined -bindir "$(bindir)"';
	fi
	;;

  i?86-*-* | x86_64-*-*)
	TARGETDIR=x86
	if test $ac_cv_sizeof_size_t = 4; then
	  case "$host" in
	    *-gnux32)
	      TARGET=X86_64
	      ;;
	    *)
	      TARGET=X86
	      ;;
          esac	
	else
	  TARGET=X86_64; 
	fi	  
	;;

  ia64*-*-*)
	TARGET=IA64; TARGETDIR=ia64
	;;

  m32r*-*-*)
	TARGET=M32R; TARGETDIR=m32r
	;;

  m68k-*-*)
	TARGET=M68K; TARGETDIR=m68k
	;;

  microblaze*-*-*)
	TARGET=MICROBLAZE; TARGETDIR=microblaze
	;;

  moxie-*-*)
	TARGET=MOXIE; TARGETDIR=moxie
	;;

  metag-*-*)
	TARGET=METAG; TARGETDIR=metag
	;;

  mips-sgi-irix5.* | mips-sgi-irix6.* | mips*-*-rtems*)
	TARGET=MIPS; TARGETDIR=mips
	;;
  mips*-*-linux* | mips*-*-openbsd*)
	# Support 128-bit long double for NewABI.
	HAVE_LONG_DOUBLE='defined(__mips64)'
	TARGET=MIPS; TARGETDIR=mips
	;;

  powerpc*-*-linux* | powerpc-*-sysv*)
	TARGET=POWERPC; TARGETDIR=powerpc
	;;
  powerpc-*-amigaos*)
	TARGET=POWERPC; TARGETDIR=powerpc
	;;
  powerpc-*-beos*)
	TARGET=POWERPC; TARGETDIR=powerpc
	;;
  powerpc-*-darwin* | powerpc64-*-darwin*)
	TARGET=POWERPC_DARWIN; TARGETDIR=powerpc
	;;
  powerpc-*-aix* | rs6000-*-aix*)
	TARGET=POWERPC_AIX; TARGETDIR=powerpc
	;;
  powerpc-*-freebsd* | powerpc-*-openbsd*)
	TARGET=POWERPC_FREEBSD; TARGETDIR=powerpc
	;;
  powerpc64-*-freebsd*)
	TARGET=POWERPC; TARGETDIR=powerpc
	;;
  powerpc*-*-rtems*)
	TARGET=POWERPC; TARGETDIR=powerpc
	;;

  s390-*-* | s390x-*-*)
	TARGET=S390; TARGETDIR=s390
	;;

  sh-*-* | sh[[34]]*-*-*)
	TARGET=SH; TARGETDIR=sh
	;;
  sh64-*-* | sh5*-*-*)
	TARGET=SH64; TARGETDIR=sh64
	;;

  sparc*-*-*)
	TARGET=SPARC; TARGETDIR=sparc
	;;

  tile*-*)
        TARGET=TILE; TARGETDIR=tile
        ;;

  xtensa*-*)
	TARGET=XTENSA; TARGETDIR=xtensa
	;;

esac

AC_SUBST(AM_RUNTESTFLAGS)
AC_SUBST(AM_LTLDFLAGS)

if test $TARGETDIR = unknown; then
  AC_MSG_ERROR(["libffi has not been ported to $host."])
fi

AM_CONDITIONAL(MIPS, test x$TARGET = xMIPS)
AM_CONDITIONAL(BFIN, test x$TARGET = xBFIN)
AM_CONDITIONAL(SPARC, test x$TARGET = xSPARC)
AM_CONDITIONAL(X86, test x$TARGET = xX86)
AM_CONDITIONAL(X86_FREEBSD, test x$TARGET = xX86_FREEBSD)
AM_CONDITIONAL(X86_WIN32, test x$TARGET = xX86_WIN32)
AM_CONDITIONAL(X86_WIN64, test x$TARGET = xX86_WIN64)
AM_CONDITIONAL(X86_DARWIN, test x$TARGET = xX86_DARWIN)
AM_CONDITIONAL(ALPHA, test x$TARGET = xALPHA)
AM_CONDITIONAL(IA64, test x$TARGET = xIA64)
AM_CONDITIONAL(M32R, test x$TARGET = xM32R)
AM_CONDITIONAL(M68K, test x$TARGET = xM68K)
AM_CONDITIONAL(MICROBLAZE, test x$TARGET = xMICROBLAZE)
AM_CONDITIONAL(METAG, test x$TARGET = xMETAG)
AM_CONDITIONAL(MOXIE, test x$TARGET = xMOXIE)
AM_CONDITIONAL(POWERPC, test x$TARGET = xPOWERPC)
AM_CONDITIONAL(POWERPC_AIX, test x$TARGET = xPOWERPC_AIX)
AM_CONDITIONAL(POWERPC_DARWIN, test x$TARGET = xPOWERPC_DARWIN)
AM_CONDITIONAL(POWERPC_FREEBSD, test x$TARGET = xPOWERPC_FREEBSD)
AM_CONDITIONAL(AARCH64, test x$TARGET = xAARCH64)
AM_CONDITIONAL(ARM, test x$TARGET = xARM)
AM_CONDITIONAL(AVR32, test x$TARGET = xAVR32)
AM_CONDITIONAL(LIBFFI_CRIS, test x$TARGET = xLIBFFI_CRIS)
AM_CONDITIONAL(FRV, test x$TARGET = xFRV)
AM_CONDITIONAL(S390, test x$TARGET = xS390)
AM_CONDITIONAL(X86_64, test x$TARGET = xX86_64)
AM_CONDITIONAL(SH, test x$TARGET = xSH)
AM_CONDITIONAL(SH64, test x$TARGET = xSH64)
AM_CONDITIONAL(PA_LINUX, test x$TARGET = xPA_LINUX)
AM_CONDITIONAL(PA_HPUX, test x$TARGET = xPA_HPUX)
AM_CONDITIONAL(PA64_HPUX, test x$TARGET = xPA64_HPUX)
AM_CONDITIONAL(TILE, test x$TARGET = xTILE)
AM_CONDITIONAL(XTENSA, test x$TARGET = xXTENSA)

AC_HEADER_STDC
AC_CHECK_FUNCS(memcpy)
AC_FUNC_ALLOCA

AC_CHECK_SIZEOF(double)
AC_CHECK_SIZEOF(long double)

# Also AC_SUBST this variable for ffi.h.
if test -z "$HAVE_LONG_DOUBLE"; then
  HAVE_LONG_DOUBLE=0
  if test $ac_cv_sizeof_double != $ac_cv_sizeof_long_double; then
    if test $ac_cv_sizeof_long_double != 0; then
      HAVE_LONG_DOUBLE=1
      AC_DEFINE(HAVE_LONG_DOUBLE, 1, [Define if you have the long double type and it is bigger than a double])
    fi
  fi
fi
AC_SUBST(HAVE_LONG_DOUBLE)

AC_C_BIGENDIAN

GCC_AS_CFI_PSEUDO_OP

if test x$TARGET = xSPARC; then
    AC_CACHE_CHECK([assembler and linker support unaligned pc related relocs],
	libffi_cv_as_sparc_ua_pcrel, [
	save_CFLAGS="$CFLAGS"
	save_LDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS -fpic"
	LDFLAGS="$LDFLAGS -shared"
	AC_TRY_LINK([asm (".text; foo: nop; .data; .align 4; .byte 0; .uaword %r_disp32(foo); .text");],,
		    [libffi_cv_as_sparc_ua_pcrel=yes],
		    [libffi_cv_as_sparc_ua_pcrel=no])
	CFLAGS="$save_CFLAGS"
	LDFLAGS="$save_LDFLAGS"])
    if test "x$libffi_cv_as_sparc_ua_pcrel" = xyes; then
	AC_DEFINE(HAVE_AS_SPARC_UA_PCREL, 1,
		  [Define if your assembler and linker support unaligned PC relative relocs.])
    fi

    AC_CACHE_CHECK([assembler .register pseudo-op support],
       libffi_cv_as_register_pseudo_op, [
       libffi_cv_as_register_pseudo_op=unknown
       # Check if we have .register
       AC_TRY_COMPILE(,[asm (".register %g2, #scratch");],
		       [libffi_cv_as_register_pseudo_op=yes],
		       [libffi_cv_as_register_pseudo_op=no])
    ])
    if test "x$libffi_cv_as_register_pseudo_op" = xyes; then
       AC_DEFINE(HAVE_AS_REGISTER_PSEUDO_OP, 1,
	       [Define if your assembler supports .register.])
    fi
fi

if test x$TARGET = xX86 || test x$TARGET = xX86_WIN32 || test x$TARGET = xX86_64; then
    AC_CACHE_CHECK([assembler supports pc related relocs],
	libffi_cv_as_x86_pcrel, [
	libffi_cv_as_x86_pcrel=no
	echo '.text; foo: nop; .data; .long foo-.; .text' > conftest.s
	if $CC $CFLAGS -c conftest.s > /dev/null 2>&1; then
	    libffi_cv_as_x86_pcrel=yes
	fi
	])
    if test "x$libffi_cv_as_x86_pcrel" = xyes; then
	AC_DEFINE(HAVE_AS_X86_PCREL, 1,
		  [Define if your assembler supports PC relative relocs.])
    fi

    AC_CACHE_CHECK([assembler .ascii pseudo-op support],
       libffi_cv_as_ascii_pseudo_op, [
       libffi_cv_as_ascii_pseudo_op=unknown
       # Check if we have .ascii
       AC_TRY_COMPILE(,[asm (".ascii \\"string\\"");],
		       [libffi_cv_as_ascii_pseudo_op=yes],
		       [libffi_cv_as_ascii_pseudo_op=no])
    ])
    if test "x$libffi_cv_as_ascii_pseudo_op" = xyes; then
       AC_DEFINE(HAVE_AS_ASCII_PSEUDO_OP, 1,
	       [Define if your assembler supports .ascii.])
    fi

    AC_CACHE_CHECK([assembler .string pseudo-op support],
       libffi_cv_as_string_pseudo_op, [
       libffi_cv_as_string_pseudo_op=unknown
       # Check if we have .string
       AC_TRY_COMPILE(,[asm (".string \\"string\\"");],
		       [libffi_cv_as_string_pseudo_op=yes],
		       [libffi_cv_as_string_pseudo_op=no])
    ])
    if test "x$libffi_cv_as_string_pseudo_op" = xyes; then
       AC_DEFINE(HAVE_AS_STRING_PSEUDO_OP, 1,
	       [Define if your assembler supports .string.])
    fi
fi

# On PaX enable kernels that have MPROTECT enable we can't use PROT_EXEC.
AC_ARG_ENABLE(pax_emutramp,
  [  --enable-pax_emutramp       enable pax emulated trampolines, for we can't use PROT_EXEC],
  if test "$enable_pax_emutramp" = "yes"; then
    AC_DEFINE(FFI_MMAP_EXEC_EMUTRAMP_PAX, 1,
      [Define this if you want to enable pax emulated trampolines])
  fi)

if test x$TARGET = xX86_WIN64; then
    LT_SYS_SYMBOL_USCORE
    if test "x$sys_symbol_underscore" = xyes; then
        AC_DEFINE(SYMBOL_UNDERSCORE, 1, [Define if symbols are underscored.])
    fi
fi

FFI_EXEC_TRAMPOLINE_TABLE=0
case "$target" in
     *arm*-apple-darwin*)
       FFI_EXEC_TRAMPOLINE_TABLE=1
       AC_DEFINE(FFI_EXEC_TRAMPOLINE_TABLE, 1,
                 [Cannot use PROT_EXEC on this target, so, we revert to
                   alternative means])
     ;;
     *-apple-darwin1* | *-*-freebsd* | *-*-kfreebsd* | *-*-openbsd* | *-pc-solaris*)
       AC_DEFINE(FFI_MMAP_EXEC_WRIT, 1,
                 [Cannot use malloc on this target, so, we revert to
                   alternative means])
     ;;
esac
AM_CONDITIONAL(FFI_EXEC_TRAMPOLINE_TABLE, test x$FFI_EXEC_TRAMPOLINE_TABLE = x1)
AC_SUBST(FFI_EXEC_TRAMPOLINE_TABLE)

if test x$TARGET = xX86_64; then
    AC_CACHE_CHECK([assembler supports unwind section type],
	libffi_cv_as_x86_64_unwind_section_type, [
	libffi_cv_as_x86_64_unwind_section_type=yes
	echo '.section .eh_frame,"a",@unwind' > conftest.s
	if $CC $CFLAGS -c conftest.s 2>&1 | grep -i warning > /dev/null; then
	    libffi_cv_as_x86_64_unwind_section_type=no
	fi
	])
    if test "x$libffi_cv_as_x86_64_unwind_section_type" = xyes; then
	AC_DEFINE(HAVE_AS_X86_64_UNWIND_SECTION_TYPE, 1,
		  [Define if your assembler supports unwind section type.])
    fi
fi

if test "x$GCC" = "xyes"; then
  AC_CACHE_CHECK([whether .eh_frame section should be read-only],
      libffi_cv_ro_eh_frame, [
  	libffi_cv_ro_eh_frame=no
  	echo 'extern void foo (void); void bar (void) { foo (); foo (); }' > conftest.c
  	if $CC $CFLAGS -c -fpic -fexceptions -o conftest.o conftest.c > /dev/null 2>&1; then
	    objdump -h conftest.o > conftest.dump 2>&1
	    libffi_eh_frame_line=`grep -n eh_frame conftest.dump | cut -d: -f 1`
	    libffi_test_line=`expr $libffi_eh_frame_line + 1`p
	    sed -n $libffi_test_line conftest.dump > conftest.line
  	    if grep READONLY conftest.line > /dev/null; then
  		libffi_cv_ro_eh_frame=yes
  	    fi
  	fi
  	rm -f conftest.*
      ])
  if test "x$libffi_cv_ro_eh_frame" = xyes; then
      AC_DEFINE(HAVE_RO_EH_FRAME, 1,
  	      [Define if .eh_frame sections should be read-only.])
      AC_DEFINE(EH_FRAME_FLAGS, "a",
  	      [Define to the flags needed for the .section .eh_frame directive.  ])
  else
      AC_DEFINE(EH_FRAME_FLAGS, "aw",
  	      [Define to the flags needed for the .section .eh_frame directive.  ])
  fi

  AC_CACHE_CHECK([for __attribute__((visibility("hidden")))],
      libffi_cv_hidden_visibility_attribute, [
  	echo 'int __attribute__ ((visibility ("hidden"))) foo (void) { return 1  ; }' > conftest.c
  	libffi_cv_hidden_visibility_attribute=no
  	if AC_TRY_COMMAND(${CC-cc} -Werror -S conftest.c -o conftest.s 1>&AS_MESSAGE_LOG_FD); then
  	    if grep '\.hidden.*foo' conftest.s >/dev/null; then
  		libffi_cv_hidden_visibility_attribute=yes
  	    fi
  	fi
  	rm -f conftest.*
      ])
  if test $libffi_cv_hidden_visibility_attribute = yes; then
      AC_DEFINE(HAVE_HIDDEN_VISIBILITY_ATTRIBUTE, 1,
  	      [Define if __attribute__((visibility("hidden"))) is supported.])
  fi
fi

AH_BOTTOM([
#ifdef HAVE_HIDDEN_VISIBILITY_ATTRIBUTE
#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name) .hidden name
#else
#define FFI_HIDDEN __attribute__ ((visibility ("hidden")))
#endif
#else
#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name)
#else
#define FFI_HIDDEN
#endif
#endif
])

AC_SUBST(TARGET)
AC_SUBST(TARGETDIR)

AC_SUBST(SHELL)

AC_ARG_ENABLE(debug,
[  --enable-debug          debugging mode],
  if test "$enable_debug" = "yes"; then
    AC_DEFINE(FFI_DEBUG, 1, [Define this if you want extra debugging.])
  fi)
AM_CONDITIONAL(FFI_DEBUG, test "$enable_debug" = "yes")

AC_ARG_ENABLE(structs,
[  --disable-structs       omit code for struct support],
  if test "$enable_structs" = "no"; then
    AC_DEFINE(FFI_NO_STRUCTS, 1, [Define this is you do not want support for aggregate types.])
  fi)
AM_CONDITIONAL(FFI_DEBUG, test "$enable_debug" = "yes")

AC_ARG_ENABLE(raw-api,
[  --disable-raw-api       make the raw api unavailable],
  if test "$enable_raw_api" = "no"; then
    AC_DEFINE(FFI_NO_RAW_API, 1, [Define this is you do not want support for the raw API.])
  fi)

AC_ARG_ENABLE(purify-safety,
[  --enable-purify-safety  purify-safe mode],
  if test "$enable_purify_safety" = "yes"; then
    AC_DEFINE(USING_PURIFY, 1, [Define this if you are using Purify and want to suppress spurious messages.])
  fi)

# These variables are only ever used when we cross-build to X86_WIN32.
# And we only support this with GCC, so...
if test "x$GCC" = "xyes"; then
  if test -n "$with_cross_host" &&
     test x"$with_cross_host" != x"no"; then
    toolexecdir='$(exec_prefix)/$(target_alias)'
    toolexeclibdir='$(toolexecdir)/lib'
  else
    toolexecdir='$(libdir)/gcc-lib/$(target_alias)'
    toolexeclibdir='$(libdir)'
  fi
  multi_os_directory=`$CC -print-multi-os-directory`
  case $multi_os_directory in
    .) ;; # Avoid trailing /.
    ../*) toolexeclibdir=$toolexeclibdir/$multi_os_directory ;;
  esac
  AC_SUBST(toolexecdir)
else
  toolexeclibdir='$(libdir)'
fi
AC_SUBST(toolexeclibdir)

AC_CONFIG_COMMANDS(include, [test -d include || mkdir include])
AC_CONFIG_COMMANDS(src, [
test -d src || mkdir src
test -d src/$TARGETDIR || mkdir src/$TARGETDIR
], [TARGETDIR="$TARGETDIR"])

AC_CONFIG_LINKS(include/ffitarget.h:src/$TARGETDIR/ffitarget.h)

AC_CONFIG_FILES(include/Makefile include/ffi.h Makefile testsuite/Makefile man/Makefile libffi.pc)

AC_OUTPUT
