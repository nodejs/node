// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/numbers/diy-fp.h"

#include <stdint.h>

namespace v8 {
namespace internal {

void DiyFp::Multiply(const DiyFp& other) {
  // Simply "emulates" a 128 bit multiplication.
  // However: the resulting number only contains 64 bits. The least
  // significant 64 bits are only used for rounding the most significant 64
  // bits.
  const uint64_t kM32 = 0xFFFFFFFFu;
  uint64_t a = f_ >> 32;
  uint64_t b = f_ & kM32;
  uint64_t c = other.f_ >> 32;
  uint64_t d = other.f_ & kM32;
  uint64_t ac = a * c;
  uint64_t bc = b * c;
  uint64_t ad = a * d;
  uint64_t bd = b * d;
  uint64_t tmp = (bd >> 32) + (ad & kM32) + (bc & kM32);
  // By adding 1U << 31 to tmp we round the final result.
  // Halfway cases will be round up.
  tmp += 1U << 31;
  uint64_t result_f = ac + (ad >> 32) + (bc >> 32) + (tmp >> 32);
  e_ += other.e_ + 64;
  f_ = result_f;
}

}  // namespace internal
}  // namespace v8
