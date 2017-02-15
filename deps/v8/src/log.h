// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOG_H_
#define V8_LOG_H_

#include <string>

#include "src/allocation.h"
#include "src/base/compiler-specific.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"
#include "src/code-events.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {

struct TickSample;

namespace sampler {
class Sampler;
}

namespace internal {

// Logger is used for collecting logging information from V8 during
// execution. The result is dumped to a file.
//
// Available command line flags:
//
//  --log
// Minimal logging (no API, code, or GC sample events), default is off.
//
// --log-all
// Log all events to the file, default is off.  This is the same as combining
// --log-api, --log-code, --log-gc, and --log-regexp.
//
// --log-api
// Log API events to the logfile, default is off.  --log-api implies --log.
//
// --log-code
// Log code (create, move, and delete) events to the logfile, default is off.
// --log-code implies --log.
//
// --log-gc
// Log GC heap samples after each GC that can be processed by hp2ps, default
// is off.  --log-gc implies --log.
//
// --log-regexp
// Log creation and use of regular expressions, Default is off.
// --log-regexp implies --log.
//
// --logfile <filename>
// Specify the name of the logfile, default is "v8.log".
//
// --prof
// Collect statistical profiling information (ticks), default is off.  The
// tick profiler requires code events, so --prof implies --log-code.

// Forward declarations.
class CodeEventListener;
class CpuProfiler;
class Isolate;
class JitLogger;
class Log;
class LowLevelLogger;
class PerfBasicLogger;
class PerfJitLogger;
class Profiler;
class ProfilerListener;
class RuntimeCallTimer;
class Ticker;

#undef LOG
#define LOG(isolate, Call)                              \
  do {                                                  \
    v8::internal::Logger* logger = (isolate)->logger(); \
    if (logger->is_logging()) logger->Call;             \
  } while (false)

#define LOG_CODE_EVENT(isolate, Call)                   \
  do {                                                  \
    v8::internal::Logger* logger = (isolate)->logger(); \
    if (logger->is_logging_code_events()) logger->Call; \
  } while (false)

class Logger : public CodeEventListener {
 public:
  enum StartEnd { START = 0, END = 1 };

  // Acquires resources for logging if the right flags are set.
  bool SetUp(Isolate* isolate);

  // Sets the current code event handler.
  void SetCodeEventHandler(uint32_t options,
                           JitCodeEventHandler event_handler);

  // Sets up ProfilerListener.
  void SetUpProfilerListener();

  // Tear down ProfilerListener if it has no observers.
  void TearDownProfilerListener();

  sampler::Sampler* sampler();

  ProfilerListener* profiler_listener() { return profiler_listener_.get(); }

  // Frees resources acquired in SetUp.
  // When a temporary file is used for the log, returns its stream descriptor,
  // leaving the file open.
  FILE* TearDown();

  // Emits an event with a string value -> (name, value).
  void StringEvent(const char* name, const char* value);

  // Emits an event with an int value -> (name, value).
  void IntEvent(const char* name, int value);
  void IntPtrTEvent(const char* name, intptr_t value);

  // Emits an event with an handle value -> (name, location).
  void HandleEvent(const char* name, Object** location);

  // Emits memory management events for C allocated structures.
  void NewEvent(const char* name, void* object, size_t size);
  void DeleteEvent(const char* name, void* object);

  // Emits an event with a tag, and some resource usage information.
  // -> (name, tag, <rusage information>).
  // Currently, the resource usage information is a process time stamp
  // and a real time timestamp.
  void ResourceEvent(const char* name, const char* tag);

  // Emits an event that an undefined property was read from an
  // object.
  void SuspectReadEvent(Name* name, Object* obj);

  // Emits an event when a message is put on or read from a debugging queue.
  // DebugTag lets us put a call-site specific label on the event.
  void DebugTag(const char* call_site_tag);
  void DebugEvent(const char* event_type, Vector<uint16_t> parameter);


  // ==== Events logged by --log-api. ====
  void ApiSecurityCheck();
  void ApiNamedPropertyAccess(const char* tag, JSObject* holder, Object* name);
  void ApiIndexedPropertyAccess(const char* tag,
                                JSObject* holder,
                                uint32_t index);
  void ApiObjectAccess(const char* tag, JSObject* obj);
  void ApiEntryCall(const char* name);

