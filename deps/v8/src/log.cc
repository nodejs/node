// Copyright 2009 the V8 project authors. All rights reserved.
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

#include <stdarg.h>

#include "v8.h"

#include "bootstrapper.h"
#include "global-handles.h"
#include "log.h"
#include "macro-assembler.h"
#include "serialize.h"
#include "string-stream.h"

namespace v8 {
namespace internal {

#ifdef ENABLE_LOGGING_AND_PROFILING

//
// Sliding state window.  Updates counters to keep track of the last
// window of kBufferSize states.  This is useful to track where we
// spent our time.
//
class SlidingStateWindow {
 public:
  SlidingStateWindow();
  ~SlidingStateWindow();
  void AddState(StateTag state);

 private:
  static const int kBufferSize = 256;
  int current_index_;
  bool is_full_;
  byte buffer_[kBufferSize];


  void IncrementStateCounter(StateTag state) {
    Counters::state_counters[state].Increment();
  }


  void DecrementStateCounter(StateTag state) {
    Counters::state_counters[state].Decrement();
  }
};


//
// The Profiler samples pc and sp values for the main thread.
// Each sample is appended to a circular buffer.
// An independent thread removes data and writes it to the log.
// This design minimizes the time spent in the sampler.
//
class Profiler: public Thread {
 public:
  Profiler();
  void Engage();
  void Disengage();

  // Inserts collected profiling data into buffer.
  void Insert(TickSample* sample) {
    if (paused_)
      return;

    if (Succ(head_) == tail_) {
      overflow_ = true;
    } else {
      buffer_[head_] = *sample;
      head_ = Succ(head_);
      buffer_semaphore_->Signal();  // Tell we have an element.
    }
  }

  // Waits for a signal and removes profiling data.
  bool Remove(TickSample* sample) {
    buffer_semaphore_->Wait();  // Wait for an element.
    *sample = buffer_[tail_];
    bool result = overflow_;
    tail_ = Succ(tail_);
    overflow_ = false;
    return result;
  }

  void Run();

  // Pause and Resume TickSample data collection.
  static bool paused() { return paused_; }
  static void pause() { paused_ = true; }
  static void resume() { paused_ = false; }

 private:
  // Returns the next index in the cyclic buffer.
  int Succ(int index) { return (index + 1) % kBufferSize; }

  // Cyclic buffer for communicating profiling samples
  // between the signal handler and the worker thread.
  static const int kBufferSize = 128;
  TickSample buffer_[kBufferSize];  // Buffer storage.
  int head_;  // Index to the buffer head.
  int tail_;  // Index to the buffer tail.
  bool overflow_;  // Tell whether a buffer overflow has occurred.
  Semaphore* buffer_semaphore_;  // Sempahore used for buffer synchronization.

  // Tells whether profiler is engaged, that is, processing thread is stated.
  bool engaged_;

  // Tells whether worker thread should continue running.
  bool running_;

  // Tells whether we are currently recording tick samples.
  static bool paused_;
};

bool Profiler::paused_ = false;


//
// StackTracer implementation
//
void StackTracer::Trace(TickSample* sample) {
  if (sample->state == GC) {
    sample->frames_count = 0;
    return;
  }

  const Address js_entry_sp = Top::js_entry_sp(Top::GetCurrentThread());
  if (js_entry_sp == 0) {
    // Not executing JS now.
    sample->frames_count = 0;
    return;
  }

  const Address functionAddr =
      sample->fp + JavaScriptFrameConstants::kFunctionOffset;
  if (SafeStackFrameIterator::IsWithinBounds(sample->sp, js_entry_sp,
                                             functionAddr)) {
    sample->function = Memory::Address_at(functionAddr) - kHeapObjectTag;
  }

  int i = 0;
  const Address callback = Logger::current_state_ != NULL ?
      Logger::current_state_->external_callback() : NULL;
  if (callback != NULL) {
    sample->stack[i++] = callback;
  }

  SafeStackTraceFrameIterator it(sample->fp, sample->sp,
                                 sample->sp, js_entry_sp);
  while (!it.done() && i < TickSample::kMaxFramesCount) {
    sample->stack[i++] = it.frame()->pc();
    it.Advance();
  }
  sample->frames_count = i;
}


//
// Ticker used to provide ticks to the profiler and the sliding state
// window.
//
class Ticker: public Sampler {
 public:
  explicit Ticker(int interval):
      Sampler(interval, FLAG_prof), window_(NULL), profiler_(NULL) {}

  ~Ticker() { if (IsActive()) Stop(); }

  void SampleStack(TickSample* sample) {
    StackTracer::Trace(sample);
  }

  void Tick(TickSample* sample) {
    if (profiler_) profiler_->Insert(sample);
    if (window_) window_->AddState(sample->state);
  }

