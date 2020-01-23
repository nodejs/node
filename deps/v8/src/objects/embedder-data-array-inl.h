// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_EMBEDDER_DATA_ARRAY_INL_H_
#define V8_OBJECTS_EMBEDDER_DATA_ARRAY_INL_H_

#include "src/objects/embedder-data-array.h"

#include "src/objects/instance-type-inl.h"
#include "src/objects/maybe-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_SMI_ACCESSORS(EmbedderDataArray, length)

TQ_OBJECT_CONSTRUCTORS_IMPL(EmbedderDataArray)

Address EmbedderDataArray::slots_start() {
  return FIELD_ADDR(*this, OffsetOfElementAt(0));
}

Address EmbedderDataArray::slots_end() {
  return FIELD_ADDR(*this, OffsetOfElementAt(length()));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_EMBEDDER_DATA_ARRAY_INL_H_
