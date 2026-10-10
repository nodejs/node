// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_LOCALE_INL_H_
#define V8_OBJECTS_JS_LOCALE_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-locale.h"
// Include the non-inl header before the rest of the headers.

#include "src/api/api-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<Managed<icu::Locale>> JSLocale::icu_locale() const {
  return Cast<Managed<icu::Locale>>(icu_locale_.load());
}

void JSLocale::set_icu_locale(Tagged<Managed<icu::Locale>> value,
                              WriteBarrierMode mode) {
  icu_locale_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LOCALE_INL_H_