  void SetWindow(SlidingStateWindow* window) {
    window_ = window;
    if (!IsActive()) Start();
  }

  void ClearWindow() {
    window_ = NULL;
    if (!profiler_ && IsActive()) Stop();
  }

  void SetProfiler(Profiler* profiler) {
    profiler_ = profiler;
    if (!FLAG_prof_lazy && !IsActive()) Start();
  }

  void ClearProfiler() {
    profiler_ = NULL;
    if (!window_ && IsActive()) Stop();
  }

 private:
  SlidingStateWindow* window_;
  Profiler* profiler_;
};


//
// SlidingStateWindow implementation.
//
SlidingStateWindow::SlidingStateWindow(): current_index_(0), is_full_(false) {
  for (int i = 0; i < kBufferSize; i++) {
    buffer_[i] = static_cast<byte>(OTHER);
  }
  Logger::ticker_->SetWindow(this);
}


SlidingStateWindow::~SlidingStateWindow() {
  Logger::ticker_->ClearWindow();
}


void SlidingStateWindow::AddState(StateTag state) {
  if (is_full_) {
    DecrementStateCounter(static_cast<StateTag>(buffer_[current_index_]));
  } else if (current_index_ == kBufferSize - 1) {
    is_full_ = true;
  }
  buffer_[current_index_] = static_cast<byte>(state);
  IncrementStateCounter(state);
  ASSERT(IsPowerOf2(kBufferSize));
  current_index_ = (current_index_ + 1) & (kBufferSize - 1);
}


//
// Profiler implementation.
//
Profiler::Profiler()
    : head_(0),
      tail_(0),
      overflow_(false),
      buffer_semaphore_(OS::CreateSemaphore(0)),
      engaged_(false),
      running_(false) {
}


void Profiler::Engage() {
  if (engaged_) return;
  engaged_ = true;

  // TODO(mnaganov): This is actually "Chromium" mode. Flags need to be revised.
  // http://code.google.com/p/v8/issues/detail?id=487
  if (!FLAG_prof_lazy) {
    OS::LogSharedLibraryAddresses();
  }

  // Start thread processing the profiler buffer.
  running_ = true;
  Start();

  // Register to get ticks.
  Logger::ticker_->SetProfiler(this);

  Logger::ProfilerBeginEvent();
  Logger::LogAliases();
}


void Profiler::Disengage() {
  if (!engaged_) return;

  // Stop receiving ticks.
  Logger::ticker_->ClearProfiler();

  // Terminate the worker thread by setting running_ to false,
  // inserting a fake element in the queue and then wait for
  // the thread to terminate.
  running_ = false;
  TickSample sample;
  // Reset 'paused_' flag, otherwise semaphore may not be signalled.
  resume();
  Insert(&sample);
  Join();

  LOG(UncheckedStringEvent("profiler", "end"));
}


void Profiler::Run() {
  TickSample sample;
  bool overflow = Logger::profiler_->Remove(&sample);
  while (running_) {
    LOG(TickEvent(&sample, overflow));
    overflow = Logger::profiler_->Remove(&sample);
  }
}


//
// Logger class implementation.
//
Ticker* Logger::ticker_ = NULL;
Profiler* Logger::profiler_ = NULL;
VMState* Logger::current_state_ = NULL;
VMState Logger::bottom_state_(EXTERNAL);
SlidingStateWindow* Logger::sliding_state_window_ = NULL;
const char** Logger::log_events_ = NULL;
CompressionHelper* Logger::compression_helper_ = NULL;
bool Logger::is_logging_ = false;
int Logger::cpu_profiler_nesting_ = 0;
int Logger::heap_profiler_nesting_ = 0;

#define DECLARE_LONG_EVENT(ignore1, long_name, ignore2) long_name,
const char* kLongLogEventsNames[Logger::NUMBER_OF_LOG_EVENTS] = {
  LOG_EVENTS_AND_TAGS_LIST(DECLARE_LONG_EVENT)
};
#undef DECLARE_LONG_EVENT

#define DECLARE_SHORT_EVENT(ignore1, ignore2, short_name) short_name,
const char* kCompressedLogEventsNames[Logger::NUMBER_OF_LOG_EVENTS] = {
  LOG_EVENTS_AND_TAGS_LIST(DECLARE_SHORT_EVENT)
};
#undef DECLARE_SHORT_EVENT


void Logger::ProfilerBeginEvent() {
  if (!Log::IsEnabled()) return;
  LogMessageBuilder msg;
  msg.Append("profiler,\"begin\",%d\n", kSamplingIntervalMs);
  if (FLAG_compress_log) {
    msg.Append("profiler,\"compression\",%d\n", kCompressionWindowSize);
  }
  msg.WriteToLogFile();
}


void Logger::LogAliases() {
  if (!Log::IsEnabled() || !FLAG_compress_log) return;
  LogMessageBuilder msg;
  for (int i = 0; i < NUMBER_OF_LOG_EVENTS; ++i) {
    msg.Append("alias,%s,%s\n",
               kCompressedLogEventsNames[i], kLongLogEventsNames[i]);
  }
  msg.WriteToLogFile();
}

#endif  // ENABLE_LOGGING_AND_PROFILING


void Logger::StringEvent(const char* name, const char* value) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log) UncheckedStringEvent(name, value);
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::UncheckedStringEvent(const char* name, const char* value) {
  if (!Log::IsEnabled()) return;
  LogMessageBuilder msg;
  msg.Append("%s,\"%s\"\n", name, value);
  msg.WriteToLogFile();
}
#endif


void Logger::IntEvent(const char* name, int value) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("%s,%d\n", name, value);
  msg.WriteToLogFile();
#endif
}


