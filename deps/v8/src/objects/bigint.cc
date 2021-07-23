// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Parts of the implementation below:

// Copyright (c) 2014 the Dart project authors.  Please see the AUTHORS file [1]
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file [2].
//
// [1] https://github.com/dart-lang/sdk/blob/master/AUTHORS
// [2] https://github.com/dart-lang/sdk/blob/master/LICENSE

// Copyright 2009 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file [3].
//
// [3] https://golang.org/LICENSE

#include "src/objects/bigint.h"

#include "src/base/numbers/double.h"
#include "src/bigint/bigint.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

// The MutableBigInt class is an implementation detail designed to prevent
// accidental mutation of a BigInt after its construction. Step-by-step
// construction of a BigInt must happen in terms of MutableBigInt, the
// final result is then passed through MutableBigInt::MakeImmutable and not
// modified further afterwards.
// Many of the functions in this class use arguments of type {BigIntBase},
// indicating that they will be used in a read-only capacity, and both
// {BigInt} and {MutableBigInt} objects can be passed in.
class MutableBigInt : public FreshlyAllocatedBigInt {
 public:
  // Bottleneck for converting MutableBigInts to BigInts.
  static MaybeHandle<BigInt> MakeImmutable(MaybeHandle<MutableBigInt> maybe);
  template <typename Isolate = v8::internal::Isolate>
  static Handle<BigInt> MakeImmutable(Handle<MutableBigInt> result);

  static void Canonicalize(MutableBigInt result);

  // Allocation helpers.
  template <typename IsolateT>
  static MaybeHandle<MutableBigInt> New(
      IsolateT* isolate, int length,
      AllocationType allocation = AllocationType::kYoung);
  static Handle<BigInt> NewFromInt(Isolate* isolate, int value);
  static Handle<BigInt> NewFromDouble(Isolate* isolate, double value);
  void InitializeDigits(int length, byte value = 0);
  static Handle<MutableBigInt> Copy(Isolate* isolate,
                                    Handle<BigIntBase> source);
  template <typename IsolateT>
  static Handle<BigInt> Zero(
      IsolateT* isolate, AllocationType allocation = AllocationType::kYoung) {
    // TODO(jkummerow): Consider caching a canonical zero-BigInt.
    return MakeImmutable<IsolateT>(
        New(isolate, 0, allocation).ToHandleChecked());
  }

  static Handle<MutableBigInt> Cast(Handle<FreshlyAllocatedBigInt> bigint) {
    SLOW_DCHECK(bigint->IsBigInt());
    return Handle<MutableBigInt>::cast(bigint);
  }
  static MutableBigInt cast(Object o) {
    SLOW_DCHECK(o.IsBigInt());
    return MutableBigInt(o.ptr());
  }
  static MutableBigInt unchecked_cast(Object o) {
    return MutableBigInt(o.ptr());
  }

  // Internal helpers.
  static MaybeHandle<MutableBigInt> BitwiseAnd(Isolate* isolate,
                                               Handle<BigInt> x,
                                               Handle<BigInt> y);
  static MaybeHandle<MutableBigInt> BitwiseXor(Isolate* isolate,
                                               Handle<BigInt> x,
                                               Handle<BigInt> y);
  static MaybeHandle<MutableBigInt> BitwiseOr(Isolate* isolate,
                                              Handle<BigInt> x,
                                              Handle<BigInt> y);

  static Handle<BigInt> TruncateToNBits(Isolate* isolate, int n,
                                        Handle<BigInt> x);
  static Handle<BigInt> TruncateAndSubFromPowerOfTwo(Isolate* isolate, int n,
                                                     Handle<BigInt> x,
                                                     bool result_sign);

  static MaybeHandle<MutableBigInt> AbsoluteAddOne(
      Isolate* isolate, Handle<BigIntBase> x, bool sign,
      MutableBigInt result_storage = MutableBigInt());
  static Handle<MutableBigInt> AbsoluteSubOne(Isolate* isolate,
                                              Handle<BigIntBase> x);
  static MaybeHandle<MutableBigInt> AbsoluteSubOne(Isolate* isolate,
                                                   Handle<BigIntBase> x,
                                                   int result_length);

  enum ExtraDigitsHandling { kCopy, kSkip };
  enum SymmetricOp { kSymmetric, kNotSymmetric };
  static inline Handle<MutableBigInt> AbsoluteBitwiseOp(
      Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
      MutableBigInt result_storage, ExtraDigitsHandling extra_digits,
      SymmetricOp symmetric,
      const std::function<digit_t(digit_t, digit_t)>& op);
  static Handle<MutableBigInt> AbsoluteAnd(
      Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
      MutableBigInt result_storage = MutableBigInt());
  static Handle<MutableBigInt> AbsoluteAndNot(
      Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
      MutableBigInt result_storage = MutableBigInt());
  static Handle<MutableBigInt> AbsoluteOr(
      Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
      MutableBigInt result_storage = MutableBigInt());
  static Handle<MutableBigInt> AbsoluteXor(
      Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
      MutableBigInt result_storage = MutableBigInt());

  static void InternalMultiplyAdd(BigIntBase source, digit_t factor,
                                  digit_t summand, int n, MutableBigInt result);
  void InplaceMultiplyAdd(uintptr_t factor, uintptr_t summand);

  // Specialized helpers for shift operations.
  static MaybeHandle<BigInt> LeftShiftByAbsolute(Isolate* isolate,
                                                 Handle<BigIntBase> x,
                                                 Handle<BigIntBase> y);
  static Handle<BigInt> RightShiftByAbsolute(Isolate* isolate,
                                             Handle<BigIntBase> x,
                                             Handle<BigIntBase> y);
  static Handle<BigInt> RightShiftByMaximum(Isolate* isolate, bool sign);
  static Maybe<digit_t> ToShiftAmount(Handle<BigIntBase> x);

  static double ToDouble(Handle<BigIntBase> x);
  enum Rounding { kRoundDown, kTie, kRoundUp };
  static Rounding DecideRounding(Handle<BigIntBase> x, int mantissa_bits_unset,
                                 int digit_index, uint64_t current_digit);

  // Returns the least significant 64 bits, simulating two's complement
  // representation.
  static uint64_t GetRawBits(BigIntBase x, bool* lossless);

  // Digit arithmetic helpers.
  static inline digit_t digit_add(digit_t a, digit_t b, digit_t* carry);
  static inline digit_t digit_sub(digit_t a, digit_t b, digit_t* borrow);
  static inline digit_t digit_mul(digit_t a, digit_t b, digit_t* high);
  static inline bool digit_ismax(digit_t x) {
    return static_cast<digit_t>(~x) == 0;
  }

// Internal field setters. Non-mutable BigInts don't have these.
#include "src/objects/object-macros.h"
  inline void set_sign(bool new_sign) {
    int32_t bitfield = RELAXED_READ_INT32_FIELD(*this, kBitfieldOffset);
    bitfield = SignBits::update(bitfield, new_sign);
    RELAXED_WRITE_INT32_FIELD(*this, kBitfieldOffset, bitfield);
  }
  inline void set_length(int new_length, ReleaseStoreTag) {
    int32_t bitfield = RELAXED_READ_INT32_FIELD(*this, kBitfieldOffset);
    bitfield = LengthBits::update(bitfield, new_length);
    RELEASE_WRITE_INT32_FIELD(*this, kBitfieldOffset, bitfield);
  }
  inline void initialize_bitfield(bool sign, int length) {
    int32_t bitfield = LengthBits::encode(length) | SignBits::encode(sign);
    WriteField<int32_t>(kBitfieldOffset, bitfield);
  }
  inline void set_digit(int n, digit_t value) {
    SLOW_DCHECK(0 <= n && n < length());
    WriteField<digit_t>(kDigitsOffset + n * kDigitSize, value);
  }

  void set_64_bits(uint64_t bits);

  bool IsMutableBigInt() const { return IsBigInt(); }

  static_assert(std::is_same<bigint::digit_t, BigIntBase::digit_t>::value,
                "We must be able to call BigInt library functions");

  NEVER_READ_ONLY_SPACE

  OBJECT_CONSTRUCTORS(MutableBigInt, FreshlyAllocatedBigInt);
};

OBJECT_CONSTRUCTORS_IMPL(MutableBigInt, FreshlyAllocatedBigInt)
NEVER_READ_ONLY_SPACE_IMPL(MutableBigInt)

#include "src/base/platform/wrappers.h"
#include "src/objects/object-macros-undef.h"

bigint::Digits GetDigits(BigIntBase bigint) {
  return bigint::Digits(
      reinterpret_cast<bigint::digit_t*>(
          bigint.ptr() + BigIntBase::kDigitsOffset - kHeapObjectTag),
      bigint.length());
}
bigint::Digits GetDigits(Handle<BigIntBase> bigint) {
  return GetDigits(*bigint);
}

bigint::RWDigits GetRWDigits(MutableBigInt bigint) {
  return bigint::RWDigits(
      reinterpret_cast<bigint::digit_t*>(
          bigint.ptr() + BigIntBase::kDigitsOffset - kHeapObjectTag),
      bigint.length());
}
bigint::RWDigits GetRWDigits(Handle<MutableBigInt> bigint) {
  return GetRWDigits(*bigint);
}

template <typename T, typename Isolate>
MaybeHandle<T> ThrowBigIntTooBig(Isolate* isolate) {
  // If the result of a BigInt computation is truncated to 64 bit, Turbofan
  // can sometimes truncate intermediate results already, which can prevent
  // those from exceeding the maximum length, effectively preventing a
  // RangeError from being thrown. As this is a performance optimization, this
  // behavior is accepted. To prevent the correctness fuzzer from detecting this
  // difference, we crash the program.
  if (FLAG_correctness_fuzzer_suppressions) {
    FATAL("Aborting on invalid BigInt length");
  }
  THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kBigIntTooBig), T);
}

template <typename IsolateT>
MaybeHandle<MutableBigInt> MutableBigInt::New(IsolateT* isolate, int length,
                                              AllocationType allocation) {
  if (length > BigInt::kMaxLength) {
    return ThrowBigIntTooBig<MutableBigInt>(isolate);
  }
  Handle<MutableBigInt> result =
      Cast(isolate->factory()->NewBigInt(length, allocation));
  result->initialize_bitfield(false, length);
#if DEBUG
  result->InitializeDigits(length, 0xBF);
#endif
  return result;
}

Handle<BigInt> MutableBigInt::NewFromInt(Isolate* isolate, int value) {
  if (value == 0) return Zero(isolate);
  Handle<MutableBigInt> result = Cast(isolate->factory()->NewBigInt(1));
  bool sign = value < 0;
  result->initialize_bitfield(sign, 1);
  if (!sign) {
    result->set_digit(0, value);
  } else {
    if (value == kMinInt) {
      STATIC_ASSERT(kMinInt == -kMaxInt - 1);
      result->set_digit(0, static_cast<BigInt::digit_t>(kMaxInt) + 1);
    } else {
      result->set_digit(0, -value);
    }
  }
  return MakeImmutable(result);
}

