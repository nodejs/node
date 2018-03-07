// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_COLLECTION_INL_H_
#define V8_OBJECTS_JS_COLLECTION_INL_H_

#include "src/objects/js-collection.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

ACCESSORS(JSCollection, table, Object, kTableOffset)
ACCESSORS(JSCollectionIterator, table, Object, kTableOffset)
ACCESSORS(JSCollectionIterator, index, Object, kIndexOffset)

ACCESSORS(JSWeakCollection, table, Object, kTableOffset)
ACCESSORS(JSWeakCollection, next, Object, kNextOffset)

TYPE_CHECKER(JSMap, JS_MAP_TYPE)
TYPE_CHECKER(JSSet, JS_SET_TYPE)
TYPE_CHECKER(JSWeakMap, JS_WEAK_MAP_TYPE)
TYPE_CHECKER(JSWeakSet, JS_WEAK_SET_TYPE)

CAST_ACCESSOR(JSSet)
CAST_ACCESSOR(JSSetIterator)
CAST_ACCESSOR(JSMap)
CAST_ACCESSOR(JSMapIterator)
CAST_ACCESSOR(JSWeakCollection)
CAST_ACCESSOR(JSWeakMap)
CAST_ACCESSOR(JSWeakSet)

Object* JSMapIterator::CurrentValue() {
  OrderedHashMap* table(OrderedHashMap::cast(this->table()));
  int index = Smi::ToInt(this->index());
  Object* value = table->ValueAt(index);
  DCHECK(!value->IsTheHole(table->GetIsolate()));
  return value;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_COLLECTION_INL_H_