void Logger::HandleEvent(const char* name, Object** location) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_handles) return;
  LogMessageBuilder msg;
  msg.Append("%s,0x%" V8PRIxPTR "\n", name, location);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
// ApiEvent is private so all the calls come from the Logger class.  It is the
// caller's responsibility to ensure that log is enabled and that
// FLAG_log_api is true.
void Logger::ApiEvent(const char* format, ...) {
  ASSERT(Log::IsEnabled() && FLAG_log_api);
  LogMessageBuilder msg;
  va_list ap;
  va_start(ap, format);
  msg.AppendVA(format, ap);
  va_end(ap);
  msg.WriteToLogFile();
}
#endif


void Logger::ApiNamedSecurityCheck(Object* key) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_api) return;
  if (key->IsString()) {
    SmartPointer<char> str =
        String::cast(key)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    ApiEvent("api,check-security,\"%s\"\n", *str);
  } else if (key->IsUndefined()) {
    ApiEvent("api,check-security,undefined\n");
  } else {
    ApiEvent("api,check-security,['no-name']\n");
  }
#endif
}


void Logger::SharedLibraryEvent(const char* library_path,
                                uintptr_t start,
                                uintptr_t end) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg;
  msg.Append("shared-library,\"%s\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR "\n",
             library_path,
             start,
             end);
  msg.WriteToLogFile();
#endif
}


void Logger::SharedLibraryEvent(const wchar_t* library_path,
                                uintptr_t start,
                                uintptr_t end) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg;
  msg.Append("shared-library,\"%ls\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR "\n",
             library_path,
             start,
             end);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::LogRegExpSource(Handle<JSRegExp> regexp) {
  // Prints "/" + re.source + "/" +
  //      (re.global?"g":"") + (re.ignorecase?"i":"") + (re.multiline?"m":"")
  LogMessageBuilder msg;

  Handle<Object> source = GetProperty(regexp, "source");
  if (!source->IsString()) {
    msg.Append("no source");
    return;
  }

  switch (regexp->TypeTag()) {
    case JSRegExp::ATOM:
      msg.Append('a');
      break;
    default:
      break;
  }
  msg.Append('/');
  msg.AppendDetailed(*Handle<String>::cast(source), false);
  msg.Append('/');

  // global flag
  Handle<Object> global = GetProperty(regexp, "global");
  if (global->IsTrue()) {
    msg.Append('g');
  }
  // ignorecase flag
  Handle<Object> ignorecase = GetProperty(regexp, "ignoreCase");
  if (ignorecase->IsTrue()) {
    msg.Append('i');
  }
  // multiline flag
  Handle<Object> multiline = GetProperty(regexp, "multiline");
  if (multiline->IsTrue()) {
    msg.Append('m');
  }

  msg.WriteToLogFile();
}
#endif  // ENABLE_LOGGING_AND_PROFILING


void Logger::RegExpCompileEvent(Handle<JSRegExp> regexp, bool in_cache) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_regexp) return;
  LogMessageBuilder msg;
  msg.Append("regexp-compile,");
  LogRegExpSource(regexp);
  msg.Append(in_cache ? ",hit\n" : ",miss\n");
  msg.WriteToLogFile();
#endif
}


void Logger::LogRuntime(Vector<const char> format, JSArray* args) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_runtime) return;
  HandleScope scope;
  LogMessageBuilder msg;
  for (int i = 0; i < format.length(); i++) {
    char c = format[i];
    if (c == '%' && i <= format.length() - 2) {
      i++;
      ASSERT('0' <= format[i] && format[i] <= '9');
      Object* obj = args->GetElement(format[i] - '0');
      i++;
      switch (format[i]) {
        case 's':
          msg.AppendDetailed(String::cast(obj), false);
          break;
        case 'S':
          msg.AppendDetailed(String::cast(obj), true);
          break;
        case 'r':
          Logger::LogRegExpSource(Handle<JSRegExp>(JSRegExp::cast(obj)));
          break;
        case 'x':
          msg.Append("0x%x", Smi::cast(obj)->value());
          break;
        case 'i':
          msg.Append("%i", Smi::cast(obj)->value());
          break;
        default:
          UNREACHABLE();
      }
    } else {
      msg.Append(c);
    }
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::ApiIndexedSecurityCheck(uint32_t index) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,check-security,%u\n", index);
#endif
}


