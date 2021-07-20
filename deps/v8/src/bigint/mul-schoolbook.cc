// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bigint/bigint-internal.h"
#include "src/bigint/digit-arithmetic.h"
#include "src/bigint/vector-arithmetic.h"

namespace v8 {
namespace bigint {

// Z := X * y, where y is a single digit.
void ProcessorImpl::MultiplySingle(RWDigits Z, Digits X, digit_t y) {
  DCHECK(y != 0);  // NOLINT(readability/check)
  digit_t carry = 0;
  digit_t high = 0;
  for (int i = 0; i < X.len(); i++) {
    digit_t new_high;
    digit_t low = digit_mul(X[i], y, &new_high);
    Z[i] = digit_add3(low, high, carry, &carry);
    high = new_high;
  }
  AddWorkEstimate(X.len());
  Z[X.len()] = carry + high;
  for (int i = X.len() + 1; i < Z.len(); i++) Z[i] = 0;
}

#define BODY(min, max)                              \
  for (int j = min; j <= max; j++) {                \
    digit_t high;                                   \
    digit_t low = digit_mul(X[j], Y[i - j], &high); \
    digit_t carrybit;                               \
    zi = digit_add2(zi, low, &carrybit);            \
    carry += carrybit;                              \
    next = digit_add2(next, high, &carrybit);       \
    next_carry += carrybit;                         \
  }                                                 \
  Z[i] = zi

// Z := X * Y.
// O(n²) "schoolbook" multiplication algorithm. Optimized to minimize
// bounds and overflow checks: rather than looping over X for every digit
// of Y (or vice versa), we loop over Z. The {BODY} macro above is what
// computes one of Z's digits as a sum of the products of relevant digits
// of X and Y. This yields a nearly 2x improvement compared to more obvious
// implementations.
// This method is *highly* performance sensitive even for the advanced
// algorithms, which use this as the base case of their recursive calls.
void ProcessorImpl::MultiplySchoolbook(RWDigits Z, Digits X, Digits Y) {
  DCHECK(IsDigitNormalized(X));
  DCHECK(IsDigitNormalized(Y));
  DCHECK(X.len() >= Y.len());
  DCHECK(Z.len() >= X.len() + Y.len());
  if (X.len() == 0 || Y.len() == 0) return Z.Clear();
  digit_t next, next_carry = 0, carry = 0;
  // Unrolled first iteration: it's trivial.
  Z[0] = digit_mul(X[0], Y[0], &next);
  int i = 1;
  // Unrolled second iteration: a little less setup.
  if (i < Y.len()) {
    digit_t zi = next;
    next = 0;
    BODY(0, 1);
    i++;
  }
  // Main part: since X.len() >= Y.len() > i, no bounds checks are needed.
  for (; i < Y.len(); i++) {
    digit_t zi = digit_add2(next, carry, &carry);
    next = next_carry + carry;
    carry = 0;
    next_carry = 0;
    BODY(0, i);
    AddWorkEstimate(i);
    if (should_terminate()) return;
  }
  // Last part: i exceeds Y now, we have to be careful about bounds.
  int loop_end = X.len() + Y.len() - 2;
  for (; i <= loop_end; i++) {
    int max_x_index = std::min(i, X.len() - 1);
    int max_y_index = Y.len() - 1;
    int min_x_index = i - max_y_index;
    digit_t zi = digit_add2(next, carry, &carry);
    next = next_carry + carry;
    carry = 0;
    next_carry = 0;
    BODY(min_x_index, max_x_index);
    AddWorkEstimate(max_x_index - min_x_index);
    if (should_terminate()) return;
  }
  // Write the last digit, and zero out any extra space in Z.
  Z[i++] = digit_add2(next, carry, &carry);
  DCHECK(carry == 0);  // NOLINT(readability/check)
  for (; i < Z.len(); i++) Z[i] = 0;
}

#undef BODY

}  // namespace bigint
}  // namespace v8
