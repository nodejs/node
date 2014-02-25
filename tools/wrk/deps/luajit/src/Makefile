##############################################################################
# LuaJIT Makefile. Requires GNU Make.
#
# Please read doc/install.html before changing any variables!
#
# Suitable for POSIX platforms (Linux, *BSD, OSX etc.).
# Also works with MinGW and Cygwin on Windows.
# Please check msvcbuild.bat for building with MSVC on Windows.
#
# Copyright (C) 2005-2012 Mike Pall. See Copyright Notice in luajit.h
##############################################################################

MAJVER=  2
MINVER=  0
RELVER=  2
ABIVER=  5.1
NODOTABIVER= 51

##############################################################################
#############################  COMPILER OPTIONS  #############################
##############################################################################
# These options mainly affect the speed of the JIT compiler itself, not the
# speed of the JIT-compiled code. Turn any of the optional settings on by
# removing the '#' in front of them. Make sure you force a full recompile
# with "make clean", followed by "make" if you change any options.
#
# LuaJIT builds as a native 32 or 64 bit binary by default.
CC= gcc
#
# Use this if you want to force a 32 bit build on a 64 bit multilib OS.
#CC= gcc -m32
#
# Since the assembler part does NOT maintain a frame pointer, it's pointless
# to slow down the C part by not omitting it. Debugging, tracebacks and
# unwinding are not affected -- the assembler part has frame unwind
# information and GCC emits it where needed (x64) or with -g (see CCDEBUG).
CCOPT= -O2 -fomit-frame-pointer
# Use this if you want to generate a smaller binary (but it's slower):
#CCOPT= -Os -fomit-frame-pointer
# Note: it's no longer recommended to use -O3 with GCC 4.x.
# The I-Cache bloat usually outweighs the benefits from aggressive inlining.
#
# Target-specific compiler options:
#
# x86 only: it's recommended to compile at least for i686. Better yet,
# compile for an architecture that has SSE2, too (-msse -msse2).
#
# x86/x64 only: For GCC 4.2 or higher and if you don't intend to distribute
# the binaries to a different machine you could also use: -march=native
#
CCOPT_x86= -march=i686
CCOPT_x64=
CCOPT_arm=
CCOPT_ppc=
CCOPT_ppcspe=
CCOPT_mips=
#
CCDEBUG=
# Uncomment the next line to generate debug information:
#CCDEBUG= -g
#
CCWARN= -Wall
# Uncomment the next line to enable more warnings:
#CCWARN+= -Wextra -Wdeclaration-after-statement -Wredundant-decls -Wshadow -Wpointer-arith
#
##############################################################################

##############################################################################
################################  BUILD MODE  ################################
##############################################################################
# The default build mode is mixed mode on POSIX. On Windows this is the same
# as dynamic mode.
#
# Mixed mode creates a static + dynamic library and a statically linked luajit.
BUILDMODE= mixed
#
# Static mode creates a static library and a statically linked luajit.
#BUILDMODE= static
#
# Dynamic mode creates a dynamic library and a dynamically linked luajit.
# Note: this executable will only run when the library is installed!
#BUILDMODE= dynamic
#
##############################################################################

##############################################################################
#################################  FEATURES  #################################
##############################################################################
# Enable/disable these features as needed, but make sure you force a full
# recompile with "make clean", followed by "make".
XCFLAGS=
#
# Permanently disable the FFI extension to reduce the size of the LuaJIT
# executable. But please consider that the FFI library is compiled-in,
# but NOT loaded by default. It only allocates any memory, if you actually
# make use of it.
#XCFLAGS+= -DLUAJIT_DISABLE_FFI
#
# Features from Lua 5.2 that are unlikely to break existing code are
# enabled by default. Some other features that *might* break some existing
# code (e.g. __pairs or os.execute() return values) can be enabled here.
# Note: this does not provide full compatibility with Lua 5.2 at this time.
#XCFLAGS+= -DLUAJIT_ENABLE_LUA52COMPAT
#
# Disable the JIT compiler, i.e. turn LuaJIT into a pure interpreter.
#XCFLAGS+= -DLUAJIT_DISABLE_JIT
#
# Some architectures (e.g. PPC) can use either single-number (1) or
# dual-number (2) mode. Uncomment one of these lines to override the
# default mode. Please see LJ_ARCH_NUMMODE in lj_arch.h for details.
#XCFLAGS+= -DLUAJIT_NUMMODE=1
#XCFLAGS+= -DLUAJIT_NUMMODE=2
#
##############################################################################

