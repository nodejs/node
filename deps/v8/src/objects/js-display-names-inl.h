// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_DISPLAY_NAMES_INL_H_
#define V8_OBJECTS_JS_DISPLAY_NAMES_INL_H_

#include "src/objects/js-display-names.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

ACCESSORS(JSDisplayNames, internal, Managed<DisplayNamesInternal>,
          kInternalOffset)
OBJECT_CONSTRUCTORS_IMPL(JSDisplayNames, JSObject)

// Base display names accessors.
SMI_ACCESSORS(JSDisplayNames, flags, kFlagsOffset)

CAST_ACCESSOR(JSDisplayNames)

inline void JSDisplayNames::set_style(Style style) {
  DCHECK_GE(StyleBits::kMax, style);
  set_flags(StyleBits::update(flags(), style));
}

inline JSDisplayNames::Style JSDisplayNames::style() const {
  return StyleBits::decode(flags());
}

inline void JSDisplayNames::set_fallback(Fallback fallback) {
  DCHECK_GE(FallbackBits::kMax, fallback);
  int hints = flags();
  hints = FallbackBits::update(hints, fallback);
  set_flags(hints);
}

inline JSDisplayNames::Fallback JSDisplayNames::fallback() const {
  return FallbackBits::decode(flags());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DISPLAY_NAMES_INL_H_
