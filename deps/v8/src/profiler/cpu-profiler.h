// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_CPU_PROFILER_H_
#define V8_PROFILER_CPU_PROFILER_H_

#include <memory>

#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/base/atomicops.h"
#include "src/base/platform/time.h"
#include "src/isolate.h"
#include "src/libsampler/sampler.h"
#include "src/locked-queue.h"
#include "src/profiler/circular-queue.h"
#include "src/profiler/profiler-listener.h"
#include "src/profiler/tick-sample.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CodeEntry;
class CodeMap;
class CpuProfile;
class CpuProfilesCollection;
class ProfileGenerator;

#define CODE_EVENTS_TYPE_LIST(V)                         \
  V(CODE_CREATION, CodeCreateEventRecord)                \
  V(CODE_MOVE, CodeMoveEventRecord)                      \
  V(CODE_DISABLE_OPT, CodeDisableOptEventRecord)         \
  V(CODE_DEOPT, CodeDeoptEventRecord)                    \
  V(REPORT_BUILTIN, ReportBuiltinEventRecord)


class CodeEventRecord {
 public:
#define DECLARE_TYPE(type, ignore) type,
  enum Type {
    NONE = 0,
    CODE_EVENTS_TYPE_LIST(DECLARE_TYPE)
  };
#undef DECLARE_TYPE

  Type type;
  mutable unsigned order;
};


class CodeCreateEventRecord : public CodeEventRecord {
 public:
  Address instruction_start;
  CodeEntry* entry;
  unsigned instruction_size;

  V8_INLINE void UpdateCodeMap(CodeMap* code_map);
};


class CodeMoveEventRecord : public CodeEventRecord {
 public:
  Address from_instruction_start;
  Address to_instruction_start;

  V8_INLINE void UpdateCodeMap(CodeMap* code_map);
};


class CodeDisableOptEventRecord : public CodeEventRecord {
 public:
  Address instruction_start;
  const char* bailout_reason;

  V8_INLINE void UpdateCodeMap(CodeMap* code_map);
};


class CodeDeoptEventRecord : public CodeEventRecord {
 public:
  Address instruction_start;
  const char* deopt_reason;
  int deopt_id;
  Address pc;
  int fp_to_sp_delta;
  CpuProfileDeoptFrame* deopt_frames;
  int deopt_frame_count;

  V8_INLINE void UpdateCodeMap(CodeMap* code_map);
};


class ReportBuiltinEventRecord : public CodeEventRecord {
 public:
  Address instruction_start;
  Builtins::Name builtin_id;

  V8_INLINE void UpdateCodeMap(CodeMap* code_map);
};


class TickSampleEventRecord {
 public:
  // The parameterless constructor is used when we dequeue data from
  // the ticks buffer.
  TickSampleEventRecord() = default;
  explicit TickSampleEventRecord(unsigned order) : order(order) { }

  unsigned order;
  TickSample sample;
};


class CodeEventsContainer {
 public:
  explicit CodeEventsContainer(
      CodeEventRecord::Type type = CodeEventRecord::NONE) {
    generic.type = type;
  }
  union  {
    CodeEventRecord generic;
#define DECLARE_CLASS(ignore, type) type type##_;
    CODE_EVENTS_TYPE_LIST(DECLARE_CLASS)
#undef DECLARE_CLASS
  };
};


// This class implements both the profile events processor thread and
// methods called by event producers: VM and stack sampler threads.
class ProfilerEventsProcessor : public base::Thread, public CodeEventObserver {
 public:
  virtual ~ProfilerEventsProcessor();

  void CodeEventHandler(const CodeEventsContainer& evt_rec) override;

  // Thread control.
  void Run() override = 0;
  void StopSynchronously();
  V8_INLINE bool running() { return !!base::Relaxed_Load(&running_); }
  void Enqueue(const CodeEventsContainer& event);

  // Puts current stack into the tick sample events buffer.
  void AddCurrentStack(bool update_stats = false);
  void AddDeoptStack(Address from, int fp_to_sp_delta);
  // Add a sample into the tick sample events buffer. Used for testing.
  void AddSample(TickSample sample);