##############################################################################
############################  DEBUGGING SUPPORT  #############################
##############################################################################
# Enable these options as needed, but make sure you force a full recompile
# with "make clean", followed by "make".
# Note that most of these are NOT suitable for benchmarking or release mode!
#
# Use the system provided memory allocator (realloc) instead of the
# bundled memory allocator. This is slower, but sometimes helpful for
# debugging. It's helpful for Valgrind's memcheck tool, too. This option
# cannot be enabled on x64, since the built-in allocator is mandatory.
#XCFLAGS+= -DLUAJIT_USE_SYSMALLOC
#
# This define is required to run LuaJIT under Valgrind. The Valgrind
# header files must be installed. You should enable debug information, too.
# Use --suppressions=lj.supp to avoid some false positives.
#XCFLAGS+= -DLUAJIT_USE_VALGRIND
#
# This is the client for the GDB JIT API. GDB 7.0 or higher is required
# to make use of it. See lj_gdbjit.c for details. Enabling this causes
# a non-negligible overhead, even when not running under GDB.
#XCFLAGS+= -DLUAJIT_USE_GDBJIT
#
# Turn on assertions for the Lua/C API to debug problems with lua_* calls.
# This is rather slow -- use only while developing C libraries/embeddings.
#XCFLAGS+= -DLUA_USE_APICHECK
#
# Turn on assertions for the whole LuaJIT VM. This significantly slows down
# everything. Use only if you suspect a problem with LuaJIT itself.
#XCFLAGS+= -DLUA_USE_ASSERT
#
##############################################################################
# You probably don't need to change anything below this line!
##############################################################################

##############################################################################
# Flags and options for host and target.
##############################################################################

# You can override the following variables at the make command line:
#   CC       HOST_CC       STATIC_CC       DYNAMIC_CC
#   CFLAGS   HOST_CFLAGS   TARGET_CFLAGS
#   LDFLAGS  HOST_LDFLAGS  TARGET_LDFLAGS  TARGET_SHLDFLAGS
#   LIBS     HOST_LIBS     TARGET_LIBS
#   CROSS    HOST_SYS      TARGET_SYS      TARGET_FLAGS
#
# Cross-compilation examples:
#   make HOST_CC="gcc -m32" CROSS=i586-mingw32msvc- TARGET_SYS=Windows
#   make HOST_CC="gcc -m32" CROSS=powerpc-linux-gnu-

CCOPTIONS= $(CCDEBUG) $(CCOPT) $(CCWARN) $(XCFLAGS) $(CFLAGS)
LDOPTIONS= $(CCDEBUG) $(LDFLAGS)

HOST_CC= $(CC)
HOST_RM= rm -f
# If left blank, minilua is built and used. You can supply an installed
# copy of (plain) Lua 5.1 or 5.2, plus Lua BitOp. E.g. with: HOST_LUA=lua
HOST_LUA=

HOST_XCFLAGS= -I.
HOST_XLDFLAGS=
HOST_XLIBS=
HOST_ACFLAGS= $(CCOPTIONS) $(HOST_XCFLAGS) $(TARGET_ARCH) $(HOST_CFLAGS)
HOST_ALDFLAGS= $(LDOPTIONS) $(HOST_XLDFLAGS) $(HOST_LDFLAGS)
HOST_ALIBS= $(HOST_XLIBS) $(LIBS) $(HOST_LIBS)

STATIC_CC = $(CROSS)$(CC)
DYNAMIC_CC = $(CROSS)$(CC) -fPIC
TARGET_CC= $(STATIC_CC)
TARGET_STCC= $(STATIC_CC)
TARGET_DYNCC= $(DYNAMIC_CC)
TARGET_LD= $(CROSS)$(CC)
TARGET_AR= $(CROSS)ar rcus
TARGET_STRIP= $(CROSS)strip

TARGET_SONAME= libluajit-$(ABIVER).so.$(MAJVER)
TARGET_DYLIBNAME= libluajit-$(ABIVER).$(MAJVER).dylib
TARGET_DYLIBPATH= $(or $(PREFIX),/usr/local)/lib/$(TARGET_DYLIBNAME)
TARGET_DLLNAME= lua$(NODOTABIVER).dll
TARGET_XSHLDFLAGS= -shared -fPIC -Wl,-soname,$(TARGET_SONAME)
TARGET_DYNXLDOPTS=

