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

#include "v8.h"

#include "cpu-profiler-inl.h"

#ifdef ENABLE_CPP_PROFILES_PROCESSOR

#include "log-inl.h"

#include "../include/v8-profiler.h"

namespace v8 {
namespace internal {

static const int kEventsBufferSize = 256*KB;
static const int kTickSamplesBufferChunkSize = 64*KB;
static const int kTickSamplesBufferChunksCount = 16;


ProfilerEventsProcessor::ProfilerEventsProcessor(ProfileGenerator* generator)
    : generator_(generator),
      running_(false),
      events_buffer_(kEventsBufferSize),
      ticks_buffer_(sizeof(TickSampleEventRecord),
                    kTickSamplesBufferChunkSize,
                    kTickSamplesBufferChunksCount),
      enqueue_order_(0) { }


void ProfilerEventsProcessor::CallbackCreateEvent(Logger::LogEventsAndTags tag,
                                                  const char* prefix,
                                                  String* name,
                                                  Address start) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = generator_->NewCodeEntry(tag, prefix, name);
  rec->size = 1;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                              String* name,
                                              String* resource_name,
                                              int line_number,
                                              Address start,
                                              unsigned size) {
  if (FilterOutCodeCreateEvent(tag)) return;
  CodeEventsContainer evt_rec;
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->type = CodeEventRecord::CODE_CREATION;
  rec->order = ++enqueue_order_;
  rec->start = start;
  rec->entry = generator_->NewCodeEntry(tag, name, resource_name, line_number);
  rec->size = size;
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
  rec->entry = generator_->NewCodeEntry(tag, name);
  rec->size = size;
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
  rec->entry = generator_->NewCodeEntry(tag, args_count);
  rec->size = size;
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


void ProfilerEventsProcessor::CodeDeleteEvent(Address from) {
  CodeEventsContainer evt_rec;
  CodeDeleteEventRecord* rec = &evt_rec.CodeDeleteEventRecord_;
  rec->type = CodeEventRecord::CODE_DELETE;
  rec->order = ++enqueue_order_;
  rec->start = from;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::FunctionCreateEvent(Address alias,
                                                  Address start) {
  CodeEventsContainer evt_rec;
  CodeAliasEventRecord* rec = &evt_rec.CodeAliasEventRecord_;
  rec->type = CodeEventRecord::CODE_ALIAS;
  rec->order = ++enqueue_order_;
  rec->alias = alias;
  rec->start = start;
  events_buffer_.Enqueue(evt_rec);
}


void ProfilerEventsProcessor::FunctionMoveEvent(Address from, Address to) {
  CodeMoveEvent(from, to);
}


void ProfilerEventsProcessor::FunctionDeleteEvent(Address from) {
  CodeDeleteEvent(from);
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
  rec->entry = generator_->NewCodeEntry(tag, prefix, name);
  rec->size = size;
  events_buffer_.Enqueue(evt_rec);
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
    const TickSampleEventRecord* rec =
        TickSampleEventRecord::cast(ticks_buffer_.StartDequeue());
    if (rec == NULL) return false;
    if (rec->order == dequeue_order) {
      generator_->RecordTickSample(rec->sample);
      ticks_buffer_.FinishDequeue();
    } else {
      return true;
    }
  }
}


void ProfilerEventsProcessor::Run() {
  unsigned dequeue_order = 0;
  running_ = true;

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


CpuProfiler* CpuProfiler::singleton_ = NULL;

void CpuProfiler::StartProfiling(const char* title) {
  ASSERT(singleton_ != NULL);
  singleton_->StartCollectingProfile(title);
}


void CpuProfiler::StartProfiling(String* title) {
  ASSERT(singleton_ != NULL);
  singleton_->StartCollectingProfile(title);
}


CpuProfile* CpuProfiler::StopProfiling(const char* title) {
  ASSERT(singleton_ != NULL);
  return singleton_->StopCollectingProfile(title);
}


CpuProfile* CpuProfiler::StopProfiling(String* title) {
  ASSERT(singleton_ != NULL);
  return singleton_->StopCollectingProfile(title);
}


int CpuProfiler::GetProfilesCount() {
  ASSERT(singleton_ != NULL);
  return singleton_->profiles_->profiles()->length();
}


CpuProfile* CpuProfiler::GetProfile(int index) {
  ASSERT(singleton_ != NULL);
  return singleton_->profiles_->profiles()->at(index);
}


CpuProfile* CpuProfiler::FindProfile(unsigned uid) {
  ASSERT(singleton_ != NULL);
  return singleton_->profiles_->GetProfile(uid);
}


TickSample* CpuProfiler::TickSampleEvent() {
  if (CpuProfiler::is_profiling()) {
    return singleton_->processor_->TickSampleEvent();
  } else {
    return NULL;
  }
}


void CpuProfiler::CallbackEvent(String* name, Address entry_point) {
  singleton_->processor_->CallbackCreateEvent(
      Logger::CALLBACK_TAG, CodeEntry::kEmptyNamePrefix, name, entry_point);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, const char* comment) {
  singleton_->processor_->CodeCreateEvent(
      tag, comment, code->address(), code->ExecutableSize());
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, String* name) {
  singleton_->processor_->CodeCreateEvent(
      tag,
      name,
      Heap::empty_string(),
      v8::CpuProfileNode::kNoLineNumberInfo,
      code->address(),
      code->ExecutableSize());
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, String* name,
                           String* source, int line) {
  singleton_->processor_->CodeCreateEvent(
      tag,
      name,
      source,
      line,
      code->address(),
      code->ExecutableSize());
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                           Code* code, int args_count) {
  singleton_->processor_->CodeCreateEvent(
      tag,
      args_count,
      code->address(),
      code->ExecutableSize());
}


void CpuProfiler::CodeMoveEvent(Address from, Address to) {
  singleton_->processor_->CodeMoveEvent(from, to);
}


void CpuProfiler::CodeDeleteEvent(Address from) {
  singleton_->processor_->CodeDeleteEvent(from);
}


void CpuProfiler::FunctionCreateEvent(JSFunction* function) {
  singleton_->processor_->FunctionCreateEvent(
      function->address(), function->code()->address());
}


void CpuProfiler::FunctionMoveEvent(Address from, Address to) {
  singleton_->processor_->FunctionMoveEvent(from, to);
}


void CpuProfiler::FunctionDeleteEvent(Address from) {
  singleton_->processor_->FunctionDeleteEvent(from);
}


void CpuProfiler::GetterCallbackEvent(String* name, Address entry_point) {
  singleton_->processor_->CallbackCreateEvent(
      Logger::CALLBACK_TAG, "get ", name, entry_point);
}


void CpuProfiler::RegExpCodeCreateEvent(Code* code, String* source) {
  singleton_->processor_->RegExpCodeCreateEvent(
      Logger::REG_EXP_TAG,
      "RegExp: ",
      source,
      code->address(),
      code->ExecutableSize());
}


void CpuProfiler::SetterCallbackEvent(String* name, Address entry_point) {
  singleton_->processor_->CallbackCreateEvent(
      Logger::CALLBACK_TAG, "set ", name, entry_point);
}


CpuProfiler::CpuProfiler()
    : profiles_(new CpuProfilesCollection()),
      next_profile_uid_(1),
      generator_(NULL),
      processor_(NULL) {
}


CpuProfiler::~CpuProfiler() {
  delete profiles_;
}


void CpuProfiler::StartCollectingProfile(const char* title) {
  if (profiles_->StartProfiling(title, next_profile_uid_++)) {
    StartProcessorIfNotStarted();
  }
}


void CpuProfiler::StartCollectingProfile(String* title) {
  if (profiles_->StartProfiling(title, next_profile_uid_++)) {
    StartProcessorIfNotStarted();
  }
}


void CpuProfiler::StartProcessorIfNotStarted() {
  if (processor_ == NULL) {
    // Disable logging when using the new implementation.
    saved_logging_nesting_ = Logger::logging_nesting_;
    Logger::logging_nesting_ = 0;
    generator_ = new ProfileGenerator(profiles_);
    processor_ = new ProfilerEventsProcessor(generator_);
    processor_->Start();
    // Enumerate stuff we already have in the heap.
    if (Heap::HasBeenSetup()) {
      Logger::LogCodeObjects();
      Logger::LogCompiledFunctions();
      Logger::LogFunctionObjects();
      Logger::LogAccessorCallbacks();
    }
    // Enable stack sampling.
    reinterpret_cast<Sampler*>(Logger::ticker_)->Start();
  }
}


CpuProfile* CpuProfiler::StopCollectingProfile(const char* title) {
  StopProcessorIfLastProfile();
  CpuProfile* result = profiles_->StopProfiling(title);
  if (result != NULL) {
    result->Print();
  }
  return result;
}


CpuProfile* CpuProfiler::StopCollectingProfile(String* title) {
  StopProcessorIfLastProfile();
  return profiles_->StopProfiling(title);
}


void CpuProfiler::StopProcessorIfLastProfile() {
  if (profiles_->is_last_profile()) {
    reinterpret_cast<Sampler*>(Logger::ticker_)->Stop();
    processor_->Stop();
    processor_->Join();
    delete processor_;
    delete generator_;
    processor_ = NULL;
    generator_ = NULL;
    Logger::logging_nesting_ = saved_logging_nesting_;
  }
}

} }  // namespace v8::internal

#endif  // ENABLE_CPP_PROFILES_PROCESSOR

namespace v8 {
namespace internal {

void CpuProfiler::Setup() {
#ifdef ENABLE_CPP_PROFILES_PROCESSOR
  if (singleton_ == NULL) {
    singleton_ = new CpuProfiler();
  }
#endif
}


void CpuProfiler::TearDown() {
#ifdef ENABLE_CPP_PROFILES_PROCESSOR
  if (singleton_ != NULL) {
    delete singleton_;
  }
  singleton_ = NULL;
#endif
}

} }  // namespace v8::internal
