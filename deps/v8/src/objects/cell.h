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

V8_OBJECT class Cell : public HeapObject {
 public:
  // [maybe_value]: field containing a possibly weak reference to an object.
  inline Tagged<MaybeObject> maybe_value() const;
  inline Tagged<MaybeObject> maybe_value(RelaxedLoadTag) const;
  inline void set_maybe_value(
      Tagged<MaybeObject> value,
      WriteBarrierMode mode = WriteBarrierMode::UPDATE_WRITE_BARRIER);

  // These strong accessors are for the cases when a Cell is known to contain
  // only Objects.
  inline Tagged<Object> value() const;
  inline Tagged<Object> value(RelaxedLoadTag) const;
  inline void set_value(
      Tagged<Object> value,
      WriteBarrierMode mode = WriteBarrierMode::UPDATE_WRITE_BARRIER);

  DECL_PRINTER(Cell)
  DECL_VERIFIER(Cell)

 public:
  TaggedMember<MaybeObject> maybe_value_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<Cell> {
  using BodyDescriptor = FixedWeakBodyDescriptor<offsetof(Cell, maybe_value_),
                                                 sizeof(Cell), sizeof(Cell)>;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CELL_H_
