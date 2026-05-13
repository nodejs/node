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

Tagged<MaybeObject> Cell::maybe_value() const { return maybe_value_.load(); }

Tagged<MaybeObject> Cell::maybe_value(RelaxedLoadTag) const {
  return maybe_value_.Relaxed_Load();
}

void Cell::set_maybe_value(Tagged<MaybeObject> value, WriteBarrierMode mode) {
  maybe_value_.store(this, value, mode);
}

Tagged<Object> Cell::value() const {
  Tagged<MaybeObject> v = maybe_value();
  DCHECK(v.IsObject());
  return v.GetHeapObjectOrSmi();
}

Tagged<Object> Cell::value(RelaxedLoadTag) const {
  Tagged<MaybeObject> v = maybe_value(kRelaxedLoad);
  DCHECK(v.IsObject());
  return v.GetHeapObjectOrSmi();
}

void Cell::set_value(Tagged<Object> value, WriteBarrierMode mode) {
  set_maybe_value(value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CELL_INL_H_
