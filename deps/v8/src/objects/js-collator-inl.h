// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLATOR_INL_H_
#define V8_OBJECTS_JS_COLLATOR_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-collator.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<Managed<icu::Collator>> JSCollator::icu_collator() const {
  return Cast<Managed<icu::Collator>>(icu_collator_.load());
}

void JSCollator::set_icu_collator(Tagged<Managed<icu::Collator>> value,
                                  WriteBarrierMode mode) {
  icu_collator_.store(this, value, mode);
}

Tagged<UnionOf<Undefined, JSFunction>> JSCollator::bound_compare() const {
  return bound_compare_.load();
}

void JSCollator::set_bound_compare(Tagged<UnionOf<Undefined, JSFunction>> value,
                                   WriteBarrierMode mode) {
  bound_compare_.store(this, value, mode);
}

Tagged<String> JSCollator::locale() const { return locale_.load(); }

void JSCollator::set_locale(Tagged<String> value, WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLATOR_INL_H_
