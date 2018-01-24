// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_CPU_PROFILER_INL_H_
#define V8_PROFILER_CPU_PROFILER_INL_H_

#include "src/profiler/cpu-profiler.h"

#include <new>
#include "src/profiler/circular-queue-inl.h"
#include "src/profiler/profile-generator-inl.h"
#include "src/profiler/unbound-queue-inl.h"

namespace v8 {
namespace internal {

void CodeCreateEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->AddCode(start, entry, size);
}


void CodeMoveEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->MoveCode(from, to);
}


void CodeDisableOptEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(start);
  if (entry != nullptr) {
    entry->set_bailout_reason(bailout_reason);
  }
}


void CodeDeoptEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(start);
  if (entry != nullptr) entry->set_deopt_info(deopt_reason, deopt_id);
}


void ReportBuiltinEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(start);
  if (!entry) {
    // Code objects for builtins should already have been added to the map but
    // some of them have been filtered out by CpuProfiler.
    return;
  }
  entry->SetBuiltinId(builtin_id);
}


TickSample* ProfilerEventsProcessor::StartTickSample() {
  void* address = ticks_buffer_.StartEnqueue();
  if (address == nullptr) return nullptr;
  TickSampleEventRecord* evt =
      new (address) TickSampleEventRecord(last_code_event_id_.Value());
  return &evt->sample;
}


void ProfilerEventsProcessor::FinishTickSample() {
  ticks_buffer_.FinishEnqueue();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_CPU_PROFILER_INL_H_
