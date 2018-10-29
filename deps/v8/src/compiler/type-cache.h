// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPE_CACHE_H_
#define V8_COMPILER_TYPE_CACHE_H_

#include "src/compiler/types.h"
#include "src/date.h"
#include "src/objects/code.h"
#include "src/objects/js-array-buffer.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {
namespace compiler {

class TypeCache final {
 private:
  // This has to be first for the initialization magic to work.
  AccountingAllocator allocator;
  Zone zone_;

 public:
  static TypeCache const& Get();

  TypeCache() : zone_(&allocator, ZONE_NAME) {}

  Type const kInt8 = CreateRange<int8_t>();
  Type const kUint8 = CreateRange<uint8_t>();
  Type const kUint8Clamped = kUint8;
  Type const kUint8OrMinusZeroOrNaN =
      Type::Union(kUint8, Type::MinusZeroOrNaN(), zone());
  Type const kInt16 = CreateRange<int16_t>();
  Type const kUint16 = CreateRange<uint16_t>();
  Type const kInt32 = Type::Signed32();
  Type const kUint32 = Type::Unsigned32();
  Type const kInt64 = CreateRange<int64_t>();
  Type const kUint64 = CreateRange<uint64_t>();
  Type const kFloat32 = Type::Number();
  Type const kFloat64 = Type::Number();
  Type const kBigInt64 = Type::BigInt();
  Type const kBigUint64 = Type::BigInt();

  Type const kHoleySmi = Type::Union(Type::SignedSmall(), Type::Hole(), zone());

  Type const kSingletonZero = CreateRange(0.0, 0.0);
  Type const kSingletonOne = CreateRange(1.0, 1.0);
  Type const kSingletonTen = CreateRange(10.0, 10.0);
  Type const kSingletonMinusOne = CreateRange(-1.0, -1.0);
  Type const kZeroOrMinusZero =
      Type::Union(kSingletonZero, Type::MinusZero(), zone());
  Type const kZeroOrUndefined =
      Type::Union(kSingletonZero, Type::Undefined(), zone());
  Type const kTenOrUndefined =
      Type::Union(kSingletonTen, Type::Undefined(), zone());
  Type const kMinusOneOrZero = CreateRange(-1.0, 0.0);
  Type const kMinusOneToOneOrMinusZeroOrNaN = Type::Union(
      Type::Union(CreateRange(-1.0, 1.0), Type::MinusZero(), zone()),
      Type::NaN(), zone());
  Type const kZeroOrOne = CreateRange(0.0, 1.0);
  Type const kZeroOrOneOrNaN = Type::Union(kZeroOrOne, Type::NaN(), zone());
  Type const kZeroToThirtyOne = CreateRange(0.0, 31.0);
  Type const kZeroToThirtyTwo = CreateRange(0.0, 32.0);
  Type const kZeroish =
      Type::Union(kSingletonZero, Type::MinusZeroOrNaN(), zone());
  Type const kInteger = CreateRange(-V8_INFINITY, V8_INFINITY);
  Type const kIntegerOrMinusZero =
      Type::Union(kInteger, Type::MinusZero(), zone());
  Type const kIntegerOrMinusZeroOrNaN =
      Type::Union(kIntegerOrMinusZero, Type::NaN(), zone());
  Type const kPositiveInteger = CreateRange(0.0, V8_INFINITY);
  Type const kPositiveIntegerOrMinusZero =
      Type::Union(kPositiveInteger, Type::MinusZero(), zone());
  Type const kPositiveIntegerOrNaN =
      Type::Union(kPositiveInteger, Type::NaN(), zone());
  Type const kPositiveIntegerOrMinusZeroOrNaN =
      Type::Union(kPositiveIntegerOrMinusZero, Type::NaN(), zone());

  Type const kAdditiveSafeInteger =
      CreateRange(-4503599627370496.0, 4503599627370496.0);
  Type const kSafeInteger = CreateRange(-kMaxSafeInteger, kMaxSafeInteger);
  Type const kAdditiveSafeIntegerOrMinusZero =
      Type::Union(kAdditiveSafeInteger, Type::MinusZero(), zone());
  Type const kSafeIntegerOrMinusZero =
      Type::Union(kSafeInteger, Type::MinusZero(), zone());
  Type const kPositiveSafeInteger = CreateRange(0.0, kMaxSafeInteger);

