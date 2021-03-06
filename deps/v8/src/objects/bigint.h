// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BIGINT_H_
#define V8_OBJECTS_BIGINT_H_

#include "src/common/globals.h"
#include "src/objects/objects.h"
#include "src/objects/primitive-heap-object.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

void MutableBigInt_AbsoluteAddAndCanonicalize(Address result_addr,
                                              Address x_addr, Address y_addr);
int32_t MutableBigInt_AbsoluteCompare(Address x_addr, Address y_addr);
void MutableBigInt_AbsoluteSubAndCanonicalize(Address result_addr,
                                              Address x_addr, Address y_addr);

class BigInt;
class ValueDeserializer;
class ValueSerializer;

#include "torque-generated/src/objects/bigint-tq.inc"

// BigIntBase is just the raw data object underlying a BigInt. Use with care!
// Most code should be using BigInts instead.
class BigIntBase : public PrimitiveHeapObject {
 public:
  inline int length() const {
    int32_t bitfield = RELAXED_READ_INT32_FIELD(*this, kBitfieldOffset);
    return LengthBits::decode(static_cast<uint32_t>(bitfield));
  }

  // For use by the GC.
  inline int synchronized_length() const {
    int32_t bitfield = ACQUIRE_READ_INT32_FIELD(*this, kBitfieldOffset);
    return LengthBits::decode(static_cast<uint32_t>(bitfield));
  }

  // The maximum kMaxLengthBits that the current implementation supports
  // would be kMaxInt - kSystemPointerSize * kBitsPerByte - 1.
  // Since we want a platform independent limit, choose a nice round number
  // somewhere below that maximum.
  static const int kMaxLengthBits = 1 << 30;  // ~1 billion.
  static const int kMaxLength =
      kMaxLengthBits / (kSystemPointerSize * kBitsPerByte);

  // Sign and length are stored in the same bitfield.  Since the GC needs to be
  // able to read the length concurrently, the getters and setters are atomic.
  static const int kLengthFieldBits = 30;
  STATIC_ASSERT(kMaxLength <= ((1 << kLengthFieldBits) - 1));
  using SignBits = base::BitField<bool, 0, 1>;
  using LengthBits = SignBits::Next<int, kLengthFieldBits>;
  STATIC_ASSERT(LengthBits::kLastUsedBit < 32);