Handle<BigInt> MutableBigInt::NewFromDouble(Isolate* isolate, double value) {
  DCHECK_EQ(value, std::floor(value));
  if (value == 0) return Zero(isolate);

  bool sign = value < 0;  // -0 was already handled above.
  uint64_t double_bits = bit_cast<uint64_t>(value);
  int raw_exponent =
      static_cast<int>(double_bits >> base::Double::kPhysicalSignificandSize) &
      0x7FF;
  DCHECK_NE(raw_exponent, 0x7FF);
  DCHECK_GE(raw_exponent, 0x3FF);
  int exponent = raw_exponent - 0x3FF;
  int digits = exponent / kDigitBits + 1;
  Handle<MutableBigInt> result = Cast(isolate->factory()->NewBigInt(digits));
  result->initialize_bitfield(sign, digits);

  // We construct a BigInt from the double {value} by shifting its mantissa
  // according to its exponent and mapping the bit pattern onto digits.
  //
  //               <----------- bitlength = exponent + 1 ----------->
  //                <----- 52 ------> <------ trailing zeroes ------>
  // mantissa:     1yyyyyyyyyyyyyyyyy 0000000000000000000000000000000
  // digits:    0001xxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
  //                <-->          <------>
  //          msd_topbit         kDigitBits
  //
  uint64_t mantissa =
      (double_bits & base::Double::kSignificandMask) | base::Double::kHiddenBit;
  const int kMantissaTopBit = base::Double::kSignificandSize - 1;  // 0-indexed.
  // 0-indexed position of most significant bit in the most significant digit.
  int msd_topbit = exponent % kDigitBits;
  // Number of unused bits in {mantissa}. We'll keep them shifted to the
  // left (i.e. most significant part) of the underlying uint64_t.
  int remaining_mantissa_bits = 0;
  // Next digit under construction.
  digit_t digit;

  // First, build the MSD by shifting the mantissa appropriately.
  if (msd_topbit < kMantissaTopBit) {
    remaining_mantissa_bits = kMantissaTopBit - msd_topbit;
    digit = mantissa >> remaining_mantissa_bits;
    mantissa = mantissa << (64 - remaining_mantissa_bits);
  } else {
    DCHECK_GE(msd_topbit, kMantissaTopBit);
    digit = mantissa << (msd_topbit - kMantissaTopBit);
    mantissa = 0;
  }
  result->set_digit(digits - 1, digit);
  // Then fill in the rest of the digits.
  for (int digit_index = digits - 2; digit_index >= 0; digit_index--) {
    if (remaining_mantissa_bits > 0) {
      remaining_mantissa_bits -= kDigitBits;
      if (sizeof(digit) == 4) {
        digit = mantissa >> 32;
        mantissa = mantissa << 32;
      } else {
        DCHECK_EQ(sizeof(digit), 8);
        digit = mantissa;
        mantissa = 0;
      }
    } else {
      digit = 0;
    }
    result->set_digit(digit_index, digit);
  }
  return MakeImmutable(result);
}

Handle<MutableBigInt> MutableBigInt::Copy(Isolate* isolate,
                                          Handle<BigIntBase> source) {
  int length = source->length();
  // Allocating a BigInt of the same length as an existing BigInt cannot throw.
  Handle<MutableBigInt> result = New(isolate, length).ToHandleChecked();
  memcpy(reinterpret_cast<void*>(result->address() + BigIntBase::kHeaderSize),
         reinterpret_cast<void*>(source->address() + BigIntBase::kHeaderSize),
         BigInt::SizeFor(length) - BigIntBase::kHeaderSize);
  return result;
}

void MutableBigInt::InitializeDigits(int length, byte value) {
  memset(reinterpret_cast<void*>(ptr() + kDigitsOffset - kHeapObjectTag), value,
         length * kDigitSize);
}

MaybeHandle<BigInt> MutableBigInt::MakeImmutable(
    MaybeHandle<MutableBigInt> maybe) {
  Handle<MutableBigInt> result;
  if (!maybe.ToHandle(&result)) return MaybeHandle<BigInt>();
  return MakeImmutable(result);
}

template <typename IsolateT>
Handle<BigInt> MutableBigInt::MakeImmutable(Handle<MutableBigInt> result) {
  MutableBigInt::Canonicalize(*result);
  return Handle<BigInt>::cast(result);
}

void MutableBigInt::Canonicalize(MutableBigInt result) {
  // Check if we need to right-trim any leading zero-digits.
  int old_length = result.length();
  int new_length = old_length;
  while (new_length > 0 && result.digit(new_length - 1) == 0) new_length--;
  int to_trim = old_length - new_length;
  if (to_trim != 0) {
    int size_delta = to_trim * MutableBigInt::kDigitSize;
    Address new_end = result.address() + BigInt::SizeFor(new_length);
    Heap* heap = result.GetHeap();
    if (!heap->IsLargeObject(result)) {
      // We do not create a filler for objects in large object space.
      // TODO(hpayer): We should shrink the large object page if the size
      // of the object changed significantly.
      heap->CreateFillerObjectAt(new_end, size_delta, ClearRecordedSlots::kNo);
    }
    result.set_length(new_length, kReleaseStore);

    // Canonicalize -0n.
    if (new_length == 0) {
      result.set_sign(false);
      // TODO(jkummerow): If we cache a canonical 0n, return that here.
    }
  }
  DCHECK_IMPLIES(result.length() > 0,
                 result.digit(result.length() - 1) != 0);  // MSD is non-zero.
}

template <typename IsolateT>
Handle<BigInt> BigInt::Zero(IsolateT* isolate, AllocationType allocation) {
  return MutableBigInt::Zero(isolate, allocation);
}
template Handle<BigInt> BigInt::Zero(Isolate* isolate,
                                     AllocationType allocation);
template Handle<BigInt> BigInt::Zero(LocalIsolate* isolate,
                                     AllocationType allocation);

Handle<BigInt> BigInt::UnaryMinus(Isolate* isolate, Handle<BigInt> x) {
  // Special case: There is no -0n.
  if (x->is_zero()) {
    return x;
  }
  Handle<MutableBigInt> result = MutableBigInt::Copy(isolate, x);
  result->set_sign(!x->sign());
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::BitwiseNot(Isolate* isolate, Handle<BigInt> x) {
  MaybeHandle<MutableBigInt> result;
  if (x->sign()) {
    // ~(-x) == ~(~(x-1)) == x-1
    result = MutableBigInt::AbsoluteSubOne(isolate, x, x->length());
  } else {
    // ~x == -x-1 == -(x+1)
    result = MutableBigInt::AbsoluteAddOne(isolate, x, true);
  }
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::Exponentiate(Isolate* isolate, Handle<BigInt> base,
                                         Handle<BigInt> exponent) {
  // 1. If exponent is < 0, throw a RangeError exception.
  if (exponent->sign()) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kBigIntNegativeExponent),
                    BigInt);
  }
  // 2. If base is 0n and exponent is 0n, return 1n.
  if (exponent->is_zero()) {
    return MutableBigInt::NewFromInt(isolate, 1);
  }
  // 3. Return a BigInt representing the mathematical value of base raised
  //    to the power exponent.
  if (base->is_zero()) return base;
  if (base->length() == 1 && base->digit(0) == 1) {
    // (-1) ** even_number == 1.
    if (base->sign() && (exponent->digit(0) & 1) == 0) {
      return UnaryMinus(isolate, base);
    }
    // (-1) ** odd_number == -1; 1 ** anything == 1.
    return base;
  }
  // For all bases >= 2, very large exponents would lead to unrepresentable
  // results.
  STATIC_ASSERT(kMaxLengthBits < std::numeric_limits<digit_t>::max());
  if (exponent->length() > 1) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  digit_t exp_value = exponent->digit(0);
  if (exp_value == 1) return base;
  if (exp_value >= kMaxLengthBits) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  STATIC_ASSERT(kMaxLengthBits <= kMaxInt);
  int n = static_cast<int>(exp_value);
  if (base->length() == 1 && base->digit(0) == 2) {
    // Fast path for 2^n.
    int needed_digits = 1 + (n / kDigitBits);
    Handle<MutableBigInt> result;
    if (!MutableBigInt::New(isolate, needed_digits).ToHandle(&result)) {
      return MaybeHandle<BigInt>();
    }
    result->InitializeDigits(needed_digits);
    // All bits are zero. Now set the n-th bit.
    digit_t msd = static_cast<digit_t>(1) << (n % kDigitBits);
    result->set_digit(needed_digits - 1, msd);
    // Result is negative for odd powers of -2n.
    if (base->sign()) result->set_sign((n & 1) != 0);
    return MutableBigInt::MakeImmutable(result);
  }
  Handle<BigInt> result;
  Handle<BigInt> running_square = base;
  // This implicitly sets the result's sign correctly.
  if (n & 1) result = base;
  n >>= 1;
  for (; n != 0; n >>= 1) {
    MaybeHandle<BigInt> maybe_result =
        Multiply(isolate, running_square, running_square);
    if (!maybe_result.ToHandle(&running_square)) return maybe_result;
    if (n & 1) {
      if (result.is_null()) {
        result = running_square;
      } else {
        maybe_result = Multiply(isolate, result, running_square);
        if (!maybe_result.ToHandle(&result)) return maybe_result;
      }
    }
  }
  return result;
}

MaybeHandle<BigInt> BigInt::Multiply(Isolate* isolate, Handle<BigInt> x,
                                     Handle<BigInt> y) {
  if (x->is_zero()) return x;
  if (y->is_zero()) return y;
  int result_length = bigint::MultiplyResultLength(GetDigits(x), GetDigits(y));
  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&result)) {
    return MaybeHandle<BigInt>();
  }
  DisallowGarbageCollection no_gc;
  bigint::Status status = isolate->bigint_processor()->Multiply(
      GetRWDigits(result), GetDigits(x), GetDigits(y));
  if (status == bigint::Status::kInterrupted) {
    AllowGarbageCollection terminating_anyway;
    isolate->TerminateExecution();
    return {};
  }
  result->set_sign(x->sign() != y->sign());
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::Divide(Isolate* isolate, Handle<BigInt> x,
                                   Handle<BigInt> y) {
  // 1. If y is 0n, throw a RangeError exception.
  if (y->is_zero()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kBigIntDivZero),
                    BigInt);
  }
  // 2. Let quotient be the mathematical value of x divided by y.
  // 3. Return a BigInt representing quotient rounded towards 0 to the next
  //    integral value.
  if (bigint::Compare(GetDigits(x), GetDigits(y)) < 0) {
    return Zero(isolate);
  }
  bool result_sign = x->sign() != y->sign();
  if (y->length() == 1 && y->digit(0) == 1) {
    return result_sign == x->sign() ? x : UnaryMinus(isolate, x);
  }
  Handle<MutableBigInt> quotient;
  int result_length = bigint::DivideResultLength(GetDigits(x), GetDigits(y));
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&quotient)) {
    return {};
  }
  DisallowGarbageCollection no_gc;
  bigint::Status status = isolate->bigint_processor()->Divide(
      GetRWDigits(quotient), GetDigits(x), GetDigits(y));
  if (status == bigint::Status::kInterrupted) {
    AllowGarbageCollection terminating_anyway;
    isolate->TerminateExecution();
    return {};
  }
  quotient->set_sign(result_sign);
  return MutableBigInt::MakeImmutable(quotient);
}

