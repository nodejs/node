// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_COLLATOR_INL_H_
#define V8_OBJECTS_JS_COLLATOR_INL_H_

#include "src/objects-inl.h"
#include "src/objects/js-collator.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSCollator, JSObject)

ACCESSORS(JSCollator, icu_collator, Managed<icu::Collator>, kICUCollatorOffset)
ACCESSORS(JSCollator, bound_compare, Object, kBoundCompareOffset);

CAST_ACCESSOR(JSCollator);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLATOR_INL_H_