  // Layout description.
#define BIGINT_FIELDS(V)                                                  \
  V(kBitfieldOffset, kInt32Size)                                          \
  V(kOptionalPaddingOffset, POINTER_SIZE_PADDING(kOptionalPaddingOffset)) \
  /* Header size. */                                                      \
  V(kHeaderSize, 0)                                                       \
  V(kDigitsOffset, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(PrimitiveHeapObject::kHeaderSize, BIGINT_FIELDS)
#undef BIGINT_FIELDS

  static constexpr bool HasOptionalPadding() {
    return FIELD_SIZE(kOptionalPaddingOffset) > 0;
  }

  DECL_CAST(BigIntBase)
  DECL_VERIFIER(BigIntBase)
  DECL_PRINTER(BigIntBase)

 private:
  friend class ::v8::internal::BigInt;  // MSVC wants full namespace.
  friend class MutableBigInt;

  using digit_t = uintptr_t;
  static const int kDigitSize = sizeof(digit_t);
  // kMaxLength definition assumes this:
  STATIC_ASSERT(kDigitSize == kSystemPointerSize);

  static const int kDigitBits = kDigitSize * kBitsPerByte;
  static const int kHalfDigitBits = kDigitBits / 2;
  static const digit_t kHalfDigitMask = (1ull << kHalfDigitBits) - 1;

  // sign() == true means negative.
  inline bool sign() const {
    int32_t bitfield = RELAXED_READ_INT32_FIELD(*this, kBitfieldOffset);
    return SignBits::decode(static_cast<uint32_t>(bitfield));
  }

  inline digit_t digit(int n) const {
    SLOW_DCHECK(0 <= n && n < length());
    return ReadField<digit_t>(kDigitsOffset + n * kDigitSize);
  }

  bool is_zero() const { return length() == 0; }

  OBJECT_CONSTRUCTORS(BigIntBase, PrimitiveHeapObject);
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
  inline static FreshlyAllocatedBigInt cast(Object object);
  inline static FreshlyAllocatedBigInt unchecked_cast(Object o) {
    return bit_cast<FreshlyAllocatedBigInt>(o);
  }

  // Clear uninitialized padding space.
  inline void clear_padding() {
    if (FIELD_SIZE(kOptionalPaddingOffset) != 0) {
      DCHECK_EQ(4, FIELD_SIZE(kOptionalPaddingOffset));
      memset(reinterpret_cast<void*>(address() + kOptionalPaddingOffset), 0,
             FIELD_SIZE(kOptionalPaddingOffset));
    }
  }

 private:
  // Only serves to make macros happy; other code should use IsBigInt.
  bool IsFreshlyAllocatedBigInt() const { return true; }

  OBJECT_CONSTRUCTORS(FreshlyAllocatedBigInt, BigIntBase);
};

// Arbitrary precision integers in JavaScript.
class BigInt : public BigIntBase {
 public:
  // Implementation of the Spec methods, see:
  // https://tc39.github.io/proposal-bigint/#sec-numeric-types
  // Sections 1.1.1 through 1.1.19.
  static Handle<BigInt> UnaryMinus(Isolate* isolate, Handle<BigInt> x);
  static MaybeHandle<BigInt> BitwiseNot(Isolate* isolate, Handle<BigInt> x);
  static MaybeHandle<BigInt> Exponentiate(Isolate* isolate, Handle<BigInt> base,
                                          Handle<BigInt> exponent);
  static MaybeHandle<BigInt> Multiply(Isolate* isolate, Handle<BigInt> x,
                                      Handle<BigInt> y);
  static MaybeHandle<BigInt> Divide(Isolate* isolate, Handle<BigInt> x,
                                    Handle<BigInt> y);
  static MaybeHandle<BigInt> Remainder(Isolate* isolate, Handle<BigInt> x,
                                       Handle<BigInt> y);
  static MaybeHandle<BigInt> Add(Isolate* isolate, Handle<BigInt> x,
                                 Handle<BigInt> y);
  static MaybeHandle<BigInt> Subtract(Isolate* isolate, Handle<BigInt> x,
                                      Handle<BigInt> y);
  static MaybeHandle<BigInt> LeftShift(Isolate* isolate, Handle<BigInt> x,
                                       Handle<BigInt> y);
  static MaybeHandle<BigInt> SignedRightShift(Isolate* isolate,
                                              Handle<BigInt> x,
                                              Handle<BigInt> y);
  static MaybeHandle<BigInt> UnsignedRightShift(Isolate* isolate,
                                                Handle<BigInt> x,
                                                Handle<BigInt> y);
  // More convenient version of "bool LessThan(x, y)".
  static ComparisonResult CompareToBigInt(Handle<BigInt> x, Handle<BigInt> y);
  static bool EqualToBigInt(BigInt x, BigInt y);
  static MaybeHandle<BigInt> BitwiseAnd(Isolate* isolate, Handle<BigInt> x,
                                        Handle<BigInt> y);
  static MaybeHandle<BigInt> BitwiseXor(Isolate* isolate, Handle<BigInt> x,
                                        Handle<BigInt> y);
  static MaybeHandle<BigInt> BitwiseOr(Isolate* isolate, Handle<BigInt> x,
                                       Handle<BigInt> y);

  // Other parts of the public interface.
  static MaybeHandle<BigInt> Increment(Isolate* isolate, Handle<BigInt> x);
  static MaybeHandle<BigInt> Decrement(Isolate* isolate, Handle<BigInt> x);

  bool ToBoolean() { return !is_zero(); }
  uint32_t Hash() {
    // TODO(jkummerow): Improve this. At least use length and sign.
    return is_zero() ? 0 : ComputeLongHash(static_cast<uint64_t>(digit(0)));
  }

  bool IsNegative() const { return sign(); }

