// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROMISE_INL_H_
#define V8_OBJECTS_PROMISE_INL_H_

#include "src/objects/promise.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-promise-inl.h"
#include "src/objects/microtask-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<Object> PromiseReactionJobTask::argument() const {
  return argument_.load();
}
void PromiseReactionJobTask::set_argument(Tagged<Object> value,
                                          WriteBarrierMode mode) {
  argument_.store(this, value, mode);
}

Tagged<Context> PromiseReactionJobTask::context() const {
  return context_.load();
}
void PromiseReactionJobTask::set_context(Tagged<Context> value,
                                         WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

Tagged<PromiseReactionHandler> PromiseReactionJobTask::handler() const {
  return handler_.load();
}
void PromiseReactionJobTask::set_handler(Tagged<PromiseReactionHandler> value,
                                         WriteBarrierMode mode) {
  handler_.store(this, value, mode);
}

Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>>
PromiseReactionJobTask::promise_or_capability() const {
  return promise_or_capability_.load();
}
void PromiseReactionJobTask::set_promise_or_capability(
    Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>> value,
    WriteBarrierMode mode) {
  promise_or_capability_.store(this, value, mode);
}

PromiseResolveThenableJobTask::PromiseResolveThenableJobTask(
    const AllocationWitness& witness, Tagged<JSPromise> promise_to_resolve,
    Tagged<JSReceiver> thenable, Tagged<JSReceiver> then,
    Tagged<Context> context
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
    ,
    Tagged<Object> continuation_preserved_embedder_data
#endif
    )
    : Microtask(witness, witness.roots().promise_resolve_thenable_job_task_map()
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
                             ,
                continuation_preserved_embedder_data
#endif
                ),
      context_(witness, context),
      promise_to_resolve_(witness, promise_to_resolve),
      thenable_(witness, thenable),
      then_(witness, then) {
}

Tagged<Context> PromiseResolveThenableJobTask::context() const {
  return context_.load();
}

Tagged<JSPromise> PromiseResolveThenableJobTask::promise_to_resolve() const {
  return promise_to_resolve_.load();
}

Tagged<JSReceiver> PromiseResolveThenableJobTask::thenable() const {
  return thenable_.load();
}

Tagged<JSReceiver> PromiseResolveThenableJobTask::then() const {
  return then_.load();
}

// PromiseCapability
PromiseCapability::PromiseCapability(
    const AllocationWitness& witness,
    Tagged<UnionOf<JSReceiver, Undefined>> promise, Tagged<JSAny> resolve,
    Tagged<JSAny> reject)
    : Struct(witness.roots().promise_capability_map()),
      promise_(witness, promise),
      resolve_(witness, resolve),
      reject_(witness, reject) {}

Tagged<UnionOf<JSReceiver, Undefined>> PromiseCapability::promise() const {
  return promise_.load();
}

Tagged<JSAny> PromiseCapability::resolve() const { return resolve_.load(); }

Tagged<JSAny> PromiseCapability::reject() const { return reject_.load(); }

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
Tagged<Object> PromiseReaction::continuation_preserved_embedder_data() const {
  return continuation_preserved_embedder_data_.load();
}
void PromiseReaction::set_continuation_preserved_embedder_data(
    Tagged<Object> value, WriteBarrierMode mode) {
  continuation_preserved_embedder_data_.store(this, value, mode);
}
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

Tagged<UnionOf<PromiseReaction, Smi>> PromiseReaction::next() const {
  return next_.load();
}
void PromiseReaction::set_next(Tagged<UnionOf<PromiseReaction, Smi>> value,
                               WriteBarrierMode mode) {
  next_.store(this, value, mode);
}

Tagged<UnionOf<JSCallable, JSGeneratorObject, Undefined>>
PromiseReaction::reject_handler() const {
  return reject_handler_.load();
}
void PromiseReaction::set_reject_handler(
    Tagged<UnionOf<JSCallable, JSGeneratorObject, Undefined>> value,
    WriteBarrierMode mode) {
  reject_handler_.store(this, value, mode);
}

Tagged<UnionOf<JSCallable, JSGeneratorObject, Undefined>>
PromiseReaction::fulfill_handler() const {
  return fulfill_handler_.load();
}
void PromiseReaction::set_fulfill_handler(
    Tagged<UnionOf<JSCallable, JSGeneratorObject, Undefined>> value,
    WriteBarrierMode mode) {
  fulfill_handler_.store(this, value, mode);
}

Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>>
PromiseReaction::promise_or_capability() const {
  return promise_or_capability_.load();
}
void PromiseReaction::set_promise_or_capability(
    Tagged<UnionOf<JSPromise, PromiseCapability, Undefined>> value,
    WriteBarrierMode mode) {
  promise_or_capability_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROMISE_INL_H_
