// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_DIV_HELPERS_INL_H_
#define V8_BIGINT_DIV_HELPERS_INL_H_

#include <memory>

#include "src/bigint/bigint-inl.h"
#include "src/bigint/bigint-internal.h"
#include "src/bigint/util.h"

namespace v8 {
namespace bigint {

// These constants are primarily needed for Barrett division in div-barrett.cc,
// and they're also needed by fast to-string conversion in tostring.cc.
constexpr uint32_t DivideBarrettScratchSpace(uint32_t n) { return n + 2; }
// Local values S and W need "n plus a few" digits; U needs 2*n "plus a few".
// In all tested cases the "few" were either 2 or 3, so give 5 to be safe.
// S and W are not live at the same time.
constexpr uint32_t kInvertNewtonExtraSpace = 5;
constexpr uint32_t InvertNewtonScratchSpace(uint32_t n) {
  static_assert(3 * size_t{kMaxNumDigits} + 2 * kInvertNewtonExtraSpace <=
                UINT32_MAX);
  return 3 * n + 2 * kInvertNewtonExtraSpace;
}
constexpr uint32_t InvertScratchSpace(uint32_t n) {
  return n < config::kNewtonInversionThreshold ? 2 * n
                                               : InvertNewtonScratchSpace(n);
}

// Z += X. Returns the "carry" (0 or 1) after adding all of X's digits.
inline digit_t InplaceAddAndReturnCarry(RWDigits Z, Digits X) {
  digit_t carry = 0;
  for (uint32_t i = 0; i < X.len(); i++) {
    Z[i] = digit_add3(Z[i], X[i], carry, &carry);
  }
  return carry;
}

// Z -= X. Returns the "borrow" (0 or 1) after subtracting all of X's digits.
inline digit_t InplaceSubAndReturnBorrow(RWDigits Z, Digits X) {
  digit_t borrow = 0;
  for (uint32_t i = 0; i < X.len(); i++) {
    Z[i] = digit_sub2(Z[i], X[i], borrow, &borrow);
  }
  return borrow;
}

// These add exactly Y's digits to the matching digits in X, storing the
// result in (part of) Z, and return the carry/borrow.
inline digit_t AddAndReturnCarry(RWDigits Z, Digits X, Digits Y) {
  CHECK(Z.len() >= Y.len() && X.len() >= Y.len());
  digit_t carry = 0;
  for (uint32_t i = 0; i < Y.len(); i++) {
    Z[i] = digit_add3(X[i], Y[i], carry, &carry);
  }
  return carry;
}

inline digit_t SubtractAndReturnBorrow(RWDigits Z, Digits X, Digits Y) {
  CHECK(Z.len() >= Y.len() && X.len() >= Y.len());
  digit_t borrow = 0;
  for (uint32_t i = 0; i < Y.len(); i++) {
    Z[i] = digit_sub2(X[i], Y[i], borrow, &borrow);
  }
  return borrow;
}

inline void Copy(RWDigits Z, Digits X) {
  if (Z == X) return;
  uint32_t i = 0;
  for (; i < X.len(); i++) Z[i] = X[i];
  for (; i < Z.len(); i++) Z[i] = 0;
}

// Z := X << shift
// Z and X may alias for an in-place shift.
inline void LeftShift(RWDigits Z, Digits X, int shift) {
  DCHECK(shift >= 0);
  DCHECK(shift < kDigitBits);
  DCHECK(Z.len() >= X.len());
  if (shift == 0) return Copy(Z, X);
  digit_t carry = 0;
  uint32_t i = 0;
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
inline void RightShift(RWDigits Z, Digits X, int shift) {
  DCHECK(shift >= 0);
  DCHECK(shift < kDigitBits);
  X.Normalize();
  DCHECK(Z.len() >= X.len());
  if (shift == 0) return Copy(Z, X);
  uint32_t i = 0;
  if (X.len() > 0) {
    digit_t carry = X[0] >> shift;
    uint32_t last = X.len() - 1;
    for (; i < last; i++) {
      digit_t d = X[i + 1];
      Z[i] = (d << (kDigitBits - shift)) | carry;
      carry = d >> shift;
    }
    Z[i++] = carry;
  }
  for (; i < Z.len(); i++) Z[i] = 0;
}

inline void PutAt(RWDigits Z, Digits A, uint32_t count) {
  uint32_t len = std::min(A.len(), count);
  uint32_t i = 0;
  for (; i < len; i++) Z[i] = A[i];
  for (; i < count; i++) Z[i] = 0;
}

// Division algorithms typically need to left-shift their inputs into
// "bit-normalized" form (i.e. top bit is set). The inputs are considered
// read-only, and V8 relies on that by allowing concurrent reads from them,
// so by default, {ShiftedDigits} allocate temporary storage for their
// contents. In-place modification is opt-in for cases where callers can
// guarantee that it is safe.
// When callers allow in-place shifting and wish to undo it, they have to do
// so manually using {Reset()}.
// If {shift} is not given, it is auto-detected from {original}'s
// leading zeros.
class ShiftedDigits : public Digits {
 public:
  explicit ShiftedDigits(Digits& original, int shift = -1,
                         bool allow_inplace = false)
      : Digits(original.digits_, original.len_) {
    int leading_zeros = CountLeadingZeros(original.msd());
    if (shift < 0) {
      shift = leading_zeros;
    } else if (shift > leading_zeros) {
      allow_inplace = false;
      len_++;
    }
    shift_ = shift;
    if (shift == 0) {
      inplace_ = true;
      return;
    }
    inplace_ = allow_inplace;
    if (!inplace_) {
      digit_t* digits = new digit_t[len_];
      storage_.reset(digits);
      digits_ = digits;
    }
    RWDigits rw_view(digits_, len_);
    LeftShift(rw_view, original, shift_);
  }

  // For callers that have available scratch memory.
  ShiftedDigits(Digits& original, RWDigits scratch)
      : Digits(original.digits_, original.len_) {
    DCHECK(scratch.len() >= original.len());
    shift_ = CountLeadingZeros(original.msd());
    if (shift_ == 0) {
      inplace_ = true;
      return;
    }
    digits_ = scratch.digits_;
    RWDigits rw_view(digits_, len_);
    LeftShift(rw_view, original, shift_);
  }

  ~ShiftedDigits() = default;

  void Reset() {
    if (inplace_ && shift_) {
      RWDigits rw_view(digits_, len_);
      RightShift(rw_view, rw_view, shift_);
    }
  }

  int shift() { return shift_; }

 private:
  int shift_;
  bool inplace_;
  std::unique_ptr<digit_t[]> storage_;
};

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_DIV_HELPERS_INL_H_
