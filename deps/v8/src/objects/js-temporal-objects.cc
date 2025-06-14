// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-temporal-objects.h"

#include <optional>
#include <set>

#include "src/common/globals.h"
#include "src/date/date.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-temporal-helpers.h"
#include "src/objects/js-temporal-objects-inl.h"
#include "src/objects/managed-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/option-utils.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/string-set.h"
#include "src/strings/string-builder-inl.h"
#include "src/temporal/temporal-parser.h"
#include "third_party/rust/chromium_crates_io/vendor/temporal_capi-v0_0/bindings/cpp/temporal_rs/I128Nanoseconds.hpp"
#include "third_party/rust/chromium_crates_io/vendor/temporal_capi-v0_0/bindings/cpp/temporal_rs/Instant.hpp"
#ifdef TEMPORAL_CAPI_VERSION_0_0_6
#include "third_party/rust/chromium_crates_io/vendor/temporal_capi-v0_0/bindings/cpp/temporal_rs/TemporalUnit.hpp"
#else  // TEMPORAL_CAPI_VERSION_0_0_6
#include "third_party/rust/chromium_crates_io/vendor/temporal_capi-v0_0/bindings/cpp/temporal_rs/Unit.hpp"
#endif  // TEMPORAL_CAPI_VERSION_0_0_6
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#include "src/objects/js-date-time-format.h"
#include "unicode/calendar.h"
#include "unicode/unistr.h"
#endif  // V8_INTL_SUPPORT

namespace v8::internal {

namespace {

#ifdef TEMPORAL_CAPI_VERSION_0_0_6
using temporal_rs::TemporalRoundingMode;
using temporal_rs::TemporalUnit;
#else   // TEMPORAL_CAPI_VERSION_0_0_6
using TemporalRoundingMode = temporal_rs::RoundingMode;
using TemporalUnit = temporal_rs::Unit;
#endif  // TEMPORAL_CAPI_VERSION_0_0_6

/**
 * This header declare the Abstract Operations defined in the
 * Temporal spec with the enum and struct for them.
 */

// Struct


using temporal::DateRecord;
using temporal::DateTimeRecord;
using temporal::TimeRecord;

struct DateRecordWithCalendar {
  DateRecord date;
  DirectHandle<Object> calendar;  // String or Undefined
};

struct TimeRecordWithCalendar {
  TimeRecord time;
  DirectHandle<Object> calendar;  // String or Undefined
};

struct TimeZoneRecord {
  bool z;
  DirectHandle<Object> offset_string;  // String or Undefined
  DirectHandle<Object> name;           // String or Undefined
};

struct DateTimeRecordWithCalendar {
  DateRecord date;
  TimeRecord time;
  TimeZoneRecord time_zone;
  DirectHandle<Object> calendar;  // String or Undefined
};

struct InstantRecord {
  DateRecord date;
  TimeRecord time;
  DirectHandle<Object> offset_string;  // String or Undefined
};

using temporal::DurationRecord;
using temporal::TimeDurationRecord;

struct DurationRecordWithRemainder {
  DurationRecord record;
  double remainder;
};

// #sec-temporal-date-duration-records
struct DateDurationRecord {
  double years;
  double months;
  double weeks;
  double days;
  // #sec-temporal-createdatedurationrecord
  static Maybe<DateDurationRecord> Create(Isolate* isolate, double years,
                                          double months, double weeks,
                                          double days);
};

// Options


// #sec-temporal-totemporaldisambiguation
enum class Disambiguation { kCompatible, kEarlier, kLater, kReject };

// #sec-temporal-totemporaloverflow
enum class ShowOverflow { kConstrain, kReject };
// #sec-temporal-toshowcalendaroption
enum class ShowCalendar { kAuto, kAlways, kNever };

// #table-temporal-unsigned-rounding-modes
enum class UnsignedRoundingMode {
  kInfinity,
  kZero,
  kHalfInfinity,
  kHalfZero,
  kHalfEven
};

enum class Precision { k0, k1, k2, k3, k4, k5, k6, k7, k8, k9, kAuto, kMinute };

enum class MatchBehaviour { kMatchExactly, kMatchMinutes };

// #sec-temporal-gettemporalunit
enum class UnitGroup {
  kDate,
  kTime,
  kDateTime,
};


// #sec-temporal-interpretisodatetimeoffset
enum class OffsetBehaviour { kOption, kExact, kWall };

// For internal temporal_rs errors that should not occur
#define NEW_TEMPORAL_INTERNAL_ERROR() \
  NewTypeError(MessageTemplate::kTemporalRsError)

#define ORDINARY_CREATE_FROM_CONSTRUCTOR(obj, target, new_target, T)           \
  DirectHandle<JSReceiver> new_target_receiver = Cast<JSReceiver>(new_target); \
  DirectHandle<Map> map;                                                       \
  ASSIGN_RETURN_ON_EXCEPTION(                                                  \
      isolate, map,                                                            \
      JSFunction::GetDerivedMap(isolate, target, new_target_receiver));        \
  DirectHandle<T> object =                                                     \
      Cast<T>(isolate->factory()->NewFastOrSlowJSObjectFromMap(map));

#define THROW_INVALID_RANGE(T) \
  THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR());

#define CONSTRUCTOR(name)                                                      \
  DirectHandle<JSFunction>(                                                    \
      Cast<JSFunction>(                                                        \
          isolate->context()->native_context()->temporal_##name##_function()), \
      isolate)

// Helper function to construct a JSType that wraps a RustType
template <typename JSType>
MaybeDirectHandle<JSType> ConstructRustWrappingType(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    diplomat::result<std::unique_ptr<typename JSType::RustType>,
                     temporal_rs::TemporalError>&& rust_result) {
  if (rust_result.is_err()) {
    auto err = std::move(rust_result).err().value();
    switch (err.kind) {
      case temporal_rs::ErrorKind::Type:
        THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR());
        break;
      case temporal_rs::ErrorKind::Range:
        THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR());
        break;
      case temporal_rs::ErrorKind::Syntax:
      case temporal_rs::ErrorKind::Assert:
      case temporal_rs::ErrorKind::Generic:
      default:
        // These cases shouldn't happen; the spec doesn't currently trigger
        // these errors
        THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INTERNAL_ERROR());
    }
    return MaybeDirectHandle<JSTemporalInstant>();
  }

  // Managed requires shared ownership
  std::shared_ptr<temporal_rs::Instant> rust_shared =
      std::move(rust_result).ok().value();

  DirectHandle<Managed<temporal_rs::Instant>> managed =
      Managed<temporal_rs::Instant>::From(isolate, 0, rust_shared);

  ORDINARY_CREATE_FROM_CONSTRUCTOR(object, target, new_target, JSType)
  object->initialize_with_wrapped_rust_value(*managed);
  return object;
}

}  // namespace

