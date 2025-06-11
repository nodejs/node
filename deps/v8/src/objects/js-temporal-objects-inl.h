// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_TEMPORAL_OBJECTS_INL_H_
#define V8_OBJECTS_JS_TEMPORAL_OBJECTS_INL_H_

#include "src/objects/js-temporal-objects.h"
// Include the non-inl header before the rest of the headers.

#include "src/api/api-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-temporal-objects-tq-inl.inc"

#define TEMPORAL_INLINE_GETTER_SETTER(T, data, field, lower, upper, B) \
  inline void T::set_##field(int32_t field) {                          \
    DCHECK_GE(upper, field);                                           \
    DCHECK_LE(lower, field);                                           \
    int hints = data();                                                \
    hints = B##Bits::update(hints, field);                             \
    set_##data(hints);                                                 \
  }                                                                    \
  inline int32_t T::field() const {                                    \
    int32_t v = B##Bits::decode(data());                               \
    DCHECK_GE(upper, v);                                               \
    DCHECK_LE(lower, v);                                               \
    return v;                                                          \
  }

#define TEMPORAL_INLINE_SIGNED_GETTER_SETTER(T, data, field, lower, upper, B) \
  inline void T::set_##field(int32_t field) {                                 \
    DCHECK_GE(upper, field);                                                  \
    DCHECK_LE(lower, field);                                                  \
    int hints = data();                                                       \
    /* Mask out unrelated bits */                                             \
    field &= (static_cast<uint32_t>(int32_t{-1})) ^                           \
             (static_cast<uint32_t>(int32_t{-1}) << B##Bits::kSize);          \
    hints = B##Bits::update(hints, field);                                    \
    set_##data(hints);                                                        \
  }                                                                           \
  inline int32_t T::field() const {                                           \
    int32_t v = B##Bits::decode(data());                                      \
    /* Restore bits for negative values based on the MSB in that field */     \
    v |= ((int32_t{1} << (B##Bits::kSize - 1) & v)                            \
              ? (static_cast<uint32_t>(int32_t{-1}) << B##Bits::kSize)        \
              : 0);                                                           \
    DCHECK_GE(upper, v);                                                      \
    DCHECK_LE(lower, v);                                                      \
    return v;                                                                 \
  }

#define TEMPORAL_DATE_INLINE_GETTER_SETTER(T, data)                        \
  TEMPORAL_INLINE_SIGNED_GETTER_SETTER(T, data, iso_year, -271821, 275760, \
                                       IsoYear)                            \
  TEMPORAL_INLINE_GETTER_SETTER(T, data, iso_month, 1, 12, IsoMonth)       \
  TEMPORAL_INLINE_GETTER_SETTER(T, data, iso_day, 1, 31, IsoDay)

#define TEMPORAL_TIME_INLINE_GETTER_SETTER(T, data1, data2)             \
  TEMPORAL_INLINE_GETTER_SETTER(T, data1, iso_hour, 0, 23, IsoHour)     \
  TEMPORAL_INLINE_GETTER_SETTER(T, data1, iso_minute, 0, 59, IsoMinute) \
  TEMPORAL_INLINE_GETTER_SETTER(T, data1, iso_second, 0, 59, IsoSecond) \
  TEMPORAL_INLINE_GETTER_SETTER(T, data2, iso_millisecond, 0, 999,      \
                                IsoMillisecond)                         \
  TEMPORAL_INLINE_GETTER_SETTER(T, data2, iso_microsecond, 0, 999,      \
                                IsoMicrosecond)                         \
  TEMPORAL_INLINE_GETTER_SETTER(T, data2, iso_nanosecond, 0, 999, IsoNanosecond)

TEMPORAL_DATE_INLINE_GETTER_SETTER(JSTemporalPlainDate, year_month_day)
TEMPORAL_DATE_INLINE_GETTER_SETTER(JSTemporalPlainDateTime, year_month_day)
TEMPORAL_TIME_INLINE_GETTER_SETTER(JSTemporalPlainDateTime, hour_minute_second,
                                   second_parts)
TEMPORAL_DATE_INLINE_GETTER_SETTER(JSTemporalPlainMonthDay, year_month_day)
TEMPORAL_TIME_INLINE_GETTER_SETTER(JSTemporalPlainTime, hour_minute_second,
                                   second_parts)
TEMPORAL_DATE_INLINE_GETTER_SETTER(JSTemporalPlainYearMonth, year_month_day)

TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalDuration)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalInstant)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainDate)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainDateTime)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainMonthDay)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainTime)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainYearMonth)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalTimeZone)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalZonedDateTime)

BOOL_ACCESSORS(JSTemporalTimeZone, flags, is_offset, IsOffsetBit::kShift)

// temporal_rs object getters
ACCESSORS(JSTemporalInstant, instant, Tagged<Managed<temporal_rs::Instant>>,
          kInstantOffset)

// Special handling of sign
TEMPORAL_INLINE_SIGNED_GETTER_SETTER(JSTemporalTimeZone, flags,
                                     offset_milliseconds, -24 * 60 * 60 * 1000,
                                     24 * 60 * 60 * 1000,
                                     OffsetMillisecondsOrTimeZoneIndex)

TEMPORAL_INLINE_SIGNED_GETTER_SETTER(JSTemporalTimeZone, details,
                                     offset_sub_milliseconds, -1000000, 1000000,
                                     OffsetSubMilliseconds)

BIT_FIELD_ACCESSORS(JSTemporalTimeZone, flags,
                    offset_milliseconds_or_time_zone_index,
                    JSTemporalTimeZone::OffsetMillisecondsOrTimeZoneIndexBits)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_TEMPORAL_OBJECTS_INL_H_
