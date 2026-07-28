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

DEF_RELAXED_GETTER(Cell, maybe_value, Tagged<MaybeObject>) {
  return TaggedField<MaybeObject, kMaybeValueOffset>::Relaxed_Load(cage_base,
                                                                   *this);
}

DEF_GETTER(Cell, value, Tagged<Object>) {
  Tagged<MaybeObject> maybe_object = maybe_value();
  DCHECK(maybe_object.IsObject());
  return Tagged<Object>(maybe_object.ptr());
}
void Cell::set_value(Tagged<Object> value, WriteBarrierMode mode) {
  set_maybe_value(value, mode);
}

DEF_RELAXED_GETTER(Cell, value, Tagged<Object>) {
  Tagged<MaybeObject> maybe_object = maybe_value(kRelaxedLoad);
  DCHECK(maybe_object.IsObject());
  return Tagged<Object>(maybe_object.ptr());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CELL_INL_H_