// Paired with DECL_ACCESSORS_FOR_RUST_WRAPPER
// Can be omitted and overridden if needed.
#define DEFINE_ACCESSORS_FOR_RUST_WRAPPER(field, JSType)  \
  inline void JSType::initialize_with_wrapped_rust_value( \
      Tagged<Managed<JSType::RustType>> handle) {         \
    this->set_##field(handle);                            \
  }

DEFINE_ACCESSORS_FOR_RUST_WRAPPER(instant, JSTemporalInstant)

namespace temporal {
// #sec-temporal-gettemporalunitvaluedoption
// In the spec text, the extraValues is defined as an optional argument of
// "a List of ECMAScript language values". Most of the caller does not pass in
// value for extraValues, which is represented by the default
// TemporalUnit::NotPresent. For the three places in the spec text calling
// GetTemporalUnit with an extraValues argument:
// << "day" >> is passed in as in the algorithm of
//   Temporal.PlainDateTime.prototype.round() and
//   Temporal.ZonedDateTime.prototype.round();
// << "auto" >> is passed in as in the algorithm of
// Temporal.Duration.prototype.round().
// Therefore we can simply use a Unit of three possible value, the default
// TemporalUnit::NotPresent, TemporalUnit::Day, and TemporalUnit::Auto to cover
// all the possible value for extraValues.
std::optional<TemporalUnit> GetTemporalUnit(
    Isolate* isolate, DirectHandle<JSReceiver> normalized_options,
    const char* key, UnitGroup unit_group,
    std::optional<TemporalUnit> default_value, bool default_is_required,
    const char* method_name,
    std::optional<TemporalUnit> extra_values = std::nullopt) {
  std::vector<const char*> str_values;
  std::vector<std::optional<TemporalUnit>> enum_values;
  switch (unit_group) {
    case UnitGroup::kDate:
      if (default_value == TemporalUnit::Auto ||
          extra_values == TemporalUnit::Auto) {
        str_values = {"year",  "month",  "week",  "day", "auto",
                      "years", "months", "weeks", "days"};
        enum_values = {
            TemporalUnit::Year,  TemporalUnit::Month, TemporalUnit::Week,
            TemporalUnit::Day,   TemporalUnit::Auto,  TemporalUnit::Year,
            TemporalUnit::Month, TemporalUnit::Week,  TemporalUnit::Day};
      } else {
        DCHECK(default_value == std::nullopt ||
               default_value == TemporalUnit::Year ||
               default_value == TemporalUnit::Month ||
               default_value == TemporalUnit::Week ||
               default_value == TemporalUnit::Day);
        str_values = {"year",  "month",  "week",  "day",
                      "years", "months", "weeks", "days"};
        enum_values = {TemporalUnit::Year, TemporalUnit::Month,
                       TemporalUnit::Week, TemporalUnit::Day,
                       TemporalUnit::Year, TemporalUnit::Month,
                       TemporalUnit::Week, TemporalUnit::Day};
      }
      break;
    case UnitGroup::kTime:
      if (default_value == TemporalUnit::Auto ||
          extra_values == TemporalUnit::Auto) {
        str_values = {"hour",        "minute",       "second",
                      "millisecond", "microsecond",  "nanosecond",
                      "auto",        "hours",        "minutes",
                      "seconds",     "milliseconds", "microseconds",
                      "nanoseconds"};
        enum_values = {TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond,
                       TemporalUnit::Auto,        TemporalUnit::Hour,
                       TemporalUnit::Minute,      TemporalUnit::Second,
                       TemporalUnit::Millisecond, TemporalUnit::Microsecond,
                       TemporalUnit::Nanosecond};
      } else if (default_value == TemporalUnit::Day ||
                 extra_values == TemporalUnit::Day) {
        str_values = {"hour",        "minute",       "second",
                      "millisecond", "microsecond",  "nanosecond",
                      "day",         "hours",        "minutes",
                      "seconds",     "milliseconds", "microseconds",
                      "nanoseconds", "days"};
        enum_values = {TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond,
                       TemporalUnit::Day,         TemporalUnit::Hour,
                       TemporalUnit::Minute,      TemporalUnit::Second,
                       TemporalUnit::Millisecond, TemporalUnit::Microsecond,
                       TemporalUnit::Nanosecond,  TemporalUnit::Day};
      } else {
        DCHECK(default_value == std::nullopt ||
               default_value == TemporalUnit::Hour ||
               default_value == TemporalUnit::Minute ||
               default_value == TemporalUnit::Second ||
               default_value == TemporalUnit::Millisecond ||
               default_value == TemporalUnit::Microsecond ||
               default_value == TemporalUnit::Nanosecond);
        str_values = {"hour",         "minute",       "second",
                      "millisecond",  "microsecond",  "nanosecond",
                      "hours",        "minutes",      "seconds",
                      "milliseconds", "microseconds", "nanoseconds"};
        enum_values = {TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond,
                       TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond};
      }
      break;
    case UnitGroup::kDateTime:
      if (default_value == TemporalUnit::Auto ||
          extra_values == TemporalUnit::Auto) {
        str_values = {"year",         "month",        "week",
                      "day",          "hour",         "minute",
                      "second",       "millisecond",  "microsecond",
                      "nanosecond",   "auto",         "years",
                      "months",       "weeks",        "days",
                      "hours",        "minutes",      "seconds",
                      "milliseconds", "microseconds", "nanoseconds"};
        enum_values = {TemporalUnit::Year,        TemporalUnit::Month,
                       TemporalUnit::Week,        TemporalUnit::Day,
                       TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond,
                       TemporalUnit::Auto,        TemporalUnit::Year,
                       TemporalUnit::Month,       TemporalUnit::Week,
                       TemporalUnit::Day,         TemporalUnit::Hour,
                       TemporalUnit::Minute,      TemporalUnit::Second,
                       TemporalUnit::Millisecond, TemporalUnit::Microsecond,
                       TemporalUnit::Nanosecond};
      } else {
        str_values = {
            "year",        "month",        "week",         "day",
            "hour",        "minute",       "second",       "millisecond",
            "microsecond", "nanosecond",   "years",        "months",
            "weeks",       "days",         "hours",        "minutes",
            "seconds",     "milliseconds", "microseconds", "nanoseconds"};
        enum_values = {TemporalUnit::Year,        TemporalUnit::Month,
                       TemporalUnit::Week,        TemporalUnit::Day,
                       TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond,
                       TemporalUnit::Year,        TemporalUnit::Month,
                       TemporalUnit::Week,        TemporalUnit::Day,
                       TemporalUnit::Hour,        TemporalUnit::Minute,
                       TemporalUnit::Second,      TemporalUnit::Millisecond,
                       TemporalUnit::Microsecond, TemporalUnit::Nanosecond};
      }
      break;
  }

  // 4. If default is required, then
  if (default_is_required) default_value = std::nullopt;
  // a. Let defaultValue be undefined.
  // 5. Else,
  // a. Let defaultValue be default.
  // b. If defaultValue is not undefined and singularNames does not contain
  // defaultValue, then i. Append defaultValue to singularNames.

  // 9. Let value be ? GetOption(normalizedOptions, key, "string",
  // allowedValues, defaultValue).
  std::optional<TemporalUnit> value;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      GetStringOption<std::optional<TemporalUnit>>(isolate, normalized_options,
                                                   key, method_name, str_values,
                                                   enum_values, default_value),
      std::nullopt);

