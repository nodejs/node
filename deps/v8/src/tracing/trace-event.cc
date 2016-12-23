// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/trace-event.h"

#include <string.h>

#include "src/counters.h"
#include "src/isolate.h"
#include "src/v8.h"

namespace v8 {
namespace internal {
namespace tracing {

// A global flag used as a shortcut to check for the
// v8.runtime-call-stats category due to its high frequency use.
base::Atomic32 kRuntimeCallStatsTracingEnabled = false;

v8::Platform* TraceEventHelper::GetCurrentPlatform() {
  return v8::internal::V8::GetCurrentPlatform();
}

void CallStatsScopedTracer::AddEndTraceEvent() {
  if (!has_parent_scope_ && p_data_->isolate) {
    v8::internal::tracing::AddTraceEvent(
        TRACE_EVENT_PHASE_END, p_data_->category_group_enabled, p_data_->name,
        v8::internal::tracing::kGlobalScope, v8::internal::tracing::kNoId,
        v8::internal::tracing::kNoId, TRACE_EVENT_FLAG_NONE,
        "runtime-call-stats", TRACE_STR_COPY(p_data_->isolate->counters()
                                                 ->runtime_call_stats()
                                                 ->Dump()
                                                 .c_str()));
  } else {
    v8::internal::tracing::AddTraceEvent(
        TRACE_EVENT_PHASE_END, p_data_->category_group_enabled, p_data_->name,
        v8::internal::tracing::kGlobalScope, v8::internal::tracing::kNoId,
        v8::internal::tracing::kNoId, TRACE_EVENT_FLAG_NONE);
  }
}

void CallStatsScopedTracer::Initialize(v8::internal::Isolate* isolate,
                                       const uint8_t* category_group_enabled,
                                       const char* name) {
  data_.isolate = isolate;
  data_.category_group_enabled = category_group_enabled;
  data_.name = name;
  p_data_ = &data_;
  RuntimeCallStats* table = isolate->counters()->runtime_call_stats();
  has_parent_scope_ = table->InUse();
  if (!has_parent_scope_) table->Reset();
  v8::internal::tracing::AddTraceEvent(
      TRACE_EVENT_PHASE_BEGIN, category_group_enabled, name,
      v8::internal::tracing::kGlobalScope, v8::internal::tracing::kNoId,
      TRACE_EVENT_FLAG_NONE, v8::internal::tracing::kNoId);
}

}  // namespace tracing
}  // namespace internal
}  // namespace v8