TARGET_LFSFLAGS= -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
TARGET_XCFLAGS= $(TARGET_LFSFLAGS) -U_FORTIFY_SOURCE
TARGET_XLDFLAGS=
TARGET_XLIBS= -lm
TARGET_TCFLAGS= $(CCOPTIONS) $(TARGET_XCFLAGS) $(TARGET_FLAGS) $(TARGET_CFLAGS)
TARGET_ACFLAGS= $(CCOPTIONS) $(TARGET_XCFLAGS) $(TARGET_FLAGS) $(TARGET_CFLAGS)
TARGET_ALDFLAGS= $(LDOPTIONS) $(TARGET_XLDFLAGS) $(TARGET_FLAGS) $(TARGET_LDFLAGS)
TARGET_ASHLDFLAGS= $(LDOPTIONS) $(TARGET_XSHLDFLAGS) $(TARGET_FLAGS) $(TARGET_SHLDFLAGS)
TARGET_ALIBS= $(TARGET_XLIBS) $(LIBS) $(TARGET_LIBS)

TARGET_TESTARCH=$(shell $(TARGET_CC) $(TARGET_TCFLAGS) -E lj_arch.h -dM)
ifneq (,$(findstring LJ_TARGET_X64 ,$(TARGET_TESTARCH)))
  TARGET_LJARCH= x64
else
ifneq (,$(findstring LJ_TARGET_X86 ,$(TARGET_TESTARCH)))
  TARGET_LJARCH= x86
else
ifneq (,$(findstring LJ_TARGET_ARM ,$(TARGET_TESTARCH)))
  TARGET_LJARCH= arm
else
ifneq (,$(findstring LJ_TARGET_PPC ,$(TARGET_TESTARCH)))
  TARGET_LJARCH= ppc
else
ifneq (,$(findstring LJ_TARGET_PPCSPE ,$(TARGET_TESTARCH)))
  TARGET_LJARCH= ppcspe
else
ifneq (,$(findstring LJ_TARGET_MIPS ,$(TARGET_TESTARCH)))
  ifneq (,$(findstring MIPSEL ,$(TARGET_TESTARCH)))
    TARGET_ARCH= -D__MIPSEL__=1
  endif
  TARGET_LJARCH= mips
else
  $(error Unsupported target architecture)
endif
endif
endif
endif
endif
endif

ifneq (,$(findstring LJ_TARGET_PS3 1,$(TARGET_TESTARCH)))
  TARGET_SYS= PS3
  TARGET_ARCH+= -D__CELLOS_LV2__
  TARGET_XCFLAGS+= -DLUAJIT_USE_SYSMALLOC
endif
ifneq (,$(findstring LJ_NO_UNWIND 1,$(TARGET_TESTARCH)))
  TARGET_ARCH+= -DLUAJIT_NO_UNWIND
endif

TARGET_XCFLAGS+= $(CCOPT_$(TARGET_LJARCH))
TARGET_ARCH+= $(patsubst %,-DLUAJIT_TARGET=LUAJIT_ARCH_%,$(TARGET_LJARCH))

ifneq (,$(PREFIX))
ifneq (/usr/local,$(PREFIX))
  TARGET_XCFLAGS+= -DLUA_XROOT=\"$(PREFIX)/\"
  ifneq (/usr,$(PREFIX))
    TARGET_DYNXLDOPTS= -Wl,-rpath,$(PREFIX)/lib
  endif
endif
endif

##############################################################################
# System detection.
##############################################################################

ifeq (Windows,$(findstring Windows,$(OS))$(MSYSTEM)$(TERM))
  HOST_SYS= Windows
  HOST_RM= del
else
  HOST_SYS:= $(shell uname -s)
  ifneq (,$(findstring MINGW,$(HOST_SYS)))
    HOST_SYS= Windows
    HOST_MSYS= mingw
  endif
  ifneq (,$(findstring CYGWIN,$(HOST_SYS)))
    HOST_SYS= Windows
    HOST_MSYS= cygwin
  endif
endif

TARGET_SYS?= $(HOST_SYS)
ifeq (Windows,$(TARGET_SYS))
  TARGET_STRIP+= --strip-unneeded
  TARGET_XSHLDFLAGS= -shared
  TARGET_DYNXLDOPTS=