  // 10. If value is undefined and default is required, throw a RangeError
  // exception.
  if (default_is_required && value == std::nullopt) {
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate,
        NewRangeError(
            MessageTemplate::kValueOutOfRange,
            isolate->factory()->undefined_value(),
            isolate->factory()->NewStringFromAsciiChecked(method_name),
            isolate->factory()->NewStringFromAsciiChecked(key)),
        std::nullopt);
  }
  // 12. Return value.

  return value;
}

// #sec-temporal-getroundingincrementoption
Maybe<uint32_t> GetRoundingIncrementOption(
    Isolate* isolate, DirectHandle<JSReceiver> normalized_options) {
  double value;

  // 1. Let value be ? Get(options, "roundingIncrement").
  // 2. If value is undefined, return 1𝔽.
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      GetNumberOptionAsDouble(isolate, normalized_options,
                              isolate->factory()->roundingIncrement_string(),
                              1.0),
      Nothing<uint32_t>());

  // 3. Let integerIncrement be ? ToIntegerWithTruncation(value).
  if (!std::isfinite(value)) {
    // (ToIntegerWithTruncation) 2. If number is NaN, +∞𝔽 or -∞𝔽, throw a
    // RangeError exception.
    THROW_NEW_ERROR_RETURN_VALUE(
        isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR(), Nothing<uint32_t>());
  }
  // (ToIntegerWithTruncation) 3. Return truncate(ℝ(number)).
  uint32_t integer_increment = std::trunc(value);
  return Just(integer_increment);
}