void Logger::ApiNamedPropertyAccess(const char* tag,
                                    JSObject* holder,
                                    Object* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  ASSERT(name->IsString());
  if (!Log::IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  SmartPointer<char> property_name =
      String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  Logger::ApiEvent("api,%s,\"%s\",\"%s\"\n", tag, *class_name, *property_name);
#endif
}

void Logger::ApiIndexedPropertyAccess(const char* tag,
                                      JSObject* holder,
                                      uint32_t index) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  Logger::ApiEvent("api,%s,\"%s\",%u\n", tag, *class_name, index);
#endif
}

void Logger::ApiObjectAccess(const char* tag, JSObject* object) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = object->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  Logger::ApiEvent("api,%s,\"%s\"\n", tag, *class_name);
#endif
}


void Logger::ApiEntryCall(const char* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_api) return;
  Logger::ApiEvent("api,%s\n", name);
#endif
}


void Logger::NewEvent(const char* name, void* object, size_t size) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("new,%s,0x%" V8PRIxPTR ",%u\n", name, object,
             static_cast<unsigned int>(size));
  msg.WriteToLogFile();
#endif
}


void Logger::DeleteEvent(const char* name, void* object) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("delete,%s,0x%" V8PRIxPTR "\n", name, object);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING

// A class that contains all common code dealing with record compression.
class CompressionHelper {
 public:
  explicit CompressionHelper(int window_size)
      : compressor_(window_size), repeat_count_(0) { }

  // Handles storing message in compressor, retrieving the previous one and
  // prefixing it with repeat count, if needed.
  // Returns true if message needs to be written to log.
  bool HandleMessage(LogMessageBuilder* msg) {
    if (!msg->StoreInCompressor(&compressor_)) {
      // Current message repeats the previous one, don't write it.
      ++repeat_count_;
      return false;
    }
    if (repeat_count_ == 0) {
      return msg->RetrieveCompressedPrevious(&compressor_);
    }
    OS::SNPrintF(prefix_, "%s,%d,",
                 Logger::log_events_[Logger::REPEAT_META_EVENT],
                 repeat_count_ + 1);
    repeat_count_ = 0;
    return msg->RetrieveCompressedPrevious(&compressor_, prefix_.start());
  }

 private:
  LogRecordCompressor compressor_;
  int repeat_count_;
  EmbeddedVector<char, 20> prefix_;
};

#endif  // ENABLE_LOGGING_AND_PROFILING


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::CallbackEventInternal(const char* prefix, const char* name,
                                   Address entry_point) {
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,%s,",
             log_events_[CODE_CREATION_EVENT], log_events_[CALLBACK_TAG]);
  msg.AppendAddress(entry_point);
  msg.Append(",1,\"%s%s\"", prefix, name);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}
#endif


void Logger::CallbackEvent(String* name, Address entry_point) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  CallbackEventInternal("", *str, entry_point);
#endif
}


void Logger::GetterCallbackEvent(String* name, Address entry_point) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  CallbackEventInternal("get ", *str, entry_point);
#endif
}


void Logger::SetterCallbackEvent(String* name, Address entry_point) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  CallbackEventInternal("set ", *str, entry_point);
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             const char* comment) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,%s,", log_events_[CODE_CREATION_EVENT], log_events_[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"", code->ExecutableSize());
  for (const char* p = comment; *p != '\0'; p++) {
    if (*p == '"') {
      msg.Append('\\');
    }
    msg.Append(*p);
  }
  msg.Append('"');
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag, Code* code, String* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("%s,%s,", log_events_[CODE_CREATION_EVENT], log_events_[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"%s\"", code->ExecutableSize(), *str);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code, String* name,
                             String* source, int line) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  SmartPointer<char> sourcestr =
      source->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("%s,%s,", log_events_[CODE_CREATION_EVENT], log_events_[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"%s %s:%d\"",
             code->ExecutableSize(), *str, *sourcestr, line);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(LogEventsAndTags tag, Code* code, int args_count) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,%s,", log_events_[CODE_CREATION_EVENT], log_events_[tag]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"args_count: %d\"", code->ExecutableSize(), args_count);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::RegExpCodeCreateEvent(Code* code, String* source) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,%s,",
             log_events_[CODE_CREATION_EVENT], log_events_[REG_EXP_TAG]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"", code->ExecutableSize());
  msg.AppendDetailed(source, false);
  msg.Append('\"');
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeMoveEvent(Address from, Address to) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  MoveEventInternal(CODE_MOVE_EVENT, from, to);
#endif
}


