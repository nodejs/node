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

#include <atomic>

#include "src/base/numbers/double.h"
#include "src/bigint/bigint.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap.h"
#include "src/numbers/conversions.h"
#include "src/objects/casting.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

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
V8_OBJECT class MutableBigInt : public FreshlyAllocatedBigInt {
 public:
  // Bottleneck for converting MutableBigInts to BigInts.
  static MaybeHandle<BigInt> MakeImmutable(MaybeHandle<MutableBigInt> maybe);
  template <typename Isolate = v8::internal::Isolate>
  static Handle<BigInt> MakeImmutable(Handle<MutableBigInt> result);

  static void Canonicalize(Tagged<MutableBigInt> result);

  // Allocation helpers.
  template <typename IsolateT>
  static MaybeHandle<MutableBigInt> New(
      IsolateT* isolate, uint32_t length,
      AllocationType allocation = AllocationType::kYoung);
  static Handle<BigInt> NewFromInt(Isolate* isolate, int value);
  static Handle<BigInt> NewFromDouble(Isolate* isolate, double value);
  void InitializeDigits(uint32_t length, uint8_t value = 0);
  static Handle<MutableBigInt> Copy(Isolate* isolate,
                                    DirectHandle<BigIntBase> source);
  template <typename IsolateT>
  static Handle<BigInt> Zero(
      IsolateT* isolate, AllocationType allocation = AllocationType::kYoung) {
    // TODO(jkummerow): Consider caching a canonical zero-BigInt.
    return MakeImmutable<IsolateT>(
        New(isolate, 0, allocation).ToHandleChecked());
  }

  // Internal helpers.
  static MaybeHandle<MutableBigInt> AbsoluteAddOne(
      Isolate* isolate, DirectHandle<BigIntBase> x, bool sign,
      Tagged<MutableBigInt> result_storage = {});
  static Handle<MutableBigInt> AbsoluteSubOne(Isolate* isolate,
                                              DirectHandle<BigIntBase> x);

  // Specialized helpers for shift operations.
  static MaybeDirectHandle<BigInt> LeftShiftByAbsolute(Isolate* isolate,
                                                       Handle<BigIntBase> x,
                                                       Handle<BigIntBase> y);
  static DirectHandle<BigInt> RightShiftByAbsolute(Isolate* isolate,
                                                   Handle<BigIntBase> x,
                                                   Handle<BigIntBase> y);
  static DirectHandle<BigInt> RightShiftByMaximum(Isolate* isolate, bool sign);
  static Maybe<digit_t> ToShiftAmount(Handle<BigIntBase> x);

  static double ToDouble(DirectHandle<BigIntBase> x);
  enum Rounding { kRoundDown, kTie, kRoundUp };
  static Rounding DecideRounding(DirectHandle<BigIntBase> x,
                                 int mantissa_bits_unset, int digit_index,
                                 uint64_t current_digit);

  // Returns the least significant 64 bits, simulating two's complement
  // representation.
  static uint64_t GetRawBits(BigIntBase* x, bool* lossless);

  static inline bool digit_ismax(digit_t x) {
    return static_cast<digit_t>(~x) == 0;
  }

  bigint::RWDigits rw_digits();

  inline void set_sign(bool new_sign) {
    bitfield_.store(
        SignBits::update(bitfield_.load(std::memory_order_relaxed), new_sign),
        std::memory_order_relaxed);
  }
  inline void set_length(uint32_t new_length, ReleaseStoreTag) {
    bitfield_.store(LengthBits::update(
                        bitfield_.load(std::memory_order_relaxed), new_length),
                    std::memory_order_relaxed);
  }
  inline void initialize_bitfield(bool sign, uint32_t length) {
    bitfield_.store(LengthBits::encode(length) | SignBits::encode(sign),
                    std::memory_order_relaxed);
  }
  inline void set_digit(uint32_t n, digit_t value) {
    SLOW_DCHECK(n < length());
    raw_digits()[n].set_value(value);
  }

  void set_64_bits(uint64_t bits);

  static bool IsMutableBigInt(Tagged<MutableBigInt> o) { return IsBigInt(o); }

  static_assert(std::is_same_v<bigint::digit_t, BigIntBase::digit_t>,
                "We must be able to call BigInt library functions");

  NEVER_READ_ONLY_SPACE
} V8_OBJECT_END;

NEVER_READ_ONLY_SPACE_IMPL(MutableBigInt)

template <>
struct CastTraits<MutableBigInt> : public CastTraits<BigInt> {};

bigint::Digits BigIntBase::digits() const {
  return bigint::Digits(reinterpret_cast<const digit_t*>(raw_digits()),
                        length());
}

bigint::RWDigits MutableBigInt::rw_digits() {
  return bigint::RWDigits(reinterpret_cast<digit_t*>(raw_digits()), length());
}

template <typename T, typename Isolate>
MaybeHandle<T> ThrowBigIntTooBig(Isolate* isolate) {
  // If the result of a BigInt computation is truncated to 64 bit, Turbofan
  // can sometimes truncate intermediate results already, which can prevent
  // those from exceeding the maximum length, effectively preventing a
  // RangeError from being thrown. As this is a performance optimization, this
  // behavior is accepted. To prevent the correctness fuzzer from detecting this
  // difference, we crash the program.
  if (v8_flags.correctness_fuzzer_suppressions) {
    FATAL("Aborting on invalid BigInt length");
  }
  THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kBigIntTooBig));
}

template <typename IsolateT>
MaybeHandle<MutableBigInt> MutableBigInt::New(IsolateT* isolate,
                                              uint32_t length,
                                              AllocationType allocation) {
  if (length > BigInt::kMaxLength) {
    return ThrowBigIntTooBig<MutableBigInt>(isolate);
  }
  Handle<MutableBigInt> result =
      Cast<MutableBigInt>(isolate->factory()->NewBigInt(length, allocation));
  result->initialize_bitfield(false, length);
#if DEBUG
  result->InitializeDigits(length, 0xBF);
#endif
  return result;
}