  // The FixedArray::length property always containts a smi in the range
  // [0, FixedArray::kMaxLength].
  Type const kFixedArrayLengthType = CreateRange(0.0, FixedArray::kMaxLength);

  // The FixedDoubleArray::length property always containts a smi in the range
  // [0, FixedDoubleArray::kMaxLength].
  Type const kFixedDoubleArrayLengthType =
      CreateRange(0.0, FixedDoubleArray::kMaxLength);

  // The JSArray::length property always contains a tagged number in the range
  // [0, kMaxUInt32].
  Type const kJSArrayLengthType = Type::Unsigned32();

  // The JSArrayBuffer::byte_length property is limited to safe integer range
  // per specification, but on 32-bit architectures is implemented as uint32_t
  // field, so it's in the [0, kMaxUInt32] range in that case.
  Type const kJSArrayBufferByteLengthType =
      CreateRange(0.0, JSArrayBuffer::kMaxByteLength);

  // The type for the JSArrayBufferView::byte_length property is the same as
  // JSArrayBuffer::byte_length above.
  Type const kJSArrayBufferViewByteLengthType = kJSArrayBufferByteLengthType;

  // The type for the JSArrayBufferView::byte_offset property is the same as
  // JSArrayBuffer::byte_length above.
  Type const kJSArrayBufferViewByteOffsetType = kJSArrayBufferByteLengthType;

  // The JSTypedArray::length property always contains a tagged number in the
  // range [0, kMaxSmiValue].
  Type const kJSTypedArrayLengthType = Type::UnsignedSmall();

  // The String::length property always contains a smi in the range
  // [0, String::kMaxLength].
  Type const kStringLengthType = CreateRange(0.0, String::kMaxLength);

  // A time value always contains a tagged number in the range
  // [-kMaxTimeInMs, kMaxTimeInMs].
  Type const kTimeValueType =
      CreateRange(-DateCache::kMaxTimeInMs, DateCache::kMaxTimeInMs);

  // The JSDate::day property always contains a tagged number in the range
  // [1, 31] or NaN.
  Type const kJSDateDayType =
      Type::Union(CreateRange(1, 31.0), Type::NaN(), zone());

  // The JSDate::hour property always contains a tagged number in the range
  // [0, 23] or NaN.
  Type const kJSDateHourType =
      Type::Union(CreateRange(0, 23.0), Type::NaN(), zone());

  // The JSDate::minute property always contains a tagged number in the range
  // [0, 59] or NaN.
  Type const kJSDateMinuteType =
      Type::Union(CreateRange(0, 59.0), Type::NaN(), zone());

  // The JSDate::month property always contains a tagged number in the range
  // [0, 11] or NaN.
  Type const kJSDateMonthType =
      Type::Union(CreateRange(0, 11.0), Type::NaN(), zone());

  // The JSDate::second property always contains a tagged number in the range
  // [0, 59] or NaN.
  Type const kJSDateSecondType = kJSDateMinuteType;

  // The JSDate::value property always contains a tagged number in the range
  // [-kMaxTimeInMs, kMaxTimeInMs] or NaN.
  Type const kJSDateValueType =
      Type::Union(kTimeValueType, Type::NaN(), zone());

  // The JSDate::weekday property always contains a tagged number in the range
  // [0, 6] or NaN.
  Type const kJSDateWeekdayType =
      Type::Union(CreateRange(0, 6.0), Type::NaN(), zone());

  // The JSDate::year property always contains a tagged number in the signed
  // small range or NaN.
  Type const kJSDateYearType =
      Type::Union(Type::SignedSmall(), Type::NaN(), zone());

  // The valid number of arguments for JavaScript functions.
  Type const kArgumentsLengthType =
      Type::Range(0.0, Code::kMaxArguments, zone());

  // The JSArrayIterator::kind property always contains an integer in the
  // range [0, 2], representing the possible IterationKinds.
  Type const kJSArrayIteratorKindType = CreateRange(0.0, 2.0);

 private:
  template <typename T>
  Type CreateRange() {
    return CreateRange(std::numeric_limits<T>::min(),
                       std::numeric_limits<T>::max());
  }

  Type CreateRange(double min, double max) {
    return Type::Range(min, max, zone());
  }

  Zone* zone() { return &zone_; }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPE_CACHE_H_
