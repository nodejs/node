// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CELL_H_
#define V8_OBJECTS_CELL_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/cell-tq.inc"

class Cell : public TorqueGeneratedCell<Cell, HeapObject> {
 public:
  static constexpr int kValueOffset = TorqueGeneratedClass::kMaybeValueOffset;

  inline Address ValueAddress() { return address() + kValueOffset; }

  using TorqueGeneratedCell::maybe_value;
  DECL_RELAXED_GETTER(maybe_value, Tagged<MaybeObject>)

  // These strong accessors are for the cases when a Cell is known to contain
  // only Objects.
  DECL_ACCESSORS(value, Tagged<Object>)
  DECL_RELAXED_GETTER(value, Tagged<Object>)

  using BodyDescriptor =
      FixedWeakBodyDescriptor<kMaybeValueOffset, kSize, kSize>;

  TQ_OBJECT_CONSTRUCTORS(Cell)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CELL_H_