else
ifeq (Darwin,$(TARGET_SYS))
  ifeq (,$(MACOSX_DEPLOYMENT_TARGET))
    export MACOSX_DEPLOYMENT_TARGET=10.4
  endif
  TARGET_STRIP+= -x
  TARGET_AR+= 2>/dev/null
  TARGET_XCFLAGS+= -fno-stack-protector
  TARGET_XSHLDFLAGS= -dynamiclib -single_module -undefined dynamic_lookup -fPIC
  TARGET_DYNXLDOPTS=
  TARGET_XSHLDFLAGS+= -install_name $(TARGET_DYLIBPATH) -compatibility_version $(MAJVER).$(MINVER) -current_version $(MAJVER).$(MINVER).$(RELVER)
  ifeq (x64,$(TARGET_LJARCH))
    TARGET_XLDFLAGS+= -pagezero_size 10000 -image_base 100000000
    TARGET_XSHLDFLAGS+= -image_base 7fff04c4a000
  endif
else
ifeq (iOS,$(TARGET_SYS))
  TARGET_STRIP+= -x
  TARGET_AR+= 2>/dev/null
  TARGET_XCFLAGS+= -fno-stack-protector
  TARGET_XSHLDFLAGS= -dynamiclib -single_module -undefined dynamic_lookup -fPIC
  TARGET_DYNXLDOPTS=
  TARGET_XSHLDFLAGS+= -install_name $(TARGET_DYLIBPATH) -compatibility_version $(MAJVER).$(MINVER) -current_version $(MAJVER).$(MINVER).$(RELVER)
else
  ifneq (,$(findstring stack-protector,$(shell $(TARGET_CC) -dumpspecs)))
    TARGET_XCFLAGS+= -fno-stack-protector
  endif
  ifneq (SunOS,$(TARGET_SYS))
    ifneq (PS3,$(TARGET_SYS))
      TARGET_XLDFLAGS+= -Wl,-E
    endif
  endif
  ifeq (Linux,$(TARGET_SYS))
    TARGET_XLIBS+= -ldl
  endif
  ifeq (GNU/kFreeBSD,$(TARGET_SYS))
    TARGET_XLIBS+= -ldl
  endif
endif
endif
endif

ifneq ($(HOST_SYS),$(TARGET_SYS))
  ifeq (Windows,$(TARGET_SYS))
    HOST_XCFLAGS+= -malign-double -DLUAJIT_OS=LUAJIT_OS_WINDOWS
  else
  ifeq (Linux,$(TARGET_SYS))
    HOST_XCFLAGS+= -DLUAJIT_OS=LUAJIT_OS_LINUX
  else
  ifeq (Darwin,$(TARGET_SYS))
    HOST_XCFLAGS+= -DLUAJIT_OS=LUAJIT_OS_OSX
  else
  ifeq (iOS,$(TARGET_SYS))
    HOST_XCFLAGS+= -DLUAJIT_OS=LUAJIT_OS_OSX
  else
    HOST_XCFLAGS+= -DLUAJIT_OS=LUAJIT_OS_OTHER
  endif
  endif
  endif
  endif
endif

ifneq (,$(CCDEBUG))
  TARGET_STRIP= @:
endif

##############################################################################
# Files and pathnames.
##############################################################################

MINILUA_O= host/minilua.o
MINILUA_LIBS= -lm
MINILUA_T= host/minilua
MINILUA_X= $(MINILUA_T)

ifeq (,$(HOST_LUA))
  HOST_LUA= $(MINILUA_X)
  DASM_DEP= $(MINILUA_T)
endif

DASM_DIR= ../dynasm
DASM= $(HOST_LUA) $(DASM_DIR)/dynasm.lua
DASM_XFLAGS=
DASM_AFLAGS=
DASM_ARCH= $(TARGET_LJARCH)

ifneq (,$(findstring LJ_ARCH_BITS 64,$(TARGET_TESTARCH)))
  DASM_AFLAGS+= -D P64
endif
ifneq (,$(findstring LJ_HASJIT 1,$(TARGET_TESTARCH)))
  DASM_AFLAGS+= -D JIT
endif
ifneq (,$(findstring LJ_HASFFI 1,$(TARGET_TESTARCH)))
  DASM_AFLAGS+= -D FFI
endif
ifneq (,$(findstring LJ_DUALNUM 1,$(TARGET_TESTARCH)))
  DASM_AFLAGS+= -D DUALNUM
