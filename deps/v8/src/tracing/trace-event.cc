// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/trace-event.h"

#include <string.h>

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
        v8::internal::tracing::kNoId, TRACE_EVENT_FLAG_COPY,
        "runtime-call-stat",
        TRACE_STR_COPY(p_data_->isolate->trace_event_stats_table()->Dump()));
  } else {
    v8::internal::tracing::AddTraceEvent(
        TRACE_EVENT_PHASE_END, p_data_->category_group_enabled, p_data_->name,
        v8::internal::tracing::kGlobalScope, v8::internal::tracing::kNoId,
        v8::internal::tracing::kNoId, TRACE_EVENT_FLAG_NONE);
  }
}

void CallStatsScopedTracer::Initialize(Isolate* isolate,
                                       const uint8_t* category_group_enabled,
                                       const char* name) {
  data_.isolate = isolate;
  data_.category_group_enabled = category_group_enabled;
  data_.name = name;
  p_data_ = &data_;
  TraceEventStatsTable* table = isolate->trace_event_stats_table();
  has_parent_scope_ = table->InUse();
  if (!has_parent_scope_) table->Reset();
  v8::internal::tracing::AddTraceEvent(
      TRACE_EVENT_PHASE_BEGIN, category_group_enabled, name,
      v8::internal::tracing::kGlobalScope, v8::internal::tracing::kNoId,
      TRACE_EVENT_FLAG_NONE, v8::internal::tracing::kNoId);
}

void TraceEventStatsTable::Enter(Isolate* isolate,
                                 TraceEventCallStatsTimer* timer,
                                 CounterId counter_id) {
  TraceEventStatsTable* table = isolate->trace_event_stats_table();
  RuntimeCallCounter* counter = &(table->*counter_id);
  timer->Start(counter, table->current_timer_);
  table->current_timer_ = timer;
}

void TraceEventStatsTable::Leave(Isolate* isolate,
                                 TraceEventCallStatsTimer* timer) {
  TraceEventStatsTable* table = isolate->trace_event_stats_table();
  if (table->current_timer_ == timer) {
    table->current_timer_ = timer->Stop();
  }
}

void TraceEventStatsTable::Reset() {
  in_use_ = true;
  current_timer_ = nullptr;

#define RESET_COUNTER(name) this->name.Reset();
  FOR_EACH_MANUAL_COUNTER(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name, nargs, result_size) this->Runtime_##name.Reset();
  FOR_EACH_INTRINSIC(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name) this->Builtin_##name.Reset();
  BUILTIN_LIST_C(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name) this->API_##name.Reset();
  FOR_EACH_API_COUNTER(RESET_COUNTER)
#undef RESET_COUNTER

#define RESET_COUNTER(name) this->Handler_##name.Reset();
  FOR_EACH_HANDLER_COUNTER(RESET_COUNTER)
#undef RESET_COUNTER
}

const char* TraceEventStatsTable::Dump() {
  buffer_.str(std::string());
  buffer_.clear();
  buffer_ << "{";
#define DUMP_COUNTER(name) \
  if (this->name.count > 0) this->name.Dump(buffer_);
  FOR_EACH_MANUAL_COUNTER(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name, nargs, result_size) \
  if (this->Runtime_##name.count > 0) this->Runtime_##name.Dump(buffer_);
  FOR_EACH_INTRINSIC(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name) \
  if (this->Builtin_##name.count > 0) this->Builtin_##name.Dump(buffer_);
  BUILTIN_LIST_C(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name) \
  if (this->API_##name.count > 0) this->API_##name.Dump(buffer_);
  FOR_EACH_API_COUNTER(DUMP_COUNTER)
#undef DUMP_COUNTER

#define DUMP_COUNTER(name) \
  if (this->Handler_##name.count > 0) this->Handler_##name.Dump(buffer_);
  FOR_EACH_HANDLER_COUNTER(DUMP_COUNTER)
#undef DUMP_COUNTER
  buffer_ << "\"END\":[]}";
  const std::string& buffer_str = buffer_.str();
  size_t length = buffer_str.size();
  if (length > len_) {
    buffer_c_str_.reset(new char[length + 1]);
    len_ = length;
  }
  strncpy(buffer_c_str_.get(), buffer_str.c_str(), length + 1);
  in_use_ = false;
  return buffer_c_str_.get();
}

}  // namespace tracing
}  // namespace internal
}  // namespace v8
