# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

# default target (when running `make` with no argument)
lib-release:

# Modules
ZSTD_LIB_COMPRESSION ?= 1
ZSTD_LIB_DECOMPRESSION ?= 1
ZSTD_LIB_DICTBUILDER ?= 1
ZSTD_LIB_DEPRECATED ?= 0

# Input variables for libzstd.mk
ifeq ($(ZSTD_LIB_COMPRESSION), 0)
  ZSTD_LIB_DICTBUILDER = 0
  ZSTD_LIB_DEPRECATED = 0
endif

ifeq ($(ZSTD_LIB_DECOMPRESSION), 0)
  ZSTD_LEGACY_SUPPORT = 0
  ZSTD_LIB_DEPRECATED = 0
endif

include libzstd.mk

ZSTD_FILES := $(ZSTD_COMMON_FILES) $(ZSTD_LEGACY_FILES)

ifneq ($(ZSTD_LIB_COMPRESSION), 0)
  ZSTD_FILES += $(ZSTD_COMPRESS_FILES)
endif

ifneq ($(ZSTD_LIB_DECOMPRESSION), 0)
  ZSTD_FILES += $(ZSTD_DECOMPRESS_FILES)
endif

ifneq ($(ZSTD_LIB_DEPRECATED), 0)
  ZSTD_FILES += $(ZSTD_DEPRECATED_FILES)
endif

ifneq ($(ZSTD_LIB_DICTBUILDER), 0)
  ZSTD_FILES += $(ZSTD_DICTBUILDER_FILES)
endif

ZSTD_LOCAL_SRC := $(notdir $(ZSTD_FILES))
ZSTD_LOCAL_OBJ0 := $(ZSTD_LOCAL_SRC:.c=.o)
ZSTD_LOCAL_OBJ := $(ZSTD_LOCAL_OBJ0:.S=.o)

VERSION := $(ZSTD_VERSION)

# Note: by default, the static library is built single-threaded and dynamic library is built
# multi-threaded. It is possible to force multi or single threaded builds by appending
# -mt or -nomt to the build target (like lib-mt for multi-threaded, lib-nomt for single-threaded).


CPPFLAGS_DYNLIB  += -DZSTD_MULTITHREAD # dynamic library build defaults to multi-threaded
LDFLAGS_DYNLIB   += -pthread
CPPFLAGS_STATICLIB +=                  # static library build defaults to single-threaded


ifeq ($(findstring GCC,$(CCVER)),GCC)
decompress/zstd_decompress_block.o : CFLAGS+=-fno-tree-vectorize
endif


# macOS linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/DynamicLibraryDesignGuidelines.html
ifeq ($(UNAME), Darwin)
  SHARED_EXT = dylib
  SHARED_EXT_MAJOR = $(LIBVER_MAJOR).$(SHARED_EXT)
  SHARED_EXT_VER = $(LIBVER).$(SHARED_EXT)
  SONAME_FLAGS = -install_name $(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR) -compatibility_version $(LIBVER_MAJOR) -current_version $(LIBVER)
else
  ifeq ($(UNAME), AIX)
    SONAME_FLAGS =
  else
    SONAME_FLAGS = -Wl,-soname=libzstd.$(SHARED_EXT).$(LIBVER_MAJOR)
  endif
  SHARED_EXT = so
  SHARED_EXT_MAJOR = $(SHARED_EXT).$(LIBVER_MAJOR)
  SHARED_EXT_VER = $(SHARED_EXT).$(LIBVER)
endif


.PHONY: all
all: lib


.PHONY: libzstd.a  # must be run every time
libzstd.a: CPPFLAGS += $(CPPFLAGS_STATICLIB)

SET_CACHE_DIRECTORY = \
   +$(MAKE) --no-print-directory $@ \
    BUILD_DIR=obj/$(HASH_DIR) \
    CPPFLAGS="$(CPPFLAGS)" \
    CFLAGS="$(CFLAGS)" \
    LDFLAGS="$(LDFLAGS)"

ifndef BUILD_DIR
# determine BUILD_DIR from compilation flags

