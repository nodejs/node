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
#include "platform.h"
#include "serialize.h"
#include "string-stream.h"

namespace v8 { namespace internal {

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

  SafeStackTraceFrameIterator it(
      reinterpret_cast<Address>(sample->fp),
      reinterpret_cast<Address>(sample->sp),
      reinterpret_cast<Address>(sample->sp),
      reinterpret_cast<Address>(low_stack_bound_));
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
  explicit Ticker(int interval, unsigned int low_stack_bound):
      Sampler(interval, FLAG_prof), window_(NULL), profiler_(NULL),
      stack_tracer_(low_stack_bound) {}

  ~Ticker() { if (IsActive()) Stop(); }

  void Tick(TickSample* sample) {
    if (IsProfiling()) stack_tracer_.Trace(sample);
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
    if (!IsActive()) Start();
  }

  void ClearProfiler() {
    profiler_ = NULL;
    if (!window_ && IsActive()) Stop();
  }

 private:
  SlidingStateWindow* window_;
  Profiler* profiler_;
  StackTracer stack_tracer_;
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

  LOG(UncheckedStringEvent("profiler", "begin"));
}


void Profiler::Disengage() {
  // Stop receiving ticks.
  Logger::ticker_->ClearProfiler();

  // Terminate the worker thread by setting running_ to false,
  // inserting a fake element in the queue and then wait for
  // the thread to terminate.
  running_ = false;
  TickSample sample;
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


#ifdef ENABLE_LOGGING_AND_PROFILING
// Utility class for formatting log messages. It fills the message into the
// static buffer in Logger.
class LogMessageBuilder BASE_EMBEDDED {
 public:
  explicit LogMessageBuilder();
  ~LogMessageBuilder() { }

  void Append(const char* format, ...);
  void Append(const char* format, va_list args);
  void Append(const char c);
  void Append(String *str);
  void AppendDetailed(String* str, bool show_impl_info);

  void WriteToLogFile();
  void WriteCStringToLogFile(const char* str);

 private:
  ScopedLock sl;
  int pos_;
};


// Create a message builder starting from position 0. This acquires the mutex
// in the logger as well.
LogMessageBuilder::LogMessageBuilder(): sl(Logger::mutex_), pos_(0) {
  ASSERT(Logger::message_buffer_ != NULL);
}


// Append string data to the log message.
void LogMessageBuilder::Append(const char* format, ...) {
  Vector<char> buf(Logger::message_buffer_ + pos_,
                   Logger::kMessageBufferSize - pos_);
  va_list args;
  va_start(args, format);
  Append(format, args);
  va_end(args);
  ASSERT(pos_ <= Logger::kMessageBufferSize);
}


// Append string data to the log message.
void LogMessageBuilder::Append(const char* format, va_list args) {
  Vector<char> buf(Logger::message_buffer_ + pos_,
                   Logger::kMessageBufferSize - pos_);
  int result = v8::internal::OS::VSNPrintF(buf, format, args);

  // Result is -1 if output was truncated.
  if (result >= 0) {
    pos_ += result;
  } else {
    pos_ = Logger::kMessageBufferSize;
  }
  ASSERT(pos_ <= Logger::kMessageBufferSize);
}


// Append a character to the log message.
void LogMessageBuilder::Append(const char c) {
  if (pos_ < Logger::kMessageBufferSize) {
    Logger::message_buffer_[pos_++] = c;
  }
  ASSERT(pos_ <= Logger::kMessageBufferSize);
}


// Append a heap string.
void LogMessageBuilder::Append(String* str) {
  AssertNoAllocation no_heap_allocation;  // Ensure string stay valid.
  int length = str->length();
  for (int i = 0; i < length; i++) {
    Append(static_cast<char>(str->Get(i)));
  }
}

void LogMessageBuilder::AppendDetailed(String* str, bool show_impl_info) {
  AssertNoAllocation no_heap_allocation;  // Ensure string stay valid.
  int len = str->length();
  if (len > 0x1000)
    len = 0x1000;
  if (show_impl_info) {
    Append(StringShape(str).IsAsciiRepresentation() ? 'a' : '2');
    if (StringShape(str).IsExternal())
      Append('e');
    if (StringShape(str).IsSymbol())
      Append('#');
    Append(":%i:", str->length());
  }
  for (int i = 0; i < len; i++) {
    uc32 c = str->Get(i);
    if (c > 0xff) {
      Append("\\u%04x", c);
    } else if (c < 32 || c > 126) {
      Append("\\x%02x", c);
    } else if (c == ',') {
      Append("\\,");
    } else if (c == '\\') {
      Append("\\\\");
    } else {
      Append("%lc", c);
    }
  }
}

// Write the log message to the log file currently opened.
void LogMessageBuilder::WriteToLogFile() {
  ASSERT(pos_ <= Logger::kMessageBufferSize);
  size_t rv = fwrite(Logger::message_buffer_, 1, pos_, Logger::logfile_);
  ASSERT(rv == static_cast<size_t>(pos_));
  USE(rv);
}

// Write a null-terminated string to to the log file currently opened.
void LogMessageBuilder::WriteCStringToLogFile(const char* str) {
  size_t len = strlen(str);
  size_t rv = fwrite(str, 1, len, Logger::logfile_);
  ASSERT(rv == len);
  USE(rv);
}
#endif


//
// Logger class implementation.
//
Ticker* Logger::ticker_ = NULL;
char* Logger::message_buffer_ = NULL;
FILE* Logger::logfile_ = NULL;
Profiler* Logger::profiler_ = NULL;
Mutex* Logger::mutex_ = NULL;
VMState* Logger::current_state_ = NULL;
VMState Logger::bottom_state_(EXTERNAL);
SlidingStateWindow* Logger::sliding_state_window_ = NULL;

#endif  // ENABLE_LOGGING_AND_PROFILING


void Logger::Preamble(const char* content) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
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
  if (logfile_ == NULL) return;
  LogMessageBuilder msg;
  msg.Append("%s,\"%s\"\n", name, value);
  msg.WriteToLogFile();
}
#endif


void Logger::IntEvent(const char* name, int value) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("%s,%d\n", name, value);
  msg.WriteToLogFile();
#endif
}


