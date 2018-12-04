// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOG_H_
#define V8_LOG_H_

#include <set>
#include <string>

#include "include/v8-profiler.h"
#include "src/allocation.h"
#include "src/base/compiler-specific.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"
#include "src/code-events.h"
#include "src/isolate.h"
#include "src/log-utils.h"
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
// --log-api, --log-code, and --log-regexp.
//
// --log-api
// Log API events to the logfile, default is off.  --log-api implies --log.
//
// --log-code
// Log code (create, move, and delete) events to the logfile, default is off.
// --log-code implies --log.
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
//
// --prof-sampling-interval <microseconds>
// The interval between --prof samples, default is 1000 microseconds (5000 on
// Android).

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
class RuntimeCallTimer;
class Ticker;

#undef LOG
#define LOG(isolate, Call)                              \
  do {                                                  \
    v8::internal::Logger* logger = (isolate)->logger(); \
    if (logger->is_logging()) logger->Call;             \
  } while (false)

#define LOG_CODE_EVENT(isolate, Call)                        \
  do {                                                       \
    v8::internal::Logger* logger = (isolate)->logger();      \
    if (logger->is_listening_to_code_events()) logger->Call; \
  } while (false)

class ExistingCodeLogger {
 public:
  explicit ExistingCodeLogger(Isolate* isolate,
                              CodeEventListener* listener = nullptr)
      : isolate_(isolate), listener_(listener) {}

  void LogCodeObjects();

  void LogCompiledFunctions();
  void LogExistingFunction(Handle<SharedFunctionInfo> shared,
                           Handle<AbstractCode> code,
                           CodeEventListener::LogEventsAndTags tag =
                               CodeEventListener::LAZY_COMPILE_TAG);
  void LogCodeObject(Object* object);

 private:
  Isolate* isolate_;
  CodeEventListener* listener_;
};

class Logger : public CodeEventListener {
 public:
  enum StartEnd { START = 0, END = 1, STAMP = 2 };

  enum class ScriptEventType {
    kReserveId,
    kCreate,
    kDeserialize,
    kBackgroundCompile,
    kStreamingCompile
  };

  // The separator is used to write an unescaped "," into the log.
  static const LogSeparator kNext = LogSeparator::kSeparator;

  // Acquires resources for logging if the right flags are set.
  bool SetUp(Isolate* isolate);

  // Sets the current code event handler.
  void SetCodeEventHandler(uint32_t options,
                           JitCodeEventHandler event_handler);

  sampler::Sampler* sampler();

  void StopProfilerThread();

  // Frees resources acquired in SetUp.
  // When a temporary file is used for the log, returns its stream descriptor,
  // leaving the file open.
  FILE* TearDown();

  // Emits an event with a string value -> (name, value).
  void StringEvent(const char* name, const char* value);

  // Emits an event with an int value -> (name, value).
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

  // ==== Events logged by --log-function-events ====
  void FunctionEvent(const char* reason, int script_id, double time_delta_ms,
                     int start_position = -1, int end_position = -1,
                     String* function_name = nullptr);
  void FunctionEvent(const char* reason, int script_id, double time_delta_ms,
                     int start_position, int end_position,
                     const char* function_name = nullptr,
                     size_t function_name_length = 0);

  void CompilationCacheEvent(const char* action, const char* cache_type,
                             SharedFunctionInfo* sfi);
  void ScriptEvent(ScriptEventType type, int script_id);
  void ScriptDetails(Script* script);

  // ==== Events logged by --log-api. ====
  void ApiSecurityCheck();
  void ApiNamedPropertyAccess(const char* tag, JSObject* holder, Object* name);
  void ApiIndexedPropertyAccess(const char* tag,
                                JSObject* holder,
                                uint32_t index);
  void ApiObjectAccess(const char* tag, JSObject* obj);
  void ApiEntryCall(const char* name);

  // ==== Events logged by --log-code. ====
  void AddCodeEventListener(CodeEventListener* listener);
  void RemoveCodeEventListener(CodeEventListener* listener);

