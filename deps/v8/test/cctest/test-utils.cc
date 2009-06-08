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


enum Mode {
  forward,
  backward_unsigned
};


static v8::internal::byte* Write(v8::internal::byte* p, Mode m, int x) {
  v8::internal::byte* q = NULL;
  switch (m) {
    case forward:
      q = EncodeInt(p, x);
      CHECK(q <= p + sizeof(x) + 1);
      break;
    case backward_unsigned:
      q = EncodeUnsignedIntBackward(p, x);
      CHECK(q >= p - sizeof(x) - 1);
      break;
  }
  return q;
}


static v8::internal::byte* Read(v8::internal::byte* p, Mode m, int x) {
  v8::internal::byte* q = NULL;
  int y;
  switch (m) {
    case forward:
      q = DecodeInt(p, &y);
      CHECK(q <= p + sizeof(y) + 1);
      break;
    case backward_unsigned: {
      unsigned int uy;
      q = DecodeUnsignedIntBackward(p, &uy);
      y = uy;
      CHECK(q >= p - sizeof(uy) - 1);
      break;
    }
  }
  CHECK(y == x);
  return q;
}


static v8::internal::byte* WriteMany(v8::internal::byte* p, Mode m, int x) {
  p = Write(p, m, x - 7);
  p = Write(p, m, x - 1);
  p = Write(p, m, x);
  p = Write(p, m, x + 1);
  p = Write(p, m, x + 2);
  p = Write(p, m, -x - 5);
  p = Write(p, m, -x - 1);
  p = Write(p, m, -x);
  p = Write(p, m, -x + 1);
  p = Write(p, m, -x + 3);

  return p;
}


static v8::internal::byte* ReadMany(v8::internal::byte* p, Mode m, int x) {
  p = Read(p, m, x - 7);
  p = Read(p, m, x - 1);
  p = Read(p, m, x);
  p = Read(p, m, x + 1);
  p = Read(p, m, x + 2);
  p = Read(p, m, -x - 5);
  p = Read(p, m, -x - 1);
  p = Read(p, m, -x);
  p = Read(p, m, -x + 1);
  p = Read(p, m, -x + 3);

  return p;
}


void ProcessValues(int* values, int n, Mode m) {
  v8::internal::byte buf[4 * KB];  // make this big enough
  v8::internal::byte* p0 = (m == forward ? buf : buf + ARRAY_SIZE(buf));

  v8::internal::byte* p = p0;
  for (int i = 0; i < n; i++) {
    p = WriteMany(p, m, values[i]);
  }

  v8::internal::byte* q = p0;
  for (int i = 0; i < n; i++) {
    q = ReadMany(q, m, values[i]);
  }

  CHECK(p == q);
}


TEST(Utils0) {
  int values[] = {
    0, 1, 10, 16, 32, 64, 128, 256, 512, 1024, 1234, 5731,
    10000, 100000, 1000000, 10000000, 100000000, 1000000000
  };
  const int n = ARRAY_SIZE(values);

  ProcessValues(values, n, forward);
  ProcessValues(values, n, backward_unsigned);
}


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
  CHECK_EQ(-2, static_cast<intptr_t>(-8) >> 2);
}


TEST(SNPrintF) {
  // Make sure that strings that are truncated because of too small
  // buffers are zero-terminated anyway.
  const char* s = "the quick lazy .... oh forget it!";
  int length = strlen(s);
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
      CHECK_EQ(i - 1, strlen(buffer.start()));
    } else {
      CHECK_EQ(length, strlen(buffer.start()));
    }
    buffer.Dispose();
  }
}