MaybeHandle<BigInt> BigInt::Remainder(Isolate* isolate, Handle<BigInt> x,
                                      Handle<BigInt> y) {
  // 1. If y is 0n, throw a RangeError exception.
  if (y->is_zero()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kBigIntDivZero),
                    BigInt);
  }
  // 2. Return the BigInt representing x modulo y.
  // See https://github.com/tc39/proposal-bigint/issues/84 though.
  if (bigint::Compare(GetDigits(x), GetDigits(y)) < 0) return x;
  if (y->length() == 1 && y->digit(0) == 1) return Zero(isolate);
  Handle<MutableBigInt> remainder;
  int result_length = bigint::ModuloResultLength(GetDigits(y));
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&remainder)) {
    return {};
  }
  DisallowGarbageCollection no_gc;
  bigint::Status status = isolate->bigint_processor()->Modulo(
      GetRWDigits(remainder), GetDigits(x), GetDigits(y));
  if (status == bigint::Status::kInterrupted) {
    AllowGarbageCollection terminating_anyway;
    isolate->TerminateExecution();
    return {};
  }
  remainder->set_sign(x->sign());
  return MutableBigInt::MakeImmutable(remainder);
}

MaybeHandle<BigInt> BigInt::Add(Isolate* isolate, Handle<BigInt> x,
                                Handle<BigInt> y) {
  if (x->is_zero()) return y;
  if (y->is_zero()) return x;
  bool xsign = x->sign();
  bool ysign = y->sign();
  int result_length =
      bigint::AddSignedResultLength(x->length(), y->length(), xsign == ysign);
  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&result)) {
    // Allocation fails when {result_length} exceeds the max BigInt size.
    return {};
  }
  DisallowGarbageCollection no_gc;
  bool result_sign = bigint::AddSigned(GetRWDigits(result), GetDigits(x), xsign,
                                       GetDigits(y), ysign);
  result->set_sign(result_sign);
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::Subtract(Isolate* isolate, Handle<BigInt> x,
                                     Handle<BigInt> y) {
  if (y->is_zero()) return x;
  if (x->is_zero()) return UnaryMinus(isolate, y);
  bool xsign = x->sign();
  bool ysign = y->sign();
  int result_length = bigint::SubtractSignedResultLength(
      x->length(), y->length(), xsign == ysign);
  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&result)) {
    // Allocation fails when {result_length} exceeds the max BigInt size.
    return {};
  }
  DisallowGarbageCollection no_gc;
  bool result_sign = bigint::SubtractSigned(GetRWDigits(result), GetDigits(x),
                                            xsign, GetDigits(y), ysign);
  result->set_sign(result_sign);
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::LeftShift(Isolate* isolate, Handle<BigInt> x,
                                      Handle<BigInt> y) {
  if (y->is_zero() || x->is_zero()) return x;
  if (y->sign()) return MutableBigInt::RightShiftByAbsolute(isolate, x, y);
  return MutableBigInt::LeftShiftByAbsolute(isolate, x, y);
}

MaybeHandle<BigInt> BigInt::SignedRightShift(Isolate* isolate, Handle<BigInt> x,
                                             Handle<BigInt> y) {
  if (y->is_zero() || x->is_zero()) return x;
  if (y->sign()) return MutableBigInt::LeftShiftByAbsolute(isolate, x, y);
  return MutableBigInt::RightShiftByAbsolute(isolate, x, y);
}

MaybeHandle<BigInt> BigInt::UnsignedRightShift(Isolate* isolate,
                                               Handle<BigInt> x,
                                               Handle<BigInt> y) {
  THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kBigIntShr), BigInt);
}

namespace {

// Produces comparison result for {left_negative} == sign(x) != sign(y).
ComparisonResult UnequalSign(bool left_negative) {
  return left_negative ? ComparisonResult::kLessThan
                       : ComparisonResult::kGreaterThan;
}

// Produces result for |x| > |y|, with {both_negative} == sign(x) == sign(y);
ComparisonResult AbsoluteGreater(bool both_negative) {
  return both_negative ? ComparisonResult::kLessThan
                       : ComparisonResult::kGreaterThan;
}

// Produces result for |x| < |y|, with {both_negative} == sign(x) == sign(y).
ComparisonResult AbsoluteLess(bool both_negative) {
  return both_negative ? ComparisonResult::kGreaterThan
                       : ComparisonResult::kLessThan;
}

}  // namespace

// (Never returns kUndefined.)
ComparisonResult BigInt::CompareToBigInt(Handle<BigInt> x, Handle<BigInt> y) {
  bool x_sign = x->sign();
  if (x_sign != y->sign()) return UnequalSign(x_sign);

  int result = bigint::Compare(GetDigits(x), GetDigits(y));
  if (result > 0) return AbsoluteGreater(x_sign);
  if (result < 0) return AbsoluteLess(x_sign);
  return ComparisonResult::kEqual;
}

bool BigInt::EqualToBigInt(BigInt x, BigInt y) {
  if (x.sign() != y.sign()) return false;
  if (x.length() != y.length()) return false;
  for (int i = 0; i < x.length(); i++) {
    if (x.digit(i) != y.digit(i)) return false;
  }
  return true;
}

MaybeHandle<BigInt> BigInt::BitwiseAnd(Isolate* isolate, Handle<BigInt> x,
                                       Handle<BigInt> y) {
  return MutableBigInt::MakeImmutable(MutableBigInt::BitwiseAnd(isolate, x, y));
}

MaybeHandle<MutableBigInt> MutableBigInt::BitwiseAnd(Isolate* isolate,
                                                     Handle<BigInt> x,
                                                     Handle<BigInt> y) {
  if (!x->sign() && !y->sign()) {
    return AbsoluteAnd(isolate, x, y);
  } else if (x->sign() && y->sign()) {
    int result_length = std::max(x->length(), y->length()) + 1;
    // (-x) & (-y) == ~(x-1) & ~(y-1) == ~((x-1) | (y-1))
    // == -(((x-1) | (y-1)) + 1)
    Handle<MutableBigInt> result;
    if (!AbsoluteSubOne(isolate, x, result_length).ToHandle(&result)) {
      return MaybeHandle<MutableBigInt>();
    }
    Handle<MutableBigInt> y_1 = AbsoluteSubOne(isolate, y);
    result = AbsoluteOr(isolate, result, y_1, *result);
    return AbsoluteAddOne(isolate, result, true, *result);
  } else {
    DCHECK(x->sign() != y->sign());
    // Assume that x is the positive BigInt.
    if (x->sign()) std::swap(x, y);
    // x & (-y) == x & ~(y-1) == x &~ (y-1)
    Handle<MutableBigInt> y_1 = AbsoluteSubOne(isolate, y);
    return AbsoluteAndNot(isolate, x, y_1);
  }
}

MaybeHandle<BigInt> BigInt::BitwiseXor(Isolate* isolate, Handle<BigInt> x,
                                       Handle<BigInt> y) {
  return MutableBigInt::MakeImmutable(MutableBigInt::BitwiseXor(isolate, x, y));
}

MaybeHandle<MutableBigInt> MutableBigInt::BitwiseXor(Isolate* isolate,
                                                     Handle<BigInt> x,
                                                     Handle<BigInt> y) {
  if (!x->sign() && !y->sign()) {
    return AbsoluteXor(isolate, x, y);
  } else if (x->sign() && y->sign()) {
    int result_length = std::max(x->length(), y->length());
    // (-x) ^ (-y) == ~(x-1) ^ ~(y-1) == (x-1) ^ (y-1)
    Handle<MutableBigInt> result =
        AbsoluteSubOne(isolate, x, result_length).ToHandleChecked();
    Handle<MutableBigInt> y_1 = AbsoluteSubOne(isolate, y);
    return AbsoluteXor(isolate, result, y_1, *result);
  } else {
    DCHECK(x->sign() != y->sign());
    int result_length = std::max(x->length(), y->length()) + 1;
    // Assume that x is the positive BigInt.
    if (x->sign()) std::swap(x, y);
    // x ^ (-y) == x ^ ~(y-1) == ~(x ^ (y-1)) == -((x ^ (y-1)) + 1)
    Handle<MutableBigInt> result;
    if (!AbsoluteSubOne(isolate, y, result_length).ToHandle(&result)) {
      return MaybeHandle<MutableBigInt>();
    }
    result = AbsoluteXor(isolate, result, x, *result);
    return AbsoluteAddOne(isolate, result, true, *result);
  }
}

MaybeHandle<BigInt> BigInt::BitwiseOr(Isolate* isolate, Handle<BigInt> x,
                                      Handle<BigInt> y) {
  return MutableBigInt::MakeImmutable(MutableBigInt::BitwiseOr(isolate, x, y));
}

MaybeHandle<MutableBigInt> MutableBigInt::BitwiseOr(Isolate* isolate,
                                                    Handle<BigInt> x,
                                                    Handle<BigInt> y) {
  int result_length = std::max(x->length(), y->length());
  if (!x->sign() && !y->sign()) {
    return AbsoluteOr(isolate, x, y);
  } else if (x->sign() && y->sign()) {
    // (-x) | (-y) == ~(x-1) | ~(y-1) == ~((x-1) & (y-1))
    // == -(((x-1) & (y-1)) + 1)
    Handle<MutableBigInt> result =
        AbsoluteSubOne(isolate, x, result_length).ToHandleChecked();
    Handle<MutableBigInt> y_1 = AbsoluteSubOne(isolate, y);
    result = AbsoluteAnd(isolate, result, y_1, *result);
    return AbsoluteAddOne(isolate, result, true, *result);
  } else {
    DCHECK(x->sign() != y->sign());
    // Assume that x is the positive BigInt.
    if (x->sign()) std::swap(x, y);
    // x | (-y) == x | ~(y-1) == ~((y-1) &~ x) == -(((y-1) &~ x) + 1)
    Handle<MutableBigInt> result =
        AbsoluteSubOne(isolate, y, result_length).ToHandleChecked();
    result = AbsoluteAndNot(isolate, result, x, *result);
    return AbsoluteAddOne(isolate, result, true, *result);
  }
}

MaybeHandle<BigInt> BigInt::Increment(Isolate* isolate, Handle<BigInt> x) {
  if (x->sign()) {
    Handle<MutableBigInt> result = MutableBigInt::AbsoluteSubOne(isolate, x);
    result->set_sign(true);
    return MutableBigInt::MakeImmutable(result);
  } else {
    return MutableBigInt::MakeImmutable(
        MutableBigInt::AbsoluteAddOne(isolate, x, false));
  }
}

MaybeHandle<BigInt> BigInt::Decrement(Isolate* isolate, Handle<BigInt> x) {
  MaybeHandle<MutableBigInt> result;
  if (x->sign()) {
    result = MutableBigInt::AbsoluteAddOne(isolate, x, true);
  } else if (x->is_zero()) {
    // TODO(jkummerow): Consider caching a canonical -1n BigInt.
    return MutableBigInt::NewFromInt(isolate, -1);
  } else {
    result = MutableBigInt::AbsoluteSubOne(isolate, x);
  }
  return MutableBigInt::MakeImmutable(result);
}

Maybe<ComparisonResult> BigInt::CompareToString(Isolate* isolate,
                                                Handle<BigInt> x,
                                                Handle<String> y) {
  // a. Let ny be StringToBigInt(y);
  MaybeHandle<BigInt> maybe_ny = StringToBigInt(isolate, y);
  // b. If ny is NaN, return undefined.
  Handle<BigInt> ny;
  if (!maybe_ny.ToHandle(&ny)) {
    if (isolate->has_pending_exception()) {
      return Nothing<ComparisonResult>();
    } else {
      return Just(ComparisonResult::kUndefined);
    }
  }
  // c. Return BigInt::lessThan(x, ny).
  return Just(CompareToBigInt(x, ny));
}

