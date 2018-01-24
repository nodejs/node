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

// BigIntBase is just the raw data object underlying a BigInt. Use with care!
// Most code should be using BigInts instead.
class BigIntBase : public HeapObject {
 public:
  inline int length() const {
    intptr_t bitfield = READ_INTPTR_FIELD(this, kBitfieldOffset);
    return LengthBits::decode(static_cast<uint32_t>(bitfield));
  }

  // The maximum length that the current implementation supports would be
  // kMaxInt / kDigitBits. However, we use a lower limit for now, because
  // raising it later is easier than lowering it.
  // Support up to 1 million bits.
  static const int kMaxLengthBits = 1024 * 1024;
  static const int kMaxLength = kMaxLengthBits / (kPointerSize * kBitsPerByte);

 private:
  friend class BigInt;
  friend class MutableBigInt;

  typedef uintptr_t digit_t;
  static const int kDigitSize = sizeof(digit_t);
  // kMaxLength definition assumes this:
  STATIC_ASSERT(kDigitSize == kPointerSize);

  static const int kDigitBits = kDigitSize * kBitsPerByte;
  static const int kHalfDigitBits = kDigitBits / 2;
  static const digit_t kHalfDigitMask = (1ull << kHalfDigitBits) - 1;

  static const int kBitfieldOffset = HeapObject::kHeaderSize;
  static const int kDigitsOffset = kBitfieldOffset + kPointerSize;
  static const int kHeaderSize = kDigitsOffset;

  static const int kLengthFieldBits = 20;
  STATIC_ASSERT(kMaxLength <= ((1 << kLengthFieldBits) - 1));
  class LengthBits : public BitField<int, 0, kLengthFieldBits> {};
  class SignBits : public BitField<bool, LengthBits::kNext, 1> {};

  // sign() == true means negative.
  inline bool sign() const {
    intptr_t bitfield = READ_INTPTR_FIELD(this, kBitfieldOffset);
    return SignBits::decode(static_cast<uint32_t>(bitfield));
  }

  inline digit_t digit(int n) const {
    SLOW_DCHECK(0 <= n && n < length());
    const byte* address =
        FIELD_ADDR_CONST(this, kDigitsOffset + n * kDigitSize);
    return *reinterpret_cast<digit_t*>(reinterpret_cast<intptr_t>(address));
  }

  bool is_zero() const { return length() == 0; }

  DISALLOW_IMPLICIT_CONSTRUCTORS(BigIntBase);
};

class FreshlyAllocatedBigInt : public BigIntBase {
  // This class is essentially the publicly accessible abstract version of
  // MutableBigInt (which is a hidden implementation detail). It serves as
  // the return type of Factory::NewBigInt, and makes it possible to enforce
  // casting restrictions:
  // - FreshlyAllocatedBigInt can be cast explicitly to MutableBigInt
  //   (with MutableBigInt::Cast) for initialization.
  // - MutableBigInt can be cast/converted explicitly to BigInt
  //   (with MutableBigInt::MakeImmutable); is afterwards treated as readonly.
  // - No accidental implicit casting is possible from BigInt to MutableBigInt
  //   (and no explicit operator is provided either).

 public:
  inline static FreshlyAllocatedBigInt* cast(Object* object) {
    SLOW_DCHECK(object->IsBigInt());
    return reinterpret_cast<FreshlyAllocatedBigInt*>(object);
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FreshlyAllocatedBigInt);
};

// UNDER CONSTRUCTION!
// Arbitrary precision integers in JavaScript.
class V8_EXPORT_PRIVATE BigInt : public BigIntBase {
 public:
  // Implementation of the Spec methods, see:
  // https://tc39.github.io/proposal-bigint/#sec-numeric-types
  // Sections 1.1.1 through 1.1.19.
  static Handle<BigInt> UnaryMinus(Handle<BigInt> x);
  static MaybeHandle<BigInt> BitwiseNot(Handle<BigInt> x);
  static MaybeHandle<BigInt> Exponentiate(Handle<BigInt> base,
                                          Handle<BigInt> exponent);
  static MaybeHandle<BigInt> Multiply(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> Divide(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> Remainder(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> Add(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> Subtract(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> LeftShift(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> SignedRightShift(Handle<BigInt> x,
                                              Handle<BigInt> y);
  static MaybeHandle<BigInt> UnsignedRightShift(Handle<BigInt> x,
                                                Handle<BigInt> y);
  // More convenient version of "bool LessThan(x, y)".
  static ComparisonResult CompareToBigInt(Handle<BigInt> x, Handle<BigInt> y);
  static bool EqualToBigInt(BigInt* x, BigInt* y);
  static MaybeHandle<BigInt> BitwiseAnd(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> BitwiseXor(Handle<BigInt> x, Handle<BigInt> y);
  static MaybeHandle<BigInt> BitwiseOr(Handle<BigInt> x, Handle<BigInt> y);

  // Other parts of the public interface.
  static MaybeHandle<BigInt> Increment(Handle<BigInt> x);
  static MaybeHandle<BigInt> Decrement(Handle<BigInt> x);

  bool ToBoolean() { return !is_zero(); }
  uint32_t Hash() {
    // TODO(jkummerow): Improve this. At least use length and sign.
    return is_zero() ? 0 : ComputeIntegerHash(static_cast<uint32_t>(digit(0)));
  }

  static bool EqualToString(Handle<BigInt> x, Handle<String> y);
  static bool EqualToNumber(Handle<BigInt> x, Handle<Object> y);
  static ComparisonResult CompareToNumber(Handle<BigInt> x, Handle<Object> y);
  // Exposed for tests, do not call directly. Use CompareToNumber() instead.
  static ComparisonResult CompareToDouble(Handle<BigInt> x, double y);

  static Handle<BigInt> AsIntN(uint64_t n, Handle<BigInt> x);
  static MaybeHandle<BigInt> AsUintN(uint64_t n, Handle<BigInt> x);

  DECL_CAST(BigInt)
  DECL_VERIFIER(BigInt)
  DECL_PRINTER(BigInt)
  void BigIntShortPrint(std::ostream& os);

  inline static int SizeFor(int length) {
    return kHeaderSize + length * kDigitSize;
  }

  static MaybeHandle<String> ToString(Handle<BigInt> bigint, int radix = 10);
  // "The Number value for x", see:
  // https://tc39.github.io/ecma262/#sec-ecmascript-language-types-number-type
  // Returns a Smi or HeapNumber.
  static Handle<Object> ToNumber(Handle<BigInt> x);

  // ECMAScript's NumberToBigInt
  static MaybeHandle<BigInt> FromNumber(Isolate* isolate,
                                        Handle<Object> number);

  // ECMAScript's ToBigInt (throws for Number input)
  static MaybeHandle<BigInt> FromObject(Isolate* isolate, Handle<Object> obj);

  class BodyDescriptor;

 private:
  friend class BigIntParseIntHelper;

  // Special functions for BigIntParseIntHelper:
  static Handle<BigInt> Zero(Isolate* isolate);
  static MaybeHandle<FreshlyAllocatedBigInt> AllocateFor(
      Isolate* isolate, int radix, int charcount, ShouldThrow should_throw);
  static void InplaceMultiplyAdd(Handle<FreshlyAllocatedBigInt> x,
                                 uintptr_t factor, uintptr_t summand);
  static Handle<BigInt> Finalize(Handle<FreshlyAllocatedBigInt> x, bool sign);

  DISALLOW_IMPLICIT_CONSTRUCTORS(BigInt);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BIGINT_H_
