// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_ARRAY_INL_H_
#define V8_OBJECTS_EMBEDDER_DATA_ARRAY_INL_H_

#include "src/objects/embedder-data-array.h"

//#include "src/objects-inl.h"  // Needed for write barriers
#include "src/objects/maybe-object-inl.h"
#include "src/objects/slots.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(EmbedderDataArray)

SMI_ACCESSORS(EmbedderDataArray, length, kLengthOffset)

OBJECT_CONSTRUCTORS_IMPL(EmbedderDataArray, HeapObject)

Address EmbedderDataArray::slots_start() {
  return FIELD_ADDR(this, OffsetOfElementAt(0));
}

Address EmbedderDataArray::slots_end() {
  return FIELD_ADDR(this, OffsetOfElementAt(length()));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_ARRAY_INL_H_
