// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/embedder-tracing.h"

#include "src/base/logging.h"

namespace v8 {
namespace internal {

void LocalEmbedderHeapTracer::TracePrologue() {
  if (!InUse()) return;

  CHECK(cached_wrappers_to_trace_.empty());
  num_v8_marking_worklist_was_empty_ = 0;
  remote_tracer_->TracePrologue();
}

void LocalEmbedderHeapTracer::TraceEpilogue() {
  if (!InUse()) return;

  CHECK(cached_wrappers_to_trace_.empty());
  remote_tracer_->TraceEpilogue();
}

void LocalEmbedderHeapTracer::AbortTracing() {
  if (!InUse()) return;

  cached_wrappers_to_trace_.clear();
  remote_tracer_->AbortTracing();
}

void LocalEmbedderHeapTracer::EnterFinalPause() {
  if (!InUse()) return;

  remote_tracer_->EnterFinalPause();
}

bool LocalEmbedderHeapTracer::Trace(
    double deadline, EmbedderHeapTracer::AdvanceTracingActions actions) {
  if (!InUse()) return false;

  DCHECK_EQ(0, NumberOfCachedWrappersToTrace());
  return remote_tracer_->AdvanceTracing(deadline, actions);
}

size_t LocalEmbedderHeapTracer::NumberOfWrappersToTrace() {
  return (InUse())
             ? cached_wrappers_to_trace_.size() +
                   remote_tracer_->NumberOfWrappersToTrace()
             : 0;
}

void LocalEmbedderHeapTracer::RegisterWrappersWithRemoteTracer() {
  if (!InUse()) return;

  if (cached_wrappers_to_trace_.empty()) {
    return;
  }

  remote_tracer_->RegisterV8References(cached_wrappers_to_trace_);
  cached_wrappers_to_trace_.clear();
}

bool LocalEmbedderHeapTracer::RequiresImmediateWrapperProcessing() {
  const size_t kTooManyWrappers = 16000;
  return cached_wrappers_to_trace_.size() > kTooManyWrappers;
}

}  // namespace internal
}  // namespace v8