Handle<BigInt> MutableBigInt::NewFromInt(Isolate* isolate, int value) {
  if (value == 0) return Zero(isolate);
  Handle<MutableBigInt> result =
      Cast<MutableBigInt>(isolate->factory()->NewBigInt(1));
  bool sign = value < 0;
  result->initialize_bitfield(sign, 1);
  if (!sign) {
    result->set_digit(0, value);
  } else {
    if (value == kMinInt) {
      static_assert(kMinInt == -kMaxInt - 1);
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
  uint64_t double_bits = base::bit_cast<uint64_t>(value);
  int32_t raw_exponent =
      static_cast<int32_t>(double_bits >>
                           base::Double::kPhysicalSignificandSize) &
      0x7FF;
  DCHECK_NE(raw_exponent, 0x7FF);
  DCHECK_GE(raw_exponent, 0x3FF);
  uint32_t exponent = raw_exponent - 0x3FF;
  uint32_t digits = exponent / kDigitBits + 1;
  Handle<MutableBigInt> result =
      Cast<MutableBigInt>(isolate->factory()->NewBigInt(digits));
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
  const uint32_t kMantissaTopBit =
      base::Double::kSignificandSize - 1;  // 0-indexed.
  // 0-indexed position of most significant bit in the most significant digit.
  uint32_t msd_topbit = exponent % kDigitBits;
  // Number of unused bits in {mantissa}. We'll keep them shifted to the
  // left (i.e. most significant part) of the underlying uint64_t.
  uint32_t remaining_mantissa_bits = 0;
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
  static_assert(BigInt::kMaxLength < kMaxInt);
  for (int32_t digit_index = digits - 2; digit_index >= 0; digit_index--) {
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
                                          DirectHandle<BigIntBase> source) {
  uint32_t length = source->length();
  // Allocating a BigInt of the same length as an existing BigInt cannot throw.
  Handle<MutableBigInt> result = New(isolate, length).ToHandleChecked();
  memcpy(result->raw_digits(), source->raw_digits(), length * kDigitSize);
  return result;
}

void MutableBigInt::InitializeDigits(uint32_t length, uint8_t value) {
  memset(raw_digits(), value, length * kDigitSize);
}

MaybeHandle<BigInt> MutableBigInt::MakeImmutable(
    MaybeHandle<MutableBigInt> maybe) {
  Handle<MutableBigInt> result;
  if (!maybe.ToHandle(&result)) return {};
  return MakeImmutable(result);
}

template <typename IsolateT>
Handle<BigInt> MutableBigInt::MakeImmutable(Handle<MutableBigInt> result) {
  MutableBigInt::Canonicalize(*result);
  return Cast<BigInt>(result);
}

void MutableBigInt::Canonicalize(Tagged<MutableBigInt> result) {
  // Check if we need to right-trim any leading zero-digits.
  uint32_t old_length = result->length();
  uint32_t new_length = old_length;
  while (new_length > 0 && result->digit(new_length - 1) == 0) new_length--;
  uint32_t to_trim = old_length - new_length;
  if (to_trim != 0) {
    Heap* heap = result->GetHeap();
    if (!heap->IsLargeObject(result)) {
      uint32_t old_size =
          ALIGN_TO_ALLOCATION_ALIGNMENT(BigInt::SizeFor(old_length));
      uint32_t new_size =
          ALIGN_TO_ALLOCATION_ALIGNMENT(BigInt::SizeFor(new_length));
      heap->NotifyObjectSizeChange(result, old_size, new_size,
                                   ClearRecordedSlots::kNo);
    }
    result->set_length(new_length, kReleaseStore);

    // Canonicalize -0n.
    if (new_length == 0) {
      result->set_sign(false);
      // TODO(jkummerow): If we cache a canonical 0n, return that here.
    }
  }
  DCHECK_IMPLIES(result->length() > 0,
                 result->digit(result->length() - 1) != 0);  // MSD is non-zero.
  // Callers that don't require trimming must ensure this themselves.
  DCHECK_IMPLIES(result->length() == 0, result->sign() == false);
}

template <typename IsolateT>
Handle<BigInt> BigInt::Zero(IsolateT* isolate, AllocationType allocation) {
  return MutableBigInt::Zero(isolate, allocation);
}
template Handle<BigInt> BigInt::Zero(Isolate* isolate,
                                     AllocationType allocation);
template Handle<BigInt> BigInt::Zero(LocalIsolate* isolate,
                                     AllocationType allocation);

Handle<BigInt> BigInt::UnaryMinus(Isolate* isolate, DirectHandle<BigInt> x) {
  // Special case: There is no -0n.
  if (x->is_zero()) return indirect_handle(x, isolate);
  Handle<MutableBigInt> result = MutableBigInt::Copy(isolate, x);
  result->set_sign(!x->sign());
  return MutableBigInt::MakeImmutable(result);
}

MaybeDirectHandle<BigInt> BigInt::BitwiseNot(Isolate* isolate,
                                             DirectHandle<BigInt> x) {
  MaybeHandle<MutableBigInt> result;
  if (x->sign()) {
    // ~(-x) == ~(~(x-1)) == x-1
    result = MutableBigInt::AbsoluteSubOne(isolate, x);
  } else {
    // ~x == -x-1 == -(x+1)
    result = MutableBigInt::AbsoluteAddOne(isolate, x, true);
  }
  return MutableBigInt::MakeImmutable(result);
}

MaybeDirectHandle<BigInt> BigInt::Exponentiate(Isolate* isolate,
                                               DirectHandle<BigInt> base,
                                               DirectHandle<BigInt> exponent) {
  // 1. If exponent is < 0, throw a RangeError exception.
  if (exponent->sign()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kMustBePositive));
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
  static_assert(kMaxLengthBits < std::numeric_limits<digit_t>::max());
  if (exponent->length() > 1) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  digit_t exp_value = exponent->digit(0);
  if (exp_value == 1) return base;
  if (exp_value >= kMaxLengthBits) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  static_assert(kMaxLengthBits <= kMaxInt);
  int n = static_cast<int>(exp_value);
  if (base->length() == 1 && base->digit(0) == 2) {
    // Fast path for 2^n.
    int needed_digits = 1 + (n / kDigitBits);
    Handle<MutableBigInt> result;
    if (!MutableBigInt::New(isolate, needed_digits).ToHandle(&result))
      return {};
    result->InitializeDigits(needed_digits);
    // All bits are zero. Now set the n-th bit.
    digit_t msd = static_cast<digit_t>(1) << (n % kDigitBits);
    result->set_digit(needed_digits - 1, msd);
    // Result is negative for odd powers of -2n.
    if (base->sign()) result->set_sign((n & 1) != 0);
    return MutableBigInt::MakeImmutable(result);
  }
  DirectHandle<BigInt> result;
  DirectHandle<BigInt> running_square = base;
  // This implicitly sets the result's sign correctly.
  if (n & 1) result = base;
  n >>= 1;
  for (; n != 0; n >>= 1) {
    MaybeDirectHandle<BigInt> maybe_result =
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

MaybeHandle<BigInt> BigInt::Multiply(Isolate* isolate, DirectHandle<BigInt> x,
                                     DirectHandle<BigInt> y) {
  if (x->is_zero()) return indirect_handle(x, isolate);
  if (y->is_zero()) return indirect_handle(y, isolate);
  uint32_t result_length =
      bigint::MultiplyResultLength(x->digits(), y->digits());
  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&result)) {
    return {};
  }
  DisallowGarbageCollection no_gc;
  bigint::Status status = isolate->bigint_processor()->Multiply(
      result->rw_digits(), x->digits(), y->digits());
  if (status == bigint::Status::kInterrupted) {
    AllowGarbageCollection terminating_anyway;
    isolate->TerminateExecution();
    return {};
  }
  result->set_sign(x->sign() != y->sign());
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::Divide(Isolate* isolate, DirectHandle<BigInt> x,
                                   DirectHandle<BigInt> y) {
  // 1. If y is 0n, throw a RangeError exception.
  if (y->is_zero()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kBigIntDivZero));
  }
  // 2. Let quotient be the mathematical value of x divided by y.
  // 3. Return a BigInt representing quotient rounded towards 0 to the next
  //    integral value.
  if (bigint::Compare(x->digits(), y->digits()) < 0) {
    return Zero(isolate);
  }
  bool result_sign = x->sign() != y->sign();
  if (y->length() == 1 && y->digit(0) == 1) {
    return result_sign == x->sign() ? indirect_handle(x, isolate)
                                    : UnaryMinus(isolate, x);
  }
  Handle<MutableBigInt> quotient;
  uint32_t result_length = bigint::DivideResultLength(x->digits(), y->digits());
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&quotient)) {
    return {};
  }
  DisallowGarbageCollection no_gc;
  bigint::Status status = isolate->bigint_processor()->Divide(
      quotient->rw_digits(), x->digits(), y->digits());
  if (status == bigint::Status::kInterrupted) {
    AllowGarbageCollection terminating_anyway;
    isolate->TerminateExecution();
    return {};
  }
  quotient->set_sign(result_sign);
  return MutableBigInt::MakeImmutable(quotient);
}

