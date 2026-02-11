// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-temporal-objects.h"

#include <algorithm>
#include <optional>
#include <set>

#include "src/base/numerics/safe_conversions.h"
#include "src/common/globals.h"
#include "src/date/date.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-temporal-helpers.h"
#include "src/objects/js-temporal-objects-inl.h"
#include "src/objects/js-temporal-zoneinfo64.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/option-utils.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/string-set.h"
#include "src/strings/string-builder-inl.h"
#include "temporal_rs/I128Nanoseconds.hpp"
#include "temporal_rs/OwnedRelativeTo.hpp"
#include "temporal_rs/Unit.hpp"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#include "src/objects/js-date-time-format.h"
#include "src/objects/js-duration-format.h"
#include "unicode/calendar.h"
#include "unicode/unistr.h"
#endif  // V8_INTL_SUPPORT

namespace v8::internal {

namespace {

// Shorten enums with `using`
using temporal_rs::RoundingMode;
using temporal_rs::Unit;

/**
 * This header declare the Abstract Operations defined in the
 * Temporal spec with the enum and struct for them.
 */

template <typename T>
using TemporalResult =
    temporal_rs::diplomat::result<T, temporal_rs::TemporalError>;
template <typename T>
using TemporalAllocatedResult = TemporalResult<std::unique_ptr<T>>;

using temporal::DurationRecord;
using temporal::TimeDurationRecord;

// Options

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldisambiguation
enum class Disambiguation { kCompatible, kEarlier, kLater, kReject };

// https://tc39.es/proposal-temporal/#sec-temporal-toshowcalendaroption
enum class ShowCalendar { kAuto, kAlways, kNever };

// https://tc39.es/proposal-temporal/#table-temporal-unsigned-rounding-modes
enum class UnsignedRoundingMode {
  kInfinity,
  kZero,
  kHalfInfinity,
  kHalfZero,
  kHalfEven
};

// https://tc39.es/proposal-temporal/#sec-temporal-GetTemporalUnit
enum class UnitGroup {
  kDate,
  kTime,
  kDateTime,
};

// Default value as used by GetTemporalUnitValuedOption
enum class DefaultValue {
  kUnset,
  kRequired,
};

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimerecord
enum Completeness {
  kComplete,
  kPartial,
};

// Convenience method for getting the timezone provider
temporal_rs::Provider& TimeZoneProvider() {
  return ZoneInfo64Provider::Singleton().Provider();
}

// Common error strings
static constexpr char kInvalidIsoDate[] = "Invalid ISO date.";
static constexpr char kInvalidTime[] = "Invalid time";
static constexpr char kFiniteInteger[] = "Expected finite integer.";
static constexpr char kIntegerOutOfRange[] = "Integer out of range.";
static constexpr char kOptionMustBeObject[] = "Option must be object:";
static constexpr char kCalendarMustBeString[] = "Calendar must be string.";
static constexpr char kRoundToMissing[] = "Must specify a roundTo parameter.";
static constexpr char kRoundToMustBeObject[] = "roundTo must be an object.";
static constexpr char kYearMustBeObject[] = "year argument must be an object.";
static constexpr char kTimeZoneMissing[] = "Must specify time zone.";
static constexpr char kWithNoPartial[] =
    "Argument to with() must contain some date/time fields.";

#define ORDINARY_CREATE_FROM_CONSTRUCTOR(obj, target, new_target, T)           \
  DirectHandle<JSReceiver> new_target_receiver = Cast<JSReceiver>(new_target); \
  DirectHandle<Map> map;                                                       \
  ASSIGN_RETURN_ON_EXCEPTION(                                                  \
      isolate, map,                                                            \
      JSFunction::GetDerivedMap(isolate, target, new_target_receiver));        \
  DirectHandle<T> object =                                                     \
      Cast<T>(isolate->factory()->NewFastOrSlowJSObjectFromMap(map));

// Helper function to split the string into its two constituent encodings
// and call two different functions depending on the encoding
template <typename RetVal>
RetVal HandleStringEncodings(
    Isolate* isolate, DirectHandle<String> string,
    std::function<RetVal(std::string_view)> utf8_fn,
    std::function<RetVal(std::u16string_view)> utf16_fn) {
  string = String::Flatten(isolate, string);
  DisallowGarbageCollection no_gc;
  auto flat = string->GetFlatContent(no_gc);
  if (flat.IsOneByte()) {
    auto content = flat.ToOneByteVector();
    // reinterpret_cast since std string types accept signed chars
    std::string_view view(reinterpret_cast<const char*>(content.data()),
                          content.size());
    return utf8_fn(view);

  } else {
    auto content = flat.ToUC16Vector();
    std::u16string_view view(reinterpret_cast<const char16_t*>(content.data()),
                             content.size());
    return utf16_fn(view);
  }
}

// Take a Rust Result and turn it into a Maybe, suitable for use
// with error handling macros.
//
// Note that a lot of the types returned by Rust code prefer
// move semantics, try using MOVE_RETURN_ON_EXCEPTION
template <typename ContainedValue>
Maybe<ContainedValue> ExtractRustResult(
    Isolate* isolate, TemporalResult<ContainedValue>&& rust_result) {
  if (rust_result.is_err()) {
    auto err = std::move(rust_result).err().value();
    Handle<String> msg;
    if (err.msg.has_value()) {
      bool success =
          isolate->factory()->NewStringFromUtf8(err.msg.value()).ToHandle(&msg);
      if (!success) {
        msg = isolate->factory()->NewStringFromStaticChars("(utf8 error)");
      }
    } else {
      // We need to have *some* message here even if Rust doesn't have one
      msg = isolate->factory()->NewStringFromStaticChars("Unspecified error.");
    }
    switch (err.kind) {
      case temporal_rs::ErrorKind::Type:
        THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kTemporal, msg));
        break;
      case temporal_rs::ErrorKind::Range:
        THROW_NEW_ERROR(isolate,
                        NewRangeError(MessageTemplate::kTemporal, msg));
        break;
      case temporal_rs::ErrorKind::Syntax:
        THROW_NEW_ERROR(isolate,
                        NewSyntaxError(MessageTemplate::kTemporal, msg));
      case temporal_rs::ErrorKind::Assert:
      case temporal_rs::ErrorKind::Generic:
      default:
        // These cases shouldn't happen; the spec doesn't currently trigger
        // these errors
        THROW_NEW_ERROR(isolate,
                        NewError(MessageTemplate::kTemporalWithArg,
                                 isolate->factory()->NewStringFromStaticChars(
                                     "Internal error:"),
                                 msg));
    }
    return Nothing<ContainedValue>();
  }
  return Just(std::move(rust_result).ok().value());
}

// Helper function to construct a JSType that wraps a RustType (infallible)
template <typename JSType>
MaybeDirectHandle<JSType> ConstructRustWrappingType(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    std::unique_ptr<typename JSType::RustType>&& rust_value) {
  // Managed requires shared ownership
  std::shared_ptr<typename JSType::RustType> rust_shared =
      std::move(rust_value);

  DirectHandle<Managed<typename JSType::RustType>> managed =
      Managed<typename JSType::RustType>::From(isolate, 0, rust_shared);

  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target, JSType)
  object->initialize_with_wrapped_rust_value(*managed);
  return object;
}

// Helper function to construct a JSType that wraps a RustType (fallible)
template <typename JSType>
MaybeDirectHandle<JSType> ConstructRustWrappingType(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    TemporalResult<std::unique_ptr<typename JSType::RustType>>&& rust_result) {
  std::unique_ptr<typename JSType::RustType> rust_value = nullptr;

  MOVE_RETURN_ON_EXCEPTION(isolate, rust_value,
                           ExtractRustResult(isolate, std::move(rust_result)));

  return ConstructRustWrappingType<JSType>(isolate, target, new_target,
                                           std::move(rust_value));
}

// Most of the time you just need JSType's constructor as the target,
// these helpers let you skip passing the argument
template <typename JSType>
MaybeDirectHandle<JSType> ConstructRustWrappingType(
    Isolate* isolate, std::unique_ptr<typename JSType::RustType>&& rust_value) {
  auto ctor = JSType::GetConstructorTarget(isolate);
  return ConstructRustWrappingType<JSType>(isolate, ctor, ctor,
                                           std::move(rust_value));
}

template <typename JSType>
MaybeDirectHandle<JSType> ConstructRustWrappingType(
    Isolate* isolate,
    TemporalResult<std::unique_ptr<typename JSType::RustType>>&& rust_result) {
  auto ctor = JSType::GetConstructorTarget(isolate);
  return ConstructRustWrappingType<JSType>(isolate, ctor, ctor,
                                           std::move(rust_result));
}

}  // namespace

namespace temporal {

// ====== Numeric conversions ======

// Note: All of these IntegralDouble functions MUST
// be given an integral number, typically obtained via
// ToIntegerIfIntegral or ToIntegerWithTruncation.
template <typename IntegerType>
IntegerType CastIntegralDouble(double d) {
  DCHECK((base::IsValueInRangeForNumericType<IntegerType, double>(d)));
  DCHECK_EQ(nearbyint(d), d);
  return static_cast<IntegerType>(d);
}

template <typename IntegerType>
IntegerType ClampIntegralDouble(double d, IntegerType min, IntegerType max) {
  DCHECK_EQ(nearbyint(d), d);
  double clamped =
      std::clamp(d, static_cast<double>(min), static_cast<double>(max));
  return CastIntegralDouble<IntegerType>(clamped);
}

template <typename IntegerType>
IntegerType ClampIntegralDoubleToRange(double d) {
  return ClampIntegralDouble<IntegerType>(
      d, std::numeric_limits<IntegerType>::min(),
      std::numeric_limits<IntegerType>::max());
}

template <typename IntegerType>
Maybe<IntegerType> CheckDoubleInRange(Isolate* isolate, double d) {
  if (!base::IsValueInRangeForNumericType<IntegerType, double>(d)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kIntegerOutOfRange));
  }
  return Just(CastIntegralDouble<IntegerType>(d));
}

// https://tc39.es/proposal-temporal/#sec-temporal-tointegerifintegral
Maybe<double> ToIntegerIfIntegral(Isolate* isolate,
                                  DirectHandle<Object> argument) {
  // 1. Let number be ? ToNumber(argument).
  DirectHandle<Number> number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, number,
                             Object::ToNumber(isolate, argument));
  double number_double = Object::NumberValue(*number);
  // 2. If number is not an integral Number, throw a RangeError exception.
  if (!std::isfinite(number_double) ||
      nearbyint(number_double) != number_double) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kFiniteInteger));
  }
  // 3. Return ℝ(number).
  return Just(number_double);
}

// temporal_rs currently accepts integer types in cases where
// the spec uses a double (and bounds-checks later). This helper
// allows safely converting objects to some known integer type.
//
// TODO(manishearth) This helper should be removed when it is unnecessary.
// Tracked in https://github.com/boa-dev/temporal/issues/334
template <typename IntegerType>
Maybe<IntegerType> ToIntegerTypeIfIntegral(Isolate* isolate,
                                           DirectHandle<Object> argument) {
  double d;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, d,
                             ToIntegerIfIntegral(isolate, argument));
  if (!base::IsValueInRangeForNumericType<IntegerType, double>(d)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kIntegerOutOfRange));
  }

  return Just(static_cast<IntegerType>(d));
}

// https://tc39.es/proposal-temporal/#sec-temporal-tointegerwithtruncation
Maybe<double> ToIntegerWithTruncation(Isolate* isolate,
                                      DirectHandle<Object> argument) {
  // 1. Let number be ? ToNumber(argument).
  DirectHandle<Number> number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, number,
                             Object::ToNumber(isolate, argument));
  double number_double = Object::NumberValue(*number);
  // 2. If number is NaN, +∞𝔽 or -∞𝔽, throw a RangeError exception.
  if (!std::isfinite(number_double)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kFiniteInteger));
  }

  // 3. Return truncate(number).
  return Just(std::trunc(number_double));
}

// Same as ToIntegerWithTruncation, but returns 0 when undefined
Maybe<double> ToIntegerWithTruncationOrZero(Isolate* isolate,
                                            DirectHandle<Object> argument) {
  if (IsUndefined(*argument)) {
    return Just(0.0);
  }
  return ToIntegerWithTruncation(isolate, argument);
}

// https://tc39.es/proposal-temporal/#sec-temporal-topositiveintegerwithtruncation
Maybe<double> ToPositiveIntegerWithTruncation(Isolate* isolate,
                                              DirectHandle<Object> argument) {
  // 1. Let integer be ?ToIntegerWithTruncation(argument).
  double integer;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, integer,
                             ToIntegerWithTruncation(isolate, argument));
  // 2. If integer is ≤ 0, throw a RangeError exception
  if (integer <= 0) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_RANGE_ERROR("Expected positive integer."));
  }

  // 3. Return integer.
  return Just(integer);
}

static constexpr uint64_t kU64HighBitMask = uint64_t{1} << 63;

// Throws RangeErrors for out-of-range BigInts, checking IsValidEpochNanoseconds
Maybe<temporal_rs::I128Nanoseconds> GetI128FromBigInt(
    Isolate* isolate, DirectHandle<BigInt> bigint) {
  static constexpr char kNSOutOfRange[] = "Nanoseconds out of range.";

  if (bigint->Words64Count() > 2) {
    // 3. ! IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
    // RangeError exception.
    //
    // This only performs part of the check, the rest of it is done below with
    // is_valid()
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kNSOutOfRange));
  }

  uint64_t words[2] = {0, 0};
  uint32_t word_count = 2;
  int sign_bit = 0;
  bigint->ToWordsArray64(&sign_bit, &word_count, words);

  if ((words[1] & kU64HighBitMask) != 0) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kNSOutOfRange));
  }

  uint64_t high = words[1];
  if (sign_bit == 1) {
    high |= kU64HighBitMask;
  }

  temporal_rs::I128Nanoseconds ns;

  ns.high = high;
  ns.low = words[0];

  if (!ns.is_valid()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kNSOutOfRange));
  }
  return Just(ns);
#undef NANOSECONDS_RANGE_ERROR
}

MaybeDirectHandle<BigInt> I128ToBigInt(Isolate* isolate,
                                       temporal_rs::I128Nanoseconds ns) {
  uint64_t words[2];
  bool sign_bit;

  if ((ns.high & temporal::kU64HighBitMask) != 0) {
    sign_bit = true;
    words[1] = ns.high & ~temporal::kU64HighBitMask;
  } else {
    sign_bit = false;
    words[1] = ns.high;
  }
  words[0] = ns.low;

  return BigInt::FromWords64(isolate, sign_bit, 2, words);
}

bool IsValidTime(double hour, double minute, double second, double millisecond,
                 double microsecond, double nanosecond) {
  // 1. If hour < 0 or hour > 23, then
  // a. Return false.
  if (hour < 0 || hour > 23) {
    return false;
  }
  // 2. If minute < 0 or minute > 59, then
  // a. Return false.
  if (minute < 0 || minute > 59) {
    return false;
  }
  // 3. If second < 0 or second > 59, then
  // a. Return false.
  if (second < 0 || second > 59) {
    return false;
  }
  // 4. If millisecond < 0 or millisecond > 999, then
  // a. Return false.

  if (millisecond < 0 || millisecond > 999) {
    return false;
  }
  // 5. If microsecond < 0 or microsecond > 999, then
  // a. Return false.
  if (microsecond < 0 || microsecond > 999) {
    return false;
  }
  // 6. If nanosecond < 0 or nanosecond > 999, then
  // a. Return false.
  if (nanosecond < 0 || nanosecond > 999) {
    return false;
  }
  // 7. Return true.
  return true;
}

// https://tc39.es/proposal-temporal/#sec-temporal-isodaysinmonth
int8_t ISODaysInMonth(int32_t year, uint8_t month) {
  switch (month) {
    // 1. If month is 1, 3, 5, 7, 8, 10, or 12, return 31.
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    // 4. Return 28 + MathematicalInLeapYear(EpochTimeForYear(year)).
    case 2:
      if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
        return 29;
      } else {
        return 28;
      }
    // 2. If month is 4, 6, 9, or 11, return 30.
    default:
      return 30;
  }
}

// https://tc39.es/proposal-temporal/#sec-temporal-isvalidisodate
bool IsValidIsoDate(double year, double month, double day) {
  // 1. If month < 1 or month > 12, then
  if (month < 1 || month > 12) {
    // a. Return false.
    return false;
  }

  // This check is technically needed later when we check if things are in the
  // Temporal range, but we do it now to ensure we can safely cast to int32_t
  // before passing to Rust See https://github.com/boa-dev/temporal/issues/334.
  if (!base::IsValueInRangeForNumericType<int32_t, double>(year)) {
    return false;
  }

  // IsValidIsoDate does not care about years that are "out of Temporal range",
  // that gets handled later.
  int32_t year_int = static_cast<int32_t>(year);
  uint8_t month_int = static_cast<uint8_t>(month);
  // 2. Let daysInMonth be ISODaysInMonth(year, month).
  // 3. If day < 1 or day > daysInMonth, then
  if (day < 1 || day > ISODaysInMonth(year_int, month_int)) {
    // a. Return false.
    return false;
  }

  // 4. Return true.
  return true;
}

// ====== Options getters ======

// https://tc39.es/proposal-temporal/#sec-temporal-tomonthcode
Maybe<std::string> ToMonthCode(Isolate* isolate,
                               DirectHandle<Object> argument) {
  static constexpr char kMonthCodeOutOfRange[] = "Month code out of range.";

  // 1. Let monthCode be ?ToPrimitive(argument, string).
  DirectHandle<Object> mc_prim;
  if (IsJSReceiver(*argument)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, mc_prim,
        JSReceiver::ToPrimitive(isolate, Cast<JSReceiver>(argument),
                                ToPrimitiveHint::kString));
  } else {
    mc_prim = argument;
  }

  // 2. If monthCode is not a String, throw a TypeError exception.
  if (!IsString(*mc_prim)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }

  auto month_code = Cast<String>(*mc_prim)->ToStdString();

  // 3. If the length of monthCode is not 3 or 4, throw a RangeError exception.
  if (month_code.size() != 3 && month_code.size() != 4) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }

  // 4. If the first code unit of monthCode is not 0x004D (LATIN CAPITAL LETTER
  // M), throw a RangeError exception.
  if (month_code[0] != 'M') {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }
  // 5. If the second code unit of monthCode is not in the inclusive interval
  // from 0x0030 (DIGIT ZERO) to 0x0039 (DIGIT NINE), throw a RangeError
  // exception.
  if (month_code[1] < '0' || month_code[1] > '9') {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }
  // 6. If the third code unit of monthCode is not in the inclusive interval
  // from 0x0030 (DIGIT ZERO) to 0x0039 (DIGIT NINE), throw a RangeError
  // exception.
  if (month_code[2] < '0' || month_code[2] > '9') {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }
  // 7. If the length of monthCode is 4 and the fourth code unit of monthCode is
  // not 0x004C (LATIN CAPITAL LETTER L), throw a RangeError exception.
  if (month_code.size() == 4 && month_code[3] != 'L') {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }
  // 8. Let monthCodeDigits be the substring of monthCode from 1 to 3.
  // 9. Let monthCodeInteger be ℝ(StringToNumber(monthCodeDigits)).
  // 10. If monthCodeInteger is 0 and the length of monthCode is not 4, throw a
  // RangeError exception.
  if (month_code[1] == '0' && month_code[2] == '0' && month_code.size() != 4) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kMonthCodeOutOfRange));
  }
  // 11. Return monthCode.
  return Just(month_code);
#undef MONTHCODE_RANGE_ERROR
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaloverflow
// Also handles the undefined check from GetOptionsObject
Maybe<temporal_rs::ArithmeticOverflow> ToTemporalOverflowHandleUndefined(
    Isolate* isolate, MaybeDirectHandle<Object> maybe_options,
    const char* method_name) {
  DirectHandle<Object> options;
  // Default is "constrain"
  if (!maybe_options.ToHandle(&options) || IsUndefined(*options))
    return Just(temporal_rs::ArithmeticOverflow(
        temporal_rs::ArithmeticOverflow::Constrain));
  auto overflow_ident = isolate->factory()->overflow_string();
  if (!IsJSReceiver(*options)) {
    // (GetOptionsObject) 3. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR_WITH_ARG(
                                 kOptionMustBeObject, overflow_ident));
  }
  // 2. Return ? GetOption(options, "overflow", « String », « "constrain",
  // "reject" », "constrain").
  return GetStringOption<temporal_rs::ArithmeticOverflow>(
      isolate, Cast<JSReceiver>(options), overflow_ident, method_name,
      std::to_array<const std::string_view>({"constrain", "reject"}),
      std::to_array<temporal_rs::ArithmeticOverflow>(
          {temporal_rs::ArithmeticOverflow::Constrain,
           temporal_rs::ArithmeticOverflow::Reject}),
      temporal_rs::ArithmeticOverflow::Constrain);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getdirectionoption

Maybe<temporal_rs::TransitionDirection> GetDirectionOption(
    Isolate* isolate, DirectHandle<JSReceiver> options,
    const char* method_name) {
  // 1. Let stringValue be ? GetOption(options, "direction", string, « "next",
  // "previous" », required).
  temporal_rs::TransitionDirection dir;

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, dir,
      GetStringOption<temporal_rs::TransitionDirection>(
          isolate, Cast<JSReceiver>(options),
          isolate->factory()->direction_string(), method_name,
          std::to_array<const std::string_view>({"next", "previous"}),
          std::to_array<temporal_rs::TransitionDirection>(
              {temporal_rs::TransitionDirection::Next,
               temporal_rs::TransitionDirection::Previous}),
          std::nullopt));

  return Just(dir);
}
// https://tc39.es/proposal-temporal/#sec-temporal-gettemporaldisambiguationoption
// Also handles the undefined check from GetOptionsObject
Maybe<temporal_rs::Disambiguation>
GetTemporalDisambiguationOptionHandleUndefined(Isolate* isolate,
                                               DirectHandle<Object> options,
                                               const char* method_name) {
  // Default is "compatible"
  if (IsUndefined(*options)) {
    return Just(
        temporal_rs::Disambiguation(temporal_rs::Disambiguation::Compatible));
  }
  auto disambiguation_ident = isolate->factory()->disambiguation_string();
  if (!IsJSReceiver(*options)) {
    // (GetOptionsObject) 3. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR_WITH_ARG(
                                 kOptionMustBeObject, disambiguation_ident));
  }
  // 1. Let stringValue be ?GetOption(options, "disambiguation", string, «
  // "compatible", "earlier", "later", "reject" », "compatible").
  return GetStringOption<temporal_rs::Disambiguation>(
      isolate, Cast<JSReceiver>(options), disambiguation_ident, method_name,
      std::to_array<const std::string_view>(
          {"compatible", "earlier", "later", "reject"}),
      std::to_array<temporal_rs::Disambiguation>({
          temporal_rs::Disambiguation::Compatible,
          temporal_rs::Disambiguation::Earlier,
          temporal_rs::Disambiguation::Later,
          temporal_rs::Disambiguation::Reject,
      }),
      temporal_rs::Disambiguation::Compatible);
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporaloffsetoption
// Also handles the undefined check from GetOptionsObject
Maybe<temporal_rs::OffsetDisambiguation> GetTemporalOffsetOptionHandleUndefined(
    Isolate* isolate, DirectHandle<Object> options,
    temporal_rs::OffsetDisambiguation fallback, const char* method_name) {
  // Default is fallback
  if (IsUndefined(*options)) {
    return Just(fallback);
  }
  auto offset_ident = isolate->factory()->offset_string();
  if (!IsJSReceiver(*options)) {
    // (GetOptionsObject) 3. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR_WITH_ARG(
                                 kOptionMustBeObject, offset_ident));
  }
  // 5. Let stringValue be ? GetOption(options, "offset", string, « "prefer",
  // "use", "ignore", "reject" », stringFallback).
  return GetStringOption<temporal_rs::OffsetDisambiguation>(
      isolate, Cast<JSReceiver>(options), offset_ident, method_name,
      std::to_array<const std::string_view>(
          {"prefer", "use", "ignore", "reject"}),
      std::to_array<temporal_rs::OffsetDisambiguation>({
          temporal_rs::OffsetDisambiguation::Prefer,
          temporal_rs::OffsetDisambiguation::Use,
          temporal_rs::OffsetDisambiguation::Ignore,
          temporal_rs::OffsetDisambiguation::Reject,
      }),
      fallback);
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalfractionalseconddigitsoption
Maybe<temporal_rs::Precision> GetTemporalFractionalSecondDigitsOption(
    Isolate* isolate, DirectHandle<JSReceiver> normalized_options,
    const char* method_name) {
  auto auto_val =
      temporal_rs::Precision{.is_minute = false, .precision = std::nullopt};

  Factory* factory = isolate->factory();
  // 1. Let digitsValue be ?Get(options, "fractionalSecondDigits").
  DirectHandle<Object> digits_val;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, digits_val,
      JSReceiver::GetProperty(isolate, normalized_options,
                              factory->fractionalSecondDigits_string()));

  // 2. If digitsValue is undefined, return auto.
  if (IsUndefined(*digits_val)) {
    return Just(auto_val);
  }

  // 3. If digitsValue is not a Number, then
  if (!IsNumber(*digits_val)) {
    DirectHandle<String> string;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, string,
                               Object::ToString(isolate, digits_val));
    //  a. If ? ToString(digitsValue) is not "auto", throw a RangeError
    //  exception.
    if (!String::Equals(isolate, string, factory->auto_string())) {
      THROW_NEW_ERROR(isolate,
                      NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                                    factory->fractionalSecondDigits_string()));
    }
    //  b. Return auto.
    return Just(auto_val);
  }
  // 4. If digitsValue is NaN, +∞𝔽, or -∞𝔽, throw a RangeError exception.
  auto digits_num = Cast<Number>(*digits_val);
  auto digits_float = Object::NumberValue(digits_num);
  if (std::isnan(digits_float) || std::isinf(digits_float)) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                                  factory->fractionalSecondDigits_string()));
  }
  // 5. Let digitCount be floor(ℝ(digitsValue)).
  double digit_count = std::floor(digits_float);
  // 6. If digitCount < 0 or digitCount > 9, throw a RangeError exception.
  if (digit_count < 0 || digit_count > 9) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                                  factory->fractionalSecondDigits_string()));
  }

  uint8_t clamped = CastIntegralDouble<uint8_t>(digit_count);

  return Just(
      temporal_rs::Precision{.is_minute = false, .precision = clamped});
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalunitvaluedoption
Maybe<std::optional<Unit>> GetTemporalUnitValuedOption(
    Isolate* isolate, DirectHandle<JSReceiver> normalized_options,
    DirectHandle<String> key, DefaultValue default_value,
    const char* method_name) {
  // 1. Let allowedStrings be a List containing all values in the "Singular
  // property name" and "Plural property name" columns of Table 21, except the
  // header row.
  // 2. Append "auto" to allowedStrings.
  // 3. NOTE: For each singular Temporal unit name that is contained within
  // allowedStrings, the corresponding plural name is also contained within it.

  constexpr auto strs = std::to_array<const std::string_view>(
      {"year",       "month",   "week",        "day",          "hour",
       "minute",     "second",  "millisecond", "microsecond",  "nanosecond",
       "auto",       "years",   "months",      "weeks",        "days",
       "hours",      "minutes", "seconds",     "milliseconds", "microseconds",
       "nanoseconds"});
  constexpr auto enums = std::to_array<const std::optional<Unit::Value>>(
      {Unit::Year,        Unit::Month,       Unit::Week,
       Unit::Day,         Unit::Hour,        Unit::Minute,
       Unit::Second,      Unit::Millisecond, Unit::Microsecond,
       Unit::Nanosecond,  Unit::Auto,        Unit::Year,
       Unit::Month,       Unit::Week,        Unit::Day,
       Unit::Hour,        Unit::Minute,      Unit::Second,
       Unit::Millisecond, Unit::Microsecond, Unit::Nanosecond});

  // 4. If default is unset, then
  // a. Let defaultValue be undefined.

  std::optional<std::optional<Unit>> wrapped_default = std::nullopt;

  if (default_value == DefaultValue::kUnset) {
    // GetStringOption treats a null default as REQUIRED
    // however, we also wish to handle undefined/UNSET here, which we
    // represent as a None value in the inner optional
    wrapped_default = std::make_optional(std::optional<Unit>(std::nullopt));
  }

  // 6. Let value be ? GetOption(options, key, string, allowedStrings,
  // defaultValue).

  std::optional<Unit::Value> value;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                             GetStringOption<std::optional<Unit::Value>>(
                                 isolate, normalized_options, key, method_name,
                                 strs, enums, wrapped_default));
  // 7. If value is undefined, return unset.
  // 8. If value is "auto", return auto.
  // 9. Return the value in the "Value" column of Table 21 corresponding to the
  // row with value in its "Singular property name" or "Plural property name"
  // column.
  if (value.has_value()) {
    return Just<std::optional<Unit>>((Unit)value.value());
  } else {
    return Just<std::optional<Unit>>(std::nullopt);
  }
}

Maybe<void> ValidateTemporalUnitValue(
    Isolate* isolate, std::optional<Unit> value_or_unset, UnitGroup unit_group,
    std::optional<Unit> extra_values = std::nullopt) {
  // 1. If value is unset, return unused.
  if (!value_or_unset.has_value()) {
    return JustVoid();
  }
  auto value = value_or_unset.value();
  // 2. If extraValues is present and extraValues contains value, return unused.
  if (extra_values == value) {
    return JustVoid();
  }

  // 3. Let category be the value in the “Category” column of the row of Table
  // 21 whose “Value” column contains value. If there is no such row, throw a
  // RangeError exception.
  // 4. If category is date and unitGroup is date or datetime, return unused.
  // 5. If category is time and unitGroup is time or datetime, return unused.
  switch (value) {
    case Unit::Auto:
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_RANGE_ERROR("Auto unit not allowed here"));
    case Unit::Year:
    case Unit::Month:
    case Unit::Week:
    case Unit::Day:
      if (unit_group == UnitGroup::kDate ||
          unit_group == UnitGroup::kDateTime) {
        return JustVoid();
      } else {
        THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(
                                     "Found date unit, expect time unit"));
      }
    default:
      if (unit_group == UnitGroup::kTime ||
          unit_group == UnitGroup::kDateTime) {
        return JustVoid();
      }
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(
                                   "Found date unit, expect time unit"));
  }
  // 6. Throw a RangeError exception.
  // (done in branches above)
}