Maybe<bool> BigInt::EqualToString(Isolate* isolate, Handle<BigInt> x,
                                  Handle<String> y) {
  // a. Let n be StringToBigInt(y).
  MaybeHandle<BigInt> maybe_n = StringToBigInt(isolate, y);
  // b. If n is NaN, return false.
  Handle<BigInt> n;
  if (!maybe_n.ToHandle(&n)) {
    if (isolate->has_pending_exception()) {
      return Nothing<bool>();
    } else {
      return Just(false);
    }
  }
  // c. Return the result of x == n.
  return Just(EqualToBigInt(*x, *n));
}

bool BigInt::EqualToNumber(Handle<BigInt> x, Handle<Object> y) {
  DCHECK(y->IsNumber());
  // a. If x or y are any of NaN, +∞, or -∞, return false.
  // b. If the mathematical value of x is equal to the mathematical value of y,
  //    return true, otherwise return false.
  if (y->IsSmi()) {
    int value = Smi::ToInt(*y);
    if (value == 0) return x->is_zero();
    // Any multi-digit BigInt is bigger than a Smi.
    STATIC_ASSERT(sizeof(digit_t) >= sizeof(value));
    return (x->length() == 1) && (x->sign() == (value < 0)) &&
           (x->digit(0) ==
            static_cast<digit_t>(std::abs(static_cast<int64_t>(value))));
  }
  DCHECK(y->IsHeapNumber());
  double value = Handle<HeapNumber>::cast(y)->value();
  return CompareToDouble(x, value) == ComparisonResult::kEqual;
}

ComparisonResult BigInt::CompareToNumber(Handle<BigInt> x, Handle<Object> y) {
  DCHECK(y->IsNumber());
  if (y->IsSmi()) {
    bool x_sign = x->sign();
    int y_value = Smi::ToInt(*y);
    bool y_sign = (y_value < 0);
    if (x_sign != y_sign) return UnequalSign(x_sign);

    if (x->is_zero()) {
      DCHECK(!y_sign);
      return y_value == 0 ? ComparisonResult::kEqual
                          : ComparisonResult::kLessThan;
    }
    // Any multi-digit BigInt is bigger than a Smi.
    STATIC_ASSERT(sizeof(digit_t) >= sizeof(y_value));
    if (x->length() > 1) return AbsoluteGreater(x_sign);

    digit_t abs_value = std::abs(static_cast<int64_t>(y_value));
    digit_t x_digit = x->digit(0);
    if (x_digit > abs_value) return AbsoluteGreater(x_sign);
    if (x_digit < abs_value) return AbsoluteLess(x_sign);
    return ComparisonResult::kEqual;
  }
  DCHECK(y->IsHeapNumber());
  double value = Handle<HeapNumber>::cast(y)->value();
  return CompareToDouble(x, value);
}

ComparisonResult BigInt::CompareToDouble(Handle<BigInt> x, double y) {
  if (std::isnan(y)) return ComparisonResult::kUndefined;
  if (y == V8_INFINITY) return ComparisonResult::kLessThan;
  if (y == -V8_INFINITY) return ComparisonResult::kGreaterThan;
  bool x_sign = x->sign();
  // Note that this is different from the double's sign bit for -0. That's
  // intentional because -0 must be treated like 0.
  bool y_sign = (y < 0);
  if (x_sign != y_sign) return UnequalSign(x_sign);
  if (y == 0) {
    DCHECK(!x_sign);
    return x->is_zero() ? ComparisonResult::kEqual
                        : ComparisonResult::kGreaterThan;
  }
  if (x->is_zero()) {
    DCHECK(!y_sign);
    return ComparisonResult::kLessThan;
  }
  uint64_t double_bits = bit_cast<uint64_t>(y);
  int raw_exponent =
      static_cast<int>(double_bits >> base::Double::kPhysicalSignificandSize) &
      0x7FF;
  uint64_t mantissa = double_bits & base::Double::kSignificandMask;
  // Non-finite doubles are handled above.
  DCHECK_NE(raw_exponent, 0x7FF);
  int exponent = raw_exponent - 0x3FF;
  if (exponent < 0) {
    // The absolute value of the double is less than 1. Only 0n has an
    // absolute value smaller than that, but we've already covered that case.
    DCHECK(!x->is_zero());
    return AbsoluteGreater(x_sign);
  }
  int x_length = x->length();
  digit_t x_msd = x->digit(x_length - 1);
  int msd_leading_zeros = base::bits::CountLeadingZeros(x_msd);
  int x_bitlength = x_length * kDigitBits - msd_leading_zeros;
  int y_bitlength = exponent + 1;
  if (x_bitlength < y_bitlength) return AbsoluteLess(x_sign);
  if (x_bitlength > y_bitlength) return AbsoluteGreater(x_sign);

  // At this point, we know that signs and bit lengths (i.e. position of
  // the most significant bit in exponent-free representation) are identical.
  // {x} is not zero, {y} is finite and not denormal.
  // Now we virtually convert the double to an integer by shifting its
  // mantissa according to its exponent, so it will align with the BigInt {x},
  // and then we compare them bit for bit until we find a difference or the
  // least significant bit.
  //                    <----- 52 ------> <-- virtual trailing zeroes -->
  // y / mantissa:     1yyyyyyyyyyyyyyyyy 0000000000000000000000000000000
  // x / digits:    0001xxxx xxxxxxxx xxxxxxxx ...
  //                    <-->          <------>
  //              msd_topbit         kDigitBits
  //
  mantissa |= base::Double::kHiddenBit;
  const int kMantissaTopBit = 52;  // 0-indexed.
  // 0-indexed position of {x}'s most significant bit within the {msd}.
  int msd_topbit = kDigitBits - 1 - msd_leading_zeros;
  DCHECK_EQ(msd_topbit, (x_bitlength - 1) % kDigitBits);
  // Shifted chunk of {mantissa} for comparing with {digit}.
  digit_t compare_mantissa;
  // Number of unprocessed bits in {mantissa}. We'll keep them shifted to
  // the left (i.e. most significant part) of the underlying uint64_t.
  int remaining_mantissa_bits = 0;

  // First, compare the most significant digit against the beginning of
  // the mantissa.
  if (msd_topbit < kMantissaTopBit) {
    remaining_mantissa_bits = (kMantissaTopBit - msd_topbit);
    compare_mantissa = mantissa >> remaining_mantissa_bits;
    mantissa = mantissa << (64 - remaining_mantissa_bits);
  } else {
    DCHECK_GE(msd_topbit, kMantissaTopBit);
    compare_mantissa = mantissa << (msd_topbit - kMantissaTopBit);
    mantissa = 0;
  }
  if (x_msd > compare_mantissa) return AbsoluteGreater(x_sign);
  if (x_msd < compare_mantissa) return AbsoluteLess(x_sign);

  // Then, compare additional digits against any remaining mantissa bits.
  for (int digit_index = x_length - 2; digit_index >= 0; digit_index--) {
    if (remaining_mantissa_bits > 0) {
      remaining_mantissa_bits -= kDigitBits;
      if (sizeof(mantissa) != sizeof(x_msd)) {
        compare_mantissa = mantissa >> (64 - kDigitBits);
        // "& 63" to appease compilers. kDigitBits is 32 here anyway.
        mantissa = mantissa << (kDigitBits & 63);
      } else {
        compare_mantissa = mantissa;
        mantissa = 0;
      }
    } else {
      compare_mantissa = 0;
    }
    digit_t digit = x->digit(digit_index);
    if (digit > compare_mantissa) return AbsoluteGreater(x_sign);
    if (digit < compare_mantissa) return AbsoluteLess(x_sign);
  }

  // Integer parts are equal; check whether {y} has a fractional part.
  if (mantissa != 0) {
    DCHECK_GT(remaining_mantissa_bits, 0);
    return AbsoluteLess(x_sign);
  }
  return ComparisonResult::kEqual;
}

MaybeHandle<String> BigInt::ToString(Isolate* isolate, Handle<BigInt> bigint,
                                     int radix, ShouldThrow should_throw) {
  if (bigint->is_zero()) {
    return isolate->factory()->zero_string();
  }
  const bool sign = bigint->sign();
  int chars_allocated;
  int chars_written;
  Handle<SeqOneByteString> result;
  if (bigint->length() == 1 && radix == 10) {
    // Fast path for the most common case, to avoid call/dispatch overhead.
    // The logic is the same as what the full implementation does below,
    // just inlined and specialized for the preconditions.
    // Microbenchmarks rejoice!
    digit_t digit = bigint->digit(0);
    int bit_length = kDigitBits - base::bits::CountLeadingZeros(digit);
    constexpr int kShift = 7;
    // This is Math.log2(10) * (1 << kShift), scaled just far enough to
    // make the computations below always precise (after rounding).
    constexpr int kShiftedBitsPerChar = 425;
    chars_allocated = (bit_length << kShift) / kShiftedBitsPerChar + 1 + sign;
    result = isolate->factory()
                 ->NewRawOneByteString(chars_allocated)
                 .ToHandleChecked();
    DisallowGarbageCollection no_gc;
    uint8_t* start = result->GetChars(no_gc);
    uint8_t* out = start + chars_allocated;
    while (digit != 0) {
      *(--out) = '0' + (digit % 10);
      digit /= 10;
    }
    if (sign) *(--out) = '-';
    if (out == start) {
      chars_written = chars_allocated;
    } else {
      DCHECK_LT(start, out);
      // The result is one character shorter than predicted. This is
      // unavoidable, e.g. a 4-bit BigInt can be as big as "10" or as small as
      // "9", so we must allocate 2 characters for it, and will only later find
      // out whether all characters were used.
      chars_written = chars_allocated - static_cast<int>(out - start);
      std::memmove(start, out, chars_written);
    }
  } else {
    // Generic path, handles anything.
    DCHECK(radix >= 2 && radix <= 36);
    chars_allocated =
        bigint::ToStringResultLength(GetDigits(bigint), radix, sign);
    if (chars_allocated > String::kMaxLength) {
      if (should_throw == kThrowOnError) {
        THROW_NEW_ERROR(isolate, NewInvalidStringLengthError(), String);
      } else {
        return {};
      }
    }
    result = isolate->factory()
                 ->NewRawOneByteString(chars_allocated)
                 .ToHandleChecked();
    chars_written = chars_allocated;
    DisallowGarbageCollection no_gc;
    char* characters = reinterpret_cast<char*>(result->GetChars(no_gc));
    bigint::Status status = isolate->bigint_processor()->ToString(
        characters, &chars_written, GetDigits(bigint), radix, sign);
    if (status == bigint::Status::kInterrupted) {
      AllowGarbageCollection terminating_anyway;
      isolate->TerminateExecution();
      return {};
    }
  }

  // Right-trim any over-allocation (which can happen due to conservative
  // estimates).
  if (chars_written < chars_allocated) {
    result->set_length(chars_written, kReleaseStore);
    int string_size = SeqOneByteString::SizeFor(chars_allocated);
    int needed_size = SeqOneByteString::SizeFor(chars_written);
    if (needed_size < string_size && !isolate->heap()->IsLargeObject(*result)) {
      Address new_end = result->address() + needed_size;
      isolate->heap()->CreateFillerObjectAt(
          new_end, (string_size - needed_size), ClearRecordedSlots::kNo);
    }
  }
#if DEBUG
  // Verify that all characters have been written.
  DCHECK(result->length() == chars_written);
  DisallowGarbageCollection no_gc;
  uint8_t* chars = result->GetChars(no_gc);
  for (int i = 0; i < chars_written; i++) {
    DCHECK_NE(chars[i], bigint::kStringZapValue);
  }
#endif
  return result;
}

