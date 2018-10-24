// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_
#define V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_

#include "src/objects-inl.h"
#include "src/objects/js-relative-time-format.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Base relative time format accessors.
ACCESSORS(JSRelativeTimeFormat, locale, String, kLocaleOffset)
ACCESSORS(JSRelativeTimeFormat, icu_formatter,
          Managed<icu::RelativeDateTimeFormatter>, kICUFormatterOffset)
SMI_ACCESSORS(JSRelativeTimeFormat, flags, kFlagsOffset)

// TODO(ftang): Use bit field accessor for style and numeric later.

inline void JSRelativeTimeFormat::set_style(Style style) {
  DCHECK_GT(Style::COUNT, style);
  int hints = flags();
  hints = StyleBits::update(hints, style);
  set_flags(hints);
}

inline JSRelativeTimeFormat::Style JSRelativeTimeFormat::style() const {
  return StyleBits::decode(flags());
}

inline void JSRelativeTimeFormat::set_numeric(Numeric numeric) {
  DCHECK_GT(Numeric::COUNT, numeric);
  int hints = flags();
  hints = NumericBits::update(hints, numeric);
  set_flags(hints);
}

inline JSRelativeTimeFormat::Numeric JSRelativeTimeFormat::numeric() const {
  return NumericBits::decode(flags());
}

CAST_ACCESSOR(JSRelativeTimeFormat);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_