endif
ifneq (,$(findstring LJ_ARCH_HASFPU 1,$(TARGET_TESTARCH)))
  DASM_AFLAGS+= -D FPU
  TARGET_ARCH+= -DLJ_ARCH_HASFPU=1
else
  TARGET_ARCH+= -DLJ_ARCH_HASFPU=0
endif
ifeq (,$(findstring LJ_ABI_SOFTFP 1,$(TARGET_TESTARCH)))
  DASM_AFLAGS+= -D HFABI
  TARGET_ARCH+= -DLJ_ABI_SOFTFP=0
else
  TARGET_ARCH+= -DLJ_ABI_SOFTFP=1
endif
DASM_AFLAGS+= -D VER=$(subst LJ_ARCH_VERSION_,,$(filter LJ_ARCH_VERSION_%,$(subst LJ_ARCH_VERSION ,LJ_ARCH_VERSION_,$(TARGET_TESTARCH))))
ifeq (Windows,$(TARGET_SYS))
  DASM_AFLAGS+= -D WIN
endif
ifeq (x86,$(TARGET_LJARCH))
  ifneq (,$(findstring __SSE2__ 1,$(TARGET_TESTARCH)))
    DASM_AFLAGS+= -D SSE
  endif
else
ifeq (x64,$(TARGET_LJARCH))
  DASM_ARCH= x86
else
ifeq (arm,$(TARGET_LJARCH))
  ifeq (iOS,$(TARGET_SYS))
    DASM_AFLAGS+= -D IOS
  endif
else
ifeq (ppc,$(TARGET_LJARCH))
  ifneq (,$(findstring LJ_ARCH_SQRT 1,$(TARGET_TESTARCH)))
    DASM_AFLAGS+= -D SQRT
  endif
  ifneq (,$(findstring LJ_ARCH_ROUND 1,$(TARGET_TESTARCH)))
    DASM_AFLAGS+= -D ROUND
  endif
  ifneq (,$(findstring LJ_ARCH_PPC64 1,$(TARGET_TESTARCH)))
    DASM_AFLAGS+= -D GPR64
  endif
  ifeq (PS3,$(TARGET_SYS))
    DASM_AFLAGS+= -D PPE -D TOC
  endif
endif
endif
endif
endif

DASM_FLAGS= $(DASM_XFLAGS) $(DASM_AFLAGS)
DASM_DASC= vm_$(DASM_ARCH).dasc

BUILDVM_O= host/buildvm.o host/buildvm_asm.o host/buildvm_peobj.o \
	   host/buildvm_lib.o host/buildvm_fold.o
BUILDVM_T= host/buildvm
BUILDVM_X= $(BUILDVM_T)

HOST_O= $(MINILUA_O) $(BUILDVM_O)
HOST_T= $(MINILUA_T) $(BUILDVM_T)

LJVM_S= lj_vm.s
LJVM_O= lj_vm.o
LJVM_BOUT= $(LJVM_S)
LJVM_MODE= elfasm

LJLIB_O= lib_base.o lib_math.o lib_bit.o lib_string.o lib_table.o \
	 lib_io.o lib_os.o lib_package.o lib_debug.o lib_jit.o lib_ffi.o
LJLIB_C= $(LJLIB_O:.o=.c)

LJCORE_O= lj_gc.o lj_err.o lj_char.o lj_bc.o lj_obj.o \
	  lj_str.o lj_tab.o lj_func.o lj_udata.o lj_meta.o lj_debug.o \
	  lj_state.o lj_dispatch.o lj_vmevent.o lj_vmmath.o lj_strscan.o \
	  lj_api.o lj_lex.o lj_parse.o lj_bcread.o lj_bcwrite.o lj_load.o \
	  lj_ir.o lj_opt_mem.o lj_opt_fold.o lj_opt_narrow.o \
	  lj_opt_dce.o lj_opt_loop.o lj_opt_split.o lj_opt_sink.o \
	  lj_mcode.o lj_snap.o lj_record.o lj_crecord.o lj_ffrecord.o \
	  lj_asm.o lj_trace.o lj_gdbjit.o \
	  lj_ctype.o lj_cdata.o lj_cconv.o lj_ccall.o lj_ccallback.o \
	  lj_carith.o lj_clib.o lj_cparse.o \
	  lj_lib.o lj_alloc.o lib_aux.o \
	  $(LJLIB_O) lib_init.o

LJVMCORE_O= $(LJVM_O) $(LJCORE_O)
LJVMCORE_DYNO= $(LJVMCORE_O:.o=_dyn.o)

