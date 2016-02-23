# Copyright Joyent, Inc. and other Node contributors. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

SRCDIR ?= $(CURDIR)

ifeq (,$(builddir_name))

VPATH := $(SRCDIR)
include $(SRCDIR)/build.mk

else  # Out of tree build.

# Drop all built-in rules.
.SUFFIXES:

.PHONY:	$(builddir_name)
$(builddir_name): $(builddir_name)/.buildstamp
	$(MAKE) -C $@ -f $(CURDIR)/Makefile $(MAKECMDGOALS) \
		SRCDIR=$(CURDIR) builddir_name=

$(builddir_name)/.buildstamp:
	mkdir -p $(dir $@)
	touch $@

# Add no-op rules for Makefiles to stop make from trying to rebuild them.
Makefile:: ;
%.mk:: ;

# Turn everything else into a no-op rule that depends on the build directory.
%:: $(builddir_name) ;

.PHONY: clean
clean:
	$(RM) -fr $(builddir_name)

endif
