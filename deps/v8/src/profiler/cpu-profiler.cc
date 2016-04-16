// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/cpu-profiler.h"

#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/locked-queue-inl.h"
#include "src/log-inl.h"
#include "src/profiler/cpu-profiler-inl.h"
#include "src/vm-state-inl.h"

#include "include/v8-profiler.h"

namespace v8 {
namespace internal {

static const int kProfilerStackSize = 64 * KB;


ProfilerEventsProcessor::ProfilerEventsProcessor(ProfileGenerator* generator,
                                                 Sampler* sampler,
                                                 base::TimeDelta period)
    : Thread(Thread::Options("v8:ProfEvntProc", kProfilerStackSize)),
      generator_(generator),
      sampler_(sampler),
      running_(1),
      period_(period),
      last_code_event_id_(0),
      last_processed_code_event_id_(0) {}


ProfilerEventsProcessor::~ProfilerEventsProcessor() {}


void ProfilerEventsProcessor::Enqueue(const CodeEventsContainer& event) {
  event.generic.order = last_code_event_id_.Increment(1);
  events_buffer_.Enqueue(event);
}


void ProfilerEventsProcessor::AddDeoptStack(Isolate* isolate, Address from,
                                            int fp_to_sp_delta) {
  TickSampleEventRecord record(last_code_event_id_.Value());
  RegisterState regs;
  Address fp = isolate->c_entry_fp(isolate->thread_local_top());
  regs.sp = fp - fp_to_sp_delta;
  regs.fp = fp;
  regs.pc = from;
  record.sample.Init(isolate, regs, TickSample::kSkipCEntryFrame, false);
  ticks_from_vm_buffer_.Enqueue(record);
}

void ProfilerEventsProcessor::AddCurrentStack(Isolate* isolate,
                                              bool update_stats) {
  TickSampleEventRecord record(last_code_event_id_.Value());
  RegisterState regs;
  StackFrameIterator it(isolate);
  if (!it.done()) {
    StackFrame* frame = it.frame();
    regs.sp = frame->sp();
    regs.fp = frame->fp();
    regs.pc = frame->pc();
  }
  record.sample.Init(isolate, regs, TickSample::kSkipCEntryFrame, update_stats);
  ticks_from_vm_buffer_.Enqueue(record);
}


void ProfilerEventsProcessor::StopSynchronously() {
  if (!base::NoBarrier_AtomicExchange(&running_, 0)) return;
  Join();
}


bool ProfilerEventsProcessor::ProcessCodeEvent() {
  CodeEventsContainer record;
  if (events_buffer_.Dequeue(&record)) {
    switch (record.generic.type) {
#define PROFILER_TYPE_CASE(type, clss)                          \
      case CodeEventRecord::type:                               \
        record.clss##_.UpdateCodeMap(generator_->code_map());   \
        break;

      CODE_EVENTS_TYPE_LIST(PROFILER_TYPE_CASE)

#undef PROFILER_TYPE_CASE
      default: return true;  // Skip record.
    }
    last_processed_code_event_id_ = record.generic.order;
    return true;
  }
  return false;
}

ProfilerEventsProcessor::SampleProcessingResult
    ProfilerEventsProcessor::ProcessOneSample() {
  TickSampleEventRecord record1;
  if (ticks_from_vm_buffer_.Peek(&record1) &&
      (record1.order == last_processed_code_event_id_)) {
    TickSampleEventRecord record;
    ticks_from_vm_buffer_.Dequeue(&record);
    generator_->RecordTickSample(record.sample);
    return OneSampleProcessed;
  }

  const TickSampleEventRecord* record = ticks_buffer_.Peek();
  if (record == NULL) {
    if (ticks_from_vm_buffer_.IsEmpty()) return NoSamplesInQueue;
    return FoundSampleForNextCodeEvent;
  }
  if (record->order != last_processed_code_event_id_) {
    return FoundSampleForNextCodeEvent;
  }
  generator_->RecordTickSample(record->sample);
  ticks_buffer_.Remove();
  return OneSampleProcessed;
}


void ProfilerEventsProcessor::Run() {
  while (!!base::NoBarrier_Load(&running_)) {
    base::TimeTicks nextSampleTime =
        base::TimeTicks::HighResolutionNow() + period_;
    base::TimeTicks now;
    SampleProcessingResult result;
    // Keep processing existing events until we need to do next sample
    // or the ticks buffer is empty.
    do {
      result = ProcessOneSample();
      if (result == FoundSampleForNextCodeEvent) {
        // All ticks of the current last_processed_code_event_id_ are
        // processed, proceed to the next code event.
        ProcessCodeEvent();
      }
      now = base::TimeTicks::HighResolutionNow();
    } while (result != NoSamplesInQueue && now < nextSampleTime);

    if (nextSampleTime > now) {
#if V8_OS_WIN
      // Do not use Sleep on Windows as it is very imprecise.
      // Could be up to 16ms jitter, which is unacceptable for the purpose.
      while (base::TimeTicks::HighResolutionNow() < nextSampleTime) {
      }
#else
      base::OS::Sleep(nextSampleTime - now);
#endif
    }

    // Schedule next sample. sampler_ is NULL in tests.
    if (sampler_) sampler_->DoSample();
  }

  // Process remaining tick events.
  do {
    SampleProcessingResult result;
    do {
      result = ProcessOneSample();
    } while (result == OneSampleProcessed);
  } while (ProcessCodeEvent());
}


void* ProfilerEventsProcessor::operator new(size_t size) {
  return AlignedAlloc(size, V8_ALIGNOF(ProfilerEventsProcessor));
}


void ProfilerEventsProcessor::operator delete(void* ptr) {
  AlignedFree(ptr);
}


int CpuProfiler::GetProfilesCount() {
  // The count of profiles doesn't depend on a security token.
  return profiles_->profiles()->length();
}


CpuProfile* CpuProfiler::GetProfile(int index) {
  return profiles_->profiles()->at(index);
}


void CpuProfiler::DeleteAllProfiles() {
  if (is_profiling_) StopProcessor();
  ResetProfiles();
}


void CpuProfiler::DeleteProfile(CpuProfile* profile) {
  profiles_->RemoveProfile(profile);
  delete profile;
  if (profiles_->profiles()->is_empty() && !is_profiling_) {
    // If this was the last profile, clean up all accessory data as well.
    ResetProfiles();
  }
}


void CpuProfiler::CallbackEvent(Name* name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = entry_point;
  rec->entry = profiles_->NewCodeEntry(
      Logger::CALLBACK_TAG,
      profiles_->GetName(name));
  rec->size = 1;
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                  Code* code,
                                  const char* name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = profiles_->NewCodeEntry(
      tag, profiles_->GetFunctionName(name), CodeEntry::kEmptyNamePrefix,
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  rec->size = code->ExecutableSize();
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                  Code* code,
                                  Name* name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = profiles_->NewCodeEntry(
      tag, profiles_->GetFunctionName(name), CodeEntry::kEmptyNamePrefix,
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  rec->size = code->ExecutableSize();
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag, Code* code,
                                  SharedFunctionInfo* shared,
                                  CompilationInfo* info, Name* script_name) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = profiles_->NewCodeEntry(
      tag, profiles_->GetFunctionName(shared->DebugName()),
      CodeEntry::kEmptyNamePrefix, profiles_->GetName(script_name),
      CpuProfileNode::kNoLineNumberInfo, CpuProfileNode::kNoColumnNumberInfo,
      NULL, code->instruction_start());
  if (info) {
    rec->entry->set_inlined_function_infos(info->inlined_function_infos());
  }
  rec->entry->FillFunctionInfo(shared);
  rec->size = code->ExecutableSize();
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag, Code* code,
                                  SharedFunctionInfo* shared,
                                  CompilationInfo* info, Name* script_name,
                                  int line, int column) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  Script* script = Script::cast(shared->script());
  JITLineInfoTable* line_table = NULL;
  if (script) {
    line_table = new JITLineInfoTable();
    for (RelocIterator it(code); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      if (RelocInfo::IsPosition(mode)) {
        int position = static_cast<int>(it.rinfo()->data());
        if (position >= 0) {
          int pc_offset = static_cast<int>(it.rinfo()->pc() - code->address());
          int line_number = script->GetLineNumber(position) + 1;
          line_table->SetPosition(pc_offset, line_number);
        }
      }
    }
  }
  rec->entry = profiles_->NewCodeEntry(
      tag, profiles_->GetFunctionName(shared->DebugName()),
      CodeEntry::kEmptyNamePrefix, profiles_->GetName(script_name), line,
      column, line_table, code->instruction_start());
  if (info) {
    rec->entry->set_inlined_function_infos(info->inlined_function_infos());
  }
  rec->entry->FillFunctionInfo(shared);
  rec->size = code->ExecutableSize();
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                  Code* code,
                                  int args_count) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = profiles_->NewCodeEntry(
      tag, profiles_->GetName(args_count), "args_count: ",
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  rec->size = code->ExecutableSize();
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeMoveEvent(Address from, Address to) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_MOVE);
  CodeMoveEventRecord* rec = &evt_rec.CodeMoveEventRecord_;
  rec->from = from;
  rec->to = to;
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeDisableOptEvent(Code* code, SharedFunctionInfo* shared) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DISABLE_OPT);
  CodeDisableOptEventRecord* rec = &evt_rec.CodeDisableOptEventRecord_;
  rec->start = code->address();
  rec->bailout_reason = GetBailoutReason(shared->disable_optimization_reason());
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::CodeDeoptEvent(Code* code, Address pc, int fp_to_sp_delta) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_DEOPT);
  CodeDeoptEventRecord* rec = &evt_rec.CodeDeoptEventRecord_;
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(code, pc);
  rec->start = code->address();
  rec->deopt_reason = Deoptimizer::GetDeoptReason(info.deopt_reason);
  rec->position = info.position;
  rec->pc_offset = pc - code->instruction_start();
  processor_->Enqueue(evt_rec);
  processor_->AddDeoptStack(isolate_, pc, fp_to_sp_delta);
}


