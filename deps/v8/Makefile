# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Variable default definitions. Override them by exporting them in your shell.
CXX ?= g++
LINK ?= g++
OUTDIR ?= out
TESTJOBS ?= -j16
GYPFLAGS ?=
TESTFLAGS ?=
ANDROID_NDK_ROOT ?=
ANDROID_TOOL_PREFIX = $(ANDROID_NDK_ROOT)/toolchain/bin/arm-linux-androideabi

# Special build flags. Use them like this: "make library=shared"

# library=shared || component=shared_library
ifeq ($(library), shared)
  GYPFLAGS += -Dcomponent=shared_library
endif
ifdef component
  GYPFLAGS += -Dcomponent=$(component)
endif
# console=readline
ifdef console
  GYPFLAGS += -Dconsole=$(console)
endif
# disassembler=on
ifeq ($(disassembler), on)
  GYPFLAGS += -Dv8_enable_disassembler=1
endif
# objectprint=on
ifeq ($(objectprint), on)
  GYPFLAGS += -Dv8_object_print=1
endif
# snapshot=off
ifeq ($(snapshot), off)
  GYPFLAGS += -Dv8_use_snapshot='false'
endif
# gdbjit=on
ifeq ($(gdbjit), on)
  GYPFLAGS += -Dv8_enable_gdbjit=1
endif
# liveobjectlist=on
ifeq ($(liveobjectlist), on)
  GYPFLAGS += -Dv8_use_liveobjectlist=true
endif
# vfp3=off
ifeq ($(vfp3), off)
  GYPFLAGS += -Dv8_can_use_vfp_instructions=false
else
  GYPFLAGS += -Dv8_can_use_vfp_instructions=true
endif
# debuggersupport=off
ifeq ($(debuggersupport), off)
  GYPFLAGS += -Dv8_enable_debugger_support=0
endif
# soname_version=1.2.3
ifdef soname_version
  GYPFLAGS += -Dsoname_version=$(soname_version)
endif
# werror=no
ifeq ($(werror), no)
  GYPFLAGS += -Dwerror=''
endif
# presubmit=no
ifeq ($(presubmit), no)
  TESTFLAGS += --no-presubmit
endif
# strictaliasing=off (workaround for GCC-4.5)
ifeq ($(strictaliasing), off)
  GYPFLAGS += -Dv8_no_strict_aliasing=1
endif

# ----------------- available targets: --------------------
# - "dependencies": pulls in external dependencies (currently: GYP)
# - any arch listed in ARCHES (see below)
# - any mode listed in MODES
# - every combination <arch>.<mode>, e.g. "ia32.release"
# - "native": current host's architecture, release mode
# - any of the above with .check appended, e.g. "ia32.release.check"
# - "android": cross-compile for Android/ARM (release mode)
# - default (no target specified): build all DEFAULT_ARCHES and MODES
# - "check": build all targets and run all tests
# - "<arch>.clean" for any <arch> in ARCHES
# - "clean": clean all ARCHES

# ----------------- internal stuff ------------------------

# Architectures and modes to be compiled. Consider these to be internal
# variables, don't override them (use the targets instead).
ARCHES = ia32 x64 arm mips
DEFAULT_ARCHES = ia32 x64 arm
MODES = release debug

# List of files that trigger Makefile regeneration:
GYPFILES = build/all.gyp build/common.gypi build/standalone.gypi \
           preparser/preparser.gyp samples/samples.gyp src/d8.gyp \
           test/cctest/cctest.gyp tools/gyp/v8.gyp

# Generates all combinations of ARCHES and MODES, e.g. "ia32.release".
BUILDS = $(foreach mode,$(MODES),$(addsuffix .$(mode),$(ARCHES)))
# Generates corresponding test targets, e.g. "ia32.release.check".
CHECKS = $(addsuffix .check,$(BUILDS))
# File where previously used GYPFLAGS are stored.
ENVFILE = $(OUTDIR)/environment

.PHONY: all check clean dependencies $(ENVFILE).new native \
        $(ARCHES) $(MODES) $(BUILDS) $(CHECKS) $(addsuffix .clean,$(ARCHES)) \
        $(addsuffix .check,$(MODES)) $(addsuffix .check,$(ARCHES)) \
        must-set-ANDROID_NDK_ROOT

# Target definitions. "all" is the default.
all: $(MODES)

# Compile targets. MODES and ARCHES are convenience targets.
.SECONDEXPANSION:
$(MODES): $(addsuffix .$$@,$(DEFAULT_ARCHES))

$(ARCHES): $(addprefix $$@.,$(MODES))

# Defines how to build a particular target (e.g. ia32.release).
$(BUILDS): $(OUTDIR)/Makefile-$$(basename $$@)
	@$(MAKE) -C "$(OUTDIR)" -f Makefile-$(basename $@) \
	         CXX="$(CXX)" LINK="$(LINK)" \
	         BUILDTYPE=$(shell echo $(subst .,,$(suffix $@)) | \
	                     python -c "print raw_input().capitalize()") \
	         builddir="$(shell pwd)/$(OUTDIR)/$@"

