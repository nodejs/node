// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/div-helpers.h"

#include "src/bigint/bigint-internal.h"

namespace v8 {
namespace bigint {

namespace {

void Copy(RWDigits Z, Digits X) {
  if (Z == X) return;
  int i = 0;
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

}  // namespace

// Z := X << shift
// Z and X may alias for an in-place shift.
void LeftShift(RWDigits Z, Digits X, int shift) {
  DCHECK(shift >= 0);
  DCHECK(shift < kDigitBits);
  DCHECK(Z.len() >= X.len());
  if (shift == 0) return Copy(Z, X);
  digit_t carry = 0;
  int i = 0;
  for (; i < X.len(); i++) {
    digit_t d = X[i];
    Z[i] = (d << shift) | carry;
    carry = d >> (kDigitBits - shift);
  }
  if (i < Z.len()) {
    Z[i++] = carry;
  } else {
    DCHECK(carry == 0);
  }
  for (; i < Z.len(); i++) Z[i] = 0;
}

// Z := X >> shift
// Z and X may alias for an in-place shift.
void RightShift(RWDigits Z, Digits X, int shift) {
  DCHECK(shift >= 0);
  DCHECK(shift < kDigitBits);
  X.Normalize();
  DCHECK(Z.len() >= X.len());
  if (shift == 0) return Copy(Z, X);
  int i = 0;
  if (X.len() > 0) {
    digit_t carry = X[0] >> shift;
    int last = X.len() - 1;
    for (; i < last; i++) {
      digit_t d = X[i + 1];
      Z[i] = (d << (kDigitBits - shift)) | carry;
      carry = d >> shift;
    }
    Z[i++] = carry;
  }
  for (; i < Z.len(); i++) Z[i] = 0;
}

}  // namespace bigint
}  // namespace v8
