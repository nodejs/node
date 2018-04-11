// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/cpu-profiler.h"

#include "src/base/lazy-instance.h"
#include "src/base/platform/mutex.h"
#include "src/base/template-utils.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/locked-queue-inl.h"
#include "src/log-inl.h"
#include "src/profiler/cpu-profiler-inl.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

static const int kProfilerStackSize = 64 * KB;

class CpuSampler : public sampler::Sampler {
 public:
  CpuSampler(Isolate* isolate, ProfilerEventsProcessor* processor)
      : sampler::Sampler(reinterpret_cast<v8::Isolate*>(isolate)),
        processor_(processor) {}

  void SampleStack(const v8::RegisterState& regs) override {
    TickSample* sample = processor_->StartTickSample();
    if (sample == nullptr) return;
    Isolate* isolate = reinterpret_cast<Isolate*>(this->isolate());
    sample->Init(isolate, regs, TickSample::kIncludeCEntryFrame, true);
    if (is_counting_samples_ && !sample->timestamp.IsNull()) {
      if (sample->state == JS) ++js_sample_count_;
      if (sample->state == EXTERNAL) ++external_sample_count_;
    }
    processor_->FinishTickSample();
  }

 private:
  ProfilerEventsProcessor* processor_;
};

ProfilerEventsProcessor::ProfilerEventsProcessor(Isolate* isolate,
                                                 ProfileGenerator* generator,
                                                 base::TimeDelta period)
    : Thread(Thread::Options("v8:ProfEvntProc", kProfilerStackSize)),
      generator_(generator),
      sampler_(new CpuSampler(isolate, this)),
      running_(1),
      period_(period),
      last_code_event_id_(0),
      last_processed_code_event_id_(0) {
  sampler_->IncreaseProfilingDepth();
}

ProfilerEventsProcessor::~ProfilerEventsProcessor() {
  sampler_->DecreaseProfilingDepth();
}

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
  record.sample.Init(isolate, regs, TickSample::kSkipCEntryFrame, false, false);
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
  record.sample.Init(isolate, regs, TickSample::kSkipCEntryFrame, update_stats,
                     false);
  ticks_from_vm_buffer_.Enqueue(record);
}


void ProfilerEventsProcessor::StopSynchronously() {
  if (!base::Relaxed_AtomicExchange(&running_, 0)) return;
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
  if (record == nullptr) {
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
  while (!!base::Relaxed_Load(&running_)) {
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
      if (nextSampleTime - now < base::TimeDelta::FromMilliseconds(100)) {
        // Do not use Sleep on Windows as it is very imprecise, with up to 16ms
        // jitter, which is unacceptable for short profile intervals.
        while (base::TimeTicks::HighResolutionNow() < nextSampleTime) {
        }
      } else  // NOLINT
#endif
      {
        base::OS::Sleep(nextSampleTime - now);
      }
    }

    // Schedule next sample. sampler_ is nullptr in tests.
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
  return static_cast<int>(profiles_->profiles()->size());
}


CpuProfile* CpuProfiler::GetProfile(int index) {
  return profiles_->profiles()->at(index).get();
}


void CpuProfiler::DeleteAllProfiles() {
  if (is_profiling_) StopProcessor();
  ResetProfiles();
}


void CpuProfiler::DeleteProfile(CpuProfile* profile) {
  profiles_->RemoveProfile(profile);
  if (profiles_->profiles()->empty() && !is_profiling_) {
    // If this was the last profile, clean up all accessory data as well.
    ResetProfiles();
  }
}

void CpuProfiler::CodeEventHandler(const CodeEventsContainer& evt_rec) {
  switch (evt_rec.generic.type) {
    case CodeEventRecord::CODE_CREATION:
    case CodeEventRecord::CODE_MOVE:
    case CodeEventRecord::CODE_DISABLE_OPT:
      processor_->Enqueue(evt_rec);
      break;
    case CodeEventRecord::CODE_DEOPT: {
      const CodeDeoptEventRecord* rec = &evt_rec.CodeDeoptEventRecord_;
      Address pc = reinterpret_cast<Address>(rec->pc);
      int fp_to_sp_delta = rec->fp_to_sp_delta;
      processor_->Enqueue(evt_rec);
      processor_->AddDeoptStack(isolate_, pc, fp_to_sp_delta);
      break;
    }
    default:
      UNREACHABLE();
  }
}

namespace {

class CpuProfilersManager {
 public:
  void AddProfiler(Isolate* isolate, CpuProfiler* profiler) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    auto result = profilers_.insert(
        std::pair<Isolate*, std::unique_ptr<std::set<CpuProfiler*>>>(
            isolate, base::make_unique<std::set<CpuProfiler*>>()));
    result.first->second->insert(profiler);
  }

  void RemoveProfiler(Isolate* isolate, CpuProfiler* profiler) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    auto it = profilers_.find(isolate);
    DCHECK(it != profilers_.end());
    it->second->erase(profiler);
    if (it->second->empty()) {
      profilers_.erase(it);
    }
  }

  void CallCollectSample(Isolate* isolate) {
    base::LockGuard<base::Mutex> lock(&mutex_);
    auto profilers = profilers_.find(isolate);
    if (profilers == profilers_.end()) return;
    for (auto it : *profilers->second) {
      it->CollectSample();
    }
  }

 private:
  std::map<Isolate*, std::unique_ptr<std::set<CpuProfiler*>>> profilers_;
  base::Mutex mutex_;
};

