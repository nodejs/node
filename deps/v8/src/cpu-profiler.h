// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CPU_PROFILER_H_
#define V8_CPU_PROFILER_H_

#include "src/allocation.h"
#include "src/base/atomicops.h"
#include "src/base/platform/time.h"
#include "src/circular-queue.h"
#include "src/sampler.h"
#include "src/unbound-queue.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CodeEntry;
class CodeMap;
class CompilationInfo;
class CpuProfile;
class CpuProfilesCollection;
class ProfileGenerator;

#define CODE_EVENTS_TYPE_LIST(V)                         \
  V(CODE_CREATION, CodeCreateEventRecord)                \
  V(CODE_MOVE, CodeMoveEventRecord)                      \
  V(CODE_DISABLE_OPT, CodeDisableOptEventRecord)         \
  V(CODE_DEOPT, CodeDeoptEventRecord)                    \
  V(SHARED_FUNC_MOVE, SharedFunctionInfoMoveEventRecord) \
  V(REPORT_BUILTIN, ReportBuiltinEventRecord)


class CodeEventRecord {
 public:
#define DECLARE_TYPE(type, ignore) type,
  enum Type {
    NONE = 0,
    CODE_EVENTS_TYPE_LIST(DECLARE_TYPE)
    NUMBER_OF_TYPES
  };
#undef DECLARE_TYPE

  Type type;
  mutable unsigned order;
};


