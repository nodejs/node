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

// temporal_rs object getters
#define DEFINE_TEMPORAL_ACCESSORS(JSType, field, RustType_)         \
  inline Tagged<Managed<RustType_>> JSType::field() const {         \
    return field##_.load();                                         \
  }                                                                 \
  inline void JSType::set_##field(Tagged<Managed<RustType_>> value, \
                                  WriteBarrierMode mode) {          \
    field##_.store(this, value, mode);                              \
  }

DEFINE_TEMPORAL_ACCESSORS(JSTemporalInstant, instant, temporal_rs::Instant)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalDuration, duration, temporal_rs::Duration)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalPlainDate, date, temporal_rs::PlainDate)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalPlainDateTime, date_time,
                          temporal_rs::PlainDateTime)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalPlainMonthDay, month_day,
                          temporal_rs::PlainMonthDay)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalPlainTime, time, temporal_rs::PlainTime)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalPlainYearMonth, year_month,
                          temporal_rs::PlainYearMonth)
DEFINE_TEMPORAL_ACCESSORS(JSTemporalZonedDateTime, zoned_date_time,
                          temporal_rs::ZonedDateTime)
#undef DEFINE_TEMPORAL_ACCESSORS

#define DEFINE_CTOR_HELPER(JSType, snake_case)                              \
  inline DirectHandle<JSFunction> JSType::GetConstructorTarget(             \
      Isolate* isolate) {                                                   \
    return DirectHandle<JSFunction>(                                        \
        Cast<JSFunction>(                                                   \
            isolate->native_context()->temporal_##snake_case##_function()), \
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
#define DEFINE_ACCESSORS_FOR_RUST_WRAPPER(field, JSType)               \
  inline void JSType::initialize_with_wrapped_rust_value(              \
      Tagged<Managed<JSType::RustType>> handle) {                      \
    this->set_##field(handle);                                         \
  }                                                                    \
  inline Managed<JSType::RustType>::Ptr JSType::wrapped_rust() const { \
    return this->field()->ptr();                                       \
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
