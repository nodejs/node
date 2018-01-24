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

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/diy-fp.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

TEST(Subtract) {
  DiyFp diy_fp1 = DiyFp(3, 0);
  DiyFp diy_fp2 = DiyFp(1, 0);
  DiyFp diff = DiyFp::Minus(diy_fp1, diy_fp2);

  CHECK_EQ(2, diff.f());
  CHECK_EQ(0, diff.e());
  diy_fp1.Subtract(diy_fp2);
  CHECK_EQ(2, diy_fp1.f());
  CHECK_EQ(0, diy_fp1.e());
}


TEST(Multiply) {
  DiyFp diy_fp1 = DiyFp(3, 0);
  DiyFp diy_fp2 = DiyFp(2, 0);
  DiyFp product = DiyFp::Times(diy_fp1, diy_fp2);

  CHECK_EQ(0, product.f());
  CHECK_EQ(64, product.e());
  diy_fp1.Multiply(diy_fp2);
  CHECK_EQ(0, diy_fp1.f());
  CHECK_EQ(64, diy_fp1.e());

  diy_fp1 = DiyFp(V8_2PART_UINT64_C(0x80000000, 00000000), 11);
  diy_fp2 = DiyFp(2, 13);
  product = DiyFp::Times(diy_fp1, diy_fp2);
  CHECK_EQ(1, product.f());
  CHECK_EQ(11 + 13 + 64, product.e());

  // Test rounding.
  diy_fp1 = DiyFp(V8_2PART_UINT64_C(0x80000000, 00000001), 11);
  diy_fp2 = DiyFp(1, 13);
  product = DiyFp::Times(diy_fp1, diy_fp2);
  CHECK_EQ(1, product.f());
  CHECK_EQ(11 + 13 + 64, product.e());

  diy_fp1 = DiyFp(V8_2PART_UINT64_C(0x7fffffff, ffffffff), 11);
  diy_fp2 = DiyFp(1, 13);
  product = DiyFp::Times(diy_fp1, diy_fp2);
  CHECK_EQ(0, product.f());
  CHECK_EQ(11 + 13 + 64, product.e());

  // Halfway cases are allowed to round either way. So don't check for it.

  // Big numbers.
  diy_fp1 = DiyFp(V8_2PART_UINT64_C(0xFFFFFFFF, FFFFFFFF), 11);
  diy_fp2 = DiyFp(V8_2PART_UINT64_C(0xFFFFFFFF, FFFFFFFF), 13);
  // 128bit result: 0xfffffffffffffffe0000000000000001
  product = DiyFp::Times(diy_fp1, diy_fp2);
  CHECK(V8_2PART_UINT64_C(0xFFFFFFFF, FFFFFFFe) == product.f());
  CHECK_EQ(11 + 13 + 64, product.e());
}

}  // namespace internal
}  // namespace v8