class CodeCreateEventRecord : public CodeEventRecord {
 public:
  Address start;
  CodeEntry* entry;
  unsigned size;
  Address shared;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class CodeMoveEventRecord : public CodeEventRecord {
 public:
  Address from;
  Address to;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class CodeDisableOptEventRecord : public CodeEventRecord {
 public:
  Address start;
  const char* bailout_reason;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class CodeDeoptEventRecord : public CodeEventRecord {
 public:
  Address start;
  const char* deopt_reason;
  int raw_position;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class SharedFunctionInfoMoveEventRecord : public CodeEventRecord {
 public:
  Address from;
  Address to;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class ReportBuiltinEventRecord : public CodeEventRecord {
 public:
  Address start;
  Builtins::Name builtin_id;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class TickSampleEventRecord {
 public:
  // The parameterless constructor is used when we dequeue data from
  // the ticks buffer.
  TickSampleEventRecord() { }
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
#undef DECLARE_TYPE
  };
};


// This class implements both the profile events processor thread and
// methods called by event producers: VM and stack sampler threads.
class ProfilerEventsProcessor : public base::Thread {
 public:
  ProfilerEventsProcessor(ProfileGenerator* generator,
                          Sampler* sampler,
                          base::TimeDelta period);
  virtual ~ProfilerEventsProcessor() {}

  // Thread control.
  virtual void Run();
  void StopSynchronously();
  INLINE(bool running()) { return !!base::NoBarrier_Load(&running_); }
  void Enqueue(const CodeEventsContainer& event);

  // Puts current stack into tick sample events buffer.
  void AddCurrentStack(Isolate* isolate);
  void AddDeoptStack(Isolate* isolate, Address from, int fp_to_sp_delta);

  // Tick sample events are filled directly in the buffer of the circular
  // queue (because the structure is of fixed width, but usually not all
  // stack frame entries are filled.) This method returns a pointer to the
  // next record of the buffer.
  inline TickSample* StartTickSample();
  inline void FinishTickSample();

  // SamplingCircularQueue has stricter alignment requirements than a normal new
  // can fulfil, so we need to provide our own new/delete here.
  void* operator new(size_t size);
  void operator delete(void* ptr);

 private:
  // Called from events processing thread (Run() method.)
  bool ProcessCodeEvent();

  enum SampleProcessingResult {
    OneSampleProcessed,
    FoundSampleForNextCodeEvent,
    NoSamplesInQueue
  };
  SampleProcessingResult ProcessOneSample();

  ProfileGenerator* generator_;
  Sampler* sampler_;
  base::Atomic32 running_;
  // Sampling period in microseconds.
  const base::TimeDelta period_;
  UnboundQueue<CodeEventsContainer> events_buffer_;
  static const size_t kTickSampleBufferSize = 1 * MB;
  static const size_t kTickSampleQueueLength =
      kTickSampleBufferSize / sizeof(TickSampleEventRecord);
  SamplingCircularQueue<TickSampleEventRecord,
                        kTickSampleQueueLength> ticks_buffer_;
  UnboundQueue<TickSampleEventRecord> ticks_from_vm_buffer_;
  unsigned last_code_event_id_;
  unsigned last_processed_code_event_id_;
};


#define PROFILE(IsolateGetter, Call)                                        \
  do {                                                                      \
    Isolate* cpu_profiler_isolate = (IsolateGetter);                        \
    v8::internal::Logger* logger = cpu_profiler_isolate->logger();          \
    CpuProfiler* cpu_profiler = cpu_profiler_isolate->cpu_profiler();       \
    if (logger->is_logging_code_events() || cpu_profiler->is_profiling()) { \
      logger->Call;                                                         \
    }                                                                       \
  } while (false)


class CpuProfiler : public CodeEventListener {
 public:
  explicit CpuProfiler(Isolate* isolate);

  CpuProfiler(Isolate* isolate,
              CpuProfilesCollection* test_collection,
              ProfileGenerator* test_generator,
              ProfilerEventsProcessor* test_processor);

  virtual ~CpuProfiler();

  void set_sampling_interval(base::TimeDelta value);
  void StartProfiling(const char* title, bool record_samples = false);
  void StartProfiling(String* title, bool record_samples);
  CpuProfile* StopProfiling(const char* title);
  CpuProfile* StopProfiling(String* title);
  int GetProfilesCount();
  CpuProfile* GetProfile(int index);
  void DeleteAllProfiles();
  void DeleteProfile(CpuProfile* profile);

  // Invoked from stack sampler (thread or signal handler.)
  inline TickSample* StartTickSample();
  inline void FinishTickSample();

  // Must be called via PROFILE macro, otherwise will crash when
  // profiling is not enabled.
  virtual void CallbackEvent(Name* name, Address entry_point);
  virtual void CodeCreateEvent(Logger::LogEventsAndTags tag,
                               Code* code, const char* comment);
  virtual void CodeCreateEvent(Logger::LogEventsAndTags tag,
                               Code* code, Name* name);
  virtual void CodeCreateEvent(Logger::LogEventsAndTags tag, Code* code,
                               SharedFunctionInfo* shared,
                               CompilationInfo* info, Name* script_name);
  virtual void CodeCreateEvent(Logger::LogEventsAndTags tag, Code* code,
                               SharedFunctionInfo* shared,
                               CompilationInfo* info, Name* script_name,
                               int line, int column);
  virtual void CodeCreateEvent(Logger::LogEventsAndTags tag,
                               Code* code, int args_count);
  virtual void CodeMovingGCEvent() {}
  virtual void CodeMoveEvent(Address from, Address to);
  virtual void CodeDisableOptEvent(Code* code, SharedFunctionInfo* shared);
  virtual void CodeDeoptEvent(Code* code, int bailout_id, Address pc,
                              int fp_to_sp_delta);
  virtual void CodeDeleteEvent(Address from);
  virtual void GetterCallbackEvent(Name* name, Address entry_point);
  virtual void RegExpCodeCreateEvent(Code* code, String* source);
  virtual void SetterCallbackEvent(Name* name, Address entry_point);
  virtual void SharedFunctionInfoMoveEvent(Address from, Address to);

  INLINE(bool is_profiling() const) { return is_profiling_; }
  bool* is_profiling_address() {
    return &is_profiling_;
  }

  ProfileGenerator* generator() const { return generator_; }
  ProfilerEventsProcessor* processor() const { return processor_; }
  Isolate* isolate() const { return isolate_; }

 private:
  void StartProcessorIfNotStarted();
  void StopProcessorIfLastProfile(const char* title);
  void StopProcessor();
  void ResetProfiles();
  void LogBuiltins();

  Isolate* isolate_;
  base::TimeDelta sampling_interval_;
  CpuProfilesCollection* profiles_;
  ProfileGenerator* generator_;
  ProfilerEventsProcessor* processor_;
  bool saved_is_logging_;
  bool is_profiling_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfiler);
};

} }  // namespace v8::internal


#endif  // V8_CPU_PROFILER_H_