LIB_VMDEF= jit/vmdef.lua
LIB_VMDEFP= $(LIB_VMDEF)

LUAJIT_O= luajit.o
LUAJIT_A= libluajit.a
LUAJIT_SO= libluajit.so
LUAJIT_T= luajit

ALL_T= $(LUAJIT_T) $(LUAJIT_A) $(LUAJIT_SO) $(HOST_T)
ALL_HDRGEN= lj_bcdef.h lj_ffdef.h lj_libdef.h lj_recdef.h lj_folddef.h \
	    host/buildvm_arch.h
ALL_GEN= $(LJVM_S) $(ALL_HDRGEN) $(LIB_VMDEFP)
WIN_RM= *.obj *.lib *.exp *.dll *.exe *.manifest *.pdb *.ilk
ALL_RM= $(ALL_T) $(ALL_GEN) *.o host/*.o $(WIN_RM)

##############################################################################
# Build mode handling.
##############################################################################

# Mixed mode defaults.
TARGET_O= $(LUAJIT_A)
TARGET_T= $(LUAJIT_T) $(LUAJIT_SO)
TARGET_DEP= $(LIB_VMDEF) $(LUAJIT_SO)

ifeq (Windows,$(TARGET_SYS))
  TARGET_DYNCC= $(STATIC_CC)
  LJVM_MODE= peobj
  LJVM_BOUT= $(LJVM_O)
  LUAJIT_T= luajit.exe
  ifeq (cygwin,$(HOST_MSYS))
    LUAJIT_SO= cyg$(TARGET_DLLNAME)
  else
    LUAJIT_SO= $(TARGET_DLLNAME)
  endif
  # Mixed mode is not supported on Windows. And static mode doesn't work well.
  # C modules cannot be loaded, because they bind to lua51.dll.
  ifneq (static,$(BUILDMODE))
    BUILDMODE= dynamic
    TARGET_XCFLAGS+= -DLUA_BUILD_AS_DLL
  endif
endif
ifeq (Darwin,$(TARGET_SYS))
  LJVM_MODE= machasm
endif
ifeq (iOS,$(TARGET_SYS))
  LJVM_MODE= machasm
endif
ifeq (SunOS,$(TARGET_SYS))
  BUILDMODE= static
endif
ifeq (PS3,$(TARGET_SYS))
  BUILDMODE= static
endif

ifeq (Windows,$(HOST_SYS))
  MINILUA_T= host/minilua.exe
  BUILDVM_T= host/buildvm.exe
  ifeq (,$(HOST_MSYS))
    MINILUA_X= host\minilua
    BUILDVM_X= host\buildvm
    ALL_RM:= $(subst /,\,$(ALL_RM))
  endif
endif

ifeq (static,$(BUILDMODE))
  TARGET_DYNCC= @:
  TARGET_T= $(LUAJIT_T)
  TARGET_DEP= $(LIB_VMDEF)
else
ifeq (dynamic,$(BUILDMODE))
  ifneq (Windows,$(TARGET_SYS))
    TARGET_CC= $(DYNAMIC_CC)
  endif
  TARGET_DYNCC= @:
  LJVMCORE_DYNO= $(LJVMCORE_O)
  TARGET_O= $(LUAJIT_SO)
  TARGET_XLDFLAGS+= $(TARGET_DYNXLDOPTS)
else
ifeq (Darwin,$(TARGET_SYS))
  TARGET_DYNCC= @:
  LJVMCORE_DYNO= $(LJVMCORE_O)
endif
ifeq (iOS,$(TARGET_SYS))
  TARGET_DYNCC= @:
  LJVMCORE_DYNO= $(LJVMCORE_O)
endif
endif
endif

Q= @
E= @echo
#Q=
#E= @:

##############################################################################
# Make targets.
##############################################################################

default all:	$(TARGET_T)

amalg:
	@grep "^[+|]" ljamalg.c
	$(MAKE) all "LJCORE_O=ljamalg.o"

clean:
	$(HOST_RM) $(ALL_RM)

depend:
	@for file in $(ALL_HDRGEN); do \
	  test -f $$file || touch $$file; \
	  done
	@$(HOST_CC) $(HOST_ACFLAGS) -MM *.c host/*.c | \
	  sed -e "s| [^ ]*/dasm_\S*\.h||g" \
	      -e "s|^\([^l ]\)|host/\1|" \
	      -e "s| lj_target_\S*\.h| lj_target_*.h|g" \
	      -e "s| lj_emit_\S*\.h| lj_emit_*.h|g" \
	      -e "s| lj_asm_\S*\.h| lj_asm_*.h|g" >Makefile.dep
	@for file in $(ALL_HDRGEN); do \
	  test -s $$file || $(HOST_RM) $$file; \
	  done

