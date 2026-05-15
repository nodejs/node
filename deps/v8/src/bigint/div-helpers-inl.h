// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_DIV_HELPERS_INL_H_
#define V8_BIGINT_DIV_HELPERS_INL_H_

#include <memory>

#include "src/bigint/bigint-inl.h"
#include "src/bigint/bigint-internal.h"

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
  ShiftedDigits(Digits& original, Platform* platform, int shift = -1,
                bool allow_inplace = false)
      : Digits(original.digits_, original.len_),
        storage_(nullptr, Platform::Deleter(platform)) {
    int leading_zeros = CountLeadingZeros(original.msd());
    if (shift < 0) {
      shift = leading_zeros;
    } else if (shift > leading_zeros) {
      allow_inplace = false;
      len_++;
    }
    shift_ = shift;
    // For shift == 0, we could always allow in-place handling if we didn't
    // have to worry about concurrent malicious corruption. The situations
    // where the source is corruptible are the same where callers don't allow
    // in-place modification.
    if (shift == 0 && allow_inplace) {
      inplace_ = true;
      return;
    }
    inplace_ = allow_inplace;
    if (!inplace_) {
      storage_.reset(platform->Allocate(len_));
      digits_ = storage_.get();
    }
    RWDigits rw_view(digits_, len_);
    LeftShift(rw_view, original, shift_);
  }

  // For callers that have available scratch memory.
  ShiftedDigits(Digits& original, RWDigits scratch)
      : Digits(scratch.digits_, original.len_),
        inplace_(false),
        // Neither {storage_} nor its deleter will be used.
        storage_(nullptr, Platform::Deleter(nullptr)) {
    DCHECK(scratch.len() >= original.len());
    shift_ = CountLeadingZeros(original.msd());
    // Always make a copy, even when shift_ == 0, to protect against
    // concurrent in-sandbox mutation.
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
  std::unique_ptr<digit_t[], Platform::Deleter> storage_;
};

}  // namespace bigint
}  // namespace v8

#endif  // V8_BIGINT_DIV_HELPERS_INL_H_