  // ==== Events logged by --log-code. ====
  void addCodeEventListener(CodeEventListener* listener);
  void removeCodeEventListener(CodeEventListener* listener);

  // Emits a code event for a callback function.
  void CallbackEvent(Name* name, Address entry_point);
  void GetterCallbackEvent(Name* name, Address entry_point);
  void SetterCallbackEvent(Name* name, Address entry_point);
  // Emits a code create event.
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, const char* source);
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, Name* name);
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, SharedFunctionInfo* shared,
                       Name* name);
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, SharedFunctionInfo* shared,
                       Name* source, int line, int column);
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, int args_count);
  // Emits a code deoptimization event.
  void CodeDisableOptEvent(AbstractCode* code, SharedFunctionInfo* shared);
  void CodeMovingGCEvent();
  // Emits a code create event for a RegExp.
  void RegExpCodeCreateEvent(AbstractCode* code, String* source);
  // Emits a code move event.
  void CodeMoveEvent(AbstractCode* from, Address to);
  // Emits a code line info record event.
  void CodeLinePosInfoRecordEvent(AbstractCode* code,
                                  ByteArray* source_position_table);

  void SharedFunctionInfoMoveEvent(Address from, Address to);

  void CodeNameEvent(Address addr, int pos, const char* code_name);

  void CodeDeoptEvent(Code* code, Address pc, int fp_to_sp_delta);

  // ==== Events logged by --log-gc. ====
  // Heap sampling events: start, end, and individual types.
  void HeapSampleBeginEvent(const char* space, const char* kind);
  void HeapSampleEndEvent(const char* space, const char* kind);
  void HeapSampleItemEvent(const char* type, int number, int bytes);
  void HeapSampleJSConstructorEvent(const char* constructor,
                                    int number, int bytes);
  void HeapSampleJSRetainersEvent(const char* constructor,
                                         const char* event);
  void HeapSampleJSProducerEvent(const char* constructor,
                                 Address* stack);
  void HeapSampleStats(const char* space, const char* kind,
                       intptr_t capacity, intptr_t used);

  void SharedLibraryEvent(const std::string& library_path, uintptr_t start,
                          uintptr_t end, intptr_t aslr_slide);

  void CurrentTimeEvent();

  void TimerEvent(StartEnd se, const char* name);

  static void EnterExternal(Isolate* isolate);
  static void LeaveExternal(Isolate* isolate);

  static void DefaultEventLoggerSentinel(const char* name, int event) {}

  INLINE(static void CallEventLogger(Isolate* isolate, const char* name,
                                     StartEnd se, bool expose_to_api));

  // ==== Events logged by --log-regexp ====
  // Regexp compilation and execution events.

  void RegExpCompileEvent(Handle<JSRegExp> regexp, bool in_cache);

  bool is_logging() {
    return is_logging_;
  }

  bool is_logging_code_events() {
    return is_logging() || jit_logger_ != NULL;
  }

  // Stop collection of profiling data.
  // When data collection is paused, CPU Tick events are discarded.
  void StopProfiler();

  void LogExistingFunction(Handle<SharedFunctionInfo> shared,
                           Handle<AbstractCode> code);
  // Logs all compiled functions found in the heap.
  void LogCompiledFunctions();
  // Logs all accessor callbacks found in the heap.
  void LogAccessorCallbacks();
  // Used for logging stubs found in the snapshot.
  void LogCodeObjects();
  // Used for logging bytecode handlers found in the snapshot.
  void LogBytecodeHandlers();

  // Converts tag to a corresponding NATIVE_... if the script is native.
  INLINE(static CodeEventListener::LogEventsAndTags ToNativeByScript(
      CodeEventListener::LogEventsAndTags, Script*));

  // Profiler's sampling interval (in milliseconds).
#if defined(ANDROID)
  // Phones and tablets have processors that are much slower than desktop
  // and laptop computers for which current heuristics are tuned.
  static const int kSamplingIntervalMs = 5;
#else
  static const int kSamplingIntervalMs = 1;