void Logger::HandleEvent(const char* name, Object** location) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_handles) return;
  LogMessageBuilder msg;
  msg.Append("%s,0x%x\n", name,
             reinterpret_cast<unsigned int>(location));
  msg.WriteToLogFile();
#endif
}


#ifdef ENABLE_LOGGING_AND_PROFILING
// ApiEvent is private so all the calls come from the Logger class.  It is the
// caller's responsibility to ensure that logfile_ is not NULL and that
// FLAG_log_api is true.
void Logger::ApiEvent(const char* format, ...) {
  ASSERT(logfile_ != NULL && FLAG_log_api);
  LogMessageBuilder msg;
  va_list ap;
  va_start(ap, format);
  msg.Append(format, ap);
  va_end(ap);
  msg.WriteToLogFile();
}
#endif


void Logger::ApiNamedSecurityCheck(Object* key) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_api) return;
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
                                unsigned start,
                                unsigned end) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_prof) return;
  LogMessageBuilder msg;
  msg.Append("shared-library,\"%s\",0x%08x,0x%08x\n", library_path,
             start, end);
  msg.WriteToLogFile();
#endif
}


void Logger::SharedLibraryEvent(const wchar_t* library_path,
                                unsigned start,
                                unsigned end) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_prof) return;
  LogMessageBuilder msg;
  msg.Append("shared-library,\"%ls\",0x%08x,0x%08x\n", library_path,
             start, end);
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
  if (logfile_ == NULL || !FLAG_log_regexp) return;
  LogMessageBuilder msg;
  msg.Append("regexp-compile,");
  LogRegExpSource(regexp);
  msg.Append(in_cache ? ",hit\n" : ",miss\n");
  msg.WriteToLogFile();
#endif
}


void Logger::LogRuntime(Vector<const char> format, JSArray* args) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_runtime) return;
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
  if (logfile_ == NULL || !FLAG_log_api) return;
  ApiEvent("api,check-security,%u\n", index);
#endif
}


void Logger::ApiNamedPropertyAccess(const char* tag,
                                    JSObject* holder,
                                    Object* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  ASSERT(name->IsString());
  if (logfile_ == NULL || !FLAG_log_api) return;
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
  if (logfile_ == NULL || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  Logger::ApiEvent("api,%s,\"%s\",%u\n", tag, *class_name, index);
#endif
}

void Logger::ApiObjectAccess(const char* tag, JSObject* object) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_api) return;
  String* class_name_obj = object->class_name();
  SmartPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  Logger::ApiEvent("api,%s,\"%s\"\n", tag, *class_name);
#endif
}


void Logger::ApiEntryCall(const char* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_api) return;
  Logger::ApiEvent("api,%s\n", name);
#endif
}


void Logger::NewEvent(const char* name, void* object, size_t size) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("new,%s,0x%x,%u\n", name,
             reinterpret_cast<unsigned int>(object),
             static_cast<unsigned int>(size));
  msg.WriteToLogFile();
#endif
}


