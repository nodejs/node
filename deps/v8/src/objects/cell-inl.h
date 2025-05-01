// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CELL_INL_H_
#define V8_OBJECTS_CELL_INL_H_

#include "src/objects/cell.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/cell-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(Cell)

DEF_RELAXED_GETTER(Cell, value, Tagged<Object>) {
  return TaggedField<Object, kValueOffset>::Relaxed_Load(cage_base, *this);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CELL_INL_H_