#endif

  // Callback from Log, stops profiling in case of insufficient resources.
  void LogFailure();

 private:
  explicit Logger(Isolate* isolate);
  ~Logger();

  // Emits the profiler's first message.
  void ProfilerBeginEvent();

  // Emits callback event messages.
  void CallbackEventInternal(const char* prefix,
                             Name* name,
                             Address entry_point);

  // Internal configurable move event.
  void MoveEventInternal(CodeEventListener::LogEventsAndTags event,
                         Address from, Address to);

  // Used for logging stubs found in the snapshot.
  void LogCodeObject(Object* code_object);

  // Helper method. It resets name_buffer_ and add tag name into it.
  void InitNameBuffer(CodeEventListener::LogEventsAndTags tag);

  // Emits a profiler tick event. Used by the profiler thread.
  void TickEvent(TickSample* sample, bool overflow);
  void RuntimeCallTimerEvent();

  PRINTF_FORMAT(2, 3) void ApiEvent(const char* format, ...);

  // Logs a StringEvent regardless of whether FLAG_log is true.
  void UncheckedStringEvent(const char* name, const char* value);

  // Logs an IntEvent regardless of whether FLAG_log is true.
  void UncheckedIntEvent(const char* name, int value);
  void UncheckedIntPtrTEvent(const char* name, intptr_t value);

  Isolate* isolate_;

  // The sampler used by the profiler and the sliding state window.
  Ticker* ticker_;

  // When the statistical profile is active, profiler_
  // points to a Profiler, that handles collection
  // of samples.
  Profiler* profiler_;

  // An array of log events names.
  const char* const* log_events_;

  // Internal implementation classes with access to
  // private members.
  friend class EventLog;
  friend class Isolate;
  friend class TimeLog;
  friend class Profiler;
  template <StateTag Tag> friend class VMState;
  friend class LoggerTestHelper;

  bool is_logging_;
  Log* log_;
  PerfBasicLogger* perf_basic_logger_;
  PerfJitLogger* perf_jit_logger_;
  LowLevelLogger* ll_logger_;
  JitLogger* jit_logger_;
  std::unique_ptr<ProfilerListener> profiler_listener_;
  List<CodeEventListener*> listeners_;

  // Guards against multiple calls to TearDown() that can happen in some tests.
  // 'true' between SetUp() and TearDown().
  bool is_initialized_;

  base::ElapsedTimer timer_;

  friend class CpuProfiler;
};

#define TIMER_EVENTS_LIST(V)    \
  V(RecompileSynchronous, true) \
  V(RecompileConcurrent, true)  \
  V(CompileIgnition, true)      \
  V(CompileFullCode, true)      \
  V(OptimizeCode, true)         \
  V(CompileCode, true)          \
  V(DeoptimizeCode, true)       \
  V(Execute, true)              \
  V(External, true)             \
  V(IcMiss, false)

#define V(TimerName, expose)                                                  \
  class TimerEvent##TimerName : public AllStatic {                            \
   public:                                                                    \
    static const char* name(void* unused = NULL) { return "V8." #TimerName; } \
    static bool expose_to_api() { return expose; }                            \
  };
TIMER_EVENTS_LIST(V)
#undef V


template <class TimerEvent>
class TimerEventScope {
 public:
  explicit TimerEventScope(Isolate* isolate) : isolate_(isolate) {
    LogTimerEvent(Logger::START);
  }

  ~TimerEventScope() { LogTimerEvent(Logger::END); }

 private:
  void LogTimerEvent(Logger::StartEnd se);
  Isolate* isolate_;
};

class CodeEventLogger : public CodeEventListener {
 public:
  CodeEventLogger();
  ~CodeEventLogger() override;

  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       const char* comment) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       Name* name) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       int args_count) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* name) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* source, int line,
                       int column) override;
  void RegExpCodeCreateEvent(AbstractCode* code, String* source) override;

  void CallbackEvent(Name* name, Address entry_point) override {}
  void GetterCallbackEvent(Name* name, Address entry_point) override {}
  void SetterCallbackEvent(Name* name, Address entry_point) override {}
  void SharedFunctionInfoMoveEvent(Address from, Address to) override {}
  void CodeMovingGCEvent() override {}
  void CodeDeoptEvent(Code* code, Address pc, int fp_to_sp_delta) override {}

 private:
  class NameBuffer;

  virtual void LogRecordedBuffer(AbstractCode* code, SharedFunctionInfo* shared,
                                 const char* name, int length) = 0;

  NameBuffer* name_buffer_;
};


}  // namespace internal
}  // namespace v8


#endif  // V8_LOG_H_
