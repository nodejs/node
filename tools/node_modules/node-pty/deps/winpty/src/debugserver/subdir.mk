# Copyright (c) 2015 Ryan Prichard
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

ALL_TARGETS += build/winpty-debugserver.exe

$(eval $(call def_mingw_target,debugserver,))

DEBUGSERVER_OBJECTS = \
	build/debugserver/debugserver/DebugServer.o \
	build/debugserver/shared/DebugClient.o \
	build/debugserver/shared/OwnedHandle.o \
	build/debugserver/shared/StringUtil.o \
	build/debugserver/shared/WindowsSecurity.o \
	build/debugserver/shared/WindowsVersion.o \
	build/debugserver/shared/WinptyAssert.o \
	build/debugserver/shared/WinptyException.o

build/debugserver/shared/WindowsVersion.o : build/gen/GenVersion.h

build/winpty-debugserver.exe : $(DEBUGSERVER_OBJECTS)
	$(info Linking $@)
	@$(MINGW_CXX) $(MINGW_LDFLAGS) -o $@ $^

-include $(DEBUGSERVER_OBJECTS:.o=.d)
