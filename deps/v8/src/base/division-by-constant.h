// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_DIVISION_BY_CONSTANT_H_
#define V8_BASE_DIVISION_BY_CONSTANT_H_

namespace v8 {
namespace base {

// ----------------------------------------------------------------------------

// The magic numbers for division via multiplication, see Warren's "Hacker's
// Delight", chapter 10. The template parameter must be one of the unsigned
// integral types.
template <class T>
struct MagicNumbersForDivision {
  MagicNumbersForDivision(T m, unsigned s, bool a)
      : multiplier(m), shift(s), add(a) {}
  bool operator==(const MagicNumbersForDivision& rhs) const;

  T multiplier;
  unsigned shift;
  bool add;
};


// Calculate the multiplier and shift for signed division via multiplication.
// The divisor must not be -1, 0 or 1 when interpreted as a signed value.
template <class T>
MagicNumbersForDivision<T> SignedDivisionByConstant(T d);


// Calculate the multiplier and shift for unsigned division via multiplication,
// see Warren's "Hacker's Delight", chapter 10. The divisor must not be 0 and
// leading_zeros can be used to speed up the calculation if the given number of
// upper bits of the dividend value are known to be zero.
template <class T>
MagicNumbersForDivision<T> UnsignedDivisionByConstant(
    T d, unsigned leading_zeros = 0);

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_DIVISION_BY_CONSTANT_H_
