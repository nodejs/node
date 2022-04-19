// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_GC_TRACER_INL_H_
#define V8_HEAP_GC_TRACER_INL_H_

#include "src/base/platform/platform.h"
#include "src/execution/isolate.h"
#include "src/heap/gc-tracer.h"

namespace v8 {
namespace internal {

GCTracer::IncrementalMarkingInfos::IncrementalMarkingInfos()
    : duration(0), longest_step(0), steps(0) {}

void GCTracer::IncrementalMarkingInfos::Update(double delta) {
  steps++;
  duration += delta;
  if (delta > longest_step) {
    longest_step = delta;
  }
}

void GCTracer::IncrementalMarkingInfos::ResetCurrentCycle() {
  duration = 0;
  longest_step = 0;
  steps = 0;
}

GCTracer::Scope::Scope(GCTracer* tracer, ScopeId scope, ThreadKind thread_kind)
    : tracer_(tracer),
      scope_(scope),
      thread_kind_(thread_kind),
      start_time_(tracer_->MonotonicallyIncreasingTimeInMs()) {
#ifdef V8_RUNTIME_CALL_STATS
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;
  if (thread_kind_ == ThreadKind::kMain) {
#if DEBUG
    AssertMainThread();
#endif  // DEBUG
    runtime_stats_ = tracer_->heap_->isolate_->counters()->runtime_call_stats();
    runtime_stats_->Enter(&timer_, GCTracer::RCSCounterFromScope(scope));
  } else {
    runtime_call_stats_scope_.emplace(
        tracer->worker_thread_runtime_call_stats());
    runtime_stats_ = runtime_call_stats_scope_->Get();
    runtime_stats_->Enter(&timer_, GCTracer::RCSCounterFromScope(scope));
  }
#endif  // defined(V8_RUNTIME_CALL_STATS)
}

GCTracer::Scope::~Scope() {
  double duration_ms = tracer_->MonotonicallyIncreasingTimeInMs() - start_time_;
  tracer_->AddScopeSample(scope_, duration_ms);

  if (thread_kind_ == ThreadKind::kMain) {
#if DEBUG
    AssertMainThread();
#endif  // DEBUG

    if (scope_ == ScopeId::MC_INCREMENTAL ||
        scope_ == ScopeId::MC_INCREMENTAL_START ||
        scope_ == ScopeId::MC_INCREMENTAL_FINALIZE) {
      auto* long_task_stats =
          tracer_->heap_->isolate_->GetCurrentLongTaskStats();
      long_task_stats->gc_full_incremental_wall_clock_duration_us +=
          static_cast<int64_t>(duration_ms *
                               base::Time::kMicrosecondsPerMillisecond);
    }
  }

#ifdef V8_RUNTIME_CALL_STATS
  if (V8_LIKELY(runtime_stats_ == nullptr)) return;
  runtime_stats_->Leave(&timer_);
#endif  // defined(V8_RUNTIME_CALL_STATS)
}

constexpr int GCTracer::Scope::IncrementalOffset(ScopeId id) {
  DCHECK_LE(FIRST_INCREMENTAL_SCOPE, id);
  DCHECK_GE(LAST_INCREMENTAL_SCOPE, id);
  return id - FIRST_INCREMENTAL_SCOPE;
}

constexpr bool GCTracer::Event::IsYoungGenerationEvent(Type type) {
  DCHECK_NE(START, type);
  return type == SCAVENGER || type == MINOR_MARK_COMPACTOR;
}

CollectionEpoch GCTracer::CurrentEpoch(Scope::ScopeId id) const {
  return Scope::NeedsYoungEpoch(id) ? epoch_young_ : epoch_full_;
}

#ifdef DEBUG
bool GCTracer::IsInObservablePause() const {
  return 0.0 < start_of_observable_pause_;
}

bool GCTracer::IsConsistentWithCollector(GarbageCollector collector) const {
  return (collector == GarbageCollector::SCAVENGER &&
          current_.type == Event::SCAVENGER) ||
         (collector == GarbageCollector::MINOR_MARK_COMPACTOR &&
          current_.type == Event::MINOR_MARK_COMPACTOR) ||
         (collector == GarbageCollector::MARK_COMPACTOR &&
          (current_.type == Event::MARK_COMPACTOR ||
           current_.type == Event::INCREMENTAL_MARK_COMPACTOR));
}

bool GCTracer::IsSweepingInProgress() const {
  return (current_.type == Event::MARK_COMPACTOR ||
          current_.type == Event::INCREMENTAL_MARK_COMPACTOR) &&
         current_.state == Event::State::SWEEPING;
}
#endif

constexpr double GCTracer::current_scope(Scope::ScopeId id) const {
  if (Scope::FIRST_INCREMENTAL_SCOPE <= id &&
      id <= Scope::LAST_INCREMENTAL_SCOPE) {
    return incremental_scope(id).duration;
  } else if (Scope::FIRST_BACKGROUND_SCOPE <= id &&
             id <= Scope::LAST_BACKGROUND_SCOPE) {
    return background_counter_[id].total_duration_ms;
  } else {
    DCHECK_GT(Scope::NUMBER_OF_SCOPES, id);
    return current_.scopes[id];
  }
}

constexpr const GCTracer::IncrementalMarkingInfos& GCTracer::incremental_scope(
    Scope::ScopeId id) const {
  return incremental_scopes_[Scope::IncrementalOffset(id)];
}

void GCTracer::AddScopeSample(Scope::ScopeId id, double duration) {
  if (Scope::FIRST_INCREMENTAL_SCOPE <= id &&
      id <= Scope::LAST_INCREMENTAL_SCOPE) {
    incremental_scopes_[Scope::IncrementalOffset(id)].Update(duration);
  } else if (Scope::FIRST_BACKGROUND_SCOPE <= id &&
             id <= Scope::LAST_BACKGROUND_SCOPE) {
    base::MutexGuard guard(&background_counter_mutex_);
    background_counter_[id].total_duration_ms += duration;
  } else {
    DCHECK_GT(Scope::NUMBER_OF_SCOPES, id);
    current_.scopes[id] += duration;
  }
}

#ifdef V8_RUNTIME_CALL_STATS
WorkerThreadRuntimeCallStats* GCTracer::worker_thread_runtime_call_stats() {
  return heap_->isolate_->counters()->worker_thread_runtime_call_stats();
}

RuntimeCallCounterId GCTracer::RCSCounterFromScope(Scope::ScopeId id) {
  STATIC_ASSERT(Scope::FIRST_SCOPE == Scope::MC_INCREMENTAL);
  return static_cast<RuntimeCallCounterId>(
      static_cast<int>(RuntimeCallCounterId::kGC_MC_INCREMENTAL) +
      static_cast<int>(id));
}
#endif  // defined(V8_RUNTIME_CALL_STATS)

double GCTracer::MonotonicallyIncreasingTimeInMs() {
  if (V8_UNLIKELY(FLAG_predictable)) {
    return heap_->MonotonicallyIncreasingTimeInMs();
  } else {
    return base::TimeTicks::Now().ToInternalValue() /
           static_cast<double>(base::Time::kMicrosecondsPerMillisecond);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_GC_TRACER_INL_H_
