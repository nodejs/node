// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BIGINT_H_
#define V8_OBJECTS_BIGINT_H_

#include "src/globals.h"
#include "src/objects.h"
#include "src/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// UNDER CONSTRUCTION!
// Arbitrary precision integers in JavaScript.
class BigInt : public HeapObject {
 public:
  // Implementation of the Spec methods, see:
  // https://tc39.github.io/proposal-bigint/#sec-numeric-types
  // Sections 1.1.1 through 1.1.19.
  static Handle<BigInt> UnaryMinus(Handle<BigInt> x);
  static Handle<BigInt> BitwiseNot(Handle<BigInt> x);
  static MaybeHandle<BigInt> Exponentiate(Handle<BigInt> base,
                                          Handle<BigInt> exponent);
  static Handle<BigInt> Multiply(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> Divide(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> Remainder(Handle<BigInt> x, Handle<BigInt> y);
  static Handle<BigInt> Add(Handle<BigInt> x, Handle<BigInt> y);
  static Handle<BigInt> Subtract(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> LeftShift(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> SignedRightShift(Handle<BigInt> x,
                                              Handle<BigInt> y);
  static MaybeHandle<BigInt> UnsignedRightShift(Handle<BigInt> x,
                                                Handle<BigInt> y);
  static bool LessThan(Handle<BigInt> x, Handle<BigInt> y);
  static bool Equal(BigInt* x, BigInt* y);
  static Handle<BigInt> BitwiseAnd(Handle<BigInt> x, Handle<BigInt> y);
  static Handle<BigInt> BitwiseXor(Handle<BigInt> x, Handle<BigInt> y);
  static Handle<BigInt> BitwiseOr(Handle<BigInt> x, Handle<BigInt> y);

  // Other parts of the public interface.
  bool ToBoolean() { return !is_zero(); }
  uint32_t Hash() {
    // TODO(jkummerow): Improve this. At least use length and sign.
    return is_zero() ? 0 : ComputeIntegerHash(static_cast<uint32_t>(digit(0)));
  }

  DECL_CAST(BigInt)
  DECL_VERIFIER(BigInt)
  DECL_PRINTER(BigInt)
  void BigIntShortPrint(std::ostream& os);

  // TODO(jkummerow): Do we need {synchronized_length} for GC purposes?
  DECL_INT_ACCESSORS(length)

  inline static int SizeFor(int length) {
    return kHeaderSize + length * kDigitSize;
  }
  void Initialize(int length, bool zero_initialize);

  static MaybeHandle<String> ToString(Handle<BigInt> bigint, int radix = 10);

  // The maximum length that the current implementation supports would be
  // kMaxInt / kDigitBits. However, we use a lower limit for now, because
  // raising it later is easier than lowering it.
  static const int kMaxLengthBits = 20;
  static const int kMaxLength = (1 << kMaxLengthBits) - 1;

  class BodyDescriptor;

 private:
  friend class Factory;
  friend class BigIntParseIntHelper;

  typedef uintptr_t digit_t;
  static const int kDigitSize = sizeof(digit_t);
  static const int kDigitBits = kDigitSize * kBitsPerByte;
  static const int kHalfDigitBits = kDigitBits / 2;
  static const digit_t kHalfDigitMask = (1ull << kHalfDigitBits) - 1;

  // Private helpers for public methods.
  static Handle<BigInt> Copy(Handle<BigInt> source);
  static MaybeHandle<BigInt> AllocateFor(Isolate* isolate, int radix,
                                         int charcount);
  void RightTrim();

  static Handle<BigInt> AbsoluteAdd(Handle<BigInt> x, Handle<BigInt> y,
                                    bool result_sign);
  static Handle<BigInt> AbsoluteSub(Handle<BigInt> x, Handle<BigInt> y,
                                    bool result_sign);
  static Handle<BigInt> AbsoluteAddOne(Handle<BigInt> x, bool sign,
                                       BigInt* result_storage);
  static Handle<BigInt> AbsoluteSubOne(Handle<BigInt> x, int result_length);

  enum ExtraDigitsHandling { kCopy, kSkip };
  static inline Handle<BigInt> AbsoluteBitwiseOp(
      Handle<BigInt> x, Handle<BigInt> y, BigInt* result_storage,
      ExtraDigitsHandling extra_digits,
      std::function<digit_t(digit_t, digit_t)> op);
  static Handle<BigInt> AbsoluteAnd(Handle<BigInt> x, Handle<BigInt> y,
                                    BigInt* result_storage = nullptr);
  static Handle<BigInt> AbsoluteAndNot(Handle<BigInt> x, Handle<BigInt> y,
                                       BigInt* result_storage = nullptr);
  static Handle<BigInt> AbsoluteOr(Handle<BigInt> x, Handle<BigInt> y,
                                   BigInt* result_storage = nullptr);
  static Handle<BigInt> AbsoluteXor(Handle<BigInt> x, Handle<BigInt> y,
                                    BigInt* result_storage = nullptr);

  static int AbsoluteCompare(Handle<BigInt> x, Handle<BigInt> y);

  static void MultiplyAccumulate(Handle<BigInt> multiplicand,
                                 digit_t multiplier, Handle<BigInt> accumulator,
                                 int accumulator_index);
  static void InternalMultiplyAdd(BigInt* source, digit_t factor,
                                  digit_t summand, int n, BigInt* result);
  void InplaceMultiplyAdd(uintptr_t factor, uintptr_t summand);

  // Specialized helpers for Divide/Remainder.
  static void AbsoluteDivSmall(Handle<BigInt> x, digit_t divisor,
                               Handle<BigInt>* quotient, digit_t* remainder);
  static void AbsoluteDivLarge(Handle<BigInt> dividend, Handle<BigInt> divisor,
                               Handle<BigInt>* quotient,
                               Handle<BigInt>* remainder);
  static bool ProductGreaterThan(digit_t factor1, digit_t factor2, digit_t high,
                                 digit_t low);
  digit_t InplaceAdd(BigInt* summand, int start_index);
  digit_t InplaceSub(BigInt* subtrahend, int start_index);
  void InplaceRightShift(int shift);
  enum SpecialLeftShiftMode {
    kSameSizeResult,
    kAlwaysAddOneDigit,
  };
  static Handle<BigInt> SpecialLeftShift(Handle<BigInt> x, int shift,
                                         SpecialLeftShiftMode mode);

  // Specialized helpers for shift operations.
  static MaybeHandle<BigInt> LeftShiftByAbsolute(Handle<BigInt> x,
                                                 Handle<BigInt> y);
  static Handle<BigInt> RightShiftByAbsolute(Handle<BigInt> x,
                                             Handle<BigInt> y);
  static Handle<BigInt> RightShiftByMaximum(Isolate* isolate, bool sign);
  static Maybe<digit_t> ToShiftAmount(Handle<BigInt> x);

  static MaybeHandle<String> ToStringBasePowerOfTwo(Handle<BigInt> x,
                                                    int radix);
  static MaybeHandle<String> ToStringGeneric(Handle<BigInt> x, int radix);

  // Digit arithmetic helpers.
  static inline digit_t digit_add(digit_t a, digit_t b, digit_t* carry);
  static inline digit_t digit_sub(digit_t a, digit_t b, digit_t* borrow);
  static inline digit_t digit_mul(digit_t a, digit_t b, digit_t* high);
  static inline digit_t digit_div(digit_t high, digit_t low, digit_t divisor,
                                  digit_t* remainder);
  static digit_t digit_pow(digit_t base, digit_t exponent);
  static inline bool digit_ismax(digit_t x) {
    return static_cast<digit_t>(~x) == 0;
  }

  class LengthBits : public BitField<int, 0, kMaxLengthBits> {};
  class SignBits : public BitField<bool, LengthBits::kNext, 1> {};

  // Low-level accessors.
  // sign() == true means negative.
  DECL_BOOLEAN_ACCESSORS(sign)
  inline digit_t digit(int n) const;
  inline void set_digit(int n, digit_t value);

  bool is_zero() {
    DCHECK(length() > 0 || !sign());  // There is no -0n.
    return length() == 0;
  }
  static const int kBitfieldOffset = HeapObject::kHeaderSize;
  static const int kDigitsOffset = kBitfieldOffset + kPointerSize;
  static const int kHeaderSize = kDigitsOffset;
  DISALLOW_IMPLICIT_CONSTRUCTORS(BigInt);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BIGINT_H_
