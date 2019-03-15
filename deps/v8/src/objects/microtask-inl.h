// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MICROTASK_INL_H_
#define V8_OBJECTS_MICROTASK_INL_H_

#include "src/objects/microtask.h"

#include "src/contexts-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/js-objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Microtask, Struct)
OBJECT_CONSTRUCTORS_IMPL(CallbackTask, Microtask)
OBJECT_CONSTRUCTORS_IMPL(CallableTask, Microtask)

CAST_ACCESSOR(Microtask)
CAST_ACCESSOR(CallbackTask)
CAST_ACCESSOR(CallableTask)

ACCESSORS(CallableTask, callable, JSReceiver, kCallableOffset)
ACCESSORS(CallableTask, context, Context, kContextOffset)

ACCESSORS(CallbackTask, callback, Foreign, kCallbackOffset)
ACCESSORS(CallbackTask, data, Foreign, kDataOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_INL_H_
