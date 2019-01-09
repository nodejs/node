// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROMISE_INL_H_
#define V8_OBJECTS_PROMISE_INL_H_

#include "src/objects/promise.h"

#include "src/objects/js-promise-inl.h"
#include "src/objects/microtask-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(PromiseReactionJobTask, Microtask)
OBJECT_CONSTRUCTORS_IMPL(PromiseFulfillReactionJobTask, PromiseReactionJobTask)
OBJECT_CONSTRUCTORS_IMPL(PromiseRejectReactionJobTask, PromiseReactionJobTask)
OBJECT_CONSTRUCTORS_IMPL(PromiseResolveThenableJobTask, Microtask)
OBJECT_CONSTRUCTORS_IMPL(PromiseCapability, Struct)
OBJECT_CONSTRUCTORS_IMPL(PromiseReaction, Struct)

CAST_ACCESSOR2(PromiseCapability)
CAST_ACCESSOR2(PromiseReaction)
CAST_ACCESSOR2(PromiseReactionJobTask)
CAST_ACCESSOR2(PromiseFulfillReactionJobTask)
CAST_ACCESSOR2(PromiseRejectReactionJobTask)
CAST_ACCESSOR2(PromiseResolveThenableJobTask)

ACCESSORS(PromiseReaction, next, Object, kNextOffset)
ACCESSORS2(PromiseReaction, reject_handler, HeapObject, kRejectHandlerOffset)
ACCESSORS2(PromiseReaction, fulfill_handler, HeapObject, kFulfillHandlerOffset)
ACCESSORS2(PromiseReaction, promise_or_capability, HeapObject,
           kPromiseOrCapabilityOffset)

ACCESSORS2(PromiseResolveThenableJobTask, context, Context, kContextOffset)
ACCESSORS2(PromiseResolveThenableJobTask, promise_to_resolve, JSPromise,
           kPromiseToResolveOffset)
ACCESSORS2(PromiseResolveThenableJobTask, then, JSReceiver, kThenOffset)
ACCESSORS2(PromiseResolveThenableJobTask, thenable, JSReceiver, kThenableOffset)

ACCESSORS2(PromiseReactionJobTask, context, Context, kContextOffset)
ACCESSORS(PromiseReactionJobTask, argument, Object, kArgumentOffset);
ACCESSORS2(PromiseReactionJobTask, handler, HeapObject, kHandlerOffset);
ACCESSORS2(PromiseReactionJobTask, promise_or_capability, HeapObject,
           kPromiseOrCapabilityOffset);

ACCESSORS2(PromiseCapability, promise, HeapObject, kPromiseOffset)
ACCESSORS(PromiseCapability, resolve, Object, kResolveOffset)
ACCESSORS(PromiseCapability, reject, Object, kRejectOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_INL_H_