 protected:
  ProfilerEventsProcessor(Isolate* isolate, ProfileGenerator* generator);

  // Called from events processing thread (Run() method.)
  bool ProcessCodeEvent();

  enum SampleProcessingResult {
    OneSampleProcessed,
    FoundSampleForNextCodeEvent,
    NoSamplesInQueue
  };
  virtual SampleProcessingResult ProcessOneSample() = 0;

  ProfileGenerator* generator_;
  base::Atomic32 running_;
  LockedQueue<CodeEventsContainer> events_buffer_;
  LockedQueue<TickSampleEventRecord> ticks_from_vm_buffer_;
  std::atomic<unsigned> last_code_event_id_;
  unsigned last_processed_code_event_id_;
  Isolate* isolate_;
};

class SamplingEventsProcessor : public ProfilerEventsProcessor {
 public:
  SamplingEventsProcessor(Isolate* isolate, ProfileGenerator* generator,
                          base::TimeDelta period);
  ~SamplingEventsProcessor() override;

  // SamplingCircularQueue has stricter alignment requirements than a normal new
  // can fulfil, so we need to provide our own new/delete here.
  void* operator new(size_t size);
  void operator delete(void* ptr);

  void Run() override;

  // Tick sample events are filled directly in the buffer of the circular
  // queue (because the structure is of fixed width, but usually not all
  // stack frame entries are filled.) This method returns a pointer to the
  // next record of the buffer.
  // These methods are not thread-safe and should only ever be called by one
  // producer (from CpuSampler::SampleStack()). For testing, use AddSample.
  inline TickSample* StartTickSample();
  inline void FinishTickSample();

  sampler::Sampler* sampler() { return sampler_.get(); }

 private:
  SampleProcessingResult ProcessOneSample() override;

  static const size_t kTickSampleBufferSize = 512 * KB;
  static const size_t kTickSampleQueueLength =
      kTickSampleBufferSize / sizeof(TickSampleEventRecord);
  SamplingCircularQueue<TickSampleEventRecord,
                        kTickSampleQueueLength> ticks_buffer_;
  std::unique_ptr<sampler::Sampler> sampler_;
  const base::TimeDelta period_;  // Samples & code events processing period.
};

class CpuProfiler {
 public:
  explicit CpuProfiler(Isolate* isolate);

  CpuProfiler(Isolate* isolate, CpuProfilesCollection* profiles,
              ProfileGenerator* test_generator,
              ProfilerEventsProcessor* test_processor);

  ~CpuProfiler();

  static void CollectSample(Isolate* isolate);

  typedef v8::CpuProfilingMode ProfilingMode;

  void set_sampling_interval(base::TimeDelta value);
  void CollectSample();
  void StartProfiling(const char* title, bool record_samples = false,
                      ProfilingMode mode = ProfilingMode::kLeafNodeLineNumbers);
  void StartProfiling(String title, bool record_samples, ProfilingMode mode);
  CpuProfile* StopProfiling(const char* title);
  CpuProfile* StopProfiling(String title);
  int GetProfilesCount();
  CpuProfile* GetProfile(int index);
  void DeleteAllProfiles();
  void DeleteProfile(CpuProfile* profile);

  bool is_profiling() const { return is_profiling_; }

  ProfileGenerator* generator() const { return generator_.get(); }
  ProfilerEventsProcessor* processor() const { return processor_.get(); }
  Isolate* isolate() const { return isolate_; }

  ProfilerListener* profiler_listener_for_test() {
    return profiler_listener_.get();
  }

 private:
  void StartProcessorIfNotStarted();
  void StopProcessorIfLastProfile(const char* title);
  void StopProcessor();
  void ResetProfiles();
  void LogBuiltins();
  void CreateEntriesForRuntimeCallStats();

  Isolate* const isolate_;
  base::TimeDelta sampling_interval_;
  std::unique_ptr<CpuProfilesCollection> profiles_;
  std::unique_ptr<ProfileGenerator> generator_;
  std::unique_ptr<ProfilerEventsProcessor> processor_;
  std::unique_ptr<ProfilerListener> profiler_listener_;
  bool saved_is_logging_;
  bool is_profiling_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfiler);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_CPU_PROFILER_H_
