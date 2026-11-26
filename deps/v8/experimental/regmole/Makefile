# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Makefile for the regmole Clang AST plugin (static register clobber analysis).

LLVM_SRC_INCLUDE:=$(LLVM_SRC_ROOT)/include
LLVM_BUILD_INCLUDE:=$(BUILD_ROOT)/include
CLANG_SRC_INCLUDE:=$(CLANG_SRC_ROOT)/include
CLANG_BUILD_INCLUDE:=$(BUILD_ROOT)/tools/clang/include

CXXFLAGS = -O3 -g3
all: libregmole.so
Release: libregmole.so

Debug: CXXFLAGS = -O1 -DDEBUG -g
Debug: libregmole.so

libregmole.so: regmole.cc
	$(CXX) -I$(LLVM_BUILD_INCLUDE) -I$(LLVM_SRC_INCLUDE)                  \
	-I$(CLANG_BUILD_INCLUDE) -I$(CLANG_SRC_INCLUDE) -I. ${CXXFLAGS}       \
	-D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS         \
	-D__STDC_LIMIT_MACROS -fomit-frame-pointer -fno-exceptions            \
	-fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual -fno-strict-aliasing \
	-pedantic -Wno-long-long -Wall -W -Wno-unused-parameter               \
	-Wwrite-strings -static-libstdc++ -std=c++20 -shared -o libregmole.so \
	regmole.cc

clean:
	$(RM) libregmole.so
