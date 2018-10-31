// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ALLOCATION_BUILDER_INL_H_
#define V8_COMPILER_ALLOCATION_BUILDER_INL_H_

#include "src/compiler/allocation-builder.h"

#include "src/compiler/access-builder.h"
#include "src/objects/map-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

void AllocationBuilder::AllocateContext(int length, Handle<Map> map) {
  DCHECK(map->instance_type() >= AWAIT_CONTEXT_TYPE &&
         map->instance_type() <= WITH_CONTEXT_TYPE);
  int size = FixedArray::SizeFor(length);
  Allocate(size, NOT_TENURED, Type::OtherInternal());
  Store(AccessBuilder::ForMap(), map);
  Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
}

// Compound allocation of a FixedArray.
void AllocationBuilder::AllocateArray(int length, Handle<Map> map,
                                      PretenureFlag pretenure) {
  DCHECK(map->instance_type() == FIXED_ARRAY_TYPE ||
         map->instance_type() == FIXED_DOUBLE_ARRAY_TYPE);
  int size = (map->instance_type() == FIXED_ARRAY_TYPE)
                 ? FixedArray::SizeFor(length)
                 : FixedDoubleArray::SizeFor(length);
  Allocate(size, pretenure, Type::OtherInternal());
  Store(AccessBuilder::ForMap(), map);
  Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ALLOCATION_BUILDER_INL_H_
