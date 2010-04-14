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

#ifdef ENABLE_CPP_PROFILES_PROCESSOR

#include "circular-queue.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CodeEntry;
class CodeMap;
class CpuProfile;
class CpuProfilesCollection;
class ProfileGenerator;


#define CODE_EVENTS_TYPE_LIST(V)                \
  V(CODE_CREATION, CodeCreateEventRecord)       \
  V(CODE_MOVE,     CodeMoveEventRecord)         \
  V(CODE_DELETE,   CodeDeleteEventRecord)       \
  V(CODE_ALIAS,    CodeAliasEventRecord)


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

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class CodeMoveEventRecord : public CodeEventRecord {
 public:
  Address from;
  Address to;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class CodeDeleteEventRecord : public CodeEventRecord {
 public:
  Address start;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class CodeAliasEventRecord : public CodeEventRecord {
 public:
  Address alias;
  Address start;

  INLINE(void UpdateCodeMap(CodeMap* code_map));
};


class TickSampleEventRecord BASE_EMBEDDED {
 public:
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

  INLINE(static TickSampleEventRecord* init(void* value));

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TickSampleEventRecord);
};


// This class implements both the profile events processor thread and
// methods called by event producers: VM and stack sampler threads.
class ProfilerEventsProcessor : public Thread {
 public:
  explicit ProfilerEventsProcessor(ProfileGenerator* generator);
  virtual ~ProfilerEventsProcessor() { }

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
                       Address start, unsigned size);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       const char* name,
                       Address start, unsigned size);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       int args_count,
                       Address start, unsigned size);
  void CodeMoveEvent(Address from, Address to);
  void CodeDeleteEvent(Address from);
  void FunctionCreateEvent(Address alias, Address start);
  void FunctionMoveEvent(Address from, Address to);
  void FunctionDeleteEvent(Address from);
  void RegExpCodeCreateEvent(Logger::LogEventsAndTags tag,
                             const char* prefix, String* name,
                             Address start, unsigned size);

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
  CircularQueue<CodeEventsContainer> events_buffer_;
  SamplingCircularQueue ticks_buffer_;
  unsigned enqueue_order_;
};

} }  // namespace v8::internal


#define PROFILE(Call)                                  \
  LOG(Call);                                           \
  do {                                                 \
    if (v8::internal::CpuProfiler::is_profiling()) {   \
      v8::internal::CpuProfiler::Call;                 \
    }                                                  \
  } while (false)
#else
#define PROFILE(Call) LOG(Call)
#endif  // ENABLE_CPP_PROFILES_PROCESSOR


namespace v8 {
namespace internal {

class CpuProfiler {
 public:
  static void Setup();
  static void TearDown();

#ifdef ENABLE_CPP_PROFILES_PROCESSOR
  static void StartProfiling(const char* title);
  static void StartProfiling(String* title);
  static CpuProfile* StopProfiling(const char* title);
  static CpuProfile* StopProfiling(String* title);
  static int GetProfilesCount();
  static CpuProfile* GetProfile(int index);
  static CpuProfile* FindProfile(unsigned uid);

  // Invoked from stack sampler (thread or signal handler.)
  static TickSample* TickSampleEvent();

  // Must be called via PROFILE macro, otherwise will crash when
  // profiling is not enabled.
  static void CallbackEvent(String* name, Address entry_point);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, const char* comment);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, String* name);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, String* name,
                              String* source, int line);
  static void CodeCreateEvent(Logger::LogEventsAndTags tag,
                              Code* code, int args_count);
  static void CodeMoveEvent(Address from, Address to);
  static void CodeDeleteEvent(Address from);
  static void FunctionCreateEvent(JSFunction* function);
  static void FunctionMoveEvent(Address from, Address to);
  static void FunctionDeleteEvent(Address from);
  static void GetterCallbackEvent(String* name, Address entry_point);
  static void RegExpCodeCreateEvent(Code* code, String* source);
  static void SetterCallbackEvent(String* name, Address entry_point);

  static INLINE(bool is_profiling()) {
    return singleton_ != NULL && singleton_->processor_ != NULL;
  }

 private:
  CpuProfiler();
  ~CpuProfiler();
  void StartCollectingProfile(const char* title);
  void StartCollectingProfile(String* title);
  void StartProcessorIfNotStarted();
  CpuProfile* StopCollectingProfile(const char* title);
  CpuProfile* StopCollectingProfile(String* title);
  void StopProcessorIfLastProfile();

  CpuProfilesCollection* profiles_;
  unsigned next_profile_uid_;
  ProfileGenerator* generator_;
  ProfilerEventsProcessor* processor_;
  int saved_logging_nesting_;

  static CpuProfiler* singleton_;

#else
  static INLINE(bool is_profiling()) { return false; }
#endif  // ENABLE_CPP_PROFILES_PROCESSOR

 private:
  DISALLOW_COPY_AND_ASSIGN(CpuProfiler);
};

} }  // namespace v8::internal


#endif  // V8_CPU_PROFILER_H_
