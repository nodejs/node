// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_
#define V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-break-iterator.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-function.h"
#include "src/objects/objects-inl.h"  // For CastTraits<Managed<T>>.
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<String> JSV8BreakIterator::locale() const { return locale_.load(); }

void JSV8BreakIterator::set_locale(Tagged<String> value,
                                   WriteBarrierMode mode) {
  locale_.store(this, value, mode);
}

Tagged<Managed<IcuBreakIteratorWithText>>
JSV8BreakIterator::icu_iterator_with_text() const {
  return Cast<Managed<IcuBreakIteratorWithText>>(
      icu_iterator_with_text_.load());
}

void JSV8BreakIterator::set_icu_iterator_with_text(
    Tagged<Managed<IcuBreakIteratorWithText>> value, WriteBarrierMode mode) {
  icu_iterator_with_text_.store(this, value, mode);
}

#define DEFINE_BOUND_ACCESSORS(name)                                         \
  Tagged<UnionOf<Undefined, JSFunction>> JSV8BreakIterator::name() const {   \
    return name##_.load();                                                   \
  }                                                                          \
  void JSV8BreakIterator::set_##name(                                        \
      Tagged<UnionOf<Undefined, JSFunction>> value, WriteBarrierMode mode) { \
    name##_.store(this, value, mode);                                        \
  }
DEFINE_BOUND_ACCESSORS(bound_adopt_text)
DEFINE_BOUND_ACCESSORS(bound_first)
DEFINE_BOUND_ACCESSORS(bound_next)
DEFINE_BOUND_ACCESSORS(bound_current)
DEFINE_BOUND_ACCESSORS(bound_break_type)
#undef DEFINE_BOUND_ACCESSORS

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_BREAK_ITERATOR_INL_H_