// https://tc39.es/proposal-temporal/#sec-temporal-canonicalizecalendar
Maybe<temporal_rs::AnyCalendarKind> CanonicalizeCalendar(
    Isolate* isolate, DirectHandle<String> calendar) {
  std::string s = calendar->ToStdString();
  // 2. If calendars does not contain the ASCII-lowercase of id, throw a
  // RangeError exception.
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);

  auto cal = temporal_rs::AnyCalendarKind::get_for_str(s);

  if (!cal.has_value()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR_WITH_ARG(
                                 "Unknown calendar type", calendar));
  }
  // Other steps unnecessary, we're not storing these as -u- values but rather
  // as enums.
  return Just(cal.value());
}

// https://tc39.es/proposal-temporal/#sec-temporal-getroundingincrementoption
Maybe<uint32_t> GetRoundingIncrementOption(
    Isolate* isolate, DirectHandle<JSReceiver> normalized_options) {
  DirectHandle<Object> value;

  // 1. Let value be ? Get(options, "roundingIncrement").
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value,
      JSReceiver::GetProperty(isolate, normalized_options,
                              isolate->factory()->roundingIncrement_string()));
  // 2. If value is undefined, return 1𝔽.
  if (IsUndefined(*value)) {
    return Just(static_cast<uint32_t>(1));
  }

  // 3. Let integerIncrement be ? ToIntegerWithTruncation(value).
  double integer_increment;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, integer_increment,
                             ToIntegerWithTruncation(isolate, value));

  // 4. If integerIncrement < 1 or integerIncrement > 10**9, throw a RangeError
  // exception.
  if (integer_increment < 1 || integer_increment > 1e9) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kIntegerOutOfRange));
  }

  return Just(static_cast<uint32_t>(integer_increment));
}

// sec-temporal-getroundingmodeoption
Maybe<RoundingMode> GetRoundingModeOption(Isolate* isolate,
                                          DirectHandle<JSReceiver> options,
                                          RoundingMode fallback,
                                          const char* method_name) {
  // 1. Return ? GetOption(normalizedOptions, "roundingMode", "string", «
  // "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor", "halfExpand",
  // "halfTrunc", "halfEven" », fallback).

  static const auto values = std::to_array<RoundingMode>(
      {RoundingMode::Ceil, RoundingMode::Floor, RoundingMode::Expand,
       RoundingMode::Trunc, RoundingMode::HalfCeil, RoundingMode::HalfFloor,
       RoundingMode::HalfExpand, RoundingMode::HalfTrunc,
       RoundingMode::HalfEven});
  return GetStringOption<RoundingMode>(
      isolate, options, isolate->factory()->roundingMode_string(), method_name,
      std::to_array<const std::string_view>(
          {"ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor",
           "halfExpand", "halfTrunc", "halfEven"}),
      values, fallback);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getshowoffsetoption
Maybe<temporal_rs::DisplayOffset> GetTemporalShowOffsetOption(
    Isolate* isolate, DirectHandle<JSReceiver> options,
    const char* method_name) {
  // 1. Let stringValue be ?GetOption(options, "offset", string, « "auto",
  // "never" », "auto").
  // 2. If stringValue is "never", return never.
  // 3. Return auto.
  static const auto values = std::to_array<temporal_rs::DisplayOffset>(
      {temporal_rs::DisplayOffset::Auto, temporal_rs::DisplayOffset::Never});
  return GetStringOption<temporal_rs::DisplayOffset>(
      isolate, options, isolate->factory()->offset_string(), method_name,
      std::to_array<const std::string_view>({"auto", "never"}), values,
      temporal_rs::DisplayOffset::Auto);
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowtimezonenameoption
Maybe<temporal_rs::DisplayTimeZone> GetTemporalShowTimeZoneNameOption(
    Isolate* isolate, DirectHandle<JSReceiver> options,
    const char* method_name) {
  // 1. Let stringValue be ? GetOption(options, "timeZoneName", string, «
  // "auto", "never", "critical" », "auto").
  // 2. If stringValue is "never", return never.
  // 3. If stringValue is "critical", return critical.
  static const auto values = std::to_array<temporal_rs::DisplayTimeZone>(
      {temporal_rs::DisplayTimeZone::Auto, temporal_rs::DisplayTimeZone::Never,
       temporal_rs::DisplayTimeZone::Critical});
  return GetStringOption<temporal_rs::DisplayTimeZone>(
      isolate, options, isolate->factory()->timeZoneName_string(), method_name,
      std::to_array<const std::string_view>({"auto", "never", "critical"}),
      values, temporal_rs::DisplayTimeZone::Auto);
}

// https://tc39.es/proposal-temporal/#sec-temporal-getdifferencesettings
//
// This does not perform any validity checks, it only does the minimum needed
// to construct a DifferenceSettings object. temporal_rs handles the rest
Maybe<temporal_rs::DifferenceSettings> GetDifferenceSettingsWithoutChecks(
    Isolate* isolate, DirectHandle<Object> options_obj, UnitGroup unit_group,
    std::optional<Unit> fallback_smallest_unit, const char* method_name) {
  DirectHandle<JSReceiver> options;
  // 1. Set options to ? GetOptionsObject(options).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 2. Let largestUnit be ? GetTemporalUnitValuedOption(options, "largestUnit",
  // unset).
  std::optional<Unit> largest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, largest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->largestUnit_string(),
          DefaultValue::kUnset, method_name));

  // 4. Let roundingIncrement be ?GetRoundingIncrementOption(options).
  uint32_t rounding_increment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, options));

  // 5. Let roundingMode be ?GetRoundingModeOption(options, trunc).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, options, RoundingMode::Trunc,
                                      method_name));
  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(options,
  // "smallestUnit", unset).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));

  // remaining steps are validation, to be performed later

  return Just(temporal_rs::DifferenceSettings{.largest_unit = largest_unit,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment});
}

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalshowcalendarnameoption
Maybe<temporal_rs::DisplayCalendar> GetTemporalShowCalendarNameOption(
    Isolate* isolate, DirectHandle<JSReceiver> options,
    const char* method_name) {
  // 1. Return ? GetOption(normalizedOptions, "calendarName", « String », «
  // "auto", "always", "never" », "auto").
  return GetStringOption<temporal_rs::DisplayCalendar>(
      isolate, options, isolate->factory()->calendarName_string(), method_name,
      std::to_array<const std::string_view>(
          {"auto", "always", "never", "critical"}),
      std::to_array<temporal_rs::DisplayCalendar>(
          {temporal_rs::DisplayCalendar::Auto,
           temporal_rs::DisplayCalendar::Always,
           temporal_rs::DisplayCalendar::Never,
           temporal_rs::DisplayCalendar::Critical}),
      temporal_rs::DisplayCalendar::Auto);
}

// convenience method for getting the "calendar" field off of a Temporal object
std::optional<temporal_rs::AnyCalendarKind> ExtractCalendarFrom(
    Isolate* isolate, Tagged<HeapObject> calendar_like) {
  InstanceType instance_type = calendar_like->map(isolate)->instance_type();
  // a. If temporalCalendarLike has an [[InitializedTemporalDate]],
  // [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]],
  // [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]]
  // internal slot, then
  //
  // i. Return temporalCalendarLike.[[Calendar]].
  if (InstanceTypeChecker::IsJSTemporalPlainDate(instance_type)) {
    return Cast<JSTemporalPlainDate>(*calendar_like)
        ->wrapped_rust()
        .calendar()
        .kind();
  } else if (InstanceTypeChecker::IsJSTemporalPlainDateTime(instance_type)) {
    return Cast<JSTemporalPlainDateTime>(*calendar_like)
        ->wrapped_rust()
        .calendar()
        .kind();
  } else if (InstanceTypeChecker::IsJSTemporalPlainMonthDay(instance_type)) {
    return Cast<JSTemporalPlainMonthDay>(*calendar_like)
        ->wrapped_rust()
        .calendar()
        .kind();
  } else if (InstanceTypeChecker::IsJSTemporalPlainYearMonth(instance_type)) {
    return Cast<JSTemporalPlainYearMonth>(*calendar_like)
        ->wrapped_rust()
        .calendar()
        .kind();
  } else if (InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
    return Cast<JSTemporalZonedDateTime>(*calendar_like)
        ->wrapped_rust()
        .calendar()
        .kind();
  }
  return std::nullopt;
}
// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalcalendarslotvaluewithisodefault
Maybe<temporal_rs::AnyCalendarKind> ToTemporalCalendarIdentifier(
    Isolate* isolate, DirectHandle<Object> calendar_like) {
  // 1. If temporalCalendarLike is an Object, then
  if (IsHeapObject(*calendar_like)) {
    // a. If temporalCalendarLike has an [[InitializedTemporalDate]],
    // [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]],
    // [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]]
    // internal slot, then
    //
    //  i. Return temporalCalendarLike.[[Calendar]].
    auto cal_field =
        ExtractCalendarFrom(isolate, Cast<HeapObject>(*calendar_like));
    if (cal_field.has_value()) {
      return Just(cal_field.value());
    }
  }

  // 2. If temporalCalendarLike is not a String, throw a TypeError exception.
  if (!IsString(*calendar_like)) {
    THROW_NEW_ERROR(
        isolate, NEW_TEMPORAL_TYPE_ERROR(
                     "Calendar must be string or calendared Temporal object."));
  }
  auto stdstr = Cast<String>(calendar_like)->ToStdString();

  // 3. Let identifier be ?ParseTemporalCalendarString(temporalCalendarLike).
  // 4. Return ?CanonicalizeCalendar(identifier).
  auto kind =
      temporal_rs::AnyCalendarKind::parse_temporal_calendar_string(stdstr);
  if (kind.has_value()) {
    return Just(kind.value());
  } else {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_RANGE_ERROR("Invalid calendar string"));
  }
}
// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalcalendarslotvaluewithisodefault
Maybe<temporal_rs::AnyCalendarKind> GetTemporalCalendarIdentifierWithISODefault(
    Isolate* isolate, DirectHandle<JSReceiver> options) {
  // 1. If item has an [[InitializedTemporalDate]],
  // [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]],
  // [[InitializedTemporalYearMonth]], or [[InitializedTemporalZonedDateTime]]
  // internal slot, then
  //
  // a. Return item.[[Calendar]].
  if (IsHeapObject(*options)) {
    auto cal_field = ExtractCalendarFrom(isolate, Cast<HeapObject>(*options));
    if (cal_field.has_value()) {
      return Just(cal_field.value());
    }
  }
  // 2. Let calendarLike be ?Get(item, "calendar").
  DirectHandle<Object> calendar;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar,
      JSReceiver::GetProperty(isolate, options,
                              isolate->factory()->calendar_string()));

  // 3. If calendarLike is undefined, then
  if (IsUndefined(*calendar)) {
    // a. Return "iso8601".
    return Just(
        temporal_rs::AnyCalendarKind(temporal_rs::AnyCalendarKind::Iso));
  }
  return ToTemporalCalendarIdentifier(isolate, calendar);
}

constexpr temporal_rs::ToStringRoundingOptions kToStringAuto =
    temporal_rs::ToStringRoundingOptions{
        .precision = temporal_rs::Precision{.is_minute = false,
                                            .precision = std::nullopt},
        .smallest_unit = std::nullopt,
        .rounding_mode = std::nullopt,
    };

// ====== Stringification operations ======

// Generic code for calling to_string()
//
// It has two varargs since there may be some implicit conversions between
// the args passed to this method and the args expected by Rust.
//
// https://tc39.es/proposal-temporal/#sec-temporal-temporaldurationtostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporalinstanttostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporalplaindatetostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporaltimerecordtostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporalisodatetimetostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporalplainyearmonthtostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporalplainmonthdaytostring
// https://tc39.es/proposal-temporal/#sec-temporal-temporalplainzoneddatetimetostring
template <typename JSType, typename... Args, typename... Args2>
MaybeDirectHandle<String> GenericTemporalToString(
    Isolate* isolate, DirectHandle<JSType> val,
    std::string (JSType::RustType::*method)(Args2...) const, Args... args) {
  // This is currently inefficient, can be improved after
  // https://github.com/rust-diplomat/diplomat/issues/866 is fixed
  auto output = (val->wrapped_rust().*method)(args...);

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(output);
  return builder.Finish().ToHandleChecked();
}

// Same as the above, but bubbles out Rust errors
template <typename JSType, typename... Args, typename... Args2>
MaybeDirectHandle<String> GenericTemporalToString(
    Isolate* isolate, DirectHandle<JSType> val,
    TemporalResult<std::string> (JSType::RustType::*method)(Args2...) const,
    Args... args) {
  std::string output;
  // This is currently inefficient, can be improved after
  // https://github.com/rust-diplomat/diplomat/issues/866 is fixed
  MOVE_RETURN_ON_EXCEPTION(
      isolate, output,
      ExtractRustResult(isolate, (val->wrapped_rust().*method)(args...)));

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(output);
  return builder.Finish().ToHandleChecked();
}

// ====== Record operations ======

constexpr temporal_rs::PartialDate kNullPartialDate = temporal_rs::PartialDate{
    .year = std::nullopt,
    .month = std::nullopt,
    .month_code = "",
    .day = std::nullopt,
    .era = "",
    .era_year = std::nullopt,
    .calendar = temporal_rs::AnyCalendarKind::Iso,
};
constexpr temporal_rs::PartialTime kNullPartialTime = temporal_rs::PartialTime{
    .hour = std::nullopt,
    .minute = std::nullopt,
    .second = std::nullopt,
    .millisecond = std::nullopt,
    .microsecond = std::nullopt,
    .nanosecond = std::nullopt,
};

constexpr temporal_rs::PartialZonedDateTime kNullPartialZonedDateTime =
    temporal_rs::PartialZonedDateTime{
        .date = kNullPartialDate,
        .time = kNullPartialTime,
        .offset = std::nullopt,
        .timezone = std::nullopt,
    };

constexpr temporal_rs::PartialDateTime kNullPartialDateTime =
    temporal_rs::PartialDateTime{.date = kNullPartialDate,
                                 .time = kNullPartialTime};

struct TimeRecord {
  std::optional<double> hour = std::nullopt;
  std::optional<double> minute = std::nullopt;
  std::optional<double> second = std::nullopt;
  std::optional<double> millisecond = std::nullopt;
  std::optional<double> microsecond = std::nullopt;
  std::optional<double> nanosecond = std::nullopt;
  Maybe<temporal_rs::PartialTime> Regulate(
      Isolate* isolate, temporal_rs::ArithmeticOverflow overflow);
};

// https://tc39.es/proposal-temporal/#sec-temporal-regulatetime
//
// N.B. The spec implicitly assumes all fields are set; however
// we plan to call this in contexts where they are not (e.g. during
// .with(). Fortunately, )
Maybe<temporal_rs::PartialTime> TimeRecord::Regulate(
    Isolate* isolate, temporal_rs::ArithmeticOverflow overflow) {
  temporal_rs::PartialTime partial = kNullPartialTime;
  // 1. If overflow is constrain, then
  if (overflow == temporal_rs::ArithmeticOverflow::Constrain) {
    // a. Set hour to the result of clamping hour between 0 and 23.
    if (hour.has_value()) {
      partial.hour = ClampIntegralDouble<uint8_t>(hour.value(), 0, 23);
    }
    // b. Set minute to the result of clamping minute between 0 and 59.
    if (minute.has_value()) {
      partial.minute = ClampIntegralDouble<uint8_t>(minute.value(), 0, 59);
    }
    // c. Set second to the result of clamping second between 0 and 59.
    if (second.has_value()) {
      partial.second = ClampIntegralDouble<uint8_t>(second.value(), 0, 59);
    }
    // d. Set millisecond to the result of clamping millisecond between 0 and
    // 999.
    if (millisecond.has_value()) {
      partial.millisecond =
          ClampIntegralDouble<uint16_t>(millisecond.value(), 0, 999);
    }
    // e. Set microsecond to the result of clamping microsecond between 0 and
    // 999.
    if (microsecond.has_value()) {
      partial.microsecond =
          ClampIntegralDouble<uint16_t>(microsecond.value(), 0, 999);
    }
    // f. Set nanosecond to the result of clamping nanosecond between 0 and 999.
    if (nanosecond.has_value()) {
      partial.nanosecond =
          ClampIntegralDouble<uint16_t>(nanosecond.value(), 0, 999);
    }
  } else {
    // b. If IsValidTime(hour, minute, second, millisecond, microsecond,
    // nanosecond) is false, throw a RangeError exception.
    if (!IsValidTime(hour.value_or(0), minute.value_or(0), second.value_or(0),
                     millisecond.value_or(0), microsecond.value_or(0),
                     nanosecond.value_or(0))) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_RANGE_ERROR("Invalid time provided"));
    }
    if (hour.has_value()) {
      partial.hour = CastIntegralDouble<uint8_t>(hour.value());
    }
    if (minute.has_value()) {
      partial.minute = CastIntegralDouble<uint8_t>(minute.value());
    }
    if (second.has_value()) {
      partial.second = CastIntegralDouble<uint8_t>(second.value());
    }
    if (millisecond.has_value()) {
      partial.millisecond = CastIntegralDouble<uint16_t>(millisecond.value());
    }
    if (microsecond.has_value()) {
      partial.microsecond = CastIntegralDouble<uint16_t>(microsecond.value());
    }
    if (nanosecond.has_value()) {
      partial.nanosecond = CastIntegralDouble<uint16_t>(nanosecond.value());
    }
  }
  return Just(partial);
}

template <typename RustObject>
temporal_rs::PartialTime GetPartialTimeFromRust(RustObject& rust_object) {
  return temporal_rs::PartialTime{
      .hour = rust_object->hour(),
      .minute = rust_object->minute(),
      .second = rust_object->second(),
      .millisecond = rust_object->millisecond(),
      .microsecond = rust_object->microsecond(),
      .nanosecond = rust_object->nanosecond(),
  };
}
// These can eventually be replaced with methods upstream
temporal_rs::PartialTime GetPartialTime(
    DirectHandle<JSTemporalPlainTime> plain_time) {
  auto rust_object = plain_time->time()->raw();
  return GetPartialTimeFromRust(rust_object);
}
temporal_rs::PartialTime GetPartialTime(
    DirectHandle<JSTemporalPlainDateTime> date_time) {
  auto rust_object = date_time->date_time()->raw();
  return GetPartialTimeFromRust(rust_object);
}
temporal_rs::PartialTime GetPartialTime(
    DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  auto rust_object = zoned_date_time->zoned_date_time()->raw();
  return GetPartialTimeFromRust(rust_object);
}

template <typename RustObject>
temporal_rs::PartialDate GetPartialDateFromRust(RustObject& rust_object) {
  return temporal_rs::PartialDate{
      .year = rust_object->year(),
      .month = rust_object->month(),
      .month_code = "",
      .day = rust_object->day(),
      .era = "",
      .era_year = std::nullopt,
      .calendar = rust_object->calendar().kind(),
  };
}
temporal_rs::PartialDate GetPartialDate(
    DirectHandle<JSTemporalPlainDate> plain_date) {
  auto rust_object = plain_date->date()->raw();
  return GetPartialDateFromRust(rust_object);
}
temporal_rs::PartialDate GetPartialDate(
    DirectHandle<JSTemporalPlainDateTime> date_time) {
  auto rust_object = date_time->date_time()->raw();
  return GetPartialDateFromRust(rust_object);
}
temporal_rs::PartialDate GetPartialDate(
    DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  auto rust_object = zoned_date_time->zoned_date_time()->raw();
  return GetPartialDateFromRust(rust_object);
}

temporal_rs::PartialDateTime GetPartialDateTime(
    DirectHandle<JSTemporalPlainDate> plain_date) {
  auto rust_object = plain_date->date()->raw();
  return temporal_rs::PartialDateTime{
      .date = GetPartialDateFromRust(rust_object),
      .time = kNullPartialTime,
  };
}
temporal_rs::PartialDateTime GetPartialDateTime(
    DirectHandle<JSTemporalPlainDateTime> date_time) {
  auto rust_object = date_time->date_time()->raw();
  return temporal_rs::PartialDateTime{
      .date = GetPartialDateFromRust(rust_object),
      .time = GetPartialTimeFromRust(rust_object),
  };
}
temporal_rs::PartialDateTime GetPartialDateTime(
    DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  auto rust_object = zoned_date_time->zoned_date_time()->raw();
  return temporal_rs::PartialDateTime{
      .date = GetPartialDateFromRust(rust_object),
      .time = GetPartialTimeFromRust(rust_object),
  };
}

// Helper for ToTemporalPartialDurationRecord
// Maybe<std::optional> since the Maybe handles errors and the optional handles
// missing fields
Maybe<std::optional<double>> GetSingleDurationField(
    Isolate* isolate, DirectHandle<JSReceiver> duration_like,
    DirectHandle<String> field_name) {
  DirectHandle<Object> val;
  //  Let val be ? Get(temporalDurationLike, fieldName).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, val,
      JSReceiver::GetProperty(isolate, duration_like, field_name));
  // c. If val is not undefined, then
  if (IsUndefined(*val)) {
    return Just((std::optional<double>)std::nullopt);
  } else {
    double field;
    // 5. If val is not undefined, set result.[[val]] to
    // ?ToIntegerIfIntegral(val).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, field,
                               temporal::ToIntegerIfIntegral(isolate, val));

    return Just(std::optional(field));
  }
}

// GetSingleDurationField, but also casts into an integer, erroring if out of
// range.
//
// Can be removed once https://github.com/boa-dev/temporal/issues/189 has been
// fixed.
Maybe<std::optional<int64_t>> GetSingleDurationFieldInteger(
    Isolate* isolate, DirectHandle<JSReceiver> duration_like,
    DirectHandle<String> field_name) {
  std::optional<double> ret_opt;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ret_opt,
      GetSingleDurationField(isolate, duration_like, field_name));
  if (ret_opt.has_value()) {
    double ret = ret_opt.value();
    if (!base::IsValueInRangeForNumericType<int64_t, double>(ret)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_RANGE_ERROR("Duration field out of range."));
    }
    return Just(std::optional(static_cast<int64_t>(ret)));
  } else {
    return Just((std::optional<int64_t>)std::nullopt);
  }
}

// https://tc39.es/proposal-temporal/#sec-temporal-tooffsetstring
Maybe<std::string> ToOffsetString(Isolate* isolate,
                                  DirectHandle<Object> argument) {
  // 1. Let offset be ?ToPrimitive(argument, string).
  DirectHandle<Object> offset_prim;
  if (IsJSReceiver(*argument)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, offset_prim,
        JSReceiver::ToPrimitive(isolate, Cast<JSReceiver>(argument),
                                ToPrimitiveHint::kString));
  } else {
    offset_prim = argument;
  }

  // 2. If offset is not a String, throw a TypeError exception.
  if (!IsString(*offset_prim)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR("Offset must be string."));
  }

  // 3. Perform ?ParseDateTimeUTCOffset(offset).
  auto offset_str = Cast<String>(offset_prim);
  auto offset = offset_str->ToStdString();

  // TODO(manishearth) this has a minor unnecessary cost of allocating a
  // TimeZone, but it can be obviated once
  // https://github.com/boa-dev/temporal/issues/330 is fixed.
  RETURN_ON_EXCEPTION(
      isolate,
      ExtractRustResult(isolate,
                        temporal_rs::TimeZone::try_from_offset_str(offset)));

  return Just(std::move(offset));
}

Maybe<temporal_rs::TimeZone> ToTemporalTimeZoneIdentifier(
    Isolate* isolate, DirectHandle<Object> tz_like) {
  // 1. If temporalTimeZoneLike is an Object, then
  // a. If temporalTimeZoneLike has an [[InitializedTemporalZonedDateTime]]
  // internal slot, then
  if (IsJSTemporalZonedDateTime(*tz_like)) {
    // i. Return temporalTimeZoneLike.[[TimeZone]].
    return Just(Cast<JSTemporalZonedDateTime>(tz_like)
                    ->zoned_date_time()
                    ->raw()
                    ->timezone());
  }
  // 2. If temporalTimeZoneLike is not a String, throw a TypeError exception.
  if (!IsString(*tz_like)) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_TYPE_ERROR(
                        "Time zone must be string or ZonedDateTime object."));
  }

  DirectHandle<String> str = Cast<String>(tz_like);

  auto std_str = str->ToStdString();

  return ExtractRustResult(isolate,
                           temporal_rs::TimeZone::try_from_str_with_provider(
                               std_str, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalpartialdurationrecord
Maybe<temporal_rs::PartialDuration> ToTemporalPartialDurationRecord(
    Isolate* isolate, DirectHandle<Object> duration_like_obj) {
  Factory* factory = isolate->factory();

  // 1. If temporalDurationLike is not an Object, then
  if (!IsJSReceiver(*duration_like_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_TYPE_ERROR("Must provide a duration."));
  }

  DirectHandle<JSReceiver> duration_like = Cast<JSReceiver>(duration_like_obj);

  // 2. Let result be a new partial Duration Record with each field set to
  // undefined.
  auto result = temporal_rs::PartialDuration{
      .years = std::nullopt,
      .months = std::nullopt,
      .weeks = std::nullopt,
      .days = std::nullopt,
      .hours = std::nullopt,
      .minutes = std::nullopt,
      .seconds = std::nullopt,
      .milliseconds = std::nullopt,
      .microseconds = std::nullopt,
      .nanoseconds = std::nullopt,
  };

  // Steps 3-14: get each field in alphabetical order

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.days,

      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->days_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.hours,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->hours_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.microseconds,
      temporal::GetSingleDurationField(isolate, duration_like,
                                       factory->microseconds_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.milliseconds,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->milliseconds_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.minutes,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->minutes_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.months,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->months_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.nanoseconds,
      temporal::GetSingleDurationField(isolate, duration_like,
                                       factory->nanoseconds_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.seconds,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->seconds_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.weeks,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->weeks_string()));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.years,
      temporal::GetSingleDurationFieldInteger(isolate, duration_like,
                                              factory->years_string()));

  // 24. If years is undefined, and months is undefined, and weeks is undefined,
  // and days is undefined, and hours is undefined, and minutes is undefined,
  // and seconds is undefined, and milliseconds is undefined, and microseconds
  // is undefined, and nanoseconds is undefined, throw a TypeError exception.

  if (!result.years.has_value() && !result.months.has_value() &&
      !result.weeks.has_value() && !result.days.has_value() &&
      !result.hours.has_value() && !result.minutes.has_value() &&
      !result.seconds.has_value() && !result.milliseconds.has_value() &&
      !result.microseconds.has_value() && !result.nanoseconds.has_value()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                 "Did not provide any valid Duration fields."));
  }

  return Just(result);
}

// Helper for ToTemporalTimeRecord
// Maybe<std::optional> since the Maybe handles errors and the optional handles
// missing fields
Maybe<std::optional<double>> GetSingleTimeRecordField(
    Isolate* isolate, DirectHandle<JSReceiver> time_like,
    DirectHandle<String> field_name, bool* any) {
  DirectHandle<Object> val;
  //  Let v be ?Get(temporalTimeLike, field_name).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, val, JSReceiver::GetProperty(isolate, time_like, field_name));
  // If val is not undefined, then
  if (!IsUndefined(*val)) {
    double field;
    // 5. a. Set result.[[Hour]] to ?ToIntegerWithTruncation(hour).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, field,
                               temporal::ToIntegerWithTruncation(isolate, val));
    // b. Set any to true.
    *any = true;

    return Just(std::optional(field));
  } else {
    return Just((std::optional<double>)std::nullopt);
  }
}

// https://tc39.es/proposal-temporal/#sec-temporal-ispartialtemporalobject
Maybe<bool> IsPartialTemporalObject(Isolate* isolate,
                                    DirectHandle<Object> value) {
  // 1. If value is not an Object, return false.
  if (!IsHeapObject(*value)) {
    return Just(false);
  }
  InstanceType instance_type =
      Cast<HeapObject>(*value)->map(isolate)->instance_type();

  if (!InstanceTypeChecker::IsJSReceiver(instance_type)) {
    return Just(false);
  }

  auto value_recvr = Cast<JSReceiver>(value);

  // 2. If value has an [[InitializedTemporalDate]],
  // [[InitializedTemporalDateTime]], [[InitializedTemporalMonthDay]],
  // [[InitializedTemporalTime]], [[InitializedTemporalYearMonth]], or
  // [[InitializedTemporalZonedDateTime]] internal slot, return false.
  if (InstanceTypeChecker::IsJSTemporalPlainDate(instance_type) ||
      InstanceTypeChecker::IsJSTemporalPlainDateTime(instance_type) ||
      InstanceTypeChecker::IsJSTemporalPlainMonthDay(instance_type) ||
      InstanceTypeChecker::IsJSTemporalPlainTime(instance_type) ||
      InstanceTypeChecker::IsJSTemporalPlainYearMonth(instance_type) ||
      InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
    return Just(false);
  }

  // 3. Let calendarProperty be ? Get(value, "calendar").
  DirectHandle<Object> cal;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, cal,
      JSReceiver::GetProperty(isolate, value_recvr,
                              isolate->factory()->calendar_string()));

  // 4. If calendarProperty is not undefined, return false.
  if (!IsUndefined(*cal)) {
    return Just(false);
  }

  // 5. Let timeZoneProperty be ? Get(value, "timeZone").
  DirectHandle<Object> tz;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, tz,
      JSReceiver::GetProperty(isolate, value_recvr,
                              isolate->factory()->timeZone_string()));

  // 6. If timeZoneProperty is not undefined, return false.
  if (!IsUndefined(*tz)) {
    return Just(false);
  }

  // 7. return true
  return Just(true);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltimerecord
