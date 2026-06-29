// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_TEMPORAL_OBJECTS_INL_H_
#define V8_OBJECTS_JS_TEMPORAL_OBJECTS_INL_H_

#include "src/objects/js-temporal-objects.h"
// Include the non-inl header before the rest of the headers.

#include "src/api/api-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/objects-inl.h"

// Rust includes to transitively include
#include "temporal_rs/Duration.hpp"
#include "temporal_rs/Instant.hpp"
#include "temporal_rs/PlainDate.hpp"
#include "temporal_rs/PlainDateTime.hpp"
#include "temporal_rs/PlainMonthDay.hpp"
#include "temporal_rs/PlainTime.hpp"
#include "temporal_rs/PlainYearMonth.hpp"
#include "temporal_rs/ZonedDateTime.hpp"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-temporal-objects-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalDuration)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalInstant)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainDate)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainDateTime)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainMonthDay)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainTime)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalPlainYearMonth)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSTemporalZonedDateTime)


// temporal_rs object getters
ACCESSORS(JSTemporalInstant, instant, Tagged<Managed<temporal_rs::Instant>>,
          kInstantOffset)
ACCESSORS(JSTemporalDuration, duration, Tagged<Managed<temporal_rs::Duration>>,
          kDurationOffset)
ACCESSORS(JSTemporalPlainDate, date, Tagged<Managed<temporal_rs::PlainDate>>,
          kDateOffset)
ACCESSORS(JSTemporalPlainDateTime, date_time,
          Tagged<Managed<temporal_rs::PlainDateTime>>, kDateTimeOffset)
ACCESSORS(JSTemporalPlainMonthDay, month_day,
          Tagged<Managed<temporal_rs::PlainMonthDay>>, kMonthDayOffset)
ACCESSORS(JSTemporalPlainTime, time, Tagged<Managed<temporal_rs::PlainTime>>,
          kTimeOffset)
ACCESSORS(JSTemporalPlainYearMonth, year_month,
          Tagged<Managed<temporal_rs::PlainYearMonth>>, kYearMonthOffset)
ACCESSORS(JSTemporalZonedDateTime, zoned_date_time,
          Tagged<Managed<temporal_rs::ZonedDateTime>>, kZonedDateTimeOffset)

#define DEFINE_CTOR_HELPER(JSType, camel_case)                              \
  inline DirectHandle<JSFunction> JSType::GetConstructorTarget(             \
      Isolate* isolate) {                                                   \
    return DirectHandle<JSFunction>(                                        \
        Cast<JSFunction>(                                                   \
            isolate->native_context()->temporal_##camel_case##_function()), \
        isolate);                                                           \
  }

DEFINE_CTOR_HELPER(JSTemporalDuration, duration)
DEFINE_CTOR_HELPER(JSTemporalInstant, instant)
DEFINE_CTOR_HELPER(JSTemporalPlainDate, plain_date)
DEFINE_CTOR_HELPER(JSTemporalPlainDateTime, plain_date_time)
DEFINE_CTOR_HELPER(JSTemporalPlainMonthDay, plain_month_day)
DEFINE_CTOR_HELPER(JSTemporalPlainTime, plain_time)
DEFINE_CTOR_HELPER(JSTemporalPlainYearMonth, plain_year_month)
DEFINE_CTOR_HELPER(JSTemporalZonedDateTime, zoned_date_time)

// Paired with DECL_ACCESSORS_FOR_RUST_WRAPPER
// Can be omitted and overridden if needed.
#define DEFINE_ACCESSORS_FOR_RUST_WRAPPER(field, JSType)        \
  inline void JSType::initialize_with_wrapped_rust_value(       \
      Tagged<Managed<JSType::RustType>> handle) {               \
    this->set_##field(handle);                                  \
  }                                                             \
  inline const JSType::RustType& JSType::wrapped_rust() const { \
    return *this->field()->raw();                               \
  }

DEFINE_ACCESSORS_FOR_RUST_WRAPPER(instant, JSTemporalInstant)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(duration, JSTemporalDuration)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(date, JSTemporalPlainDate)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(date_time, JSTemporalPlainDateTime)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(month_day, JSTemporalPlainMonthDay)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(time, JSTemporalPlainTime)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(year_month, JSTemporalPlainYearMonth)
DEFINE_ACCESSORS_FOR_RUST_WRAPPER(zoned_date_time, JSTemporalZonedDateTime)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_TEMPORAL_OBJECTS_INL_H_