MaybeHandle<BigInt> BigInt::Remainder(Isolate* isolate, DirectHandle<BigInt> x,
                                      DirectHandle<BigInt> y) {
  // 1. If y is 0n, throw a RangeError exception.
  if (y->is_zero()) {
    THROW_NEW_ERROR(isolate, NewRangeError(MessageTemplate::kBigIntDivZero));
  }
  // 2. Return the BigInt representing x modulo y.
  // See https://github.com/tc39/proposal-bigint/issues/84 though.
  if (bigint::Compare(x->digits(), y->digits()) < 0)
    return indirect_handle(x, isolate);
  if (y->length() == 1 && y->digit(0) == 1) return Zero(isolate);
  Handle<MutableBigInt> remainder;
  uint32_t result_length = bigint::ModuloResultLength(y->digits());
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&remainder)) {
    return {};
  }
  DisallowGarbageCollection no_gc;
  bigint::Status status = isolate->bigint_processor()->Modulo(
      remainder->rw_digits(), x->digits(), y->digits());
  if (status == bigint::Status::kInterrupted) {
    AllowGarbageCollection terminating_anyway;
    isolate->TerminateExecution();
    return {};
  }
  remainder->set_sign(x->sign());
  return MutableBigInt::MakeImmutable(remainder);
}

MaybeHandle<BigInt> BigInt::Add(Isolate* isolate, DirectHandle<BigInt> x,
                                DirectHandle<BigInt> y) {
  if (x->is_zero()) return indirect_handle(y, isolate);
  if (y->is_zero()) return indirect_handle(x, isolate);
  bool xsign = x->sign();
  bool ysign = y->sign();
  uint32_t result_length =
      bigint::AddSignedResultLength(x->length(), y->length(), xsign == ysign);
  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&result)) {
    // Allocation fails when {result_length} exceeds the max BigInt size.
    return {};
  }
  DisallowGarbageCollection no_gc;
  bool result_sign = bigint::AddSigned(result->rw_digits(), x->digits(), xsign,
                                       y->digits(), ysign);
  result->set_sign(result_sign);
  return MutableBigInt::MakeImmutable(result);
}

MaybeHandle<BigInt> BigInt::Subtract(Isolate* isolate, DirectHandle<BigInt> x,
                                     DirectHandle<BigInt> y) {
  if (y->is_zero()) return indirect_handle(x, isolate);
  if (x->is_zero()) return UnaryMinus(isolate, y);
  bool xsign = x->sign();
  bool ysign = y->sign();
  uint32_t result_length = bigint::SubtractSignedResultLength(
      x->length(), y->length(), xsign == ysign);
  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, result_length).ToHandle(&result)) {
    // Allocation fails when {result_length} exceeds the max BigInt size.
    return {};
  }
  DisallowGarbageCollection no_gc;
  bool result_sign = bigint::SubtractSigned(result->rw_digits(), x->digits(),
                                            xsign, y->digits(), ysign);
  result->set_sign(result_sign);
  return MutableBigInt::MakeImmutable(result);
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
ComparisonResult BigInt::CompareToBigInt(DirectHandle<BigInt> x,
                                         DirectHandle<BigInt> y) {
  bool x_sign = x->sign();
  if (x_sign != y->sign()) return UnequalSign(x_sign);

  int result = bigint::Compare(x->digits(), y->digits());
  if (result > 0) return AbsoluteGreater(x_sign);
  if (result < 0) return AbsoluteLess(x_sign);
  return ComparisonResult::kEqual;
}

bool BigInt::EqualToBigInt(Tagged<BigInt> x, Tagged<BigInt> y) {
  if (x->sign() != y->sign()) return false;
  if (x->length() != y->length()) return false;
  for (uint32_t i = 0; i < x->length(); i++) {
    if (x->digit(i) != y->digit(i)) return false;
  }
  return true;
}

MaybeHandle<BigInt> BigInt::Increment(Isolate* isolate,
                                      DirectHandle<BigInt> x) {
  if (x->sign()) {
    Handle<MutableBigInt> result = MutableBigInt::AbsoluteSubOne(isolate, x);
    result->set_sign(true);
    return MutableBigInt::MakeImmutable(result);
  } else {
    return MutableBigInt::MakeImmutable(
        MutableBigInt::AbsoluteAddOne(isolate, x, false));
  }
}