Maybe<TimeRecord> ToTemporalTimeRecord(Isolate* isolate,
                                       DirectHandle<JSReceiver> time_like,
                                       const char* method_name,
                                       Completeness completeness = kComplete) {
  Factory* factory = isolate->factory();

  // 2. If completeness is complete, then
  // a. Let result be a new TemporalTimeLike Record with each field set to 0.
  // 3. Else,
  // a. Let result be a new TemporalTimeLike Record with each field set to
  // unset.
  auto result = completeness == kPartial ? TimeRecord {
    .hour = std::nullopt,
    .minute = std::nullopt,
    .second = std::nullopt,
    .millisecond = std::nullopt,
    .microsecond = std::nullopt,
    .nanosecond = std::nullopt,
  } : TimeRecord {
    .hour = 0,
    .minute = 0,
    .second = 0,
    .millisecond = 0,
    .microsecond = 0,
    .nanosecond = 0,
  };

  bool any = false;

  // Steps 3-14: get each field in alphabetical order

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.hour,
      temporal::GetSingleTimeRecordField(isolate, time_like,
                                         factory->hour_string(), &any));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.microsecond,
      temporal::GetSingleTimeRecordField(isolate, time_like,
                                         factory->microsecond_string(), &any));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.millisecond,
      temporal::GetSingleTimeRecordField(isolate, time_like,
                                         factory->millisecond_string(), &any));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.minute,
      temporal::GetSingleTimeRecordField(isolate, time_like,
                                         factory->minute_string(), &any));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.nanosecond,
      temporal::GetSingleTimeRecordField(isolate, time_like,
                                         factory->nanosecond_string(), &any));
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result.second,
      temporal::GetSingleTimeRecordField(isolate, time_like,
                                         factory->second_string(), &any));

  if (!any) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                 "Must specify at least one time field."));
  }

  return Just(result);
}

struct DateRecord {
  std::optional<double> year;
  std::optional<double> month;
  std::optional<std::string> month_code;
  std::optional<double> day;
  std::optional<std::string> era;
  std::optional<double> era_year;
  temporal_rs::AnyCalendarKind calendar = temporal_rs::AnyCalendarKind::Iso;
  Maybe<temporal_rs::PartialDate> Regulate(
      Isolate* isolate, temporal_rs::ArithmeticOverflow overflow);
};

// https://tc39.es/proposal-temporal/#sec-temporal-regulatetime
//
// N.B. The spec implicitly assumes all fields are set; however
// we plan to call this in contexts where they are not (e.g. during
// .with(). Fortunately, )
Maybe<temporal_rs::PartialDate> DateRecord::Regulate(
    Isolate* isolate, temporal_rs::ArithmeticOverflow overflow) {
  temporal_rs::PartialDate partial = kNullPartialDate;

  if (month_code.has_value()) {
    partial.month_code = month_code.value();
  }
  if (era.has_value()) {
    partial.era = era.value();
  }
  partial.calendar = calendar;
  // 1. If overflow is constrain, then
  if (overflow == temporal_rs::ArithmeticOverflow::Constrain) {
    if (year.has_value()) {
      partial.year = ClampIntegralDoubleToRange<int32_t>(year.value());
    }
    if (month.has_value()) {
      partial.month = ClampIntegralDoubleToRange<int8_t>(month.value());
    }
    if (day.has_value()) {
      partial.day = ClampIntegralDoubleToRange<int8_t>(day.value());
    }
    if (era_year.has_value()) {
      partial.era_year = ClampIntegralDoubleToRange<int32_t>(era_year.value());
    }
  } else {
    if (year.has_value()) {
      int32_t result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result, CheckDoubleInRange<int32_t>(isolate, year.value()));
      partial.year = result;
    }
    if (month.has_value()) {
      uint8_t result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result, CheckDoubleInRange<uint8_t>(isolate, month.value()));
      partial.month = result;
    }
    if (day.has_value()) {
      uint8_t result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result, CheckDoubleInRange<uint8_t>(isolate, day.value()));
      partial.day = result;
    }
    if (era_year.has_value()) {
      int32_t result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result,
          CheckDoubleInRange<int32_t>(isolate, era_year.value()));
      partial.era_year = result;
    }
  }
  return Just(partial);
}
// Returned by PrepareCalendarFields
struct CombinedRecord {
  DateRecord date;
  TimeRecord time;
  std::optional<std::string> offset;
  std::optional<temporal_rs::TimeZone> time_zone;

  // For use in generic contexts
  template <typename Ret>
  Maybe<Ret> Regulate(Isolate* isolate,
                      temporal_rs::ArithmeticOverflow overflow);
};

template <>
Maybe<temporal_rs::PartialDate> CombinedRecord::Regulate(
    Isolate* isolate, temporal_rs::ArithmeticOverflow overflow) {
  DCHECK(!offset.has_value() && !time_zone.has_value());
  DCHECK(!time.hour.has_value() && !time.minute.has_value() &&
         !time.second.has_value() && !time.millisecond.has_value() &&
         !time.microsecond.has_value() && !time.nanosecond.has_value());
  return date.Regulate(isolate, overflow);
}
template <>
Maybe<temporal_rs::PartialTime> CombinedRecord::Regulate(
    Isolate* isolate, temporal_rs::ArithmeticOverflow overflow) {
  DCHECK(!offset.has_value() && !time_zone.has_value());
  DCHECK(!date.year.has_value() && !date.month.has_value() &&
         date.month_code == "" && !date.day.has_value() && date.era == "" &&
         !date.era_year.has_value() &&
         date.calendar == temporal_rs::AnyCalendarKind::Iso);

  return time.Regulate(isolate, overflow);
}

template <>
Maybe<temporal_rs::PartialDateTime> CombinedRecord::Regulate(
    Isolate* isolate, temporal_rs::ArithmeticOverflow overflow) {
  DCHECK(!offset.has_value() && !time_zone.has_value());
  temporal_rs::PartialDate regulated_date = kNullPartialDate;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, regulated_date,
                             date.Regulate(isolate, overflow));
  temporal_rs::PartialTime regulated_time = kNullPartialTime;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, regulated_time,
                             time.Regulate(isolate, overflow));
  return Just(temporal_rs::PartialDateTime{
      .date = regulated_date,
      .time = regulated_time,
  });
}

template <>
Maybe<temporal_rs::PartialZonedDateTime> CombinedRecord::Regulate(
    Isolate* isolate, temporal_rs::ArithmeticOverflow overflow) {
  temporal_rs::PartialDate regulated_date = kNullPartialDate;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, regulated_date,
                             date.Regulate(isolate, overflow));
  temporal_rs::PartialTime regulated_time = kNullPartialTime;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, regulated_time,
                             time.Regulate(isolate, overflow));
  auto record = temporal_rs::PartialZonedDateTime{
      .date = regulated_date,
      .time = regulated_time,
      .offset = std::nullopt,
      .timezone = std::nullopt,
  };
  if (time_zone.has_value()) {
    record.timezone = time_zone.value();
  }
  if (offset.has_value()) {
    record.offset = offset.value();
  }
  return Just(record);
}

enum class CalendarFieldsFlag : uint8_t {
  kDay = 1 << 0,
  // Month and MonthCode
  kMonthFields = 1 << 1,
  // year, era, eraYear
  kYearFields = 1 << 2,
  // hour, minute, second, millisecond, microsecont, nanosecond
  kTimeFields = 1 << 3,
  kOffset = 1 << 4,
  kTimeZone = 1 << 5,
};
using CalendarFieldsFlags = base::Flags<CalendarFieldsFlag>;

DEFINE_OPERATORS_FOR_FLAGS(CalendarFieldsFlags)

constexpr CalendarFieldsFlags kAllDateFlags = CalendarFieldsFlag::kDay |
                                              CalendarFieldsFlag::kMonthFields |
                                              CalendarFieldsFlag::kYearFields;

enum class RequiredFields {
  kNone,
  kPartial,
  kTimeZone,
};

// A single run of the PrepareCalendarFields iteration (Step 9, substeps a-c,
// NOT d) Returns whether or not the field was found. Does not handle the case
// when the field is not found.
template <typename OutType>
Maybe<bool> GetSingleCalendarField(
    Isolate* isolate, DirectHandle<JSReceiver> fields,
    DirectHandle<String> field_name, bool& any, OutType& output,
    Maybe<OutType> (*conversion_func)(Isolate*, DirectHandle<Object>)) {
  // b. Let value be ?Get(fields, property).
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, JSReceiver::GetProperty(isolate, fields, field_name));

  // c. If value is not undefined, then
  if (!IsUndefined(*value)) {
    // i. Set any to true.
    any = true;
    // ii. Let Conversion be the Conversion value of the same row.
    // (perform conversion)
    // ix. Set result's field whose name is given in the Field Name column of
    // the same row to value.
    MOVE_RETURN_ON_EXCEPTION(isolate, output, conversion_func(isolate, value));
    return Just(true);
  }

  return Just(false);
}
// Same as above but for DirectHandles
template <typename OutType>
Maybe<bool> GetSingleCalendarField(
    Isolate* isolate, DirectHandle<JSReceiver> fields,
    DirectHandle<String> field_name, bool& any, DirectHandle<OutType>& output,
    MaybeDirectHandle<OutType> (*conversion_func)(Isolate*,
                                                  DirectHandle<Object>)) {
  // b. Let value be ?Get(fields, property).
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, JSReceiver::GetProperty(isolate, fields, field_name));

  // c. If value is not undefined, then
  if (!IsUndefined(*value)) {
    // i. Set any to true.
    any = true;
    // ii. Let Conversion be the Conversion value of the same row.
    // (perform conversion)
    // ix. Set result's field whose name is given in the Field Name column of
    // the same row to value.
    ASSIGN_RETURN_ON_EXCEPTION(isolate, output,
                               conversion_func(isolate, value));
    return Just(true);
  }

  return Just(false);
}

// Setters take `resultField` (a field expression of CombinedRecords)
// and `field` (a variable holding a value, and the name of the property),
// and set resultField to field, performing additional work if necessary.
#define SIMPLE_SETTER(resultField, field) resultField = field;
#define MOVING_SETTER(resultField, field) resultField = std::move(field);
#define STR_CONVERSION_SETTER(resultField, field) \
  resultField = field->ToStdString();

// Conditions take a boolean expression and wrap it with additional checks
#define SIMPLE_CONDITION(cond) cond
#define ERA_CONDITION(cond) calendarUsesEras && (cond)

#define NOOP_REQUIRED_CHECK
#define TIMEZONE_REQUIRED_CHECK                                          \
  if (!found && required_fields == RequiredFields::kTimeZone) {          \
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kTimeZoneMissing)); \
  }

// https://tc39.es/proposal-temporal/#sec-temporal-calendar-fields-records
// V(CalendarFieldsFlags, propertyName, resultField, Type, Conversion,
// CONDITION, SETTER, REQUIRED_CHECK, AssignOrMove)
#define CALENDAR_FIELDS(V)                                                     \
  V(kDay, day, result.date.day, double, ToPositiveIntegerWithTruncation,       \
    SIMPLE_CONDITION, SIMPLE_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)              \
  V(kYearFields, era, result.date.era, DirectHandle<String>, Object::ToString, \
    ERA_CONDITION, STR_CONVERSION_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)         \
  V(kYearFields, eraYear, result.date.era_year, double,                        \
    ToIntegerWithTruncation, ERA_CONDITION, SIMPLE_SETTER,                     \
    NOOP_REQUIRED_CHECK, ASSIGN)                                               \
  V(kTimeFields, hour, result.time.hour, double, ToIntegerWithTruncation,      \
    SIMPLE_CONDITION, SIMPLE_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)              \
  V(kTimeFields, microsecond, result.time.microsecond, double,                 \
    ToIntegerWithTruncation, SIMPLE_CONDITION, SIMPLE_SETTER,                  \
    NOOP_REQUIRED_CHECK, ASSIGN)                                               \
  V(kTimeFields, millisecond, result.time.millisecond, double,                 \
    ToIntegerWithTruncation, SIMPLE_CONDITION, SIMPLE_SETTER,                  \
    NOOP_REQUIRED_CHECK, ASSIGN)                                               \
  V(kTimeFields, minute, result.time.minute, double, ToIntegerWithTruncation,  \
    SIMPLE_CONDITION, SIMPLE_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)              \
  V(kMonthFields, month, result.date.month, double,                            \
    ToPositiveIntegerWithTruncation, SIMPLE_CONDITION, SIMPLE_SETTER,          \
    NOOP_REQUIRED_CHECK, ASSIGN)                                               \
  V(kMonthFields, monthCode, result.date.month_code, std::string, ToMonthCode, \
    SIMPLE_CONDITION, MOVING_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)              \
  V(kTimeFields, nanosecond, result.time.nanosecond, double,                   \
    ToIntegerWithTruncation, SIMPLE_CONDITION, SIMPLE_SETTER,                  \
    NOOP_REQUIRED_CHECK, ASSIGN)                                               \
  V(kOffset, offset, result.offset, std::string, ToOffsetString,               \
    SIMPLE_CONDITION, MOVING_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)              \
  V(kTimeFields, second, result.time.second, double, ToIntegerWithTruncation,  \
    SIMPLE_CONDITION, SIMPLE_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)              \
  V(kTimeZone, timeZone, result.time_zone, temporal_rs::TimeZone,              \
    ToTemporalTimeZoneIdentifier, SIMPLE_CONDITION, MOVING_SETTER,             \
    TIMEZONE_REQUIRED_CHECK, MOVE)                                             \
  V(kYearFields, year, result.date.year, double, ToIntegerWithTruncation,      \
    SIMPLE_CONDITION, SIMPLE_SETTER, NOOP_REQUIRED_CHECK, ASSIGN)

// https://tc39.es/proposal-temporal/#sec-temporal-preparecalendarfields
Maybe<CombinedRecord> PrepareCalendarFields(Isolate* isolate,
                                            temporal_rs::AnyCalendarKind kind,
                                            DirectHandle<JSReceiver> fields,
                                            CalendarFieldsFlags which_fields,
                                            RequiredFields required_fields) {
  // 1. Assert: If requiredFieldNames is a List, requiredFieldNames contains
  // zero or one of each of the elements of calendarFieldNames and
  // nonCalendarFieldNames.
  // 2. Let fieldNames be the list-concatenation of calendarFieldNames and
  // nonCalendarFieldNames.
  // 3. Let extraFieldNames be CalendarExtraFields(calendar,
  // calendarFieldNames).
  // Currently al
  // 4. Set fieldNames to the list-concatenation of fieldNames and
  // extraFieldNames.
  // 5. Assert: fieldNames contains no duplicate elements.

  // All steps handled by RequiredFields/CalendarFieldsFlag being enums, and
  // CalendarExtraFields is handled by calendarUsesEras below.

  // Currently all calendars except for iso, chinese, and dangi support eras.
  // This may change, but is unlikely to.
  //
  // https://tc39.es/proposal-intl-era-monthcode/#sec-temporal-calendarsupportsera
  bool calendarUsesEras = kind != temporal_rs::AnyCalendarKind::Iso &&
                          kind != temporal_rs::AnyCalendarKind::Chinese &&
                          kind != temporal_rs::AnyCalendarKind::Dangi;

  // 6. Let result be a Calendar Fields Record with all fields equal to unset.
  CombinedRecord result;

  // This is not explicitly specced, but CombinedRecord contains the calendar
  // kind unlike the spec, and no caller of PrepareCalendarFields does anything
  // other than pair `fields` with `calendar` when passing to subsequent
  // algorithms.
  result.date.calendar = kind;

  // 7. Let any be false.
  bool any = false;

  // 8. Let sortedPropertyNames be a List whose elements are the values in the
  // Property Key column of Table 19 corresponding to the elements of
  // fieldNames, sorted according to lexicographic code unit order. (handled by
  // sorting)

  // 9. For each property name property of sortedPropertyNames, do
  //  a. Let key be the value in the Enumeration Key column of Table 19
  //  corresponding to the row whose Property Key value is property.
  //
  //  b. Let value be ?Get(fields, property).
  //
  //  c. If value is not undefined, then
  //   i. Set any to true.
  //   ii. Let Conversion be the Conversion value of the same row.
  //   iii. If Conversion is to-integer-with-truncation, then
  //     1. Set value to ? ToIntegerWithTruncation(value).
  //     2. Set value to 𝔽(value).
  //   iv. Else if Conversion is to-positive-integer-with-truncation, then
  //     1. Set value to ? ToPositiveIntegerWithTruncation(value).
  //     2. Set value to 𝔽(value).
  //   v. Else if Conversion is to-string, then
  //     1. Set value to ? ToString(value).
  //   vi. Else if Conversion is to-temporal-time-zone-identifier, then
  //     1. Set value to ? ToTemporalTimeZoneIdentifier(value).
  //   vii. Else if Conversion is to-month-code, then
  //     1. Set value to ? ToMonthCode(value).
  //   viii. Else,
  //     1. Assert: Conversion is to-offset-string.
  //     2. Set value to ? ToOffsetString(value).
  //   ix. Set result's field whose name is given in the Field Name column of
  //   the same row to value.
  //  d. Else if requiredFieldNames is a List, then
  //   i. If requiredFieldNames contains key, then
  //     1. Throw a TypeError exception.
  //   ii. Set result's field whose name is given in the Field Name column of
  //   the same row to the corresponding Default value of the same row.

#define GET_SINGLE_CALENDAR_FIELD(fieldsFlag, propertyName, resultField, Type, \
                                  Conversion, CONDITION, SETTER,               \
                                  REQUIRED_CHECK, AssignOrMove)                \
  if (CONDITION(which_fields & CalendarFieldsFlag::fieldsFlag)) {              \
    Type propertyName;                                                         \
    bool found = 0;                                                            \
    AssignOrMove##_RETURN_ON_EXCEPTION(                                        \
        isolate, found,                                                        \
        GetSingleCalendarField(isolate, fields,                                \
                               isolate->factory()->propertyName##_string(),    \
                               any, propertyName, Conversion));                \
    if (found) {                                                               \
      SETTER(resultField, propertyName);                                       \
    }                                                                          \
    REQUIRED_CHECK                                                             \
  }

  CALENDAR_FIELDS(GET_SINGLE_CALENDAR_FIELD);

#undef GET_SINGLE_CALENDAR_FIELD

  // 10. If requiredFieldNames is partial and any is false, then
  if (required_fields == RequiredFields::kPartial && !any) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                 "Must specify at least one calendar field."));
  }

  return Just(std::move(result));
}

#undef SIMPLE_SETTER
#undef MOVING_SETTER
#undef ANCHORED_SETTER_WITH_MOVE
#undef ANCHORED_SETTER_WITH_STR_CONVERSION
#undef SIMPLE_CONDITION
#undef ERA_CONDITION
#undef NOOP_REQUIRED_CHECK
#undef TIMEZONE_REQUIRED_CHECK
#undef CALENDAR_FIELDS

// ====== System time ======

// https://tc39.es/proposal-temporal/#sec-systemtimezoneidentifier
temporal_rs::TimeZone UTCTimeZoneInner() {
  auto result = temporal_rs::TimeZone::utc_with_provider(TimeZoneProvider());
  if (result.is_ok()) {
    return std::move(result).ok().value();
  }
  // TODO(Manishearth) use https://github.com/boa-dev/temporal/pull/554 instead
  // when we can
  return temporal_rs::TimeZone::zero();
}

// https://tc39.es/proposal-temporal/#sec-systemtimezoneidentifier
temporal_rs::TimeZone UTCTimeZone() {
  static temporal_rs::TimeZone UTC_TZ = UTCTimeZoneInner();
  return UTC_TZ;
}

// https://tc39.es/proposal-temporal/#sec-systemtimezoneidentifier
#ifdef V8_INTL_SUPPORT
temporal_rs::TimeZone SystemTimeZoneIdentifier() {
  auto tz_str = Intl::DefaultTimeZone();
  auto tz = temporal_rs::TimeZone::try_from_identifier_str_with_provider(
                tz_str, TimeZoneProvider())
                .ok();
  if (tz.has_value()) {
    return std::move(tz).value();
  }
  return UTCTimeZone();
}
#else   //  V8_INTL_SUPPORT
temporal_rs::TimeZone SystemTimeZoneIdentifier() { return UTCTimeZone(); }
#endif  //  V8_INTL_SUPPORT

// We don't have nanosecond precision counters, so it's pointless to perform
// numeric conversions back and forth.
//
// https://tc39.es/proposal-temporal/#sec-temporal-systemutcepochnanoseconds
// https://tc39.es/proposal-temporal/#sec-temporal-systemutcepochmilliseconds
int64_t SystemUTCEpochMilliseconds() {
  double ms =
      V8::GetCurrentPlatform()->CurrentClockTimeMillisecondsHighResolution();

  auto min = static_cast<double>(std::numeric_limits<int64_t>::min());
  auto max = static_cast<double>(std::numeric_limits<int64_t>::max());
  double clamped = std::clamp(ms, min, max);

  return static_cast<int64_t>(clamped);
}

// Gets a ZonedDateTime in the ISO calendar representing system time, using
// either a passed in timezone or the system timezone.
//
// Primarily patterned off of Now.zonedDateTimeISO(), all other callers can
// convert to specific types.
//
// https://tc39.es/proposal-temporal/#sec-temporal.now.zoneddatetimeiso
// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindatetimeiso
// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindateiso
// https://tc39.es/proposal-temporal/#sec-temporal.now.plaintimeiso
Maybe<std::unique_ptr<temporal_rs::ZonedDateTime>> GenericTemporalNowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  temporal_rs::TimeZone time_zone;

  // 1. If temporalTimeZoneLike is undefined, then
  if (IsUndefined(*temporal_time_zone_like)) {
    // a. Let timeZone be SystemTimeZoneIdentifier().
    time_zone = SystemTimeZoneIdentifier();
    // 2. Else,
  } else {
    // a. Let timeZone be ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
    MOVE_RETURN_ON_EXCEPTION(isolate, time_zone,
                             temporal::ToTemporalTimeZoneIdentifier(
                                 isolate, temporal_time_zone_like));
  }

  // 3. Let ns be SystemUTCEpochNanoseconds().
  auto ms = SystemUTCEpochMilliseconds();

  // 4. Return ! CreateTemporalZonedDateTime(ns, timeZone, "iso8601").
  // TODO(manishearth) we can avoid the multiple layers of allocation here
  // once https://github.com/boa-dev/temporal/pull/359 lands.
  std::unique_ptr<temporal_rs::Instant> instant;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, instant,
      ExtractRustResult(isolate,
                        temporal_rs::Instant::from_epoch_milliseconds(ms)));
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      ExtractRustResult(isolate, instant->to_zoned_date_time_iso_with_provider(
                                     time_zone, TimeZoneProvider())));

  return Just(std::move(zdt));
}

// ====== Construction operations ======

// Implementing val.toFoo() for different Temporal types Foo
// (this is distinct from the ToTemporalFoo abstract operations below)
template <typename DstType, typename SrcType>
MaybeDirectHandle<DstType> GenericToTemporalMethod(
    Isolate* isolate, DirectHandle<SrcType> val,
    TemporalAllocatedResult<typename DstType::RustType> (
        SrcType::RustType::*method)() const) {
  return ConstructRustWrappingType<DstType>(isolate,
                                            (val->wrapped_rust().*method)());
}

// Same as above, but for infallible conversions
template <typename DstType, typename SrcType>
MaybeDirectHandle<DstType> GenericToTemporalMethod(
    Isolate* isolate, DirectHandle<SrcType> val,
    std::unique_ptr<typename DstType::RustType> (SrcType::RustType::*method)()
        const) {
  return ConstructRustWrappingType<DstType>(isolate,
                                            (val->wrapped_rust().*method)());
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
//
// We split this out since Temporal is lazy-initialized; so we do not wish
// to call ConstructRustWrappingType in a context where Temporal has not yet
// been initialized (e.g. DurationFormat)
Maybe<std::unique_ptr<temporal_rs::Duration>> ToTemporalDurationRust(
    Isolate* isolate, DirectHandle<Object> item, const char* method_name) {
  // 1. If item is an Object and item has an [[InitializedTemporalDuration]]
  // internal slot, then a. a. Return ! CreateTemporalDuration(item.[[Years]],
  // item.[[Months]], item.[[Weeks]], item.[[Days]], item.[[Hours]],
  // item.[[Minutes]], item.[[Seconds]], item.[[Milliseconds]],
  // item.[[Microseconds]], item.[[Nanoseconds]]).
  if (IsJSTemporalDuration(*item)) {
    auto duration = Cast<JSTemporalDuration>(item);
    // i. Return !CreateTemporalInstant(item.[[EpochNanoseconds]]).
    return Just(duration->duration()->raw()->clone());
  }

  // 2. If item is not an Object, then
  if (!IsJSReceiver(*item)) {
    // a. If item is not a String, throw a TypeError exception.
    if (!IsString(*item)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR(
                          "Duration argument must be Duration or string."));
    }
    DirectHandle<String> str = Cast<String>(item);
    // b. Let result be ? ParseTemporalDurationString(string).
    DirectHandle<JSTemporalInstant> result;

    auto rust_result =
        HandleStringEncodings<TemporalAllocatedResult<temporal_rs::Duration>>(
            isolate, str,
            [](std::string_view view)
                -> TemporalAllocatedResult<temporal_rs::Duration> {
              return temporal_rs::Duration::from_utf8(view);
            },
            [](std::u16string_view view)
                -> TemporalAllocatedResult<temporal_rs::Duration> {
              return temporal_rs::Duration::from_utf16(view);
            });
    return ExtractRustResult(isolate, std::move(rust_result));
  }

  temporal_rs::PartialDuration partial;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, partial,
      temporal::ToTemporalPartialDurationRecord(isolate, item));

  return ExtractRustResult(
      isolate, temporal_rs::Duration::from_partial_duration(partial));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
MaybeDirectHandle<JSTemporalDuration> ToTemporalDuration(
    Isolate* isolate, DirectHandle<Object> item, const char* method_name) {
  std::unique_ptr<temporal_rs::Duration> duration;
  MOVE_RETURN_ON_EXCEPTION(isolate, duration,
                           ToTemporalDurationRust(isolate, item, method_name));
  return ConstructRustWrappingType<JSTemporalDuration>(isolate,
                                                       std::move(duration));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalduration
//
// Used by DurationFormat
Maybe<DurationRecord> ToTemporalDurationAsRecord(Isolate* isolate,
                                                 DirectHandle<Object> item,
                                                 const char* method_name) {
  std::unique_ptr<temporal_rs::Duration> duration;
  MOVE_RETURN_ON_EXCEPTION(isolate, duration,
                           ToTemporalDurationRust(isolate, item, method_name));
  return Just(temporal::DurationRecord{
      .years = static_cast<double>(duration->years()),
      .months = static_cast<double>(duration->months()),
      .weeks = static_cast<double>(duration->weeks()),
      .time_duration = {
          .days = static_cast<double>(duration->days()),
          .hours = static_cast<double>(duration->hours()),
          .minutes = static_cast<double>(duration->minutes()),
          .seconds = static_cast<double>(duration->seconds()),
          .milliseconds = static_cast<double>(duration->milliseconds()),
          .microseconds = static_cast<double>(duration->microseconds()),
          .nanoseconds = static_cast<double>(duration->nanoseconds()),
      }});
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalinstant
MaybeDirectHandle<JSTemporalInstant> ToTemporalInstant(
    Isolate* isolate, DirectHandle<Object> item, const char* method_name) {

  // 1. If item is an Object, then
  // a. If item has an [[InitializedTemporalInstant]] or
  //    [[InitializedTemporalZonedDateTime]] internal slot, then
  if (IsJSTemporalInstant(*item)) {
    auto instant = Cast<JSTemporalInstant>(item);
    // i. Return !CreateTemporalInstant(item.[[EpochNanoseconds]]).
    return ConstructRustWrappingType<JSTemporalInstant>(
        isolate, instant->instant()->raw()->clone());
    // ... or  [[InitializedTemporalZonedDateTime]] internal slot
  } else if (IsJSTemporalZonedDateTime(*item)) {
    auto zdt = Cast<JSTemporalZonedDateTime>(item);
    auto ns = zdt->zoned_date_time()->raw()->epoch_nanoseconds();
    // i. Return !CreateTemporalInstant(item.[[EpochNanoseconds]]).
    return ConstructRustWrappingType<JSTemporalInstant>(
        isolate, temporal_rs::Instant::try_new(ns));
  }
  // c. Set item to ?ToPrimitive(item, STRING).
  DirectHandle<Object> item_prim;
  if (IsJSReceiver(*item)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, item_prim,
        JSReceiver::ToPrimitive(isolate, Cast<JSReceiver>(item),
                                ToPrimitiveHint::kString));
  } else {
    item_prim = item;
  }

  if (!IsString(*item_prim)) {
    THROW_NEW_ERROR(
        isolate,
        NEW_TEMPORAL_TYPE_ERROR("Instant argument must be Instant or string."));
  }

  DirectHandle<String> item_string = Cast<String>(item_prim);


  auto rust_result =
      HandleStringEncodings<TemporalAllocatedResult<temporal_rs::Instant>>(
          isolate, item_string,
          [](std::string_view view)
              -> TemporalAllocatedResult<temporal_rs::Instant> {
            return temporal_rs::Instant::from_utf8(view);
          },
          [](std::u16string_view view)
              -> TemporalAllocatedResult<temporal_rs::Instant> {
            return temporal_rs::Instant::from_utf16(view);
          });
  return ConstructRustWrappingType<JSTemporalInstant>(isolate,
                                                      std::move(rust_result));
}

// A lot of branches in the ToTemporalFoo functions read the overflow object and
// do nothing with it
#define READ_AND_DISCARD_OVERFLOW(options_obj)                              \
  RETURN_ON_EXCEPTION(isolate, temporal::ToTemporalOverflowHandleUndefined( \
                                   isolate, options_obj, method_name))

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaltime
MaybeDirectHandle<JSTemporalPlainTime> ToTemporalTime(
    Isolate* isolate, DirectHandle<Object> item,
    MaybeDirectHandle<Object> options_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  // (handled by caller)

  // This error is eventually thrown by step 3; we perform a check early
  // so that we can optimize with InstanceType. Step 1 and 2 are unobservable.
  if (!IsHeapObject(*item)) {
    THROW_NEW_ERROR(
        isolate,
        NEW_TEMPORAL_TYPE_ERROR("Time-like argument must be object or string"));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*item)->map(isolate)->instance_type();

  // 2. If item is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    auto partial = temporal::kNullPartialTime;
    // a. If item has an [[InitializedTemporalTime]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainTime(instance_type)) {
      auto cast = Cast<JSTemporalPlainTime>(item);

      // ii. Perform ? GetTemporalOverflowOption(resolvedOptions).
      READ_AND_DISCARD_OVERFLOW(options_obj);

      // iii. Return !CreateTemporalTime(item.[[Time]]).
      return ConstructRustWrappingType<JSTemporalPlainTime>(
          isolate, cast->time()->raw()->clone());
      // b. If item has an [[InitializedTemporalDateTime]] internal slot, then
    } else if (InstanceTypeChecker::IsJSTemporalPlainDateTime(instance_type)) {
      // iii. Return ! CreateTemporalTime(item.[[ISODateTime]].[[Time]]).
      partial = GetPartialTime(Cast<JSTemporalPlainDateTime>(item));
      // c. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
      // then
    } else if (InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
      // i. Let isoDateTime be GetISODateTimeFor(item.[[TimeZone]],
      // item.[[EpochNanoseconds]]).
      partial = GetPartialTime(Cast<JSTemporalZonedDateTime>(item));
      // iv. Return !CreateTemporalTime(isoDateTime.[[Time]]).
    } else {
      // d. Let result be ?ToTemporalTimeRecord(item).
      DirectHandle<JSReceiver> item_recvr = Cast<JSReceiver>(item);
      temporal::TimeRecord record;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, record,
          temporal::ToTemporalTimeRecord(isolate, item_recvr, method_name));

      // e. Let resolvedOptions be ? GetOptionsObject(options).
      // f. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
      temporal_rs::ArithmeticOverflow overflow;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                                 temporal::ToTemporalOverflowHandleUndefined(
                                     isolate, options_obj, method_name));
      // g. Set result to ? RegulateTime(result.[[Hour]], result.[[Minute]],
      // result.[[Second]], result.[[Millisecond]], result.[[Microsecond]],
      // result.[[Nanosecond]], overflow).
      ASSIGN_RETURN_ON_EXCEPTION(isolate, partial,
                                 record.Regulate(isolate, overflow));

      return ConstructRustWrappingType<JSTemporalPlainTime>(
          isolate, temporal_rs::PlainTime::from_partial(partial, overflow));
    }

    // (found in each branch above)
    temporal_rs::ArithmeticOverflow overflow;
    // e. Let resolvedOptions be ?GetOptionsObject(options).
    // f. f. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                               temporal::ToTemporalOverflowHandleUndefined(
                                   isolate, options_obj, method_name));

    return ConstructRustWrappingType<JSTemporalPlainTime>(
        isolate, temporal_rs::PlainTime::from_partial(partial, overflow));

    // 3. Else,
  } else {
    // a. If item is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR(
                          "Time-like argument must be object or string"));
    }
    DirectHandle<String> str = Cast<String>(item);

    auto rust_result =
        HandleStringEncodings<TemporalAllocatedResult<temporal_rs::PlainTime>>(
            isolate, str,
            [](std::string_view view)
                -> TemporalAllocatedResult<temporal_rs::PlainTime> {
              return temporal_rs::PlainTime::from_utf8(view);
            },
            [](std::u16string_view view)
                -> TemporalAllocatedResult<temporal_rs::PlainTime> {
              return temporal_rs::PlainTime::from_utf16(view);
            });

    std::unique_ptr<temporal_rs::PlainTime> time;
    MOVE_RETURN_ON_EXCEPTION(
        isolate, time, ExtractRustResult(isolate, std::move(rust_result)));

    READ_AND_DISCARD_OVERFLOW(options_obj);
    return ConstructRustWrappingType<JSTemporalPlainTime>(isolate,
                                                          std::move(time));
  }
}

