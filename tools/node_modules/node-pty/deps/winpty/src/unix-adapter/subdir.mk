# Copyright (c) 2011-2015 Ryan Prichard
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

ALL_TARGETS += build/$(UNIX_ADAPTER_EXE)

$(eval $(call def_unix_target,unix-adapter,))

UNIX_ADAPTER_OBJECTS = \
	build/unix-adapter/unix-adapter/InputHandler.o \
	build/unix-adapter/unix-adapter/OutputHandler.o \
	build/unix-adapter/unix-adapter/Util.o \
	build/unix-adapter/unix-adapter/WakeupFd.o \
	build/unix-adapter/unix-adapter/main.o \
	build/unix-adapter/shared/DebugClient.o \
	build/unix-adapter/shared/WinptyAssert.o \
	build/unix-adapter/shared/WinptyVersion.o

build/unix-adapter/shared/WinptyVersion.o : build/gen/GenVersion.h

build/$(UNIX_ADAPTER_EXE) : $(UNIX_ADAPTER_OBJECTS) build/winpty.dll
	$(info Linking $@)
	@$(UNIX_CXX) $(UNIX_LDFLAGS) -o $@ $^

-include $(UNIX_ADAPTER_OBJECTS:.o=.d)
