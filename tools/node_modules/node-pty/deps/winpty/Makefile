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

# Use make -n to see the actual command-lines make would run.

# The default "make install" prefix is /usr/local.  Pass PREFIX=<path> on the
# command-line to override the default.

.SECONDEXPANSION :

.PHONY : default
default : all

PREFIX := /usr/local
UNIX_ADAPTER_EXE := winpty.exe
MINGW_ENABLE_CXX11_FLAG := -std=c++11
USE_PCH := 1

COMMON_CXXFLAGS :=
UNIX_CXXFLAGS :=
MINGW_CXXFLAGS :=
MINGW_LDFLAGS :=
UNIX_LDFLAGS :=

# Include config.mk but complain if it hasn't been created yet.
ifeq "$(wildcard config.mk)" ""
    $(error config.mk does not exist.  Please run ./configure)
endif
include config.mk

COMMON_CXXFLAGS += \
	-MMD -Wall \
	-DUNICODE \
	-D_UNICODE \
	-D_WIN32_WINNT=0x0501 \
	-Ibuild/gen

UNIX_CXXFLAGS += \
	$(COMMON_CXXFLAGS)

MINGW_CXXFLAGS += \
	$(COMMON_CXXFLAGS) \
	-O2 \
	$(MINGW_ENABLE_CXX11_FLAG)

MINGW_LDFLAGS += -static -static-libgcc -static-libstdc++
UNIX_LDFLAGS += $(UNIX_LDFLAGS_STATIC)

ifeq "$(USE_PCH)" "1"
MINGW_CXXFLAGS += -include build/mingw/PrecompiledHeader.h
PCH_DEP := build/mingw/PrecompiledHeader.h.gch
else
PCH_DEP :=
endif

build/gen/GenVersion.h : VERSION.txt $(COMMIT_HASH_DEP) | $$(@D)/.mkdir
	$(info Updating build/gen/GenVersion.h)
	@echo "const char GenVersion_Version[] = \"$(shell cat VERSION.txt | tr -d '\r\n')\";" > build/gen/GenVersion.h
	@echo "const char GenVersion_Commit[] = \"$(COMMIT_HASH)\";" >> build/gen/GenVersion.h

build/mingw/PrecompiledHeader.h : src/shared/PrecompiledHeader.h | $$(@D)/.mkdir
	$(info Copying $< to $@)
	@cp $< $@

build/mingw/PrecompiledHeader.h.gch : build/mingw/PrecompiledHeader.h | $$(@D)/.mkdir
	$(info Compiling $<)
	@$(MINGW_CXX) $(MINGW_CXXFLAGS) -c -o $@ $<

-include build/mingw/PrecompiledHeader.h.d

define def_unix_target
build/$1/%.o : src/%.cc | $$$$(@D)/.mkdir
	$$(info Compiling $$<)
	@$$(UNIX_CXX) $$(UNIX_CXXFLAGS) $2 -I src/include -c -o $$@ $$<
endef

define def_mingw_target
build/$1/%.o : src/%.cc $$(PCH_DEP) | $$$$(@D)/.mkdir
	$$(info Compiling $$<)
	@$$(MINGW_CXX) $$(MINGW_CXXFLAGS) $2 -I src/include -c -o $$@ $$<
endef

include src/subdir.mk

.PHONY : all
all : $(ALL_TARGETS)

.PHONY : tests
tests : $(TEST_PROGRAMS)

.PHONY : install-bin
install-bin : all
	mkdir -p $(PREFIX)/bin
	install -m 755 -p -s build/$(UNIX_ADAPTER_EXE) $(PREFIX)/bin
	install -m 755 -p -s build/winpty.dll $(PREFIX)/bin
	install -m 755 -p -s build/winpty-agent.exe $(PREFIX)/bin

.PHONY : install-debugserver
install-debugserver : all
	mkdir -p $(PREFIX)/bin
	install -m 755 -p -s build/winpty-debugserver.exe $(PREFIX)/bin

.PHONY : install-lib
install-lib : all
	mkdir -p $(PREFIX)/lib
	install -m 644 -p build/winpty.lib $(PREFIX)/lib

.PHONY : install-doc
install-doc :
	mkdir -p $(PREFIX)/share/doc/winpty
	install -m 644 -p LICENSE $(PREFIX)/share/doc/winpty
	install -m 644 -p README.md $(PREFIX)/share/doc/winpty
	install -m 644 -p RELEASES.md $(PREFIX)/share/doc/winpty

.PHONY : install-include
install-include :
	mkdir -p $(PREFIX)/include/winpty
	install -m 644 -p src/include/winpty.h $(PREFIX)/include/winpty
	install -m 644 -p src/include/winpty_constants.h $(PREFIX)/include/winpty

.PHONY : install
install : \
	install-bin \
	install-debugserver \
	install-lib \
	install-doc \
	install-include

.PHONY : clean
clean :
	rm -fr build

.PHONY : clean-msvc
clean-msvc :
	rm -fr src/Default src/Release src/.vs src/gen
	rm -f src/*.vcxproj src/*.vcxproj.filters src/*.sln src/*.sdf

.PHONY : distclean
distclean : clean
	rm -f config.mk

.PRECIOUS : %.mkdir
%.mkdir :
	$(info Creating directory $(dir $@))
	@mkdir -p $(dir $@)
	@touch $@

src/%.h :
	@echo "Missing header file $@ (stale dependency file?)"