MaybeHandle<BigInt> BigInt::Decrement(Isolate* isolate,
                                      DirectHandle<BigInt> x) {
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
                                                DirectHandle<BigInt> x,
                                                DirectHandle<String> y) {
  // a. Let ny be StringToBigInt(y);
  MaybeDirectHandle<BigInt> maybe_ny = StringToBigInt(isolate, y);
  // b. If ny is NaN, return undefined.
  DirectHandle<BigInt> ny;
  if (!maybe_ny.ToHandle(&ny)) {
    if (isolate->has_exception()) {
      return Nothing<ComparisonResult>();
    } else {
      return Just(ComparisonResult::kUndefined);
    }
  }
  // c. Return BigInt::lessThan(x, ny).
  return Just(CompareToBigInt(x, ny));
}

Maybe<bool> BigInt::EqualToString(Isolate* isolate, DirectHandle<BigInt> x,
                                  DirectHandle<String> y) {
  // a. Let n be StringToBigInt(y).
  MaybeDirectHandle<BigInt> maybe_n = StringToBigInt(isolate, y);
  // b. If n is NaN, return false.
  DirectHandle<BigInt> n;
  if (!maybe_n.ToHandle(&n)) {
    if (isolate->has_exception()) {
      return Nothing<bool>();
    } else {
      return Just(false);
    }
  }
  // c. Return the result of x == n.
  return Just(EqualToBigInt(*x, *n));
}

bool BigInt::EqualToNumber(DirectHandle<BigInt> x, DirectHandle<Object> y) {
  DCHECK(IsNumber(*y));
  // a. If x or y are any of NaN, +∞, or -∞, return false.
  // b. If the mathematical value of x is equal to the mathematical value of y,
  //    return true, otherwise return false.
  if (IsSmi(*y)) {
    int value = Smi::ToInt(*y);
    if (value == 0) return x->is_zero();
    // Any multi-digit BigInt is bigger than a Smi.
    static_assert(sizeof(digit_t) >= sizeof(value));
    return (x->length() == 1) && (x->sign() == (value < 0)) &&
           (x->digit(0) ==
            static_cast<digit_t>(std::abs(static_cast<int64_t>(value))));
  }
  DCHECK(IsHeapNumber(*y));
  double value = Cast<HeapNumber>(y)->value();
  return CompareToDouble(x, value) == ComparisonResult::kEqual;
}

ComparisonResult BigInt::CompareToNumber(DirectHandle<BigInt> x,
                                         DirectHandle<Object> y) {
  DCHECK(IsNumber(*y));
  if (IsSmi(*y)) {
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
    static_assert(sizeof(digit_t) >= sizeof(y_value));
    if (x->length() > 1) return AbsoluteGreater(x_sign);

    digit_t abs_value = std::abs(static_cast<int64_t>(y_value));
    digit_t x_digit = x->digit(0);
    if (x_digit > abs_value) return AbsoluteGreater(x_sign);
    if (x_digit < abs_value) return AbsoluteLess(x_sign);
    return ComparisonResult::kEqual;
  }
  DCHECK(IsHeapNumber(*y));
  double value = Cast<HeapNumber>(y)->value();
  return CompareToDouble(x, value);
}

ComparisonResult BigInt::CompareToDouble(DirectHandle<BigInt> x, double y) {
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
  uint64_t double_bits = base::bit_cast<uint64_t>(y);
  int32_t raw_exponent =
      static_cast<int32_t>(double_bits >>
                           base::Double::kPhysicalSignificandSize) &
      0x7FF;
  uint64_t mantissa = double_bits & base::Double::kSignificandMask;
  // Non-finite doubles are handled above.
  DCHECK_NE(raw_exponent, 0x7FF);
  int32_t exponent = raw_exponent - 0x3FF;
  if (exponent < 0) {
    // The absolute value of the double is less than 1. Only 0n has an
    // absolute value smaller than that, but we've already covered that case.
    DCHECK(!x->is_zero());
    return AbsoluteGreater(x_sign);
  }
  uint32_t x_length = x->length();
  digit_t x_msd = x->digit(x_length - 1);
  uint32_t msd_leading_zeros = base::bits::CountLeadingZeros(x_msd);
  uint32_t x_bitlength = x_length * kDigitBits - msd_leading_zeros;
  uint32_t y_bitlength = exponent + 1;
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
  const uint32_t kMantissaTopBit = 52;  // 0-indexed.
  // 0-indexed position of {x}'s most significant bit within the {msd}.
  uint32_t msd_topbit = kDigitBits - 1 - msd_leading_zeros;
  DCHECK_EQ(msd_topbit, (x_bitlength - 1) % kDigitBits);
  // Shifted chunk of {mantissa} for comparing with {digit}.
  digit_t compare_mantissa;
  // Number of unprocessed bits in {mantissa}. We'll keep them shifted to
  // the left (i.e. most significant part) of the underlying uint64_t.
  uint32_t remaining_mantissa_bits = 0;

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
  static_assert(BigInt::kMaxLength < kMaxInt);
  for (int32_t digit_index = x_length - 2; digit_index >= 0; digit_index--) {
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

namespace {

void RightTrimString(Isolate* isolate, DirectHandle<SeqOneByteString> string,
                     int chars_allocated, int chars_written) {
  DCHECK_LE(chars_written, chars_allocated);
  if (chars_written == chars_allocated) return;
  int string_size =
      ALIGN_TO_ALLOCATION_ALIGNMENT(SeqOneByteString::SizeFor(chars_allocated));
  int needed_size =
      ALIGN_TO_ALLOCATION_ALIGNMENT(SeqOneByteString::SizeFor(chars_written));
  if (needed_size < string_size && !isolate->heap()->IsLargeObject(*string)) {
    isolate->heap()->NotifyObjectSizeChange(*string, string_size, needed_size,
                                            ClearRecordedSlots::kNo);
  }
  string->set_length(chars_written, kReleaseStore);
}

}  // namespace

MaybeHandle<String> BigInt::ToString(Isolate* isolate,
                                     DirectHandle<BigInt> bigint, int radix,
                                     ShouldThrow should_throw) {
  if (bigint->is_zero()) {
    return isolate->factory()->zero_string();
  }
  const bool sign = bigint->sign();
  uint32_t chars_allocated;
  uint32_t chars_written;
  Handle<SeqOneByteString> result;
  if (bigint->length() == 1 && radix == 10) {
    // Fast path for the most common case, to avoid call/dispatch overhead.
    // The logic is the same as what the full implementation does below,
    // just inlined and specialized for the preconditions.
    // Microbenchmarks rejoice!
    digit_t digit = bigint->digit(0);
    uint32_t bit_length = kDigitBits - base::bits::CountLeadingZeros(digit);
    constexpr uint32_t kShift = 7;
    // This is Math.log2(10) * (1 << kShift), scaled just far enough to
    // make the computations below always precise (after rounding).
    constexpr uint32_t kShiftedBitsPerChar = 425;
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
      chars_written = chars_allocated - static_cast<uint32_t>(out - start);
      std::memmove(start, out, chars_written);
      memset(start + chars_written, 0, chars_allocated - chars_written);
    }
  } else {
    // Generic path, handles anything.
    DCHECK(radix >= 2 && radix <= 36);
    chars_allocated =
        bigint::ToStringResultLength(bigint->digits(), radix, sign);
    if (chars_allocated > String::kMaxLength) {
      if (should_throw == kThrowOnError) {
        THROW_NEW_ERROR(isolate, NewInvalidStringLengthError());
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
        characters, &chars_written, bigint->digits(), radix, sign);
    if (status == bigint::Status::kInterrupted) {
      AllowGarbageCollection terminating_anyway;
      isolate->TerminateExecution();
      return {};
    }
  }

  // Right-trim any over-allocation (which can happen due to conservative
  // estimates).
  RightTrimString(isolate, result, chars_allocated, chars_written);
#if DEBUG
  // Verify that all characters have been written.
  DCHECK(result->length() == chars_written);
  DisallowGarbageCollection no_gc;
  uint8_t* chars = result->GetChars(no_gc);
  for (uint32_t i = 0; i < chars_written; i++) {
    DCHECK_NE(chars[i], bigint::kStringZapValue);
  }
#endif
  return result;
}

DirectHandle<String> BigInt::NoSideEffectsToString(
    Isolate* isolate, DirectHandle<BigInt> bigint) {
  if (bigint->is_zero()) {
    return isolate->factory()->zero_string();
  }
  // The threshold is chosen such that the operation will be fast enough to
  // not need interrupt checks. This function is meant for producing human-
  // readable error messages, so super-long results aren't useful anyway.
  if (bigint->length() > 100) {
    return isolate->factory()->NewStringFromStaticChars(
        "<a very large BigInt>");
  }

  uint32_t chars_allocated =
      bigint::ToStringResultLength(bigint->digits(), 10, bigint->sign());
  DCHECK_LE(chars_allocated, String::kMaxLength);
  DirectHandle<SeqOneByteString> result =
      isolate->factory()
          ->NewRawOneByteString(chars_allocated)
          .ToHandleChecked();
  uint32_t chars_written = chars_allocated;
  DisallowGarbageCollection no_gc;
  char* characters = reinterpret_cast<char*>(result->GetChars(no_gc));
  std::unique_ptr<bigint::Processor, bigint::Processor::Destroyer>
      non_interruptible_processor(
          bigint::Processor::New(new bigint::Platform()));
  non_interruptible_processor->ToString(characters, &chars_written,
                                        bigint->digits(), 10, bigint->sign());
  RightTrimString(isolate, result, chars_allocated, chars_written);
  return result;
}

MaybeHandle<BigInt> BigInt::FromNumber(Isolate* isolate,
                                       DirectHandle<Object> number) {
  DCHECK(IsNumber(*number));
  if (IsSmi(*number)) {
    return MutableBigInt::NewFromInt(isolate, Smi::ToInt(*number));
  }
  double value = Cast<HeapNumber>(*number)->value();
  if (!std::isfinite(value) || (DoubleToInteger(value) != value)) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kBigIntFromNumber, number));
  }
  return MutableBigInt::NewFromDouble(isolate, value);
}