void Logger::CodeDeleteEvent(Address from) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  DeleteEventInternal(CODE_DELETE_EVENT, from);
#endif
}


void Logger::SnapshotPositionEvent(Address addr, int pos) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_snapshot_positions) return;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[SNAPSHOT_POSITION_EVENT]);
  msg.AppendAddress(addr);
  msg.Append(",%d", pos);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::FunctionCreateEvent(JSFunction* function) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  static Address prev_code = NULL;
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[FUNCTION_CREATION_EVENT]);
  msg.AppendAddress(function->address());
  msg.Append(',');
  msg.AppendAddress(function->code()->address(), prev_code);
  prev_code = function->code()->address();
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::FunctionMoveEvent(Address from, Address to) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  MoveEventInternal(FUNCTION_MOVE_EVENT, from, to);
#endif
}


void Logger::FunctionDeleteEvent(Address from) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  DeleteEventInternal(FUNCTION_DELETE_EVENT, from);
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::MoveEventInternal(LogEventsAndTags event,
                               Address from,
                               Address to) {
  static Address prev_to_ = NULL;
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[event]);
  msg.AppendAddress(from);
  msg.Append(',');
  msg.AppendAddress(to, prev_to_);
  prev_to_ = to;
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}
#endif


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::DeleteEventInternal(LogEventsAndTags event, Address from) {
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[event]);
  msg.AppendAddress(from);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}
#endif


void Logger::ResourceEvent(const char* name, const char* tag) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("%s,%s,", name, tag);

  uint32_t sec, usec;
  if (OS::GetUserTime(&sec, &usec) != -1) {
    msg.Append("%d,%d,", sec, usec);
  }
  msg.Append("%.0f", OS::TimeCurrentMillis());

  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::SuspectReadEvent(String* name, Object* obj) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_suspect) return;
  LogMessageBuilder msg;
  String* class_name = obj->IsJSObject()
                       ? JSObject::cast(obj)->class_name()
                       : Heap::empty_string();
  msg.Append("suspect-read,");
  msg.Append(class_name);
  msg.Append(',');
  msg.Append('"');
  msg.Append(name);
  msg.Append('"');
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleBeginEvent(const char* space, const char* kind) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  // Using non-relative system time in order to be able to synchronize with
  // external memory profiling events (e.g. DOM memory size).
  msg.Append("heap-sample-begin,\"%s\",\"%s\",%.0f\n",
             space, kind, OS::TimeCurrentMillis());
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleStats(const char* space, const char* kind,
                             int capacity, int used) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-sample-stats,\"%s\",\"%s\",%d,%d\n",
             space, kind, capacity, used);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleEndEvent(const char* space, const char* kind) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-sample-end,\"%s\",\"%s\"\n", space, kind);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleItemEvent(const char* type, int number, int bytes) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-sample-item,%s,%d,%d\n", type, number, bytes);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleJSConstructorEvent(const char* constructor,
                                          int number, int bytes) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-js-cons-item,%s,%d,%d\n", constructor, number, bytes);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleJSRetainersEvent(
    const char* constructor, const char* event) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  // Event starts with comma, so we don't have it in the format string.
  static const char* event_text = "heap-js-ret-item,%s";
  // We take placeholder strings into account, but it's OK to be conservative.
  static const int event_text_len = StrLength(event_text);
  const int cons_len = StrLength(constructor);
  const int event_len = StrLength(event);
  int pos = 0;
  // Retainer lists can be long. We may need to split them into multiple events.
  do {
    LogMessageBuilder msg;
    msg.Append(event_text, constructor);
    int to_write = event_len - pos;
    if (to_write > Log::kMessageBufferSize - (cons_len + event_text_len)) {
      int cut_pos = pos + Log::kMessageBufferSize - (cons_len + event_text_len);
      ASSERT(cut_pos < event_len);
      while (cut_pos > pos && event[cut_pos] != ',') --cut_pos;
      if (event[cut_pos] != ',') {
        // Crash in debug mode, skip in release mode.
        ASSERT(false);
        return;
      }
      // Append a piece of event that fits, without trailing comma.
      msg.AppendStringPart(event + pos, cut_pos - pos);
      // Start next piece with comma.
      pos = cut_pos;
    } else {
      msg.Append("%s", event + pos);
      pos += event_len;
    }
    msg.Append('\n');
    msg.WriteToLogFile();
  } while (pos < event_len);
#endif
}


