// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_GENERATOR_INL_H_
#define V8_OBJECTS_JS_GENERATOR_INL_H_

#include "src/objects/js-generator.h"

#include "src/objects-inl.h"  // Needed for write barriers

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(JSGeneratorObject)
CAST_ACCESSOR(JSAsyncGeneratorObject)

ACCESSORS(JSGeneratorObject, function, JSFunction, kFunctionOffset)
ACCESSORS(JSGeneratorObject, context, Context, kContextOffset)
ACCESSORS(JSGeneratorObject, receiver, Object, kReceiverOffset)
ACCESSORS(JSGeneratorObject, input_or_debug_pos, Object, kInputOrDebugPosOffset)
SMI_ACCESSORS(JSGeneratorObject, resume_mode, kResumeModeOffset)
SMI_ACCESSORS(JSGeneratorObject, continuation, kContinuationOffset)
ACCESSORS(JSGeneratorObject, parameters_and_registers, FixedArray,
          kParametersAndRegistersOffset)

bool JSGeneratorObject::is_suspended() const {
  DCHECK_LT(kGeneratorExecuting, 0);
  DCHECK_LT(kGeneratorClosed, 0);
  return continuation() >= 0;
}

bool JSGeneratorObject::is_closed() const {
  return continuation() == kGeneratorClosed;
}

bool JSGeneratorObject::is_executing() const {
  return continuation() == kGeneratorExecuting;
}

ACCESSORS(JSAsyncGeneratorObject, queue, HeapObject, kQueueOffset)
SMI_ACCESSORS(JSAsyncGeneratorObject, is_awaiting, kIsAwaitingOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_GENERATOR_INL_H_
