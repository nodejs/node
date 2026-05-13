// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_LIST_FORMAT_INL_H_
#define V8_OBJECTS_JS_LIST_FORMAT_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-list-format.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSListFormat::locale() const { return locale_.load(); }
void JSListFormat::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

Tagged<Managed<icu::ListFormatter>> JSListFormat::icu_formatter() const {
  return Cast<Managed<icu::ListFormatter>>(icu_formatter_.load());
}
void JSListFormat::set_icu_formatter(Tagged<Managed<icu::ListFormatter>> value,
                                     WriteBarrierMode mode) {
  icu_formatter_.store(this, value, mode);
}

int JSListFormat::flags() const { return flags_.load().value(); }
void JSListFormat::set_flags(int value) {
  flags_.store(this, Smi::FromInt(value));
}

inline void JSListFormat::set_style(Style style) {
  DCHECK(StyleBits::is_valid(style));
  int hints = flags();
  hints = StyleBits::update(hints, style);
  set_flags(hints);
}

inline JSListFormat::Style JSListFormat::style() const {
  return StyleBits::decode(flags());
}

inline void JSListFormat::set_type(Type type) {
  DCHECK(TypeBits::is_valid(type));
  int hints = flags();
  hints = TypeBits::update(hints, type);
  set_flags(hints);
}

inline JSListFormat::Type JSListFormat::type() const {
  return TypeBits::decode(flags());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LIST_FORMAT_INL_H_
