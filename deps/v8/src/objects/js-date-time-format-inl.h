// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_
#define V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_

#include "src/objects/js-date-time-format.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSDateTimeFormat, JSObject)

ACCESSORS(JSDateTimeFormat, locale, String, kLocaleOffset)
ACCESSORS(JSDateTimeFormat, icu_locale, Managed<icu::Locale>, kIcuLocaleOffset)
ACCESSORS(JSDateTimeFormat, icu_simple_date_format,
          Managed<icu::SimpleDateFormat>, kIcuSimpleDateFormatOffset)
ACCESSORS(JSDateTimeFormat, icu_date_interval_format,
          Managed<icu::DateIntervalFormat>, kIcuDateIntervalFormatOffset)
ACCESSORS(JSDateTimeFormat, bound_format, Object, kBoundFormatOffset)
SMI_ACCESSORS(JSDateTimeFormat, flags, kFlagsOffset)

inline void JSDateTimeFormat::set_hour_cycle(HourCycle hour_cycle) {
  int hints = flags();
  hints = HourCycleBits::update(hints, hour_cycle);
  set_flags(hints);
}

inline JSDateTimeFormat::HourCycle JSDateTimeFormat::hour_cycle() const {
  return HourCycleBits::decode(flags());
}

inline void JSDateTimeFormat::set_date_style(
    JSDateTimeFormat::DateTimeStyle date_style) {
  int hints = flags();
  hints = DateStyleBits::update(hints, date_style);
  set_flags(hints);
}

inline JSDateTimeFormat::DateTimeStyle JSDateTimeFormat::date_style() const {
  return DateStyleBits::decode(flags());
}

inline void JSDateTimeFormat::set_time_style(
    JSDateTimeFormat::DateTimeStyle time_style) {
  int hints = flags();
  hints = TimeStyleBits::update(hints, time_style);
  set_flags(hints);
}

inline JSDateTimeFormat::DateTimeStyle JSDateTimeFormat::time_style() const {
  return TimeStyleBits::decode(flags());
}

CAST_ACCESSOR(JSDateTimeFormat)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_