// sec-temporal-getroundingmodeoption
Maybe<TemporalRoundingMode> GetRoundingModeOption(
    Isolate* isolate, DirectHandle<JSReceiver> options,
    TemporalRoundingMode fallback, const char* method_name) {
  // 1. Return ? GetOption(normalizedOptions, "roundingMode", "string", «
  // "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor", "halfExpand",
  // "halfTrunc", "halfEven" », fallback).

  return GetStringOption<TemporalRoundingMode>(
      isolate, options, "roundingMode", method_name,
      {"ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor",
       "halfExpand", "halfTrunc", "halfEven"},
      {TemporalRoundingMode::Ceil, TemporalRoundingMode::Floor,
       TemporalRoundingMode::Expand, TemporalRoundingMode::Trunc,
       TemporalRoundingMode::HalfCeil, TemporalRoundingMode::HalfFloor,
       TemporalRoundingMode::HalfExpand, TemporalRoundingMode::HalfTrunc,
       TemporalRoundingMode::HalfEven},
      fallback);
}

}  // namespace temporal

namespace temporal {

MaybeDirectHandle<JSTemporalPlainDateTime> CreateTemporalDateTime(
    Isolate* isolate, const DateTimeRecord& date_time) {
  UNIMPLEMENTED();
}
MaybeDirectHandle<JSTemporalTimeZone> CreateTemporalTimeZone(
    Isolate* isolate, DirectHandle<String> identifier) {
  UNIMPLEMENTED();
}
}  // namespace temporal

// #sec-temporal.duration
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> years,
    DirectHandle<Object> months, DirectHandle<Object> weeks,
    DirectHandle<Object> days, DirectHandle<Object> hours,
    DirectHandle<Object> minutes, DirectHandle<Object> seconds,
    DirectHandle<Object> milliseconds, DirectHandle<Object> microseconds,
    DirectHandle<Object> nanoseconds) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.compare
