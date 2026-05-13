// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PROMISE_INL_H_
#define V8_OBJECTS_JS_PROMISE_INL_H_

#include "src/objects/js-promise.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<UnionOf<PromiseReaction, JSAny>> JSPromise::reactions_or_result() const {
  return reactions_or_result_.load();
}

void JSPromise::set_reactions_or_result(
    Tagged<UnionOf<PromiseReaction, JSAny>> value, WriteBarrierMode mode) {
  reactions_or_result_.store(this, value, mode);
}

int JSPromise::flags() const { return flags_.load().value(); }

void JSPromise::set_flags(int value) {
  flags_.store(this, Smi::From31BitPattern(value));
}

BOOL_ACCESSORS(JSPromise, flags, has_handler, HasHandlerBit::kShift)
BOOL_ACCESSORS(JSPromise, flags, is_silent, IsSilentBit::kShift)

// static
uint32_t JSPromise::GetNextAsyncTaskId(uint32_t async_task_id) {
  do {
    ++async_task_id;
    async_task_id &= AsyncTaskIdBits::kMax;
  } while (async_task_id == kInvalidAsyncTaskId);
  return async_task_id;
}

bool JSPromise::has_async_task_id() const {
  return async_task_id() != kInvalidAsyncTaskId;
}

uint32_t JSPromise::async_task_id() const {
  return AsyncTaskIdBits::decode(flags());
}

void JSPromise::set_async_task_id(uint32_t id) {
  set_flags(AsyncTaskIdBits::update(flags(), id));
}

Tagged<Object> JSPromise::result() const {
  DCHECK_NE(Promise::kPending, status());
  return reactions_or_result();
}

Tagged<Object> JSPromise::reactions() const {
  DCHECK_EQ(Promise::kPending, status());
  return reactions_or_result();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PROMISE_INL_H_