.PHONY: default all amalg clean depend

##############################################################################
# Rules for generated files.
##############################################################################

$(MINILUA_T): $(MINILUA_O)
	$(E) "HOSTLINK  $@"
	$(Q)$(HOST_CC) $(HOST_ALDFLAGS) -o $@ $(MINILUA_O) $(MINILUA_LIBS) $(HOST_ALIBS)

host/buildvm_arch.h: $(DASM_DASC) $(DASM_DEP)
	$(E) "DYNASM    $@"
	$(Q)$(DASM) $(DASM_FLAGS) -o $@ $(DASM_DASC)

host/buildvm.o: $(DASM_DIR)/dasm_*.h

$(BUILDVM_T): $(BUILDVM_O)
	$(E) "HOSTLINK  $@"
	$(Q)$(HOST_CC) $(HOST_ALDFLAGS) -o $@ $(BUILDVM_O) $(HOST_ALIBS)

$(LJVM_BOUT): $(BUILDVM_T)
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m $(LJVM_MODE) -o $@

lj_bcdef.h: $(BUILDVM_T) $(LJLIB_C)
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m bcdef -o $@ $(LJLIB_C)

lj_ffdef.h: $(BUILDVM_T) $(LJLIB_C)
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m ffdef -o $@ $(LJLIB_C)

lj_libdef.h: $(BUILDVM_T) $(LJLIB_C)
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m libdef -o $@ $(LJLIB_C)

lj_recdef.h: $(BUILDVM_T) $(LJLIB_C)
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m recdef -o $@ $(LJLIB_C)

$(LIB_VMDEF): $(BUILDVM_T) $(LJLIB_C)
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m vmdef -o $(LIB_VMDEFP) $(LJLIB_C)

lj_folddef.h: $(BUILDVM_T) lj_opt_fold.c
	$(E) "BUILDVM   $@"
	$(Q)$(BUILDVM_X) -m folddef -o $@ lj_opt_fold.c

##############################################################################
# Object file rules.
##############################################################################

%.o: %.c
	$(E) "CC        $@"
	$(Q)$(TARGET_DYNCC) $(TARGET_ACFLAGS) -c -o $(@:.o=_dyn.o) $<
	$(Q)$(TARGET_CC) $(TARGET_ACFLAGS) -c -o $@ $<

%.o: %.s
	$(E) "ASM       $@"
	$(Q)$(TARGET_DYNCC) $(TARGET_ACFLAGS) -c -o $(@:.o=_dyn.o) $<
	$(Q)$(TARGET_CC) $(TARGET_ACFLAGS) -c -o $@ $<

$(LUAJIT_O):
	$(E) "CC        $@"
	$(Q)$(TARGET_STCC) $(TARGET_ACFLAGS) -c -o $@ $<

$(HOST_O): %.o: %.c
	$(E) "HOSTCC    $@"
	$(Q)$(HOST_CC) $(HOST_ACFLAGS) -c -o $@ $<

include Makefile.dep

##############################################################################
# Target file rules.
##############################################################################

$(LUAJIT_A): $(LJVMCORE_O)
	$(E) "AR        $@"
	$(Q)$(TARGET_AR) $@ $(LJVMCORE_O)

# The dependency on _O, but linking with _DYNO is intentional.
$(LUAJIT_SO): $(LJVMCORE_O)
	$(E) "DYNLINK   $@"
	$(Q)$(TARGET_LD) $(TARGET_ASHLDFLAGS) -o $@ $(LJVMCORE_DYNO) $(TARGET_ALIBS)
	$(Q)$(TARGET_STRIP) $@

$(LUAJIT_T): $(TARGET_O) $(LUAJIT_O) $(TARGET_DEP)
	$(E) "LINK      $@"
	$(Q)$(TARGET_LD) $(TARGET_ALDFLAGS) -o $@ $(LUAJIT_O) $(TARGET_O) $(TARGET_ALIBS)
	$(Q)$(TARGET_STRIP) $@
	$(E) "OK        Successfully built LuaJIT"

##############################################################################