void CpuProfiler::CodeDeleteEvent(Address from) {
}


void CpuProfiler::GetterCallbackEvent(Name* name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = entry_point;
  rec->entry = profiles_->NewCodeEntry(
      Logger::CALLBACK_TAG,
      profiles_->GetName(name),
      "get ");
  rec->size = 1;
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::RegExpCodeCreateEvent(Code* code, String* source) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = code->address();
  rec->entry = profiles_->NewCodeEntry(
      Logger::REG_EXP_TAG, profiles_->GetName(source), "RegExp: ",
      CodeEntry::kEmptyResourceName, CpuProfileNode::kNoLineNumberInfo,
      CpuProfileNode::kNoColumnNumberInfo, NULL, code->instruction_start());
  rec->size = code->ExecutableSize();
  processor_->Enqueue(evt_rec);
}


void CpuProfiler::SetterCallbackEvent(Name* name, Address entry_point) {
  CodeEventsContainer evt_rec(CodeEventRecord::CODE_CREATION);
  CodeCreateEventRecord* rec = &evt_rec.CodeCreateEventRecord_;
  rec->start = entry_point;
  rec->entry = profiles_->NewCodeEntry(
      Logger::CALLBACK_TAG,
      profiles_->GetName(name),
      "set ");
  rec->size = 1;
  processor_->Enqueue(evt_rec);
}


