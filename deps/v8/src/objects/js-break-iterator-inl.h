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

inline void JSV8BreakIterator::set_type(Type type) {
  DCHECK_GT(JSV8BreakIterator::Type::COUNT, type);
  WRITE_FIELD(*this, kTypeOffset, Smi::FromInt(static_cast<int>(type)));
}

inline JSV8BreakIterator::Type JSV8BreakIterator::type() const {
  Object value = READ_FIELD(*this, kTypeOffset);
  return static_cast<JSV8BreakIterator::Type>(Smi::ToInt(value));
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

CAST_ACCESSOR(JSV8BreakIterator)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_