  static Maybe<bool> EqualToString(Isolate* isolate, Handle<BigInt> x,
                                   Handle<String> y);
  static bool EqualToNumber(Handle<BigInt> x, Handle<Object> y);
  static Maybe<ComparisonResult> CompareToString(Isolate* isolate,
                                                 Handle<BigInt> x,
                                                 Handle<String> y);
  static ComparisonResult CompareToNumber(Handle<BigInt> x, Handle<Object> y);
  // Exposed for tests, do not call directly. Use CompareToNumber() instead.
  V8_EXPORT_PRIVATE static ComparisonResult CompareToDouble(Handle<BigInt> x,
                                                            double y);

  static Handle<BigInt> AsIntN(Isolate* isolate, uint64_t n, Handle<BigInt> x);
  static MaybeHandle<BigInt> AsUintN(Isolate* isolate, uint64_t n,
                                     Handle<BigInt> x);

  V8_EXPORT_PRIVATE static Handle<BigInt> FromInt64(Isolate* isolate,
                                                    int64_t n);
  static Handle<BigInt> FromUint64(Isolate* isolate, uint64_t n);
  static MaybeHandle<BigInt> FromWords64(Isolate* isolate, int sign_bit,
                                         int words64_count,
                                         const uint64_t* words);
  V8_EXPORT_PRIVATE int64_t AsInt64(bool* lossless = nullptr);
  uint64_t AsUint64(bool* lossless = nullptr);
  int Words64Count();
  void ToWordsArray64(int* sign_bit, int* words64_count, uint64_t* words);

  DECL_CAST(BigInt)
  void BigIntShortPrint(std::ostream& os);

  inline static int SizeFor(int length) {
    return kHeaderSize + length * kDigitSize;
  }

  static MaybeHandle<String> ToString(Isolate* isolate, Handle<BigInt> bigint,
                                      int radix = 10,
                                      ShouldThrow should_throw = kThrowOnError);
  // "The Number value for x", see:
  // https://tc39.github.io/ecma262/#sec-ecmascript-language-types-number-type
  // Returns a Smi or HeapNumber.
  static Handle<Object> ToNumber(Isolate* isolate, Handle<BigInt> x);

  // ECMAScript's NumberToBigInt
  V8_EXPORT_PRIVATE static MaybeHandle<BigInt> FromNumber(
      Isolate* isolate, Handle<Object> number);

  // ECMAScript's ToBigInt (throws for Number input)
  static MaybeHandle<BigInt> FromObject(Isolate* isolate, Handle<Object> obj);

  class BodyDescriptor;

 private:
  template <typename LocalIsolate>
  friend class StringToBigIntHelper;
  friend class ValueDeserializer;
  friend class ValueSerializer;

  // Special functions for StringToBigIntHelper:
  template <typename LocalIsolate>
  static Handle<BigInt> Zero(LocalIsolate* isolate, AllocationType allocation =
                                                        AllocationType::kYoung);
  template <typename LocalIsolate>
  static MaybeHandle<FreshlyAllocatedBigInt> AllocateFor(
      LocalIsolate* isolate, int radix, int charcount, ShouldThrow should_throw,
      AllocationType allocation);
  static void InplaceMultiplyAdd(FreshlyAllocatedBigInt x, uintptr_t factor,
                                 uintptr_t summand);
  template <typename LocalIsolate>
  static Handle<BigInt> Finalize(Handle<FreshlyAllocatedBigInt> x, bool sign);

  // Special functions for ValueSerializer/ValueDeserializer:
  uint32_t GetBitfieldForSerialization() const;
  static int DigitsByteLengthForBitfield(uint32_t bitfield);
  // Expects {storage} to have a length of at least
  // {DigitsByteLengthForBitfield(GetBitfieldForSerialization())}.
  void SerializeDigits(uint8_t* storage);
  V8_WARN_UNUSED_RESULT static MaybeHandle<BigInt> FromSerializedDigits(
      Isolate* isolate, uint32_t bitfield,
      Vector<const uint8_t> digits_storage);

  OBJECT_CONSTRUCTORS(BigInt, BigIntBase);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_BIGINT_H_
