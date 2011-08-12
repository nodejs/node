# Copyright 2011 the V8 project authors. All rights reserved.
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
CXX ?= "g++"  # For distcc: export CXX="distcc g++"
LINK ?= "g++"
OUTDIR ?= out
TESTJOBS ?= -j16
GYPFLAGS ?= -Dv8_can_use_vfp_instructions=true

# Architectures and modes to be compiled.
ARCHES = ia32 x64 arm
MODES = release debug

# List of files that trigger Makefile regeneration:
GYPFILES = build/all.gyp build/common.gypi build/v8-features.gypi \
           preparser/preparser.gyp samples/samples.gyp src/d8.gyp \
           test/cctest/cctest.gyp tools/gyp/v8.gyp

# Generates all combinations of ARCHES and MODES, e.g. "ia32.release".
BUILDS = $(foreach mode,$(MODES),$(addsuffix .$(mode),$(ARCHES)))
CHECKS = $(addsuffix .check,$(BUILDS))
# Generates corresponding test targets, e.g. "ia32.release.check".

.PHONY: all release debug ia32 x64 arm $(BUILDS)

# Target definitions. "all" is the default, you can specify any others on the
# command line, e.g. "make ia32". Targets defined in $(BUILDS), e.g.
# "ia32.debug", can also be specified.
all: release debug

release: $(addsuffix .release,$(ARCHES))

debug: $(addsuffix .debug,$(ARCHES))

ia32: $(addprefix ia32.,$(MODES))

x64: $(addprefix x64.,$(MODES))

arm: $(addprefix arm.,$(MODES))

.SECONDEXPANSION:
$(BUILDS): $(OUTDIR)/Makefile-$$(basename $$@)
	@$(MAKE) -C "$(OUTDIR)" -f Makefile-$(basename $@) \
	         CXX="$(CXX)" LINK="$(LINK)" \
	         BUILDTYPE=$(shell echo $(subst .,,$(suffix $@)) | \
	                     python -c "print raw_input().capitalize()") \
	         builddir="$(shell pwd)/$(OUTDIR)/$@"

# Test targets.
check: all
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR)

$(addsuffix .check,$(MODES)): $$(basename $$@)
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) --mode=$(basename $@)

$(addsuffix .check,$(ARCHES)): $$(basename $$@)
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) --arch=$(basename $@)

$(CHECKS): $$(basename $$@)
	@tools/test-wrapper-gypbuild.py $(TESTJOBS) --outdir=$(OUTDIR) --arch-and-mode=$(basename $@)

# Clean targets. You can clean each architecture individually, or everything.
$(addsuffix .clean,$(ARCHES)):
	rm -f $(OUTDIR)/Makefile-$(basename $@)
	rm -rf $(OUTDIR)/$(basename $@).release
	rm -rf $(OUTDIR)/$(basename $@).debug

clean: $(addsuffix .clean,$(ARCHES))

# GYP file generation targets.
$(OUTDIR)/Makefile-ia32: $(GYPFILES)
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/common.gypi --depth=. -Dtarget_arch=ia32 -S-ia32 \
	              $(GYPFLAGS)

$(OUTDIR)/Makefile-x64: $(GYPFILES)
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/common.gypi --depth=. -Dtarget_arch=x64 -S-x64 \
	              $(GYPFLAGS)

$(OUTDIR)/Makefile-arm: $(GYPFILES) build/armu.gypi
	build/gyp/gyp --generator-output="$(OUTDIR)" build/all.gyp \
	              -Ibuild/common.gypi --depth=. -Ibuild/armu.gypi -S-arm \
	              $(GYPFLAGS)