void Logger::DeleteEvent(const char* name, void* object) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("delete,%s,0x%x\n", name,
             reinterpret_cast<unsigned int>(object));
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(const char* tag, Code* code, const char* comment) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("code-creation,%s,0x%x,%d,\"", tag,
             reinterpret_cast<unsigned int>(code->address()),
             code->ExecutableSize());
  for (const char* p = comment; *p != '\0'; p++) {
    if (*p == '"') {
      msg.Append('\\');
    }
    msg.Append(*p);
  }
  msg.Append('"');
  msg.Append('\n');
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(const char* tag, Code* code, String* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("code-creation,%s,0x%x,%d,\"%s\"\n", tag,
             reinterpret_cast<unsigned int>(code->address()),
             code->ExecutableSize(), *str);
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(const char* tag, Code* code, String* name,
                             String* source, int line) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  SmartPointer<char> str =
      name->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  SmartPointer<char> sourcestr =
      source->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("code-creation,%s,0x%x,%d,\"%s %s:%d\"\n", tag,
             reinterpret_cast<unsigned int>(code->address()),
             code->ExecutableSize(),
             *str, *sourcestr, line);
  msg.WriteToLogFile();
#endif
}


void Logger::CodeCreateEvent(const char* tag, Code* code, int args_count) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("code-creation,%s,0x%x,%d,\"args_count: %d\"\n", tag,
             reinterpret_cast<unsigned int>(code->address()),
             code->ExecutableSize(),
             args_count);
  msg.WriteToLogFile();
#endif
}


void Logger::RegExpCodeCreateEvent(Code* code, String* source) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("code-creation,%s,0x%x,%d,\"", "RegExp",
             reinterpret_cast<unsigned int>(code->address()),
             code->ExecutableSize());
  msg.AppendDetailed(source, false);
  msg.Append("\"\n");
  msg.WriteToLogFile();
#endif
}


void Logger::CodeAllocateEvent(Code* code, Assembler* assem) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("code-allocate,0x%x,0x%x\n",
             reinterpret_cast<unsigned int>(code->address()),
             reinterpret_cast<unsigned int>(assem));
  msg.WriteToLogFile();
#endif
}


void Logger::CodeMoveEvent(Address from, Address to) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("code-move,0x%x,0x%x\n",
             reinterpret_cast<unsigned int>(from),
             reinterpret_cast<unsigned int>(to));
  msg.WriteToLogFile();
#endif
}


void Logger::CodeDeleteEvent(Address from) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("code-delete,0x%x\n", reinterpret_cast<unsigned int>(from));
  msg.WriteToLogFile();
#endif
}


void Logger::BeginCodeRegionEvent(CodeRegion* region,
                                  Assembler* masm,
                                  const char* name) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("begin-code-region,0x%x,0x%x,0x%x,%s\n",
             reinterpret_cast<unsigned int>(region),
             reinterpret_cast<unsigned int>(masm),
             masm->pc_offset(),
             name);
  msg.WriteToLogFile();
#endif
}


void Logger::EndCodeRegionEvent(CodeRegion* region, Assembler* masm) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_code) return;
  LogMessageBuilder msg;
  msg.Append("end-code-region,0x%x,0x%x,0x%x\n",
             reinterpret_cast<unsigned int>(region),
             reinterpret_cast<unsigned int>(masm),
             masm->pc_offset());
  msg.WriteToLogFile();
#endif
}


void Logger::ResourceEvent(const char* name, const char* tag) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log) return;
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
  if (logfile_ == NULL || !FLAG_log_suspect) return;
  LogMessageBuilder msg;
  String* class_name = obj->IsJSObject()
                       ? JSObject::cast(obj)->class_name()
                       : Heap::empty_string();
  ScopedLock sl(mutex_);
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
  if (logfile_ == NULL || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-sample-begin,\"%s\",\"%s\"\n", space, kind);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleEndEvent(const char* space, const char* kind) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-sample-end,\"%s\",\"%s\"\n", space, kind);
  msg.WriteToLogFile();
#endif
}


void Logger::HeapSampleItemEvent(const char* type, int number, int bytes) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log_gc) return;
  LogMessageBuilder msg;
  msg.Append("heap-sample-item,%s,%d,%d\n", type, number, bytes);
  msg.WriteToLogFile();
#endif
}


void Logger::DebugTag(const char* call_site_tag) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log) return;
  LogMessageBuilder msg;
  msg.Append("debug-tag,%s\n", call_site_tag);
  msg.WriteToLogFile();
#endif
}


