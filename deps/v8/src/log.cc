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

  SafeStackTraceFrameIterator it(
      reinterpret_cast<Address>(sample->fp),
      reinterpret_cast<Address>(sample->sp),
      reinterpret_cast<Address>(sample->sp),
      js_entry_sp);
  int i = 0;
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
Profiler::Profiler() {
  buffer_semaphore_ = OS::CreateSemaphore(0);
  head_ = 0;
  tail_ = 0;
  overflow_ = false;
  running_ = false;
}


void Profiler::Engage() {
  OS::LogSharedLibraryAddresses();

  // Start thread processing the profiler buffer.
  running_ = true;
  Start();

  // Register to get ticks.
  Logger::ticker_->SetProfiler(this);

  Logger::ProfilerBeginEvent();
  Logger::LogAliases();
}


void Profiler::Disengage() {
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


void Logger::Preamble(const char* content) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.WriteCStringToLogFile(content);
#endif
}


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
  static Address prev_to_ = NULL;
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[CODE_MOVE_EVENT]);
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
#endif
}


void Logger::CodeDeleteEvent(Address from) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (!Log::IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[CODE_DELETE_EVENT]);
  msg.AppendAddress(from);
  if (FLAG_compress_log) {
    ASSERT(compression_helper_ != NULL);
    if (!compression_helper_->HandleMessage(&msg)) return;
  }
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


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
  msg.Append("heap-js-cons-item,%s,%d,%d\n",
             constructor[0] != '\0' ? constructor : "(anonymous)",
             number, bytes);
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
  LogMessageBuilder msg;
  msg.Append("%s,", log_events_[TICK_EVENT]);
  Address prev_addr = reinterpret_cast<Address>(sample->pc);
  msg.AppendAddress(prev_addr);
  msg.Append(',');
  msg.AppendAddress(reinterpret_cast<Address>(sample->sp), prev_sp);
  prev_sp = reinterpret_cast<Address>(sample->sp);
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


void Logger::PauseProfiler(int flags) {
  if (!Log::IsEnabled()) return;
  const int active_modules = GetActiveProfilerModules();
  const int modules_to_disable = active_modules & flags;
  if (modules_to_disable == PROFILER_MODULE_NONE) return;

  if (modules_to_disable & PROFILER_MODULE_CPU) {
    profiler_->pause();
    if (FLAG_prof_lazy) {
      if (!FLAG_sliding_state_window) ticker_->Stop();
      FLAG_log_code = false;
      // Must be the same message as Log::kDynamicBufferSeal.
      LOG(UncheckedStringEvent("profiler", "pause"));
    }
  }
  if (modules_to_disable &
      (PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS)) {
    FLAG_log_gc = false;
  }
  // Turn off logging if no active modules remain.
  if ((active_modules & ~flags) == PROFILER_MODULE_NONE) {
    is_logging_ = false;
  }
}


void Logger::ResumeProfiler(int flags) {
  if (!Log::IsEnabled()) return;
  const int modules_to_enable = ~GetActiveProfilerModules() & flags;
  if (modules_to_enable != PROFILER_MODULE_NONE) {
    is_logging_ = true;
  }
  if (modules_to_enable & PROFILER_MODULE_CPU) {
    if (FLAG_prof_lazy) {
      LOG(UncheckedStringEvent("profiler", "resume"));
      FLAG_log_code = true;
      LogCompiledFunctions();
      if (!FLAG_sliding_state_window) ticker_->Start();
    }
    profiler_->resume();
  }
  if (modules_to_enable &
      (PROFILER_MODULE_HEAP_STATS | PROFILER_MODULE_JS_CONSTRUCTORS)) {
    FLAG_log_gc = true;
  }
}


// This function can be called when Log's mutex is acquired,
// either from main or Profiler's thread.
void Logger::StopLoggingAndProfiling() {
  Log::stop();
  PauseProfiler(PROFILER_MODULE_CPU);
}


bool Logger::IsProfilerSamplerActive() {
  return ticker_->IsActive();
}


int Logger::GetLogLines(int from_pos, char* dest_buf, int max_size) {
  return Log::GetLogLines(from_pos, dest_buf, max_size);
}


void Logger::LogCompiledFunctions() {
  HandleScope scope;
  Handle<SharedFunctionInfo>* sfis = NULL;
  int compiled_funcs_count = 0;

  {
    AssertNoAllocation no_alloc;

    HeapIterator iterator;
    while (iterator.has_next()) {
      HeapObject* obj = iterator.next();
      ASSERT(obj != NULL);
      if (obj->IsSharedFunctionInfo()
          && SharedFunctionInfo::cast(obj)->is_compiled()) {
        ++compiled_funcs_count;
      }
    }

    sfis = NewArray< Handle<SharedFunctionInfo> >(compiled_funcs_count);
    iterator.reset();

    int i = 0;
    while (iterator.has_next()) {
      HeapObject* obj = iterator.next();
      ASSERT(obj != NULL);
      if (obj->IsSharedFunctionInfo()
          && SharedFunctionInfo::cast(obj)->is_compiled()) {
        sfis[i++] = Handle<SharedFunctionInfo>(SharedFunctionInfo::cast(obj));
      }
    }
  }

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
        continue;
      }
    }
    // If no script or script has no name.
    LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, shared->code(), *func_name));
  }

  DeleteArray(sfis);
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
    profiler_->Engage();
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
