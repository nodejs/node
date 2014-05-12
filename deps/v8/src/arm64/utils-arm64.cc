// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "arm64/utils-arm64.h"


namespace v8 {
namespace internal {

#define __ assm->


int CountLeadingZeros(uint64_t value, int width) {
  // TODO(jbramley): Optimize this for ARM64 hosts.
  ASSERT((width == 32) || (width == 64));
  int count = 0;
  uint64_t bit_test = 1UL << (width - 1);
  while ((count < width) && ((bit_test & value) == 0)) {
    count++;
    bit_test >>= 1;
  }
  return count;
}


int CountLeadingSignBits(int64_t value, int width) {
  // TODO(jbramley): Optimize this for ARM64 hosts.
  ASSERT((width == 32) || (width == 64));
  if (value >= 0) {
    return CountLeadingZeros(value, width) - 1;
  } else {
    return CountLeadingZeros(~value, width) - 1;
  }
}


int CountTrailingZeros(uint64_t value, int width) {
  // TODO(jbramley): Optimize this for ARM64 hosts.
  ASSERT((width == 32) || (width == 64));
  int count = 0;
  while ((count < width) && (((value >> count) & 1) == 0)) {
    count++;
  }
  return count;
}


int CountSetBits(uint64_t value, int width) {
  // TODO(jbramley): Would it be useful to allow other widths? The
  // implementation already supports them.
  ASSERT((width == 32) || (width == 64));

  // Mask out unused bits to ensure that they are not counted.
  value &= (0xffffffffffffffffUL >> (64-width));

  // Add up the set bits.
  // The algorithm works by adding pairs of bit fields together iteratively,
  // where the size of each bit field doubles each time.
  // An example for an 8-bit value:
  // Bits:  h  g  f  e  d  c  b  a
  //         \ |   \ |   \ |   \ |
  // value = h+g   f+e   d+c   b+a
  //            \    |      \    |
  // value =   h+g+f+e     d+c+b+a
  //                  \          |
  // value =       h+g+f+e+d+c+b+a
  value = ((value >> 1) & 0x5555555555555555) + (value & 0x5555555555555555);
  value = ((value >> 2) & 0x3333333333333333) + (value & 0x3333333333333333);
  value = ((value >> 4) & 0x0f0f0f0f0f0f0f0f) + (value & 0x0f0f0f0f0f0f0f0f);
  value = ((value >> 8) & 0x00ff00ff00ff00ff) + (value & 0x00ff00ff00ff00ff);
  value = ((value >> 16) & 0x0000ffff0000ffff) + (value & 0x0000ffff0000ffff);
  value = ((value >> 32) & 0x00000000ffffffff) + (value & 0x00000000ffffffff);

  return value;
}


int MaskToBit(uint64_t mask) {
  ASSERT(CountSetBits(mask, 64) == 1);
  return CountTrailingZeros(mask, 64);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
