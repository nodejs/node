// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HOLE_H_
#define V8_OBJECTS_HOLE_H_

#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#define V8_CAN_UNMAP_HOLES_BOOL \
  (V8_STATIC_ROOTS_BOOL || V8_STATIC_ROOTS_GENERATION_BOOL)

V8_OBJECT class Hole : public HeapObjectLayout {
 public:
  DECL_VERIFIER(Hole)
  DECL_PRINTER(Hole)

  class BodyDescriptor;

 private:
#if V8_CAN_UNMAP_HOLES_BOOL
  friend class Heap;
  friend class Isolate;

  // TODO(leszeks): Make it smaller if able and needed.
  static constexpr int kPayloadSize = 64 * KB;
  // Payload should be a multiple of page size.
  static_assert(kPayloadSize % kMinimumOSPageSize == 0);

  char payload_[kPayloadSize];
#endif
} V8_OBJECT_END;

static_assert(V8_CAN_UNMAP_HOLES_BOOL ||
              sizeof(Hole) == sizeof(HeapObjectLayout));

#define DEFINE_HOLE_TYPE(Name, name, Root) \
  V8_OBJECT class Name : public Hole {     \
  } V8_OBJECT_END;

HOLE_LIST(DEFINE_HOLE_TYPE)
#undef DEFINE_HOLE_TYPE

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HOLE_H_