// Instead of returning Midnight, just returns null, since temporal_rs
// handles the midnight-setting
//
// https://tc39.es/proposal-temporal/#sec-temporal-totimerecordormidnight
Maybe<const temporal_rs::PlainTime*> ToTimeRecordOrMidnight(
    Isolate* isolate, DirectHandle<Object> item,
    DirectHandle<JSTemporalPlainTime>& output_time, const char* method_name) {
  // 1. If item is undefined, return MidnightTimeRecord().
  if (IsUndefined(*item)) {
    return Just(static_cast<const temporal_rs::PlainTime*>(nullptr));
  }

  // 2. Let plainTime be ? ToTemporalTime(item).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, output_time,
                             ToTemporalTime(isolate, item, {}, method_name));

  // 3. Return plainTime.[[Time]].
  return Just(
      static_cast<const temporal_rs::PlainTime*>(output_time->time()->raw()));
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldate
MaybeDirectHandle<JSTemporalPlainDate> ToTemporalDate(
    Isolate* isolate, DirectHandle<Object> item,
    MaybeDirectHandle<Object> options_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  // (handled by caller)

  // This error is eventually thrown by step 3a; we perform a check early
  // so that we can optimize with InstanceType. Step 1 and 2 are unobservable.
  if (!IsHeapObject(*item)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                 "Date argument must be object or string."));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*item)->map(isolate)->instance_type();
  // 2. If item is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    auto partial = temporal::kNullPartialDate;
    // a. If item has an [[InitializedTemporalDate]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainDate(instance_type)) {
      auto cast = Cast<JSTemporalPlainDate>(item);
      // ii. Perform ? GetTemporalOverflowOption(resolvedOptions).
      READ_AND_DISCARD_OVERFLOW(options_obj);
      // iii. Return !CreateTemporalDate(item.[[Date]], item.[[Calendar]]).
      return ConstructRustWrappingType<JSTemporalPlainDate>(
          isolate, cast->date()->raw()->clone());
      // b. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
      // then
    } else if (InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
      // i. Let isoDateTime be GetISODateTimeFor(item.[[TimeZone]],
      // item.[[EpochNanoseconds]]).
      //
      // iv. Return !CreateTemporalDate(isoDateTime.[[ISODate]],
      // item.[[Calendar]]).
      partial = GetPartialDate(Cast<JSTemporalZonedDateTime>(item));
      // c. If item has an [[InitializedTemporalDateTime]] internal slot, then
    } else if (InstanceTypeChecker::IsJSTemporalPlainDateTime(instance_type)) {
      // iii. Return !CreateTemporalDate(item.[[ISODateTime]].[[ISODate]],
      // item.[[Calendar]]).
      partial = GetPartialDate(Cast<JSTemporalPlainDateTime>(item));
    } else {
      // d. Let calendar be ?GetTemporalCalendarIdentifierWithISODefault(item).
      temporal_rs::AnyCalendarKind kind = temporal_rs::AnyCalendarKind::Iso;
      DirectHandle<JSReceiver> item_recvr = Cast<JSReceiver>(item);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, kind,
          temporal::GetTemporalCalendarIdentifierWithISODefault(isolate,
                                                                item_recvr));

      // e. Let fields be ?PrepareCalendarFields(calendar, item, « year, month,
      // month-code, day», «», «»).
      CombinedRecord fields;

      MOVE_RETURN_ON_EXCEPTION(
          isolate, fields,
          PrepareCalendarFields(isolate, kind, item_recvr, kAllDateFlags,
                                RequiredFields::kNone));
      temporal_rs::ArithmeticOverflow overflow;
      // f. Let resolvedOptions be ?GetOptionsObject(options).
      // g. f. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
      ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                                 temporal::ToTemporalOverflowHandleUndefined(
                                     isolate, options_obj, method_name));

      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, partial,
          fields.Regulate<temporal_rs::PartialDate>(isolate, overflow));
      return ConstructRustWrappingType<JSTemporalPlainDate>(
          isolate, temporal_rs::PlainDate::from_partial(partial, overflow));
    }

    // (from each branch above)
    // i. Let resolvedOptions be ? GetOptionsObject(options).
    // ii. Perform ? GetTemporalOverflowOption(resolvedOptions).
    READ_AND_DISCARD_OVERFLOW(options_obj);

    return ConstructRustWrappingType<JSTemporalPlainDate>(
        isolate, temporal_rs::PlainDate::from_partial(partial, std::nullopt));
    // 3. Else,
  } else {
    // a. If item is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                   "Date argument must be object or string."));
    }
    DirectHandle<String> str = Cast<String>(item);

    // Rest of the steps handled in Rust.

    std::unique_ptr<temporal_rs::ParsedDate> date;
    auto rust_result =
        HandleStringEncodings<TemporalAllocatedResult<temporal_rs::ParsedDate>>(
            isolate, str,
            [](std::string_view view)
                -> TemporalAllocatedResult<temporal_rs::ParsedDate> {
              return temporal_rs::ParsedDate::from_utf8(view);
            },
            [](std::u16string_view view)
                -> TemporalAllocatedResult<temporal_rs::ParsedDate> {
              return temporal_rs::ParsedDate::from_utf16(view);
            });
    MOVE_RETURN_ON_EXCEPTION(
        isolate, date, ExtractRustResult(isolate, std::move(rust_result)));

    // 9. Perform ? GetTemporalOverflowOption(resolvedOptions).
    READ_AND_DISCARD_OVERFLOW(options_obj);

    return ConstructRustWrappingType<JSTemporalPlainDate>(
        isolate, temporal_rs::PlainDate::from_parsed(*date));
  }
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporaldatetime
MaybeDirectHandle<JSTemporalPlainDateTime> ToTemporalDateTime(
    Isolate* isolate, DirectHandle<Object> item,
    MaybeDirectHandle<Object> options_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  // (handled by caller)

  // This error is eventually thrown by step 3; we perform a check early
  // so that we can optimize with InstanceType. Step 1 and 2 are unobservable.
  if (!IsHeapObject(*item)) {
    THROW_NEW_ERROR(
        isolate,
        NEW_TEMPORAL_TYPE_ERROR("DateTime argument must be object or string."));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*item)->map(isolate)->instance_type();

  // 2. If item is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    auto partial = temporal::kNullPartialDateTime;

    // a. If item has an [[InitializedTemporalDateTime]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainDateTime(instance_type)) {
      auto cast = Cast<JSTemporalPlainDateTime>(item);
      // ii. Perform ? GetTemporalOverflowOption(resolvedOptions).
      READ_AND_DISCARD_OVERFLOW(options_obj);
      // iii. Return !CreateTemporalDate(item.[[Date]], item.[[Calendar]]).
      return ConstructRustWrappingType<JSTemporalPlainDateTime>(
          isolate, cast->date_time()->raw()->clone());
      // b. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
      // then
    } else if (InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
      // i. Let isoDateTime be GetISODateTimeFor(item.[[TimeZone]],
      // item.[[EpochNanoseconds]]).
      //
      // iv. Return !CreateTemporalDateTime(isoDateTime, item.[[Calendar]]).
      partial = GetPartialDateTime(Cast<JSTemporalZonedDateTime>(item));
      // c. If item has an [[InitializedTemporalDate]] internal slot, then
    } else if (InstanceTypeChecker::IsJSTemporalPlainDate(instance_type)) {
      // iii. Return !CreateTemporalDate(item.[[ISODateTime]].[[ISODate]],
      // item.[[Calendar]]).
      partial = GetPartialDateTime(Cast<JSTemporalPlainDate>(item));
    } else {
      // d. Let calendar be ?GetTemporalCalendarIdentifierWithISODefault(item).
      temporal_rs::AnyCalendarKind kind = temporal_rs::AnyCalendarKind::Iso;
      DirectHandle<JSReceiver> item_recvr = Cast<JSReceiver>(item);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, kind,
          temporal::GetTemporalCalendarIdentifierWithISODefault(isolate,
                                                                item_recvr));

      // e. Let fields be ?PrepareCalendarFields(calendar, item, « year, month,
      // month-code, day», « hour, minute, second, millisecond, microsecond,
      // nanosecond», «»).
      CombinedRecord fields;
      using enum CalendarFieldsFlag;

      MOVE_RETURN_ON_EXCEPTION(
          isolate, fields,
          PrepareCalendarFields(isolate, kind, item_recvr,
                                kAllDateFlags | kTimeFields,
                                RequiredFields::kNone));

      temporal_rs::ArithmeticOverflow overflow;
      // f. Let resolvedOptions be ?GetOptionsObject(options).
      // g. f. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
      ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                                 temporal::ToTemporalOverflowHandleUndefined(
                                     isolate, options_obj, method_name));

      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, partial,
          fields.Regulate<temporal_rs::PartialDateTime>(isolate, overflow));
      return ConstructRustWrappingType<JSTemporalPlainDateTime>(
          isolate, temporal_rs::PlainDateTime::from_partial(partial, overflow));
    }

    temporal_rs::ArithmeticOverflow overflow;
    // f. Let resolvedOptions be ?GetOptionsObject(options).
    // g. f. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
    ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                               temporal::ToTemporalOverflowHandleUndefined(
                                   isolate, options_obj, method_name));

    return ConstructRustWrappingType<JSTemporalPlainDateTime>(
        isolate, temporal_rs::PlainDateTime::from_partial(partial, overflow));

  } else {
    // 3. If item is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR(
                          "DateTime argument must be object or string."));
    }
    DirectHandle<String> str = Cast<String>(item);

    // Rest of the steps handled in Rust

    std::unique_ptr<temporal_rs::ParsedDateTime> date;
    auto rust_result = HandleStringEncodings<
        TemporalAllocatedResult<temporal_rs::ParsedDateTime>>(
        isolate, str,
        [](std::string_view view)
            -> TemporalAllocatedResult<temporal_rs::ParsedDateTime> {
          return temporal_rs::ParsedDateTime::from_utf8(view);
        },
        [](std::u16string_view view)
            -> TemporalAllocatedResult<temporal_rs::ParsedDateTime> {
          return temporal_rs::ParsedDateTime::from_utf16(view);
        });
    MOVE_RETURN_ON_EXCEPTION(
        isolate, date, ExtractRustResult(isolate, std::move(rust_result)));

    // 9. Perform ? GetTemporalOverflowOption(resolvedOptions).
    READ_AND_DISCARD_OVERFLOW(options_obj);

    return ConstructRustWrappingType<JSTemporalPlainDateTime>(
        isolate, temporal_rs::PlainDateTime::from_parsed(*date));
  }
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalyearmonth
MaybeDirectHandle<JSTemporalPlainYearMonth> ToTemporalYearMonth(
    Isolate* isolate, DirectHandle<Object> item,
    MaybeDirectHandle<Object> options_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  // (handled by caller)

  // This error is eventually thrown by step 3; we perform a check early
  // so that we can optimize with InstanceType. Step 1 and 2 are unobservable.
  if (!IsHeapObject(*item)) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_TYPE_ERROR(
                        "YearMonth argument must be object or string."));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*item)->map(isolate)->instance_type();
  // 2. If item is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    // a. If item has an [[InitializedTemporalYearMonth]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainYearMonth(instance_type)) {
      auto cast = Cast<JSTemporalPlainYearMonth>(item);

      // ii. Perform ? GetTemporalOverflowOption(resolvedOptions).
      READ_AND_DISCARD_OVERFLOW(options_obj);
      // iii. Return ! CreateTemporalYearMonth(item.[[ISODate]],
      // item.[[Calendar]]).
      return ConstructRustWrappingType<JSTemporalPlainYearMonth>(
          isolate, cast->year_month()->raw()->clone());
    } else {
      // b. Let calendar be ?GetTemporalCalendarIdentifierWithISODefault(item).
      DirectHandle<JSReceiver> item_recvr = Cast<JSReceiver>(item);

      temporal_rs::AnyCalendarKind kind;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, kind,
          temporal::GetTemporalCalendarIdentifierWithISODefault(isolate,
                                                                item_recvr));

      // c. Let fields be ?PrepareCalendarFields(calendar, item, « year, month,
      // month-code», «», «»).
      CombinedRecord fields;

      using enum CalendarFieldsFlag;

      MOVE_RETURN_ON_EXCEPTION(isolate, fields,
                               PrepareCalendarFields(isolate, kind, item_recvr,
                                                     kYearFields | kMonthFields,
                                                     RequiredFields::kNone));

      // e. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).

      temporal_rs::ArithmeticOverflow overflow;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                                 temporal::ToTemporalOverflowHandleUndefined(
                                     isolate, options_obj, method_name));

      temporal_rs::PartialDate partial = temporal::kNullPartialDate;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, partial,
          fields.Regulate<temporal_rs::PartialDate>(isolate, overflow));
      // f. Let isoDate be ? CalendarYearMonthFromFields(calendar, fields,
      // overflow).
      //
      // g. Return !CreateTemporalYearMonth(isoDate, calendar).
      return ConstructRustWrappingType<JSTemporalPlainYearMonth>(
          isolate,
          temporal_rs::PlainYearMonth::from_partial(partial, overflow));
    }
  } else {
    // 3. If item is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR(
                          "YearMonth argument must be object or string."));
    }
    DirectHandle<String> str = Cast<String>(item);

    // Rest of the steps handled in Rust

    std::unique_ptr<temporal_rs::ParsedDate> date;
    auto rust_result =
        HandleStringEncodings<TemporalAllocatedResult<temporal_rs::ParsedDate>>(
            isolate, str,
            [](std::string_view view)
                -> TemporalAllocatedResult<temporal_rs::ParsedDate> {
              return temporal_rs::ParsedDate::year_month_from_utf8(view);
            },
            [](std::u16string_view view)
                -> TemporalAllocatedResult<temporal_rs::ParsedDate> {
              return temporal_rs::ParsedDate::year_month_from_utf16(view);
            });
    MOVE_RETURN_ON_EXCEPTION(
        isolate, date, ExtractRustResult(isolate, std::move(rust_result)));

    // 9. Perform ? GetTemporalOverflowOption(resolvedOptions).
    READ_AND_DISCARD_OVERFLOW(options_obj);

    return ConstructRustWrappingType<JSTemporalPlainYearMonth>(
        isolate, temporal_rs::PlainYearMonth::from_parsed(*date));
  }
}

struct ZDTOptions {
  temporal_rs::Disambiguation disambiguation;
  temporal_rs::OffsetDisambiguation offset_option;
  temporal_rs::ArithmeticOverflow overflow;
};

Maybe<ZDTOptions> GetZDTOptions(Isolate* isolate,
                                MaybeDirectHandle<Object> maybe_options_obj,
                                const char* method_name) {
  ZDTOptions options =
      ZDTOptions{.disambiguation = temporal_rs::Disambiguation::Compatible,
                 .offset_option = temporal_rs::OffsetDisambiguation::Reject,
                 .overflow = temporal_rs::ArithmeticOverflow::Constrain};

  DirectHandle<Object> options_obj;

  if (!maybe_options_obj.ToHandle(&options_obj)) {
    return Just(options);
  }
  // (ToTemporalZonedDateTime) g. Let resolvedOptions be
  // ?GetOptionsObject(options).
  //
  // (ToTemporalZonedDateTime) h. Let disambiguation be
  // ?GetTemporalDisambiguationOption(resolvedOptions).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options.disambiguation,
      temporal::GetTemporalDisambiguationOptionHandleUndefined(
          isolate, options_obj, method_name));

  // (ToTemporalZonedDateTime) i. Let offsetOption be
  // ?GetTemporalOffsetOption(resolvedOptions, reject).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options.offset_option,
      temporal::GetTemporalOffsetOptionHandleUndefined(
          isolate, options_obj, temporal_rs::OffsetDisambiguation::Reject,
          method_name));
  // (ToTemporalZonedDateTime) ii. Perform
  // ?GetTemporalOverflowOption(resolvedOptions).
  ASSIGN_RETURN_ON_EXCEPTION(isolate, options.overflow,
                             temporal::ToTemporalOverflowHandleUndefined(
                                 isolate, options_obj, method_name));

  return Just(options);
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalzoneddatetime
// Note this skips the options-parsing steps and instead asks the caller to pass
// it in
MaybeDirectHandle<JSTemporalZonedDateTime> ToTemporalZonedDateTime(
    Isolate* isolate, DirectHandle<Object> item,
    MaybeDirectHandle<Object> options_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  // (handled by caller)

  // This error is eventually thrown by step 3; we perform a check early
  // so that we can optimize with InstanceType. Step 1 and 2 are unobservable.
  if (!IsHeapObject(*item)) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_TYPE_ERROR(
                        "ZonedDateTime argument must be object or string."));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*item)->map(isolate)->instance_type();

  // 2. Let offsetBehaviour be option.
  // 3. Let matchBehaviour be match-exactly.
  // (handled in Rust)

  // 4. If item is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    // a. If item has an [[InitializedTemporalZonedDateTime]] internal slot,
    // then
    if (InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
      auto cast = Cast<JSTemporalZonedDateTime>(item);

      // iii. Perform ? GetTemporalDisambiguationOption(resolvedOptions).
      // iv. Perform ? GetTemporalOffsetOption(resolvedOptions, reject).
      // v. Perform ? GetTemporalOverflowOption(resolvedOptions).
      ZDTOptions options;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, options, GetZDTOptions(isolate, options_obj, method_name));
      // vi. Return !CreateTemporalZonedDateTime(item.[[EpochNanoseconds]],
      // item.[[TimeZone]], item.[[Calendar]]).
      return ConstructRustWrappingType<JSTemporalZonedDateTime>(
          isolate, cast->zoned_date_time()->raw()->clone());

    } else {
      // b. Let calendar be ?GetTemporalCalendarIdentifierWithISODefault(item).
      temporal_rs::AnyCalendarKind kind = temporal_rs::AnyCalendarKind::Iso;
      DirectHandle<JSReceiver> item_recvr = Cast<JSReceiver>(item);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, kind,
          temporal::GetTemporalCalendarIdentifierWithISODefault(isolate,
                                                                item_recvr));

      // c. Let fields be ? PrepareCalendarFields(calendar, item, « year,
      // month, month-code, day», « hour, minute, second, millisecond,
      // microsecond, nanosecond, offset, time-zone», « time-zone»).
      CombinedRecord fields;
      using enum CalendarFieldsFlag;
      MOVE_RETURN_ON_EXCEPTION(
          isolate, fields,
          PrepareCalendarFields(
              isolate, kind, item_recvr,
              kAllDateFlags | kTimeFields | kOffset | kTimeZone,
              RequiredFields::kTimeZone));

      // h. Perform ? GetTemporalDisambiguationOption(resolvedOptions).
      // i. Perform ? GetTemporalOffsetOption(resolvedOptions, reject).
      // j. Perform ? GetTemporalOverflowOption(resolvedOptions).
      ZDTOptions options;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, options, GetZDTOptions(isolate, options_obj, method_name));

      temporal_rs::PartialZonedDateTime partial = kNullPartialZonedDateTime;

      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, partial,
          fields.Regulate<temporal_rs::PartialZonedDateTime>(isolate,
                                                             options.overflow));

      return ConstructRustWrappingType<JSTemporalZonedDateTime>(
          isolate, temporal_rs::ZonedDateTime::from_partial_with_provider(
                       partial, options.overflow, options.disambiguation,
                       options.offset_option, TimeZoneProvider()));
    }

  } else {
    // 3. If item is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR(
                          "ZonedDateTime argument must be object or string."));
    }
    DirectHandle<String> str = Cast<String>(item);

    // b. Let result be ? ParseISODateTime(item, «
    // TemporalDateTimeString[+Zoned] »).
    //
    // Steps b-l handled in Rust
    std::unique_ptr<temporal_rs::ParsedZonedDateTime> parsed;

    auto rust_result = HandleStringEncodings<
        TemporalAllocatedResult<temporal_rs::ParsedZonedDateTime>>(
        isolate, str,
        [](std::string_view view)
            -> TemporalAllocatedResult<temporal_rs::ParsedZonedDateTime> {
          return temporal_rs::ParsedZonedDateTime::from_utf8_with_provider(
              view, TimeZoneProvider());
        },
        [](std::u16string_view view)
            -> TemporalAllocatedResult<temporal_rs::ParsedZonedDateTime> {
          return temporal_rs::ParsedZonedDateTime::from_utf16_with_provider(
              view, TimeZoneProvider());
        });
    MOVE_RETURN_ON_EXCEPTION(
        isolate, parsed, ExtractRustResult(isolate, std::move(rust_result)));

    // o. Perform ? GetTemporalDisambiguationOption(resolvedOptions).
    // p. Perform ? GetTemporalOffsetOption(resolvedOptions, reject).
    // q. Perform ? GetTemporalOverflowOption(resolvedOptions).
    ZDTOptions options;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, options, GetZDTOptions(isolate, options_obj, method_name));

    // Rest of the steps handled in Rust
    return ConstructRustWrappingType<JSTemporalZonedDateTime>(
        isolate, temporal_rs::ZonedDateTime::from_parsed_with_provider(
                     *parsed, options.disambiguation, options.offset_option,
                     TimeZoneProvider()));
  }
}

// https://tc39.es/proposal-temporal/#sec-temporal-totemporalmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay> ToTemporalMonthDay(
    Isolate* isolate, DirectHandle<Object> item,
    MaybeDirectHandle<Object> options_obj, const char* method_name) {
  // 1. If options is not present, set options to undefined.
  // (handled by caller)

  // This error is eventually thrown by step 3; we perform a check early
  // so that we can optimize with InstanceType. Step 1 and 2 are unobservable.
  if (!IsHeapObject(*item)) {
    THROW_NEW_ERROR(
        isolate,
        NEW_TEMPORAL_TYPE_ERROR("MonthDay argument must be object or string."));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*item)->map(isolate)->instance_type();
  // 2. If item is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    // a. If item has an [[InitializedTemporalMonthDay]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainMonthDay(instance_type)) {
      auto cast = Cast<JSTemporalPlainMonthDay>(item);

      // ii. Perform ? GetTemporalOverflowOption(resolvedOptions).
      READ_AND_DISCARD_OVERFLOW(options_obj);

      // iii. Return ! CreateTemporalMonthDay(item.[[ISODate]],
      // item.[[Calendar]]).
      return ConstructRustWrappingType<JSTemporalPlainMonthDay>(
          isolate, cast->month_day()->raw()->clone());
    } else {
      // b. Let calendar be ?GetTemporalCalendarIdentifierWithISODefault(item).
      DirectHandle<JSReceiver> item_recvr = Cast<JSReceiver>(item);

      temporal_rs::AnyCalendarKind kind;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, kind,
          temporal::GetTemporalCalendarIdentifierWithISODefault(isolate,
                                                                item_recvr));

      // c. Let fields be ?PrepareCalendarFields(calendar, item, « year, month,
      // month-code, day», «», «»).
      CombinedRecord fields;

      using enum CalendarFieldsFlag;

      MOVE_RETURN_ON_EXCEPTION(
          isolate, fields,
          PrepareCalendarFields(isolate, kind, item_recvr,
                                kYearFields | kMonthFields | kDay,
                                RequiredFields::kNone));

      // Remaining steps handled in Rust

      // e. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).

      temporal_rs::ArithmeticOverflow overflow;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                                 temporal::ToTemporalOverflowHandleUndefined(
                                     isolate, options_obj, method_name));
      // f. Let isoDate be ? CalendarMonthDayFromFields(calendar, fields,
      //    overflow).
      //
      // g. Return ! CreateTemporalMonthDay(isoDate, calendar).
      temporal_rs::PartialDate partial = temporal::kNullPartialDate;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, partial,
          fields.Regulate<temporal_rs::PartialDate>(isolate, overflow));

      return ConstructRustWrappingType<JSTemporalPlainMonthDay>(
          isolate, temporal_rs::PlainMonthDay::from_partial(partial, overflow));
    }
  } else {
    // 3. If item is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR(
                          "MonthDay argument must be object or string."));
    }
    DirectHandle<String> str = Cast<String>(item);
    DirectHandle<JSTemporalPlainDate> result;

    // Rest of the steps handled in Rust

    std::unique_ptr<temporal_rs::ParsedDate> date;
    auto rust_result =
        HandleStringEncodings<TemporalAllocatedResult<temporal_rs::ParsedDate>>(
            isolate, str,
            [](std::string_view view)
                -> TemporalAllocatedResult<temporal_rs::ParsedDate> {
              return temporal_rs::ParsedDate::month_day_from_utf8(view);
            },
            [](std::u16string_view view)
                -> TemporalAllocatedResult<temporal_rs::ParsedDate> {
              return temporal_rs::ParsedDate::month_day_from_utf16(view);
            });
    MOVE_RETURN_ON_EXCEPTION(
        isolate, date, ExtractRustResult(isolate, std::move(rust_result)));

    // 9. Perform ? GetTemporalOverflowOption(resolvedOptions).
    READ_AND_DISCARD_OVERFLOW(options_obj);

    return ConstructRustWrappingType<JSTemporalPlainMonthDay>(
        isolate, temporal_rs::PlainMonthDay::from_parsed(*date));
  }
}

// For calling ToTemporalFoo in generic contexts.
// Note that this ignores all options `overflow`.
template <typename JSType>
MaybeDirectHandle<JSType> ToTemporalGeneric(
    Isolate* isolate, DirectHandle<Object> item,
    const char* method_name);

#define DEFINE_TO_TEMPORAL_GENERIC(Type)                                       \
  template <>                                                                  \
  MaybeDirectHandle<JSTemporalPlain##Type>                                     \
  ToTemporalGeneric<JSTemporalPlain##Type>(                                    \
      Isolate * isolate, DirectHandle<Object> item, const char* method_name) { \
    return ToTemporal##Type(isolate, item, {}, method_name);                   \
  }

DEFINE_TO_TEMPORAL_GENERIC(Time)
DEFINE_TO_TEMPORAL_GENERIC(Date)
DEFINE_TO_TEMPORAL_GENERIC(DateTime)
DEFINE_TO_TEMPORAL_GENERIC(YearMonth)
DEFINE_TO_TEMPORAL_GENERIC(MonthDay)

#undef DEFINE_TO_TEMPORAL_GENERIC

