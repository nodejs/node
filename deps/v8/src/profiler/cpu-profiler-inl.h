// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_CPU_PROFILER_INL_H_
#define V8_PROFILER_CPU_PROFILER_INL_H_

#include "src/profiler/cpu-profiler.h"

#include <new>
#include "src/profiler/circular-queue-inl.h"
#include "src/profiler/profile-generator-inl.h"

namespace v8 {
namespace internal {

void CodeCreateEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->AddCode(instruction_start, entry, instruction_size);
}


void CodeMoveEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->MoveCode(from_instruction_start, to_instruction_start);
}


void CodeDisableOptEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(instruction_start);
  if (entry != nullptr) {
    entry->set_bailout_reason(bailout_reason);
  }
}


void CodeDeoptEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(instruction_start);
  if (entry != nullptr) {
    std::vector<CpuProfileDeoptFrame> frames_vector(
        deopt_frames, deopt_frames + deopt_frame_count);
    entry->set_deopt_info(deopt_reason, deopt_id, std::move(frames_vector));
  }
  delete[] deopt_frames;
}


void ReportBuiltinEventRecord::UpdateCodeMap(CodeMap* code_map) {
  CodeEntry* entry = code_map->FindEntry(instruction_start);
  if (entry) {
    entry->SetBuiltinId(builtin_id);
  } else if (builtin_id == Builtins::kGenericJSToWasmWrapper) {
    // Make sure to add the generic js-to-wasm wrapper builtin, because that
    // one is supposed to show up in profiles.
    entry = new CodeEntry(CodeEventListener::BUILTIN_TAG,
                          Builtins::name(builtin_id));
    code_map->AddCode(instruction_start, entry, instruction_size);
  }
}

TickSample* SamplingEventsProcessor::StartTickSample() {
  void* address = ticks_buffer_.StartEnqueue();
  if (address == nullptr) return nullptr;
  TickSampleEventRecord* evt =
      new (address) TickSampleEventRecord(last_code_event_id_);
  return &evt->sample;
}

void SamplingEventsProcessor::FinishTickSample() {
  ticks_buffer_.FinishEnqueue();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_CPU_PROFILER_INL_H_
