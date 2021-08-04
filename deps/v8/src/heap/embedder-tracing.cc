// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/embedder-tracing.h"

#include "include/v8-cppgc.h"
#include "src/base/logging.h"
#include "src/heap/gc-tracer.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects-inl.h"

namespace v8 {
namespace internal {

void LocalEmbedderHeapTracer::SetRemoteTracer(EmbedderHeapTracer* tracer) {
  if (remote_tracer_) remote_tracer_->isolate_ = nullptr;

  remote_tracer_ = tracer;
  default_embedder_roots_handler_.SetTracer(tracer);
  if (remote_tracer_)
    remote_tracer_->isolate_ = reinterpret_cast<v8::Isolate*>(isolate_);
}

void LocalEmbedderHeapTracer::TracePrologue(
    EmbedderHeapTracer::TraceFlags flags) {
  if (!InUse()) return;

  embedder_worklist_empty_ = false;
  remote_tracer_->TracePrologue(flags);
}

void LocalEmbedderHeapTracer::TraceEpilogue() {
  if (!InUse()) return;

  EmbedderHeapTracer::TraceSummary summary;
  remote_tracer_->TraceEpilogue(&summary);
  if (summary.allocated_size == SIZE_MAX) return;
  UpdateRemoteStats(summary.allocated_size, summary.time);
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

  remote_tracer_->EnterFinalPause(embedder_stack_state_);
  // Resetting to state unknown as there may be follow up garbage collections
  // triggered from callbacks that have a different stack state.
  embedder_stack_state_ =
      EmbedderHeapTracer::EmbedderStackState::kMayContainHeapPointers;
}

bool LocalEmbedderHeapTracer::Trace(double deadline) {
  if (!InUse()) return true;

  return remote_tracer_->AdvanceTracing(deadline);
}

bool LocalEmbedderHeapTracer::IsRemoteTracingDone() {
  return !InUse() || remote_tracer_->IsTracingDone();
}

void LocalEmbedderHeapTracer::SetEmbedderStackStateForNextFinalization(
    EmbedderHeapTracer::EmbedderStackState stack_state) {
  if (!InUse()) return;

  embedder_stack_state_ = stack_state;
  if (EmbedderHeapTracer::EmbedderStackState::kNoHeapPointers == stack_state)
    NotifyEmptyEmbedderStack();
}

namespace {

bool ExtractWrappableInfo(Isolate* isolate, JSObject js_object,
                          const WrapperDescriptor& wrapper_descriptor,
                          LocalEmbedderHeapTracer::WrapperInfo* info) {
  DCHECK(js_object.IsApiWrapper());
  if (js_object.GetEmbedderFieldCount() < 2) return false;

  if (EmbedderDataSlot(js_object, wrapper_descriptor.wrappable_type_index)
          .ToAlignedPointerSafe(isolate, &info->first) &&
      info->first &&
      EmbedderDataSlot(js_object, wrapper_descriptor.wrappable_instance_index)
          .ToAlignedPointerSafe(isolate, &info->second) &&
      info->second) {
    return (wrapper_descriptor.embedder_id_for_garbage_collected ==
            WrapperDescriptor::kUnknownEmbedderId) ||
           (*static_cast<uint16_t*>(info->first) ==
            wrapper_descriptor.embedder_id_for_garbage_collected);
  }
  return false;
}

}  // namespace

LocalEmbedderHeapTracer::ProcessingScope::ProcessingScope(
    LocalEmbedderHeapTracer* tracer)
    : tracer_(tracer), wrapper_descriptor_(tracer->wrapper_descriptor_) {
  wrapper_cache_.reserve(kWrapperCacheSize);
}

LocalEmbedderHeapTracer::ProcessingScope::~ProcessingScope() {
  if (!wrapper_cache_.empty()) {
    tracer_->remote_tracer()->RegisterV8References(std::move(wrapper_cache_));
  }
}

LocalEmbedderHeapTracer::WrapperInfo
LocalEmbedderHeapTracer::ExtractWrapperInfo(Isolate* isolate,
                                            JSObject js_object) {
  WrapperInfo info;
  if (ExtractWrappableInfo(isolate, js_object, wrapper_descriptor_, &info)) {
    return info;
  }
  return {nullptr, nullptr};
}

void LocalEmbedderHeapTracer::ProcessingScope::TracePossibleWrapper(
    JSObject js_object) {
  DCHECK(js_object.IsApiWrapper());
  WrapperInfo info;
  if (ExtractWrappableInfo(tracer_->isolate_, js_object, wrapper_descriptor_,
                           &info)) {
    wrapper_cache_.push_back(std::move(info));
    FlushWrapperCacheIfFull();
  }
}

void LocalEmbedderHeapTracer::ProcessingScope::FlushWrapperCacheIfFull() {
  if (wrapper_cache_.size() == wrapper_cache_.capacity()) {
    tracer_->remote_tracer()->RegisterV8References(std::move(wrapper_cache_));
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

bool DefaultEmbedderRootsHandler::IsRoot(
    const v8::TracedReference<v8::Value>& handle) {
  return !tracer_ || tracer_->IsRootForNonTracingGC(handle);
}

bool DefaultEmbedderRootsHandler::IsRoot(
    const v8::TracedGlobal<v8::Value>& handle) {
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