// These two have a different number of arguments
template <>
MaybeDirectHandle<JSTemporalInstant> ToTemporalGeneric<JSTemporalInstant>(
    Isolate* isolate, DirectHandle<Object> item,
    const char* method_name) {
  return ToTemporalInstant(isolate, item, method_name);
}

template <>
MaybeDirectHandle<JSTemporalZonedDateTime>
ToTemporalGeneric<JSTemporalZonedDateTime>(
    Isolate* isolate, DirectHandle<Object> item,
    const char* method_name) {
  return ToTemporalZonedDateTime(isolate, item, {}, method_name);
}

// A class that wraps a PlainDate or a ZonedDateTime that allows
// them to be either owned in a unique_ptr or borrowed.
//
// Setters should only be called once (this is not a safety
// invariant, but the spec should not be setting things multiple
// times)
class RelativeTo {
 public:
  RelativeTo()
      : date_(std::nullopt),
        zoned_(std::nullopt),
        date_ptr_(nullptr),
        zoned_ptr_(nullptr) {}

  // These methods are not constructors so that they can be explicitly invoked,
  // to avoid e.g. passing in an owned type as a pointer.
  static RelativeTo Owned(std::unique_ptr<temporal_rs::PlainDate>&& val) {
    RelativeTo ret;
    ret.date_ = std::move(val);
    ret.date_ptr_ = ret.date_.value().get();
    return ret;
  }
  static RelativeTo Owned(std::unique_ptr<temporal_rs::ZonedDateTime>&& val) {
    RelativeTo ret;
    ret.zoned_ = std::move(val);
    ret.zoned_ptr_ = ret.zoned_.value().get();
    return ret;
  }

  static RelativeTo Owned(temporal_rs::OwnedRelativeTo&& owned) {
    if (owned.date) {
      return Owned(std::move(owned.date));
    } else if (owned.zoned) {
      return Owned(std::move(owned.zoned));
    }

    return RelativeTo();
  }

  static RelativeTo Borrowed(temporal_rs::PlainDate const* val) {
    RelativeTo ret;
    ret.date_ptr_ = val;
    return ret;
  }

  static RelativeTo Borrowed(temporal_rs::ZonedDateTime const* val) {
    RelativeTo ret;
    ret.zoned_ptr_ = val;
    return ret;
  }
  temporal_rs::RelativeTo ToRust() const {
    return temporal_rs::RelativeTo{
        .date = date_ptr_,
        .zoned = zoned_ptr_,
    };
  }

 private:
  std::optional<std::unique_ptr<temporal_rs::PlainDate>> date_;
  std::optional<std::unique_ptr<temporal_rs::ZonedDateTime>> zoned_;

  temporal_rs::PlainDate const* date_ptr_;
  temporal_rs::ZonedDateTime const* zoned_ptr_;
};

// https://tc39.es/proposal-temporal/#sec-temporal-gettemporalrelativetooption
// Also handles the undefined case from GetOptionsObject
Maybe<RelativeTo> GetTemporalRelativeToOptionHandleUndefined(
    Isolate* isolate, DirectHandle<Object> options) {
  RelativeTo ret;

  // Default is empty
  if (IsUndefined(*options)) {
    return Just(RelativeTo());
  }

  auto relativeto_ident = isolate->factory()->relativeTo_string();
  if (!IsJSReceiver(*options)) {
    // (GetOptionsObject) 3. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR_WITH_ARG(
                                 kOptionMustBeObject, relativeto_ident));
  }

  // 1. Let value be ?Get(options, "relativeTo").
  DirectHandle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value,
      JSReceiver::GetProperty(isolate, Cast<JSReceiver>(options),
                              relativeto_ident));

  // 2. If value is undefined, return the Record { [[PlainRelativeTo]]:
  // undefined, [[ZonedRelativeTo]]: undefined }.
  if (IsUndefined(*value)) {
    return Just(RelativeTo());
  }

  // 3. Let offsetBehaviour be option.
  // 4. Let matchBehaviour be match-exactly.

  // This error is eventually thrown by step 6a; we perform a check early
  // so that we can optimize with InstanceType. Step 5-6 are unobservable
  // in this case.
  if (!IsHeapObject(*value)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                 "relativeTo must be object or string."));
  }
  InstanceType instance_type =
      Cast<HeapObject>(*value)->map(isolate)->instance_type();

  // 5. If value is an Object, then
  if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    // a. If value has an [[InitializedTemporalZonedDateTime]] internal slot,
    // then
    if (InstanceTypeChecker::IsJSTemporalZonedDateTime(instance_type)) {
      // i. Return the Record { [[PlainRelativeTo]]: undefined,
      // [[ZonedRelativeTo]]: value }.
      return Just(RelativeTo::Borrowed(
          Cast<JSTemporalZonedDateTime>(value)->zoned_date_time()->raw()));
    }

    // b. If value has an [[InitializedTemporalDate]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainDate(instance_type)) {
      // i. Return the Record { [[PlainRelativeTo]]: value, [[ZonedRelativeTo]]:
      // undefined }.
      return Just(RelativeTo::Borrowed(
          Cast<JSTemporalPlainDate>(value)->date()->raw()));
    }

    // c. If value has an [[InitializedTemporalDateTime]] internal slot, then
    if (InstanceTypeChecker::IsJSTemporalPlainDateTime(instance_type)) {
      // i. Let plainDate be
      // !CreateTemporalDate(value.[[ISODateTime]].[[ISODate]],
      // value.[[Calendar]]).
      auto date_record = GetPartialDate(Cast<JSTemporalPlainDateTime>(value));
      std::unique_ptr<temporal_rs::PlainDate> plain_date = nullptr;

      MOVE_RETURN_ON_EXCEPTION(
          isolate, plain_date,
          ExtractRustResult(isolate, temporal_rs::PlainDate::from_partial(
                                         date_record, std::nullopt)));
      // ii. Return the Record { [[PlainRelativeTo]]: plainDate,
      // [[ZonedRelativeTo]]: undefined }.
      return Just(RelativeTo::Owned(std::move(plain_date)));
    }
    // d. Let calendar be ? GetTemporalCalendarIdentifierWithISODefault(value).
    temporal_rs::AnyCalendarKind kind = temporal_rs::AnyCalendarKind::Iso;
    auto value_recvr = Cast<JSReceiver>(value);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, kind,
        temporal::GetTemporalCalendarIdentifierWithISODefault(isolate,
                                                              value_recvr));
    // e. Let fields be ? PrepareCalendarFields(calendar, value, « year, month,
    // month-code, day », « hour, minute, second, millisecond, microsecond,
    // nanosecond, offset, time-zone », «»).
    CombinedRecord fields;

    using enum CalendarFieldsFlag;
    MOVE_RETURN_ON_EXCEPTION(
        isolate, fields,
        PrepareCalendarFields(isolate, kind, value_recvr,
                              kAllDateFlags | kTimeFields | kOffset | kTimeZone,
                              RequiredFields::kNone));

    auto partial = kNullPartialZonedDateTime;

    // f. Let result be ? InterpretTemporalDateTimeFields(calendar, fields,
    // constrain).
    // g. Let timeZone be fields.[[TimeZone]].
    // h. Let offsetString be fields.[[OffsetString]].
    // j. Let isoDate be result.[[ISODate]].
    // k. Let time be result.[[Time]].
    auto overflow = temporal_rs::ArithmeticOverflow::Constrain;

    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, partial,
        fields.Regulate<temporal_rs::PartialZonedDateTime>(isolate, overflow));

    // We use different construction methods for ZonedDateTime in these two
    // branches, so we've pulled steps 7-12 into this branch

    // 7. If timeZone is unset, then
    if (!partial.timezone) {
      // a. Let plainDate be ? CreateTemporalDate(isoDate, calendar).
      std::unique_ptr<temporal_rs::PlainDate> plain_relative_to;
      MOVE_RETURN_ON_EXCEPTION(
          isolate, plain_relative_to,
          ExtractRustResult(isolate, temporal_rs::PlainDate::from_partial(
                                         partial.date, overflow)));

      // b. Return the Record { [[PlainRelativeTo]]: plainDate,
      // [[ZonedRelativeTo]]: undefined }.
      return Just(RelativeTo::Owned(std::move(plain_relative_to)));
    }
    // 8. If offsetBehaviour is option, then
    // a. Let offsetNs be ! ParseDateTimeUTCOffset(offsetString).
    // 9. Else,
    // a. Let offsetNs be 0.
    // 10. Let epochNanoseconds be ? InterpretISODateTimeOffset(isoDate, time,
    // offsetBehaviour, offsetNs, timeZone, compatible, reject, matchBehaviour).

    // 11. Let zonedRelativeTo be !
    // CreateTemporalZonedDateTime(epochNanoseconds, timeZone, calendar).

    // (All these steps handled in Rust)

    std::unique_ptr<temporal_rs::ZonedDateTime> zoned_relative_to;
    MOVE_RETURN_ON_EXCEPTION(
        isolate, zoned_relative_to,
        ExtractRustResult(
            isolate,
            temporal_rs::ZonedDateTime::from_partial_with_provider(
                partial, overflow, temporal_rs::Disambiguation::Compatible,
                temporal_rs::OffsetDisambiguation::Reject,
                TimeZoneProvider())));
    // 12. Return the Record { [[PlainRelativeTo]]: undefined,
    // [[ZonedRelativeTo]]: zonedRelativeTo }.
    return Just(RelativeTo::Owned(std::move(zoned_relative_to)));

    // 6. Else,
  } else {
    // a. If value is not a String, throw a TypeError exception.
    if (!InstanceTypeChecker::IsString(instance_type)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                   "relativeTo must be object or string."));
    }

    DirectHandle<String> str = Cast<String>(value);

    // b. Let result be ? ParseISODateTime(value, «
    // TemporalDateTimeString[+Zoned], TemporalDateTimeString[~Zoned] »). Rest
    // of the steps handled in Rust
    temporal_rs::OwnedRelativeTo relative_to;

    auto rust_result =
        HandleStringEncodings<TemporalResult<temporal_rs::OwnedRelativeTo>>(
            isolate, str,
            [](std::string_view view)
                -> TemporalResult<temporal_rs::OwnedRelativeTo> {
              return temporal_rs::OwnedRelativeTo::from_utf8_with_provider(
                  view, TimeZoneProvider());
            },
            [](std::u16string_view view)
                -> TemporalResult<temporal_rs::OwnedRelativeTo> {
              return temporal_rs::OwnedRelativeTo::from_utf16_with_provider(
                  view, TimeZoneProvider());
            });
    MOVE_RETURN_ON_EXCEPTION(
        isolate, relative_to,
        ExtractRustResult(isolate, std::move(rust_result)));

    // 12. Return the Record { [[PlainRelativeTo]]: undefined,
    // [[ZonedRelativeTo]]: zonedRelativeTo }.
    return Just(RelativeTo::Owned(std::move(relative_to)));
  }
}

// ====== Difference operations ======

// ProviderArg is only for wrapping temporal_rs::Provider&
template <typename RustType, typename... ProviderArg>
using DifferenceOperation = TemporalAllocatedResult<temporal_rs::Duration> (
    RustType::*)(const RustType&, temporal_rs::DifferenceSettings,
                 const ProviderArg&...) const;

// Generic function for all DifferenceTemporalFoo operations
//
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalinstant
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindate
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplaindatetime
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalplainyearmonth
// https://tc39.es/proposal-temporal/#sec-temporal-differencetemporalzoneddatetime
template <typename JSType, typename... ProviderArg>
MaybeDirectHandle<JSTemporalDuration> GenericDifferenceTemporal(
    Isolate* isolate,
    DifferenceOperation<typename JSType::RustType, ProviderArg...> operation,
    UnitGroup group, Unit fallback_smallest_unit, DirectHandle<JSType> handle,
    DirectHandle<Object> other_obj, DirectHandle<Object> options,
    const char* method_name, const ProviderArg&... provider) {
  // Steps are written for PlainDate, but are similar for other types

  // 1. Set other to ?ToTemporalDate(other).
  DirectHandle<JSType> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalGeneric<JSType>(isolate, other_obj, method_name));

  auto settings = temporal_rs::DifferenceSettings{.largest_unit = std::nullopt,
                                                  .smallest_unit = std::nullopt,
                                                  .rounding_mode = std::nullopt,
                                                  .increment = std::nullopt};

  auto& this_rust = handle->wrapped_rust();
  auto& other_rust = other->wrapped_rust();

  // This step only exists for calendared types (everything other than PlainTime
  // and Instant)
  if constexpr (JSType::kTypeContainsCalendar) {
    // 2. If CalendarEquals(temporalDate.[[Calendar]], other.[[Calendar]]) is
    // false, throw a RangeError exception.
    if (this_rust.calendar().kind() != other_rust.calendar().kind()) {
      THROW_NEW_ERROR(isolate,
                      NewRangeError(MessageTemplate::kMismatchedCalendars));
    }
  }

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  // 4. Let settings be ? GetDifferenceSettings(operation, resolvedOptions,
  // date, « », day, day).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, settings,
      temporal::GetDifferenceSettingsWithoutChecks(
          isolate, options, group, fallback_smallest_unit, method_name));

  // Remaining steps handled by temporal_rs
  // operation negation (step 6) is also handled in temporal_rs, we should not
  // negate again here.

  auto diff = (this_rust.*operation)(other_rust, settings, provider...);

  return ConstructRustWrappingType<JSTemporalDuration>(isolate,
                                                       std::move(diff));
}

// ====== Add operations ======

// Wrapping Rust-side ::add and ::subtract methods
// OverflowArgument is generic since some of these
// methods take std::optional.
//
// ProviderArg is only for wrapping temporal_rs::Provider&
template <typename RustType, typename OverflowArgument, typename... ProviderArg>
using BinaryOperation = TemporalAllocatedResult<RustType> (RustType::*)(
    const temporal_rs::Duration&, OverflowArgument,
    const ProviderArg&...) const;

// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodate
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtodatetime
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
// https://tc39.es/proposal-temporal/#sec-temporal-adddurationtozoneddatetime
template <
    typename JSType,
    typename OverflowArgument = std::optional<temporal_rs::ArithmeticOverflow>,
    typename... ProviderArg>
MaybeDirectHandle<JSType> AddDurationToGeneric(
    Isolate* isolate,
    BinaryOperation<typename JSType::RustType, OverflowArgument, ProviderArg...>
        operation,
    DirectHandle<JSType> temporal_js_type,
    DirectHandle<Object> temporal_duration_like,
    DirectHandle<Object> options_obj, const char* method_name,
    const ProviderArg&... provider) {
  // 1. Let duration be ? ToTemporalDuration(temporalDurationLike).
  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, other_duration,
                             temporal::ToTemporalDuration(
                                 isolate, temporal_duration_like, method_name));

  // 2. If operation is subtract, set duration to
  // CreateNegatedTemporalDuration(duration).

  // (handled by Rust, we should not double-negate)

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  // 4. Let overflow be ?GetTemporalOverflowOption(resolvedOptions).
  temporal_rs::ArithmeticOverflow overflow;

  ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                             temporal::ToTemporalOverflowHandleUndefined(
                                 isolate, options_obj, method_name));

  // Remaining steps handled in Rust.
  auto added = (temporal_js_type->wrapped_rust().*operation)(
      *other_duration->duration()->raw(), overflow, provider...);

  return ConstructRustWrappingType<JSType>(isolate, std::move(added));
}
// ====== .with() methods ======

// Helper for GenericWith: extract relevant options from options_obj, call
// .with() on Rust type.
//
// Steps 8 and later from
// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
template <typename JSType, typename PartialType>
MaybeDirectHandle<JSType> GenericWithHelper(
    Isolate* isolate, const typename JSType::RustType& rust_object,
    CombinedRecord& fields, DirectHandle<Object> options_obj,
    const char* method_name) {
  // 8. Let resolvedOptions be ? GetOptionsObject(options).
  // 9. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
  temporal_rs::ArithmeticOverflow overflow;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, overflow,
      ToTemporalOverflowHandleUndefined(isolate, options_obj, method_name));

  PartialType partial;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, partial,
                             fields.Regulate<PartialType>(isolate, overflow));
  // Rest handled by Rust.
  return ConstructRustWrappingType<JSType>(isolate,
                                           rust_object.with(partial, overflow));
}

// ZonedDateTime needs to extract extra options
//
// Steps 19 and later in
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
template <>
MaybeDirectHandle<JSTemporalZonedDateTime>
GenericWithHelper<JSTemporalZonedDateTime, temporal_rs::PartialZonedDateTime>(
    Isolate* isolate, const typename temporal_rs::ZonedDateTime& rust_object,
    CombinedRecord& fields, DirectHandle<Object> options_obj,
    const char* method_name) {
  // 19. Let resolvedOptions be ? GetOptionsObject(options).

  // (Not explicitly needed since we use HandleUndefined methods)

  temporal_rs::Disambiguation disambiguation;
  temporal_rs::OffsetDisambiguation offset_option;
  temporal_rs::ArithmeticOverflow overflow;

  // 20. Let disambiguation be
  // ? GetTemporalDisambiguationOption(resolvedOptions).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, disambiguation,
      temporal::GetTemporalDisambiguationOptionHandleUndefined(
          isolate, options_obj, method_name));
  // 21. Let offset be ? GetTemporalOffsetOption(resolvedOptions, prefer).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, offset_option,
      temporal::GetTemporalOffsetOptionHandleUndefined(
          isolate, options_obj, temporal_rs::OffsetDisambiguation::Prefer,
          method_name));
  // 22. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, overflow,
      ToTemporalOverflowHandleUndefined(isolate, options_obj, method_name));

  temporal_rs::PartialZonedDateTime partial = kNullPartialZonedDateTime;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, partial,
      fields.Regulate<temporal_rs::PartialZonedDateTime>(isolate, overflow));
  // Rest handled by Rust.
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate,
      rust_object.with_with_provider(partial, disambiguation, offset_option,
                                     overflow, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
//
// The function has comments for PlainDate, but the non-Rust-side code for the
// others is identical.
template <typename JSType, typename PartialType>
MaybeDirectHandle<JSType> GenericWith(Isolate* isolate,
                                      DirectHandle<JSType> this_obj,
                                      DirectHandle<Object> temporal_like_obj,
                                      DirectHandle<Object> options_obj,
                                      CalendarFieldsFlags flags,
                                      const char* method_name) {
  // 1. Let temporalDate be the this value.
  // 2. Perform ? RequireInternalSlot(temporalDate,
  // [[InitializedTemporalDate]]).

  // (handled by type system)

  // 3. If ? IsPartialTemporalObject(temporalDateLike) is false, throw a
  // TypeError exception.

  bool is_partial = false;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, is_partial, IsPartialTemporalObject(isolate, temporal_like_obj));
  if (!is_partial) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kWithNoPartial));
  }
  // IsPartialTemporalObject only returns true for valid JSReceivers.
  auto options_recvr = Cast<JSReceiver>(temporal_like_obj);

  auto& rust_object = this_obj->wrapped_rust();
  // 4. Let calendar be temporalDate.[[Calendar]].
  auto kind = rust_object.calendar().kind();

  // 5. Let fields be ISODateToFields(calendar, temporalDate.[[ISODate]], date).

  // (handled in Rust code, not observable)

  // 6. Let partialDate be ? PrepareCalendarFields(calendar, temporalDateLike, «
  // year, month, month-code, day », « », partial).

  CombinedRecord fields;

  MOVE_RETURN_ON_EXCEPTION(
      isolate, fields,
      PrepareCalendarFields(isolate, kind, options_recvr, flags,
                            RequiredFields::kPartial));

  // 7. Set fields to CalendarMergeFields(calendar, fields, partialDate).

  // (handled in Rust code, not observable)

  // Fetching options handled by GenericWithHelper.
  // Remaining steps handled by Rust code called by GenericWithHelper.

  return GenericWithHelper<JSType, PartialType>(isolate, rust_object, fields,
                                                options_obj, method_name);
}

// ====== Misc ======

V8_WARN_UNUSED_RESULT Maybe<temporal_rs::TimeZone> ToRustTimeZone(
    Isolate* isolate, std::string_view tz) {
  return ExtractRustResult(isolate,
                           temporal_rs::TimeZone::try_from_str_with_provider(
                               tz, TimeZoneProvider()));
}

// Partial implementation:
//
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldatetime
Maybe<int64_t> GetEpochMillisecondsForDateTime(Isolate* isolate,
                                               temporal_rs::PlainDateTime& date,
                                               std::string_view time_zone) {
  temporal_rs::TimeZone tz;
  MOVE_RETURN_ON_EXCEPTION(isolate, tz, ToRustTimeZone(isolate, time_zone));

  //  2. Let epochNs be ? GetEpochNanosecondsFor(dateTimeFormat.[[TimeZone]],
  //  isoDateTime, compatible).
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      ExtractRustResult(isolate,
                        date.to_zoned_date_time_with_provider(
                            tz, temporal_rs::Disambiguation::Compatible,
                            TimeZoneProvider())));
  return Just(zdt->epoch_milliseconds());
}

// Partial implementation:
//
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalmonthday
// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalyearmonth
Maybe<int64_t> GetEpochMillisecondsForDate(
    Isolate* isolate, temporal_rs::PlainDate& date, std::string_view time_zone,
    temporal_rs::PlainTime* time = nullptr) {
  temporal_rs::TimeZone tz;
  MOVE_RETURN_ON_EXCEPTION(isolate, tz, ToRustTimeZone(isolate, time_zone));

  // 2. Let isoDateTime be CombineISODateAndTimeRecord(temporalDate.[[ISODate]],
  // NoonTimeRecord()).
  // 3. Let epochNs be ? GetEpochNanosecondsFor(dateTimeFormat.[[TimeZone]],
  // isoDateTime, compatible).

  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      ExtractRustResult(isolate, date.to_zoned_date_time_with_provider(
                                     tz, time, TimeZoneProvider())));
  return Just(zdt->epoch_milliseconds());
}

}  // namespace temporal

// https://tc39.es/proposal-temporal/#sec-temporal.duration
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> years,
    DirectHandle<Object> months, DirectHandle<Object> weeks,
    DirectHandle<Object> days, DirectHandle<Object> hours,
    DirectHandle<Object> minutes, DirectHandle<Object> seconds,
    DirectHandle<Object> milliseconds, DirectHandle<Object> microseconds,
    DirectHandle<Object> nanoseconds) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.Duration")));
  }
  // 2. If years is undefined, let y be 0; else let y be
  // ? ToIntegerIfIntegral(years).
  int64_t y = 0;
  if (!IsUndefined(*years)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, y, temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, years));
  }

  // 3. If months is undefined, let mo be 0; else let mo be
  // ? ToIntegerIfIntegral(months).
  int64_t mo = 0;
  if (!IsUndefined(*months)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, mo,
        temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, months));
  }

  // 4. If weeks is undefined, let w be 0; else let w be
  // ? ToIntegerIfIntegral(weeks).
  int64_t w = 0;
  if (!IsUndefined(*weeks)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, w, temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, weeks));
  }

  // 5. If days is undefined, let d be 0; else let d be
  // ? ToIntegerIfIntegral(days).
  int64_t d = 0;
  if (!IsUndefined(*days)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, d, temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, days));
  }

  // 6. If hours is undefined, let h be 0; else let h be
  // ? ToIntegerIfIntegral(hours).
  int64_t h = 0;
  if (!IsUndefined(*hours)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, h, temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, hours));
  }

  // 7. If minutes is undefined, let m be 0; else let m be
  // ? ToIntegerIfIntegral(minutes).
  int64_t m = 0;
  if (!IsUndefined(*minutes)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, m,
        temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, minutes));
  }

  // 8. If seconds is undefined, let s be 0; else let s be
  // ? ToIntegerIfIntegral(seconds).
  int64_t s = 0;
  if (!IsUndefined(*seconds)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, s,
        temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, seconds));
  }

  // 9. If milliseconds is undefined, let ms be 0; else let ms be
  // ? ToIntegerIfIntegral(milliseconds).
  int64_t ms = 0;
  if (!IsUndefined(*milliseconds)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, ms,
        temporal::ToIntegerTypeIfIntegral<int64_t>(isolate, milliseconds));
  }

  // 10. If microseconds is undefined, let mis be 0; else let mis be
  // ? ToIntegerIfIntegral(microseconds).
  double mis = 0;
  if (!IsUndefined(*microseconds)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, mis, temporal::ToIntegerIfIntegral(isolate, microseconds));
  }

  // 11. If nanoseconds is undefined, let ns be 0; else let ns be
  // ? ToIntegerIfIntegral(nanoseconds).
  double ns = 0;
  if (!IsUndefined(*nanoseconds)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, ns, temporal::ToIntegerIfIntegral(isolate, nanoseconds));
  }

  // 12. Return ?CreateTemporalDuration(y, mo, w, d, h, m, s, ms, mis, ns,
  // NewTarget).
  return ConstructRustWrappingType<JSTemporalDuration>(
      isolate, target, new_target,
      temporal_rs::Duration::create(y, mo, w, d, h, m, s, ms, mis, ns));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.compare
MaybeDirectHandle<Smi> JSTemporalDuration::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj, DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.Duration.compare";
  DirectHandle<JSTemporalDuration> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one,
      temporal::ToTemporalDuration(isolate, one_obj, method_name));
  DirectHandle<JSTemporalDuration> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two,
      temporal::ToTemporalDuration(isolate, two_obj, method_name));

  temporal::RelativeTo relative_to;

  MOVE_RETURN_ON_EXCEPTION(isolate, relative_to,
                           temporal::GetTemporalRelativeToOptionHandleUndefined(
                               isolate, options_obj));

  int8_t comparison = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, comparison,
      ExtractRustResult(isolate,
                        one->duration()->raw()->compare_with_provider(
                            *two->duration()->raw(), relative_to.ToRust(),
                            TimeZoneProvider())));

  return direct_handle(Smi::FromInt(comparison), isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.from
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::From(
    Isolate* isolate, DirectHandle<Object> item) {
  static const char method_name[] = "Temporal.Duration.from";

  return temporal::ToTemporalDuration(isolate, item, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.round
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Round(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> round_to_obj) {
  static const char method_name[] = "Temporal.Duration.prototype.round";
  // 1. Let duration be the this value.
  // 2. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).

  // (handled by type system)
  // 3. If roundTo is undefined, then
  if (IsUndefined(*round_to_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMissing));
  }

  DirectHandle<JSReceiver> round_to;
  auto factory = isolate->factory();

  // 4. If roundTo is a String, then
  if (IsString(*round_to_obj)) {
    // a. Let paramString be roundTo.
    DirectHandle<String> param_string = Cast<String>(round_to_obj);
    // b. Set roundTo to ! OrdinaryObjectCreate(null).
    round_to = factory->NewJSObjectWithNullProto();
    // TODO(manishearth) Ideally we don't have to perform this allocation
    // c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit",
    // paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, round_to,
                                         factory->smallestUnit_string(),
                                         param_string, Just(kThrowOnError))
              .FromJust());
    // 5. Else,
  } else {
    // a. Set roundTo to ? GetOptionsObject(roundTo).
    // We have already checked for undefined, we can hoist the JSReceiver
    // check out and just cast
    if (!IsJSReceiver(*round_to_obj)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMustBeObject));
    }
    round_to = Cast<JSReceiver>(round_to_obj);
  }

  // 6. Let smallestUnitPresent be true.
  // 7. Let largestUnitPresent be true.

  // (handled by Rust)

  // 8. NOTE: (...)

  // 9. Let largestUnit be ? GetTemporalUnitValuedOption(roundTo, "largestUnit",
  // unset).
  std::optional<Unit> largest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, largest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, round_to, isolate->factory()->largestUnit_string(),
          DefaultValue::kUnset, method_name));

  // 10. Let relativeToRecord be ? GetTemporalRelativeToOption(roundTo).
  // 11. Let zonedRelativeTo be relativeToRecord.[[ZonedRelativeTo]].
  // 12. Let plainRelativeTo be relativeToRecord.[[PlainRelativeTo]].

  temporal::RelativeTo relative_to;

  MOVE_RETURN_ON_EXCEPTION(
      isolate, relative_to,
      temporal::GetTemporalRelativeToOptionHandleUndefined(isolate, round_to));

  // 13. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).

  uint32_t rounding_increment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, round_to));

  // 14. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, round_to,
                                      RoundingMode::HalfExpand, method_name));

  // 15. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo,
  // "smallestUnit", unset).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, round_to, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));
  // 16. Perform ? ValidateTemporalUnitValue(smallestUnit, datetime).
  RETURN_ON_EXCEPTION(
      isolate, temporal::ValidateTemporalUnitValue(isolate, smallest_unit,
                                                   UnitGroup::kDateTime));

  // Rest of the steps handled in Rust

  auto options = temporal_rs::RoundingOptions{.largest_unit = largest_unit,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment};

  auto rounded = duration->duration()->raw()->round_with_provider(
      options, relative_to.ToRust(), TimeZoneProvider());
  return ConstructRustWrappingType<JSTemporalDuration>(isolate,
                                                       std::move(rounded));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.total
