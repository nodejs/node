// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MICROTASK_INL_H_
#define V8_OBJECTS_MICROTASK_INL_H_

#include "src/objects/microtask.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/foreign-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Microtask, Struct)
OBJECT_CONSTRUCTORS_IMPL(CallbackTask, Microtask)
OBJECT_CONSTRUCTORS_IMPL(CallableTask, Microtask)

CAST_ACCESSOR2(Microtask)
CAST_ACCESSOR2(CallbackTask)
CAST_ACCESSOR2(CallableTask)

ACCESSORS2(CallableTask, callable, JSReceiver, kCallableOffset)
ACCESSORS2(CallableTask, context, Context, kContextOffset)

ACCESSORS2(CallbackTask, callback, Foreign, kCallbackOffset)
ACCESSORS2(CallbackTask, data, Foreign, kDataOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_INL_H_