void Logger::HeapSampleJSProducerEvent(const char* constructor,
                                       Address* stack) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-js-prod-item,%s", constructor);
  while (*stack != NULL) {
    msg.Append(",0x%" V8PRIxPTR, *stack++);
  }
  msg.Append("\n");
  msg.WriteToLogFile();
#endif
}


void Logger::DebugTag(const char* call_site_tag) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("debug-tag,%s\n", call_site_tag);
  msg.WriteToLogFile();
#endif
}


void Logger::DebugEvent(const char* event_type, Vector<uint16_t> parameter) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log) return;
  StringBuilder s(parameter.length() + 1);
  for (int i = 0; i < parameter.length(); ++i) {
    s.AddCharacter(static_cast<char>(parameter[i]));
  }
  char* parameter_string = s.Finalize();
  LogMessageBuilder msg;
  msg.Append("debug-queue-event,%s,%15.3f,%s\n",
             event_type,
             OS::TimeCurrentMillis(),
             parameter_string);
  DeleteArray(parameter_string);
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
void Logger::TickEvent(TickSample* sample, bool overflow) {
  if (!Log::IsEnabled() || !FLAG_prof) return;
  static Address prev_sp = NULL;
  static Address prev_function = NULL;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[TICK_EVENT]);
  Address prev_addr = sample->pc;
  msg.AppendAddress(prev_addr);
  msg.Append(',');
  msg.AppendAddress(sample->sp, prev_sp);
  prev_sp = sample->sp;
  msg.Append(',');
  msg.AppendAddress(sample->function, prev_function);
  prev_function = sample->function;
  msg.Append(",%d", static_cast<int>(sample->state));
  if (overflow) {
    msg.Append(",overflow");
  }
  for (int i = 0; i < sample->frames_count; ++i) {
    msg.Append(',');
    msg.AppendAddress(sample->stack[i], prev_addr);
    prev_addr = sample->stack[i];
  }
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


int Logger::GetActiveProfilerModules() {
  int result = PROFILER_MODULE_NONE;
  if (!profiler_->paused()) {
    result |= PROFILER_MODULE_CPU;
  }
  if (FLAG_log_gc) {
    result |= PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS;
  }
  return result;
}


void Logger::PauseProfiler(int flags, int tag) {
  if (!Log::IsEnabled()) return;
  if (flags & PROFILER_MODULE_CPU) {
    // It is OK to have negative nesting.
    if (--cpu_profiler_nesting_ == 0) {
      profiler_->pause();
      if (FLAG_prof_lazy) {
        if (!FLAG_sliding_state_window) ticker_->Stop();
        FLAG_log_code = false;
        // Must be the same message as Log::kDynamicBufferSeal.
        LOG(UncheckedStringEvent("profiler", "pause"));
      }
    }
  }
  if (flags &
      (PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS)) {
    if (--heap_profiler_nesting_ == 0) {
      FLAG_log_gc = false;
    }
  }
  if (tag != 0) {
    IntEvent("close-tag", tag);
  }
  if (GetActiveProfilerModules() == PROFILER_MODULE_NONE) {
    is_logging_ = false;
  }
}


void Logger::ResumeProfiler(int flags, int tag) {
  if (!Log::IsEnabled()) return;
  if (tag != 0) {
    IntEvent("open-tag", tag);
  }
  if (flags & PROFILER_MODULE_CPU) {
    if (cpu_profiler_nesting_++ == 0) {
      is_logging_ = true;
      if (FLAG_prof_lazy) {
        profiler_->Engage();
        LOG(UncheckedStringEvent("profiler", "resume"));
        FLAG_log_code = true;
        LogCompiledFunctions();
        LogFunctionObjects();
        LogAccessorCallbacks();
        if (!FLAG_sliding_state_window) ticker_->Start();
      }
      profiler_->resume();
    }
  }
  if (flags &
      (PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS)) {
    if (heap_profiler_nesting_++ == 0) {
      is_logging_ = true;
      FLAG_log_gc = true;
    }
  }
}


// This function can be called when Log's mutex is acquired,
// either from main or Profiler's thread.
void Logger::StopLoggingAndProfiling() {
  Log::stop();
  PauseProfiler(PROFILER_MODULE_CPU, 0);
}


bool Logger::IsProfilerSamplerActive() {
  return ticker_->IsActive();
}


int Logger::GetLogLines(int from_pos, char* dest_buf, int max_size) {
  return Log::GetLogLines(from_pos, dest_buf, max_size);
}