// #sec-temporal.duration.prototype.total
MaybeDirectHandle<Number> JSTemporalDuration::Total(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> total_of_obj) {
  static const char method_name[] = "Temporal.Duration.prototype.total";
  // 1. Let duration be the this value.
  // 2. Perform ? RequireInternalSlot(duration,
  // [[InitializedTemporalDuration]]).

  // (handled by type system)

  // 3. If totalOf is undefined, throw a TypeError exception.
  if (IsUndefined(*total_of_obj)) {
    THROW_NEW_ERROR(
        isolate, NEW_TEMPORAL_TYPE_ERROR("Must specify a totalOf parameter"));
  }

  DirectHandle<JSReceiver> total_of;
  auto factory = isolate->factory();

  // 4. If totalOf is a String, then
  if (IsString(*total_of_obj)) {
    // a. Let paramString be totalOf.
    DirectHandle<String> param_string = Cast<String>(total_of_obj);
    // b. Set totalOf to ! OrdinaryObjectCreate(null).
    total_of = factory->NewJSObjectWithNullProto();
    // c. Perform ! CreateDataPropertyOrThrow(total_of, "unit", paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, total_of,
                                         factory->unit_string(), param_string,
                                         Just(kThrowOnError))
              .FromJust());
    // 5. Else,
  } else {
    // a. Set totalOf to ? GetOptionsObject(totalOf).
    // We have already checked for undefined, we can hoist the JSReceiver
    // check out and just cast
    if (!IsJSReceiver(*total_of_obj)) {
      THROW_NEW_ERROR(isolate,
                      NEW_TEMPORAL_TYPE_ERROR("totalOf must be an object."));
    }
    total_of = Cast<JSReceiver>(total_of_obj);
  }

  // 6. NOTE (...)

  // 7. Let relativeToRecord be ? GetTemporalRelativeToOption(totalOf).
  // 8. Let zonedRelativeTo be relativeToRecord.[[ZonedRelativeTo]].
  // 9. Let plainRelativeTo be relativeToRecord.[[PlainRelativeTo]].

  temporal::RelativeTo relative_to;

  MOVE_RETURN_ON_EXCEPTION(
      isolate, relative_to,
      temporal::GetTemporalRelativeToOptionHandleUndefined(isolate, total_of));

  // 10. Let unit be ? GetTemporalUnitValuedOption(totalOf, "unit", required).
  std::optional<Unit> unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, total_of, isolate->factory()->unit_string(),
          DefaultValue::kRequired, method_name));
  // 11. Perform ? ValidateTemporalUnitValue(unit, datetime).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, unit, UnitGroup::kDateTime));

  // We set required to true.
  DCHECK(unit.has_value());

  // Remaining steps handled in Rust
  double ret;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ret,
      ExtractRustResult(
          isolate,
          duration->duration()->raw()->total_with_provider(
              unit.value(), relative_to.ToRust(), TimeZoneProvider())));

  return factory->NewNumber(ret);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.with
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::With(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> temporal_duration_like) {
  temporal_rs::PartialDuration partial;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, partial,
                             temporal::ToTemporalPartialDurationRecord(
                                 isolate, temporal_duration_like));
  if (!partial.years.has_value()) {
    partial.years = duration->duration()->raw()->years();
  }
  if (!partial.months.has_value()) {
    partial.months = duration->duration()->raw()->months();
  }
  if (!partial.months.has_value()) {
    partial.months = duration->duration()->raw()->months();
  }
  if (!partial.weeks.has_value()) {
    partial.weeks = duration->duration()->raw()->weeks();
  }
  if (!partial.days.has_value()) {
    partial.days = duration->duration()->raw()->days();
  }
  if (!partial.hours.has_value()) {
    partial.hours = duration->duration()->raw()->hours();
  }
  if (!partial.minutes.has_value()) {
    partial.minutes = duration->duration()->raw()->minutes();
  }
  if (!partial.seconds.has_value()) {
    partial.seconds = duration->duration()->raw()->seconds();
  }
  if (!partial.milliseconds.has_value()) {
    partial.milliseconds = duration->duration()->raw()->milliseconds();
  }
  if (!partial.microseconds.has_value()) {
    partial.microseconds = duration->duration()->raw()->microseconds();
  }
  if (!partial.nanoseconds.has_value()) {
    partial.nanoseconds = duration->duration()->raw()->nanoseconds();
  }
  return ConstructRustWrappingType<JSTemporalDuration>(
      isolate, temporal_rs::Duration::from_partial_duration(partial));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.sign
MaybeDirectHandle<Smi> JSTemporalDuration::Sign(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  auto sign = duration->duration()->raw()->sign();
  return direct_handle(Smi::FromInt((temporal_rs::Sign::Value)sign), isolate);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.duration.prototype.blank
MaybeDirectHandle<Oddball> JSTemporalDuration::Blank(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  return isolate->factory()->ToBoolean(duration->duration()->raw()->sign() ==
                                       temporal_rs::Sign::Zero);
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.negated
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Negated(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  return ConstructRustWrappingType<JSTemporalDuration>(
      isolate, duration->duration()->raw()->negated());
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.abs
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Abs(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  return ConstructRustWrappingType<JSTemporalDuration>(
      isolate, duration->duration()->raw()->abs());
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.add
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Add(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.Duration.prototype.add";

  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other_duration,
      temporal::ToTemporalDuration(isolate, other, method_name));

  auto result =
      duration->duration()->raw()->add(*other_duration->duration()->raw());
  return ConstructRustWrappingType<JSTemporalDuration>(isolate,
                                                       std::move(result));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.subtract
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.Duration.prototype.subtract";

  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other_duration,
      temporal::ToTemporalDuration(isolate, other, method_name));

  auto result =
      duration->duration()->raw()->subtract(*other_duration->duration()->raw());
  return ConstructRustWrappingType<JSTemporalDuration>(isolate,
                                                       std::move(result));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tostring
MaybeDirectHandle<String> JSTemporalDuration::ToString(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.Duration.prototype.toString";

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 5. Let digits be ?GetTemporalFractionalSecondDigitsOption(resolvedOptions).

  temporal_rs::Precision digits;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, digits,
                             temporal::GetTemporalFractionalSecondDigitsOption(
                                 isolate, options, method_name));

  // 6. Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).

  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, options, RoundingMode::Trunc,
                                      method_name));

  // 7. Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions,
  // "smallestUnit", unset).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));

  // 8. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  // 8-17 performed by Rust
  auto rust_options = temporal_rs::ToStringRoundingOptions{
      .precision = digits,
      .smallest_unit = smallest_unit,
      .rounding_mode = rounding_mode,
  };

  return temporal::GenericTemporalToString(isolate, duration,
                                           &temporal_rs::Duration::to_string,
                                           std::move(rust_options));
}

// https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tojson
MaybeDirectHandle<String> JSTemporalDuration::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  return temporal::GenericTemporalToString(isolate, duration,
                                           &temporal_rs::Duration::to_string,
                                           std::move(temporal::kToStringAuto));
}

MaybeDirectHandle<String> JSTemporalDuration::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sup-temporal.duration.prototype.tolocalestring
  return JSDurationFormat::TemporalToLocaleString(isolate, duration, locales,
                                                  options);
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.duration.prototype.tolocalestring
  return temporal::GenericTemporalToString(isolate, duration,
                                           &temporal_rs::Duration::to_string,
                                           std::move(temporal::kToStringAuto));
#endif  // V8_INTL_SUPPORT
}

MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_year_obj,
    DirectHandle<Object> iso_month_obj, DirectHandle<Object> iso_day_obj,
    DirectHandle<Object> calendar_like) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.PlainDate")));
  }
  // 2. Let y be ? ToIntegerWithTruncation(isoYear).
  double y = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, y, temporal::ToIntegerWithTruncation(isolate, iso_year_obj));
  // 3. Let m be ? ToIntegerWithTruncation(isoMonth).
  double m = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, m, temporal::ToIntegerWithTruncation(isolate, iso_month_obj));
  // 4. Let d be ? ToIntegerWithTruncation(isoDay).
  double d = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, d, temporal::ToIntegerWithTruncation(isolate, iso_day_obj));

  // 5. If calendar is undefined, set calendar to "iso8601".
  temporal_rs::AnyCalendarKind calendar = temporal_rs::AnyCalendarKind::Iso;

  if (!IsUndefined(*calendar_like)) {
    // 6. If calendar is not a String, throw a TypeError exception.
    if (!IsString(*calendar_like)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kCalendarMustBeString));
    }

    // 7. Set calendar to ?CanonicalizeCalendar(calendar).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        temporal::CanonicalizeCalendar(isolate, Cast<String>(calendar_like)));
  }
  // 8. If IsValidISODate(y, m, d) is false, throw a RangeError exception.
  if (!temporal::IsValidIsoDate(y, m, d)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kInvalidIsoDate));
  }

  // Rest of the steps handled in Rust

  // These static_casts are fine to perform since IsValid* will have constrained
  // these values to range already.
  // https://github.com/boa-dev/temporal/issues/334 for moving this logic into
  // the Rust code.
  auto rust_object = temporal_rs::PlainDate::try_new(
      static_cast<int32_t>(y), static_cast<uint8_t>(m), static_cast<uint8_t>(d),
      calendar);
  return ConstructRustWrappingType<JSTemporalPlainDate>(
      isolate, target, new_target, std::move(rust_object));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.compare
MaybeDirectHandle<Smi> JSTemporalPlainDate::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  static const char method_name[] = "Temporal.PlainDate.compare";
  DirectHandle<JSTemporalPlainDate> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one,
      temporal::ToTemporalDate(isolate, one_obj, {}, method_name));
  DirectHandle<JSTemporalPlainDate> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two,
      temporal::ToTemporalDate(isolate, two_obj, {}, method_name));

  return direct_handle(Smi::FromInt(temporal_rs::PlainDate::compare(
                           *one->date()->raw(), *two->date()->raw())),
                       isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainDate::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.PlainDate.prototype.equals";

  DirectHandle<JSTemporalPlainDate> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalDate(isolate, other_obj, {}, method_name));

  auto equals = temporal_date->date()->raw()->equals(*other->date()->raw());

  return isolate->factory()->ToBoolean(equals);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainyearmonth
MaybeDirectHandle<JSTemporalPlainYearMonth>
JSTemporalPlainDate::ToPlainYearMonth(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainYearMonth>(
      isolate, temporal_date, &temporal_rs::PlainDate::to_plain_year_month);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplainmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainDate::ToPlainMonthDay(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainMonthDay>(
      isolate, temporal_date, &temporal_rs::PlainDate::to_plain_month_day);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.toplaindatetime
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDate::ToPlainDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_time_obj) {
  static const char method_name[] = "Temporal.PlainDate.toPlainDateTime";

  // 3. Let time be ? ToTimeRecordOrMidnight(temporalTime).
  const temporal_rs::PlainTime* maybe_time;
  DirectHandle<JSTemporalPlainTime> time_output;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, maybe_time,
      temporal::ToTimeRecordOrMidnight(isolate, temporal_time_obj, time_output,
                                       method_name));

  // Rest of the steps handled in Rust.
  return ConstructRustWrappingType<JSTemporalPlainDateTime>(
      isolate, temporal_date->date()->raw()->to_plain_date_time(maybe_time));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.with
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_date_like_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDate.prototype.with";
  return temporal::GenericWith<JSTemporalPlainDate, temporal_rs::PartialDate>(
      isolate, temporal_date, temporal_date_like_obj, options_obj,
      temporal::kAllDateFlags, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.withcalendar
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::WithCalendar(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> calendar_id) {
  // 3. Let calendar be ? ToTemporalCalendarIdentifier(calendarLike).
  temporal_rs::AnyCalendarKind calendar_kind;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar_kind,
      temporal::ToTemporalCalendarIdentifier(isolate, calendar_id));

  // Rest of the steps handled in Rust
  return ConstructRustWrappingType<JSTemporalPlainDate>(
      isolate, temporal_date->date()->raw()->with_calendar(calendar_kind));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalPlainDate::ToZonedDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> item_obj) {
  static const char method_name[] = "Temporal.PlainDate.toZonedDateTime";
  // 1. Let temporalDate be the this value.
  // 2. Perform ? RequireInternalSlot(temporalDate,
  // [[InitializedTemporalDate]]).

  temporal_rs::TimeZone time_zone;
  DirectHandle<Object> temporal_time_obj;

  // 3. If item is an Object, then
  if (IsJSReceiver(*item_obj)) {
    DirectHandle<JSReceiver> item = Cast<JSReceiver>(item_obj);
    // a. Let timeZoneLike be ? Get(item, "timeZone").
    DirectHandle<Object> time_zone_like;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, time_zone_like,
        JSReceiver::GetProperty(isolate, item,
                                isolate->factory()->timeZone_string()));
    // b. If timeZoneLike is undefined, then
    if (IsUndefined(*time_zone_like)) {
      // i. Let timeZone be ? ToTemporalTimeZoneIdentifier(item).
      MOVE_RETURN_ON_EXCEPTION(
          isolate, time_zone,
          temporal::ToTemporalTimeZoneIdentifier(isolate, item));
      // ii. Let temporalTime be undefined.

      // c. Else,
    } else {
      // i. Let timeZone be ? ToTemporalTimeZoneIdentifier(timeZoneLike).
      MOVE_RETURN_ON_EXCEPTION(
          isolate, time_zone,
          temporal::ToTemporalTimeZoneIdentifier(isolate, time_zone_like));
      // ii. Let temporalTime be ? Get(item, "plainTime").
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, temporal_time_obj,
          JSReceiver::GetProperty(isolate, item,
                                  isolate->factory()->plainTime_string()));
    }
    // 4. Else,
  } else {
    // a. Let timeZone be ? ToTemporalTimeZoneIdentifier(item).
    MOVE_RETURN_ON_EXCEPTION(
        isolate, time_zone,
        temporal::ToTemporalTimeZoneIdentifier(isolate, item_obj));
    // b. Let temporalTime be undefined.
  }

  DirectHandle<JSTemporalPlainTime> temporal_time;
  temporal_rs::PlainTime* temporal_time_rust = nullptr;

  // 5. If temporalTime is undefined, then
  if (temporal_time_obj.is_null() || IsUndefined(*temporal_time_obj)) {
    // a. Let epochNs be ? GetStartOfDay(timeZone, temporalDate.[[ISODate]]).
    // (handled in Rust by temporal_time_rust being a null pointer)

    // 6. Else,
  } else {
    // a. Set temporalTime to ? ToTemporalTime(temporalTime).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, temporal_time,
        temporal::ToTemporalTime(isolate, temporal_time_obj, {}, method_name));
    temporal_time_rust = temporal_time->time()->raw();
    // (Rest of the steps handled in Rust)
  }

  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate, temporal_date->date()->raw()->to_zoned_date_time_with_provider(
                   time_zone, temporal_time_rust, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.add
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_duration_like,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDate.prototype.add";
  // 3. Return ? AddDurationToDate(add, temporalDate, temporalDurationLike,
  // options).
  return temporal::AddDurationToGeneric(isolate, &temporal_rs::PlainDate::add,
                                        temporal_date, temporal_duration_like,
                                        options_obj, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.subtract
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_duration_like,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDate.prototype.subtract";
  // 3. Return ? AddDurationToDate(subtract, temporalDate, temporalDurationLike,
  // options).
  return temporal::AddDurationToGeneric<JSTemporalPlainDate>(
      isolate, &temporal_rs::PlainDate::subtract, temporal_date,
      temporal_duration_like, options_obj, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDate::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainDate.prototype.until";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainDate::until, UnitGroup::kDate, Unit::Day,
      handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDate::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainDate.prototype.since";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainDate::since, UnitGroup::kDate, Unit::Day,
      handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindateiso
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      temporal::GenericTemporalNowISO(isolate, temporal_time_zone_like));
  return ConstructRustWrappingType<JSTemporalPlainDate>(isolate,
                                                        zdt->to_plain_date());
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.from
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDate.from";
  DirectHandle<JSTemporalPlainDate> item;

  return temporal::ToTemporalDate(isolate, item_obj, options_obj, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainDate::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDate.prototype.toString";

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 4. Let showCalendar be ?GetTemporalShowCalendarNameOption(resolvedOptions).
  temporal_rs::DisplayCalendar show_calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, show_calendar,
                             temporal::GetTemporalShowCalendarNameOption(
                                 isolate, options, method_name));

  // 5. Return TemporalDateToString(temporalDate, showCalendar).
  return temporal::GenericTemporalToString(
      isolate, temporal_date, &temporal_rs::PlainDate::to_ixdtf_string,
      show_calendar);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainDate::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  return temporal::GenericTemporalToString(
      isolate, temporal_date, &temporal_rs::PlainDate::to_ixdtf_string,
      temporal_rs::DisplayCalendar::Auto);
}

MaybeDirectHandle<String> JSTemporalPlainDate::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sup-temporal.plaindate.prototype.tolocalestring
  return JSDateTimeFormat::TemporalToLocaleString(
      isolate, temporal_date, locales, options,
      JSDateTimeFormat::RequiredOption::kDate,
      JSDateTimeFormat::DefaultsOption::kDate,
      "Temporal.PlainDate.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.plaindate.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, temporal_date, &temporal_rs::PlainDate::to_ixdtf_string,
      temporal_rs::DisplayCalendar::Auto);
#endif  // V8_INTL_SUPPORT
}

Maybe<int64_t> JSTemporalPlainDate::GetEpochMillisecondsFor(
    Isolate* isolate, std::string_view time_zone) {
  return temporal::GetEpochMillisecondsForDate(isolate, *this->date()->raw(),
                                               time_zone);
}

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporaldatetime
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_year_obj,
    DirectHandle<Object> iso_month_obj, DirectHandle<Object> iso_day_obj,
    DirectHandle<Object> hour_obj, DirectHandle<Object> minute_obj,
    DirectHandle<Object> second_obj, DirectHandle<Object> millisecond_obj,
    DirectHandle<Object> microsecond_obj, DirectHandle<Object> nanosecond_obj,
    DirectHandle<Object> calendar_like) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.PlainDateTime")));
  }
  // 2. Set isoYear to ?ToIntegerWithTruncation(isoYear).
  double y = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, y, temporal::ToIntegerWithTruncation(isolate, iso_year_obj));
  // 3. Set isoMonth to ?ToIntegerWithTruncation(isoMonth).
  double m = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, m, temporal::ToIntegerWithTruncation(isolate, iso_month_obj));
  // 4. Set isoDay to ?ToIntegerWithTruncation(isoDay).
  double d = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, d, temporal::ToIntegerWithTruncation(isolate, iso_day_obj));

  // 5. If hour is undefined, set hour to 0; else set hour to ?
  // ToIntegerWithTruncation(hour).
  double hour = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, hour,
      temporal::ToIntegerWithTruncationOrZero(isolate, hour_obj));
  // 6. If minute is undefined, set minute to 0; else set minute to ?
  // ToIntegerWithTruncation(minute).
  double minute = 0;
  if (!IsUndefined(*minute_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, minute,
        temporal::ToIntegerWithTruncationOrZero(isolate, minute_obj));
  }
  // 7. If second is undefined, set second to 0; else set second to ?
  // ToIntegerWithTruncation(second).
  double second = 0;
  if (!IsUndefined(*second_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, second,
        temporal::ToIntegerWithTruncationOrZero(isolate, second_obj));
  }
  // 8. If millisecond is undefined, set millisecond to 0; else set millisecond
  // to ? ToIntegerWithTruncation(millisecond).
  double millisecond = 0;
  if (!IsUndefined(*millisecond_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, millisecond,
        temporal::ToIntegerWithTruncationOrZero(isolate, millisecond_obj));
  }
  // 9. If microsecond is undefined, set microsecond to 0; else set microsecond
  // to ? ToIntegerWithTruncation(microsecond).

  double microsecond = 0;
  if (!IsUndefined(*microsecond_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, microsecond,
        temporal::ToIntegerWithTruncationOrZero(isolate, microsecond_obj));
  }
  // 10. If nanosecond is undefined, set nanosecond to 0; else set nanosecond to
  // ? ToIntegerWithTruncation(nanosecond).
  double nanosecond = 0;
  if (!IsUndefined(*nanosecond_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, nanosecond,
        temporal::ToIntegerWithTruncationOrZero(isolate, nanosecond_obj));
  }

  // 11. If calendar is undefined, set calendar to "iso8601".
  temporal_rs::AnyCalendarKind calendar = temporal_rs::AnyCalendarKind::Iso;

  if (!IsUndefined(*calendar_like)) {
    // 12. If calendar is not a String, throw a TypeError exception.
    if (!IsString(*calendar_like)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kCalendarMustBeString));
    }

    // 13. Set calendar to ?CanonicalizeCalendar(calendar).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        temporal::CanonicalizeCalendar(isolate, Cast<String>(calendar_like)));
  }
  // 14. If IsValidISODate(isoYear, isoMonth, isoDay) is false, throw a
  // RangeError exception.
  if (!temporal::IsValidIsoDate(y, m, d)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kInvalidIsoDate));
  }
  // 16. If IsValidTime(hour, minute, second, millisecond, microsecond,
  // nanosecond) is false, throw a RangeError exception.
  if (!temporal::IsValidTime(hour, minute, second, millisecond, microsecond,
                             nanosecond)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kInvalidTime));
  }

  // Rest of the steps handled in Rust.

  // These static_casts are fine to perform since IsValid* will have constrained
  // these values to range already.
  // https://github.com/boa-dev/temporal/issues/334 for moving this logic into
  // the Rust code.
  auto rust_object = temporal_rs::PlainDateTime::try_new(
      static_cast<int32_t>(y), static_cast<uint8_t>(m), static_cast<uint8_t>(d),
      static_cast<uint8_t>(hour), static_cast<uint8_t>(minute),
      static_cast<uint8_t>(second), static_cast<uint16_t>(millisecond),
      static_cast<uint16_t>(microsecond), static_cast<uint16_t>(nanosecond),
      calendar);
  return ConstructRustWrappingType<JSTemporalPlainDateTime>(
      isolate, target, new_target, std::move(rust_object));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.from
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.from";
  DirectHandle<JSTemporalPlainDateTime> item;

  return temporal::ToTemporalDateTime(isolate, item_obj, options_obj,
                                      method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.compare
MaybeDirectHandle<Smi> JSTemporalPlainDateTime::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.compare";
  DirectHandle<JSTemporalPlainDateTime> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one,
      temporal::ToTemporalDateTime(isolate, one_obj, {}, method_name));
  DirectHandle<JSTemporalPlainDateTime> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two,
      temporal::ToTemporalDateTime(isolate, two_obj, {}, method_name));

  return direct_handle(Smi::FromInt(temporal_rs::PlainDateTime::compare(
                           *one->date_time()->raw(), *two->date_time()->raw())),
                       isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainDateTime::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.equals";

  DirectHandle<JSTemporalPlainDateTime> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalDateTime(isolate, other_obj, {}, method_name));

  auto equals =
      date_time->date_time()->raw()->equals(*other->date_time()->raw());

  return isolate->factory()->ToBoolean(equals);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.with
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_date_time_like_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.with";

  using enum temporal::CalendarFieldsFlag;

  return temporal::GenericWith<JSTemporalPlainDateTime,
                               temporal_rs::PartialDateTime>(
      isolate, date_time, temporal_date_time_like_obj, options_obj,
      temporal::kAllDateFlags | kTimeFields, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withcalendar
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalPlainDateTime::WithCalendar(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> temporal_date,
    DirectHandle<Object> calendar_id) {
  // 3. Let calendar be ? ToTemporalCalendarIdentifier(calendarLike).
  temporal_rs::AnyCalendarKind calendar_kind;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar_kind,
      temporal::ToTemporalCalendarIdentifier(isolate, calendar_id));

  // Rest of the steps handled in Rust
  return ConstructRustWrappingType<JSTemporalPlainDateTime>(
      isolate, temporal_date->date_time()->raw()->with_calendar(calendar_kind));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.withplaintime
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalPlainDateTime::WithPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> plain_time_like) {
  static const char method_name[] =
      "Temporal.PlainDateTime.prototype.withPlainTime";

  // 3. Let time be ? ToTimeRecordOrMidnight(plainTimeLike).
  const temporal_rs::PlainTime* maybe_time;
  DirectHandle<JSTemporalPlainTime> time_output;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, maybe_time,
      temporal::ToTimeRecordOrMidnight(isolate, plain_time_like, time_output,
                                       method_name));

  // Rest of the steps handled in Rust.
  return ConstructRustWrappingType<JSTemporalPlainDateTime>(
      isolate, date_time->date_time()->raw()->with_time(maybe_time));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalPlainDateTime::ToZonedDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_time_zone_like,
    DirectHandle<Object> options_obj) {
  static const char method_name[] =
      "Temporal.PlainDateTime.prototype.toZonedDateTime";
  // 3. Let timeZone be ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
  temporal_rs::TimeZone time_zone;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      temporal::ToTemporalTimeZoneIdentifier(isolate, temporal_time_zone_like));
  // 4. Let resolvedOptions be ? GetOptionsObject(options).
  // 5. Let disambiguation be ?
  // GetTemporalDisambiguationOption(resolvedOptions).

  temporal_rs::Disambiguation disambiguation;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, disambiguation,
      temporal::GetTemporalDisambiguationOptionHandleUndefined(
          isolate, options_obj, method_name));

  // Rest of the steps handled in Rust.

  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate, date_time->date_time()->raw()->to_zoned_date_time_with_provider(
                   time_zone, disambiguation, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainDateTime::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.DateTime.prototype.toString";

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 5. Let showCalendar be ?GetTemporalShowCalendarNameOption(resolvedOptions).
  temporal_rs::DisplayCalendar show_calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, show_calendar,
                             temporal::GetTemporalShowCalendarNameOption(
                                 isolate, options, method_name));

  // 6. Let digits be ?GetTemporalFractionalSecondDigitsOption(resolvedOptions).
  temporal_rs::Precision digits;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, digits,
                             temporal::GetTemporalFractionalSecondDigitsOption(
                                 isolate, options, method_name));

  // 7. Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, options, RoundingMode::Trunc,
                                      method_name));

  // 8. Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions,
  // "smallestUnit", unset).

  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));
  // 9. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  // Rest of the steps handled in Rust
  auto rust_options = temporal_rs::ToStringRoundingOptions{
      .precision = digits,
      .smallest_unit = smallest_unit,
      .rounding_mode = rounding_mode,
  };
  return temporal::GenericTemporalToString(
      isolate, date_time, &temporal_rs::PlainDateTime::to_ixdtf_string,
      std::move(rust_options), show_calendar);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainDateTime::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  return temporal::GenericTemporalToString(
      isolate, date_time, &temporal_rs::PlainDateTime::to_ixdtf_string,
      std::move(temporal::kToStringAuto), temporal_rs::DisplayCalendar::Auto);
}

MaybeDirectHandle<String> JSTemporalPlainDateTime::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sup-temporal.plaindatetime.prototype.tolocalestring
  return JSDateTimeFormat::TemporalToLocaleString(
      isolate, date_time, locales, options,
      JSDateTimeFormat::RequiredOption::kAny,
      JSDateTimeFormat::DefaultsOption::kAll,
      "Temporal.PlainDateTime.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, date_time, &temporal_rs::PlainDateTime::to_ixdtf_string,
      std::move(temporal::kToStringAuto), temporal_rs::DisplayCalendar::Auto);
