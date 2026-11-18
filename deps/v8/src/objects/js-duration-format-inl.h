// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DURATION_FORMAT_INL_H_
#define V8_OBJECTS_JS_DURATION_FORMAT_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-duration-format.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

JSDurationFormat::StyleFlags JSDurationFormat::style_flags() const {
  return style_flags_.load();
}
void JSDurationFormat::set_style_flags(StyleFlags value) {
  style_flags_.store(value);
}

JSDurationFormat::DisplayFlags JSDurationFormat::display_flags() const {
  return display_flags_.load();
}
void JSDurationFormat::set_display_flags(DisplayFlags value) {
  display_flags_.store(value);
}

Tagged<Managed<icu::Locale>> JSDurationFormat::icu_locale() const {
  return Cast<Managed<icu::Locale>>(icu_locale_.load());
}
void JSDurationFormat::set_icu_locale(Tagged<Managed<icu::Locale>> value,
                                      WriteBarrierMode mode) {
  icu_locale_.store(this, value, mode);
}

#define IMPL_INLINE_SETTER_GETTER(T, n, B, f, M)   \
  inline void JSDurationFormat::set_##n(T value) { \
    DCHECK(B::is_valid(value));                    \
    DCHECK_GE(T::M, value);                        \
    set_##f(f().with_##n(value));                  \
  }                                                \
  inline JSDurationFormat::T JSDurationFormat::n() const { return f().n(); }

#define IMPL_INLINE_DISPLAY_SETTER_GETTER(f, R)                                \
  IMPL_INLINE_SETTER_GETTER(Display, f##_display, DisplayFlags::R##DisplayBit, \
                            display_flags, kAlways)

#define IMPL_INLINE_FIELD_STYLE3_SETTER_GETTER(f, R)                         \
  IMPL_INLINE_SETTER_GETTER(FieldStyle, f##_style, StyleFlags::R##StyleBits, \
                            style_flags, kStyle3Max)

#define IMPL_INLINE_FIELD_STYLE4_SETTER_GETTER(f, R)                         \
  IMPL_INLINE_SETTER_GETTER(FieldStyle, f##_style, StyleFlags::R##StyleBits, \
                            style_flags, kStyle4Max)

#define IMPL_INLINE_FIELD_STYLE5_SETTER_GETTER(f, R)                         \
  IMPL_INLINE_SETTER_GETTER(FieldStyle, f##_style, StyleFlags::R##StyleBits, \
                            style_flags, kStyle5Max)

IMPL_INLINE_DISPLAY_SETTER_GETTER(years, Years)
IMPL_INLINE_DISPLAY_SETTER_GETTER(months, Months)
IMPL_INLINE_DISPLAY_SETTER_GETTER(weeks, Weeks)
IMPL_INLINE_DISPLAY_SETTER_GETTER(days, Days)
IMPL_INLINE_DISPLAY_SETTER_GETTER(hours, Hours)
IMPL_INLINE_DISPLAY_SETTER_GETTER(minutes, Minutes)
IMPL_INLINE_DISPLAY_SETTER_GETTER(seconds, Seconds)
IMPL_INLINE_DISPLAY_SETTER_GETTER(milliseconds, Milliseconds)
IMPL_INLINE_DISPLAY_SETTER_GETTER(microseconds, Microseconds)
IMPL_INLINE_DISPLAY_SETTER_GETTER(nanoseconds, Nanoseconds)

IMPL_INLINE_SETTER_GETTER(Style, style, StyleFlags::StyleBits, style_flags,
                          kDigital)
IMPL_INLINE_SETTER_GETTER(Separator, separator, StyleFlags::SeparatorBits,
                          style_flags, kMax)

IMPL_INLINE_FIELD_STYLE3_SETTER_GETTER(years, Years)
IMPL_INLINE_FIELD_STYLE3_SETTER_GETTER(months, Months)
IMPL_INLINE_FIELD_STYLE3_SETTER_GETTER(weeks, Weeks)
IMPL_INLINE_FIELD_STYLE3_SETTER_GETTER(days, Days)
IMPL_INLINE_FIELD_STYLE5_SETTER_GETTER(hours, Hours)
IMPL_INLINE_FIELD_STYLE5_SETTER_GETTER(minutes, Minutes)
IMPL_INLINE_FIELD_STYLE5_SETTER_GETTER(seconds, Seconds)
IMPL_INLINE_FIELD_STYLE4_SETTER_GETTER(milliseconds, Milliseconds)
IMPL_INLINE_FIELD_STYLE4_SETTER_GETTER(microseconds, Microseconds)
IMPL_INLINE_FIELD_STYLE4_SETTER_GETTER(nanoseconds, Nanoseconds)

#undef IMPL_INLINE_SETTER_GETTER
#undef IMPL_INLINE_DISPLAY_SETTER_GETTER
#undef IMPL_INLINE_FIELD_STYLE3_SETTER_GETTER
#undef IMPL_INLINE_FIELD_STYLE5_SETTER_GETTER

inline void JSDurationFormat::set_fractional_digits(int32_t digits) {
  DCHECK((0 <= digits && digits <= 9) || digits == kUndefinedFractionalDigits);
  set_display_flags(display_flags().with_fractional_digits(digits));
}
inline int32_t JSDurationFormat::fractional_digits() const {
  int32_t v = display_flags().fractional_digits();
  DCHECK((0 <= v && v <= 9) || v == kUndefinedFractionalDigits);
  return v;
}

Tagged<Managed<icu::number::LocalizedNumberFormatter>>
JSDurationFormat::icu_number_formatter() const {
  return Cast<Managed<icu::number::LocalizedNumberFormatter>>(
      icu_number_formatter_.load());
}
void JSDurationFormat::set_icu_number_formatter(
    Tagged<Managed<icu::number::LocalizedNumberFormatter>> value,
    WriteBarrierMode mode) {
  icu_number_formatter_.store(this, value, mode);
}
}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DURATION_FORMAT_INL_H_
