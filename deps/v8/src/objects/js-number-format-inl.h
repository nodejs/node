// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_NUMBER_FORMAT_INL_H_
#define V8_OBJECTS_JS_NUMBER_FORMAT_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-number-format.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSNumberFormat::locale() const { return locale_.load(); }
void JSNumberFormat::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

Tagged<Managed<icu::number::LocalizedNumberFormatter>>
JSNumberFormat::icu_number_formatter() const {
  return Cast<Managed<icu::number::LocalizedNumberFormatter>>(
      icu_number_formatter_.load());
}
void JSNumberFormat::set_icu_number_formatter(
    Tagged<Managed<icu::number::LocalizedNumberFormatter>> value,
    WriteBarrierMode mode) {
  icu_number_formatter_.store(this, value, mode);
}

Tagged<UnionOf<JSFunction, Undefined>> JSNumberFormat::bound_format() const {
  return bound_format_.load();
}
void JSNumberFormat::set_bound_format(
    Tagged<UnionOf<JSFunction, Undefined>> value, WriteBarrierMode mode) {
  bound_format_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_NUMBER_FORMAT_INL_H_