libzstd.a:
	$(SET_CACHE_DIRECTORY)

else
# BUILD_DIR is defined

ZSTD_STATICLIB_DIR := $(BUILD_DIR)/static
ZSTD_STATICLIB := $(ZSTD_STATICLIB_DIR)/libzstd.a
ZSTD_STATICLIB_OBJ := $(addprefix $(ZSTD_STATICLIB_DIR)/,$(ZSTD_LOCAL_OBJ))
$(ZSTD_STATICLIB): ARFLAGS = rcs
$(ZSTD_STATICLIB): | $(ZSTD_STATICLIB_DIR)
$(ZSTD_STATICLIB): $(ZSTD_STATICLIB_OBJ)
  # Check for multithread flag at target execution time
	$(if $(filter -DZSTD_MULTITHREAD,$(CPPFLAGS)),\
    @echo compiling multi-threaded static library $(LIBVER),\
    @echo compiling single-threaded static library $(LIBVER))
	$(AR) $(ARFLAGS) $@ $^

libzstd.a: $(ZSTD_STATICLIB)
	cp -f $< $@

endif

ifneq (,$(filter Windows%,$(TARGET_SYSTEM)))

LIBZSTD = dll/libzstd.dll
$(LIBZSTD): $(ZSTD_FILES)
	@echo compiling dynamic library $(LIBVER)
	$(CC) $(FLAGS) -DZSTD_DLL_EXPORT=1 -Wl,--out-implib,dll/libzstd.dll.a -shared $^ -o $@

else  # not Windows

LIBZSTD = libzstd.$(SHARED_EXT_VER)
.PHONY: $(LIBZSTD)  # must be run every time
$(LIBZSTD): CPPFLAGS += $(CPPFLAGS_DYNLIB)
$(LIBZSTD): CFLAGS   += -fPIC -fvisibility=hidden
$(LIBZSTD): LDFLAGS  += -shared $(LDFLAGS_DYNLIB)

ifndef BUILD_DIR
# determine BUILD_DIR from compilation flags

$(LIBZSTD):
	$(SET_CACHE_DIRECTORY)

else
# BUILD_DIR is defined

ZSTD_DYNLIB_DIR := $(BUILD_DIR)/dynamic
ZSTD_DYNLIB := $(ZSTD_DYNLIB_DIR)/$(LIBZSTD)
ZSTD_DYNLIB_OBJ := $(addprefix $(ZSTD_DYNLIB_DIR)/,$(ZSTD_LOCAL_OBJ))

$(ZSTD_DYNLIB): | $(ZSTD_DYNLIB_DIR)
$(ZSTD_DYNLIB): $(ZSTD_DYNLIB_OBJ)
# Check for multithread flag at target execution time
	$(if $(filter -DZSTD_MULTITHREAD,$(CPPFLAGS)),\
    @echo compiling multi-threaded dynamic library $(LIBVER),\
    @echo compiling single-threaded dynamic library $(LIBVER))
	$(CC) $(FLAGS) $^ $(LDFLAGS) $(SONAME_FLAGS) -o $@
	@echo creating versioned links
	ln -sf $@ libzstd.$(SHARED_EXT_MAJOR)
	ln -sf $@ libzstd.$(SHARED_EXT)

$(LIBZSTD): $(ZSTD_DYNLIB)
	cp -f $< $@

endif  # ifndef BUILD_DIR
endif  # if windows

.PHONY: libzstd
libzstd : $(LIBZSTD)

.PHONY: lib
lib : libzstd.a libzstd


# note : do not define lib-mt or lib-release as .PHONY
# make does not consider implicit pattern rule for .PHONY target

%-mt : CPPFLAGS_DYNLIB  := -DZSTD_MULTITHREAD
%-mt : CPPFLAGS_STATICLIB := -DZSTD_MULTITHREAD
%-mt : LDFLAGS_DYNLIB   := -pthread
%-mt : %
	@echo multi-threaded build completed

%-nomt : CPPFLAGS_DYNLIB  :=
%-nomt : LDFLAGS_DYNLIB   :=
%-nomt : CPPFLAGS_STATICLIB :=
%-nomt : %
	@echo single-threaded build completed

