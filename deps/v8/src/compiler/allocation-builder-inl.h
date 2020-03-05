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

void AllocationBuilder::AllocateContext(int variadic_part_length, MapRef map) {
  DCHECK(base::IsInRange(map.instance_type(), FIRST_CONTEXT_TYPE,
                         LAST_CONTEXT_TYPE));
  DCHECK_NE(NATIVE_CONTEXT_TYPE, map.instance_type());
  int size = Context::SizeFor(variadic_part_length);
  Allocate(size, AllocationType::kYoung, Type::OtherInternal());
  Store(AccessBuilder::ForMap(), map);
  STATIC_ASSERT(static_cast<int>(Context::kLengthOffset) ==
                static_cast<int>(FixedArray::kLengthOffset));
  Store(AccessBuilder::ForFixedArrayLength(),
        jsgraph()->Constant(variadic_part_length));
}

// Compound allocation of a FixedArray.
void AllocationBuilder::AllocateArray(int length, MapRef map,
                                      AllocationType allocation) {
  DCHECK(map.instance_type() == FIXED_ARRAY_TYPE ||
         map.instance_type() == FIXED_DOUBLE_ARRAY_TYPE);
  int size = (map.instance_type() == FIXED_ARRAY_TYPE)
                 ? FixedArray::SizeFor(length)
                 : FixedDoubleArray::SizeFor(length);
  Allocate(size, allocation, Type::OtherInternal());
  Store(AccessBuilder::ForMap(), map);
  Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ALLOCATION_BUILDER_INL_H_