native: $(OUTDIR)/Makefile-native
	@$(MAKE) -C "$(OUTDIR)" -f Makefile-native \
	         CXX="$(CXX)" LINK="$(LINK)" BUILDTYPE=Release \
	         builddir="$(shell pwd)/$(OUTDIR)/$@"

# TODO(jkummerow): add "android.debug" when we need it.
android android.release: $(OUTDIR)/Makefile-android
	@$(MAKE) -C "$(OUTDIR)" -f Makefile-android \
	        CXX="$(ANDROID_TOOL_PREFIX)-g++" \
	        AR="$(ANDROID_TOOL_PREFIX)-ar" \
	        RANLIB="$(ANDROID_TOOL_PREFIX)-ranlib" \
	        CC="$(ANDROID_TOOL_PREFIX)-gcc" \
	        LD="$(ANDROID_TOOL_PREFIX)-ld" \
	        LINK="$(ANDROID_TOOL_PREFIX)-g++" \
	        BUILDTYPE=Release \
	        builddir="$(shell pwd)/$(OUTDIR)/android.release"

# Test targets.
check: all
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) \
	    --arch=$(shell echo $(DEFAULT_ARCHES) | sed -e 's/ /,/g') \
	    $(TESTFLAGS)

$(addsuffix .check,$(MODES)): $$(basename $$@)
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) \
	    --mode=$(basename $@) $(TESTFLAGS)

$(addsuffix .check,$(ARCHES)): $$(basename $$@)
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) \
	    --arch=$(basename $@) $(TESTFLAGS)

$(CHECKS): $$(basename $$@)
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) \
	    --arch-and-mode=$(basename $@) $(TESTFLAGS)

native.check: native
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR)/native \
	    --arch-and-mode=. $(TESTFLAGS)

# Clean targets. You can clean each architecture individually, or everything.
$(addsuffix .clean,$(ARCHES)):
	rm -f $(OUTDIR)/Makefile-$(basename $@)
	rm -rf $(OUTDIR)/$(basename $@).release
	rm -rf $(OUTDIR)/$(basename $@).debug
	find $(OUTDIR) -regex '.*\(host\|target\)-$(basename $@)\.mk' -delete

native.clean:
	rm -f $(OUTDIR)/Makefile-native
	rm -rf $(OUTDIR)/native
	find $(OUTDIR) -regex '.*\(host\|target\)-native\.mk' -delete

android.clean:
	rm -f $(OUTDIR)/Makefile-android
	rm -rf $(OUTDIR)/android.release
	find $(OUTDIR) -regex '.*\(host\|target\)-android\.mk' -delete

clean: $(addsuffix .clean,$(ARCHES)) native.clean

# GYP file generation targets.
$(OUTDIR)/Makefile-ia32: $(GYPFILES) $(ENVFILE)
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/standalone.gypi --depth=. -Dtarget_arch=ia32 \
	              -S-ia32 $(GYPFLAGS)

$(OUTDIR)/Makefile-x64: $(GYPFILES) $(ENVFILE)
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/standalone.gypi --depth=. -Dtarget_arch=x64 \
	              -S-x64 $(GYPFLAGS)

$(OUTDIR)/Makefile-arm: $(GYPFILES) $(ENVFILE) build/armu.gypi
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/standalone.gypi --depth=. -Ibuild/armu.gypi \
	              -S-arm $(GYPFLAGS)

$(OUTDIR)/Makefile-mips: $(GYPFILES) $(ENVFILE) build/mipsu.gypi
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/standalone.gypi --depth=. -Ibuild/mipsu.gypi \
	              -S-mips $(GYPFLAGS)

$(OUTDIR)/Makefile-native: $(GYPFILES) $(ENVFILE)
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/standalone.gypi --depth=. -S-native $(GYPFLAGS)

$(OUTDIR)/Makefile-android: $(GYPFILES) $(ENVFILE) build/android.gypi \
                            must-set-ANDROID_NDK_ROOT
	CC="${ANDROID_TOOL_PREFIX}-gcc" \
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/standalone.gypi --depth=. -Ibuild/android.gypi \
	              -S-android $(GYPFLAGS)

must-set-ANDROID_NDK_ROOT:
ifndef ANDROID_NDK_ROOT
	  $(error ANDROID_NDK_ROOT is not set)
endif

# Replaces the old with the new environment file if they're different, which
# will trigger GYP to regenerate Makefiles.
$(ENVFILE): $(ENVFILE).new
	@if test -r $(ENVFILE) && cmp $(ENVFILE).new $(ENVFILE) >/dev/null; \
	    then rm $(ENVFILE).new; \
	    else mv $(ENVFILE).new $(ENVFILE); fi

# Stores current GYPFLAGS in a file.
$(ENVFILE).new:
	@mkdir -p $(OUTDIR); echo "GYPFLAGS=$(GYPFLAGS)" > $(ENVFILE).new;

# Dependencies.
dependencies:
	svn checkout --force http://gyp.googlecode.com/svn/trunk build/gyp \
	    --revision 1026
