// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_VECTOR_ARITHMETIC_INL_H_
#define V8_BIGINT_VECTOR_ARITHMETIC_INL_H_

// Helper functions that operate on {Digits} vectors of digits.

#include "src/bigint/bigint-inl.h"
#include "src/bigint/util.h"

namespace v8 {
namespace bigint {

// Z += X. Returns carry on overflow.
inline digit_t AddAndReturnOverflow(RWDigits Z, Digits X) {
  X.Normalize();
  if (X.len() == 0) return 0;
  // Here and below: callers are careful to pass sufficiently large result
  // storage Z. If that ever goes wrong, then something is corrupted; could
  // be a concurrent-mutation attack. So we harden against that with Release-
  // mode CHECKs.
  CHECK(Z.len() >= X.len());
  digit_t carry = 0;
  uint32_t i = 0;
  for (; i < X.len(); i++) {
    Z[i] = digit_add3(Z[i], X[i], carry, &carry);
  }
  for (; i < Z.len() && carry != 0; i++) {
    Z[i] = digit_add2(Z[i], carry, &carry);
  }
  return carry;
}

// Z -= X. Returns borrow on overflow.
inline digit_t SubAndReturnBorrow(RWDigits Z, Digits X) {
  X.Normalize();
  if (X.len() == 0) return 0;
  CHECK(Z.len() >= X.len());
  digit_t borrow = 0;
  uint32_t i = 0;
  for (; i < X.len(); i++) {
    Z[i] = digit_sub2(Z[i], X[i], borrow, &borrow);
  }
  for (; i < Z.len() && borrow != 0; i++) {
    Z[i] = digit_sub(Z[i], borrow, &borrow);
  }
  return borrow;
}

// X -= y.
inline void Subtract(RWDigits X, digit_t y) {
  digit_t borrow = y;
  uint32_t i = 0;
  do {
    X[i] = digit_sub(X[i], borrow, &borrow);
    i++;
  } while (borrow != 0);
}

inline bool IsBitNormalized(Digits X) {
  return (X.msd() >> (kDigitBits - 1)) == 1;
}

inline uint32_t BitLength(Digits X) {
  return X.len() * kDigitBits - CountLeadingZeros(X.msd());
}

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_VECTOR_ARITHMETIC_INL_H_
