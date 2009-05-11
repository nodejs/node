// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_LOG_H_
#define V8_LOG_H_

namespace v8 { namespace internal {

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
class Ticker;
class Profiler;
class Semaphore;
class SlidingStateWindow;
class LogMessageBuilder;

#undef LOG
#ifdef ENABLE_LOGGING_AND_PROFILING
#define LOG(Call)                           \
  do {                                      \
    if (v8::internal::Logger::is_enabled()) \
      v8::internal::Logger::Call;           \
  } while (false)
#else
#define LOG(Call) ((void) 0)
#endif


class VMState BASE_EMBEDDED {
#ifdef ENABLE_LOGGING_AND_PROFILING
 public:
  explicit VMState(StateTag state);
  ~VMState();

  StateTag state() { return state_; }

 private:
  StateTag state_;
  VMState* previous_;
#else
 public:
  explicit VMState(StateTag state) {}
#endif
};


class Logger {
 public:
  // Acquires resources for logging if the right flags are set.
  static bool Setup();

  // Frees resources acquired in Setup.
  static void TearDown();

  // Enable the computation of a sliding window of states.
  static void EnableSlidingStateWindow();

  // Write a raw string to the log to be used as a preamble.
  // No check is made that the 'preamble' is actually at the beginning
  // of the log. The preample is used to write code events saved in the
  // snapshot.
  static void Preamble(const char* content);

  // Emits an event with a string value -> (name, value).
  static void StringEvent(const char* name, const char* value);

  // Emits an event with an int value -> (name, value).
  static void IntEvent(const char* name, int value);

  // Emits an event with an handle value -> (name, location).
  static void HandleEvent(const char* name, Object** location);

  // Emits memory management events for C allocated structures.
  static void NewEvent(const char* name, void* object, size_t size);
  static void DeleteEvent(const char* name, void* object);

  // Emits an event with a tag, and some resource usage information.
  // -> (name, tag, <rusage information>).
  // Currently, the resource usage information is a process time stamp
  // and a real time timestamp.
  static void ResourceEvent(const char* name, const char* tag);

  // Emits an event that an undefined property was read from an
  // object.
  static void SuspectReadEvent(String* name, Object* obj);

  // Emits an event when a message is put on or read from a debugging queue.
  // DebugTag lets us put a call-site specific label on the event.
  static void DebugTag(const char* call_site_tag);
  static void DebugEvent(const char* event_type, Vector<uint16_t> parameter);


  // ==== Events logged by --log-api. ====
  static void ApiNamedSecurityCheck(Object* key);
  static void ApiIndexedSecurityCheck(uint32_t index);
  static void ApiNamedPropertyAccess(const char* tag,
                                     JSObject* holder,
                                     Object* name);
  static void ApiIndexedPropertyAccess(const char* tag,
                                       JSObject* holder,
                                       uint32_t index);
  static void ApiObjectAccess(const char* tag, JSObject* obj);
  static void ApiEntryCall(const char* name);


  // ==== Events logged by --log-code. ====
  // Emits a code create event.
  static void CodeCreateEvent(const char* tag, Code* code, const char* source);
  static void CodeCreateEvent(const char* tag, Code* code, String* name);
  static void CodeCreateEvent(const char* tag, Code* code, String* name,
                              String* source, int line);
  static void CodeCreateEvent(const char* tag, Code* code, int args_count);
  // Emits a code create event for a RegExp.
  static void RegExpCodeCreateEvent(Code* code, String* source);
  static void CodeAllocateEvent(Code* code, Assembler* assem);
  // Emits a code move event.
  static void CodeMoveEvent(Address from, Address to);
  // Emits a code delete event.
  static void CodeDeleteEvent(Address from);

  // ==== Events logged by --log-gc. ====
  // Heap sampling events: start, end, and individual types.
  static void HeapSampleBeginEvent(const char* space, const char* kind);
  static void HeapSampleEndEvent(const char* space, const char* kind);
  static void HeapSampleItemEvent(const char* type, int number, int bytes);

  static void SharedLibraryEvent(const char* library_path,
                                 unsigned start,
                                 unsigned end);
  static void SharedLibraryEvent(const wchar_t* library_path,
                                 unsigned start,
                                 unsigned end);

  // ==== Events logged by --log-regexp ====
  // Regexp compilation and execution events.

  static void RegExpCompileEvent(Handle<JSRegExp> regexp, bool in_cache);

  // Log an event reported from generated code
  static void LogRuntime(Vector<const char> format, JSArray* args);

#ifdef ENABLE_LOGGING_AND_PROFILING
  static StateTag state() {
    return current_state_ ? current_state_->state() : OTHER;
  }

  static bool is_enabled();

  // Pause/Resume collection of profiling data.
  // When data collection is paused, Tick events are discarded until
  // data collection is Resumed.
  static bool IsProfilerPaused();
  static void PauseProfiler();
  static void ResumeProfiler();

  // If logging is performed into a memory buffer, allows to
  // retrieve previously written messages. See v8.h.
  static int GetLogLines(int from_pos, char* dest_buf, int max_size);

 private:

  // Emits the source code of a regexp. Used by regexp events.
  static void LogRegExpSource(Handle<JSRegExp> regexp);

  // Emits a profiler tick event. Used by the profiler thread.
  static void TickEvent(TickSample* sample, bool overflow);

  static void ApiEvent(const char* name, ...);

  // Logs a StringEvent regardless of whether FLAG_log is true.
  static void UncheckedStringEvent(const char* name, const char* value);

  // The sampler used by the profiler and the sliding state window.
  static Ticker* ticker_;

  // When the statistical profile is active, profiler_
  // points to a Profiler, that handles collection
  // of samples.
  static Profiler* profiler_;

  // A stack of VM states.
  static VMState* current_state_;

  // Singleton bottom or default vm state.
  static VMState bottom_state_;

  // SlidingStateWindow instance keeping a sliding window of the most
  // recent VM states.
  static SlidingStateWindow* sliding_state_window_;

  // Internal implementation classes with access to
  // private members.
  friend class EventLog;
  friend class TimeLog;
  friend class Profiler;
  friend class SlidingStateWindow;
  friend class VMState;
#else
  static bool is_enabled() { return false; }
#endif
};


// Class that extracts stack trace, used for profiling.
class StackTracer BASE_EMBEDDED {
 public:
  explicit StackTracer(uintptr_t low_stack_bound)
      : low_stack_bound_(low_stack_bound) { }
  void Trace(TickSample* sample);
 private:

  uintptr_t low_stack_bound_;
};


} }  // namespace v8::internal

#endif  // V8_LOG_H_
