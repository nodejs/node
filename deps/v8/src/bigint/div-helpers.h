// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BIGINT_DIV_HELPERS_H_
#define V8_BIGINT_DIV_HELPERS_H_

#include <memory>

#include "src/bigint/bigint.h"
#include "src/bigint/util.h"

namespace v8 {
namespace bigint {

void LeftShift(RWDigits Z, Digits X, int shift);
void RightShift(RWDigits Z, Digits X, int shift);

inline void PutAt(RWDigits Z, Digits A, int count) {
  int len = std::min(A.len(), count);
  int i = 0;
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
  ~ShiftedDigits() = default;

  void Reset() {
    if (inplace_) {
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

#endif  // V8_BIGINT_DIV_HELPERS_H_
