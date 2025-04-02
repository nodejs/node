// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOSHAFT_TYPES_INL_H_
#define V8_OBJECTS_TURBOSHAFT_TYPES_INL_H_

#include "src/heap/heap-write-barrier.h"
#include "src/objects/objects-inl.h"
#include "src/objects/turboshaft-types.h"
#include "src/torque/runtime-macro-shims.h"
#include "src/torque/runtime-support.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/turboshaft-types-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftType)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftWord32Type)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftWord32RangeType)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftWord32SetType)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftWord64Type)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftWord64RangeType)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftWord64SetType)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftFloat64Type)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftFloat64RangeType)
TQ_OBJECT_CONSTRUCTORS_IMPL(TurboshaftFloat64SetType)

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOSHAFT_TYPES_INL_H_
