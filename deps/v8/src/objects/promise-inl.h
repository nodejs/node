// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROMISE_INL_H_
#define V8_OBJECTS_PROMISE_INL_H_

#include "src/objects/promise.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(PromiseCapability)
CAST_ACCESSOR(PromiseReaction)
CAST_ACCESSOR(PromiseReactionJobTask)
CAST_ACCESSOR(PromiseFulfillReactionJobTask)
CAST_ACCESSOR(PromiseRejectReactionJobTask)
CAST_ACCESSOR(PromiseResolveThenableJobTask)

ACCESSORS(PromiseReaction, next, Object, kNextOffset)
ACCESSORS(PromiseReaction, reject_handler, HeapObject, kRejectHandlerOffset)
ACCESSORS(PromiseReaction, fulfill_handler, HeapObject, kFulfillHandlerOffset)
ACCESSORS(PromiseReaction, promise_or_capability, HeapObject,
          kPromiseOrCapabilityOffset)

ACCESSORS(PromiseResolveThenableJobTask, context, Context, kContextOffset)
ACCESSORS(PromiseResolveThenableJobTask, promise_to_resolve, JSPromise,
          kPromiseToResolveOffset)
ACCESSORS(PromiseResolveThenableJobTask, then, JSReceiver, kThenOffset)
ACCESSORS(PromiseResolveThenableJobTask, thenable, JSReceiver, kThenableOffset)

ACCESSORS(PromiseReactionJobTask, context, Context, kContextOffset)
ACCESSORS(PromiseReactionJobTask, argument, Object, kArgumentOffset);
ACCESSORS(PromiseReactionJobTask, handler, HeapObject, kHandlerOffset);
ACCESSORS(PromiseReactionJobTask, promise_or_capability, HeapObject,
          kPromiseOrCapabilityOffset);

ACCESSORS(PromiseCapability, promise, HeapObject, kPromiseOffset)
ACCESSORS(PromiseCapability, resolve, Object, kResolveOffset)
ACCESSORS(PromiseCapability, reject, Object, kRejectOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_INL_H_