MaybeHandle<BigInt> BigInt::FromNumber(Isolate* isolate,
                                       Handle<Object> number) {
  DCHECK(number->IsNumber());
  if (number->IsSmi()) {
    return MutableBigInt::NewFromInt(isolate, Smi::ToInt(*number));
  }
  double value = HeapNumber::cast(*number).value();
  if (!std::isfinite(value) || (DoubleToInteger(value) != value)) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kBigIntFromNumber, number),
                    BigInt);
  }
  return MutableBigInt::NewFromDouble(isolate, value);
}

MaybeHandle<BigInt> BigInt::FromObject(Isolate* isolate, Handle<Object> obj) {
  if (obj->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, obj,
        JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(obj),
                                ToPrimitiveHint::kNumber),
        BigInt);
  }

  if (obj->IsBoolean()) {
    return MutableBigInt::NewFromInt(isolate, obj->BooleanValue(isolate));
  }
  if (obj->IsBigInt()) {
    return Handle<BigInt>::cast(obj);
  }
  if (obj->IsString()) {
    Handle<BigInt> n;
    if (!StringToBigInt(isolate, Handle<String>::cast(obj)).ToHandle(&n)) {
      if (isolate->has_pending_exception()) {
        return MaybeHandle<BigInt>();
      } else {
        THROW_NEW_ERROR(isolate,
                        NewSyntaxError(MessageTemplate::kBigIntFromObject, obj),
                        BigInt);
      }
    }
    return n;
  }

  THROW_NEW_ERROR(
      isolate, NewTypeError(MessageTemplate::kBigIntFromObject, obj), BigInt);
}

Handle<Object> BigInt::ToNumber(Isolate* isolate, Handle<BigInt> x) {
  if (x->is_zero()) return Handle<Smi>(Smi::zero(), isolate);
  if (x->length() == 1 && x->digit(0) < Smi::kMaxValue) {
    int value = static_cast<int>(x->digit(0));
    if (x->sign()) value = -value;
    return Handle<Smi>(Smi::FromInt(value), isolate);
  }
  double result = MutableBigInt::ToDouble(x);
  return isolate->factory()->NewHeapNumber(result);
}

double MutableBigInt::ToDouble(Handle<BigIntBase> x) {
  if (x->is_zero()) return 0.0;
  int x_length = x->length();
  digit_t x_msd = x->digit(x_length - 1);
  int msd_leading_zeros = base::bits::CountLeadingZeros(x_msd);
  int x_bitlength = x_length * kDigitBits - msd_leading_zeros;
  if (x_bitlength > 1024) return x->sign() ? -V8_INFINITY : V8_INFINITY;
  uint64_t exponent = x_bitlength - 1;
  // We need the most significant bit shifted to the position of a double's
  // "hidden bit". We also need to hide that MSB, so we shift it out.
  uint64_t current_digit = x_msd;
  int digit_index = x_length - 1;
  int shift = msd_leading_zeros + 1 + (64 - kDigitBits);
  DCHECK_LE(1, shift);
  DCHECK_LE(shift, 64);
  uint64_t mantissa = (shift == 64) ? 0 : current_digit << shift;
  mantissa >>= 12;
  int mantissa_bits_unset = shift - 12;
  // If not all mantissa bits are defined yet, get more digits as needed.
  if (mantissa_bits_unset >= kDigitBits && digit_index > 0) {
    digit_index--;
    current_digit = static_cast<uint64_t>(x->digit(digit_index));
    mantissa |= (current_digit << (mantissa_bits_unset - kDigitBits));
    mantissa_bits_unset -= kDigitBits;
  }
  if (mantissa_bits_unset > 0 && digit_index > 0) {
    DCHECK_LT(mantissa_bits_unset, kDigitBits);
    digit_index--;
    current_digit = static_cast<uint64_t>(x->digit(digit_index));
    mantissa |= (current_digit >> (kDigitBits - mantissa_bits_unset));
    mantissa_bits_unset -= kDigitBits;
  }
  // If there are unconsumed digits left, we may have to round.
  Rounding rounding =
      DecideRounding(x, mantissa_bits_unset, digit_index, current_digit);
  if (rounding == kRoundUp || (rounding == kTie && (mantissa & 1) == 1)) {
    mantissa++;
    // Incrementing the mantissa can overflow the mantissa bits. In that case
    // the new mantissa will be all zero (plus hidden bit).
    if ((mantissa >> base::Double::kPhysicalSignificandSize) != 0) {
      mantissa = 0;
      exponent++;
      // Incrementing the exponent can overflow too.
      if (exponent > 1023) {
        return x->sign() ? -V8_INFINITY : V8_INFINITY;
      }
    }
  }
  // Assemble the result.
  uint64_t sign_bit = x->sign() ? (static_cast<uint64_t>(1) << 63) : 0;
  exponent = (exponent + 0x3FF) << base::Double::kPhysicalSignificandSize;
  uint64_t double_bits = sign_bit | exponent | mantissa;
  return bit_cast<double>(double_bits);
}

// This is its own function to simplify control flow. The meaning of the
// parameters is defined by {ToDouble}'s local variable usage.
MutableBigInt::Rounding MutableBigInt::DecideRounding(Handle<BigIntBase> x,
                                                      int mantissa_bits_unset,
                                                      int digit_index,
                                                      uint64_t current_digit) {
  if (mantissa_bits_unset > 0) return kRoundDown;
  int top_unconsumed_bit;
  if (mantissa_bits_unset < 0) {
    // There are unconsumed bits in {current_digit}.
    top_unconsumed_bit = -mantissa_bits_unset - 1;
  } else {
    DCHECK_EQ(mantissa_bits_unset, 0);
    // {current_digit} fit the mantissa exactly; look at the next digit.
    if (digit_index == 0) return kRoundDown;
    digit_index--;
    current_digit = static_cast<uint64_t>(x->digit(digit_index));
    top_unconsumed_bit = kDigitBits - 1;
  }
  // If the most significant remaining bit is 0, round down.
  uint64_t bitmask = static_cast<uint64_t>(1) << top_unconsumed_bit;
  if ((current_digit & bitmask) == 0) {
    return kRoundDown;
  }
  // If any other remaining bit is set, round up.
  bitmask -= 1;
  if ((current_digit & bitmask) != 0) return kRoundUp;
  while (digit_index > 0) {
    digit_index--;
    if (x->digit(digit_index) != 0) return kRoundUp;
  }
  return kTie;
}

void BigInt::BigIntShortPrint(std::ostream& os) {
  if (sign()) os << "-";
  int len = length();
  if (len == 0) {
    os << "0";
    return;
  }
  if (len > 1) os << "...";
  os << digit(0);
}

// Internal helpers.

// Adds 1 to the absolute value of {x} and sets the result's sign to {sign}.
// {result_storage} is optional; if present, it will be used to store the
// result, otherwise a new BigInt will be allocated for the result.
// {result_storage} and {x} may refer to the same BigInt for in-place
// modification.
MaybeHandle<MutableBigInt> MutableBigInt::AbsoluteAddOne(
    Isolate* isolate, Handle<BigIntBase> x, bool sign,
    MutableBigInt result_storage) {
  int input_length = x->length();
  // The addition will overflow into a new digit if all existing digits are
  // at maximum.
  bool will_overflow = true;
  for (int i = 0; i < input_length; i++) {
    if (!digit_ismax(x->digit(i))) {
      will_overflow = false;
      break;
    }
  }
  int result_length = input_length + will_overflow;
  Handle<MutableBigInt> result(result_storage, isolate);
  if (result_storage.is_null()) {
    if (!New(isolate, result_length).ToHandle(&result)) {
      return MaybeHandle<MutableBigInt>();
    }
  } else {
    DCHECK(result->length() == result_length);
  }
  digit_t carry = 1;
  for (int i = 0; i < input_length; i++) {
    digit_t new_carry = 0;
    result->set_digit(i, digit_add(x->digit(i), carry, &new_carry));
    carry = new_carry;
  }
  if (result_length > input_length) {
    result->set_digit(input_length, carry);
  } else {
    DCHECK_EQ(carry, 0);
  }
  result->set_sign(sign);
  return result;
}

// Subtracts 1 from the absolute value of {x}. {x} must not be zero.
Handle<MutableBigInt> MutableBigInt::AbsoluteSubOne(Isolate* isolate,
                                                    Handle<BigIntBase> x) {
  DCHECK(!x->is_zero());
  // Requesting a result length identical to an existing BigInt's length
  // cannot overflow the limit.
  return AbsoluteSubOne(isolate, x, x->length()).ToHandleChecked();
}

// Like the above, but you can specify that the allocated result should have
// length {result_length}, which must be at least as large as {x->length()}.
MaybeHandle<MutableBigInt> MutableBigInt::AbsoluteSubOne(Isolate* isolate,
                                                         Handle<BigIntBase> x,
                                                         int result_length) {
  DCHECK(!x->is_zero());
  DCHECK(result_length >= x->length());
  Handle<MutableBigInt> result;
  if (!New(isolate, result_length).ToHandle(&result)) {
    return MaybeHandle<MutableBigInt>();
  }
  int length = x->length();
  digit_t borrow = 1;
  for (int i = 0; i < length; i++) {
    digit_t new_borrow = 0;
    result->set_digit(i, digit_sub(x->digit(i), borrow, &new_borrow));
    borrow = new_borrow;
  }
  DCHECK_EQ(borrow, 0);
  for (int i = length; i < result_length; i++) {
    result->set_digit(i, borrow);
  }
  return result;
}

// Helper for Absolute{And,AndNot,Or,Xor}.
// Performs the given binary {op} on digit pairs of {x} and {y}; when the
// end of the shorter of the two is reached, {extra_digits} configures how
// remaining digits in the longer input (if {symmetric} == kSymmetric, in
// {x} otherwise) are handled: copied to the result or ignored.
// If {result_storage} is non-nullptr, it will be used for the result and
// any extra digits in it will be zeroed out, otherwise a new BigInt (with
// the same length as the longer input) will be allocated.
// {result_storage} may alias {x} or {y} for in-place modification.
// Example:
//              y:             [ y2 ][ y1 ][ y0 ]
//              x:       [ x3 ][ x2 ][ x1 ][ x0 ]
//                          |     |     |     |
//                      (kCopy)  (op)  (op)  (op)
//                          |     |     |     |
//                          v     v     v     v
// result_storage: [  0 ][ x3 ][ r2 ][ r1 ][ r0 ]
inline Handle<MutableBigInt> MutableBigInt::AbsoluteBitwiseOp(
    Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
    MutableBigInt result_storage, ExtraDigitsHandling extra_digits,
    SymmetricOp symmetric, const std::function<digit_t(digit_t, digit_t)>& op) {
  int x_length = x->length();
  int y_length = y->length();
  int num_pairs = y_length;
  if (x_length < y_length) {
    num_pairs = x_length;
    if (symmetric == kSymmetric) {
      std::swap(x, y);
      std::swap(x_length, y_length);
    }
  }
  DCHECK(num_pairs == std::min(x_length, y_length));
  Handle<MutableBigInt> result(result_storage, isolate);
  int result_length = extra_digits == kCopy ? x_length : num_pairs;
  if (result_storage.is_null()) {
    result = New(isolate, result_length).ToHandleChecked();
  } else {
    DCHECK(result_storage.length() >= result_length);
    result_length = result_storage.length();
  }
  int i = 0;
  for (; i < num_pairs; i++) {
    result->set_digit(i, op(x->digit(i), y->digit(i)));
  }
  if (extra_digits == kCopy) {
    for (; i < x_length; i++) {
      result->set_digit(i, x->digit(i));
    }
  }
  for (; i < result_length; i++) {
    result->set_digit(i, 0);
  }
  return result;
}