#endif  //  V8_INTL_SUPPORT
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaindatetimeiso
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      temporal::GenericTemporalNowISO(isolate, temporal_time_zone_like));
  return ConstructRustWrappingType<JSTemporalPlainDateTime>(
      isolate, zdt->to_plain_datetime());
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.round
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Round(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> round_to_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.round";
  // 1. Let dateTime be the this value.
  // 2. Perform ? RequireInternalSlot(dateTime,
  // [[InitializedTemporalDateTime]]).

  // (handled by type system)

  // 3. If roundTo is undefined, then
  if (IsUndefined(*round_to_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMissing));
  }

  DirectHandle<JSReceiver> round_to;
  auto factory = isolate->factory();

  // 4. If roundTo is a String, then
  if (IsString(*round_to_obj)) {
    // a. Let paramString be roundTo.
    DirectHandle<String> param_string = Cast<String>(round_to_obj);
    // b. Set roundTo to ! OrdinaryObjectCreate(null).
    round_to = factory->NewJSObjectWithNullProto();
    // c. c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit",
    // paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, round_to,
                                         factory->smallestUnit_string(),
                                         param_string, Just(kThrowOnError))
              .FromJust());
    // 5. Else,
  } else {
    // a. Set roundTo to ? GetOptionsObject(roundTo).
    // We have already checked for undefined, we can hoist the JSReceiver
    // check out and just cast
    if (!IsJSReceiver(*round_to_obj)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMustBeObject));
    }
    round_to = Cast<JSReceiver>(round_to_obj);
  }

  // 6. NOTE: (...)

  // 7. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).

  uint32_t rounding_increment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, round_to));

  // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, round_to,
                                      RoundingMode::HalfExpand, method_name));

  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo,
  // "smallestUnit", required).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, round_to, isolate->factory()->smallestUnit_string(),
          DefaultValue::kRequired, method_name));
  // 10. Perform ? ValidateTemporalUnitValue(smallestUnit, time, « day »).
  RETURN_ON_EXCEPTION(isolate,
                      temporal::ValidateTemporalUnitValue(
                          isolate, smallest_unit, UnitGroup::kTime, Unit::Day));

  // Rest of the steps handled in Rust

  auto options = temporal_rs::RoundingOptions{.largest_unit = std::nullopt,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment};

  auto rounded = date_time->date_time()->raw()->round(options);
  return ConstructRustWrappingType<JSTemporalPlainDateTime>(isolate,
                                                            std::move(rounded));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.add
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.add";
  // 3. Return ? AddDurationToDateTime(add, temporalDate, temporalDurationLike,
  // options).
  return temporal::AddDurationToGeneric(
      isolate, &temporal_rs::PlainDateTime::add, date_time,
      temporal_duration_like, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.subtract
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.subtract";
  // 3. Return ? AddDurationToDateTime(subtract, temporalDate,
  // temporalDurationLike, options).
  return temporal::AddDurationToGeneric(
      isolate, &temporal_rs::PlainDateTime::subtract, date_time,
      temporal_duration_like, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDateTime::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.until";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainDateTime::until, UnitGroup::kDateTime,
      Unit::Nanosecond, handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDateTime::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.since";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainDateTime::since, UnitGroup::kDateTime,
      Unit::Nanosecond, handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDateTime::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainDate>(
      isolate, date_time, &temporal_rs::PlainDateTime::to_plain_date);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaindatetime.prototype.toplaintime
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainDateTime::ToPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainTime>(
      isolate, date_time, &temporal_rs::PlainDateTime::to_plain_time);
}

// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
V8_WARN_UNUSED_RESULT Maybe<int64_t>
JSTemporalPlainDateTime::GetEpochMillisecondsFor(Isolate* isolate,
                                                 std::string_view time_zone) {
  return temporal::GetEpochMillisecondsForDateTime(isolate, *date_time()->raw(),
                                                   time_zone);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_month_obj,
    DirectHandle<Object> iso_day_obj, DirectHandle<Object> calendar_like,
    DirectHandle<Object> reference_iso_year_obj) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.PlainYearMonth")));
  }

  // 2. If referenceISOYear is undefined, then
  // a. Set referenceISOYear to 1𝔽.
  double reference_iso_year = 1972.0;

  // 3. Let m be ? ToIntegerWithTruncation(isoMonth).
  double m = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, m, temporal::ToIntegerWithTruncation(isolate, iso_month_obj));
  // 4. Let d be ? ToIntegerWithTruncation(isoYear).
  double d = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, d, temporal::ToIntegerWithTruncation(isolate, iso_day_obj));

  // 5. If calendar is undefined, set calendar to "iso8601".
  temporal_rs::AnyCalendarKind calendar = temporal_rs::AnyCalendarKind::Iso;

  if (!IsUndefined(*calendar_like)) {
    // 6. If calendar is not a String, throw a TypeError exception.
    if (!IsString(*calendar_like)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kCalendarMustBeString));
    }

    // 7. Set calendar to ?CanonicalizeCalendar(calendar).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        temporal::CanonicalizeCalendar(isolate, Cast<String>(calendar_like)));
  }

  if (!IsUndefined(*reference_iso_year_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, reference_iso_year,
        temporal::ToIntegerWithTruncation(isolate, reference_iso_year_obj));
  }

  // 9. If IsValidISODate(ref, m, d) is false, throw a RangeError exception.
  if (!temporal::IsValidIsoDate(reference_iso_year, m, d)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kInvalidIsoDate));
  }
  // Rest of the steps handled in Rust.

  // These static_casts are fine to perform since IsValid* will have constrained
  // these values to range already.
  // https://github.com/boa-dev/temporal/issues/334 for moving this logic into
  // the Rust code.
  auto rust_object = temporal_rs::PlainMonthDay::try_new_with_overflow(
      static_cast<uint8_t>(m), static_cast<uint8_t>(d), calendar,
      temporal_rs::ArithmeticOverflow::Reject,
      static_cast<int32_t>(reference_iso_year));
  return ConstructRustWrappingType<JSTemporalPlainMonthDay>(
      isolate, target, new_target, std::move(rust_object));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.from
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainMonthDay.from";
  DirectHandle<JSTemporalPlainMonthDay> item;

  return temporal::ToTemporalMonthDay(isolate, item_obj, options_obj,
                                      method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainMonthDay::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.PlainMonthDay.prototype.equals";

  DirectHandle<JSTemporalPlainMonthDay> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalMonthDay(isolate, other_obj, {}, method_name));

  auto equals =
      month_day->month_day()->raw()->equals(*other->month_day()->raw());

  return isolate->factory()->ToBoolean(equals);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.with
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> temporal_month_day,
    DirectHandle<Object> temporal_month_day_like_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainYearMonth.prototype.with";

  using enum temporal::CalendarFieldsFlag;

  // 6. Let partialMonthDay be ? PrepareCalendarFields(calendar,
  // temporalMonthDayLike, « year, month, month-code, day », « », partial).
  return temporal::GenericWith<JSTemporalPlainMonthDay,
                               temporal_rs::PartialDate>(
      isolate, temporal_month_day, temporal_month_day_like_obj, options_obj,
      kYearFields | kMonthFields | kDay, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainMonthDay::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> item_obj) {
  // 1. Let monthDay be the this value.
  // 2. Perform ? RequireInternalSlot(monthDay,
  // [[InitializedTemporalMonthDay]]).

  // (handled by type system)

  // 3. If item is not an Object, then
  if (!IsJSReceiver(*item_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kYearMustBeObject));
  }
  DirectHandle<JSReceiver> item = Cast<JSReceiver>(item_obj);
  // 4. Let calendar be monthDay.[[Calendar]].
  auto calendar = month_day->month_day()->raw()->calendar().kind();
  // 5. Let fields be ISODateToFields(calendar, monthDay.[[ISODate]],
  // month-day).

  // (handled in Rust)

  // 6. Let inputFields be ? PrepareCalendarFields(calendar, item, « year », «
  // », « »).

  using enum temporal::CalendarFieldsFlag;
  temporal::CombinedRecord fields;

  MOVE_RETURN_ON_EXCEPTION(
      isolate, fields,
      temporal::PrepareCalendarFields(isolate, calendar, item, kYearFields,
                                      temporal::RequiredFields::kNone));

  temporal_rs::PartialDate partial_date = temporal::kNullPartialDate;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, partial_date,
      fields.Regulate<temporal_rs::PartialDate>(
          isolate, temporal_rs::ArithmeticOverflow::Constrain));
  return ConstructRustWrappingType<JSTemporalPlainDate>(
      isolate, month_day->month_day()->raw()->to_plain_date(partial_date));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainMonthDay::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainMonthDay.prototype.toString";

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 4. Let showCalendar be ?GetTemporalShowCalendarNameOption(resolvedOptions).
  temporal_rs::DisplayCalendar show_calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, show_calendar,
                             temporal::GetTemporalShowCalendarNameOption(
                                 isolate, options, method_name));

  // 5. Return TemporalYearMonthToString(monthDay, showCalendar).
  return temporal::GenericTemporalToString(
      isolate, month_day, &temporal_rs::PlainMonthDay::to_ixdtf_string,
      show_calendar);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainMonthDay::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day) {
  return temporal::GenericTemporalToString(
      isolate, month_day, &temporal_rs::PlainMonthDay::to_ixdtf_string,
      temporal_rs::DisplayCalendar::Auto);
}

MaybeDirectHandle<String> JSTemporalPlainMonthDay::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sup-temporal.plainmonthday.prototype.tolocalestring
  return JSDateTimeFormat::TemporalToLocaleString(
      isolate, month_day, locales, options,
      JSDateTimeFormat::RequiredOption::kDate,
      JSDateTimeFormat::DefaultsOption::kDate,
      "Temporal.PlainMonthDay.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.plainmonthday.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, month_day, &temporal_rs::PlainMonthDay::to_ixdtf_string,
      temporal_rs::DisplayCalendar::Auto);
#endif  // V8_INTL_SUPPORT
}

// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalmonthday
Maybe<int64_t> JSTemporalPlainMonthDay::GetEpochMillisecondsFor(
    Isolate* isolate, std::string_view time_zone) {
  temporal_rs::TimeZone tz;
  MOVE_RETURN_ON_EXCEPTION(isolate, tz,
                           temporal::ToRustTimeZone(isolate, time_zone));

  return ExtractRustResult(isolate,
                           this->month_day()->raw()->epoch_ms_for_with_provider(
                               tz, TimeZoneProvider()));
}

MaybeDirectHandle<JSTemporalPlainYearMonth>
JSTemporalPlainYearMonth::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_year_obj,
    DirectHandle<Object> iso_month_obj, DirectHandle<Object> calendar_like,
    DirectHandle<Object> reference_iso_day_obj) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.PlainYearMonth")));
  }

  // 2. If referenceISODay is undefined, then
  // a. Set referenceISODay to 1𝔽.
  double reference_iso_day = 1.0;

  // 3. Let y be ? ToIntegerWithTruncation(isoYear).
  double y = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, y, temporal::ToIntegerWithTruncation(isolate, iso_year_obj));
  // 4. Let m be ? ToIntegerWithTruncation(isoMonth).
  double m = 0;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, m, temporal::ToIntegerWithTruncation(isolate, iso_month_obj));

  // 5. If calendar is undefined, set calendar to "iso8601".
  temporal_rs::AnyCalendarKind calendar = temporal_rs::AnyCalendarKind::Iso;

  if (!IsUndefined(*calendar_like)) {
    // 6. If calendar is not a String, throw a TypeError exception.
    if (!IsString(*calendar_like)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kCalendarMustBeString));
    }

    // 7. Set calendar to ?CanonicalizeCalendar(calendar).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, calendar,
        temporal::CanonicalizeCalendar(isolate, Cast<String>(calendar_like)));
  }

  if (!IsUndefined(*reference_iso_day_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, reference_iso_day,
        temporal::ToIntegerWithTruncation(isolate, reference_iso_day_obj));
  }

  // 9. If IsValidISODate(y, m, ref) is false, throw a RangeError exception.
  if (!temporal::IsValidIsoDate(y, m, reference_iso_day)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kInvalidIsoDate));
  }
  // Rest of the steps handled in Rust.

  // These static_casts are fine to perform since IsValid* will have constrained
  // these values to range already.
  // https://github.com/boa-dev/temporal/issues/334 for moving this logic into
  // the Rust code.
  auto rust_object = temporal_rs::PlainYearMonth::try_new_with_overflow(
      static_cast<int32_t>(y), static_cast<uint8_t>(m),
      static_cast<uint8_t>(reference_iso_day), calendar,
      temporal_rs::ArithmeticOverflow::Reject);
  return ConstructRustWrappingType<JSTemporalPlainYearMonth>(
      isolate, target, new_target, std::move(rust_object));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.from
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainYearMonth.from";
  DirectHandle<JSTemporalPlainYearMonth> item;

  return temporal::ToTemporalYearMonth(isolate, item_obj, options_obj,
                                       method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.compare
MaybeDirectHandle<Smi> JSTemporalPlainYearMonth::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  static const char method_name[] = "Temporal.PlainYearMonth.compare";
  DirectHandle<JSTemporalPlainYearMonth> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one,
      temporal::ToTemporalYearMonth(isolate, one_obj, {}, method_name));
  DirectHandle<JSTemporalPlainYearMonth> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two,
      temporal::ToTemporalYearMonth(isolate, two_obj, {}, method_name));

  return direct_handle(
      Smi::FromInt(temporal_rs::PlainYearMonth::compare(
          *one->year_month()->raw(), *two->year_month()->raw())),
      isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainYearMonth::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.PlainYearMonth.prototype.equals";

  DirectHandle<JSTemporalPlainYearMonth> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalYearMonth(isolate, other_obj, {}, method_name));

  auto equals =
      year_month->year_month()->raw()->equals(*other->year_month()->raw());

  return isolate->factory()->ToBoolean(equals);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.add
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainYearMonth.prototype.add";
  // 3. Return ? AddDurationToYearMonth(add, temporalDate, temporalDurationLike,
  // options).
  return temporal::AddDurationToGeneric(
      isolate, &temporal_rs::PlainYearMonth::add, year_month,
      temporal_duration_like, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.subtract
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  static const char method_name[] =
      "Temporal.PlainYearMonth.prototype.subtract";
  // 3. Return ? AddDurationToYearMonth(subtract, temporalDate,
  // temporalDurationLike, options).
  return temporal::AddDurationToGeneric(
      isolate, &temporal_rs::PlainYearMonth::subtract, year_month,
      temporal_duration_like, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainYearMonth::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainYearMonth.prototype.until";

  // Uplifted from step 5 of DifferenceTemporalPlainYearMonth
  //
  // (Differ..YearMonth) 5. Let settings be
  // ? GetDifferenceSettings(operation, resolvedOptions, date, « week, day »,
  // month, year).
  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainYearMonth::until, UnitGroup::kDate,
      Unit::Month, handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainYearMonth::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainYearMonth.prototype.since";

  // Uplifted from step 5 of DifferenceTemporalPlainYearMonth
  //
  // (Differ..YearMonth) 5. Let settings be
  // ? GetDifferenceSettings(operation, resolvedOptions, date, « week, day »,
  // month, year).
  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainYearMonth::since, UnitGroup::kDate,
      Unit::Month, handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.with
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::With(
    Isolate* isolate,
    DirectHandle<JSTemporalPlainYearMonth> temporal_year_month,
    DirectHandle<Object> temporal_year_month_like_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainYearMonth.prototype.with";

  using enum temporal::CalendarFieldsFlag;

  return temporal::GenericWith<JSTemporalPlainYearMonth,
                               temporal_rs::PartialDate>(
      isolate, temporal_year_month, temporal_year_month_like_obj, options_obj,
      kYearFields | kMonthFields, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainYearMonth::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> item_obj) {
  // 1. Let yearMonth be the this value.
  // 2. Perform ? RequireInternalSlot(yearMonth,
  // [[InitializedTemporalYearMonth]]).

  // (handled by type system)

  // 3. If item is not an Object, then
  if (!IsJSReceiver(*item_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kYearMustBeObject));
  }
  DirectHandle<JSReceiver> item = Cast<JSReceiver>(item_obj);
  // 4. Let calendar be yearMonth.[[Calendar]].
  auto calendar = year_month->year_month()->raw()->calendar().kind();
  // 5. Let fields be ISODateToFields(calendar, yearMonth.[[ISODate]],
  // year-month).

  // (handled in Rust)

  // 6. Let inputFields be ? PrepareCalendarFields(calendar, item, « day », « »,
  // « »).

  using enum temporal::CalendarFieldsFlag;
  temporal::CombinedRecord fields;

  MOVE_RETURN_ON_EXCEPTION(
      isolate, fields,
      temporal::PrepareCalendarFields(isolate, calendar, item, kDay,
                                      temporal::RequiredFields::kNone));
  temporal_rs::PartialDate partial_date = temporal::kNullPartialDate;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, partial_date,
      fields.Regulate<temporal_rs::PartialDate>(
          isolate, temporal_rs::ArithmeticOverflow::Constrain));
  return ConstructRustWrappingType<JSTemporalPlainDate>(
      isolate, year_month->year_month()->raw()->to_plain_date(partial_date));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainYearMonth::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> options_obj) {
  static const char method_name[] =
      "Temporal.PlainYearMonth.prototype.toString";

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 4. Let showCalendar be ?GetTemporalShowCalendarNameOption(resolvedOptions).
  temporal_rs::DisplayCalendar show_calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, show_calendar,
                             temporal::GetTemporalShowCalendarNameOption(
                                 isolate, options, method_name));

  // 5. Return TemporalYearMonthToString(yearMonth, showCalendar).
  return temporal::GenericTemporalToString(
      isolate, year_month, &temporal_rs::PlainYearMonth::to_ixdtf_string,
      show_calendar);
}

MaybeDirectHandle<String> JSTemporalPlainYearMonth::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sup-temporal.plainyearmonth.prototype.tolocalestring
  return JSDateTimeFormat::TemporalToLocaleString(
      isolate, year_month, locales, options,
      JSDateTimeFormat::RequiredOption::kDate,
      JSDateTimeFormat::DefaultsOption::kDate,
      "Temporal.PlainYearMonth.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, year_month, &temporal_rs::PlainYearMonth::to_ixdtf_string,
      temporal_rs::DisplayCalendar::Auto);
#endif  // V8_INTL_SUPPORT
}

// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporalyearmonth
Maybe<int64_t> JSTemporalPlainYearMonth::GetEpochMillisecondsFor(
    Isolate* isolate, std::string_view time_zone) {
  temporal_rs::TimeZone tz;
  MOVE_RETURN_ON_EXCEPTION(isolate, tz,
                           temporal::ToRustTimeZone(isolate, time_zone));

  return ExtractRustResult(
      isolate, this->year_month()->raw()->epoch_ms_for_with_provider(
                   tz, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plainyearmonth.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainYearMonth::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month) {
  return temporal::GenericTemporalToString(
      isolate, year_month, &temporal_rs::PlainYearMonth::to_ixdtf_string,
      temporal_rs::DisplayCalendar::Auto);
}

// https://tc39.es/proposal-temporal/#sec-temporal-plaintime-constructor
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> hour_obj,
    DirectHandle<Object> minute_obj, DirectHandle<Object> second_obj,
    DirectHandle<Object> millisecond_obj, DirectHandle<Object> microsecond_obj,
    DirectHandle<Object> nanosecond_obj) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.PlainTime")));
  }
  // 2. If hour is undefined, set hour to 0; else set hour to ?
  // ToIntegerWithTruncation(hour).
  double hour = 0;
  if (!IsUndefined(*hour_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, hour,
        temporal::ToIntegerWithTruncationOrZero(isolate, hour_obj));
  }
  // 3. If minute is undefined, set minute to 0; else set minute to ?
  // ToIntegerWithTruncation(minute).
  double minute = 0;
  if (!IsUndefined(*minute_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, minute,
        temporal::ToIntegerWithTruncationOrZero(isolate, minute_obj));
  }
  // 4. If second is undefined, set second to 0; else set second to ?
  // ToIntegerWithTruncation(second).
  double second = 0;
  if (!IsUndefined(*second_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, second,
        temporal::ToIntegerWithTruncationOrZero(isolate, second_obj));
  }
  // 5. If millisecond is undefined, set millisecond to 0; else set millisecond
  // to ? ToIntegerWithTruncation(millisecond).
  double millisecond = 0;
  if (!IsUndefined(*millisecond_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, millisecond,
        temporal::ToIntegerWithTruncationOrZero(isolate, millisecond_obj));
  }
  // 6. If microsecond is undefined, set microsecond to 0; else set microsecond
  // to ? ToIntegerWithTruncation(microsecond).

  double microsecond = 0;
  if (!IsUndefined(*microsecond_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, microsecond,
        temporal::ToIntegerWithTruncationOrZero(isolate, microsecond_obj));
  }
  // 7. If nanosecond is undefined, set nanosecond to 0; else set nanosecond to
  // ? ToIntegerWithTruncation(nanosecond).

  double nanosecond = 0;
  if (!IsUndefined(*nanosecond_obj)) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, nanosecond,
        temporal::ToIntegerWithTruncationOrZero(isolate, nanosecond_obj));
  }

  // 8. If IsValidTime(hour, minute, second, millisecond, microsecond,
  // nanosecond) is false, throw a RangeError exception.
  if (!temporal::IsValidTime(hour, minute, second, millisecond, microsecond,
                             nanosecond)) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_RANGE_ERROR(kInvalidTime));
  }

  // Rest of the steps handled in Rust

  // These static_casts are fine to perform since IsValid* will have constrained
  // these values to range already.
  // https://github.com/boa-dev/temporal/issues/334 for moving this logic into
  // the Rust code
  auto rust_object = temporal_rs::PlainTime::try_new(
      static_cast<uint8_t>(hour), static_cast<uint8_t>(minute),
      static_cast<uint8_t>(second), static_cast<uint16_t>(millisecond),
      static_cast<uint16_t>(microsecond), static_cast<uint16_t>(nanosecond));
  return ConstructRustWrappingType<JSTemporalPlainTime>(
      isolate, target, new_target, std::move(rust_object));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.compare
MaybeDirectHandle<Smi> JSTemporalPlainTime::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  static const char method_name[] = "Temporal.PlainTime.compare";
  DirectHandle<JSTemporalPlainTime> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one,
      temporal::ToTemporalTime(isolate, one_obj, {}, method_name));
  DirectHandle<JSTemporalPlainTime> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two,
      temporal::ToTemporalTime(isolate, two_obj, {}, method_name));

  return direct_handle(Smi::FromInt(temporal_rs::PlainTime::compare(
                           *one->time()->raw(), *two->time()->raw())),
                       isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainTime::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.PlainTime.prototype.equals";

  // 3. Set other to ? ToTemporalTime(other).
  DirectHandle<JSTemporalPlainTime> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalTime(isolate, other_obj, {}, method_name));

  // Rest of the steps handled in Rust
  auto equals = temporal_time->time()->raw()->equals(*other->time()->raw());

  return isolate->factory()->ToBoolean(equals);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.round
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Round(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> round_to_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.round";
  // 1. Let temporalTime be the this value.
  // 2. Perform ? RequireInternalSlot(dateTime, [[InitializedTemporalTime]]).

  // (handled by type system)

  // 3. If roundTo is undefined, then
  if (IsUndefined(*round_to_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMissing));
  }

  DirectHandle<JSReceiver> round_to;
  auto factory = isolate->factory();

  // 4. If roundTo is a String, then
  if (IsString(*round_to_obj)) {
    // a. Let paramString be roundTo.
    DirectHandle<String> param_string = Cast<String>(round_to_obj);
    // b. Set roundTo to ! OrdinaryObjectCreate(null).
    round_to = factory->NewJSObjectWithNullProto();
    // TODO(manishearth) Ideally we don't have to perform this allocation
    // c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit",
    // paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, round_to,
                                         factory->smallestUnit_string(),
                                         param_string, Just(kThrowOnError))
              .FromJust());
    // 5. Else,
  } else {
    // a. Set roundTo to ? GetOptionsObject(roundTo).
    // We have already checked for undefined, we can hoist the JSReceiver
    // check out and just cast
    if (!IsJSReceiver(*round_to_obj)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMustBeObject));
    }
    round_to = Cast<JSReceiver>(round_to_obj);
  }

  // 6. NOTE: (...)

  // 7. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).

  uint32_t rounding_increment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, round_to));

  // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, round_to,
                                      RoundingMode::HalfExpand, method_name));

  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo,
  // "smallestUnit", required).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, round_to, factory->smallestUnit_string(),
          DefaultValue::kRequired, method_name));
  // 10. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  // Rest of the steps handled in Rust
  auto options = temporal_rs::RoundingOptions{.largest_unit = std::nullopt,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment};
  auto rounded = temporal_time->time()->raw()->round(options);
  return ConstructRustWrappingType<JSTemporalPlainTime>(isolate,
                                                        std::move(rounded));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.with
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_time_like_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainTime.prototype.with";

  // 3. If ? IsPartialTemporalObject(temporalTimeLike) is false, throw a
  // TypeError exception.
  bool is_partial = false;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, is_partial,
      temporal::IsPartialTemporalObject(isolate, temporal_time_like_obj));

  if (!is_partial) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kWithNoPartial));
  }

  // 4. Let partialTime be ? ToTemporalTimeRecord(temporalTimeLike, partial).
  temporal::TimeRecord partial_time;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, partial_time,
      temporal::ToTemporalTimeRecord(isolate,
                                     Cast<JSReceiver>(temporal_time_like_obj),
                                     method_name, kPartial));

  // Intervening steps handled by Rust, but are not externally observable

  // 17. Let resolvedOptions be ? GetOptionsObject(options).
  // 18. Let overflow be ? GetTemporalOverflowOption(resolvedOptions).

  temporal_rs::ArithmeticOverflow overflow;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, overflow,
                             temporal::ToTemporalOverflowHandleUndefined(
                                 isolate, options_obj, method_name));

  // 19. Let result be ? RegulateTime(hour, minute, second, millisecond,
  // microsecond, nanosecond, overflow).
  // *technically* this wants to use a full TimeRecord object with
  // all None fields filled from the PlainTime. However, we don't
  // actually need to do this: RegulateTime will ignore the None
  // fields and the Rust code below will handle the rest.
  temporal_rs::PartialTime result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             partial_time.Regulate(isolate, overflow));
  // 20. Return ! CreateTemporalTime(result).
  return ConstructRustWrappingType<JSTemporalPlainTime>(
      isolate, temporal_time->time()->raw()->with(result, overflow));
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.plaintimeiso
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      temporal::GenericTemporalNowISO(isolate, temporal_time_zone_like));
  return ConstructRustWrappingType<JSTemporalPlainTime>(isolate,
                                                        zdt->to_plain_time());
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.from
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainTime.from";
  DirectHandle<JSTemporalPlainTime> item;

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, item,
      temporal::ToTemporalTime(isolate, item_obj, options_obj, method_name));

  return item;
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.add
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_duration_like) {
  static const char method_name[] = "Temporal.PlainTime.prototype.add";

  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, other_duration,
                             temporal::ToTemporalDuration(
                                 isolate, temporal_duration_like, method_name));

  auto added =
      temporal_time->time()->raw()->add(*other_duration->duration()->raw());

  return ConstructRustWrappingType<JSTemporalPlainTime>(isolate,
                                                        std::move(added));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.subtract
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_duration_like) {
  static const char method_name[] = "Temporal.PlainTime.prototype.subtract";

  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, other_duration,
                             temporal::ToTemporalDuration(
                                 isolate, temporal_duration_like, method_name));

  auto subtracted = temporal_time->time()->raw()->subtract(
      *other_duration->duration()->raw());

  return ConstructRustWrappingType<JSTemporalPlainTime>(isolate,
                                                        std::move(subtracted));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainTime::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainTime.prototype.until";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainTime::until, UnitGroup::kTime,
      Unit::Nanosecond, handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainTime::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.PlainTime.prototype.since";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::PlainTime::since, UnitGroup::kTime,
      Unit::Nanosecond, handle, other, options, method_name);
}


// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainTime::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.PlainTime.prototype.toString";
  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 5. Let digits be ?GetTemporalFractionalSecondDigitsOption(resolvedOptions).

  temporal_rs::Precision digits;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, digits,
                             temporal::GetTemporalFractionalSecondDigitsOption(
                                 isolate, options, method_name));

  // 6. Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).

  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, options, RoundingMode::Trunc,
                                      method_name));

  // 8. Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions,
  // "smallestUnit", unset).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));
  // 9. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  // 8-10 performed by Rust
  auto rust_options = temporal_rs::ToStringRoundingOptions{
      .precision = digits,
      .smallest_unit = smallest_unit,
      .rounding_mode = rounding_mode,
  };

  return temporal::GenericTemporalToString(
      isolate, temporal_time, &temporal_rs::PlainTime::to_ixdtf_string,
      std::move(rust_options));
}

// https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainTime::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time) {
  return temporal::GenericTemporalToString(
      isolate, temporal_time, &temporal_rs::PlainTime::to_ixdtf_string,
      std::move(temporal::kToStringAuto));
}

MaybeDirectHandle<String> JSTemporalPlainTime::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sup-temporal.plaintime.prototype.tolocalestring
  return JSDateTimeFormat::TemporalToLocaleString(
      isolate, temporal_time, locales, options,
      JSDateTimeFormat::RequiredOption::kTime,
      JSDateTimeFormat::DefaultsOption::kTime,
      "Temporal.PlainTime.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.plaintime.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, temporal_time, &temporal_rs::PlainTime::to_ixdtf_string,
      std::move(temporal::kToStringAuto));
#endif  // V8_INTL_SUPPORT
}

