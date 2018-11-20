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
ACCESSORS(JSRelativeTimeFormat, locale, String, kLocaleOffset);
ACCESSORS(JSRelativeTimeFormat, formatter, Foreign, kFormatterOffset);

// TODO(ftang): Use bit field accessor for style and numeric later.

inline void JSRelativeTimeFormat::set_style(Style style) {
  DCHECK_GT(Style::COUNT, style);
  int value = static_cast<int>(style);
  WRITE_FIELD(this, kStyleOffset, Smi::FromInt(value));
}

inline JSRelativeTimeFormat::Style JSRelativeTimeFormat::style() const {
  Object* value = READ_FIELD(this, kStyleOffset);
  int style = Smi::ToInt(value);
  DCHECK_LE(0, style);
  DCHECK_GT(static_cast<int>(Style::COUNT), style);
  return static_cast<Style>(style);
}

inline void JSRelativeTimeFormat::set_numeric(Numeric numeric) {
  DCHECK_GT(Numeric::COUNT, numeric);
  int value = static_cast<int>(numeric);
  WRITE_FIELD(this, kNumericOffset, Smi::FromInt(value));
}

inline JSRelativeTimeFormat::Numeric JSRelativeTimeFormat::numeric() const {
  Object* value = READ_FIELD(this, kNumericOffset);
  int numeric = Smi::ToInt(value);
  DCHECK_LE(0, numeric);
  DCHECK_GT(static_cast<int>(Numeric::COUNT), numeric);
  return static_cast<Numeric>(numeric);
}

CAST_ACCESSOR(JSRelativeTimeFormat);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_
