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

# This is Makefile for clang plugin part of gcmole tool. See README.

LLVM_SRC_INCLUDE:=$(LLVM_SRC_ROOT)/include
LLVM_BUILD_INCLUDE:=$(BUILD_ROOT)/include
CLANG_SRC_INCLUDE:=$(CLANG_SRC_ROOT)/include
CLANG_BUILD_INCLUDE:=$(BUILD_ROOT)/tools/clang/include

CXXFLAGS = -O3 -g3
all: libgcmole.so
Release: libgcmole.so

Debug: CXXFLAGS = -O1 -DDEBUG -g
Debug: libgcmole.so

libgcmole.so: gcmole.cc
	$(CXX) -I$(LLVM_BUILD_INCLUDE) -I$(LLVM_SRC_INCLUDE)                  \
	-I$(CLANG_BUILD_INCLUDE) -I$(CLANG_SRC_INCLUDE) -I. ${CXXFLAGS}          \
	-D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS         \
	-D__STDC_LIMIT_MACROS -fomit-frame-pointer -fno-exceptions        \
	-fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual -fno-strict-aliasing \
	-pedantic -Wno-long-long -Wall -W -Wno-unused-parameter               \
	-Wwrite-strings -static-libstdc++ -std=c++0x -shared -o libgcmole.so  \
	gcmole.cc

clean:
	$(RM) libgcmole.so
