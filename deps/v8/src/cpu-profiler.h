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

#ifndef V8_CPU_PROFILER_H_
#define V8_CPU_PROFILER_H_

#include "allocation.h"
#include "atomicops.h"
#include "circular-queue.h"
#include "unbound-queue.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CodeEntry;
class CodeMap;
class CpuProfile;
class CpuProfilesCollection;
class HashMap;
class ProfileGenerator;
class TokenEnumerator;

#define CODE_EVENTS_TYPE_LIST(V)                                   \
  V(CODE_CREATION,    CodeCreateEventRecord)                       \
  V(CODE_MOVE,        CodeMoveEventRecord)                         \
  V(SHARED_FUNC_MOVE, SharedFunctionInfoMoveEventRecord)


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
  unsigned order;
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


class SharedFunctionInfoMoveEventRecord : public CodeEventRecord {
 public:
  Address from;
  Address to;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class TickSampleEventRecord {
 public:
  // The parameterless constructor is used when we dequeue data from
  // the ticks buffer.
  TickSampleEventRecord() { }
  explicit TickSampleEventRecord(unsigned order)
      : filler(1),
        order(order) {
    ASSERT(filler != SamplingCircularQueue::kClear);
  }

  // The first machine word of a TickSampleEventRecord must not ever
  // become equal to SamplingCircularQueue::kClear.  As both order and
  // TickSample's first field are not reliable in this sense (order
  // can overflow, TickSample can have all fields reset), we are
  // forced to use an artificial filler field.
  int filler;
  unsigned order;
  TickSample sample;

  static TickSampleEventRecord* cast(void* value) {
    return reinterpret_cast<TickSampleEventRecord*>(value);
  }
};


// This class implements both the profile events processor thread and
// methods called by event producers: VM and stack sampler threads.
class ProfilerEventsProcessor : public Thread {
 public:
  explicit ProfilerEventsProcessor(ProfileGenerator* generator);
  virtual ~ProfilerEventsProcessor() {}

  // Thread control.
  virtual void Run();
  inline void Stop() { running_ = false; }
  INLINE(bool running()) { return running_; }

  // Events adding methods. Called by VM threads.
  void CallbackCreateEvent(Logger::LogEventsAndTags tag,
                           const char* prefix, String* name,
                           Address start);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       String* name,
                       String* resource_name, int line_number,
                       Address start, unsigned size,
                       Address shared);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       const char* name,
                       Address start, unsigned size);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       int args_count,
                       Address start, unsigned size);
  void CodeMoveEvent(Address from, Address to);
  void CodeDeleteEvent(Address from);
  void SharedFunctionInfoMoveEvent(Address from, Address to);
  void RegExpCodeCreateEvent(Logger::LogEventsAndTags tag,
                             const char* prefix, String* name,
                             Address start, unsigned size);
  // Puts current stack into tick sample events buffer.
  void AddCurrentStack();

  // Tick sample events are filled directly in the buffer of the circular
  // queue (because the structure is of fixed width, but usually not all
  // stack frame entries are filled.) This method returns a pointer to the
  // next record of the buffer.
  INLINE(TickSample* TickSampleEvent());

 private:
  union CodeEventsContainer {
    CodeEventRecord generic;
#define DECLARE_CLASS(ignore, type) type type##_;
    CODE_EVENTS_TYPE_LIST(DECLARE_CLASS)
#undef DECLARE_TYPE
  };

  // Called from events processing thread (Run() method.)
  bool ProcessCodeEvent(unsigned* dequeue_order);
  bool ProcessTicks(unsigned dequeue_order);

  INLINE(static bool FilterOutCodeCreateEvent(Logger::LogEventsAndTags tag));

  ProfileGenerator* generator_;
  bool running_;
  UnboundQueue<CodeEventsContainer> events_buffer_;
  SamplingCircularQueue ticks_buffer_;
  UnboundQueue<TickSampleEventRecord> ticks_from_vm_buffer_;
  unsigned enqueue_order_;
};

} }  // namespace v8::internal


#define PROFILE(isolate, Call)                                \
  LOG(isolate, Call);                                         \
  do {                                                        \
    if (v8::internal::CpuProfiler::is_profiling(isolate)) {   \
      v8::internal::CpuProfiler::Call;                        \
    }                                                         \
  } while (false)


namespace v8 {
namespace internal {


// TODO(isolates): isolatify this class.
class CpuProfiler {
 public:
  static void Setup();
  static void TearDown();

  static void StartProfiling(const char* title);
  static void StartProfiling(String* title);
  static CpuProfile* StopProfiling(const char* title);
  static CpuProfile* StopProfiling(Object* security_token, String* title);
  static int GetProfilesCount();
  static CpuProfile* GetProfile(Object* security_token, int index);
  static CpuProfile* FindProfile(Object* security_token, unsigned uid);
  static void DeleteAllProfiles();
  static void DeleteProfile(CpuProfile* profile);
  static bool HasDetachedProfiles();

  // Invoked from stack sampler (thread or signal handler.)
  static TickSample* TickSampleEvent(Isolate* isolate);

  // Must be called via PROFILE macro, otherwise will crash when
  // profiling is not enabled.
  static void CallbackEvent(String* name, Address entry_point);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, const char* comment);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, String* name);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code,
                              SharedFunctionInfo *shared,
                              String* name);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code,
                              SharedFunctionInfo *shared,
                              String* source, int line);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, int args_count);
  static void CodeMovingGCEvent() {}
  static void CodeMoveEvent(Address from, Address to);
  static void CodeDeleteEvent(Address from);
  static void GetterCallbackEvent(String* name, Address entry_point);
  static void RegExpCodeCreateEvent(Code* code, String* source);
  static void SetterCallbackEvent(String* name, Address entry_point);
  static void SharedFunctionInfoMoveEvent(Address from, Address to);

  // TODO(isolates): this doesn't have to use atomics anymore.

  static INLINE(bool is_profiling(Isolate* isolate)) {
    CpuProfiler* profiler = isolate->cpu_profiler();
    return profiler != NULL && NoBarrier_Load(&profiler->is_profiling_);
  }

 private:
  CpuProfiler();
  ~CpuProfiler();
  void StartCollectingProfile(const char* title);
  void StartCollectingProfile(String* title);
  void StartProcessorIfNotStarted();
  CpuProfile* StopCollectingProfile(const char* title);
  CpuProfile* StopCollectingProfile(Object* security_token, String* title);
  void StopProcessorIfLastProfile(const char* title);
  void StopProcessor();
  void ResetProfiles();

  CpuProfilesCollection* profiles_;
  unsigned next_profile_uid_;
  TokenEnumerator* token_enumerator_;
  ProfileGenerator* generator_;
  ProfilerEventsProcessor* processor_;
  int saved_logging_nesting_;
  bool need_to_stop_sampler_;
  Atomic32 is_profiling_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CpuProfiler);
};

} }  // namespace v8::internal


#endif  // V8_CPU_PROFILER_H_