static int EnumerateCompiledFunctions(Handle<SharedFunctionInfo>* sfis) {
  AssertNoAllocation no_alloc;
  int compiled_funcs_count = 0;
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsSharedFunctionInfo()) continue;
    SharedFunctionInfo* sfi = SharedFunctionInfo::cast(obj);
    if (sfi->is_compiled()
        && (!sfi->script()->IsScript()
            || Script::cast(sfi->script())->HasValidSource())) {
      if (sfis != NULL)
        sfis[compiled_funcs_count] = Handle<SharedFunctionInfo>(sfi);
      ++compiled_funcs_count;
    }
  }
  return compiled_funcs_count;
}


void Logger::LogCodeObject(Object* object) {
  if (FLAG_log_code) {
    Code* code_object = Code::cast(object);
    LogEventsAndTags tag = Logger::STUB_TAG;
    const char* description = "Unknown code from the snapshot";
    switch (code_object->kind()) {
      case Code::FUNCTION:
        return;  // We log this later using LogCompiledFunctions.
      case Code::STUB:
        description = CodeStub::MajorName(code_object->major_key(), true);
        if (description == NULL)
          description = "A stub from the snapshot";
        tag = Logger::STUB_TAG;
        break;
      case Code::BUILTIN:
        description = "A builtin from the snapshot";
        tag = Logger::BUILTIN_TAG;
        break;
      case Code::KEYED_LOAD_IC:
        description = "A keyed load IC from the snapshot";
        tag = Logger::KEYED_LOAD_IC_TAG;
        break;
      case Code::LOAD_IC:
        description = "A load IC from the snapshot";
        tag = Logger::LOAD_IC_TAG;
        break;
      case Code::STORE_IC:
        description = "A store IC from the snapshot";
        tag = Logger::STORE_IC_TAG;
        break;
      case Code::KEYED_STORE_IC:
        description = "A keyed store IC from the snapshot";
        tag = Logger::KEYED_STORE_IC_TAG;
        break;
      case Code::CALL_IC:
        description = "A call IC from the snapshot";
        tag = Logger::CALL_IC_TAG;
        break;
    }
    LOG(CodeCreateEvent(tag, code_object, description));
  }
}


void Logger::LogCodeObjects() {
  AssertNoAllocation no_alloc;
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsCode()) LogCodeObject(obj);
  }
}


void Logger::LogCompiledFunctions() {
  HandleScope scope;
  const int compiled_funcs_count = EnumerateCompiledFunctions(NULL);
  Handle<SharedFunctionInfo>* sfis =
      NewArray< Handle<SharedFunctionInfo> >(compiled_funcs_count);
  EnumerateCompiledFunctions(sfis);

  // During iteration, there can be heap allocation due to
  // GetScriptLineNumber call.
  for (int i = 0; i < compiled_funcs_count; ++i) {
    Handle<SharedFunctionInfo> shared = sfis[i];
    Handle<String> name(String::cast(shared->name()));
    Handle<String> func_name(name->length() > 0 ?
                             *name : shared->inferred_name());
    if (shared->script()->IsScript()) {
      Handle<Script> script(Script::cast(shared->script()));
      if (script->name()->IsString()) {
        Handle<String> script_name(String::cast(script->name()));
        int line_num = GetScriptLineNumber(script, shared->start_position());
        if (line_num > 0) {
          LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG,
                              shared->code(), *func_name,
                              *script_name, line_num + 1));
        } else {
          // Can't distinguish enum and script here, so always use Script.
          LOG(CodeCreateEvent(Logger::SCRIPT_TAG,
                              shared->code(), *script_name));
        }
      } else {
        LOG(CodeCreateEvent(
            Logger::LAZY_COMPILE_TAG, shared->code(), *func_name));
      }
    } else if (shared->function_data()->IsFunctionTemplateInfo()) {
      // API function.
      FunctionTemplateInfo* fun_data =
          FunctionTemplateInfo::cast(shared->function_data());
      Object* raw_call_data = fun_data->call_code();
      if (!raw_call_data->IsUndefined()) {
        CallHandlerInfo* call_data = CallHandlerInfo::cast(raw_call_data);
        Object* callback_obj = call_data->callback();
        Address entry_point = v8::ToCData<Address>(callback_obj);
        LOG(CallbackEvent(*func_name, entry_point));
      }
    } else {
      LOG(CodeCreateEvent(
          Logger::LAZY_COMPILE_TAG, shared->code(), *func_name));
    }
  }

  DeleteArray(sfis);
}


void Logger::LogFunctionObjects() {
  AssertNoAllocation no_alloc;
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsJSFunction()) continue;
    JSFunction* jsf = JSFunction::cast(obj);
    if (!jsf->is_compiled()) continue;
    LOG(FunctionCreateEvent(jsf));
  }
}