// If {result_storage} is non-nullptr, it will be used for the result,
// otherwise a new BigInt of appropriate length will be allocated.
// {result_storage} may alias {x} or {y} for in-place modification.
Handle<MutableBigInt> MutableBigInt::AbsoluteAnd(Isolate* isolate,
                                                 Handle<BigIntBase> x,
                                                 Handle<BigIntBase> y,
                                                 MutableBigInt result_storage) {
  return AbsoluteBitwiseOp(isolate, x, y, result_storage, kSkip, kSymmetric,
                           [](digit_t a, digit_t b) { return a & b; });
}

// If {result_storage} is non-nullptr, it will be used for the result,
// otherwise a new BigInt of appropriate length will be allocated.
// {result_storage} may alias {x} or {y} for in-place modification.
Handle<MutableBigInt> MutableBigInt::AbsoluteAndNot(
    Isolate* isolate, Handle<BigIntBase> x, Handle<BigIntBase> y,
    MutableBigInt result_storage) {
  return AbsoluteBitwiseOp(isolate, x, y, result_storage, kCopy, kNotSymmetric,
                           [](digit_t a, digit_t b) { return a & ~b; });
}

// If {result_storage} is non-nullptr, it will be used for the result,
// otherwise a new BigInt of appropriate length will be allocated.
// {result_storage} may alias {x} or {y} for in-place modification.
Handle<MutableBigInt> MutableBigInt::AbsoluteOr(Isolate* isolate,
                                                Handle<BigIntBase> x,
                                                Handle<BigIntBase> y,
                                                MutableBigInt result_storage) {
  return AbsoluteBitwiseOp(isolate, x, y, result_storage, kCopy, kSymmetric,
                           [](digit_t a, digit_t b) { return a | b; });
}

// If {result_storage} is non-nullptr, it will be used for the result,
// otherwise a new BigInt of appropriate length will be allocated.
// {result_storage} may alias {x} or {y} for in-place modification.
Handle<MutableBigInt> MutableBigInt::AbsoluteXor(Isolate* isolate,
                                                 Handle<BigIntBase> x,
                                                 Handle<BigIntBase> y,
                                                 MutableBigInt result_storage) {
  return AbsoluteBitwiseOp(isolate, x, y, result_storage, kCopy, kSymmetric,
                           [](digit_t a, digit_t b) { return a ^ b; });
}

// Multiplies {source} with {factor} and adds {summand} to the result.
// {result} and {source} may be the same BigInt for inplace modification.
void MutableBigInt::InternalMultiplyAdd(BigIntBase source, digit_t factor,
                                        digit_t summand, int n,
                                        MutableBigInt result) {
  DCHECK(source.length() >= n);
  DCHECK(result.length() >= n);
  digit_t carry = summand;
  digit_t high = 0;
  for (int i = 0; i < n; i++) {
    digit_t current = source.digit(i);
    digit_t new_carry = 0;
    // Compute this round's multiplication.
    digit_t new_high = 0;
    current = digit_mul(current, factor, &new_high);
    // Add last round's carryovers.
    current = digit_add(current, high, &new_carry);
    current = digit_add(current, carry, &new_carry);
    // Store result and prepare for next round.
    result.set_digit(i, current);
    carry = new_carry;
    high = new_high;
  }
  if (result.length() > n) {
    result.set_digit(n++, carry + high);
    // Current callers don't pass in such large results, but let's be robust.
    while (n < result.length()) {
      result.set_digit(n++, 0);
    }
  } else {
    CHECK_EQ(carry + high, 0);
  }
}

// Multiplies {x} with {factor} and then adds {summand} to it.
void BigInt::InplaceMultiplyAdd(FreshlyAllocatedBigInt x, uintptr_t factor,
                                uintptr_t summand) {
  STATIC_ASSERT(sizeof(factor) == sizeof(digit_t));
  STATIC_ASSERT(sizeof(summand) == sizeof(digit_t));
  MutableBigInt bigint = MutableBigInt::cast(x);
  MutableBigInt::InternalMultiplyAdd(bigint, factor, summand, bigint.length(),
                                     bigint);
}

MaybeHandle<BigInt> MutableBigInt::LeftShiftByAbsolute(Isolate* isolate,
                                                       Handle<BigIntBase> x,
                                                       Handle<BigIntBase> y) {
  Maybe<digit_t> maybe_shift = ToShiftAmount(y);
  if (maybe_shift.IsNothing()) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  digit_t shift = maybe_shift.FromJust();
  int digit_shift = static_cast<int>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);
  int length = x->length();
  bool grow = bits_shift != 0 &&
              (x->digit(length - 1) >> (kDigitBits - bits_shift)) != 0;
  int result_length = length + digit_shift + grow;
  if (result_length > kMaxLength) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  Handle<MutableBigInt> result;
  if (!New(isolate, result_length).ToHandle(&result)) {
    return MaybeHandle<BigInt>();
  }
  if (bits_shift == 0) {
    int i = 0;
    for (; i < digit_shift; i++) result->set_digit(i, 0ul);
    for (; i < result_length; i++) {
      result->set_digit(i, x->digit(i - digit_shift));
    }
  } else {
    digit_t carry = 0;
    for (int i = 0; i < digit_shift; i++) result->set_digit(i, 0ul);
    for (int i = 0; i < length; i++) {
      digit_t d = x->digit(i);
      result->set_digit(i + digit_shift, (d << bits_shift) | carry);
      carry = d >> (kDigitBits - bits_shift);
    }
    if (grow) {
      result->set_digit(length + digit_shift, carry);
    } else {
      DCHECK_EQ(carry, 0);
    }
  }
  result->set_sign(x->sign());
  return MakeImmutable(result);
}

Handle<BigInt> MutableBigInt::RightShiftByAbsolute(Isolate* isolate,
                                                   Handle<BigIntBase> x,
                                                   Handle<BigIntBase> y) {
  int length = x->length();
  bool sign = x->sign();
  Maybe<digit_t> maybe_shift = ToShiftAmount(y);
  if (maybe_shift.IsNothing()) {
    return RightShiftByMaximum(isolate, sign);
  }
  digit_t shift = maybe_shift.FromJust();
  int digit_shift = static_cast<int>(shift / kDigitBits);
  int bits_shift = static_cast<int>(shift % kDigitBits);
  int result_length = length - digit_shift;
  if (result_length <= 0) {
    return RightShiftByMaximum(isolate, sign);
  }
  // For negative numbers, round down if any bit was shifted out (so that e.g.
  // -5n >> 1n == -3n and not -2n). Check now whether this will happen and
  // whether it can cause overflow into a new digit. If we allocate the result
  // large enough up front, it avoids having to do a second allocation later.
  bool must_round_down = false;
  if (sign) {
    const digit_t mask = (static_cast<digit_t>(1) << bits_shift) - 1;
    if ((x->digit(digit_shift) & mask) != 0) {
      must_round_down = true;
    } else {
      for (int i = 0; i < digit_shift; i++) {
        if (x->digit(i) != 0) {
          must_round_down = true;
          break;
        }
      }
    }
  }
  // If bits_shift is non-zero, it frees up bits, preventing overflow.
  if (must_round_down && bits_shift == 0) {
    // Overflow cannot happen if the most significant digit has unset bits.
    digit_t msd = x->digit(length - 1);
    bool rounding_can_overflow = digit_ismax(msd);
    if (rounding_can_overflow) result_length++;
  }

  DCHECK_LE(result_length, length);
  Handle<MutableBigInt> result = New(isolate, result_length).ToHandleChecked();
  if (bits_shift == 0) {
    // Zero out any overflow digit (see "rounding_can_overflow" above).
    result->set_digit(result_length - 1, 0);
    for (int i = digit_shift; i < length; i++) {
      result->set_digit(i - digit_shift, x->digit(i));
    }
  } else {
    digit_t carry = x->digit(digit_shift) >> bits_shift;
    int last = length - digit_shift - 1;
    for (int i = 0; i < last; i++) {
      digit_t d = x->digit(i + digit_shift + 1);
      result->set_digit(i, (d << (kDigitBits - bits_shift)) | carry);
      carry = d >> bits_shift;
    }
    result->set_digit(last, carry);
  }

  if (sign) {
    result->set_sign(true);
    if (must_round_down) {
      // Since the result is negative, rounding down means adding one to
      // its absolute value. This cannot overflow.
      result = AbsoluteAddOne(isolate, result, true, *result).ToHandleChecked();
    }
  }
  return MakeImmutable(result);
}

Handle<BigInt> MutableBigInt::RightShiftByMaximum(Isolate* isolate, bool sign) {
  if (sign) {
    // TODO(jkummerow): Consider caching a canonical -1n BigInt.
    return NewFromInt(isolate, -1);
  } else {
    return Zero(isolate);
  }
}

// Returns the value of {x} if it is less than the maximum bit length of
// a BigInt, or Nothing otherwise.
Maybe<BigInt::digit_t> MutableBigInt::ToShiftAmount(Handle<BigIntBase> x) {
  if (x->length() > 1) return Nothing<digit_t>();
  digit_t value = x->digit(0);
  STATIC_ASSERT(kMaxLengthBits < std::numeric_limits<digit_t>::max());
  if (value > kMaxLengthBits) return Nothing<digit_t>();
  return Just(value);
}

// Lookup table for the maximum number of bits required per character of a
// base-N string representation of a number. To increase accuracy, the array
// value is the actual value multiplied by 32. To generate this table:
// for (var i = 0; i <= 36; i++) { print(Math.ceil(Math.log2(i) * 32) + ","); }
constexpr uint8_t kMaxBitsPerChar[] = {
    0,   0,   32,  51,  64,  75,  83,  90,  96,  // 0..8
    102, 107, 111, 115, 119, 122, 126, 128,      // 9..16
    131, 134, 136, 139, 141, 143, 145, 147,      // 17..24
    149, 151, 153, 154, 156, 158, 159, 160,      // 25..32
    162, 163, 165, 166,                          // 33..36
};

static const int kBitsPerCharTableShift = 5;
static const size_t kBitsPerCharTableMultiplier = 1u << kBitsPerCharTableShift;

