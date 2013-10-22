// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_CPU_PROFILER_INL_H_
#define V8_CPU_PROFILER_INL_H_

#include "cpu-profiler.h"

#include <new>
#include "circular-queue-inl.h"
#include "profile-generator-inl.h"
#include "unbound-queue-inl.h"

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
