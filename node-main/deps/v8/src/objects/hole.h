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

V8_OBJECT class Hole : public HeapObjectLayout {
 public:
  DECL_VERIFIER(Hole)
  DECL_PRINTER(Hole)
} V8_OBJECT_END;

template <>
struct ObjectTraits<Hole> {
  using BodyDescriptor = FixedBodyDescriptor<0, 0, sizeof(Hole)>;
};

#define DEFINE_HOLE_TYPE(Name, name, Root) \
  V8_OBJECT class Name : public Hole {     \
  } V8_OBJECT_END;

HOLE_LIST(DEFINE_HOLE_TYPE)
#undef DEFINE_HOLE_TYPE

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HOLE_H_
