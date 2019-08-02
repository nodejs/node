// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOGGING_COUNTERS_INL_H_
#define V8_LOGGING_COUNTERS_INL_H_

#include "src/logging/counters.h"

namespace v8 {
namespace internal {

void RuntimeCallTimer::Start(RuntimeCallCounter* counter,
                             RuntimeCallTimer* parent) {
  DCHECK(!IsStarted());
  counter_ = counter;
  parent_.SetValue(parent);
  if (TracingFlags::runtime_stats.load(std::memory_order_relaxed) ==
      v8::tracing::TracingCategoryObserver::ENABLED_BY_SAMPLING) {
    return;
  }
  base::TimeTicks now = RuntimeCallTimer::Now();
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
  base::TimeTicks now = RuntimeCallTimer::Now();
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

RuntimeCallTimerScope::RuntimeCallTimerScope(Isolate* isolate,
                                             HeapObject heap_object,
                                             RuntimeCallCounterId counter_id)
    : RuntimeCallTimerScope(isolate, counter_id) {}

}  // namespace internal
}  // namespace v8

#endif  // V8_LOGGING_COUNTERS_INL_H_
