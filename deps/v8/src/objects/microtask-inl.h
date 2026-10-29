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
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Microtask::Microtask(const AllocationWitness& witness, Tagged<ReadOnly<Map>> map
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
                     ,
                     Tagged<Object> continuation_preserved_embedder_data
#endif
                     )
    : Struct(map)
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
      ,
      continuation_preserved_embedder_data_(
          witness, continuation_preserved_embedder_data)
#endif
{
  USE(witness);
}

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
Tagged<Object> Microtask::continuation_preserved_embedder_data() const {
  return continuation_preserved_embedder_data_.load();
}
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

CallbackTask::CallbackTask(const AllocationWitness& witness,
                           Tagged<Foreign> callback, Tagged<Object> data
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
                           ,
                           Tagged<Object> continuation_preserved_embedder_data
#endif
                           )
    : Microtask(witness, witness.roots().callback_task_map()
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
                             ,
                continuation_preserved_embedder_data
#endif
                ),
      callback_(witness, callback),
      data_(witness, data) {
}

Tagged<Foreign> CallbackTask::callback() const { return callback_.load(); }

Tagged<Object> CallbackTask::data() const { return data_.load(); }

CallableTask::CallableTask(const AllocationWitness& witness,
                           Tagged<JSReceiver> callable,
                           Tagged<NativeContext> context
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
                           ,
                           Tagged<Object> continuation_preserved_embedder_data
#endif
                           )
    : Microtask(witness, witness.roots().callable_task_map()
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
                             ,
                continuation_preserved_embedder_data
#endif
                ),
      callable_(witness, callable),
      context_(witness, context) {
}

Tagged<JSReceiver> CallableTask::callable() const { return callable_.load(); }

Tagged<NativeContext> CallableTask::context() const { return context_.load(); }

Tagged<JSGeneratorObject> AsyncResumeTask::generator() const {
  return generator_.load();
}
void AsyncResumeTask::set_generator(Tagged<JSGeneratorObject> value,
                                    WriteBarrierMode mode) {
  generator_.store(this, value, mode);
}

Tagged<Object> AsyncResumeTask::value() const { return value_.load(); }
void AsyncResumeTask::set_value(Tagged<Object> val, WriteBarrierMode mode) {
  value_.store(this, val, mode);
}

int AsyncResumeTask::kind() const { return kind_.load().value(); }
void AsyncResumeTask::set_kind(int kind) {
  kind_.store(this, Smi::FromInt(kind));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MICROTASK_INL_H_