  // Emits a code event for a callback function.
  void CallbackEvent(Name* name, Address entry_point) override;
  void GetterCallbackEvent(Name* name, Address entry_point) override;
  void SetterCallbackEvent(Name* name, Address entry_point) override;
  // Emits a code create event.
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, const char* source) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, Name* name) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, SharedFunctionInfo* shared,
                       Name* name) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       AbstractCode* code, SharedFunctionInfo* shared,
                       Name* source, int line, int column) override;
  void CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                       const wasm::WasmCode* code,
                       wasm::WasmName name) override;
  // Emits a code deoptimization event.
  void CodeDisableOptEvent(AbstractCode* code,
                           SharedFunctionInfo* shared) override;
  void CodeMovingGCEvent() override;
  // Emits a code create event for a RegExp.
  void RegExpCodeCreateEvent(AbstractCode* code, String* source) override;
  // Emits a code move event.
  void CodeMoveEvent(AbstractCode* from, AbstractCode* to) override;
  // Emits a code line info record event.
  void CodeLinePosInfoRecordEvent(Address code_start,
                                  ByteArray* source_position_table);
  void CodeLinePosInfoRecordEvent(Address code_start,
                                  Vector<const byte> source_position_table);

  void SharedFunctionInfoMoveEvent(Address from, Address to) override;

  void CodeNameEvent(Address addr, int pos, const char* code_name);

  void CodeDeoptEvent(Code* code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta) override;

  void ICEvent(const char* type, bool keyed, Map* map, Object* key,
               char old_state, char new_state, const char* modifier,
               const char* slow_stub_reason);

  void MapEvent(const char* type, Map* from, Map* to,
                const char* reason = nullptr,
                HeapObject* name_or_sfi = nullptr);
  void MapCreate(Map* map);
  void MapDetails(Map* map);


  void SharedLibraryEvent(const std::string& library_path, uintptr_t start,
                          uintptr_t end, intptr_t aslr_slide);

  void CurrentTimeEvent();

  V8_EXPORT_PRIVATE void TimerEvent(StartEnd se, const char* name);

  static void EnterExternal(Isolate* isolate);
  static void LeaveExternal(Isolate* isolate);

  static void DefaultEventLoggerSentinel(const char* name, int event) {}

  V8_INLINE static void CallEventLogger(Isolate* isolate, const char* name,
                                        StartEnd se, bool expose_to_api);

  bool is_logging() {
    return is_logging_;
  }

  bool is_listening_to_code_events() override {
    return is_logging() || jit_logger_ != nullptr;
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
  // Logs all Mpas foind in the heap.
  void LogMaps();

  // Converts tag to a corresponding NATIVE_... if the script is native.
  V8_INLINE static CodeEventListener::LogEventsAndTags ToNativeByScript(
      CodeEventListener::LogEventsAndTags, Script*);

  // Callback from Log, stops profiling in case of insufficient resources.
  void LogFailure();

  // Used for logging stubs found in the snapshot.
  void LogCodeObject(Object* code_object);

 private:
  explicit Logger(Isolate* isolate);
  ~Logger() override;

  // Emits the profiler's first message.
  void ProfilerBeginEvent();

  // Emits callback event messages.
  void CallbackEventInternal(const char* prefix,
                             Name* name,
                             Address entry_point);

  // Internal configurable move event.
  void MoveEventInternal(CodeEventListener::LogEventsAndTags event,
                         Address from, Address to);

  // Helper method. It resets name_buffer_ and add tag name into it.
  void InitNameBuffer(CodeEventListener::LogEventsAndTags tag);

  // Emits a profiler tick event. Used by the profiler thread.
  void TickEvent(TickSample* sample, bool overflow);
  void RuntimeCallTimerEvent();

  // Logs a StringEvent regardless of whether FLAG_log is true.
  void UncheckedStringEvent(const char* name, const char* value);

  // Logs an IntPtrTEvent regardless of whether FLAG_log is true.
  void UncheckedIntPtrTEvent(const char* name, intptr_t value);

  // Logs a scripts sources. Keeps track of all logged scripts to ensure that
  // each script is logged only once.
  bool EnsureLogScriptSource(Script* script);

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
  std::set<int> logged_source_code_;
  uint32_t next_source_info_id_ = 0;

  // Guards against multiple calls to TearDown() that can happen in some tests.
  // 'true' between SetUp() and TearDown().
  bool is_initialized_;

  ExistingCodeLogger existing_code_logger_;

  base::ElapsedTimer timer_;

  friend class CpuProfiler;
};

