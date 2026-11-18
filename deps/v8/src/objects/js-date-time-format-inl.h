// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_
#define V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-date-time-format.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSDateTimeFormat::locale() const { return locale_.load(); }
void JSDateTimeFormat::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

Tagged<Managed<icu::Locale>> JSDateTimeFormat::icu_locale() const {
  return Cast<Managed<icu::Locale>>(icu_locale_.load());
}
void JSDateTimeFormat::set_icu_locale(Tagged<Managed<icu::Locale>> value,
                                      WriteBarrierMode mode) {
  icu_locale_.store(this, value, mode);
}

Tagged<Managed<icu::SimpleDateFormat>>
JSDateTimeFormat::icu_simple_date_format() const {
  return Cast<Managed<icu::SimpleDateFormat>>(icu_simple_date_format_.load());
}
void JSDateTimeFormat::set_icu_simple_date_format(
    Tagged<Managed<icu::SimpleDateFormat>> value, WriteBarrierMode mode) {
  icu_simple_date_format_.store(this, value, mode);
}

Tagged<Managed<icu::DateIntervalFormat>>
JSDateTimeFormat::icu_date_interval_format() const {
  return Cast<Managed<icu::DateIntervalFormat>>(
      icu_date_interval_format_.load());
}
void JSDateTimeFormat::set_icu_date_interval_format(
    Tagged<Managed<icu::DateIntervalFormat>> value, WriteBarrierMode mode) {
  icu_date_interval_format_.store(this, value, mode);
}

Tagged<UnionOf<JSFunction, Undefined>> JSDateTimeFormat::bound_format() const {
  return bound_format_.load();
}
void JSDateTimeFormat::set_bound_format(
    Tagged<UnionOf<JSFunction, Undefined>> value, WriteBarrierMode mode) {
  bound_format_.store(this, value, mode);
}

JSDateTimeFormat::Flags JSDateTimeFormat::flags() const {
  return flags_.load();
}
void JSDateTimeFormat::set_flags(Flags value) { flags_.store(value); }

BIT_FIELD_ACCESSORS(JSDateTimeFormat, flags, has_to_locale_string_time_zone,
                    JSDateTimeFormat::Flags::HasToLocaleStringTimeZoneBit)

inline void JSDateTimeFormat::set_explicit_components_in_options(
    int32_t explicit_components_in_options) {
  set_flags(flags().with_explicit_components_in_options(
      explicit_components_in_options));
}

inline int32_t JSDateTimeFormat::explicit_components_in_options() const {
  return flags().explicit_components_in_options();
}

inline void JSDateTimeFormat::set_hour_cycle(HourCycle hour_cycle) {
  set_flags(flags().with_hour_cycle(hour_cycle));
}

inline JSDateTimeFormat::HourCycle JSDateTimeFormat::hour_cycle() const {
  return flags().hour_cycle();
}

inline void JSDateTimeFormat::set_date_style(
    JSDateTimeFormat::DateTimeStyle date_style) {
  set_flags(flags().with_date_style(date_style));
}

inline JSDateTimeFormat::DateTimeStyle JSDateTimeFormat::date_style() const {
  return flags().date_style();
}

inline void JSDateTimeFormat::set_time_style(
    JSDateTimeFormat::DateTimeStyle time_style) {
  set_flags(flags().with_time_style(time_style));
}

inline JSDateTimeFormat::DateTimeStyle JSDateTimeFormat::time_style() const {
  return flags().time_style();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DATE_TIME_FORMAT_INL_H_
