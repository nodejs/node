// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COUNTERS_INL_H_
#define V8_COUNTERS_INL_H_

#include "src/counters.h"

namespace v8 {
namespace internal {

void RuntimeCallTimer::Start(RuntimeCallCounter* counter,
                             RuntimeCallTimer* parent) {
  DCHECK(!IsStarted());
  counter_ = counter;
  parent_.SetValue(parent);
  if (FLAG_runtime_stats ==
      v8::tracing::TracingCategoryObserver::ENABLED_BY_SAMPLING) {
    return;
  }
  base::TimeTicks now = Now();
  if (parent) parent->Pause(now);
  Resume(now);
  DCHECK(IsStarted());
}

void RuntimeCallTimer::Pause(base::TimeTicks now) {
  DCHECK(IsStarted());
  elapsed_ += (now - start_ticks_);
  start_ticks_ = base::TimeTicks();
}

void RuntimeCallTimer::Resume(base::TimeTicks now) {
  DCHECK(!IsStarted());
  start_ticks_ = now;
}

RuntimeCallTimer* RuntimeCallTimer::Stop() {
  if (!IsStarted()) return parent();
  base::TimeTicks now = Now();
  Pause(now);
  counter_->Increment();
  CommitTimeToCounter();

  RuntimeCallTimer* parent_timer = parent();
  if (parent_timer) {
    parent_timer->Resume(now);
  }
  return parent_timer;
}

void RuntimeCallTimer::CommitTimeToCounter() {
  counter_->Add(elapsed_);
  elapsed_ = base::TimeDelta();
}

bool RuntimeCallTimer::IsStarted() { return start_ticks_ != base::TimeTicks(); }

base::TimeTicks RuntimeCallTimer::Now() {
  return base::TimeTicks::HighResolutionNow();
}

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