template <template <typename> typename HandleType>
  requires(std::is_convertible_v<HandleType<Object>, DirectHandle<Object>>)
typename HandleType<BigInt>::MaybeType BigInt::FromObject(
    Isolate* isolate, HandleType<Object> obj) {
  if (IsJSReceiver(*obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, obj,
        JSReceiver::ToPrimitive(isolate, Cast<JSReceiver>(obj),
                                ToPrimitiveHint::kNumber));
  }

  if (IsBoolean(*obj)) {
    return MutableBigInt::NewFromInt(isolate,
                                     Object::BooleanValue(*obj, isolate));
  }
  if (IsBigInt(*obj)) {
    return Cast<BigInt>(obj);
  }
  if (IsString(*obj)) {
    HandleType<BigInt> n;
    if (!StringToBigInt(isolate, Cast<String>(obj)).ToHandle(&n)) {
      if (isolate->has_exception()) {
        return {};
      } else {
        DirectHandle<String> str = Cast<String>(obj);
        constexpr uint32_t kMaxRenderedLength = 1000;
        if (str->length() > kMaxRenderedLength) {
          Factory* factory = isolate->factory();
          Handle<String> prefix =
              factory->NewProperSubString(str, 0, kMaxRenderedLength);
          Handle<SeqTwoByteString> ellipsis =
              factory->NewRawTwoByteString(1).ToHandleChecked();
          ellipsis->SeqTwoByteStringSet(0, 0x2026);
          str = factory->NewConsString(prefix, ellipsis).ToHandleChecked();
        }
        THROW_NEW_ERROR(
            isolate, NewSyntaxError(MessageTemplate::kBigIntFromObject, str));
      }
    }
    return n;
  }

  THROW_NEW_ERROR(isolate,
                  NewTypeError(MessageTemplate::kBigIntFromObject, obj));
}

template V8_EXPORT_PRIVATE MaybeDirectHandle<BigInt> BigInt::FromObject(
    Isolate* isolate, DirectHandle<Object> obj);
template V8_EXPORT_PRIVATE MaybeIndirectHandle<BigInt> BigInt::FromObject(
    Isolate* isolate, IndirectHandle<Object> obj);

DirectHandle<Number> BigInt::ToNumber(Isolate* isolate,
                                      DirectHandle<BigInt> x) {
  if (x->is_zero()) return direct_handle(Smi::zero(), isolate);
  if (x->length() == 1 && x->digit(0) < Smi::kMaxValue) {
    int value = static_cast<int>(x->digit(0));
    if (x->sign()) value = -value;
    return direct_handle(Smi::FromInt(value), isolate);
  }
  double result = MutableBigInt::ToDouble(x);
  return isolate->factory()->NewHeapNumber(result);
}

