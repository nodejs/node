// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_INL_H_
#define V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_INL_H_

#include "src/objects/js-regexp-string-iterator.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-objects-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<JSReceiver> JSRegExpStringIterator::iterating_reg_exp() const {
  return iterating_reg_exp_.load();
}

void JSRegExpStringIterator::set_iterating_reg_exp(Tagged<JSReceiver> value,
                                                   WriteBarrierMode mode) {
  iterating_reg_exp_.store(this, value, mode);
}

Tagged<String> JSRegExpStringIterator::iterated_string() const {
  return iterated_string_.load();
}

void JSRegExpStringIterator::set_iterated_string(Tagged<String> value,
                                                 WriteBarrierMode mode) {
  iterated_string_.store(this, value, mode);
}

int JSRegExpStringIterator::flags() const { return flags_.load().value(); }

void JSRegExpStringIterator::set_flags(int value) {
  flags_.store(this, Smi::From31BitPattern(value));
}

BOOL_ACCESSORS(JSRegExpStringIterator, flags, done, DoneBit::kShift)
BOOL_ACCESSORS(JSRegExpStringIterator, flags, global, GlobalBit::kShift)
BOOL_ACCESSORS(JSRegExpStringIterator, flags, unicode, UnicodeBit::kShift)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_REGEXP_STRING_ITERATOR_INL_H_