MaybeDirectHandle<Smi> JSTemporalDuration::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj, DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.from
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::From(
    Isolate* isolate, DirectHandle<Object> item) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.round
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Round(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> round_to_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.total
MaybeDirectHandle<Object> JSTemporalDuration::Total(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> total_of_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.with
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::With(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> temporal_duration_like) {
  UNIMPLEMENTED();
}

// #sec-get-temporal.duration.prototype.sign
MaybeDirectHandle<Smi> JSTemporalDuration::Sign(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  UNIMPLEMENTED();
}

// #sec-get-temporal.duration.prototype.blank
MaybeDirectHandle<Oddball> JSTemporalDuration::Blank(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.negated
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Negated(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.abs
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Abs(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.add
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Add(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.subtract
MaybeDirectHandle<JSTemporalDuration> JSTemporalDuration::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.tojson
MaybeDirectHandle<String> JSTemporalDuration::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalDuration::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.duration.prototype.tostring
MaybeDirectHandle<String> JSTemporalDuration::ToString(
    Isolate* isolate, DirectHandle<JSTemporalDuration> duration,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.timezone
MaybeDirectHandle<JSTemporalTimeZone> JSTemporalTimeZone::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> identifier_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.timezone
MaybeDirectHandle<JSTemporalTimeZone> JSTemporalTimeZone::Now(
    Isolate* isolate) {
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.getinstantfor
MaybeDirectHandle<JSTemporalInstant> JSTemporalTimeZone::GetInstantFor(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> date_time_obj, DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.getplaindatetimefor
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalTimeZone::GetPlainDateTimeFor(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> instant_obj, DirectHandle<Object> calendar_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.getnexttransition
MaybeDirectHandle<Object> JSTemporalTimeZone::GetNextTransition(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> starting_point_obj) {
  UNIMPLEMENTED();
}
// #sec-temporal.timezone.prototype.getprevioustransition
MaybeDirectHandle<Object> JSTemporalTimeZone::GetPreviousTransition(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> starting_point_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.getpossibleinstantsfor
MaybeDirectHandle<JSArray> JSTemporalTimeZone::GetPossibleInstantsFor(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> date_time_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.getoffsetnanosecondsfor
MaybeDirectHandle<Object> JSTemporalTimeZone::GetOffsetNanosecondsFor(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> instant_obj) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.getoffsetstringfor
MaybeDirectHandle<String> JSTemporalTimeZone::GetOffsetStringFor(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    DirectHandle<Object> instant_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.timezone.prototype.tostring
MaybeDirectHandle<Object> JSTemporalTimeZone::ToString(
    Isolate* isolate, DirectHandle<JSTemporalTimeZone> time_zone,
    const char* method_name) {
  UNIMPLEMENTED();
}

int32_t JSTemporalTimeZone::time_zone_index() const {
  DCHECK(is_offset() == false);
  return offset_milliseconds_or_time_zone_index();
}

int64_t JSTemporalTimeZone::offset_nanoseconds() const {
  TEMPORAL_ENTER_FUNC();
  DCHECK(is_offset());
  return static_cast<int64_t>(offset_milliseconds()) * 1000000 +
         static_cast<int64_t>(offset_sub_milliseconds());
}

void JSTemporalTimeZone::set_offset_nanoseconds(int64_t ns) {
  this->set_offset_milliseconds(static_cast<int32_t>(ns / 1000000));
  this->set_offset_sub_milliseconds(static_cast<int32_t>(ns % 1000000));
}

MaybeDirectHandle<String> JSTemporalTimeZone::id(Isolate* isolate) const {
  UNIMPLEMENTED();
}

MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_year_obj,
    DirectHandle<Object> iso_month_obj, DirectHandle<Object> iso_day_obj,
    DirectHandle<Object> calendar_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.compare
MaybeDirectHandle<Smi> JSTemporalPlainDate::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainDate::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> other_obj) {
  UNIMPLEMENTED();
}


// #sec-temporal.plaindate.prototype.toplainyearmonth
MaybeDirectHandle<JSTemporalPlainYearMonth>
JSTemporalPlainDate::ToPlainYearMonth(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.toplainmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainDate::ToPlainMonthDay(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.toplaindatetime
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDate::ToPlainDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_time_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.with
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_date_like_obj,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalPlainDate::ToZonedDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> item_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.add
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_duration_like,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.subtract
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> temporal_duration_like,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDate::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDate::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.now.plaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::Now(
    Isolate* isolate, DirectHandle<Object> calendar_like,
    DirectHandle<Object> temporal_time_zone_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.plaindateiso
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.from
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDate::From(
    Isolate* isolate, DirectHandle<Object> item,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}


// #sec-temporal.plaindate.prototype.getisofields
MaybeDirectHandle<JSReceiver> JSTemporalPlainDate::GetISOFields(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainDate::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindate.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainDate::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sup-temporal.plaindate.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalPlainDate::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDate> temporal_date,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal-createtemporaldatetime
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_year_obj,
    DirectHandle<Object> iso_month_obj, DirectHandle<Object> iso_day_obj,
    DirectHandle<Object> hour_obj, DirectHandle<Object> minute_obj,
    DirectHandle<Object> second_obj, DirectHandle<Object> millisecond_obj,
    DirectHandle<Object> microsecond_obj, DirectHandle<Object> nanosecond_obj,
    DirectHandle<Object> calendar_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.from
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::From(
    Isolate* isolate, DirectHandle<Object> item,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.compare
MaybeDirectHandle<Smi> JSTemporalPlainDateTime::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainDateTime::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> other_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.with
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_date_time_like_obj,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.withplaintime
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalPlainDateTime::WithPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> plain_time_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.toplainyearmonth
MaybeDirectHandle<JSTemporalPlainYearMonth>
JSTemporalPlainDateTime::ToPlainYearMonth(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.toplainmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay>
JSTemporalPlainDateTime::ToPlainMonthDay(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalPlainDateTime::ToZonedDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_time_zone_like,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}


// #sec-temporal.plaindatetime.prototype.withplaindate
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalPlainDateTime::WithPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_date_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainDateTime::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalPlainDateTime::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainDateTime::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.plaindatetime
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Now(
    Isolate* isolate, DirectHandle<Object> calendar_like,
    DirectHandle<Object> temporal_time_zone_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.plaindatetimeiso
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  UNIMPLEMENTED();
}


// #sec-temporal.plaindatetime.prototype.round
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Round(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> round_to_obj) {
  UNIMPLEMENTED();
}


// #sec-temporal.plaindatetime.prototype.add
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.subtract
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainDateTime::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDateTime::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainDateTime::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.getisofields
MaybeDirectHandle<JSReceiver> JSTemporalPlainDateTime::GetISOFields(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainDateTime::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaindatetime.prototype.toplaintime
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainDateTime::ToPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainDateTime> date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_month_obj,
    DirectHandle<Object> iso_day_obj, DirectHandle<Object> calendar_like,
    DirectHandle<Object> reference_iso_year_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.from
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::From(
    Isolate* isolate, DirectHandle<Object> item,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainMonthDay::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> other_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.prototype.with
MaybeDirectHandle<JSTemporalPlainMonthDay> JSTemporalPlainMonthDay::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> temporal_month_day,
    DirectHandle<Object> temporal_month_day_like_obj,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainMonthDay::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> item_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.prototype.getisofields
MaybeDirectHandle<JSReceiver> JSTemporalPlainMonthDay::GetISOFields(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainMonthDay::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainMonthDay::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainmonthday.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalPlainMonthDay::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainMonthDay> month_day,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

MaybeDirectHandle<JSTemporalPlainYearMonth>
JSTemporalPlainYearMonth::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> iso_year_obj,
    DirectHandle<Object> iso_month_obj, DirectHandle<Object> calendar_like,
    DirectHandle<Object> reference_iso_day_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.from
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::From(
    Isolate* isolate, DirectHandle<Object> item,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.compare
MaybeDirectHandle<Smi> JSTemporalPlainYearMonth::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainYearMonth::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> other_obj) {
  UNIMPLEMENTED();
}


// #sec-temporal.plainyearmonth.prototype.add
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.subtract
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainYearMonth::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainYearMonth::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.with
MaybeDirectHandle<JSTemporalPlainYearMonth> JSTemporalPlainYearMonth::With(
    Isolate* isolate,
    DirectHandle<JSTemporalPlainYearMonth> temporal_year_month,
    DirectHandle<Object> temporal_year_month_like_obj,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalPlainYearMonth::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> item_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.getisofields
MaybeDirectHandle<JSReceiver> JSTemporalPlainYearMonth::GetISOFields(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainYearMonth::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainYearMonth::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plainyearmonth.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalPlainYearMonth::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainYearMonth> year_month,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal-plaintime-constructor
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target, DirectHandle<Object> hour_obj,
    DirectHandle<Object> minute_obj, DirectHandle<Object> second_obj,
    DirectHandle<Object> millisecond_obj, DirectHandle<Object> microsecond_obj,
    DirectHandle<Object> nanosecond_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalPlainTime::ToZonedDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> item_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.compare
MaybeDirectHandle<Smi> JSTemporalPlainTime::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalPlainTime::Equals(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> other_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.round
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Round(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> round_to_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.with
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::With(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_time_like_obj,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.plaintimeiso
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.from
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::From(
    Isolate* isolate, DirectHandle<Object> item_obj,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.toplaindatetime
MaybeDirectHandle<JSTemporalPlainDateTime> JSTemporalPlainTime::ToPlainDateTime(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_date_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.add
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Add(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_duration_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.subtract
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalPlainTime::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> temporal_duration_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainTime::Until(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalPlainTime::Since(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.getisofields
MaybeDirectHandle<JSReceiver> JSTemporalPlainTime::GetISOFields(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.tojson
MaybeDirectHandle<String> JSTemporalPlainTime::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time) {
  UNIMPLEMENTED();
}

// #sup-temporal.plaintime.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalPlainTime::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.plaintime.prototype.tostring
MaybeDirectHandle<String> JSTemporalPlainTime::ToString(
    Isolate* isolate, DirectHandle<JSTemporalPlainTime> temporal_time,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    DirectHandle<Object> epoch_nanoseconds_obj,
    DirectHandle<Object> time_zone_like, DirectHandle<Object> calendar_like) {
  UNIMPLEMENTED();
}

// #sec-get-temporal.zoneddatetime.prototype.hoursinday
MaybeDirectHandle<Object> JSTemporalZonedDateTime::HoursInDay(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.from
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::From(
    Isolate* isolate, DirectHandle<Object> item,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.compare
MaybeDirectHandle<Smi> JSTemporalZonedDateTime::Compare(
    Isolate* isolate, DirectHandle<Object> one_obj,
    DirectHandle<Object> two_obj) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalZonedDateTime::Equals(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> other_obj) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}


// #sec-temporal.zoneddatetime.prototype.with
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::With(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> temporal_zoned_date_time_like_obj,
    DirectHandle<Object> options_obj) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}


// #sec-temporal.zoneddatetime.prototype.withplaindate
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalZonedDateTime::WithPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> plain_date_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.withplaintime
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalZonedDateTime::WithPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> plain_time_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.withtimezone
MaybeDirectHandle<JSTemporalZonedDateTime>
JSTemporalZonedDateTime::WithTimeZone(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> time_zone_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}


// #sec-temporal.zoneddatetime.prototype.toplainyearmonth
MaybeDirectHandle<JSTemporalPlainYearMonth>
JSTemporalZonedDateTime::ToPlainYearMonth(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.toplainmonthday
MaybeDirectHandle<JSTemporalPlainMonthDay>
JSTemporalZonedDateTime::ToPlainMonthDay(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.tojson
MaybeDirectHandle<String> JSTemporalZonedDateTime::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalZonedDateTime::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.tostring
MaybeDirectHandle<String> JSTemporalZonedDateTime::ToString(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> options_obj) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.zoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Now(
    Isolate* isolate, DirectHandle<Object> calendar_like,
    DirectHandle<Object> temporal_time_zone_like) {
  UNIMPLEMENTED();
}

// #sec-temporal.now.zoneddatetimeiso
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::NowISO(
    Isolate* isolate, DirectHandle<Object> temporal_time_zone_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.round
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Round(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> round_to_obj) {
  UNIMPLEMENTED();
}


// #sec-temporal.zoneddatetime.prototype.add
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Add(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}
// #sec-temporal.zoneddatetime.prototype.subtract
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time,
    DirectHandle<Object> temporal_duration_like, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}



// #sec-temporal.zoneddatetime.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalZonedDateTime::Until(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalZonedDateTime::Since(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.getisofields
MaybeDirectHandle<JSReceiver> JSTemporalZonedDateTime::GetISOFields(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.now.instant
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Now(Isolate* isolate) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds
MaybeDirectHandle<Object> JSTemporalZonedDateTime::OffsetNanoseconds(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-get-temporal.zoneddatetime.prototype.offset
MaybeDirectHandle<String> JSTemporalZonedDateTime::Offset(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.startofday
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalZonedDateTime::StartOfDay(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.toinstant
MaybeDirectHandle<JSTemporalInstant> JSTemporalZonedDateTime::ToInstant(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}


// #sec-temporal.zoneddatetime.prototype.toplaindate
MaybeDirectHandle<JSTemporalPlainDate> JSTemporalZonedDateTime::ToPlainDate(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.toplaintime
MaybeDirectHandle<JSTemporalPlainTime> JSTemporalZonedDateTime::ToPlainTime(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  UNIMPLEMENTED();
}

// #sec-temporal.zoneddatetime.prototype.toplaindatetime
MaybeDirectHandle<JSTemporalPlainDateTime>
JSTemporalZonedDateTime::ToPlainDateTime(
    Isolate* isolate, DirectHandle<JSTemporalZonedDateTime> zoned_date_time) {
  UNIMPLEMENTED();
}

namespace temporal {

// #sec-temporal-createtemporalinstant, but this also performs the validity
// check
MaybeDirectHandle<JSTemporalInstant> CreateTemporalInstantWithValidityCheck(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    DirectHandle<BigInt> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();
  if (epoch_nanoseconds->Words64Count() > 2) {
    // 3. If ! IsValidEpochNanoseconds(epochNanoseconds) is false, throw a
    // RangeError exception.
    // Most validation is performed by the Instant ctor.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR());
  }

  uint64_t words[2] = {0, 0};
  uint32_t word_count = 2;
  int sign_bit = 0;
  epoch_nanoseconds->ToWordsArray64(&sign_bit, &word_count, words);

  if (words[1] > std::numeric_limits<int64_t>::max()) {
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_RANGE_ERROR());
  }

  int64_t high = static_cast<int64_t>(words[1]);
  if (sign_bit == 1) {
    high = -high;
  }

  auto ns = temporal_rs::I128Nanoseconds{.high = high, .low = words[0]};

  return ConstructRustWrappingType<JSTemporalInstant>(
      isolate, target, new_target, temporal_rs::Instant::try_new(ns));
}

MaybeDirectHandle<JSTemporalInstant> CreateTemporalInstantWithValidityCheck(
    Isolate* isolate, DirectHandle<BigInt> epoch_nanoseconds) {
  TEMPORAL_ENTER_FUNC();
  return CreateTemporalInstantWithValidityCheck(
      isolate, CONSTRUCTOR(instant), CONSTRUCTOR(instant), epoch_nanoseconds);
}

}  // namespace temporal

// #sec-temporal.instant
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Constructor(
    Isolate* isolate, DirectHandle<JSFunction> target,
    DirectHandle<HeapObject> new_target,
    DirectHandle<Object> epoch_nanoseconds_obj) {
  TEMPORAL_ENTER_FUNC();
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

// #sec-temporal.instant.prototype.equals
MaybeDirectHandle<Oddball> JSTemporalInstant::Equals(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> other_obj) {
  TEMPORAL_ENTER_FUNC();

  UNIMPLEMENTED();
}

// #sec-temporal.instant.prototype.round
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Round(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> round_to_obj) {
  TEMPORAL_ENTER_FUNC();
  const char* method_name = "Temporal.Instant.prototype.round";
  Factory* factory = isolate->factory();
  // 1. Let instant be the this value.
  // 2. Perform ? RequireInternalSlot(instant, [[InitializedTemporalInstant]]).
  // 3. If roundTo is undefined, then
  if (IsUndefined(*round_to_obj)) {
    // a. Throw a TypeError exception.
    THROW_NEW_ERROR(isolate, NEW_TEMPORAL_INVALID_ARG_TYPE_ERROR());
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
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, rounding_increment,
      temporal::GetRoundingIncrementOption(isolate, round_to),
      DirectHandle<JSTemporalInstant>());

  // 8. Let roundingMode be ? GetRoundingModeOption(roundTo, half-expand).
  TemporalRoundingMode rounding_mode;
  MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, rounding_mode,
      temporal::GetRoundingModeOption(
          isolate, round_to, TemporalRoundingMode::HalfExpand, method_name),
      DirectHandle<JSTemporalInstant>());

  // 9. Let smallestUnit be ? GetTemporalUnitValuedOption(roundTo,
  // "smallestUnit", time, required
  std::optional<TemporalUnit> smallest_unit = temporal::GetTemporalUnit(
      isolate, round_to, "smallestUnit", UnitGroup::kTime, std::nullopt, true,
      method_name);
  // GetTemporalUnit returns an optional, not a Maybe, so we can't use
  // MAYBE_ASSIGN_RETURN_ON_EXCEPTION_VALUE
  RETURN_VALUE_IF_EXCEPTION(isolate, DirectHandle<JSTemporalInstant>());
  auto options = temporal_rs::RoundingOptions{.largest_unit = std::nullopt,
                                              .smallest_unit = smallest_unit,
                                              .rounding_mode = rounding_mode,
                                              .increment = rounding_increment};

  auto rounded = handle->instant()->raw()->round(options);
  return ConstructRustWrappingType<JSTemporalInstant>(
      isolate, CONSTRUCTOR(instant), CONSTRUCTOR(instant), std::move(rounded));
}

// #sec-temporal.instant.prototype.epochmilliseconds
MaybeDirectHandle<Number> JSTemporalInstant::EpochMilliseconds(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle) {
  TEMPORAL_ENTER_FUNC();
  int64_t ms = handle->instant()->raw()->epoch_milliseconds();

  return isolate->factory()->NewNumberFromInt64(ms);
}

// #sec-temporal.instant.prototype.epochnanoseconds
MaybeDirectHandle<BigInt> JSTemporalInstant::EpochNanoseconds(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle) {
  TEMPORAL_ENTER_FUNC();
  temporal_rs::I128Nanoseconds ns =
      handle->instant()->raw()->epoch_nanoseconds();
  uint64_t words[2];
  bool sign_bit;
  if (ns.high < 0) {
    sign_bit = true;
    words[1] = static_cast<uint64_t>(-ns.high);
  } else {
    sign_bit = false;
    words[1] = static_cast<uint64_t>(ns.high);
  }
  words[0] = ns.low;
  return BigInt::FromWords64(isolate, sign_bit, 2, words);
}

// #sec-temporal.instant.prototype.tozoneddatetime
MaybeDirectHandle<JSTemporalZonedDateTime> JSTemporalInstant::ToZonedDateTime(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> item_obj) {
  TEMPORAL_ENTER_FUNC();

  UNIMPLEMENTED();
}


// #sec-temporal.instant.prototype.tojson
MaybeDirectHandle<String> JSTemporalInstant::ToJSON(
    Isolate* isolate, DirectHandle<JSTemporalInstant> instant) {
  TEMPORAL_ENTER_FUNC();

  UNIMPLEMENTED();
}

// #sec-temporal.instant.prototype.tolocalestring
MaybeDirectHandle<String> JSTemporalInstant::ToLocaleString(
    Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
    DirectHandle<Object> locales, DirectHandle<Object> options) {
  UNIMPLEMENTED();
}

// #sec-temporal.instant.prototype.tostring
MaybeDirectHandle<String> JSTemporalInstant::ToString(
    Isolate* isolate, DirectHandle<JSTemporalInstant> instant,
    DirectHandle<Object> options_obj) {
  // TODO(42201538) this is an incorrect impl, exists solely so that the
  // interpreter doesn't keep hitting ToString() unimplementeds temporal_rs has
  // code for this, but it's missing from the CAPI
  // (https://github.com/boa-dev/temporal/pull/276)
  IncrementalStringBuilder builder(isolate);
  auto ns = instant->instant()->raw()->epoch_milliseconds();
  builder.AppendInt(static_cast<int>(ns));
  builder.AppendString("ms since epoch");
  return builder.Finish().ToHandleChecked();
}

// #sec-temporal.instant.prototype.add
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Add(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> temporal_duration_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.instant.prototype.subtract
MaybeDirectHandle<JSTemporalInstant> JSTemporalInstant::Subtract(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> temporal_duration_like) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.instant.prototype.until
MaybeDirectHandle<JSTemporalDuration> JSTemporalInstant::Until(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}

// #sec-temporal.instant.prototype.since
MaybeDirectHandle<JSTemporalDuration> JSTemporalInstant::Since(
    Isolate* isolate, DirectHandle<JSTemporalInstant> handle,
    DirectHandle<Object> other, DirectHandle<Object> options) {
  TEMPORAL_ENTER_FUNC();
  UNIMPLEMENTED();
}
namespace temporal {

// A simple convenient function to avoid the need to unnecessarily exposing
// the definition of enum Disambiguation.
MaybeDirectHandle<JSTemporalInstant> BuiltinTimeZoneGetInstantForCompatible(
    Isolate* isolate, DirectHandle<JSReceiver> time_zone,
    DirectHandle<JSTemporalPlainDateTime> date_time, const char* method_name) {
  UNIMPLEMENTED();
}

}  // namespace temporal
}  // namespace v8::internal
