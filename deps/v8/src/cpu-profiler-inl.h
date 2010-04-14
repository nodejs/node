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

#ifdef ENABLE_CPP_PROFILES_PROCESSOR

#include "circular-queue-inl.h"
#include "profile-generator-inl.h"

namespace v8 {
namespace internal {

void CodeCreateEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->AddCode(start, entry, size);
}


void CodeMoveEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->MoveCode(from, to);
}


void CodeDeleteEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->DeleteCode(start);
}


void CodeAliasEventRecord::UpdateCodeMap(CodeMap* code_map) {
  code_map->AddAlias(alias, start);
}


TickSampleEventRecord* TickSampleEventRecord::init(void* value) {
  TickSampleEventRecord* result =
      reinterpret_cast<TickSampleEventRecord*>(value);
  result->filler = 1;
  ASSERT(result->filler != SamplingCircularQueue::kClear);
  // Init the required fields only.
  result->sample.pc = NULL;
  result->sample.frames_count = 0;
  return result;
}


TickSample* ProfilerEventsProcessor::TickSampleEvent() {
  TickSampleEventRecord* evt =
      TickSampleEventRecord::init(ticks_buffer_.Enqueue());
  evt->order = enqueue_order_;  // No increment!
  return &evt->sample;
}


bool ProfilerEventsProcessor::FilterOutCodeCreateEvent(
    Logger::LogEventsAndTags tag) {
  // In browser mode, leave only callbacks and non-native JS entries.
  // We filter out regular expressions as currently we can't tell
  // whether they origin from native scripts, so let's not confise people by
  // showing them weird regexes they didn't wrote.
  return FLAG_prof_browser_mode
      && (tag != Logger::CALLBACK_TAG
          && tag != Logger::FUNCTION_TAG
          && tag != Logger::LAZY_COMPILE_TAG
          && tag != Logger::SCRIPT_TAG);
}

} }  // namespace v8::internal

#endif  // ENABLE_CPP_PROFILES_PROCESSOR

#endif  // V8_CPU_PROFILER_INL_H_
