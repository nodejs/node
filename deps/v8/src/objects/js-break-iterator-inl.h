// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_
#define V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_

#include "src/objects/js-break-iterator.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSV8BreakIterator, JSObject)

inline JSV8BreakIterator::Type JSV8BreakIterator::type() const {
  return static_cast<JSV8BreakIterator::Type>(raw_type());
}

inline void JSV8BreakIterator::set_type(Type type) {
  set_raw_type(static_cast<int>(type));
}

ACCESSORS(JSV8BreakIterator, locale, String, kLocaleOffset)
ACCESSORS(JSV8BreakIterator, break_iterator, Managed<icu::BreakIterator>,
          kBreakIteratorOffset)
ACCESSORS(JSV8BreakIterator, unicode_string, Managed<icu::UnicodeString>,
          kUnicodeStringOffset)
ACCESSORS(JSV8BreakIterator, bound_adopt_text, Object, kBoundAdoptTextOffset)
ACCESSORS(JSV8BreakIterator, bound_first, Object, kBoundFirstOffset)
ACCESSORS(JSV8BreakIterator, bound_next, Object, kBoundNextOffset)
ACCESSORS(JSV8BreakIterator, bound_current, Object, kBoundCurrentOffset)
ACCESSORS(JSV8BreakIterator, bound_break_type, Object, kBoundBreakTypeOffset)

SMI_ACCESSORS(JSV8BreakIterator, raw_type, kBreakIteratorTypeOffset)

CAST_ACCESSOR(JSV8BreakIterator)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_
