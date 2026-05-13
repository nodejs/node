// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_OBJECT_INL_H_
#define V8_OBJECTS_HEAP_OBJECT_INL_H_

#include "src/objects/heap-object.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/ptr-compr-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/map-word-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

MapWord HeapObject::map_word(RelaxedLoadTag tag) const {
  return MapField::Relaxed_Load_Map_Word(this);
}

MapWord HeapObject::map_word(AcquireLoadTag tag) const {
  return MapField::Acquire_Load_No_Unpack(this);
}

Tagged<Map> HeapObject::map() const { return map_word(kRelaxedLoad).ToMap(); }

DEF_ACQUIRE_GETTER(HeapObject, map, Tagged<Map>) {
  return map_word(kAcquireLoad).ToMap();
}

int HeapObject::Size() const { return SizeFromMap(map()); }

SafeHeapObjectSize HeapObject::SafeSize() const {
  return SafeSizeFromMap(map());
}

#define TYPE_CHECKER(type, ...)                                          \
  bool Is##type(Tagged<HeapObject> obj) {                                \
    Tagged<Map> map_object = obj->map();                                 \
    return InstanceTypeChecker::Is##type(map_object);                    \
  }

INSTANCE_TYPE_CHECKERS(TYPE_CHECKER)
#undef TYPE_CHECKER

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_OBJECT_INL_H_