template <typename IsolateT>
MaybeHandle<FreshlyAllocatedBigInt> BigInt::AllocateFor(
    IsolateT* isolate, int radix, int charcount, ShouldThrow should_throw,
    AllocationType allocation) {
  DCHECK(2 <= radix && radix <= 36);
  DCHECK_GE(charcount, 0);
  size_t bits_per_char = kMaxBitsPerChar[radix];
  uint64_t chars = static_cast<uint64_t>(charcount);
  const int roundup = kBitsPerCharTableMultiplier - 1;
  if (chars <=
      (std::numeric_limits<uint64_t>::max() - roundup) / bits_per_char) {
    uint64_t bits_min = bits_per_char * chars;
    // Divide by 32 (see table), rounding up.
    bits_min = (bits_min + roundup) >> kBitsPerCharTableShift;
    if (bits_min <= static_cast<uint64_t>(kMaxInt)) {
      // Divide by kDigitsBits, rounding up.
      int length = static_cast<int>((bits_min + kDigitBits - 1) / kDigitBits);
      if (length <= kMaxLength) {
        Handle<MutableBigInt> result =
            MutableBigInt::New(isolate, length, allocation).ToHandleChecked();
        result->InitializeDigits(length);
        return result;
      }
    }
  }
  // All the overflow/maximum checks above fall through to here.
  if (should_throw == kThrowOnError) {
    return ThrowBigIntTooBig<FreshlyAllocatedBigInt>(isolate);
  } else {
    return MaybeHandle<FreshlyAllocatedBigInt>();
  }
}
template MaybeHandle<FreshlyAllocatedBigInt> BigInt::AllocateFor(
    Isolate* isolate, int radix, int charcount, ShouldThrow should_throw,
    AllocationType allocation);
template MaybeHandle<FreshlyAllocatedBigInt> BigInt::AllocateFor(
    LocalIsolate* isolate, int radix, int charcount, ShouldThrow should_throw,
    AllocationType allocation);

template <typename IsolateT>
Handle<BigInt> BigInt::Finalize(Handle<FreshlyAllocatedBigInt> x, bool sign) {
  Handle<MutableBigInt> bigint = Handle<MutableBigInt>::cast(x);
  bigint->set_sign(sign);
  return MutableBigInt::MakeImmutable<Isolate>(bigint);
}

template Handle<BigInt> BigInt::Finalize<Isolate>(
    Handle<FreshlyAllocatedBigInt>, bool);
template Handle<BigInt> BigInt::Finalize<LocalIsolate>(
    Handle<FreshlyAllocatedBigInt>, bool);

// The serialization format MUST NOT CHANGE without updating the format
// version in value-serializer.cc!
uint32_t BigInt::GetBitfieldForSerialization() const {
  // In order to make the serialization format the same on 32/64 bit builds,
  // we convert the length-in-digits to length-in-bytes for serialization.
  // Being able to do this depends on having enough LengthBits:
  STATIC_ASSERT(kMaxLength * kDigitSize <= LengthBits::kMax);
  int bytelength = length() * kDigitSize;
  return SignBits::encode(sign()) | LengthBits::encode(bytelength);
}

int BigInt::DigitsByteLengthForBitfield(uint32_t bitfield) {
  return LengthBits::decode(bitfield);
}

// The serialization format MUST NOT CHANGE without updating the format
// version in value-serializer.cc!
void BigInt::SerializeDigits(uint8_t* storage) {
  void* digits =
      reinterpret_cast<void*>(ptr() + kDigitsOffset - kHeapObjectTag);
#if defined(V8_TARGET_LITTLE_ENDIAN)
  int bytelength = length() * kDigitSize;
  memcpy(storage, digits, bytelength);
#elif defined(V8_TARGET_BIG_ENDIAN)
  digit_t* digit_storage = reinterpret_cast<digit_t*>(storage);
  const digit_t* digit = reinterpret_cast<const digit_t*>(digits);
  for (int i = 0; i < length(); i++) {
    *digit_storage = ByteReverse(*digit);
    digit_storage++;
    digit++;
  }
#endif  // V8_TARGET_BIG_ENDIAN
}

// The serialization format MUST NOT CHANGE without updating the format
// version in value-serializer.cc!
MaybeHandle<BigInt> BigInt::FromSerializedDigits(
    Isolate* isolate, uint32_t bitfield,
    base::Vector<const uint8_t> digits_storage) {
  int bytelength = LengthBits::decode(bitfield);
  DCHECK(digits_storage.length() == bytelength);
  bool sign = SignBits::decode(bitfield);
  int length = (bytelength + kDigitSize - 1) / kDigitSize;  // Round up.
  Handle<MutableBigInt> result =
      MutableBigInt::Cast(isolate->factory()->NewBigInt(length));
  result->initialize_bitfield(sign, length);
  void* digits =
      reinterpret_cast<void*>(result->ptr() + kDigitsOffset - kHeapObjectTag);
#if defined(V8_TARGET_LITTLE_ENDIAN)
  memcpy(digits, digits_storage.begin(), bytelength);
  void* padding_start =
      reinterpret_cast<void*>(reinterpret_cast<Address>(digits) + bytelength);
  memset(padding_start, 0, length * kDigitSize - bytelength);
#elif defined(V8_TARGET_BIG_ENDIAN)
  digit_t* digit = reinterpret_cast<digit_t*>(digits);
  const digit_t* digit_storage =
      reinterpret_cast<const digit_t*>(digits_storage.begin());
  for (int i = 0; i < bytelength / kDigitSize; i++) {
    *digit = ByteReverse(*digit_storage);
    digit_storage++;
    digit++;
  }
  if (bytelength % kDigitSize) {
    *digit = 0;
    byte* digit_byte = reinterpret_cast<byte*>(digit);
    digit_byte += sizeof(*digit) - 1;
    const byte* digit_storage_byte =
        reinterpret_cast<const byte*>(digit_storage);
    for (int i = 0; i < bytelength % kDigitSize; i++) {
      *digit_byte = *digit_storage_byte;
      digit_byte--;
      digit_storage_byte++;
    }
  }
#endif  // V8_TARGET_BIG_ENDIAN
  return MutableBigInt::MakeImmutable(result);
}

Handle<BigInt> BigInt::AsIntN(Isolate* isolate, uint64_t n, Handle<BigInt> x) {
  if (x->is_zero()) return x;
  if (n == 0) return MutableBigInt::Zero(isolate);
  uint64_t needed_length = (n + kDigitBits - 1) / kDigitBits;
  uint64_t x_length = static_cast<uint64_t>(x->length());
  // If {x} has less than {n} bits, return it directly.
  if (x_length < needed_length) return x;
  DCHECK_LE(needed_length, kMaxInt);
  digit_t top_digit = x->digit(static_cast<int>(needed_length) - 1);
  digit_t compare_digit = static_cast<digit_t>(1) << ((n - 1) % kDigitBits);
  if (x_length == needed_length && top_digit < compare_digit) return x;
  // Otherwise we have to truncate (which is a no-op in the special case
  // of x == -2^(n-1)), and determine the right sign. We also might have
  // to subtract from 2^n to simulate having two's complement representation.
  // In most cases, the result's sign is x->sign() xor "(n-1)th bit present".
  // The only exception is when x is negative, has the (n-1)th bit, and all
  // its bits below (n-1) are zero. In that case, the result is the minimum
  // n-bit integer (example: asIntN(3, -12n) => -4n).
  bool has_bit = (top_digit & compare_digit) == compare_digit;
  DCHECK_LE(n, kMaxInt);
  int N = static_cast<int>(n);
  if (!has_bit) {
    return MutableBigInt::TruncateToNBits(isolate, N, x);
  }
  if (!x->sign()) {
    return MutableBigInt::TruncateAndSubFromPowerOfTwo(isolate, N, x, true);
  }
  // Negative numbers must subtract from 2^n, except for the special case
  // described above.
  if ((top_digit & (compare_digit - 1)) == 0) {
    for (int i = static_cast<int>(needed_length) - 2; i >= 0; i--) {
      if (x->digit(i) != 0) {
        return MutableBigInt::TruncateAndSubFromPowerOfTwo(isolate, N, x,
                                                           false);
      }
    }
    // Truncation is no-op if x == -2^(n-1).
    if (x_length == needed_length && top_digit == compare_digit) return x;
    return MutableBigInt::TruncateToNBits(isolate, N, x);
  }
  return MutableBigInt::TruncateAndSubFromPowerOfTwo(isolate, N, x, false);
}

MaybeHandle<BigInt> BigInt::AsUintN(Isolate* isolate, uint64_t n,
                                    Handle<BigInt> x) {
  if (x->is_zero()) return x;
  if (n == 0) return MutableBigInt::Zero(isolate);
  // If {x} is negative, simulate two's complement representation.
  if (x->sign()) {
    if (n > kMaxLengthBits) {
      return ThrowBigIntTooBig<BigInt>(isolate);
    }
    return MutableBigInt::TruncateAndSubFromPowerOfTwo(
        isolate, static_cast<int>(n), x, false);
  }
  // If {x} is positive and has up to {n} bits, return it directly.
  if (n >= kMaxLengthBits) return x;
  STATIC_ASSERT(kMaxLengthBits < kMaxInt - kDigitBits);
  int needed_length = static_cast<int>((n + kDigitBits - 1) / kDigitBits);
  if (x->length() < needed_length) return x;
  int bits_in_top_digit = n % kDigitBits;
  if (x->length() == needed_length) {
    if (bits_in_top_digit == 0) return x;
    digit_t top_digit = x->digit(needed_length - 1);
    if ((top_digit >> bits_in_top_digit) == 0) return x;
  }
  // Otherwise, truncate.
  DCHECK_LE(n, kMaxInt);
  return MutableBigInt::TruncateToNBits(isolate, static_cast<int>(n), x);
}

Handle<BigInt> MutableBigInt::TruncateToNBits(Isolate* isolate, int n,
                                              Handle<BigInt> x) {
  // Only call this when there's something to do.
  DCHECK_NE(n, 0);
  DCHECK_GT(x->length(), n / kDigitBits);

  int needed_digits = (n + (kDigitBits - 1)) / kDigitBits;
  DCHECK_LE(needed_digits, x->length());
  Handle<MutableBigInt> result = New(isolate, needed_digits).ToHandleChecked();

  // Copy all digits except the MSD.
  int last = needed_digits - 1;
  for (int i = 0; i < last; i++) {
    result->set_digit(i, x->digit(i));
  }

  // The MSD might contain extra bits that we don't want.
  digit_t msd = x->digit(last);
  if (n % kDigitBits != 0) {
    int drop = kDigitBits - (n % kDigitBits);
    msd = (msd << drop) >> drop;
  }
  result->set_digit(last, msd);
  result->set_sign(x->sign());
  return MakeImmutable(result);
}

