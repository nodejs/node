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

#ifdef ENABLE_LOGGING_AND_PROFILING

#include "frames-inl.h"
#include "hashmap.h"
#include "log-inl.h"

#include "../include/v8-profiler.h"

namespace v8 {
namespace internal {

static const int kEventsBufferSize = 256*KB;
static const int kTickSamplesBufferChunkSize = 64*KB;
static const int kTickSamplesBufferChunksCount = 16;


ProfilerEventsProcessor::ProfilerEventsProcessor(ProfileGenerator* generator)
    : generator_(generator),
      running_(true),
      ticks_buffer_(sizeof(TickSampleEventRecord),
                    kTickSamplesBufferChunkSize,
                    kTickSamplesBufferChunksCount),
      enqueue_order_(0),
      known_functions_(new HashMap(AddressesMatch)) {
}


ProfilerEventsProcessor::~ProfilerEventsProcessor() {
  delete known_functions_;
}


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
                                                  Address start,
                                                  int security_token_id) {
  CodeEventsContainer evt_rec;
  CodeAliasEventRecord* rec = &evt_rec.CodeAliasEventRecord_;
  rec->type = CodeEventRecord::CODE_ALIAS;
  rec->order = ++enqueue_order_;
  rec->start = alias;
  rec->entry = generator_->NewCodeEntry(security_token_id);
  rec->code_start = start;
  events_buffer_.Enqueue(evt_rec);

  known_functions_->Lookup(alias, AddressHash(alias), true);
}


void ProfilerEventsProcessor::FunctionMoveEvent(Address from, Address to) {
  CodeMoveEvent(from, to);

  if (IsKnownFunction(from)) {
    known_functions_->Remove(from, AddressHash(from));
    known_functions_->Lookup(to, AddressHash(to), true);
  }
}


void ProfilerEventsProcessor::FunctionDeleteEvent(Address from) {
  CodeDeleteEvent(from);

  known_functions_->Remove(from, AddressHash(from));
}


bool ProfilerEventsProcessor::IsKnownFunction(Address start) {
  HashMap::Entry* entry =
      known_functions_->Lookup(start, AddressHash(start), false);
  return entry != NULL;
}


void ProfilerEventsProcessor::ProcessMovedFunctions() {
  for (int i = 0; i < moved_functions_.length(); ++i) {
    JSFunction* function = moved_functions_[i];
    CpuProfiler::FunctionCreateEvent(function);
  }
  moved_functions_.Clear();
}


void ProfilerEventsProcessor::RememberMovedFunction(JSFunction* function) {
  moved_functions_.Add(function);
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


void ProfilerEventsProcessor::AddCurrentStack() {
  TickSampleEventRecord record;
  TickSample* sample = &record.sample;
  sample->state = VMState::current_state();
  sample->pc = reinterpret_cast<Address>(sample);  // Not NULL.
  sample->frames_count = 0;
  for (StackTraceFrameIterator it;
       !it.done() && sample->frames_count < TickSample::kMaxFramesCount;
       it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    sample->stack[sample->frames_count++] =
        reinterpret_cast<Address>(frame->function());
  }
  record.order = enqueue_order_;
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
          || record.sample.frames_count >= TickSample::kMaxFramesCount)
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
  return is_profiling() ? singleton_->StopCollectingProfile(title) : NULL;
}


CpuProfile* CpuProfiler::StopProfiling(Object* security_token, String* title) {
  return is_profiling() ?
      singleton_->StopCollectingProfile(security_token, title) : NULL;
}


int CpuProfiler::GetProfilesCount() {
  ASSERT(singleton_ != NULL);
  // The count of profiles doesn't depend on a security token.
  return singleton_->profiles_->Profiles(
      TokenEnumerator::kNoSecurityToken)->length();
}


CpuProfile* CpuProfiler::GetProfile(Object* security_token, int index) {
  ASSERT(singleton_ != NULL);
  const int token = singleton_->token_enumerator_->GetTokenId(security_token);
  return singleton_->profiles_->Profiles(token)->at(index);
}


CpuProfile* CpuProfiler::FindProfile(Object* security_token, unsigned uid) {
  ASSERT(singleton_ != NULL);
  const int token = singleton_->token_enumerator_->GetTokenId(security_token);
  return singleton_->profiles_->GetProfile(token, uid);
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
  int security_token_id = TokenEnumerator::kNoSecurityToken;
  if (function->unchecked_context()->IsContext()) {
    security_token_id = singleton_->token_enumerator_->GetTokenId(
        function->context()->global_context()->security_token());
  }
  singleton_->processor_->FunctionCreateEvent(
      function->address(),
      function->code()->address(),
      security_token_id);
}


void CpuProfiler::ProcessMovedFunctions() {
  singleton_->processor_->ProcessMovedFunctions();
}


void CpuProfiler::FunctionCreateEventFromMove(JSFunction* function) {
  // This function is called from GC iterators (during Scavenge,
  // MC, and MS), so marking bits can be set on objects. That's
  // why unchecked accessors are used here.

  // The same function can be reported several times.
  if (function->unchecked_code() == Builtins::builtin(Builtins::LazyCompile)
      || singleton_->processor_->IsKnownFunction(function->address())) return;

  singleton_->processor_->RememberMovedFunction(function);
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
      token_enumerator_(new TokenEnumerator()),
      generator_(NULL),
      processor_(NULL) {
}


CpuProfiler::~CpuProfiler() {
  delete token_enumerator_;
  delete profiles_;
}


void CpuProfiler::StartCollectingProfile(const char* title) {
  if (profiles_->StartProfiling(title, next_profile_uid_++)) {
    StartProcessorIfNotStarted();
  }
  processor_->AddCurrentStack();
}


void CpuProfiler::StartCollectingProfile(String* title) {
  StartCollectingProfile(profiles_->GetName(title));
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
      if (!FLAG_prof_browser_mode) {
        bool saved_log_code_flag = FLAG_log_code;
        FLAG_log_code = true;
        Logger::LogCodeObjects();
        FLAG_log_code = saved_log_code_flag;
      }
      Logger::LogCompiledFunctions();
      Logger::LogFunctionObjects();
      Logger::LogAccessorCallbacks();
    }
    // Enable stack sampling.
    reinterpret_cast<Sampler*>(Logger::ticker_)->Start();
  }
}


CpuProfile* CpuProfiler::StopCollectingProfile(const char* title) {
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


CpuProfile* CpuProfiler::StopCollectingProfile(Object* security_token,
                                               String* title) {
  const double actual_sampling_rate = generator_->actual_sampling_rate();
  const char* profile_title = profiles_->GetName(title);
  StopProcessorIfLastProfile(profile_title);
  int token = token_enumerator_->GetTokenId(security_token);
  return profiles_->StopProfiling(token, profile_title, actual_sampling_rate);
}


void CpuProfiler::StopProcessorIfLastProfile(const char* title) {
  if (profiles_->IsLastProfile(title)) {
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

#endif  // ENABLE_LOGGING_AND_PROFILING

namespace v8 {
namespace internal {

void CpuProfiler::Setup() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (singleton_ == NULL) {
    singleton_ = new CpuProfiler();
  }
#endif
}


void CpuProfiler::TearDown() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (singleton_ != NULL) {
    delete singleton_;
  }
  singleton_ = NULL;
#endif
}

} }  // namespace v8::internal