void Logger::LogAccessorCallbacks() {
  AssertNoAllocation no_alloc;
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsAccessorInfo()) continue;
    AccessorInfo* ai = AccessorInfo::cast(obj);
    if (!ai->name()->IsString()) continue;
    String* name = String::cast(ai->name());
    Address getter_entry = v8::ToCData<Address>(ai->getter());
    if (getter_entry != 0) {
      LOG(GetterCallbackEvent(name, getter_entry));
    }
    Address setter_entry = v8::ToCData<Address>(ai->setter());
    if (setter_entry != 0) {
      LOG(SetterCallbackEvent(name, setter_entry));
    }
  }
}

#endif


bool Logger::Setup() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // --log-all enables all the log flags.
  if (FLAG_log_all) {
    FLAG_log_runtime = true;
    FLAG_log_api = true;
    FLAG_log_code = true;
    FLAG_log_gc = true;
    FLAG_log_suspect = true;
    FLAG_log_handles = true;
    FLAG_log_regexp = true;
  }

  // --prof implies --log-code.
  if (FLAG_prof) FLAG_log_code = true;

  // --prof_lazy controls --log-code, implies --noprof_auto.
  if (FLAG_prof_lazy) {
    FLAG_log_code = false;
    FLAG_prof_auto = false;
  }

  bool start_logging = FLAG_log || FLAG_log_runtime || FLAG_log_api
      || FLAG_log_code || FLAG_log_gc || FLAG_log_handles || FLAG_log_suspect
      || FLAG_log_regexp || FLAG_log_state_changes;

  bool open_log_file = start_logging || FLAG_prof_lazy;

  // If we're logging anything, we need to open the log file.
  if (open_log_file) {
    if (strcmp(FLAG_logfile, "-") == 0) {
      Log::OpenStdout();
    } else if (strcmp(FLAG_logfile, "*") == 0) {
      Log::OpenMemoryBuffer();
    } else if (strchr(FLAG_logfile, '%') != NULL) {
      // If there's a '%' in the log file name we have to expand
      // placeholders.
      HeapStringAllocator allocator;
      StringStream stream(&allocator);
      for (const char* p = FLAG_logfile; *p; p++) {
        if (*p == '%') {
          p++;
          switch (*p) {
            case '\0':
              // If there's a % at the end of the string we back up
              // one character so we can escape the loop properly.
              p--;
              break;
            case 't': {
              // %t expands to the current time in milliseconds.
              double time = OS::TimeCurrentMillis();
              stream.Add("%.0f", FmtElm(time));
              break;
            }
            case '%':
              // %% expands (contracts really) to %.
              stream.Put('%');
              break;
            default:
              // All other %'s expand to themselves.
              stream.Put('%');
              stream.Put(*p);
              break;
          }
        } else {
          stream.Put(*p);
        }
      }
      SmartPointer<const char> expanded = stream.ToCString();
      Log::OpenFile(*expanded);
    } else {
      Log::OpenFile(FLAG_logfile);
    }
  }

  current_state_ = &bottom_state_;

  ticker_ = new Ticker(kSamplingIntervalMs);

  if (FLAG_sliding_state_window && sliding_state_window_ == NULL) {
    sliding_state_window_ = new SlidingStateWindow();
  }

  log_events_ = FLAG_compress_log ?
      kCompressedLogEventsNames : kLongLogEventsNames;
  if (FLAG_compress_log) {
    compression_helper_ = new CompressionHelper(kCompressionWindowSize);
  }

  is_logging_ = start_logging;

  if (FLAG_prof) {
    profiler_ = new Profiler();
    if (!FLAG_prof_auto) {
      profiler_->pause();
    } else {
      is_logging_ = true;
    }
    if (!FLAG_prof_lazy) {
      profiler_->Engage();
    }
  }

  LogMessageBuilder::set_write_failure_handler(StopLoggingAndProfiling);

  return true;

#else
  return false;
#endif
}


void Logger::TearDown() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  LogMessageBuilder::set_write_failure_handler(NULL);

  // Stop the profiler before closing the file.
  if (profiler_ != NULL) {
    profiler_->Disengage();
    delete profiler_;
    profiler_ = NULL;
  }

  delete compression_helper_;
  compression_helper_ = NULL;

  delete sliding_state_window_;
  sliding_state_window_ = NULL;

  delete ticker_;
  ticker_ = NULL;

  Log::Close();
#endif
}


void Logger::EnableSlidingStateWindow() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // If the ticker is NULL, Logger::Setup has not been called yet.  In
  // that case, we set the sliding_state_window flag so that the
  // sliding window computation will be started when Logger::Setup is
  // called.
  if (ticker_ == NULL) {
    FLAG_sliding_state_window = true;
    return;
  }
  // Otherwise, if the sliding state window computation has not been
  // started we do it now.
  if (sliding_state_window_ == NULL) {
    sliding_state_window_ = new SlidingStateWindow();
  }
#endif
}


} }  // namespace v8::internal