%-release : DEBUGFLAGS :=
%-release : %
	@echo release build completed


# Generate .h dependencies automatically

# -MMD: compiler generates dependency information as a side-effect of compilation, without system headers
# -MP: adds phony target for each dependency other than main file.
DEPFLAGS = -MMD -MP

# ensure that ZSTD_DYNLIB_DIR exists prior to generating %.o
$(ZSTD_DYNLIB_DIR)/%.o : %.c | $(ZSTD_DYNLIB_DIR)
	@echo CC $@
	$(COMPILE.c) $(DEPFLAGS) $(OUTPUT_OPTION) $<

$(ZSTD_STATICLIB_DIR)/%.o : %.c | $(ZSTD_STATICLIB_DIR)
	@echo CC $@
	$(COMPILE.c) $(DEPFLAGS) $(OUTPUT_OPTION) $<

$(ZSTD_DYNLIB_DIR)/%.o : %.S | $(ZSTD_DYNLIB_DIR)
	@echo AS $@
	$(COMPILE.S) $(OUTPUT_OPTION) $<

$(ZSTD_STATICLIB_DIR)/%.o : %.S | $(ZSTD_STATICLIB_DIR)
	@echo AS $@
	$(COMPILE.S) $(OUTPUT_OPTION) $<

MKDIR ?= mkdir -p
$(BUILD_DIR) $(ZSTD_DYNLIB_DIR) $(ZSTD_STATICLIB_DIR):
	$(MKDIR) $@

DEPFILES := $(ZSTD_DYNLIB_OBJ:.o=.d) $(ZSTD_STATICLIB_OBJ:.o=.d)
$(DEPFILES):

# The leading '-' means: do not fail is include fails (ex: directory does not exist yet)
-include $(wildcard $(DEPFILES))


# Special case : build library in single-thread mode _and_ without zstdmt_compress.c
# Note : we still need threading.c and pool.c for the dictionary builder,
# but they will correctly behave single-threaded.
ZSTDMT_FILES = zstdmt_compress.c
ZSTD_NOMT_FILES = $(filter-out $(ZSTDMT_FILES),$(notdir $(ZSTD_FILES)))
libzstd-nomt: CFLAGS += -fPIC -fvisibility=hidden
libzstd-nomt: LDFLAGS += -shared
libzstd-nomt: $(ZSTD_NOMT_FILES)
	@echo compiling single-thread dynamic library $(LIBVER)
	@echo files : $(ZSTD_NOMT_FILES)
	@if echo "$(ZSTD_NOMT_FILES)" | tr ' ' '\n' | $(GREP) -q zstdmt; then \
        echo "Error: Found zstdmt in list."; \
        exit 1; \
    fi
	$(CC) $(FLAGS) $^ $(LDFLAGS) $(SONAME_FLAGS) -o $@

