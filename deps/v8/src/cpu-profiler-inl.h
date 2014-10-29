// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CPU_PROFILER_INL_H_
#define V8_CPU_PROFILER_INL_H_

#include "src/cpu-profiler.h"

#include <new>
#include "src/circular-queue-inl.h"
#include "src/profile-generator-inl.h"
#include "src/unbound-queue-inl.h"

namespace v8 {
namespace internal {

void CodeCreateEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->AddCode(start, entry, size);
  if (shared != NULL) {
    entry->set_shared_id(code_map->GetSharedId(shared));
  }
}


void CodeMoveEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->MoveCode(from, to);
}


void CodeDisableOptEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(start);
  if (entry != NULL) {
    entry->set_bailout_reason(bailout_reason);
  }
}


void SharedFunctionInfoMoveEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->MoveCode(from, to);
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


TickSample* CpuProfiler::StartTickSample() {
  if (is_profiling_) return processor_->StartTickSample();
  return NULL;
}


void CpuProfiler::FinishTickSample() {
  processor_->FinishTickSample();
}


TickSample* ProfilerEventsProcessor::StartTickSample() {
  void* address = ticks_buffer_.StartEnqueue();
  if (address == NULL) return NULL;
  TickSampleEventRecord* evt =
      new(address) TickSampleEventRecord(last_code_event_id_);
  return &evt->sample;
}


void ProfilerEventsProcessor::FinishTickSample() {
  ticks_buffer_.FinishEnqueue();
}

} }  // namespace v8::internal

#endif  // V8_CPU_PROFILER_INL_H_