CpuProfiler::CpuProfiler(Isolate* isolate)
    : isolate_(isolate),
      sampling_interval_(base::TimeDelta::FromMicroseconds(
          FLAG_cpu_profiler_sampling_interval)),
      profiles_(new CpuProfilesCollection(isolate->heap())),
      generator_(NULL),
      processor_(NULL),
      is_profiling_(false) {
}


CpuProfiler::CpuProfiler(Isolate* isolate,
                         CpuProfilesCollection* test_profiles,
                         ProfileGenerator* test_generator,
                         ProfilerEventsProcessor* test_processor)
    : isolate_(isolate),
      sampling_interval_(base::TimeDelta::FromMicroseconds(
          FLAG_cpu_profiler_sampling_interval)),
      profiles_(test_profiles),
      generator_(test_generator),
      processor_(test_processor),
      is_profiling_(false) {
}


CpuProfiler::~CpuProfiler() {
  DCHECK(!is_profiling_);
  delete profiles_;
}


void CpuProfiler::set_sampling_interval(base::TimeDelta value) {
  DCHECK(!is_profiling_);
  sampling_interval_ = value;
}


void CpuProfiler::ResetProfiles() {
  delete profiles_;
  profiles_ = new CpuProfilesCollection(isolate()->heap());
}

void CpuProfiler::CollectSample() {
  if (processor_ != NULL) {
    processor_->AddCurrentStack(isolate_);
  }
}

