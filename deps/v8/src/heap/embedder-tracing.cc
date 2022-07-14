// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/embedder-tracing.h"

#include "include/v8-cppgc.h"
#include "src/base/logging.h"
#include "src/handles/global-handles.h"
#include "src/heap/embedder-tracing-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/marking-worklist-inl.h"

namespace v8 {
namespace internal {

void LocalEmbedderHeapTracer::SetRemoteTracer(EmbedderHeapTracer* tracer) {
  CHECK_NULL(cpp_heap_);
  if (remote_tracer_) remote_tracer_->isolate_ = nullptr;

  remote_tracer_ = tracer;
  default_embedder_roots_handler_.SetTracer(tracer);
  if (remote_tracer_)
    remote_tracer_->isolate_ = reinterpret_cast<v8::Isolate*>(isolate_);
}

void LocalEmbedderHeapTracer::SetCppHeap(CppHeap* cpp_heap) {
  CHECK_NULL(remote_tracer_);
  cpp_heap_ = cpp_heap;
}

namespace {
CppHeap::GarbageCollectionFlags ConvertTraceFlags(
    EmbedderHeapTracer::TraceFlags flags) {
  CppHeap::GarbageCollectionFlags result;
  if (flags & EmbedderHeapTracer::TraceFlags::kForced)
    result |= CppHeap::GarbageCollectionFlagValues::kForced;
  if (flags & EmbedderHeapTracer::TraceFlags::kReduceMemory)
    result |= CppHeap::GarbageCollectionFlagValues::kReduceMemory;
  return result;
}
}  // namespace

void LocalEmbedderHeapTracer::PrepareForTrace(
    EmbedderHeapTracer::TraceFlags flags) {
  if (cpp_heap_)
    cpp_heap()->InitializeTracing(
        cppgc::internal::GarbageCollector::Config::CollectionType::kMajor,
        ConvertTraceFlags(flags));
}

void LocalEmbedderHeapTracer::TracePrologue(
    EmbedderHeapTracer::TraceFlags flags) {
  if (!InUse()) return;

  embedder_worklist_empty_ = false;
  if (cpp_heap_)
    cpp_heap()->StartTracing();
  else
    remote_tracer_->TracePrologue(flags);
}

void LocalEmbedderHeapTracer::TraceEpilogue() {
  if (!InUse()) return;

  // Resetting to state unknown as there may be follow up garbage collections
  // triggered from callbacks that have a different stack state.
  embedder_stack_state_ =
      EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers;

  if (cpp_heap_) {
    cpp_heap()->TraceEpilogue();
  } else {
    EmbedderHeapTracer::TraceSummary summary;
    remote_tracer_->TraceEpilogue(&summary);
    UpdateRemoteStats(summary.allocated_size, summary.time);
  }
}

void LocalEmbedderHeapTracer::UpdateRemoteStats(size_t allocated_size,
                                                double time) {
  remote_stats_.used_size = allocated_size;
  // Force a check next time increased memory is reported. This allows for
  // setting limits close to actual heap sizes.
  remote_stats_.allocated_size_limit_for_check = 0;
  constexpr double kMinReportingTimeMs = 0.5;
  if (time > kMinReportingTimeMs) {
    isolate_->heap()->tracer()->RecordEmbedderSpeed(allocated_size, time);
  }
}

void LocalEmbedderHeapTracer::EnterFinalPause() {
  if (!InUse()) return;

  if (cpp_heap_)
    cpp_heap()->EnterFinalPause(embedder_stack_state_);
  else
    remote_tracer_->EnterFinalPause(embedder_stack_state_);
}

bool LocalEmbedderHeapTracer::Trace(double max_duration) {
  if (!InUse()) return true;

  if (cpp_heap_)
    return cpp_heap()->AdvanceTracing(max_duration);
  else
    return remote_tracer_->AdvanceTracing(max_duration);
}

bool LocalEmbedderHeapTracer::IsRemoteTracingDone() {
  return !InUse() || (cpp_heap_ ? cpp_heap()->IsTracingDone()
                                : remote_tracer_->IsTracingDone());
}

LocalEmbedderHeapTracer::ProcessingScope::ProcessingScope(
    LocalEmbedderHeapTracer* tracer)
    : tracer_(tracer), wrapper_descriptor_(tracer->wrapper_descriptor_) {
  DCHECK(!tracer_->cpp_heap_);
  wrapper_cache_.reserve(kWrapperCacheSize);
}

LocalEmbedderHeapTracer::ProcessingScope::~ProcessingScope() {
  DCHECK(!tracer_->cpp_heap_);
  if (!wrapper_cache_.empty()) {
    tracer_->remote_tracer_->RegisterV8References(std::move(wrapper_cache_));
  }
}

LocalEmbedderHeapTracer::WrapperInfo
LocalEmbedderHeapTracer::ExtractWrapperInfo(Isolate* isolate,
                                            JSObject js_object) {
  WrapperInfo info;
  if (ExtractWrappableInfo(isolate, js_object, wrapper_descriptor(), &info)) {
    return info;
  }
  return {nullptr, nullptr};
}

void LocalEmbedderHeapTracer::ProcessingScope::TracePossibleWrapper(
    JSObject js_object) {
  DCHECK(js_object.MayHaveEmbedderFields());
  WrapperInfo info;
  if (ExtractWrappableInfo(tracer_->isolate_, js_object, wrapper_descriptor_,
                           &info)) {
    wrapper_cache_.push_back(std::move(info));
    FlushWrapperCacheIfFull();
  }
}

void LocalEmbedderHeapTracer::ProcessingScope::FlushWrapperCacheIfFull() {
  DCHECK(!tracer_->cpp_heap_);
  if (wrapper_cache_.size() == wrapper_cache_.capacity()) {
    tracer_->remote_tracer_->RegisterV8References(std::move(wrapper_cache_));
    wrapper_cache_.clear();
    wrapper_cache_.reserve(kWrapperCacheSize);
  }
}

void LocalEmbedderHeapTracer::ProcessingScope::AddWrapperInfoForTesting(
    WrapperInfo info) {
  wrapper_cache_.push_back(info);
  FlushWrapperCacheIfFull();
}

void LocalEmbedderHeapTracer::StartIncrementalMarkingIfNeeded() {
  if (!FLAG_global_gc_scheduling || !FLAG_incremental_marking) return;

  Heap* heap = isolate_->heap();
  heap->StartIncrementalMarkingIfAllocationLimitIsReached(
      heap->GCFlagsForIncrementalMarking(),
      kGCCallbackScheduleIdleGarbageCollection);
  if (heap->AllocationLimitOvershotByLargeMargin()) {
    heap->FinalizeIncrementalMarkingAtomically(
        i::GarbageCollectionReason::kExternalFinalize);
  }
}

void LocalEmbedderHeapTracer::NotifyEmptyEmbedderStack() {
  auto* overriden_stack_state = isolate_->heap()->overriden_stack_state();
  if (overriden_stack_state &&
      (*overriden_stack_state ==
       cppgc::EmbedderStackState::kMayContainHeapPointers))
    return;

  isolate_->global_handles()->NotifyEmptyEmbedderStack();
}

void LocalEmbedderHeapTracer::EmbedderWriteBarrier(Heap* heap,
                                                   JSObject js_object) {
  DCHECK(InUse());
  DCHECK(js_object.MayHaveEmbedderFields());
  if (cpp_heap_) {
    DCHECK_NOT_NULL(heap->mark_compact_collector());
    const EmbedderDataSlot type_slot(js_object,
                                     wrapper_descriptor_.wrappable_type_index);
    const EmbedderDataSlot instance_slot(
        js_object, wrapper_descriptor_.wrappable_instance_index);
    heap->mark_compact_collector()
        ->local_marking_worklists()
        ->cpp_marking_state()
        ->MarkAndPush(type_slot, instance_slot);
    return;
  }
  LocalEmbedderHeapTracer::ProcessingScope scope(this);
  scope.TracePossibleWrapper(js_object);
}

bool DefaultEmbedderRootsHandler::IsRoot(
    const v8::TracedReference<v8::Value>& handle) {
  return !tracer_ || tracer_->IsRootForNonTracingGC(handle);
}

void DefaultEmbedderRootsHandler::ResetRoot(
    const v8::TracedReference<v8::Value>& handle) {
  // Resetting is only called when IsRoot() returns false which
  // can only happen the EmbedderHeapTracer is set on API level.
  DCHECK(tracer_);
  tracer_->ResetHandleInNonTracingGC(handle);
}

}  // namespace internal
}  // namespace v8