// https://tc39.es/proposal-temporal/#sec-temporal-handledatetimetemporaldate
Maybe<int64_t> JSTemporalPlainTime::GetEpochMillisecondsFor(
    Isolate* isolate, std::string_view time_zone) {
  // 1. Let isoDate be CreateISODateRecord(1970, 1, 1).

  std::unique_ptr<temporal_rs::PlainDate> pd;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, pd,
      ExtractRustResult(isolate,
                        temporal_rs::PlainDate::try_new(
                            1970, 1, 1, temporal_rs::AnyCalendarKind::Iso)));

  // 2. Let isoDateTime be CombineISODateAndTimeRecord(isoDate,
  // temporalTime.[[Time]]).
  // 3. Let epochNs be ? GetEpochNanosecondsFor(dateTimeFormat.[[TimeZone]],
  // isoDateTime, compatible).

  return temporal::GetEpochMillisecondsForDate(isolate, *pd, time_zone,
                                               time()->raw());
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    DirectHandle<Object> epoch_nanoseconds_obj,
    DirectHandle<Object> time_zone_like, DirectHandle<Object> calendar_like) {
  // 1. If NewTarget is undefined, throw a TypeError exception.
  if (IsUndefined(*new_target)) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.ZonedDateTime")));
  }

  // 2. Set epochNanoseconds to ? ToBigInt(epochNanoseconds).
  DirectHandle<BigInt> epoch_nanoseconds;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, epoch_nanoseconds,
      BigInt::FromObject(isolate, epoch_nanoseconds_obj));

  // 3. If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
  // RangeError exception.
  temporal_rs::I128Nanoseconds ns;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ns, temporal::GetI128FromBigInt(isolate, epoch_nanoseconds));

  // 4. If timeZone is not a String, throw a TypeError exception.
  if (!IsString(*time_zone_like)) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_TYPE_ERROR("Time zone must be string"));
  }

  auto tz_str = Cast<String>(time_zone_like);

  auto tz_stdstr = tz_str->ToStdString();

  // 5. Let timeZoneParse be ? ParseTimeZoneIdentifier(timeZone).
  // 6. If timeZoneParse.[[OffsetMinutes]] is empty, then
  //   a. Let identifierRecord be
  //   GetAvailableNamedTimeZoneIdentifier(timeZoneParse.[[Name]]). b. If
  //   identifierRecord is empty, throw a RangeError exception. c. Set timeZone
  //   to identifierRecord.[[Identifier]].
  // 7. Else,
  //   a. Set timeZone to
  //   FormatOffsetTimeZoneIdentifier(timeZoneParse.[[OffsetMinutes]]).
  temporal_rs::TimeZone time_zone;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      ExtractRustResult(
          isolate, temporal_rs::TimeZone::try_from_identifier_str_with_provider(
                       tz_stdstr, TimeZoneProvider())));

  // 8. If calendar is undefined, set calendar to "iso8601".
  temporal_rs::AnyCalendarKind calendar = temporal_rs::AnyCalendarKind::Iso;

  // We pre-parsed "iso8601" so we can do steps 9 and 10 in a branch
  if (!IsUndefined(*calendar_like)) {
    // 9. If calendar is not a String, throw a TypeError exception.
    if (!IsString(*calendar_like)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kCalendarMustBeString));
    }

    // 10. Set calendar to ? CanonicalizeCalendar(calendar).
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, calendar,
        temporal::CanonicalizeCalendar(isolate, Cast<String>(calendar_like)),
        kNullMaybeHandle);
  }

  // 11. Return ? CreateTemporalZonedDateTime(epochNanoseconds, timeZone,
  // calendar, NewTarget).
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate, target, new_target,
      temporal_rs::ZonedDateTime::try_new_with_provider(ns, calendar, time_zone,
                                                        TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.hoursinday
MaybeDirectHandle<Number> JSTemporalZonedDateTime::HoursInDay(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  double hours;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, hours,
      ExtractRustResult(
          isolate,
          zoned_date_time->zoned_date_time()->raw()->hours_in_day_with_provider(
              TimeZoneProvider())));
  return isolate->factory()->NewNumber(hours);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.from
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.ZonedDateTime.from";
  DirectHandle<JSTemporalPlainDateTime> item;

  return temporal::ToTemporalZonedDateTime(isolate, item_obj, options_obj,
                                           method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.compare
MaybeDirectHandle<Smi> JSTemporalZonedDateTime::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  static const char method_name[] = "Temporal.ZonedDateTime.compare";
  DirectHandle<JSTemporalZonedDateTime> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one,
      temporal::ToTemporalZonedDateTime(isolate, one_obj, {}, method_name));
  DirectHandle<JSTemporalZonedDateTime> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two,
      temporal::ToTemporalZonedDateTime(isolate, two_obj, {}, method_name));

  return direct_handle(
      Smi::FromInt(one->zoned_date_time()->raw()->compare_instant(
          *two->zoned_date_time()->raw())),
      isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalZonedDateTime::Equals(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.equals";

  // 3. Set other to ? ToTemporalZonedDateTime(other).
  DirectHandle<JSTemporalZonedDateTime> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalZonedDateTime(isolate, other_obj, {}, method_name));

  // Rest of the steps handled in Rust.
  auto result = zoned_date_time->zoned_date_time()->raw()->equals_with_provider(
      *other->zoned_date_time()->raw(), TimeZoneProvider());
  bool equals;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, equals,
                             ExtractRustResult(isolate, std::move(result)));

  return isolate->factory()->ToBoolean(equals);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.with
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::With(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> temporal_zoned_date_time_like_obj,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.with";

  using enum temporal::CalendarFieldsFlag;

  return temporal::GenericWith<JSTemporalZonedDateTime,
                               temporal_rs::PartialZonedDateTime>(
      isolate, zoned_date_time, temporal_zoned_date_time_like_obj, options_obj,
      temporal::kAllDateFlags | kTimeFields | kOffset, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withcalendar
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalZonedDateTime::WithCalendar(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> temporal_date,
    DirectHandle<Object> calendar_id) {
  // 3. Let calendar be ? ToTemporalCalendarIdentifier(calendarLike).
  temporal_rs::AnyCalendarKind calendar_kind;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, calendar_kind,
      temporal::ToTemporalCalendarIdentifier(isolate, calendar_id));

  // Rest of the steps handled in Rust
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate,
      temporal_date->zoned_date_time()->raw()->with_calendar(calendar_kind));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withplaintime
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalZonedDateTime::WithPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> plain_time_like) {
  static const char method_name[] =
      "Temporal.ZonedDateTime.prototype.withPlainTime";
  // 1. Let zonedDateTime be the this value.
  //
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).
  //
  // 3. Let timeZone be zonedDateTime.[[TimeZone]].
  //
  // 4. Let calendar be zonedDateTime.[[Calendar]].

  // (handled by type system)

  // 5. Let isoDateTime be GetISODateTimeFor(timeZone,
  // zonedDateTime.[[EpochNanoseconds]]).

  // (handled later in Rust, idempotent)

  DirectHandle<JSTemporalPlainTime> plain_time_obj;
  temporal_rs::PlainTime* plain_time = nullptr;

  // 6. If plainTimeLike is undefined, then
  if (IsUndefined(*plain_time_like)) {
    // a. Let epochNs be ? GetStartOfDay(timeZone, isoDateTime.[[ISODate]]).

    // (handled later in Rust, there are no steps between this and the Rust
    // call. Rust handles this by detecting a null time argument)

    // 7. Else,
  } else {
    // a. Let plainTime be ? ToTemporalTime(plainTimeLike).

    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, plain_time_obj,
        temporal::ToTemporalTime(isolate, plain_time_like, {}, method_name));

    plain_time = plain_time_obj->time()->raw();
    // b. Let resultISODateTime be
    // CombineISODateAndTimeRecord(isoDateTime.[[ISODate]], plainTime.[[Time]]).
    // c. Let epochNs be ? GetEpochNanosecondsFor(timeZone, resultISODateTime,
    // compatible).

    // (handled later in Rust, there are no steps between this and the Rust
    // call)
  }

  // Rest of the steps handled in Rust.

  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate,
      zoned_date_time->zoned_date_time()->raw()->with_plain_time_and_provider(
          plain_time, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.withtimezone
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalZonedDateTime::WithTimeZone(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> time_zone_like) {
  // 3. Let timeZone be ? ToTemporalTimeZoneIdentifier(timeZoneLike).
  temporal_rs::TimeZone time_zone;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      temporal::ToTemporalTimeZoneIdentifier(isolate, time_zone_like));

  // 4. Return ! CreateTemporalZonedDateTime(zonedDateTime.[[EpochNanoseconds]],
  // timeZone, zonedDateTime.[[Calendar]]).
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate,
      zoned_date_time->zoned_date_time()->raw()->with_timezone_with_provider(
          time_zone, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tostring
MaybeDirectHandle<String> JSTemporalZonedDateTime::ToString(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.toString";

  // 3. Let resolvedOptions be ?GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 5. Let showCalendar be ?GetTemporalShowCalendarNameOption(resolvedOptions).
  temporal_rs::DisplayCalendar show_calendar;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, show_calendar,
                             temporal::GetTemporalShowCalendarNameOption(
                                 isolate, options, method_name));

  // 6. Let digits be ?GetTemporalFractionalSecondDigitsOption(resolvedOptions).
  temporal_rs::Precision digits;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, digits,
                             temporal::GetTemporalFractionalSecondDigitsOption(
                                 isolate, options, method_name));

  // 7. Let showOffset be ? GetTemporalShowOffsetOption(resolvedOptions)..
  temporal_rs::DisplayOffset show_offset;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, show_offset,
      temporal::GetTemporalShowOffsetOption(isolate, options, method_name));

  // 8. Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, options, RoundingMode::Trunc,
                                      method_name));

  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions,
  // "smallestUnit", unset).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));

  // 10. Let showTimeZone be
  // ? GetTemporalShowTimeZoneNameOption(resolvedOptions).
  temporal_rs::DisplayTimeZone show_tz;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, show_tz,
                             temporal::GetTemporalShowTimeZoneNameOption(
                                 isolate, options, method_name));

  // 11. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  // 12. If smallestUnit is hour, throw a RangeError exception.
  if (smallest_unit == Unit::Hour) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_RANGE_ERROR("smallestUnit cannot be Hour."));
  }

  // Rest of the steps handled in Rust
  auto rust_options = temporal_rs::ToStringRoundingOptions{
      .precision = digits,
      .smallest_unit = smallest_unit,
      .rounding_mode = rounding_mode,
  };
  return temporal::GenericTemporalToString(
      isolate, zoned_date_time,
      &temporal_rs::ZonedDateTime::to_ixdtf_string_with_provider, show_offset,
      show_tz, show_calendar, std::move(rust_options),
      std::ref(TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tojson
MaybeDirectHandle<String> JSTemporalZonedDateTime::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return temporal::GenericTemporalToString(
      isolate, zoned_date_time,
      &temporal_rs::ZonedDateTime::to_ixdtf_string_with_provider,
      temporal_rs::DisplayOffset::Auto, temporal_rs::DisplayTimeZone::Auto,
      temporal_rs::DisplayCalendar::Auto, std::move(temporal::kToStringAuto),
      std::ref(TimeZoneProvider()));
}

MaybeDirectHandle<String> JSTemporalZonedDateTime::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sub-temporal.zoneddatetime.prototype.tolocalestring
  return JSDateTimeFormat::TemporalZonedDateTimeToLocaleString(
      isolate, zoned_date_time, locales, options,
      "Temporal.ZonedDateTime.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, zoned_date_time,
      &temporal_rs::ZonedDateTime::to_ixdtf_string_with_provider,
      temporal_rs::DisplayOffset::Auto, temporal_rs::DisplayTimeZone::Auto,
      temporal_rs::DisplayCalendar::Auto, std::move(temporal::kToStringAuto),
      std::ref(TimeZoneProvider()));
#endif  // V8_INTL_SUPPORT
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.zoneddatetimeiso
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      temporal::GenericTemporalNowISO(isolate, temporal_time_zone_like));
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(isolate,
                                                            std::move(zdt));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.round
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Round(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> round_to_obj) {
  static const char method_name[] = "Temporal.PlainDateTime.prototype.round";
  // 1. Let zonedDateTime be the this value.
  // 2. Perform ? RequireInternalSlot(zonedDateTime,
  // [[InitializedTemporalZonedDateTime]]).

  // (handled by type system)

  // 3. If roundTo is undefined, then
  if (IsUndefined(*round_to_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMissing));
  }

  DirectHandle<JSReceiver> round_to;
  auto factory = isolate->factory();

  // 4. If roundTo is a String, then
  if (IsString(*round_to_obj)) {
    // a. Let paramString be roundTo.
    DirectHandle<String> param_string = Cast<String>(round_to_obj);
    // b. Set roundTo to ! OrdinaryObjectCreate(null).
    round_to = factory->NewJSObjectWithNullProto();
    // c. c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit",
    // paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, round_to,
                                         factory->smallestUnit_string(),
                                         param_string, Just(kThrowOnError))
              .FromJust());
    // 5. Else,
  } else {
    // a. Set roundTo to ? GetOptionsObject(roundTo).
    // We have already checked for undefined, we can hoist the JSReceiver
    // check out and just cast
    if (!IsJSReceiver(*round_to_obj)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMustBeObject));
    }
    round_to = Cast<JSReceiver>(round_to_obj);
  }

  // 6. NOTE: (...)

  // 7. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).

  uint32_t rounding_increment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, round_to));

  // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, round_to,
                                      RoundingMode::HalfExpand, method_name));

  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo,
  // "smallestUnit", required).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, round_to, isolate->factory()->smallestUnit_string(),
          DefaultValue::kRequired, method_name));

  // 10. Perform ? ValidateTemporalUnitValue(smallestUnit, time, « day »).
  RETURN_ON_EXCEPTION(isolate,
                      temporal::ValidateTemporalUnitValue(
                          isolate, smallest_unit, UnitGroup::kTime, Unit::Day));

  // Rest of the steps handled in Rust

  auto options = temporal_rs::RoundingOptions{.largest_unit = std::nullopt,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment};

  auto rounded = zoned_date_time->zoned_date_time()->raw()->round_with_provider(
      options, TimeZoneProvider());
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(isolate,
                                                            std::move(rounded));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.add
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Add(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.add";
  // 3. Return ? AddDurationToZonedDateTime(add, temporalDate,
  // temporalDurationLike, options).
  return temporal::AddDurationToGeneric(
      isolate, &temporal_rs::ZonedDateTime::add_with_provider, zoned_date_time,
      temporal_duration_like, options, method_name, TimeZoneProvider());
}
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.subtract
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.subtract";
  // 3. Return ? AddDurationToZonedDateTime(subtract, temporalDate,
  // temporalDurationLike, options).
  return temporal::AddDurationToGeneric(
      isolate, &temporal_rs::ZonedDateTime::subtract_with_provider,
      zoned_date_time, temporal_duration_like, options, method_name,
      TimeZoneProvider());
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalZonedDateTime::Until(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.since";
  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::ZonedDateTime::until_with_provider,
      UnitGroup::kDateTime, Unit::Nanosecond, handle, other, options,
      method_name, TimeZoneProvider());
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalZonedDateTime::Since(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.ZonedDateTime.prototype.since";
  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::ZonedDateTime::since_with_provider,
      UnitGroup::kDateTime, Unit::Nanosecond, handle, other, options,
      method_name, TimeZoneProvider());
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.instant
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Now(Isolate* isolate) {
  auto ms = temporal::SystemUTCEpochMilliseconds();
  return ConstructRustWrappingType<JSTemporalInstant>(
      isolate, temporal_rs::Instant::from_epoch_milliseconds(ms));
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
MaybeDirectHandle<Object> JSTemporalZonedDateTime::OffsetNanoseconds(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  int64_t offset_ns =
      zoned_date_time->zoned_date_time()->raw()->offset_nanoseconds();
  return isolate->factory()->NewNumberFromInt64(offset_ns);
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.epochnanoseconds
MaybeDirectHandle<BigInt> JSTemporalZonedDateTime::EpochNanoseconds(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return temporal::I128ToBigInt(
      isolate, zoned_date_time->zoned_date_time()->raw()->epoch_nanoseconds());
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.timezoneid
MaybeDirectHandle<String> JSTemporalZonedDateTime::TimeZoneId(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  std::string id;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, id,
      ExtractRustResult(isolate,
                        zoned_date_time->zoned_date_time()
                            ->raw()
                            ->timezone()
                            .identifier_with_provider(TimeZoneProvider())));

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(id);
  return builder.Finish().ToHandleChecked();
}

// https://tc39.es/proposal-temporal/#sec-get-temporal.zoneddatetime.prototype.offset
MaybeDirectHandle<String> JSTemporalZonedDateTime::Offset(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {

  std::string offset;

  MOVE_RETURN_ON_EXCEPTION(
      isolate, offset,
      ExtractRustResult(isolate,
                        zoned_date_time->zoned_date_time()->raw()->offset()));

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(offset);
  return builder.Finish().ToHandleChecked();
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.startofday
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::StartOfDay(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate,
      zoned_date_time->zoned_date_time()->raw()->start_of_day_with_provider(
          TimeZoneProvider()));
}
// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.gettimezonetransition
MaybeDirectHandle<UnionOf<JSTemporalZonedDateTime, Null>>
JSTemporalZonedDateTime::GetTimeZoneTransition(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> direction_param_obj) {
  static const char method_name[] =
      "Temporal.ZonedDateTime.prototype.getTimeZoneTransition";

  // 4. If directionParam is undefined, throw a TypeError exception.
  if (IsUndefined(*direction_param_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                 "Must specify a direction parameter."));
  }

  // 3. If roundTo is undefined, then

  DirectHandle<JSReceiver> direction_param;
  auto factory = isolate->factory();

  // 5. If directionParam is a String, then
  if (IsString(*direction_param_obj)) {
    // a. a. Let paramString be directionParam.
    DirectHandle<String> param_string = Cast<String>(direction_param_obj);
    //  b. Set directionParam to OrdinaryObjectCreate(null).
    direction_param = factory->NewJSObjectWithNullProto();
    // TODO(manishearth) Ideally we don't have to perform this allocation
    // c. Perform ! CreateDataPropertyOrThrow(directionParam, "direction",
    // paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, direction_param,
                                         factory->direction_string(),
                                         param_string, Just(kThrowOnError))
              .FromJust());
    // 6. Else,
  } else {
    // a. Set directionParam to ? GetOptionsObject(directionParam).
    // We have already checked for undefined, we can hoist the JSReceiver
    // check out and just cast
    if (!IsJSReceiver(*direction_param_obj)) {
      THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(
                                   "directionParam must be object or string."));
    }
    direction_param = Cast<JSReceiver>(direction_param_obj);
  }

  // 7. Let direction be ? GetDirectionOption(directionParam).
  temporal_rs::TransitionDirection dir;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, dir,
      temporal::GetDirectionOption(isolate, direction_param, method_name));

  // Steps 8-10 handled in Rust.
  std::unique_ptr<temporal_rs::ZonedDateTime> zdt;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, zdt,
      ExtractRustResult(isolate, zoned_date_time->zoned_date_time()
                                     ->raw()
                                     ->get_time_zone_transition_with_provider(
                                         dir, TimeZoneProvider())));

  // 11. If transition is null, return null.
  if (!zdt) {
    return isolate->factory()->null_value();
  }

  return ConstructRustWrappingType<JSTemporalZonedDateTime>(isolate,
                                                            std::move(zdt));
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toinstant
MaybeDirectHandle<JSTemporalInstant> JSTemporalZonedDateTime::ToInstant(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return temporal::GenericToTemporalMethod<JSTemporalInstant>(
      isolate, zoned_date_time, &temporal_rs::ZonedDateTime::to_instant);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalZonedDateTime::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainDate>(
      isolate, zoned_date_time, &temporal_rs::ZonedDateTime::to_plain_date);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaintime
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalZonedDateTime::ToPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainTime>(
      isolate, zoned_date_time, &temporal_rs::ZonedDateTime::to_plain_time);
}

// https://tc39.es/proposal-temporal/#sec-temporal.zoneddatetime.prototype.toplaindatetime
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalZonedDateTime::ToPlainDateTime(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  return temporal::GenericToTemporalMethod<JSTemporalPlainDateTime>(
      isolate, zoned_date_time, &temporal_rs::ZonedDateTime::to_plain_datetime);
}

namespace temporal {

// https://tc39.es/proposal-temporal/#sec-temporal-createtemporalinstant, but
// this also performs the validity check
MaybeDirectHandle<JSTemporalInstant> CreateTemporalInstantWithValidityCheck(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    DirectHandle<BigInt> epoch_nanoseconds) {

  temporal_rs::I128Nanoseconds ns;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, ns,
                             GetI128FromBigInt(isolate, epoch_nanoseconds));

  return ConstructRustWrappingType<JSTemporalInstant>(
      isolate, target, new_target, temporal_rs::Instant::try_new(ns));
}

MaybeDirectHandle<JSTemporalInstant> CreateTemporalInstantWithValidityCheck(
    Isolate* isolate, DirectHandle<BigInt> epoch_nanoseconds) {
  auto ctor = JSTemporalInstant::GetConstructorTarget(isolate);
  return CreateTemporalInstantWithValidityCheck(isolate, ctor, ctor,
                                                epoch_nanoseconds);
}

}  // namespace temporal

// https://tc39.es/proposal-temporal/#sec-temporal.instant
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    DirectHandle<Object> epoch_nanoseconds_obj) {
  // 1. If NewTarget is undefined, then
  if (IsUndefined(*new_target)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kMethodInvokedOnWrongType,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "Temporal.Instant")));
  }
  // 2. Let epochNanoseconds be ? ToBigInt(epochNanoseconds).
  DirectHandle<BigInt> epoch_nanoseconds;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, epoch_nanoseconds,
      BigInt::FromObject(isolate, epoch_nanoseconds_obj));

  return temporal::CreateTemporalInstantWithValidityCheck(
      isolate, target, new_target, epoch_nanoseconds);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.from
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::From(
    Isolate* isolate, DirectHandle<Object> item) {
  static const char method_name[] = "Temporal.Instant.from";
  DirectHandle<JSTemporalInstant> item_instant;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, item_instant,
      temporal::ToTemporalInstant(isolate, item, method_name));

  return item_instant;
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochmilliseconds
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::FromEpochMilliseconds(
    Isolate* isolate, DirectHandle<Object> epoch_milliseconds) {
  // 1. Let number be ? ToNumber(epochMilliseconds).
  DirectHandle<Number> number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, number,
                             Object::ToNumber(isolate, epoch_milliseconds));
  double ms = Object::NumberValue(*number);

  // 2. Set epochMilliseconds to ?NumberToBigInt(epochMilliseconds).
  //
  // Instead of allocating a BigInt, we perform the necessary check from
  // NumberToBigInt and cast:
  //
  // (NumberToBigInt) 1. If number is not an integral Number, throw a RangeError
  // exception.
  if (!std::isfinite(ms) ||
      !base::IsValueInRangeForNumericType<int64_t, double>(ms) ||
      nearbyint(ms) != ms) {
    THROW_NEW_ERROR(isolate,
                    NEW_TEMPORAL_RANGE_ERROR("Expected finite integer."));
  }
  auto ms_int = static_cast<int64_t>(ms);

  // 3. Let epochNanoseconds be epochMilliseconds × ℤ(10**6).
  // 4. If IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
  // RangeError exception.
  //
  // (Rest of the steps handled by Rust)

  return ConstructRustWrappingType<JSTemporalInstant>(
      isolate, temporal_rs::Instant::from_epoch_milliseconds(ms_int));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.fromepochnanoseconds
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::FromEpochNanoseconds(
    Isolate* isolate, DirectHandle<Object> epoch_nanoseconds_obj) {
  // 1. Set epochNanoseconds to ? ToBigInt(epochNanoseconds).
  DirectHandle<BigInt> epoch_nanoseconds;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, epoch_nanoseconds,
      BigInt::FromObject(isolate, epoch_nanoseconds_obj));

  // Rest of the steps handled by CreateTemporalInstantWithValidityCheck

  return temporal::CreateTemporalInstantWithValidityCheck(isolate,
                                                          epoch_nanoseconds);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.compare
MaybeDirectHandle<Smi> JSTemporalInstant::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  static const char method_name[] = "Temporal.Instant.compare";
  DirectHandle<JSTemporalInstant> one;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, one, temporal::ToTemporalInstant(isolate, one_obj, method_name));
  DirectHandle<JSTemporalInstant> two;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, two, temporal::ToTemporalInstant(isolate, two_obj, method_name));

  return direct_handle(
      Smi::FromInt(one->instant()->raw()->compare(*two->instant()->raw())),
      isolate);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalInstant::Equals(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> other_obj) {
  static const char method_name[] = "Temporal.Instant.prototype.equals";

  DirectHandle<JSTemporalInstant> other;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, other,
      temporal::ToTemporalInstant(isolate, other_obj, method_name));

  auto this_ns = handle->instant()->raw()->epoch_nanoseconds();
  auto other_ns = other->instant()->raw()->epoch_nanoseconds();

  // equals() isn't exposed over FFI, but it's easy enough to do here
  // in the future we can use https://github.com/boa-dev/temporal/pull/311
  return isolate->factory()->ToBoolean(this_ns.high == other_ns.high &&
                                       this_ns.low == other_ns.low);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.round
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Round(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> round_to_obj) {
  static const char method_name[] = "Temporal.Instant.prototype.round";
  Factory* factory = isolate->factory();
  // 1. Let instant be the this value.
  // 2. Perform ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
  // 3. If roundTo is undefined, then
  if (IsUndefined(*round_to_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_TYPE_ERROR(kRoundToMissing));
  }
  DirectHandle<JSReceiver> round_to;
  // 4. If Type(roundTo) is String, then
  if (IsString(*round_to_obj)) {
    // TODO(415359720) This could be done more efficiently, if we had better
    // GetStringOption APIs
    // a. Let paramString be roundTo.
    DirectHandle<String> param_string = Cast<String>(round_to_obj);
    // b. Set roundTo to ! OrdinaryObjectCreate(null).
    round_to = factory->NewJSObjectWithNullProto();
    // c. Perform ! CreateDataPropertyOrThrow(roundTo, "smallestUnit",
    // paramString).
    CHECK(JSReceiver::CreateDataProperty(isolate, round_to,
                                         factory->smallestUnit_string(),
                                         param_string, Just(kThrowOnError))
              .FromJust());
  } else {
    // a. Set roundTo to ? GetOptionsObject(roundTo).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, round_to,
        GetOptionsObject(isolate, round_to_obj, method_name));
  }

  // 7. Let roundingIncrement be ? GetRoundingIncrementOption(roundTo).
  uint32_t rounding_increment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, round_to));

  // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, round_to,
                                      RoundingMode::HalfExpand, method_name));

  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo,
  // "smallestUnit", required).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, round_to, isolate->factory()->smallestUnit_string(),
          DefaultValue::kRequired, method_name));
  // 10. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  auto options = temporal_rs::RoundingOptions{.largest_unit = std::nullopt,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment};

  auto rounded = handle->instant()->raw()->round(options);
  return ConstructRustWrappingType<JSTemporalInstant>(isolate,
                                                      std::move(rounded));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.epochmilliseconds
MaybeDirectHandle<Number> JSTemporalInstant::EpochMilliseconds(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle) {
  int64_t ms = handle->instant()->raw()->epoch_milliseconds();

  return isolate->factory()->NewNumberFromInt64(ms);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.epochnanoseconds
MaybeDirectHandle<BigInt> JSTemporalInstant::EpochNanoseconds(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle) {

  return temporal::I128ToBigInt(isolate,
                                handle->instant()->raw()->epoch_nanoseconds());
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalInstant::ToZonedDateTimeISO(Isolate* isolate,
                                      DirectHandle<JSTemporalInstant> instant,
                                      DirectHandle<Object> time_zone_obj) {
  // 3. Let timeZone be ? ToTemporalTimeZoneIdentifier(temporalTimeZoneLike).
  temporal_rs::TimeZone time_zone;
  MOVE_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      temporal::ToTemporalTimeZoneIdentifier(isolate, time_zone_obj));

  return ConstructRustWrappingType<JSTemporalZonedDateTime>(
      isolate, instant->instant()->raw()->to_zoned_date_time_iso_with_provider(
                   time_zone, TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tostring
MaybeDirectHandle<String> JSTemporalInstant::ToString(
    Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
    DirectHandle<Object> options_obj) {
  static const char method_name[] = "Temporal.Instant.prototype.toString";

  // 3. Set options to ? GetOptionsObject(options).
  DirectHandle<JSReceiver> options;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, options, GetOptionsObject(isolate, options_obj, method_name));

  // 5. Let digits be ?
  // GetTemporalFractionalSecondDigitsOption(resolvedOptions).

  temporal_rs::Precision digits;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, digits,
                             temporal::GetTemporalFractionalSecondDigitsOption(
                                 isolate, options, method_name));

  // 6. Let roundingMode be ? GetRoundingModeOption(resolvedOptions, trunc).

  RoundingMode rounding_mode;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(isolate, options, RoundingMode::Trunc,
                                      method_name));

  // 7. Let smallestUnit be ? GetTemporalUnitValuedOption(resolvedOptions,
  // "smallestUnit", unset).
  std::optional<Unit> smallest_unit;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, smallest_unit,
      temporal::GetTemporalUnitValuedOption(
          isolate, options, isolate->factory()->smallestUnit_string(),
          DefaultValue::kUnset, method_name));

  // 8. Let timeZone be ? Get(resolvedOptions, "timeZone").
  DirectHandle<Object> time_zone;
  //  Let val be ? Get(temporalDurationLike, fieldName).
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, time_zone,
      JSReceiver::GetProperty(isolate, options,
                              isolate->factory()->timeZone_string()));

  // 9. Perform ? ValidateTemporalUnitValue(smallestUnit, time).
  RETURN_ON_EXCEPTION(isolate, temporal::ValidateTemporalUnitValue(
                                   isolate, smallest_unit, UnitGroup::kTime));

  // 10. If smallestUnit is hour, throw a RangeError exception.
  if (smallest_unit == Unit::Hour) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kPropertyValueOutOfRange,
                                  isolate->factory()->smallestUnit_string()));
  }

  std::optional<temporal_rs::TimeZone> tz_for_passing;
  // 11. If timeZone is not undefined, then
  if (!IsUndefined(*time_zone)) {
    temporal_rs::TimeZone rust_time_zone;
    // a. Set timeZone to ? ToTemporalTimeZoneIdentifier(timeZone).
    MOVE_RETURN_ON_EXCEPTION(
        isolate, rust_time_zone,
        temporal::ToTemporalTimeZoneIdentifier(isolate, time_zone));
    tz_for_passing = rust_time_zone;
  }

  auto rust_options = temporal_rs::ToStringRoundingOptions{
      .precision = digits,
      .smallest_unit = smallest_unit,
      .rounding_mode = rounding_mode,
  };

  return temporal::GenericTemporalToString(
      isolate, instant, &temporal_rs::Instant::to_ixdtf_string_with_provider,
      tz_for_passing, std::move(rust_options), std::ref(TimeZoneProvider()));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tojson
MaybeDirectHandle<String> JSTemporalInstant::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalInstant> instant) {
  return temporal::GenericTemporalToString(
      isolate, instant, &temporal_rs::Instant::to_ixdtf_string_with_provider,
      std::nullopt, std::move(temporal::kToStringAuto),
      std::ref(TimeZoneProvider()));
}

MaybeDirectHandle<String> JSTemporalInstant::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
#ifdef V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sub-temporal.instant.prototype.tolocalestring
  return JSDateTimeFormat::TemporalToLocaleString(
      isolate, instant, locales, options,
      JSDateTimeFormat::RequiredOption::kAny,
      JSDateTimeFormat::DefaultsOption::kAll,
      "Temporal.Instant.prototype.toLocaleString");
#else   // V8_INTL_SUPPORT
  // https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.tolocalestring
  return temporal::GenericTemporalToString(
      isolate, instant, &temporal_rs::Instant::to_ixdtf_string_with_provider,
      std::nullopt, std::move(temporal::kToStringAuto),
      std::ref(TimeZoneProvider()));
#endif  // V8_INTL_SUPPORT
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.add
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Add(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> temporal_duration_like) {
  static const char method_name[] = "Temporal.Duration.prototype.add";

  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, other_duration,
                             temporal::ToTemporalDuration(
                                 isolate, temporal_duration_like, method_name));

  auto added =
      handle->instant()->raw()->add(*other_duration->duration()->raw());

  return ConstructRustWrappingType<JSTemporalInstant>(isolate,
                                                      std::move(added));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.subtract
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> temporal_duration_like) {
  static const char method_name[] = "Temporal.Duration.prototype.subtract";

  DirectHandle<JSTemporalDuration> other_duration;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, other_duration,
                             temporal::ToTemporalDuration(
                                 isolate, temporal_duration_like, method_name));

  auto subtracted =
      handle->instant()->raw()->subtract(*other_duration->duration()->raw());

  return ConstructRustWrappingType<JSTemporalInstant>(isolate,
                                                      std::move(subtracted));
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalInstant::Until(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.Instant.prototype.until";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::Instant::until, UnitGroup::kTime, Unit::Nanosecond,
      handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.instant.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalInstant::Since(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  static const char method_name[] = "Temporal.Instant.prototype.since";

  return temporal::GenericDifferenceTemporal(
      isolate, &temporal_rs::Instant::since, UnitGroup::kTime, Unit::Nanosecond,
      handle, other, options, method_name);
}

// https://tc39.es/proposal-temporal/#sec-temporal.now.timezoneid
V8_WARN_UNUSED_RESULT MaybeDirectHandle<String> JSTemporalNowTimeZoneId(
    Isolate* isolate) {
  // 1. Return SystemTimeZoneIdentifier().
#ifdef V8_INTL_SUPPORT
  auto tz_str = Intl::DefaultTimeZone();

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(tz_str);
  return builder.Finish().ToHandleChecked();
#else
  return isolate->factory()->UTC_string();
#endif
}

}  // namespace v8::internal