base::LazyInstance<CpuProfilersManager>::type g_profilers_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

CpuProfiler::CpuProfiler(Isolate* isolate)
    : CpuProfiler(isolate, new CpuProfilesCollection(isolate), nullptr,
                  nullptr) {}

CpuProfiler::CpuProfiler(Isolate* isolate, CpuProfilesCollection* test_profiles,
                         ProfileGenerator* test_generator,
                         ProfilerEventsProcessor* test_processor)
    : isolate_(isolate),
      sampling_interval_(base::TimeDelta::FromMicroseconds(
          FLAG_cpu_profiler_sampling_interval)),
      profiles_(test_profiles),
      generator_(test_generator),
      processor_(test_processor),
      is_profiling_(false) {
  profiles_->set_cpu_profiler(this);
  g_profilers_manager.Pointer()->AddProfiler(isolate, this);
}

CpuProfiler::~CpuProfiler() {
  DCHECK(!is_profiling_);
  g_profilers_manager.Pointer()->RemoveProfiler(isolate_, this);
}

void CpuProfiler::set_sampling_interval(base::TimeDelta value) {
  DCHECK(!is_profiling_);
  sampling_interval_ = value;
}

void CpuProfiler::ResetProfiles() {
  profiles_.reset(new CpuProfilesCollection(isolate_));
  profiles_->set_cpu_profiler(this);
}

void CpuProfiler::CreateEntriesForRuntimeCallStats() {
  static_entries_.clear();
  RuntimeCallStats* rcs = isolate_->counters()->runtime_call_stats();
  CodeMap* code_map = generator_->code_map();
  for (int i = 0; i < RuntimeCallStats::kNumberOfCounters; ++i) {
    RuntimeCallCounter* counter = rcs->GetCounter(i);
    DCHECK(counter->name());
    std::unique_ptr<CodeEntry> entry(
        new CodeEntry(CodeEventListener::FUNCTION_TAG, counter->name(),
                      CodeEntry::kEmptyNamePrefix, "native V8Runtime"));
    code_map->AddCode(reinterpret_cast<Address>(counter), entry.get(), 1);
    static_entries_.push_back(std::move(entry));
  }
}

// static
void CpuProfiler::CollectSample(Isolate* isolate) {
  g_profilers_manager.Pointer()->CallCollectSample(isolate);
}

void CpuProfiler::CollectSample() {
  if (processor_) {
    processor_->AddCurrentStack(isolate_);
  }
}

void CpuProfiler::StartProfiling(const char* title, bool record_samples) {
  if (profiles_->StartProfiling(title, record_samples)) {
    TRACE_EVENT0("v8", "CpuProfiler::StartProfiling");
    StartProcessorIfNotStarted();
  }
}


void CpuProfiler::StartProfiling(String* title, bool record_samples) {
  StartProfiling(profiles_->GetName(title), record_samples);
  isolate_->debug()->feature_tracker()->Track(DebugFeatureTracker::kProfiler);
}


void CpuProfiler::StartProcessorIfNotStarted() {
  if (processor_) {
    processor_->AddCurrentStack(isolate_);
    return;
  }
  Logger* logger = isolate_->logger();
  // Disable logging when using the new implementation.
  saved_is_logging_ = logger->is_logging_;
  logger->is_logging_ = false;
  generator_.reset(new ProfileGenerator(profiles_.get()));
  processor_.reset(new ProfilerEventsProcessor(isolate_, generator_.get(),
                                               sampling_interval_));
  CreateEntriesForRuntimeCallStats();
  logger->SetUpProfilerListener();
  ProfilerListener* profiler_listener = logger->profiler_listener();
  profiler_listener->AddObserver(this);
  is_profiling_ = true;
  isolate_->set_is_profiling(true);
  // Enumerate stuff we already have in the heap.
  DCHECK(isolate_->heap()->HasBeenSetUp());
  if (!FLAG_prof_browser_mode) {
    logger->LogCodeObjects();
  }
  logger->LogCompiledFunctions();
  logger->LogAccessorCallbacks();
  LogBuiltins();
  // Enable stack sampling.
  processor_->AddCurrentStack(isolate_);
  processor_->StartSynchronously();
}

CpuProfile* CpuProfiler::StopProfiling(const char* title) {
  if (!is_profiling_) return nullptr;
  StopProcessorIfLastProfile(title);
  return profiles_->StopProfiling(title);
}

CpuProfile* CpuProfiler::StopProfiling(String* title) {
  return StopProfiling(profiles_->GetName(title));
}

void CpuProfiler::StopProcessorIfLastProfile(const char* title) {
  if (!profiles_->IsLastProfile(title)) return;
  StopProcessor();
}

void CpuProfiler::StopProcessor() {
  Logger* logger = isolate_->logger();
  is_profiling_ = false;
  isolate_->set_is_profiling(false);
  ProfilerListener* profiler_listener = logger->profiler_listener();
  profiler_listener->RemoveObserver(this);
  processor_->StopSynchronously();
  logger->TearDownProfilerListener();
  processor_.reset();
  generator_.reset();
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