void Logger::DebugEvent(const char* event_type, Vector<uint16_t> parameter) {
#ifdef ENABLE_LOGGING_AND_PROFILING
  if (logfile_ == NULL || !FLAG_log) return;
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
  if (logfile_ == NULL || !FLAG_prof) return;
  LogMessageBuilder msg;
  msg.Append("tick,0x%x,0x%x,%d", sample->pc, sample->sp,
             static_cast<int>(sample->state));
  if (overflow) {
    msg.Append(",overflow");
  }
  for (int i = 0; i < sample->frames_count; ++i) {
    msg.Append(",0x%x", reinterpret_cast<uint32_t>(sample->stack[i]));
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


bool Logger::IsProfilerPaused() {
  return profiler_->paused();
}


void Logger::PauseProfiler() {
  profiler_->pause();
}


void Logger::ResumeProfiler() {
  profiler_->resume();
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

  bool open_log_file = FLAG_log || FLAG_log_runtime || FLAG_log_api
      || FLAG_log_code || FLAG_log_gc || FLAG_log_handles || FLAG_log_suspect
      || FLAG_log_regexp || FLAG_log_state_changes;

  // If we're logging anything, we need to open the log file.
  if (open_log_file) {
    if (strcmp(FLAG_logfile, "-") == 0) {
      logfile_ = stdout;
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
              uint32_t time = static_cast<uint32_t>(OS::TimeCurrentMillis());
              stream.Add("%u", time);
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
      logfile_ = OS::FOpen(*expanded, OS::LogFileOpenMode);
    } else {
      logfile_ = OS::FOpen(FLAG_logfile, OS::LogFileOpenMode);
    }
    message_buffer_ = NewArray<char>(kMessageBufferSize);
    mutex_ = OS::CreateMutex();
  }

  current_state_ = &bottom_state_;

  // as log is initialized early with V8, we can assume that JS execution
  // frames can never reach this point on stack
  int stack_var;
  ticker_ = new Ticker(1, reinterpret_cast<unsigned int>(&stack_var));

  if (FLAG_sliding_state_window && sliding_state_window_ == NULL) {
    sliding_state_window_ = new SlidingStateWindow();
  }

  if (FLAG_prof) {
    profiler_ = new Profiler();
    if (!FLAG_prof_auto)
      profiler_->pause();
    profiler_->Engage();
  }

  return true;

#else
  return false;
#endif
}


void Logger::TearDown() {
#ifdef ENABLE_LOGGING_AND_PROFILING
  // Stop the profiler before closing the file.
  if (profiler_ != NULL) {
    profiler_->Disengage();
    delete profiler_;
    profiler_ = NULL;
  }

  delete sliding_state_window_;

  delete ticker_;

  if (logfile_ != NULL) {
    fclose(logfile_);
    logfile_ = NULL;
    delete mutex_;
    mutex_ = NULL;
    DeleteArray(message_buffer_);
  }
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


//
// VMState class implementation.  A simple stack of VM states held by the
// logger and partially threaded through the call stack.  States are pushed by
// VMState construction and popped by destruction.
//
#ifdef ENABLE_LOGGING_AND_PROFILING
static const char* StateToString(StateTag state) {
  switch (state) {
    case JS:
      return "JS";
    case GC:
      return "GC";
    case COMPILER:
      return "COMPILER";
    case OTHER:
      return "OTHER";
    default:
      UNREACHABLE();
      return NULL;
  }
}

VMState::VMState(StateTag state) {
#if !defined(ENABLE_HEAP_PROTECTION)
  // When not protecting the heap, there is no difference between
  // EXTERNAL and OTHER.  As an optimizatin in that case, we will not
  // perform EXTERNAL->OTHER transitions through the API.  We thus
  // compress the two states into one.
  if (state == EXTERNAL) state = OTHER;
#endif
  state_ = state;
  previous_ = Logger::current_state_;
  Logger::current_state_ = this;

  if (FLAG_log_state_changes) {
    LOG(UncheckedStringEvent("Entering", StateToString(state_)));
    if (previous_ != NULL) {
      LOG(UncheckedStringEvent("From", StateToString(previous_->state_)));
    }
  }

#ifdef ENABLE_HEAP_PROTECTION
  if (FLAG_protect_heap && previous_ != NULL) {
    if (state_ == EXTERNAL) {
      // We are leaving V8.
      ASSERT(previous_->state_ != EXTERNAL);
      Heap::Protect();
    } else if (previous_->state_ == EXTERNAL) {
      // We are entering V8.
      Heap::Unprotect();
    }
  }
#endif
}


VMState::~VMState() {
  Logger::current_state_ = previous_;

  if (FLAG_log_state_changes) {
    LOG(UncheckedStringEvent("Leaving", StateToString(state_)));
    if (previous_ != NULL) {
      LOG(UncheckedStringEvent("To", StateToString(previous_->state_)));
    }
  }

#ifdef ENABLE_HEAP_PROTECTION
  if (FLAG_protect_heap && previous_ != NULL) {
    if (state_ == EXTERNAL) {
      // We are reentering V8.
      ASSERT(previous_->state_ != EXTERNAL);
      Heap::Unprotect();
    } else if (previous_->state_ == EXTERNAL) {
      // We are leaving V8.
      Heap::Protect();
    }
  }
#endif
}
#endif

} }  // namespace v8::internal
