// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <limits>

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/code-stubs.h"
#include "src/double.h"
#include "src/heap/factory.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-code-stubs.h"

namespace v8 {
namespace internal {

int STDCALL ConvertDToICVersion(double d) {
#if defined(V8_TARGET_BIG_ENDIAN)
  const int kExponentIndex = 0;
  const int kMantissaIndex = 1;
#elif defined(V8_TARGET_LITTLE_ENDIAN)
  const int kExponentIndex = 1;
  const int kMantissaIndex = 0;
#else
#error Unsupported endianness
#endif
  union { double d; uint32_t u[2]; } dbl;
  dbl.d = d;
  uint32_t exponent_bits = dbl.u[kExponentIndex];
  int32_t shifted_mask = static_cast<int32_t>(Double::kExponentMask >> 32);
  int32_t exponent = (((exponent_bits & shifted_mask) >>
                       (Double::kPhysicalSignificandSize - 32)) -
                      HeapNumber::kExponentBias);
  if (exponent < 0) {
    return 0;
  }
  uint32_t unsigned_exponent = static_cast<uint32_t>(exponent);
  int result = 0;
  uint32_t max_exponent =
    static_cast<uint32_t>(Double::kPhysicalSignificandSize);
  if (unsigned_exponent >= max_exponent) {
    if ((exponent - Double::kPhysicalSignificandSize) < 32) {
      result = dbl.u[kMantissaIndex]
               << (exponent - Double::kPhysicalSignificandSize);
    }
  } else {
    uint64_t big_result =
        (bit_cast<uint64_t>(d) & Double::kSignificandMask) | Double::kHiddenBit;
    big_result = big_result >> (Double::kPhysicalSignificandSize - exponent);
    result = static_cast<uint32_t>(big_result);
  }
  if (static_cast<int32_t>(exponent_bits) < 0) {
    return (0 - result);
  } else {
    return result;
  }
}


void RunOneTruncationTestWithTest(ConvertDToICallWrapper callWrapper,
                                  ConvertDToIFunc func,
                                  double from,
                                  int32_t to) {
  int32_t result = (*callWrapper)(func, from);
  CHECK_EQ(to, result);
}


int32_t DefaultCallWrapper(ConvertDToIFunc func,
                           double from) {
  return (*func)(from);
}


// #define NaN and Infinity so that it's possible to cut-and-paste these tests
// directly to a .js file and run them.
#define NaN (std::numeric_limits<double>::quiet_NaN())
#define Infinity (std::numeric_limits<double>::infinity())
#define RunOneTruncationTest(p1, p2) \
    RunOneTruncationTestWithTest(callWrapper, func, p1, p2)


void RunAllTruncationTests(ConvertDToIFunc func) {
  RunAllTruncationTests(DefaultCallWrapper, func);
}


void RunAllTruncationTests(ConvertDToICallWrapper callWrapper,
                           ConvertDToIFunc func) {
  RunOneTruncationTest(0, 0);
  RunOneTruncationTest(0.5, 0);
  RunOneTruncationTest(-0.5, 0);
  RunOneTruncationTest(1.5, 1);
  RunOneTruncationTest(-1.5, -1);
  RunOneTruncationTest(5.5, 5);
  RunOneTruncationTest(-5.0, -5);
  RunOneTruncationTest(NaN, 0);
  RunOneTruncationTest(Infinity, 0);
  RunOneTruncationTest(-NaN, 0);
  RunOneTruncationTest(-Infinity, 0);
  RunOneTruncationTest(4.94065645841e-324, 0);
  RunOneTruncationTest(-4.94065645841e-324, 0);

  RunOneTruncationTest(0.9999999999999999, 0);
  RunOneTruncationTest(-0.9999999999999999, 0);
  RunOneTruncationTest(4294967296.0, 0);
  RunOneTruncationTest(-4294967296.0, 0);
  RunOneTruncationTest(9223372036854775000.0, -1024);
  RunOneTruncationTest(-9223372036854775000.0, 1024);
  RunOneTruncationTest(4.5036e+15, 372629504);
  RunOneTruncationTest(-4.5036e+15, -372629504);

  RunOneTruncationTest(287524199.5377777, 0x11234567);
  RunOneTruncationTest(-287524199.5377777, -0x11234567);
  RunOneTruncationTest(2300193596.302222, -1994773700);
  RunOneTruncationTest(-2300193596.302222, 1994773700);
  RunOneTruncationTest(4600387192.604444, 305419896);
  RunOneTruncationTest(-4600387192.604444, -305419896);
  RunOneTruncationTest(4823855600872397.0, 1737075661);
  RunOneTruncationTest(-4823855600872397.0, -1737075661);

  RunOneTruncationTest(4503603922337791.0, -1);
  RunOneTruncationTest(-4503603922337791.0, 1);
  RunOneTruncationTest(4503601774854143.0, 2147483647);
  RunOneTruncationTest(-4503601774854143.0, -2147483647);
  RunOneTruncationTest(9007207844675582.0, -2);
  RunOneTruncationTest(-9007207844675582.0, 2);

  RunOneTruncationTest(2.4178527921507624e+24, -536870912);
  RunOneTruncationTest(-2.4178527921507624e+24, 536870912);
  RunOneTruncationTest(2.417853945072267e+24, -536870912);
  RunOneTruncationTest(-2.417853945072267e+24, 536870912);

  RunOneTruncationTest(4.8357055843015248e+24, -1073741824);
  RunOneTruncationTest(-4.8357055843015248e+24, 1073741824);
  RunOneTruncationTest(4.8357078901445341e+24, -1073741824);
  RunOneTruncationTest(-4.8357078901445341e+24, 1073741824);

  RunOneTruncationTest(2147483647.0, 2147483647);
  RunOneTruncationTest(-2147483648.0, -2147483647-1);
  RunOneTruncationTest(9.6714111686030497e+24, -2147483647-1);
  RunOneTruncationTest(-9.6714111686030497e+24, -2147483647-1);
  RunOneTruncationTest(9.6714157802890681e+24, -2147483647-1);
  RunOneTruncationTest(-9.6714157802890681e+24, -2147483647-1);
  RunOneTruncationTest(1.9342813113834065e+25, -2147483647-1);
  RunOneTruncationTest(-1.9342813113834065e+25, -2147483647-1);

  RunOneTruncationTest(3.868562622766813e+25, 0);
  RunOneTruncationTest(-3.868562622766813e+25, 0);
  RunOneTruncationTest(1.7976931348623157e+308, 0);
  RunOneTruncationTest(-1.7976931348623157e+308, 0);
}

#undef NaN
#undef Infinity
#undef RunOneTruncationTest


TEST(CodeStubMajorKeys) {
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();

#define CHECK_STUB(NAME)                        \
  {                                             \
    HandleScope scope(isolate);                 \
    NAME##Stub stub_impl(0xABCD, isolate);      \
    CodeStub* stub = &stub_impl;                \
    CHECK_EQ(stub->MajorKey(), CodeStub::NAME); \
  }
  CODE_STUB_LIST(CHECK_STUB);
}

}  // namespace internal
}  // namespace v8
