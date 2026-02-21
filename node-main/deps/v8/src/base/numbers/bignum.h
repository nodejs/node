// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_NUMBERS_BIGNUM_H_
#define V8_BASE_NUMBERS_BIGNUM_H_

#include "src/base/vector.h"

namespace v8 {
namespace base {

class V8_BASE_EXPORT Bignum {
 public:
  // 3584 = 128 * 28. We can represent 2^3584 > 10^1000 accurately.
  // This bignum can encode much bigger numbers, since it contains an
  // exponent.
  static const int kMaxSignificantBits = 3584;

  Bignum();
  Bignum(const Bignum&) = delete;
  Bignum& operator=(const Bignum&) = delete;
  void AssignUInt16(uint16_t value);
  void AssignUInt64(uint64_t value);
  void AssignBignum(const Bignum& other);

  void AssignDecimalString(Vector<const char> value);
  void AssignHexString(Vector<const char> value);

  void AssignPowerUInt16(uint16_t base, int exponent);

  void AddUInt16(uint16_t operand);
  void AddUInt64(uint64_t operand);
  void AddBignum(const Bignum& other);
  // Precondition: this >= other.
  void SubtractBignum(const Bignum& other);

  void Square();
  void ShiftLeft(int shift_amount);
  void MultiplyByUInt32(uint32_t factor);
  void MultiplyByUInt64(uint64_t factor);
  void MultiplyByPowerOfTen(int exponent);
  void Times10() { return MultiplyByUInt32(10); }
  // Pseudocode:
  //  int result = this / other;
  //  this = this % other;
  // In the worst case this function is in O(this/other).
  uint16_t DivideModuloIntBignum(const Bignum& other);

  bool ToHexString(char* buffer, int buffer_size) const;

  static int Compare(const Bignum& a, const Bignum& b);
  static bool Equal(const Bignum& a, const Bignum& b) {
    return Compare(a, b) == 0;
  }
  static bool LessEqual(const Bignum& a, const Bignum& b) {
    return Compare(a, b) <= 0;
  }
  static bool Less(const Bignum& a, const Bignum& b) {
    return Compare(a, b) < 0;
  }
  // Returns Compare(a + b, c);
  static int PlusCompare(const Bignum& a, const Bignum& b, const Bignum& c);
  // Returns a + b == c
  static bool PlusEqual(const Bignum& a, const Bignum& b, const Bignum& c) {
    return PlusCompare(a, b, c) == 0;
  }
  // Returns a + b <= c
  static bool PlusLessEqual(const Bignum& a, const Bignum& b, const Bignum& c) {
    return PlusCompare(a, b, c) <= 0;
  }
  // Returns a + b < c
  static bool PlusLess(const Bignum& a, const Bignum& b, const Bignum& c) {
    return PlusCompare(a, b, c) < 0;
  }

 private:
  using Chunk = uint32_t;
  using DoubleChunk = uint64_t;

  static const int kChunkSize = sizeof(Chunk) * 8;
  static const int kDoubleChunkSize = sizeof(DoubleChunk) * 8;
  // With bigit size of 28 we loose some bits, but a double still fits easily
  // into two chunks, and more importantly we can use the Comba multiplication.
  static const int kBigitSize = 28;
  static const Chunk kBigitMask = (1 << kBigitSize) - 1;
  // Every instance allocates kBigitLength chunks on the stack. Bignums cannot
  // grow. There are no checks if the stack-allocated space is sufficient.
  static const int kBigitCapacity = kMaxSignificantBits / kBigitSize;

  void EnsureCapacity(int size) {
    if (size > kBigitCapacity) {
      UNREACHABLE();
    }
  }
  void Align(const Bignum& other);
  void Clamp();
  bool IsClamped() const;
  void Zero();
  // Requires this to have enough capacity (no tests done).
  // Updates used_digits_ if necessary.
  // by must be < kBigitSize.
  void BigitsShiftLeft(int shift_amount);
  // BigitLength includes the "hidden" digits encoded in the exponent.
  int BigitLength() const { return used_digits_ + exponent_; }
  Chunk BigitAt(int index) const;
  void SubtractTimes(const Bignum& other, int factor);

  Chunk bigits_buffer_[kBigitCapacity];
  // A vector backed by bigits_buffer_. This way accesses to the array are
  // checked for out-of-bounds errors.
  Vector<Chunk> bigits_;
  int used_digits_;
  // The Bignum's value equals value(bigits_) * 2^(exponent_ * kBigitSize).
  int exponent_;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_NUMBERS_BIGNUM_H_
