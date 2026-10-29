// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLECTION_ITERATOR_INL_H_
#define V8_OBJECTS_JS_COLLECTION_ITERATOR_INL_H_

#include "src/objects/js-collection-iterator.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/objects-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<Object> JSCollectionIterator::table() const { return table_.load(); }
void JSCollectionIterator::set_table(Tagged<Object> value,
                                     WriteBarrierMode mode) {
  table_.store(this, value, mode);
}

Tagged<Object> JSCollectionIterator::index() const { return index_.load(); }
void JSCollectionIterator::set_index(Tagged<Object> value,
                                     WriteBarrierMode mode) {
  index_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_ITERATOR_INL_H_