// Subtracts the least significant n bits of abs(x) from 2^n.
Handle<BigInt> MutableBigInt::TruncateAndSubFromPowerOfTwo(Isolate* isolate,
                                                           int n,
                                                           Handle<BigInt> x,
                                                           bool result_sign) {
  DCHECK_NE(n, 0);
  DCHECK_LE(n, kMaxLengthBits);

  int needed_digits = (n + (kDigitBits - 1)) / kDigitBits;
  DCHECK_LE(needed_digits, kMaxLength);  // Follows from n <= kMaxLengthBits.
  Handle<MutableBigInt> result = New(isolate, needed_digits).ToHandleChecked();

  // Process all digits except the MSD.
  int i = 0;
  int last = needed_digits - 1;
  int x_length = x->length();
  digit_t borrow = 0;
  // Take digits from {x} unless its length is exhausted.
  int limit = std::min(last, x_length);
  for (; i < limit; i++) {
    digit_t new_borrow = 0;
    digit_t difference = digit_sub(0, x->digit(i), &new_borrow);
    difference = digit_sub(difference, borrow, &new_borrow);
    result->set_digit(i, difference);
    borrow = new_borrow;
  }
  // Then simulate leading zeroes in {x} as needed.
  for (; i < last; i++) {
    digit_t new_borrow = 0;
    digit_t difference = digit_sub(0, borrow, &new_borrow);
    result->set_digit(i, difference);
    borrow = new_borrow;
  }

  // The MSD might contain extra bits that we don't want.
  digit_t msd = last < x_length ? x->digit(last) : 0;
  int msd_bits_consumed = n % kDigitBits;
  digit_t result_msd;
  if (msd_bits_consumed == 0) {
    digit_t new_borrow = 0;
    result_msd = digit_sub(0, msd, &new_borrow);
    result_msd = digit_sub(result_msd, borrow, &new_borrow);
  } else {
    int drop = kDigitBits - msd_bits_consumed;
    msd = (msd << drop) >> drop;
    digit_t minuend_msd = static_cast<digit_t>(1) << (kDigitBits - drop);
    digit_t new_borrow = 0;
    result_msd = digit_sub(minuend_msd, msd, &new_borrow);
    result_msd = digit_sub(result_msd, borrow, &new_borrow);
    DCHECK_EQ(new_borrow, 0);  // result < 2^n.
    // If all subtracted bits were zero, we have to get rid of the
    // materialized minuend_msd again.
    result_msd &= (minuend_msd - 1);
  }
  result->set_digit(last, result_msd);
  result->set_sign(result_sign);
  return MakeImmutable(result);
}

Handle<BigInt> BigInt::FromInt64(Isolate* isolate, int64_t n) {
  if (n == 0) return MutableBigInt::Zero(isolate);
  STATIC_ASSERT(kDigitBits == 64 || kDigitBits == 32);
  int length = 64 / kDigitBits;
  Handle<MutableBigInt> result =
      MutableBigInt::Cast(isolate->factory()->NewBigInt(length));
  bool sign = n < 0;
  result->initialize_bitfield(sign, length);
  uint64_t absolute;
  if (!sign) {
    absolute = static_cast<uint64_t>(n);
  } else {
    if (n == std::numeric_limits<int64_t>::min()) {
      absolute = static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1;
    } else {
      absolute = static_cast<uint64_t>(-n);
    }
  }
  result->set_64_bits(absolute);
  return MutableBigInt::MakeImmutable(result);
}

Handle<BigInt> BigInt::FromUint64(Isolate* isolate, uint64_t n) {
  if (n == 0) return MutableBigInt::Zero(isolate);
  STATIC_ASSERT(kDigitBits == 64 || kDigitBits == 32);
  int length = 64 / kDigitBits;
  Handle<MutableBigInt> result =
      MutableBigInt::Cast(isolate->factory()->NewBigInt(length));
  result->initialize_bitfield(false, length);
  result->set_64_bits(n);
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::FromWords64(Isolate* isolate, int sign_bit,
                                        int words64_count,
                                        const uint64_t* words) {
  if (words64_count < 0 || words64_count > kMaxLength / (64 / kDigitBits)) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  if (words64_count == 0) return MutableBigInt::Zero(isolate);
  STATIC_ASSERT(kDigitBits == 64 || kDigitBits == 32);
  int length = (64 / kDigitBits) * words64_count;
  DCHECK_GT(length, 0);
  if (kDigitBits == 32 && words[words64_count - 1] <= (1ULL << 32)) length--;

  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, length).ToHandle(&result)) {
    return MaybeHandle<BigInt>();
  }

  result->set_sign(sign_bit);
  if (kDigitBits == 64) {
    for (int i = 0; i < length; ++i) {
      result->set_digit(i, static_cast<digit_t>(words[i]));
    }
  } else {
    for (int i = 0; i < length; i += 2) {
      digit_t lo = static_cast<digit_t>(words[i / 2]);
      digit_t hi = static_cast<digit_t>(words[i / 2] >> 32);
      result->set_digit(i, lo);
      if (i + 1 < length) result->set_digit(i + 1, hi);
    }
  }

  return MutableBigInt::MakeImmutable(result);
}

int BigInt::Words64Count() {
  STATIC_ASSERT(kDigitBits == 64 || kDigitBits == 32);
  return length() / (64 / kDigitBits) +
         (kDigitBits == 32 && length() % 2 == 1 ? 1 : 0);
}

void BigInt::ToWordsArray64(int* sign_bit, int* words64_count,
                            uint64_t* words) {
  DCHECK_NE(sign_bit, nullptr);
  DCHECK_NE(words64_count, nullptr);
  *sign_bit = sign();
  int available_words = *words64_count;
  *words64_count = Words64Count();
  if (available_words == 0) return;
  DCHECK_NE(words, nullptr);

  int len = length();
  if (kDigitBits == 64) {
    for (int i = 0; i < len && i < available_words; ++i) words[i] = digit(i);
  } else {
    for (int i = 0; i < len && available_words > 0; i += 2) {
      uint64_t lo = digit(i);
      uint64_t hi = (i + 1) < len ? digit(i + 1) : 0;
      words[i / 2] = lo | (hi << 32);
      available_words--;
    }
  }
}

uint64_t MutableBigInt::GetRawBits(BigIntBase x, bool* lossless) {
  if (lossless != nullptr) *lossless = true;
  if (x.is_zero()) return 0;
  int len = x.length();
  STATIC_ASSERT(kDigitBits == 64 || kDigitBits == 32);
  if (lossless != nullptr && len > 64 / kDigitBits) *lossless = false;
  uint64_t raw = static_cast<uint64_t>(x.digit(0));
  if (kDigitBits == 32 && len > 1) {
    raw |= static_cast<uint64_t>(x.digit(1)) << 32;
  }
  // Simulate two's complement. MSVC dislikes "-raw".
  return x.sign() ? ((~raw) + 1u) : raw;
}

int64_t BigInt::AsInt64(bool* lossless) {
  uint64_t raw = MutableBigInt::GetRawBits(*this, lossless);
  int64_t result = static_cast<int64_t>(raw);
  if (lossless != nullptr && (result < 0) != sign()) *lossless = false;
  return result;
}

uint64_t BigInt::AsUint64(bool* lossless) {
  uint64_t result = MutableBigInt::GetRawBits(*this, lossless);
  if (lossless != nullptr && sign()) *lossless = false;
  return result;
}

// Digit arithmetic helpers.

#if V8_TARGET_ARCH_32_BIT
#define HAVE_TWODIGIT_T 1
using twodigit_t = uint64_t;
#elif defined(__SIZEOF_INT128__)
// Both Clang and GCC support this on x64.
#define HAVE_TWODIGIT_T 1
using twodigit_t = __uint128_t;
#endif

// {carry} must point to an initialized digit_t and will either be incremented
// by one or left alone.
inline BigInt::digit_t MutableBigInt::digit_add(digit_t a, digit_t b,
                                                digit_t* carry) {
#if HAVE_TWODIGIT_T
  twodigit_t result = static_cast<twodigit_t>(a) + static_cast<twodigit_t>(b);
  *carry += result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  digit_t result = a + b;
  if (result < a) *carry += 1;
  return result;
#endif
}

// {borrow} must point to an initialized digit_t and will either be incremented
// by one or left alone.
inline BigInt::digit_t MutableBigInt::digit_sub(digit_t a, digit_t b,
                                                digit_t* borrow) {
#if HAVE_TWODIGIT_T
  twodigit_t result = static_cast<twodigit_t>(a) - static_cast<twodigit_t>(b);
  *borrow += (result >> kDigitBits) & 1;
  return static_cast<digit_t>(result);
#else
  digit_t result = a - b;
  if (result > a) *borrow += 1;
  return static_cast<digit_t>(result);
#endif
}

// Returns the low half of the result. High half is in {high}.
inline BigInt::digit_t MutableBigInt::digit_mul(digit_t a, digit_t b,
                                                digit_t* high) {
#if HAVE_TWODIGIT_T
  twodigit_t result = static_cast<twodigit_t>(a) * static_cast<twodigit_t>(b);
  *high = result >> kDigitBits;
  return static_cast<digit_t>(result);
#else
  // Multiply in half-pointer-sized chunks.
  // For inputs [AH AL]*[BH BL], the result is:
  //
  //            [AL*BL]  // r_low
  //    +    [AL*BH]     // r_mid1
  //    +    [AH*BL]     // r_mid2
  //    + [AH*BH]        // r_high
  //    = [R4 R3 R2 R1]  // high = [R4 R3], low = [R2 R1]
  //
  // Where of course we must be careful with carries between the columns.
  digit_t a_low = a & kHalfDigitMask;
  digit_t a_high = a >> kHalfDigitBits;
  digit_t b_low = b & kHalfDigitMask;
  digit_t b_high = b >> kHalfDigitBits;

  digit_t r_low = a_low * b_low;
  digit_t r_mid1 = a_low * b_high;
  digit_t r_mid2 = a_high * b_low;
  digit_t r_high = a_high * b_high;

  digit_t carry = 0;
  digit_t low = digit_add(r_low, r_mid1 << kHalfDigitBits, &carry);
  low = digit_add(low, r_mid2 << kHalfDigitBits, &carry);
  *high =
      (r_mid1 >> kHalfDigitBits) + (r_mid2 >> kHalfDigitBits) + r_high + carry;
  return low;
#endif
}

#undef HAVE_TWODIGIT_T

void MutableBigInt::set_64_bits(uint64_t bits) {
  STATIC_ASSERT(kDigitBits == 64 || kDigitBits == 32);
  if (kDigitBits == 64) {
    set_digit(0, static_cast<digit_t>(bits));
  } else {
    set_digit(0, static_cast<digit_t>(bits & 0xFFFFFFFFu));
    set_digit(1, static_cast<digit_t>(bits >> 32));
  }
}

#ifdef OBJECT_PRINT
void BigIntBase::BigIntBasePrint(std::ostream& os) {
  DisallowGarbageCollection no_gc;
  PrintHeader(os, "BigInt");
  int len = length();
  os << "\n- length: " << len;
  os << "\n- sign: " << sign();
  if (len > 0) {
    os << "\n- digits:";
    for (int i = 0; i < len; i++) {
      os << "\n    0x" << std::hex << digit(i);
    }
  }
  os << std::dec << "\n";
}
#endif  // OBJECT_PRINT

void MutableBigInt_AbsoluteAddAndCanonicalize(Address result_addr,
                                              Address x_addr, Address y_addr) {
  BigInt x = BigInt::cast(Object(x_addr));
  BigInt y = BigInt::cast(Object(y_addr));
  MutableBigInt result = MutableBigInt::cast(Object(result_addr));

  bigint::Add(GetRWDigits(result), GetDigits(x), GetDigits(y));
  MutableBigInt::Canonicalize(result);
}

int32_t MutableBigInt_AbsoluteCompare(Address x_addr, Address y_addr) {
  BigInt x = BigInt::cast(Object(x_addr));
  BigInt y = BigInt::cast(Object(y_addr));

  return bigint::Compare(GetDigits(x), GetDigits(y));
}

void MutableBigInt_AbsoluteSubAndCanonicalize(Address result_addr,
                                              Address x_addr, Address y_addr) {
  BigInt x = BigInt::cast(Object(x_addr));
  BigInt y = BigInt::cast(Object(y_addr));
  MutableBigInt result = MutableBigInt::cast(Object(result_addr));

  bigint::Subtract(GetRWDigits(result), GetDigits(x), GetDigits(y));
  MutableBigInt::Canonicalize(result);
}

}  // namespace internal
}  // namespace v8