#define TIMER_EVENTS_LIST(V)     \
  V(RecompileSynchronous, true)  \
  V(RecompileConcurrent, true)   \
  V(CompileIgnition, true)       \
  V(CompileFullCode, true)       \
  V(OptimizeCode, true)          \
  V(CompileCode, true)           \
  V(CompileCodeBackground, true) \
  V(DeoptimizeCode, true)        \
  V(Execute, true)               \
  V(External, true)

#define V(TimerName, expose)                          \
  class TimerEvent##TimerName : public AllStatic {    \
   public:                                            \
    static const char* name(void* unused = nullptr) { \
      return "V8." #TimerName;                        \
    }                                                 \
    static bool expose_to_api() { return expose; }    \
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
  explicit CodeEventLogger(Isolate* isolate);
  ~CodeEventLogger() override;

  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       const char* comment) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       Name* name) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* name) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* source, int line,
                       int column) override;
  void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                       wasm::WasmName name) override;

  void RegExpCodeCreateEvent(AbstractCode* code, String* source) override;
  void CallbackEvent(Name* name, Address entry_point) override {}
  void GetterCallbackEvent(Name* name, Address entry_point) override {}
  void SetterCallbackEvent(Name* name, Address entry_point) override {}
  void SharedFunctionInfoMoveEvent(Address from, Address to) override {}
  void CodeMovingGCEvent() override {}
  void CodeDeoptEvent(Code* code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta) override {}

 protected:
  Isolate* isolate_;

 private:
  class NameBuffer;

  virtual void LogRecordedBuffer(AbstractCode* code, SharedFunctionInfo* shared,
                                 const char* name, int length) = 0;
  virtual void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                                 int length) = 0;

  NameBuffer* name_buffer_;
};

struct CodeEvent {
  Isolate* isolate_;
  uintptr_t code_start_address;
  size_t code_size;
  Handle<String> function_name;
  Handle<String> script_name;
  int script_line;
  int script_column;
  CodeEventType code_type;
  const char* comment;
};

class ExternalCodeEventListener : public CodeEventListener {
 public:
  explicit ExternalCodeEventListener(Isolate* isolate);
  ~ExternalCodeEventListener() override;

  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       const char* comment) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       Name* name) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* name) override;
  void CodeCreateEvent(LogEventsAndTags tag, AbstractCode* code,
                       SharedFunctionInfo* shared, Name* source, int line,
                       int column) override;
  void CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                       wasm::WasmName name) override;

  void RegExpCodeCreateEvent(AbstractCode* code, String* source) override;
  void CallbackEvent(Name* name, Address entry_point) override {}
  void GetterCallbackEvent(Name* name, Address entry_point) override {}
  void SetterCallbackEvent(Name* name, Address entry_point) override {}
  void SharedFunctionInfoMoveEvent(Address from, Address to) override {}
  void CodeMoveEvent(AbstractCode* from, AbstractCode* to) override {}
  void CodeDisableOptEvent(AbstractCode* code,
                           SharedFunctionInfo* shared) override {}
  void CodeMovingGCEvent() override {}
  void CodeDeoptEvent(Code* code, DeoptimizeKind kind, Address pc,
                      int fp_to_sp_delta) override {}

  void StartListening(CodeEventHandler* code_event_handler);
  void StopListening();

  bool is_listening_to_code_events() override { return true; }

 private:
  void LogExistingCode();

  bool is_listening_;
  Isolate* isolate_;
  v8::CodeEventHandler* code_event_handler_;
};

}  // namespace internal
}  // namespace v8


#endif  // V8_LOG_H_
