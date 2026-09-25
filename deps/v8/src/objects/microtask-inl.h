// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MICROTASK_INL_H_
#define V8_OBJECTS_MICROTASK_INL_H_

#include "src/objects/microtask.h"
// Include the non-inl header before the rest of the headers.

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/foreign-inl.h"
#include "src/objects/js-objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/microtask-tq-inl.inc"

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
Tagged<Object> Microtask::continuation_preserved_embedder_data() const {
  return continuation_preserved_embedder_data_.load();
}
void Microtask::set_continuation_preserved_embedder_data(
    Tagged<Object> value, WriteBarrierMode mode) {
  continuation_preserved_embedder_data_.store(this, value, mode);
}
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

Tagged<Foreign> CallbackTask::callback() const { return callback_.load(); }
void CallbackTask::set_callback(Tagged<Foreign> value, WriteBarrierMode mode) {
  callback_.store(this, value, mode);
}

Tagged<Foreign> CallbackTask::data() const { return data_.load(); }
void CallbackTask::set_data(Tagged<Foreign> value, WriteBarrierMode mode) {
  data_.store(this, value, mode);
}

Tagged<JSReceiver> CallableTask::callable() const { return callable_.load(); }
void CallableTask::set_callable(Tagged<JSReceiver> value,
                                WriteBarrierMode mode) {
  callable_.store(this, value, mode);
}

Tagged<Context> CallableTask::context() const { return context_.load(); }
void CallableTask::set_context(Tagged<Context> value, WriteBarrierMode mode) {
  context_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_INL_H_