double MutableBigInt::ToDouble(DirectHandle<BigIntBase> x) {
  if (x->is_zero()) return 0.0;
  uint32_t x_length = x->length();
  digit_t x_msd = x->digit(x_length - 1);
  uint32_t msd_leading_zeros = base::bits::CountLeadingZeros(x_msd);
  uint32_t x_bitlength = x_length * kDigitBits - msd_leading_zeros;
  if (x_bitlength > 1024) return x->sign() ? -V8_INFINITY : V8_INFINITY;
  uint64_t exponent = x_bitlength - 1;
  // We need the most significant bit shifted to the position of a double's
  // "hidden bit". We also need to hide that MSB, so we shift it out.
  uint64_t current_digit = x_msd;
  uint32_t digit_index = x_length - 1;
  uint32_t shift = msd_leading_zeros + 1 + (64 - kDigitBits);
  DCHECK_LE(1, shift);
  DCHECK_LE(shift, 64);
  uint64_t mantissa = (shift == 64) ? 0 : current_digit << shift;
  mantissa >>= 12;
  int32_t mantissa_bits_unset = shift - 12;
  // If not all mantissa bits are defined yet, get more digits as needed.
  if (mantissa_bits_unset >= static_cast<int32_t>(kDigitBits) &&
      digit_index > 0) {
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
  return base::bit_cast<double>(double_bits);
}

// This is its own function to simplify control flow. The meaning of the
// parameters is defined by {ToDouble}'s local variable usage.
MutableBigInt::Rounding MutableBigInt::DecideRounding(
    DirectHandle<BigIntBase> x, int mantissa_bits_unset, int digit_index,
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
  uint32_t len = length();
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
    Isolate* isolate, DirectHandle<BigIntBase> x, bool sign,
    Tagged<MutableBigInt> result_storage) {
  uint32_t input_length = x->length();
  // The addition will overflow into a new digit if all existing digits are
  // at maximum.
  bool will_overflow = true;
  for (uint32_t i = 0; i < input_length; i++) {
    if (!digit_ismax(x->digit(i))) {
      will_overflow = false;
      break;
    }
  }
  uint32_t result_length = input_length + will_overflow;
  Handle<MutableBigInt> result(result_storage, isolate);
  if (result_storage.is_null()) {
    if (!New(isolate, result_length).ToHandle(&result)) return {};
  } else {
    DCHECK(result->length() == result_length);
  }
  if (input_length == 0) {
    result->set_digit(0, 1);
  } else if (input_length == 1 && !will_overflow) {
    result->set_digit(0, x->digit(0) + 1);
  } else {
    bigint::AddOne(result->rw_digits(), x->digits());
  }
  result->set_sign(sign);
  return result;
}

// Subtracts 1 from the absolute value of {x}. {x} must not be zero.
Handle<MutableBigInt> MutableBigInt::AbsoluteSubOne(
    Isolate* isolate, DirectHandle<BigIntBase> x) {
  DCHECK(!x->is_zero());
  uint32_t length = x->length();
  Handle<MutableBigInt> result = New(isolate, length).ToHandleChecked();
  if (length == 1) {
    result->set_digit(0, x->digit(0) - 1);
  } else {
    bigint::SubtractOne(result->rw_digits(), x->digits());
  }
  return result;
}

void Terminate(Isolate* isolate) { isolate->TerminateExecution(); }
// {LocalIsolate} doesn't support interruption or termination.
void Terminate(LocalIsolate* isolate) { UNREACHABLE(); }

template <typename IsolateT>
MaybeHandle<BigInt> BigInt::Allocate(IsolateT* isolate,
                                     bigint::FromStringAccumulator* accumulator,
                                     bool negative, AllocationType allocation) {
  uint32_t digits = accumulator->ResultLength();
  DCHECK_LE(digits, kMaxLength);
  Handle<MutableBigInt> result =
      MutableBigInt::New(isolate, digits, allocation).ToHandleChecked();
  bigint::Status status =
      isolate->bigint_processor()->FromString(result->rw_digits(), accumulator);
  if (status == bigint::Status::kInterrupted) {
    Terminate(isolate);
    return {};
  }
  if (digits > 0) result->set_sign(negative);
  return MutableBigInt::MakeImmutable(result);
}
template MaybeHandle<BigInt> BigInt::Allocate(Isolate*,
                                              bigint::FromStringAccumulator*,
                                              bool, AllocationType);
template MaybeHandle<BigInt> BigInt::Allocate(LocalIsolate*,
                                              bigint::FromStringAccumulator*,
                                              bool, AllocationType);

// The serialization format MUST NOT CHANGE without updating the format
// version in value-serializer.cc!
uint32_t BigInt::GetBitfieldForSerialization() const {
  // In order to make the serialization format the same on 32/64 bit builds,
  // we convert the length-in-digits to length-in-bytes for serialization.
  // Being able to do this depends on having enough LengthBits:
  static_assert(kMaxLength * kDigitSize <= LengthBits::kMax);
  uint32_t bytelength = length() * kDigitSize;
  return SignBits::encode(sign()) | LengthBits::encode(bytelength);
}

size_t BigInt::DigitsByteLengthForBitfield(uint32_t bitfield) {
  return LengthBits::decode(bitfield);
}

// The serialization format MUST NOT CHANGE without updating the format
// version in value-serializer.cc!
void BigInt::SerializeDigits(uint8_t* storage, size_t storage_length) {
  size_t num_digits_to_write = storage_length / kDigitSize;
  // {storage_length} should have been computed from {length()}.
  DCHECK_EQ(num_digits_to_write, length());
#if defined(V8_TARGET_LITTLE_ENDIAN)
  memcpy(storage, raw_digits(), num_digits_to_write * kDigitSize);
#elif defined(V8_TARGET_BIG_ENDIAN)
  digit_t* digit_storage = reinterpret_cast<digit_t*>(storage);
  const digit_t* digit = reinterpret_cast<const digit_t*>(raw_digits());
  for (size_t i = 0; i < num_digits_to_write; i++) {
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
  uint32_t bytelength = LengthBits::decode(bitfield);
  DCHECK_EQ(static_cast<uint32_t>(digits_storage.length()), bytelength);
  bool sign = SignBits::decode(bitfield);
  uint32_t length = (bytelength + kDigitSize - 1) / kDigitSize;  // Round up.
  // There is no -0n. Reject corrupted serialized data.
  if (length == 0 && sign == true) return {};
  Handle<MutableBigInt> result =
      Cast<MutableBigInt>(isolate->factory()->NewBigInt(length));
  result->initialize_bitfield(sign, length);
  UnalignedValueMember<digit_t>* digits = result->raw_digits();
#if defined(V8_TARGET_LITTLE_ENDIAN)
  memcpy(digits, digits_storage.begin(), bytelength);
  void* padding_start =
      reinterpret_cast<void*>(reinterpret_cast<Address>(digits) + bytelength);
  memset(padding_start, 0, length * kDigitSize - bytelength);
#elif defined(V8_TARGET_BIG_ENDIAN)
  digit_t* digit = reinterpret_cast<digit_t*>(digits);
  const digit_t* digit_storage =
      reinterpret_cast<const digit_t*>(digits_storage.begin());
  for (uint32_t i = 0; i < bytelength / kDigitSize; i++) {
    *digit = ByteReverse(*digit_storage);
    digit_storage++;
    digit++;
  }
  if (bytelength % kDigitSize) {
    *digit = 0;
    uint8_t* digit_byte = reinterpret_cast<uint8_t*>(digit);
    digit_byte += sizeof(*digit) - 1;
    const uint8_t* digit_storage_byte =
        reinterpret_cast<const uint8_t*>(digit_storage);
    for (uint32_t i = 0; i < bytelength % kDigitSize; i++) {
      *digit_byte = *digit_storage_byte;
      digit_byte--;
      digit_storage_byte++;
    }
  }
#endif  // V8_TARGET_BIG_ENDIAN
  return MutableBigInt::MakeImmutable(result);
}

DirectHandle<BigInt> BigInt::AsIntN(Isolate* isolate, uint64_t n,
                                    DirectHandle<BigInt> x) {
  if (x->is_zero() || n > kMaxLengthBits) return x;
  if (n == 0) return MutableBigInt::Zero(isolate);
  int needed_length =
      bigint::AsIntNResultLength(x->digits(), x->sign(), static_cast<int>(n));
  if (needed_length == -1) return x;
  Handle<MutableBigInt> result =
      MutableBigInt::New(isolate, needed_length).ToHandleChecked();
  bool negative = bigint::AsIntN(result->rw_digits(), x->digits(), x->sign(),
                                 static_cast<int>(n));
  result->set_sign(negative);
  return MutableBigInt::MakeImmutable(result);
}

MaybeDirectHandle<BigInt> BigInt::AsUintN(Isolate* isolate, uint64_t n,
                                          DirectHandle<BigInt> x) {
  if (x->is_zero()) return x;
  if (n == 0) return MutableBigInt::Zero(isolate);
  Handle<MutableBigInt> result;
  if (x->sign()) {
    if (n > kMaxLengthBits) {
      return ThrowBigIntTooBig<BigInt>(isolate);
    }
    int result_length = bigint::AsUintN_Neg_ResultLength(static_cast<int>(n));
    result = MutableBigInt::New(isolate, result_length).ToHandleChecked();
    bigint::AsUintN_Neg(result->rw_digits(), x->digits(), static_cast<int>(n));
  } else {
    if (n >= kMaxLengthBits) return x;
    int result_length =
        bigint::AsUintN_Pos_ResultLength(x->digits(), static_cast<int>(n));
    if (result_length < 0) return x;
    result = MutableBigInt::New(isolate, result_length).ToHandleChecked();
    bigint::AsUintN_Pos(result->rw_digits(), x->digits(), static_cast<int>(n));
  }
  DCHECK(!result->sign());
  return MutableBigInt::MakeImmutable(result);
}

Handle<BigInt> BigInt::FromInt64(Isolate* isolate, int64_t n) {
  if (n == 0) return MutableBigInt::Zero(isolate);
  static_assert(kDigitBits == 64 || kDigitBits == 32);
  uint32_t length = 64 / kDigitBits;
  Handle<MutableBigInt> result =
      Cast<MutableBigInt>(isolate->factory()->NewBigInt(length));
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
  static_assert(kDigitBits == 64 || kDigitBits == 32);
  uint32_t length = 64 / kDigitBits;
  Handle<MutableBigInt> result =
      Cast<MutableBigInt>(isolate->factory()->NewBigInt(length));
  result->initialize_bitfield(false, length);
  result->set_64_bits(n);
  return MutableBigInt::MakeImmutable(result);
}

MaybeDirectHandle<BigInt> BigInt::FromWords64(Isolate* isolate, int sign_bit,
                                              uint32_t words64_count,
                                              const uint64_t* words) {
  if (words64_count > kMaxLength / (64 / kDigitBits)) {
    return ThrowBigIntTooBig<BigInt>(isolate);
  }
  if (words64_count == 0) return MutableBigInt::Zero(isolate);
  static_assert(kDigitBits == 64 || kDigitBits == 32);
  uint32_t length = (64 / kDigitBits) * words64_count;
  DCHECK_GT(length, 0);
  if (kDigitBits == 32 && words[words64_count - 1] <= (1ULL << 32)) length--;

  Handle<MutableBigInt> result;
  if (!MutableBigInt::New(isolate, length).ToHandle(&result)) return {};

  result->set_sign(sign_bit);
  if (kDigitBits == 64) {
    for (uint32_t i = 0; i < length; ++i) {
      result->set_digit(i, static_cast<digit_t>(words[i]));
    }
  } else {
    for (uint32_t i = 0; i < length; i += 2) {
      digit_t lo = static_cast<digit_t>(words[i / 2]);
      digit_t hi = static_cast<digit_t>(words[i / 2] >> 32);
      result->set_digit(i, lo);
      if (i + 1 < length) result->set_digit(i + 1, hi);
    }
  }

  return MutableBigInt::MakeImmutable(result);
}

uint32_t BigInt::Words64Count() {
  static_assert(kDigitBits == 64 || kDigitBits == 32);
  return length() / (64 / kDigitBits) +
         (kDigitBits == 32 && length() % 2 == 1 ? 1 : 0);
}

void BigInt::ToWordsArray64(int* sign_bit, uint32_t* words64_count,
                            uint64_t* words) {
  DCHECK_NE(sign_bit, nullptr);
  DCHECK_NE(words64_count, nullptr);
  *sign_bit = sign();
  uint32_t available_words = *words64_count;
  *words64_count = Words64Count();
  if (available_words == 0) return;
  DCHECK_NE(words, nullptr);

  uint32_t len = length();
  if (kDigitBits == 64) {
    for (uint32_t i = 0; i < len && i < available_words; ++i)
      words[i] = digit(i);
  } else {
    for (uint32_t i = 0; i < len && available_words > 0; i += 2) {
      uint64_t lo = digit(i);
      uint64_t hi = (i + 1) < len ? digit(i + 1) : 0;
      words[i / 2] = lo | (hi << 32);
      available_words--;
    }
  }
}

uint64_t MutableBigInt::GetRawBits(BigIntBase* x, bool* lossless) {
  if (lossless != nullptr) *lossless = true;
  if (x->is_zero()) return 0;
  uint32_t len = x->length();
  static_assert(kDigitBits == 64 || kDigitBits == 32);
  if (lossless != nullptr && len > 64 / kDigitBits) *lossless = false;
  uint64_t raw = static_cast<uint64_t>(x->digit(0));
  if (kDigitBits == 32 && len > 1) {
    raw |= static_cast<uint64_t>(x->digit(1)) << 32;
  }
  // Simulate two's complement. MSVC dislikes "-raw".
  return x->sign() ? ((~raw) + 1u) : raw;
}

int64_t BigInt::AsInt64(bool* lossless) {
  uint64_t raw = MutableBigInt::GetRawBits(this, lossless);
  int64_t result = static_cast<int64_t>(raw);
  if (lossless != nullptr && (result < 0) != sign()) *lossless = false;
  return result;
}

uint64_t BigInt::AsUint64(bool* lossless) {
  uint64_t result = MutableBigInt::GetRawBits(this, lossless);
  if (lossless != nullptr && sign()) *lossless = false;
  return result;
}

void MutableBigInt::set_64_bits(uint64_t bits) {
  static_assert(kDigitBits == 64 || kDigitBits == 32);
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
  uint32_t len = length();
  os << "\n- length: " << len;
  os << "\n- sign: " << sign();
  if (len > 0) {
    os << "\n- digits:";
    for (uint32_t i = 0; i < len; i++) {
      os << "\n    0x" << std::hex << digit(i);
    }
  }
  os << std::dec << "\n";
}
#endif  // OBJECT_PRINT

void MutableBigInt_AbsoluteAddAndCanonicalize(Address result_addr,
                                              Address x_addr, Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::Add(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

int32_t MutableBigInt_AbsoluteCompare(Address x_addr, Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));

  return bigint::Compare(x->digits(), y->digits());
}

void MutableBigInt_AbsoluteSubAndCanonicalize(Address result_addr,
                                              Address x_addr, Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::Subtract(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

// Returns 0 if it succeeded to obtain the result of multiplication.
// Returns 1 if the computation is interrupted.
int32_t MutableBigInt_AbsoluteMulAndCanonicalize(Address result_addr,
                                                 Address x_addr,
                                                 Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  Isolate* isolate;
  if (!GetIsolateFromHeapObject(x, &isolate)) {
    // We should always get the isolate from the BigInt.
    UNREACHABLE();
  }

  bigint::Status status = isolate->bigint_processor()->Multiply(
      result->rw_digits(), x->digits(), y->digits());
  if (status == bigint::Status::kInterrupted) {
    return 1;
  }

  MutableBigInt::Canonicalize(result);
  return 0;
}

int32_t MutableBigInt_AbsoluteDivAndCanonicalize(Address result_addr,
                                                 Address x_addr,
                                                 Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));
  DCHECK_GE(result->length(),
            bigint::DivideResultLength(x->digits(), y->digits()));

  Isolate* isolate;
  if (!GetIsolateFromHeapObject(x, &isolate)) {
    // We should always get the isolate from the BigInt.
    UNREACHABLE();
  }

  bigint::Status status = isolate->bigint_processor()->Divide(
      result->rw_digits(), x->digits(), y->digits());
  if (status == bigint::Status::kInterrupted) {
    return 1;
  }

  MutableBigInt::Canonicalize(result);
  return 0;
}

int32_t MutableBigInt_AbsoluteModAndCanonicalize(Address result_addr,
                                                 Address x_addr,
                                                 Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  Isolate* isolate;
  if (!GetIsolateFromHeapObject(x, &isolate)) {
    // We should always get the isolate from the BigInt.
    UNREACHABLE();
  }

  bigint::Status status = isolate->bigint_processor()->Modulo(
      result->rw_digits(), x->digits(), y->digits());
  if (status == bigint::Status::kInterrupted) {
    return 1;
  }

  MutableBigInt::Canonicalize(result);
  return 0;
}

void MutableBigInt_BitwiseAndPosPosAndCanonicalize(Address result_addr,
                                                   Address x_addr,
                                                   Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseAnd_PosPos(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseAndNegNegAndCanonicalize(Address result_addr,
                                                   Address x_addr,
                                                   Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseAnd_NegNeg(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseAndPosNegAndCanonicalize(Address result_addr,
                                                   Address x_addr,
                                                   Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseAnd_PosNeg(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseOrPosPosAndCanonicalize(Address result_addr,
                                                  Address x_addr,
                                                  Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseOr_PosPos(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseOrNegNegAndCanonicalize(Address result_addr,
                                                  Address x_addr,
                                                  Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseOr_NegNeg(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseOrPosNegAndCanonicalize(Address result_addr,
                                                  Address x_addr,
                                                  Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseOr_PosNeg(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseXorPosPosAndCanonicalize(Address result_addr,
                                                   Address x_addr,
                                                   Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseXor_PosPos(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseXorNegNegAndCanonicalize(Address result_addr,
                                                   Address x_addr,
                                                   Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseXor_NegNeg(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_BitwiseXorPosNegAndCanonicalize(Address result_addr,
                                                   Address x_addr,
                                                   Address y_addr) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<BigInt> y = Cast<BigInt>(Tagged<Object>(y_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::BitwiseXor_PosNeg(result->rw_digits(), x->digits(), y->digits());
  MutableBigInt::Canonicalize(result);
}

void MutableBigInt_LeftShiftAndCanonicalize(Address result_addr, Address x_addr,
                                            intptr_t shift) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));

  bigint::LeftShift(result->rw_digits(), x->digits(), shift);
  MutableBigInt::Canonicalize(result);
}

uint32_t RightShiftResultLength(Address x_addr, uint32_t x_sign,
                                intptr_t shift) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  bigint::RightShiftState state;
  uint32_t length =
      bigint::RightShift_ResultLength(x->digits(), x_sign, shift, &state);
  // {length} should be non-negative and fit in 30 bits.
  DCHECK_EQ(length >> BigInt::kLengthFieldBits, 0);
  return (static_cast<uint32_t>(state.must_round_down)
          << BigInt::kLengthFieldBits) |
         length;
}

void MutableBigInt_RightShiftAndCanonicalize(Address result_addr,
                                             Address x_addr, intptr_t shift,
                                             uint32_t must_round_down) {
  Tagged<BigInt> x = Cast<BigInt>(Tagged<Object>(x_addr));
  Tagged<MutableBigInt> result =
      Cast<MutableBigInt>(Tagged<Object>(result_addr));
  bigint::RightShiftState state{must_round_down == 1};
  bigint::RightShift(result->rw_digits(), x->digits(), shift, state);
  MutableBigInt::Canonicalize(result);
}

#include "src/objects/object-macros-undef.h"

}  // namespace internal
}  // namespace v8