void CpuProfiler::StartProfiling(const char* title, bool record_samples) {
  if (profiles_->StartProfiling(title, record_samples)) {
    StartProcessorIfNotStarted();
  }
}


void CpuProfiler::StartProfiling(String* title, bool record_samples) {
  StartProfiling(profiles_->GetName(title), record_samples);
  isolate_->debug()->feature_tracker()->Track(DebugFeatureTracker::kProfiler);
}


void CpuProfiler::StartProcessorIfNotStarted() {
  if (processor_ != NULL) {
    processor_->AddCurrentStack(isolate_);
    return;
  }
  Logger* logger = isolate_->logger();
  // Disable logging when using the new implementation.
  saved_is_logging_ = logger->is_logging_;
  logger->is_logging_ = false;
  generator_ = new ProfileGenerator(profiles_);
  Sampler* sampler = logger->sampler();
  processor_ = new ProfilerEventsProcessor(
      generator_, sampler, sampling_interval_);
  is_profiling_ = true;
  // Enumerate stuff we already have in the heap.
  DCHECK(isolate_->heap()->HasBeenSetUp());
  if (!FLAG_prof_browser_mode) {
    logger->LogCodeObjects();
  }
  logger->LogCompiledFunctions();
  logger->LogAccessorCallbacks();
  LogBuiltins();
  // Enable stack sampling.
  sampler->SetHasProcessingThread(true);
  sampler->IncreaseProfilingDepth();
  processor_->AddCurrentStack(isolate_);
  processor_->StartSynchronously();
}


CpuProfile* CpuProfiler::StopProfiling(const char* title) {
  if (!is_profiling_) return NULL;
  StopProcessorIfLastProfile(title);
  CpuProfile* result = profiles_->StopProfiling(title);
  if (result != NULL) {
    result->Print();
  }
  return result;
}


CpuProfile* CpuProfiler::StopProfiling(String* title) {
  if (!is_profiling_) return NULL;
  const char* profile_title = profiles_->GetName(title);
  StopProcessorIfLastProfile(profile_title);
  return profiles_->StopProfiling(profile_title);
}


void CpuProfiler::StopProcessorIfLastProfile(const char* title) {
  if (profiles_->IsLastProfile(title)) StopProcessor();
}


void CpuProfiler::StopProcessor() {
  Logger* logger = isolate_->logger();
  Sampler* sampler = reinterpret_cast<Sampler*>(logger->ticker_);
  is_profiling_ = false;
  processor_->StopSynchronously();
  delete processor_;
  delete generator_;
  processor_ = NULL;
  generator_ = NULL;
  sampler->SetHasProcessingThread(false);
  sampler->DecreaseProfilingDepth();
  logger->is_logging_ = saved_is_logging_;
}


void CpuProfiler::LogBuiltins() {
  Builtins* builtins = isolate_->builtins();
  DCHECK(builtins->is_initialized());
  for (int i = 0; i < Builtins::builtin_count; i++) {
    CodeEventsContainer evt_rec(CodeEventRecord::REPORT_BUILTIN);
    ReportBuiltinEventRecord* rec = &evt_rec.ReportBuiltinEventRecord_;
    Builtins::Name id = static_cast<Builtins::Name>(i);
    rec->start = builtins->builtin(id)->address();
    rec->builtin_id = id;
    processor_->Enqueue(evt_rec);
  }
}


}  // namespace internal
}  // namespace v8
