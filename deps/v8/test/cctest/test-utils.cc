// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"

#include "platform.h"
#include "cctest.h"

using namespace v8::internal;


TEST(Utils1) {
  CHECK_EQ(-1000000, FastD2I(-1000000.0));
  CHECK_EQ(-1, FastD2I(-1.0));
  CHECK_EQ(0, FastD2I(0.0));
  CHECK_EQ(1, FastD2I(1.0));
  CHECK_EQ(1000000, FastD2I(1000000.0));

  CHECK_EQ(-1000000, FastD2I(-1000000.123));
  CHECK_EQ(-1, FastD2I(-1.234));
  CHECK_EQ(0, FastD2I(0.345));
  CHECK_EQ(1, FastD2I(1.234));
  CHECK_EQ(1000000, FastD2I(1000000.123));
  // Check that >> is implemented as arithmetic shift right.
  // If this is not true, then ArithmeticShiftRight() must be changed,
  // There are also documented right shifts in assembler.cc of
  // int8_t and intptr_t signed integers.
  CHECK_EQ(-2, -8 >> 2);
  CHECK_EQ(-2, static_cast<int8_t>(-8) >> 2);
  CHECK_EQ(-2, static_cast<int>(static_cast<intptr_t>(-8) >> 2));
}


TEST(SNPrintF) {
  // Make sure that strings that are truncated because of too small
  // buffers are zero-terminated anyway.
  const char* s = "the quick lazy .... oh forget it!";
  int length = StrLength(s);
  for (int i = 1; i < length * 2; i++) {
    static const char kMarker = static_cast<char>(42);
    Vector<char> buffer = Vector<char>::New(i + 1);
    buffer[i] = kMarker;
    int n = OS::SNPrintF(Vector<char>(buffer.start(), i), "%s", s);
    CHECK(n <= i);
    CHECK(n == length || n == -1);
    CHECK_EQ(0, strncmp(buffer.start(), s, i - 1));
    CHECK_EQ(kMarker, buffer[i]);
    if (i <= length) {
      CHECK_EQ(i - 1, StrLength(buffer.start()));
    } else {
      CHECK_EQ(length, StrLength(buffer.start()));
    }
    buffer.Dispose();
  }
}


void TestMemCopy(Vector<byte> src,
                 Vector<byte> dst,
                 int source_alignment,
                 int destination_alignment,
                 int length_alignment) {
  memset(dst.start(), 0xFF, dst.length());
  byte* to = dst.start() + 32 + destination_alignment;
  byte* from = src.start() + source_alignment;
  int length = kMinComplexMemCopy + length_alignment;
  MemCopy(to, from, static_cast<size_t>(length));
  printf("[%d,%d,%d]\n",
         source_alignment, destination_alignment, length_alignment);
  for (int i = 0; i < length; i++) {
    CHECK_EQ(from[i], to[i]);
  }
  CHECK_EQ(0xFF, to[-1]);
  CHECK_EQ(0xFF, to[length]);
}



TEST(MemCopy) {
  const int N = kMinComplexMemCopy + 128;
  Vector<byte> buffer1 = Vector<byte>::New(N);
  Vector<byte> buffer2 = Vector<byte>::New(N);

  for (int i = 0; i < N; i++) {
    buffer1[i] = static_cast<byte>(i & 0x7F);
  }

  // Same alignment.
  for (int i = 0; i < 32; i++) {
    TestMemCopy(buffer1, buffer2, i, i, i * 2);
  }

  // Different alignment.
  for (int i = 0; i < 32; i++) {
    for (int j = 1; j < 32; j++) {
      TestMemCopy(buffer1, buffer2, i, (i + j) & 0x1F , 0);
    }
  }

  // Different lengths
  for (int i = 0; i < 32; i++) {
    TestMemCopy(buffer1, buffer2, 3, 7, i);
  }

  buffer2.Dispose();
  buffer1.Dispose();
}
