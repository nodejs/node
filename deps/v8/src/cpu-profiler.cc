// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "cpu-profiler-inl.h"

#include "compiler.h"
#include "frames-inl.h"
#include "hashmap.h"
#include "log-inl.h"
#include "vm-state-inl.h"

#include "../include/v8-profiler.h"

namespace v8 {
namespace internal {

static const int kTickSamplesBufferChunkSize = 64 * KB;
static const int kTickSamplesBufferChunksCount = 16;
static const int kProfilerStackSize = 64 * KB;


ProfilerEventsProcessor::ProfilerEventsProcessor(
    ProfileGenerator* generator, CpuProfilesCollection* profiles)
    : Thread(Thread::Options("v8:ProfEvntProc", kProfilerStackSize)),
      generator_(generator),
      profiles_(profiles),
      running_(true),
      ticks_buffer_(sizeof(TickSampleEventRecord),
                    kTickSamplesBufferChunkSize,
                    kTickSamplesBufferChunksCount),
      enqueue_order_(0) {
}


void ProfilerEventsProcessor::CallbackCreateEvent(Logger::LogEventsAndTags tag,
                                                  const char* prefix,
                                                  Name* name,
                                                  Address start) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = profiles_->NewCodeEntry(tag, prefix, name);
  rec->size = 1;
  rec->shared = NULL;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                              Name* name,
                                              String* resource_name,
                                              int line_number,
                                              Address start,
                                              unsigned size,
                                              Address shared,
                                              CompilationInfo* info) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = profiles_->NewCodeEntry(tag, name, resource_name, line_number);
  if (info) {
    rec->entry->set_no_frame_ranges(info->ReleaseNoFrameRanges());
  }
  rec->size = size;
  rec->shared = shared;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                              const char* name,
                                              Address start,
                                              unsigned size) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = profiles_->NewCodeEntry(tag, name);
  rec->size = size;
  rec->shared = NULL;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                              int args_count,
                                              Address start,
                                              unsigned size) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = profiles_->NewCodeEntry(tag, args_count);
  rec->size = size;
  rec->shared = NULL;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::CodeMoveEvent(Address from, Address to) {
  CodeEventsContainer evt_rec;
  CodeMoveEventRecord* rec = &evt_rec.CodeMoveEventRecord_;
  rec->type = CodeEventRecord::CODE_MOVE;
  rec->order = ++enqueue_order_;
  rec->from = from;
  rec->to = to;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::SharedFunctionInfoMoveEvent(Address from,
                                                          Address to) {
  CodeEventsContainer evt_rec;
  SharedFunctionInfoMoveEventRecord* rec =
      &evt_rec.SharedFunctionInfoMoveEventRecord_;
  rec->type = CodeEventRecord::SHARED_FUNC_MOVE;
  rec->order = ++enqueue_order_;
  rec->from = from;
  rec->to = to;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::RegExpCodeCreateEvent(
    Logger::LogEventsAndTags tag,
    const char* prefix,
    String* name,
    Address start,
    unsigned size) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = profiles_->NewCodeEntry(tag, prefix, name);
  rec->size = size;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::AddCurrentStack() {
  TickSampleEventRecord record(enqueue_order_);
  TickSample* sample = &record.sample;
  Isolate* isolate = Isolate::Current();
  sample->state = isolate->current_vm_state();
  sample->pc = reinterpret_cast<Address>(sample);  // Not NULL.
  for (StackTraceFrameIterator it(isolate);
       !it.done() && sample->frames_count < TickSample::kMaxFramesCount;
       it.Advance()) {
    sample->stack[sample->frames_count++] = it.frame()->pc();
  }
  ticks_from_vm_buffer_.Enqueue(record);
}


bool ProfilerEventsProcessor::ProcessCodeEvent(unsigned* dequeue_order) {
  if (!events_buffer_.IsEmpty()) {
    CodeEventsContainer record;
    events_buffer_.Dequeue(&record);
    switch (record.generic.type) {
#define PROFILER_TYPE_CASE(type, clss)                          \
      case CodeEventRecord::type:                               \
        record.clss##_.UpdateCodeMap(generator_->code_map());   \
        break;

      CODE_EVENTS_TYPE_LIST(PROFILER_TYPE_CASE)

#undef PROFILER_TYPE_CASE
      default: return true;  // Skip record.
    }
    *dequeue_order = record.generic.order;
    return true;
  }
  return false;
}


bool ProfilerEventsProcessor::ProcessTicks(unsigned dequeue_order) {
  while (true) {
    if (!ticks_from_vm_buffer_.IsEmpty()
        && ticks_from_vm_buffer_.Peek()->order == dequeue_order) {
      TickSampleEventRecord record;
      ticks_from_vm_buffer_.Dequeue(&record);
      generator_->RecordTickSample(record.sample);
    }

    const TickSampleEventRecord* rec =
        TickSampleEventRecord::cast(ticks_buffer_.StartDequeue());
    if (rec == NULL) return !ticks_from_vm_buffer_.IsEmpty();
    // Make a local copy of tick sample record to ensure that it won't
    // be modified as we are processing it. This is possible as the
    // sampler writes w/o any sync to the queue, so if the processor
    // will get far behind, a record may be modified right under its
    // feet.
    TickSampleEventRecord record = *rec;
    if (record.order == dequeue_order) {
      // A paranoid check to make sure that we don't get a memory overrun
      // in case of frames_count having a wild value.
      if (record.sample.frames_count < 0
          || record.sample.frames_count > TickSample::kMaxFramesCount)
        record.sample.frames_count = 0;
      generator_->RecordTickSample(record.sample);
      ticks_buffer_.FinishDequeue();
    } else {
      return true;
    }
  }
}


void ProfilerEventsProcessor::Run() {
  unsigned dequeue_order = 0;

  while (running_) {
    // Process ticks until we have any.
    if (ProcessTicks(dequeue_order)) {
      // All ticks of the current dequeue_order are processed,
      // proceed to the next code event.
      ProcessCodeEvent(&dequeue_order);
    }
    YieldCPU();
  }

  // Process remaining tick events.
  ticks_buffer_.FlushResidualRecords();
  // Perform processing until we have tick events, skip remaining code events.
  while (ProcessTicks(dequeue_order) && ProcessCodeEvent(&dequeue_order)) { }
}


int CpuProfiler::GetProfilesCount() {
  // The count of profiles doesn't depend on a security token.
  return profiles_->Profiles(TokenEnumerator::kNoSecurityToken)->length();
}


CpuProfile* CpuProfiler::GetProfile(Object* security_token, int index) {
  const int token = token_enumerator_->GetTokenId(security_token);
  return profiles_->Profiles(token)->at(index);
}


CpuProfile* CpuProfiler::FindProfile(Object* security_token, unsigned uid) {
  const int token = token_enumerator_->GetTokenId(security_token);
  return profiles_->GetProfile(token, uid);
}


TickSample* CpuProfiler::TickSampleEvent() {
  if (is_profiling_) return processor_->TickSampleEvent();
  return NULL;
}


void CpuProfiler::DeleteAllProfiles() {
  if (is_profiling_) StopProcessor();
  ResetProfiles();
}


void CpuProfiler::DeleteProfile(CpuProfile* profile) {
  profiles_->RemoveProfile(profile);
  delete profile;
}


bool CpuProfiler::HasDetachedProfiles() {
  return profiles_->HasDetachedProfiles();
}


void CpuProfiler::CallbackEvent(Name* name, Address entry_point) {
  processor_->CallbackCreateEvent(
      Logger::CALLBACK_TAG, CodeEntry::kEmptyNamePrefix, name, entry_point);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, const char* comment) {
  processor_->CodeCreateEvent(
      tag, comment, code->address(), code->ExecutableSize());
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, Name* name) {
  processor_->CodeCreateEvent(
      tag,
      name,
      isolate_->heap()->empty_string(),
      v8::CpuProfileNode::kNoLineNumberInfo,
      code->address(),
      code->ExecutableSize(),
      NULL,
      NULL);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                  Code* code,
                                  SharedFunctionInfo* shared,
                                  CompilationInfo* info,
                                  Name* name) {
  processor_->CodeCreateEvent(
      tag,
      name,
      isolate_->heap()->empty_string(),
      v8::CpuProfileNode::kNoLineNumberInfo,
      code->address(),
      code->ExecutableSize(),
      shared->address(),
      info);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                  Code* code,
                                  SharedFunctionInfo* shared,
                                  CompilationInfo* info,
                                  String* source, int line) {
  processor_->CodeCreateEvent(
      tag,
      shared->DebugName(),
      source,
      line,
      code->address(),
      code->ExecutableSize(),
      shared->address(),
      info);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, int args_count) {
  processor_->CodeCreateEvent(
      tag,
      args_count,
      code->address(),
      code->ExecutableSize());
}


void CpuProfiler::CodeMoveEvent(Address from, Address to) {
  processor_->CodeMoveEvent(from, to);
}


void CpuProfiler::CodeDeleteEvent(Address from) {
}


void CpuProfiler::SharedFunctionInfoMoveEvent(Address from, Address to) {
  processor_->SharedFunctionInfoMoveEvent(from, to);
}


void CpuProfiler::GetterCallbackEvent(Name* name, Address entry_point) {
  processor_->CallbackCreateEvent(
      Logger::CALLBACK_TAG, "get ", name, entry_point);
}


void CpuProfiler::RegExpCodeCreateEvent(Code* code, String* source) {
  processor_->RegExpCodeCreateEvent(
      Logger::REG_EXP_TAG,
      "RegExp: ",
      source,
      code->address(),
      code->ExecutableSize());
}


void CpuProfiler::SetterCallbackEvent(Name* name, Address entry_point) {
  processor_->CallbackCreateEvent(
      Logger::CALLBACK_TAG, "set ", name, entry_point);
}


CpuProfiler::CpuProfiler(Isolate* isolate)
    : isolate_(isolate),
      profiles_(new CpuProfilesCollection()),
      next_profile_uid_(1),
      token_enumerator_(new TokenEnumerator()),
      generator_(NULL),
      processor_(NULL),
      need_to_stop_sampler_(false),
      is_profiling_(false) {
}


CpuProfiler::~CpuProfiler() {
  delete token_enumerator_;
  delete profiles_;
}


void CpuProfiler::ResetProfiles() {
  delete profiles_;
  profiles_ = new CpuProfilesCollection();
}

void CpuProfiler::StartProfiling(const char* title, bool record_samples) {
  if (profiles_->StartProfiling(title, next_profile_uid_++, record_samples)) {
    StartProcessorIfNotStarted();
  }
  processor_->AddCurrentStack();
}


void CpuProfiler::StartProfiling(String* title, bool record_samples) {
  StartProfiling(profiles_->GetName(title), record_samples);
}


void CpuProfiler::StartProcessorIfNotStarted() {
  if (processor_ == NULL) {
    // Disable logging when using the new implementation.
    saved_logging_nesting_ = isolate_->logger()->logging_nesting_;
    isolate_->logger()->logging_nesting_ = 0;
    generator_ = new ProfileGenerator(profiles_);
    processor_ = new ProfilerEventsProcessor(generator_, profiles_);
    is_profiling_ = true;
    processor_->StartSynchronously();
    // Enumerate stuff we already have in the heap.
    if (isolate_->heap()->HasBeenSetUp()) {
      if (!FLAG_prof_browser_mode) {
        isolate_->logger()->LogCodeObjects();
      }
      isolate_->logger()->LogCompiledFunctions();
      isolate_->logger()->LogAccessorCallbacks();
    }
    // Enable stack sampling.
    Sampler* sampler = isolate_->logger()->sampler();
    sampler->IncreaseProfilingDepth();
    if (!sampler->IsActive()) {
      sampler->Start();
      need_to_stop_sampler_ = true;
    }
  }
}


CpuProfile* CpuProfiler::StopProfiling(const char* title) {
  if (!is_profiling_) return NULL;
  const double actual_sampling_rate = generator_->actual_sampling_rate();
  StopProcessorIfLastProfile(title);
  CpuProfile* result =
      profiles_->StopProfiling(TokenEnumerator::kNoSecurityToken,
                               title,
                               actual_sampling_rate);
  if (result != NULL) {
    result->Print();
  }
  return result;
}


CpuProfile* CpuProfiler::StopProfiling(Object* security_token, String* title) {
  if (!is_profiling_) return NULL;
  const double actual_sampling_rate = generator_->actual_sampling_rate();
  const char* profile_title = profiles_->GetName(title);
  StopProcessorIfLastProfile(profile_title);
  int token = token_enumerator_->GetTokenId(security_token);
  return profiles_->StopProfiling(token, profile_title, actual_sampling_rate);
}


void CpuProfiler::StopProcessorIfLastProfile(const char* title) {
  if (profiles_->IsLastProfile(title)) StopProcessor();
}


void CpuProfiler::StopProcessor() {
  Logger* logger = isolate_->logger();
  Sampler* sampler = reinterpret_cast<Sampler*>(logger->ticker_);
  sampler->DecreaseProfilingDepth();
  if (need_to_stop_sampler_) {
    sampler->Stop();
    need_to_stop_sampler_ = false;
  }
  is_profiling_ = false;
  processor_->Stop();
  processor_->Join();
  delete processor_;
  delete generator_;
  processor_ = NULL;
  generator_ = NULL;
  logger->logging_nesting_ = saved_logging_nesting_;
}


} }  // namespace v8::internal
