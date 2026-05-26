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

#include "torque-generated/src/objects/promise-tq-inl.inc"

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

Tagged<UnionOf<JSCallable, Undefined>> PromiseReactionJobTask::handler() const {
  return handler_.load();
}
void PromiseReactionJobTask::set_handler(
    Tagged<UnionOf<JSCallable, Undefined>> value, WriteBarrierMode mode) {
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

Tagged<Context> PromiseResolveThenableJobTask::context() const {
  return context_.load();
}
void PromiseResolveThenableJobTask::set_context(Tagged<Context> value,
                                                WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

Tagged<JSPromise> PromiseResolveThenableJobTask::promise_to_resolve() const {
  return promise_to_resolve_.load();
}
void PromiseResolveThenableJobTask::set_promise_to_resolve(
    Tagged<JSPromise> value, WriteBarrierMode mode) {
  promise_to_resolve_.store(this, value, mode);
}

Tagged<JSReceiver> PromiseResolveThenableJobTask::thenable() const {
  return thenable_.load();
}
void PromiseResolveThenableJobTask::set_thenable(Tagged<JSReceiver> value,
                                                 WriteBarrierMode mode) {
  thenable_.store(this, value, mode);
}

Tagged<JSReceiver> PromiseResolveThenableJobTask::then() const {
  return then_.load();
}
void PromiseResolveThenableJobTask::set_then(Tagged<JSReceiver> value,
                                             WriteBarrierMode mode) {
  then_.store(this, value, mode);
}

// PromiseCapability
Tagged<UnionOf<JSReceiver, Undefined>> PromiseCapability::promise() const {
  return promise_.load();
}
void PromiseCapability::set_promise(
    Tagged<UnionOf<JSReceiver, Undefined>> value, WriteBarrierMode mode) {
  promise_.store(this, value, mode);
}

Tagged<Object> PromiseCapability::resolve() const { return resolve_.load(); }
void PromiseCapability::set_resolve(Tagged<Object> value,
                                    WriteBarrierMode mode) {
  resolve_.store(this, value, mode);
}

Tagged<Object> PromiseCapability::reject() const { return reject_.load(); }
void PromiseCapability::set_reject(Tagged<Object> value,
                                   WriteBarrierMode mode) {
  reject_.store(this, value, mode);
}

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

Tagged<UnionOf<JSCallable, Undefined>> PromiseReaction::reject_handler() const {
  return reject_handler_.load();
}
void PromiseReaction::set_reject_handler(
    Tagged<UnionOf<JSCallable, Undefined>> value, WriteBarrierMode mode) {
  reject_handler_.store(this, value, mode);
}

Tagged<UnionOf<JSCallable, Undefined>> PromiseReaction::fulfill_handler()
    const {
  return fulfill_handler_.load();
}
void PromiseReaction::set_fulfill_handler(
    Tagged<UnionOf<JSCallable, Undefined>> value, WriteBarrierMode mode) {
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
