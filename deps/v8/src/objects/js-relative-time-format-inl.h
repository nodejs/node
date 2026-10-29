// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_
#define V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-relative-time-format.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSRelativeTimeFormat::locale() const { return locale_.load(); }
void JSRelativeTimeFormat::set_locale(Tagged<String> value,
                                      WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

Tagged<String> JSRelativeTimeFormat::numberingSystem() const {
  return numberingSystem_.load();
}
void JSRelativeTimeFormat::set_numberingSystem(Tagged<String> value,
                                               WriteBarrierMode mode) {
  numberingSystem_.store(this, value, mode);
}

Tagged<Managed<icu::RelativeDateTimeFormatter>>
JSRelativeTimeFormat::icu_formatter() const {
  return Cast<Managed<icu::RelativeDateTimeFormatter>>(icu_formatter_.load());
}
void JSRelativeTimeFormat::set_icu_formatter(
    Tagged<Managed<icu::RelativeDateTimeFormatter>> value,
    WriteBarrierMode mode) {
  icu_formatter_.store(this, value, mode);
}

int JSRelativeTimeFormat::flags() const { return flags_.load().value(); }
void JSRelativeTimeFormat::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

inline void JSRelativeTimeFormat::set_numeric(Numeric numeric) {
  DCHECK(NumericBit::is_valid(numeric));
  int hints = flags();
  hints = NumericBit::update(hints, numeric);
  set_flags(hints);
}

inline JSRelativeTimeFormat::Numeric JSRelativeTimeFormat::numeric() const {
  return NumericBit::decode(flags());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_INL_H_
