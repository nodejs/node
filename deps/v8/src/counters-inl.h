// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COUNTERS_INL_H_
#define V8_COUNTERS_INL_H_

#include "src/counters.h"

namespace v8 {
namespace internal {

RuntimeCallTimerScope::RuntimeCallTimerScope(
    Isolate* isolate, RuntimeCallStats::CounterId counter_id) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    Initialize(isolate->counters()->runtime_call_stats(), counter_id);
  }
}

RuntimeCallTimerScope::RuntimeCallTimerScope(
    HeapObject* heap_object, RuntimeCallStats::CounterId counter_id) {
  RuntimeCallTimerScope(heap_object->GetIsolate(), counter_id);
}

RuntimeCallTimerScope::RuntimeCallTimerScope(
    RuntimeCallStats* stats, RuntimeCallStats::CounterId counter_id) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    Initialize(stats, counter_id);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_COUNTERS_INL_H_