.PHONY: clean
clean:
	$(RM) -r *.dSYM   # macOS-specific
	$(RM) core *.o *.a *.gcda *.$(SHARED_EXT) *.$(SHARED_EXT).* libzstd.pc
	$(RM) dll/libzstd.dll dll/libzstd.lib libzstd-nomt*
	$(RM) -r obj/*
	@echo Cleaning library completed

#-----------------------------------------------------------------------------
# make install is validated only for below listed environments
#-----------------------------------------------------------------------------
ifneq (,$(filter $(UNAME),Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS Haiku AIX MSYS_NT CYGWIN_NT))

lib: libzstd.pc

HAS_EXPLICIT_EXEC_PREFIX := $(if $(or $(EXEC_PREFIX),$(exec_prefix)),1,)

DESTDIR     ?=
# directory variables : GNU conventions prefer lowercase
# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html
# support both lower and uppercase (BSD), use uppercase in script
prefix      ?= /usr/local
PREFIX      ?= $(prefix)
exec_prefix ?= $(PREFIX)
EXEC_PREFIX ?= $(exec_prefix)
libdir      ?= $(EXEC_PREFIX)/lib
LIBDIR      ?= $(libdir)
includedir  ?= $(PREFIX)/include
INCLUDEDIR  ?= $(includedir)

PCINCDIR := $(patsubst $(PREFIX)%,%,$(INCLUDEDIR))
PCLIBDIR := $(patsubst $(EXEC_PREFIX)%,%,$(LIBDIR))

# If we successfully stripped off a prefix, we'll add a reference to the
# relevant pc variable.
PCINCPREFIX := $(if $(findstring $(INCLUDEDIR),$(PCINCDIR)),,$${prefix})
PCLIBPREFIX := $(if $(findstring $(LIBDIR),$(PCLIBDIR)),,$${exec_prefix})

# If no explicit EXEC_PREFIX was set by the caller, write it out as a reference
# to PREFIX, rather than as a resolved value.
PCEXEC_PREFIX := $(if $(HAS_EXPLICIT_EXEC_PREFIX),$(EXEC_PREFIX),$${prefix})

ifneq (,$(filter $(UNAME),FreeBSD NetBSD DragonFly))
  PKGCONFIGDIR ?= $(PREFIX)/libdata/pkgconfig
else
  PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
endif

ifneq (,$(filter $(UNAME),SunOS))
  INSTALL ?= ginstall
else
  INSTALL ?= install
endif

INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA    ?= $(INSTALL) -m 644


libzstd.pc: libzstd.pc.in
	@echo creating pkgconfig
	@sed \
	        -e 's|@PREFIX@|$(PREFIX)|' \
	        -e 's|@EXEC_PREFIX@|$(PCEXEC_PREFIX)|' \
	        -e 's|@INCLUDEDIR@|$(PCINCPREFIX)$(PCINCDIR)|' \
	        -e 's|@LIBDIR@|$(PCLIBPREFIX)$(PCLIBDIR)|' \
	        -e 's|@VERSION@|$(VERSION)|' \
	        -e 's|@LIBS_PRIVATE@|$(LDFLAGS_DYNLIB)|' \
	        $< >$@

.PHONY: install
install: install-pc install-static install-shared install-includes
	@echo zstd static and shared library installed

.PHONY: install-pc
install-pc: libzstd.pc
	[ -e $(DESTDIR)$(PKGCONFIGDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(PKGCONFIGDIR)/
	$(INSTALL_DATA) libzstd.pc $(DESTDIR)$(PKGCONFIGDIR)/

.PHONY: install-static
install-static:
	# only generate libzstd.a if it's not already present
	[ -e libzstd.a ] || $(MAKE) libzstd.a-release
	[ -e $(DESTDIR)$(LIBDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)/
	@echo Installing static library
	$(INSTALL_DATA) libzstd.a $(DESTDIR)$(LIBDIR)

.PHONY: install-shared
install-shared:
	# only generate libzstd.so if it's not already present
	[ -e $(LIBZSTD) ] || $(MAKE) libzstd-release
	[ -e $(DESTDIR)$(LIBDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(LIBDIR)/
	@echo Installing shared library
	$(INSTALL_PROGRAM) $(LIBZSTD) $(DESTDIR)$(LIBDIR)
	ln -sf $(LIBZSTD) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	ln -sf $(LIBZSTD) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)

.PHONY: install-includes
install-includes:
	[ -e $(DESTDIR)$(INCLUDEDIR) ] || $(INSTALL) -d -m 755 $(DESTDIR)$(INCLUDEDIR)/
	@echo Installing includes
	$(INSTALL_DATA) zstd.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_DATA) zstd_errors.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_DATA) zdict.h $(DESTDIR)$(INCLUDEDIR)

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.a
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT)
	$(RM) $(DESTDIR)$(LIBDIR)/libzstd.$(SHARED_EXT_MAJOR)
	$(RM) $(DESTDIR)$(LIBDIR)/$(LIBZSTD)
	$(RM) $(DESTDIR)$(PKGCONFIGDIR)/libzstd.pc
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd.h
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zstd_errors.h
	$(RM) $(DESTDIR)$(INCLUDEDIR)/zdict.h
	@echo zstd libraries successfully uninstalled

endif
