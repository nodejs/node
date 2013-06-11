// Copyright 2011 the V8 project authors. All rights reserved.
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
#include "code-stubs.h"
#include "deoptimizer.h"
#include "global-handles.h"
#include "log.h"
#include "macro-assembler.h"
#include "platform.h"
#include "runtime-profiler.h"
#include "serialize.h"
#include "string-stream.h"
#include "vm-state-inl.h"

namespace v8 {
namespace internal {

// The Profiler samples pc and sp values for the main thread.
// Each sample is appended to a circular buffer.
// An independent thread removes data and writes it to the log.
// This design minimizes the time spent in the sampler.
//
class Profiler: public Thread {
 public:
  explicit Profiler(Isolate* isolate);
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
  bool paused() const { return paused_; }
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

 private:
  // Returns the next index in the cyclic buffer.
  int Succ(int index) { return (index + 1) % kBufferSize; }

  Isolate* isolate_;
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
  bool paused_;
};


//
// Ticker used to provide ticks to the profiler and the sliding state
// window.
//
class Ticker: public Sampler {
 public:
  Ticker(Isolate* isolate, int interval):
      Sampler(isolate, interval),
      profiler_(NULL) {}

  ~Ticker() { if (IsActive()) Stop(); }

  virtual void Tick(TickSample* sample) {
    if (profiler_) profiler_->Insert(sample);
  }

  void SetProfiler(Profiler* profiler) {
    ASSERT(profiler_ == NULL);
    profiler_ = profiler;
    IncreaseProfilingDepth();
    if (!FLAG_prof_lazy && !IsActive()) Start();
  }

  void ClearProfiler() {
    DecreaseProfilingDepth();
    profiler_ = NULL;
    if (IsActive()) Stop();
  }

 private:
  Profiler* profiler_;
};


//
// Profiler implementation.
//
Profiler::Profiler(Isolate* isolate)
    : Thread("v8:Profiler"),
      isolate_(isolate),
      head_(0),
      tail_(0),
      overflow_(false),
      buffer_semaphore_(OS::CreateSemaphore(0)),
      engaged_(false),
      running_(false),
      paused_(false) {
}


void Profiler::Engage() {
  if (engaged_) return;
  engaged_ = true;

  OS::LogSharedLibraryAddresses();

  // Start thread processing the profiler buffer.
  running_ = true;
  Start();

  // Register to get ticks.
  Logger* logger = isolate_->logger();
  logger->ticker_->SetProfiler(this);

  logger->ProfilerBeginEvent();
}


void Profiler::Disengage() {
  if (!engaged_) return;

  // Stop receiving ticks.
  isolate_->logger()->ticker_->ClearProfiler();

  // Terminate the worker thread by setting running_ to false,
  // inserting a fake element in the queue and then wait for
  // the thread to terminate.
  running_ = false;
  TickSample sample;
  // Reset 'paused_' flag, otherwise semaphore may not be signalled.
  resume();
  Insert(&sample);
  Join();

  LOG(ISOLATE, UncheckedStringEvent("profiler", "end"));
}


void Profiler::Run() {
  TickSample sample;
  bool overflow = Remove(&sample);
  while (running_) {
    LOG(isolate_, TickEvent(&sample, overflow));
    overflow = Remove(&sample);
  }
}


// Low-level profiling event structures.

struct LowLevelCodeCreateStruct {
  static const char kTag = 'C';

  int32_t name_size;
  Address code_address;
  int32_t code_size;
};


struct LowLevelCodeMoveStruct {
  static const char kTag = 'M';

  Address from_address;
  Address to_address;
};


struct LowLevelCodeDeleteStruct {
  static const char kTag = 'D';

  Address address;
};


struct LowLevelSnapshotPositionStruct {
  static const char kTag = 'P';

  Address address;
  int32_t position;
};


static const char kCodeMovingGCTag = 'G';


//
// Logger class implementation.
//

class Logger::NameMap {
 public:
  NameMap() : impl_(&PointerEquals) {}

  ~NameMap() {
    for (HashMap::Entry* p = impl_.Start(); p != NULL; p = impl_.Next(p)) {
      DeleteArray(static_cast<const char*>(p->value));
    }
  }

  void Insert(Address code_address, const char* name, int name_size) {
    HashMap::Entry* entry = FindOrCreateEntry(code_address);
    if (entry->value == NULL) {
      entry->value = CopyName(name, name_size);
    }
  }

  const char* Lookup(Address code_address) {
    HashMap::Entry* entry = FindEntry(code_address);
    return (entry != NULL) ? static_cast<const char*>(entry->value) : NULL;
  }

  void Remove(Address code_address) {
    HashMap::Entry* entry = FindEntry(code_address);
    if (entry != NULL) {
      DeleteArray(static_cast<char*>(entry->value));
      RemoveEntry(entry);
    }
  }

  void Move(Address from, Address to) {
    if (from == to) return;
    HashMap::Entry* from_entry = FindEntry(from);
    ASSERT(from_entry != NULL);
    void* value = from_entry->value;
    RemoveEntry(from_entry);
    HashMap::Entry* to_entry = FindOrCreateEntry(to);
    ASSERT(to_entry->value == NULL);
    to_entry->value = value;
  }

 private:
  static bool PointerEquals(void* lhs, void* rhs) {
    return lhs == rhs;
  }

  static char* CopyName(const char* name, int name_size) {
    char* result = NewArray<char>(name_size + 1);
    for (int i = 0; i < name_size; ++i) {
      char c = name[i];
      if (c == '\0') c = ' ';
      result[i] = c;
    }
    result[name_size] = '\0';
    return result;
  }

  HashMap::Entry* FindOrCreateEntry(Address code_address) {
    return impl_.Lookup(code_address, ComputePointerHash(code_address), true);
  }

  HashMap::Entry* FindEntry(Address code_address) {
    return impl_.Lookup(code_address, ComputePointerHash(code_address), false);
  }

  void RemoveEntry(HashMap::Entry* entry) {
    impl_.Remove(entry->key, entry->hash);
  }

  HashMap impl_;

  DISALLOW_COPY_AND_ASSIGN(NameMap);
};


class Logger::NameBuffer {
 public:
  NameBuffer() { Reset(); }

  void Reset() {
    utf8_pos_ = 0;
  }

  void AppendString(String* str) {
    if (str == NULL) return;
    int uc16_length = Min(str->length(), kUtf16BufferSize);
    String::WriteToFlat(str, utf16_buffer, 0, uc16_length);
    int previous = unibrow::Utf16::kNoPreviousCharacter;
    for (int i = 0; i < uc16_length && utf8_pos_ < kUtf8BufferSize; ++i) {
      uc16 c = utf16_buffer[i];
      if (c <= unibrow::Utf8::kMaxOneByteChar) {
        utf8_buffer_[utf8_pos_++] = static_cast<char>(c);
      } else {
        int char_length = unibrow::Utf8::Length(c, previous);
        if (utf8_pos_ + char_length > kUtf8BufferSize) break;
        unibrow::Utf8::Encode(utf8_buffer_ + utf8_pos_, c, previous);
        utf8_pos_ += char_length;
      }
      previous = c;
    }
  }

  void AppendBytes(const char* bytes, int size) {
    size = Min(size, kUtf8BufferSize - utf8_pos_);
    OS::MemCopy(utf8_buffer_ + utf8_pos_, bytes, size);
    utf8_pos_ += size;
  }

  void AppendBytes(const char* bytes) {
    AppendBytes(bytes, StrLength(bytes));
  }

  void AppendByte(char c) {
    if (utf8_pos_ >= kUtf8BufferSize) return;
    utf8_buffer_[utf8_pos_++] = c;
  }

  void AppendInt(int n) {
    Vector<char> buffer(utf8_buffer_ + utf8_pos_, kUtf8BufferSize - utf8_pos_);
    int size = OS::SNPrintF(buffer, "%d", n);
    if (size > 0 && utf8_pos_ + size <= kUtf8BufferSize) {
      utf8_pos_ += size;
    }
  }

  void AppendHex(uint32_t n) {
    Vector<char> buffer(utf8_buffer_ + utf8_pos_, kUtf8BufferSize - utf8_pos_);
    int size = OS::SNPrintF(buffer, "%x", n);
    if (size > 0 && utf8_pos_ + size <= kUtf8BufferSize) {
      utf8_pos_ += size;
    }
  }

  const char* get() { return utf8_buffer_; }
  int size() const { return utf8_pos_; }

 private:
  static const int kUtf8BufferSize = 512;
  static const int kUtf16BufferSize = 128;

  int utf8_pos_;
  char utf8_buffer_[kUtf8BufferSize];
  uc16 utf16_buffer[kUtf16BufferSize];
};


Logger::Logger(Isolate* isolate)
  : isolate_(isolate),
    ticker_(NULL),
    profiler_(NULL),
    log_events_(NULL),
    logging_nesting_(0),
    cpu_profiler_nesting_(0),
    log_(new Log(this)),
    name_buffer_(new NameBuffer),
    address_to_name_map_(NULL),
    is_initialized_(false),
    code_event_handler_(NULL),
    last_address_(NULL),
    prev_sp_(NULL),
    prev_function_(NULL),
    prev_to_(NULL),
    prev_code_(NULL),
    epoch_(0) {
}


Logger::~Logger() {
  delete address_to_name_map_;
  delete name_buffer_;
  delete log_;
}


void Logger::IssueCodeAddedEvent(Code* code,
                                 Script* script,
                                 const char* name,
                                 size_t name_len) {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADDED;
  event.code_start = code->instruction_start();
  event.code_len = code->instruction_size();
  Handle<Script> script_handle =
      script != NULL ? Handle<Script>(script) : Handle<Script>();
  event.script = v8::Handle<v8::Script>(ToApi<v8::Script>(script_handle));
  event.name.str = name;
  event.name.len = name_len;

  code_event_handler_(&event);
}


void Logger::IssueCodeMovedEvent(Address from, Address to) {
  Code* from_code = Code::cast(HeapObject::FromAddress(from));

  JitCodeEvent event;
  event.type = JitCodeEvent::CODE_MOVED;
  event.code_start = from_code->instruction_start();
  event.code_len = from_code->instruction_size();

  // Calculate the header size.
  const size_t header_size =
      from_code->instruction_start() - reinterpret_cast<byte*>(from_code);

  // Calculate the new start address of the instructions.
  event.new_code_start =
      reinterpret_cast<byte*>(HeapObject::FromAddress(to)) + header_size;

  code_event_handler_(&event);
}


void Logger::IssueCodeRemovedEvent(Address from) {
  Code* from_code = Code::cast(HeapObject::FromAddress(from));

  JitCodeEvent event;
  event.type = JitCodeEvent::CODE_REMOVED;
  event.code_start = from_code->instruction_start();
  event.code_len = from_code->instruction_size();

  code_event_handler_(&event);
}

void Logger::IssueAddCodeLinePosInfoEvent(
    void* jit_handler_data,
    int pc_offset,
    int position,
    JitCodeEvent::PositionType position_type) {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADD_LINE_POS_INFO;
  event.user_data = jit_handler_data;
  event.line_info.offset = pc_offset;
  event.line_info.pos = position;
  event.line_info.position_type = position_type;

  code_event_handler_(&event);
}

void* Logger::IssueStartCodePosInfoEvent() {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_START_LINE_INFO_RECORDING;

  code_event_handler_(&event);
  return event.user_data;
}

void Logger::IssueEndCodePosInfoEvent(Code* code, void* jit_handler_data) {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_END_LINE_INFO_RECORDING;
  event.code_start = code->instruction_start();
  event.user_data = jit_handler_data;

  code_event_handler_(&event);
}

#define DECLARE_EVENT(ignore1, name) name,
static const char* const kLogEventsNames[Logger::NUMBER_OF_LOG_EVENTS] = {
  LOG_EVENTS_AND_TAGS_LIST(DECLARE_EVENT)
};
#undef DECLARE_EVENT


void Logger::ProfilerBeginEvent() {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("profiler,\"begin\",%d\n", kSamplingIntervalMs);
  msg.WriteToLogFile();
}


void Logger::StringEvent(const char* name, const char* value) {
  if (FLAG_log) UncheckedStringEvent(name, value);
}


void Logger::UncheckedStringEvent(const char* name, const char* value) {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,\"%s\"\n", name, value);
  msg.WriteToLogFile();
}


void Logger::IntEvent(const char* name, int value) {
  if (FLAG_log) UncheckedIntEvent(name, value);
}


void Logger::IntPtrTEvent(const char* name, intptr_t value) {
  if (FLAG_log) UncheckedIntPtrTEvent(name, value);
}


void Logger::UncheckedIntEvent(const char* name, int value) {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%d\n", name, value);
  msg.WriteToLogFile();
}


void Logger::UncheckedIntPtrTEvent(const char* name, intptr_t value) {
  if (!log_->IsEnabled()) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%" V8_PTR_PREFIX "d\n", name, value);
  msg.WriteToLogFile();
}


void Logger::HandleEvent(const char* name, Object** location) {
  if (!log_->IsEnabled() || !FLAG_log_handles) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,0x%" V8PRIxPTR "\n", name, location);
  msg.WriteToLogFile();
}


// ApiEvent is private so all the calls come from the Logger class.  It is the
// caller's responsibility to ensure that log is enabled and that
// FLAG_log_api is true.
void Logger::ApiEvent(const char* format, ...) {
  ASSERT(log_->IsEnabled() && FLAG_log_api);
  LogMessageBuilder msg(this);
  va_list ap;
  va_start(ap, format);
  msg.AppendVA(format, ap);
  va_end(ap);
  msg.WriteToLogFile();
}


void Logger::ApiNamedSecurityCheck(Object* key) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  if (key->IsString()) {
    SmartArrayPointer<char> str =
        String::cast(key)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    ApiEvent("api,check-security,\"%s\"\n", *str);
  } else if (key->IsSymbol()) {
    Symbol* symbol = Symbol::cast(key);
    if (symbol->name()->IsUndefined()) {
      ApiEvent("api,check-security,symbol(hash %x)\n",
               Symbol::cast(key)->Hash());
    } else {
      SmartArrayPointer<char> str = String::cast(symbol->name())->ToCString(
          DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
      ApiEvent("api,check-security,symbol(\"%s\" hash %x)\n",
               *str,
               Symbol::cast(key)->Hash());
    }
  } else if (key->IsUndefined()) {
    ApiEvent("api,check-security,undefined\n");
  } else {
    ApiEvent("api,check-security,['no-name']\n");
  }
}


void Logger::SharedLibraryEvent(const char* library_path,
                                uintptr_t start,
                                uintptr_t end) {
  if (!log_->IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("shared-library,\"%s\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR "\n",
             library_path,
             start,
             end);
  msg.WriteToLogFile();
}


void Logger::SharedLibraryEvent(const wchar_t* library_path,
                                uintptr_t start,
                                uintptr_t end) {
  if (!log_->IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("shared-library,\"%ls\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR "\n",
             library_path,
             start,
             end);
  msg.WriteToLogFile();
}


void Logger::TimerEvent(StartEnd se, const char* name) {
  if (!log_->IsEnabled()) return;
  ASSERT(FLAG_log_internal_timer_events);
  LogMessageBuilder msg(this);
  int since_epoch = static_cast<int>(OS::Ticks() - epoch_);
  const char* format = (se == START) ? "timer-event-start,\"%s\",%ld\n"
                                     : "timer-event-end,\"%s\",%ld\n";
  msg.Append(format, name, since_epoch);
  msg.WriteToLogFile();
}


void Logger::EnterExternal(Isolate* isolate) {
  LOG(isolate, TimerEvent(START, TimerEventScope::v8_external));
  ASSERT(isolate->current_vm_state() == JS);
  isolate->set_current_vm_state(EXTERNAL);
}


void Logger::LeaveExternal(Isolate* isolate) {
  LOG(isolate, TimerEvent(END, TimerEventScope::v8_external));
  ASSERT(isolate->current_vm_state() == EXTERNAL);
  isolate->set_current_vm_state(JS);
}


void Logger::TimerEventScope::LogTimerEvent(StartEnd se) {
  LOG(isolate_, TimerEvent(se, name_));
}


const char* Logger::TimerEventScope::v8_recompile_synchronous =
    "V8.RecompileSynchronous";
const char* Logger::TimerEventScope::v8_recompile_parallel =
    "V8.RecompileParallel";
const char* Logger::TimerEventScope::v8_compile_full_code =
    "V8.CompileFullCode";
const char* Logger::TimerEventScope::v8_execute = "V8.Execute";
const char* Logger::TimerEventScope::v8_external = "V8.External";


void Logger::LogRegExpSource(Handle<JSRegExp> regexp) {
  // Prints "/" + re.source + "/" +
  //      (re.global?"g":"") + (re.ignorecase?"i":"") + (re.multiline?"m":"")
  LogMessageBuilder msg(this);

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


void Logger::RegExpCompileEvent(Handle<JSRegExp> regexp, bool in_cache) {
  if (!log_->IsEnabled() || !FLAG_log_regexp) return;
  LogMessageBuilder msg(this);
  msg.Append("regexp-compile,");
  LogRegExpSource(regexp);
  msg.Append(in_cache ? ",hit\n" : ",miss\n");
  msg.WriteToLogFile();
}


void Logger::LogRuntime(Vector<const char> format,
                        JSArray* args) {
  if (!log_->IsEnabled() || !FLAG_log_runtime) return;
  HandleScope scope(isolate_);
  LogMessageBuilder msg(this);
  for (int i = 0; i < format.length(); i++) {
    char c = format[i];
    if (c == '%' && i <= format.length() - 2) {
      i++;
      ASSERT('0' <= format[i] && format[i] <= '9');
      MaybeObject* maybe = args->GetElement(format[i] - '0');
      Object* obj;
      if (!maybe->ToObject(&obj)) {
        msg.Append("<exception>");
        continue;
      }
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
}


void Logger::ApiIndexedSecurityCheck(uint32_t index) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,check-security,%u\n", index);
}


void Logger::ApiNamedPropertyAccess(const char* tag,
                                    JSObject* holder,
                                    Object* name) {
  ASSERT(name->IsName());
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartArrayPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  if (name->IsString()) {
    SmartArrayPointer<char> property_name =
        String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    ApiEvent("api,%s,\"%s\",\"%s\"\n", tag, *class_name, *property_name);
  } else {
    Symbol* symbol = Symbol::cast(name);
    uint32_t hash = symbol->Hash();
    if (symbol->name()->IsUndefined()) {
      ApiEvent("api,%s,\"%s\",symbol(hash %x)\n", tag, *class_name, hash);
    } else {
      SmartArrayPointer<char> str = String::cast(symbol->name())->ToCString(
          DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
      ApiEvent("api,%s,\"%s\",symbol(\"%s\" hash %x)\n",
               tag, *class_name, *str, hash);
    }
  }
}

void Logger::ApiIndexedPropertyAccess(const char* tag,
                                      JSObject* holder,
                                      uint32_t index) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartArrayPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  ApiEvent("api,%s,\"%s\",%u\n", tag, *class_name, index);
}

void Logger::ApiObjectAccess(const char* tag, JSObject* object) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = object->class_name();
  SmartArrayPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  ApiEvent("api,%s,\"%s\"\n", tag, *class_name);
}


void Logger::ApiEntryCall(const char* name) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,%s\n", name);
}


void Logger::NewEvent(const char* name, void* object, size_t size) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("new,%s,0x%" V8PRIxPTR ",%u\n", name, object,
             static_cast<unsigned int>(size));
  msg.WriteToLogFile();
}


void Logger::DeleteEvent(const char* name, void* object) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("delete,%s,0x%" V8PRIxPTR "\n", name, object);
  msg.WriteToLogFile();
}


void Logger::NewEventStatic(const char* name, void* object, size_t size) {
  Isolate::Current()->logger()->NewEvent(name, object, size);
}


void Logger::DeleteEventStatic(const char* name, void* object) {
  Isolate::Current()->logger()->DeleteEvent(name, object);
}

void Logger::CallbackEventInternal(const char* prefix, Name* name,
                                   Address entry_point) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,-3,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[CALLBACK_TAG]);
  msg.AppendAddress(entry_point);
  if (name->IsString()) {
    SmartArrayPointer<char> str =
        String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    msg.Append(",1,\"%s%s\"", prefix, *str);
  } else {
    Symbol* symbol = Symbol::cast(name);
    if (symbol->name()->IsUndefined()) {
      msg.Append(",1,symbol(hash %x)", prefix, symbol->Hash());
    } else {
      SmartArrayPointer<char> str = String::cast(symbol->name())->ToCString(
          DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
      msg.Append(",1,symbol(\"%s\" hash %x)", prefix, *str, symbol->Hash());
    }
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::CallbackEvent(Name* name, Address entry_point) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  CallbackEventInternal("", name, entry_point);
}


void Logger::GetterCallbackEvent(Name* name, Address entry_point) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  CallbackEventInternal("get ", name, entry_point);
}


void Logger::SetterCallbackEvent(Name* name, Address entry_point) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  CallbackEventInternal("set ", name, entry_point);
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             const char* comment) {
  if (!is_logging_code_events()) return;
  if (FLAG_ll_prof || Serializer::enabled() || code_event_handler_ != NULL) {
    name_buffer_->Reset();
    name_buffer_->AppendBytes(kLogEventsNames[tag]);
    name_buffer_->AppendByte(':');
    name_buffer_->AppendBytes(comment);
  }
  if (code_event_handler_ != NULL) {
    IssueCodeAddedEvent(code, NULL, name_buffer_->get(), name_buffer_->size());
  }
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) {
    LowLevelCodeCreateEvent(code, name_buffer_->get(), name_buffer_->size());
  }
  if (Serializer::enabled()) {
    RegisterSnapshotCodeName(code, name_buffer_->get(), name_buffer_->size());
  }
  if (!FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,%d,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag],
             code->kind());
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"", code->ExecutableSize());
  for (const char* p = comment; *p != '\0'; p++) {
    if (*p == '"') {
      msg.Append('\\');
    }
    msg.Append(*p);
  }
  msg.Append('"');
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             Name* name) {
  if (!is_logging_code_events()) return;
  if (FLAG_ll_prof || Serializer::enabled() || code_event_handler_ != NULL) {
    name_buffer_->Reset();
    name_buffer_->AppendBytes(kLogEventsNames[tag]);
    name_buffer_->AppendByte(':');
    if (name->IsString()) {
      name_buffer_->AppendString(String::cast(name));
    } else {
      Symbol* symbol = Symbol::cast(name);
      name_buffer_->AppendBytes("symbol(");
      if (!symbol->name()->IsUndefined()) {
        name_buffer_->AppendBytes("\"");
        name_buffer_->AppendString(String::cast(symbol->name()));
        name_buffer_->AppendBytes("\" ");
      }
      name_buffer_->AppendBytes("hash ");
      name_buffer_->AppendHex(symbol->Hash());
      name_buffer_->AppendByte(')');
    }
  }
  if (code_event_handler_ != NULL) {
    IssueCodeAddedEvent(code, NULL, name_buffer_->get(), name_buffer_->size());
  }
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) {
    LowLevelCodeCreateEvent(code, name_buffer_->get(), name_buffer_->size());
  }
  if (Serializer::enabled()) {
    RegisterSnapshotCodeName(code, name_buffer_->get(), name_buffer_->size());
  }
  if (!FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,%d,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag],
             code->kind());
  msg.AppendAddress(code->address());
  msg.Append(",%d,", code->ExecutableSize());
  if (name->IsString()) {
    msg.Append('"');
    msg.AppendDetailed(String::cast(name), false);
    msg.Append('"');
  } else {
    Symbol* symbol = Symbol::cast(name);
    msg.Append("symbol(");
    if (!symbol->name()->IsUndefined()) {
      msg.Append("\"");
      msg.AppendDetailed(String::cast(symbol->name()), false);
      msg.Append("\" ");
    }
    msg.Append("hash %x)", symbol->Hash());
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


// ComputeMarker must only be used when SharedFunctionInfo is known.
static const char* ComputeMarker(Code* code) {
  switch (code->kind()) {
    case Code::FUNCTION: return code->optimizable() ? "~" : "";
    case Code::OPTIMIZED_FUNCTION: return "*";
    default: return "";
  }
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             SharedFunctionInfo* shared,
                             CompilationInfo* info,
                             Name* name) {
  if (!is_logging_code_events()) return;
  if (FLAG_ll_prof || Serializer::enabled() || code_event_handler_ != NULL) {
    name_buffer_->Reset();
    name_buffer_->AppendBytes(kLogEventsNames[tag]);
    name_buffer_->AppendByte(':');
    name_buffer_->AppendBytes(ComputeMarker(code));
    if (name->IsString()) {
      name_buffer_->AppendString(String::cast(name));
    } else {
      Symbol* symbol = Symbol::cast(name);
      name_buffer_->AppendBytes("symbol(");
      if (!symbol->name()->IsUndefined()) {
        name_buffer_->AppendBytes("\"");
        name_buffer_->AppendString(String::cast(symbol->name()));
        name_buffer_->AppendBytes("\" ");
      }
      name_buffer_->AppendBytes("hash ");
      name_buffer_->AppendHex(symbol->Hash());
      name_buffer_->AppendByte(')');
    }
  }
  if (code_event_handler_ != NULL) {
    Script* script =
        shared->script()->IsScript() ? Script::cast(shared->script()) : NULL;
    IssueCodeAddedEvent(code,
                        script,
                        name_buffer_->get(),
                        name_buffer_->size());
  }
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) {
    LowLevelCodeCreateEvent(code, name_buffer_->get(), name_buffer_->size());
  }
  if (Serializer::enabled()) {
    RegisterSnapshotCodeName(code, name_buffer_->get(), name_buffer_->size());
  }
  if (!FLAG_log_code) return;
  if (code == Isolate::Current()->builtins()->builtin(
      Builtins::kLazyCompile))
    return;

  LogMessageBuilder msg(this);
  msg.Append("%s,%s,%d,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag],
             code->kind());
  msg.AppendAddress(code->address());
  msg.Append(",%d,", code->ExecutableSize());
  if (name->IsString()) {
    SmartArrayPointer<char> str =
        String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    msg.Append("\"%s\"", *str);
  } else {
    Symbol* symbol = Symbol::cast(name);
    msg.Append("symbol(");
    if (!symbol->name()->IsUndefined()) {
      msg.Append("\"");
      msg.AppendDetailed(String::cast(symbol->name()), false);
      msg.Append("\" ");
    }
    msg.Append("hash %x)", symbol->Hash());
  }
  msg.Append(',');
  msg.AppendAddress(shared->address());
  msg.Append(",%s", ComputeMarker(code));
  msg.Append('\n');
  msg.WriteToLogFile();
}


// Although, it is possible to extract source and line from
// the SharedFunctionInfo object, we left it to caller
// to leave logging functions free from heap allocations.
void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             SharedFunctionInfo* shared,
                             CompilationInfo* info,
                             Name* source, int line) {
  if (!is_logging_code_events()) return;
  if (FLAG_ll_prof || Serializer::enabled() || code_event_handler_ != NULL) {
    name_buffer_->Reset();
    name_buffer_->AppendBytes(kLogEventsNames[tag]);
    name_buffer_->AppendByte(':');
    name_buffer_->AppendBytes(ComputeMarker(code));
    name_buffer_->AppendString(shared->DebugName());
    name_buffer_->AppendByte(' ');
    if (source->IsString()) {
      name_buffer_->AppendString(String::cast(source));
    } else {
      name_buffer_->AppendBytes("symbol(hash ");
      name_buffer_->AppendHex(Name::cast(source)->Hash());
      name_buffer_->AppendByte(')');
    }
    name_buffer_->AppendByte(':');
    name_buffer_->AppendInt(line);
  }
  if (code_event_handler_ != NULL) {
    Script* script =
        shared->script()->IsScript() ? Script::cast(shared->script()) : NULL;
    IssueCodeAddedEvent(code,
                        script,
                        name_buffer_->get(),
                        name_buffer_->size());
  }
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) {
    LowLevelCodeCreateEvent(code, name_buffer_->get(), name_buffer_->size());
  }
  if (Serializer::enabled()) {
    RegisterSnapshotCodeName(code, name_buffer_->get(), name_buffer_->size());
  }
  if (!FLAG_log_code) return;
  LogMessageBuilder msg(this);
  SmartArrayPointer<char> name =
      shared->DebugName()->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("%s,%s,%d,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag],
             code->kind());
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"%s ", code->ExecutableSize(), *name);
  if (source->IsString()) {
    SmartArrayPointer<char> sourcestr =
       String::cast(source)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    msg.Append("%s", *sourcestr);
  } else {
    Symbol* symbol = Symbol::cast(source);
    msg.Append("symbol(");
    if (!symbol->name()->IsUndefined()) {
      msg.Append("\"");
      msg.AppendDetailed(String::cast(symbol->name()), false);
      msg.Append("\" ");
    }
    msg.Append("hash %x)", symbol->Hash());
  }
  msg.Append(":%d\",", line);
  msg.AppendAddress(shared->address());
  msg.Append(",%s", ComputeMarker(code));
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::CodeCreateEvent(LogEventsAndTags tag, Code* code, int args_count) {
  if (!is_logging_code_events()) return;
  if (FLAG_ll_prof || Serializer::enabled() || code_event_handler_ != NULL) {
    name_buffer_->Reset();
    name_buffer_->AppendBytes(kLogEventsNames[tag]);
    name_buffer_->AppendByte(':');
    name_buffer_->AppendInt(args_count);
  }
  if (code_event_handler_ != NULL) {
    IssueCodeAddedEvent(code, NULL, name_buffer_->get(), name_buffer_->size());
  }
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) {
    LowLevelCodeCreateEvent(code, name_buffer_->get(), name_buffer_->size());
  }
  if (Serializer::enabled()) {
    RegisterSnapshotCodeName(code, name_buffer_->get(), name_buffer_->size());
  }
  if (!FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,%d,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[tag],
             code->kind());
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"args_count: %d\"", code->ExecutableSize(), args_count);
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::CodeMovingGCEvent() {
  if (!log_->IsEnabled() || !FLAG_ll_prof) return;
  LowLevelLogWriteBytes(&kCodeMovingGCTag, sizeof(kCodeMovingGCTag));
  OS::SignalCodeMovingGC();
}


void Logger::RegExpCodeCreateEvent(Code* code, String* source) {
  if (!is_logging_code_events()) return;
  if (FLAG_ll_prof || Serializer::enabled() || code_event_handler_ != NULL) {
    name_buffer_->Reset();
    name_buffer_->AppendBytes(kLogEventsNames[REG_EXP_TAG]);
    name_buffer_->AppendByte(':');
    name_buffer_->AppendString(source);
  }
  if (code_event_handler_ != NULL) {
    IssueCodeAddedEvent(code, NULL, name_buffer_->get(), name_buffer_->size());
  }
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) {
    LowLevelCodeCreateEvent(code, name_buffer_->get(), name_buffer_->size());
  }
  if (Serializer::enabled()) {
    RegisterSnapshotCodeName(code, name_buffer_->get(), name_buffer_->size());
  }
  if (!FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,-2,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[REG_EXP_TAG]);
  msg.AppendAddress(code->address());
  msg.Append(",%d,\"", code->ExecutableSize());
  msg.AppendDetailed(source, false);
  msg.Append('\"');
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::CodeMoveEvent(Address from, Address to) {
  if (code_event_handler_ != NULL) IssueCodeMovedEvent(from, to);
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) LowLevelCodeMoveEvent(from, to);
  if (Serializer::enabled() && address_to_name_map_ != NULL) {
    address_to_name_map_->Move(from, to);
  }
  MoveEventInternal(CODE_MOVE_EVENT, from, to);
}


void Logger::CodeDeleteEvent(Address from) {
  if (code_event_handler_ != NULL) IssueCodeRemovedEvent(from);
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) LowLevelCodeDeleteEvent(from);
  if (Serializer::enabled() && address_to_name_map_ != NULL) {
    address_to_name_map_->Remove(from);
  }
  DeleteEventInternal(CODE_DELETE_EVENT, from);
}

void Logger::CodeLinePosInfoAddPositionEvent(void* jit_handler_data,
                                     int pc_offset,
                                     int position) {
  if (code_event_handler_ != NULL) {
    IssueAddCodeLinePosInfoEvent(jit_handler_data,
                                 pc_offset,
                                 position,
                                 JitCodeEvent::POSITION);
  }
}

void Logger::CodeLinePosInfoAddStatementPositionEvent(void* jit_handler_data,
                                                      int pc_offset,
                                                      int position) {
  if (code_event_handler_ != NULL) {
    IssueAddCodeLinePosInfoEvent(jit_handler_data,
                                 pc_offset,
                                 position,
                                 JitCodeEvent::STATEMENT_POSITION);
  }
}

void Logger::CodeStartLinePosInfoRecordEvent(PositionsRecorder* pos_recorder) {
  if (code_event_handler_ != NULL) {
      pos_recorder->AttachJITHandlerData(IssueStartCodePosInfoEvent());
  }
}

void Logger::CodeEndLinePosInfoRecordEvent(Code* code,
                                           void* jit_handler_data) {
  if (code_event_handler_ != NULL) {
    IssueEndCodePosInfoEvent(code, jit_handler_data);
  }
}

void Logger::SnapshotPositionEvent(Address addr, int pos) {
  if (!log_->IsEnabled()) return;
  if (FLAG_ll_prof) LowLevelSnapshotPositionEvent(addr, pos);
  if (Serializer::enabled() && address_to_name_map_ != NULL) {
    const char* code_name = address_to_name_map_->Lookup(addr);
    if (code_name == NULL) return;  // Not a code object.
    LogMessageBuilder msg(this);
    msg.Append("%s,%d,\"", kLogEventsNames[SNAPSHOT_CODE_NAME_EVENT], pos);
    for (const char* p = code_name; *p != '\0'; ++p) {
      if (*p == '"') msg.Append('\\');
      msg.Append(*p);
    }
    msg.Append("\"\n");
    msg.WriteToLogFile();
  }
  if (!FLAG_log_snapshot_positions) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[SNAPSHOT_POSITION_EVENT]);
  msg.AppendAddress(addr);
  msg.Append(",%d", pos);
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::SharedFunctionInfoMoveEvent(Address from, Address to) {
  MoveEventInternal(SHARED_FUNC_MOVE_EVENT, from, to);
}


void Logger::MoveEventInternal(LogEventsAndTags event,
                               Address from,
                               Address to) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[event]);
  msg.AppendAddress(from);
  msg.Append(',');
  msg.AppendAddress(to);
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::DeleteEventInternal(LogEventsAndTags event, Address from) {
  if (!log_->IsEnabled() || !FLAG_log_code) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[event]);
  msg.AppendAddress(from);
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::ResourceEvent(const char* name, const char* tag) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,%s,", name, tag);

  uint32_t sec, usec;
  if (OS::GetUserTime(&sec, &usec) != -1) {
    msg.Append("%d,%d,", sec, usec);
  }
  msg.Append("%.0f", OS::TimeCurrentMillis());

  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::SuspectReadEvent(Name* name, Object* obj) {
  if (!log_->IsEnabled() || !FLAG_log_suspect) return;
  LogMessageBuilder msg(this);
  String* class_name = obj->IsJSObject()
                       ? JSObject::cast(obj)->class_name()
                       : isolate_->heap()->empty_string();
  msg.Append("suspect-read,");
  msg.Append(class_name);
  msg.Append(',');
  if (name->IsString()) {
    msg.Append('"');
    msg.Append(String::cast(name));
    msg.Append('"');
  } else {
    Symbol* symbol = Symbol::cast(name);
    msg.Append("symbol(");
    if (!symbol->name()->IsUndefined()) {
      msg.Append("\"");
      msg.AppendDetailed(String::cast(symbol->name()), false);
      msg.Append("\" ");
    }
    msg.Append("hash %x)", symbol->Hash());
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


void Logger::HeapSampleBeginEvent(const char* space, const char* kind) {
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  // Using non-relative system time in order to be able to synchronize with
  // external memory profiling events (e.g. DOM memory size).
  msg.Append("heap-sample-begin,\"%s\",\"%s\",%.0f\n",
             space, kind, OS::TimeCurrentMillis());
  msg.WriteToLogFile();
}


void Logger::HeapSampleEndEvent(const char* space, const char* kind) {
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-sample-end,\"%s\",\"%s\"\n", space, kind);
  msg.WriteToLogFile();
}


void Logger::HeapSampleItemEvent(const char* type, int number, int bytes) {
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  LogMessageBuilder msg(this);
  msg.Append("heap-sample-item,%s,%d,%d\n", type, number, bytes);
  msg.WriteToLogFile();
}


void Logger::DebugTag(const char* call_site_tag) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  LogMessageBuilder msg(this);
  msg.Append("debug-tag,%s\n", call_site_tag);
  msg.WriteToLogFile();
}


void Logger::DebugEvent(const char* event_type, Vector<uint16_t> parameter) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  StringBuilder s(parameter.length() + 1);
  for (int i = 0; i < parameter.length(); ++i) {
    s.AddCharacter(static_cast<char>(parameter[i]));
  }
  char* parameter_string = s.Finalize();
  LogMessageBuilder msg(this);
  msg.Append("debug-queue-event,%s,%15.3f,%s\n",
             event_type,
             OS::TimeCurrentMillis(),
             parameter_string);
  DeleteArray(parameter_string);
  msg.WriteToLogFile();
}


void Logger::TickEvent(TickSample* sample, bool overflow) {
  if (!log_->IsEnabled() || !FLAG_prof) return;
  LogMessageBuilder msg(this);
  msg.Append("%s,", kLogEventsNames[TICK_EVENT]);
  msg.AppendAddress(sample->pc);
  msg.Append(',');
  msg.AppendAddress(sample->sp);
  msg.Append(",%ld", static_cast<int>(OS::Ticks() - epoch_));
  if (sample->has_external_callback) {
    msg.Append(",1,");
    msg.AppendAddress(sample->external_callback);
  } else {
    msg.Append(",0,");
    msg.AppendAddress(sample->tos);
  }
  msg.Append(",%d", static_cast<int>(sample->state));
  if (overflow) {
    msg.Append(",overflow");
  }
  for (int i = 0; i < sample->frames_count; ++i) {
    msg.Append(',');
    msg.AppendAddress(sample->stack[i]);
  }
  msg.Append('\n');
  msg.WriteToLogFile();
}


bool Logger::IsProfilerPaused() {
  return profiler_ == NULL || profiler_->paused();
}


void Logger::PauseProfiler() {
  if (!log_->IsEnabled()) return;
  if (profiler_ != NULL) {
    // It is OK to have negative nesting.
    if (--cpu_profiler_nesting_ == 0) {
      profiler_->pause();
      if (FLAG_prof_lazy) {
        ticker_->Stop();
        FLAG_log_code = false;
        LOG(ISOLATE, UncheckedStringEvent("profiler", "pause"));
      }
      --logging_nesting_;
    }
  }
}


void Logger::ResumeProfiler() {
  if (!log_->IsEnabled()) return;
  if (profiler_ != NULL) {
    if (cpu_profiler_nesting_++ == 0) {
      ++logging_nesting_;
      if (FLAG_prof_lazy) {
        profiler_->Engage();
        LOG(ISOLATE, UncheckedStringEvent("profiler", "resume"));
        FLAG_log_code = true;
        LogCompiledFunctions();
        LogAccessorCallbacks();
        if (!ticker_->IsActive()) ticker_->Start();
      }
      profiler_->resume();
    }
  }
}


// This function can be called when Log's mutex is acquired,
// either from main or Profiler's thread.
void Logger::LogFailure() {
  PauseProfiler();
}


class EnumerateOptimizedFunctionsVisitor: public OptimizedFunctionVisitor {
 public:
  EnumerateOptimizedFunctionsVisitor(Handle<SharedFunctionInfo>* sfis,
                                     Handle<Code>* code_objects,
                                     int* count)
      : sfis_(sfis), code_objects_(code_objects), count_(count) { }

  virtual void EnterContext(Context* context) {}
  virtual void LeaveContext(Context* context) {}

  virtual void VisitFunction(JSFunction* function) {
    SharedFunctionInfo* sfi = SharedFunctionInfo::cast(function->shared());
    Object* maybe_script = sfi->script();
    if (maybe_script->IsScript()
        && !Script::cast(maybe_script)->HasValidSource()) return;
    if (sfis_ != NULL) {
      sfis_[*count_] = Handle<SharedFunctionInfo>(sfi);
    }
    if (code_objects_ != NULL) {
      ASSERT(function->code()->kind() == Code::OPTIMIZED_FUNCTION);
      code_objects_[*count_] = Handle<Code>(function->code());
    }
    *count_ = *count_ + 1;
  }

 private:
  Handle<SharedFunctionInfo>* sfis_;
  Handle<Code>* code_objects_;
  int* count_;
};


static int EnumerateCompiledFunctions(Heap* heap,
                                      Handle<SharedFunctionInfo>* sfis,
                                      Handle<Code>* code_objects) {
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  int compiled_funcs_count = 0;

  // Iterate the heap to find shared function info objects and record
  // the unoptimized code for them.
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsSharedFunctionInfo()) continue;
    SharedFunctionInfo* sfi = SharedFunctionInfo::cast(obj);
    if (sfi->is_compiled()
        && (!sfi->script()->IsScript()
            || Script::cast(sfi->script())->HasValidSource())) {
      if (sfis != NULL) {
        sfis[compiled_funcs_count] = Handle<SharedFunctionInfo>(sfi);
      }
      if (code_objects != NULL) {
        code_objects[compiled_funcs_count] = Handle<Code>(sfi->code());
      }
      ++compiled_funcs_count;
    }
  }

  // Iterate all optimized functions in all contexts.
  EnumerateOptimizedFunctionsVisitor visitor(sfis,
                                             code_objects,
                                             &compiled_funcs_count);
  Deoptimizer::VisitAllOptimizedFunctions(heap->isolate(), &visitor);

  return compiled_funcs_count;
}


void Logger::LogCodeObject(Object* object) {
  Code* code_object = Code::cast(object);
  LogEventsAndTags tag = Logger::STUB_TAG;
  const char* description = "Unknown code from the snapshot";
  switch (code_object->kind()) {
    case Code::FUNCTION:
    case Code::OPTIMIZED_FUNCTION:
      return;  // We log this later using LogCompiledFunctions.
    case Code::UNARY_OP_IC:   // fall through
    case Code::BINARY_OP_IC:   // fall through
    case Code::COMPARE_IC:  // fall through
    case Code::COMPARE_NIL_IC:   // fall through
    case Code::TO_BOOLEAN_IC:  // fall through
    case Code::STUB:
      description =
          CodeStub::MajorName(CodeStub::GetMajorKey(code_object), true);
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
    case Code::KEYED_CALL_IC:
      description = "A keyed call IC from the snapshot";
      tag = Logger::KEYED_CALL_IC_TAG;
      break;
  }
  PROFILE(isolate_, CodeCreateEvent(tag, code_object, description));
}


void Logger::LogCodeInfo() {
  if (!log_->IsEnabled() || !FLAG_ll_prof) return;
#if V8_TARGET_ARCH_IA32
  const char arch[] = "ia32";
#elif V8_TARGET_ARCH_X64
  const char arch[] = "x64";
#elif V8_TARGET_ARCH_ARM
  const char arch[] = "arm";
#elif V8_TARGET_ARCH_MIPS
  const char arch[] = "mips";
#else
  const char arch[] = "unknown";
#endif
  LowLevelLogWriteBytes(arch, sizeof(arch));
}


void Logger::RegisterSnapshotCodeName(Code* code,
                                      const char* name,
                                      int name_size) {
  ASSERT(Serializer::enabled());
  if (address_to_name_map_ == NULL) {
    address_to_name_map_ = new NameMap;
  }
  address_to_name_map_->Insert(code->address(), name, name_size);
}


void Logger::LowLevelCodeCreateEvent(Code* code,
                                     const char* name,
                                     int name_size) {
  if (log_->ll_output_handle_ == NULL) return;
  LowLevelCodeCreateStruct event;
  event.name_size = name_size;
  event.code_address = code->instruction_start();
  ASSERT(event.code_address == code->address() + Code::kHeaderSize);
  event.code_size = code->instruction_size();
  LowLevelLogWriteStruct(event);
  LowLevelLogWriteBytes(name, name_size);
  LowLevelLogWriteBytes(
      reinterpret_cast<const char*>(code->instruction_start()),
      code->instruction_size());
}


void Logger::LowLevelCodeMoveEvent(Address from, Address to) {
  if (log_->ll_output_handle_ == NULL) return;
  LowLevelCodeMoveStruct event;
  event.from_address = from + Code::kHeaderSize;
  event.to_address = to + Code::kHeaderSize;
  LowLevelLogWriteStruct(event);
}


void Logger::LowLevelCodeDeleteEvent(Address from) {
  if (log_->ll_output_handle_ == NULL) return;
  LowLevelCodeDeleteStruct event;
  event.address = from + Code::kHeaderSize;
  LowLevelLogWriteStruct(event);
}


void Logger::LowLevelSnapshotPositionEvent(Address addr, int pos) {
  if (log_->ll_output_handle_ == NULL) return;
  LowLevelSnapshotPositionStruct event;
  event.address = addr + Code::kHeaderSize;
  event.position = pos;
  LowLevelLogWriteStruct(event);
}


void Logger::LowLevelLogWriteBytes(const char* bytes, int size) {
  size_t rv = fwrite(bytes, 1, size, log_->ll_output_handle_);
  ASSERT(static_cast<size_t>(size) == rv);
  USE(rv);
}


void Logger::LogCodeObjects() {
  Heap* heap = isolate_->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                          "Logger::LogCodeObjects");
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (obj->IsCode()) LogCodeObject(obj);
  }
}


void Logger::LogExistingFunction(Handle<SharedFunctionInfo> shared,
                                 Handle<Code> code) {
  Handle<String> func_name(shared->DebugName());
  if (shared->script()->IsScript()) {
    Handle<Script> script(Script::cast(shared->script()));
    if (script->name()->IsString()) {
      Handle<String> script_name(String::cast(script->name()));
      int line_num = GetScriptLineNumber(script, shared->start_position());
      if (line_num > 0) {
        PROFILE(isolate_,
                CodeCreateEvent(
                    Logger::ToNativeByScript(Logger::LAZY_COMPILE_TAG, *script),
                    *code, *shared, NULL,
                    *script_name, line_num + 1));
      } else {
        // Can't distinguish eval and script here, so always use Script.
        PROFILE(isolate_,
                CodeCreateEvent(
                    Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
                    *code, *shared, NULL, *script_name));
      }
    } else {
      PROFILE(isolate_,
              CodeCreateEvent(
                  Logger::ToNativeByScript(Logger::LAZY_COMPILE_TAG, *script),
                  *code, *shared, NULL, *func_name));
    }
  } else if (shared->IsApiFunction()) {
    // API function.
    FunctionTemplateInfo* fun_data = shared->get_api_func_data();
    Object* raw_call_data = fun_data->call_code();
    if (!raw_call_data->IsUndefined()) {
      CallHandlerInfo* call_data = CallHandlerInfo::cast(raw_call_data);
      Object* callback_obj = call_data->callback();
      Address entry_point = v8::ToCData<Address>(callback_obj);
      PROFILE(isolate_, CallbackEvent(*func_name, entry_point));
    }
  } else {
    PROFILE(isolate_,
            CodeCreateEvent(
                Logger::LAZY_COMPILE_TAG, *code, *shared, NULL, *func_name));
  }
}


void Logger::LogCompiledFunctions() {
  Heap* heap = isolate_->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                          "Logger::LogCompiledFunctions");
  HandleScope scope(isolate_);
  const int compiled_funcs_count = EnumerateCompiledFunctions(heap, NULL, NULL);
  ScopedVector< Handle<SharedFunctionInfo> > sfis(compiled_funcs_count);
  ScopedVector< Handle<Code> > code_objects(compiled_funcs_count);
  EnumerateCompiledFunctions(heap, sfis.start(), code_objects.start());

  // During iteration, there can be heap allocation due to
  // GetScriptLineNumber call.
  for (int i = 0; i < compiled_funcs_count; ++i) {
    if (*code_objects[i] == Isolate::Current()->builtins()->builtin(
        Builtins::kLazyCompile))
      continue;
    LogExistingFunction(sfis[i], code_objects[i]);
  }
}


void Logger::LogAccessorCallbacks() {
  Heap* heap = isolate_->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                          "Logger::LogAccessorCallbacks");
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    if (!obj->IsExecutableAccessorInfo()) continue;
    ExecutableAccessorInfo* ai = ExecutableAccessorInfo::cast(obj);
    if (!ai->name()->IsName()) continue;
    Address getter_entry = v8::ToCData<Address>(ai->getter());
    Name* name = Name::cast(ai->name());
    if (getter_entry != 0) {
      PROFILE(isolate_, GetterCallbackEvent(name, getter_entry));
    }
    Address setter_entry = v8::ToCData<Address>(ai->setter());
    if (setter_entry != 0) {
      PROFILE(isolate_, SetterCallbackEvent(name, setter_entry));
    }
  }
}


bool Logger::SetUp(Isolate* isolate) {
  // Tests and EnsureInitialize() can call this twice in a row. It's harmless.
  if (is_initialized_) return true;
  is_initialized_ = true;

  // --ll-prof implies --log-code and --log-snapshot-positions.
  if (FLAG_ll_prof) {
    FLAG_log_snapshot_positions = true;
  }

  // --prof_lazy controls --log-code, implies --noprof_auto.
  if (FLAG_prof_lazy) {
    FLAG_log_code = false;
    FLAG_prof_auto = false;
  }

  log_->Initialize();

  if (FLAG_ll_prof) LogCodeInfo();

  ticker_ = new Ticker(isolate, kSamplingIntervalMs);

  if (Log::InitLogAtStart()) {
    logging_nesting_ = 1;
  }

  if (FLAG_prof) {
    profiler_ = new Profiler(isolate);
    if (!FLAG_prof_auto) {
      profiler_->pause();
    } else {
      logging_nesting_ = 1;
    }
    if (!FLAG_prof_lazy) {
      profiler_->Engage();
    }
  }

  if (FLAG_log_internal_timer_events || FLAG_prof) epoch_ = OS::Ticks();

  return true;
}


void Logger::SetCodeEventHandler(uint32_t options,
                                 JitCodeEventHandler event_handler) {
  code_event_handler_ = event_handler;

  if (code_event_handler_ != NULL && (options & kJitCodeEventEnumExisting)) {
    HandleScope scope(Isolate::Current());
    LogCodeObjects();
    LogCompiledFunctions();
  }
}


Sampler* Logger::sampler() {
  return ticker_;
}


FILE* Logger::TearDown() {
  if (!is_initialized_) return NULL;
  is_initialized_ = false;

  // Stop the profiler before closing the file.
  if (profiler_ != NULL) {
    profiler_->Disengage();
    delete profiler_;
    profiler_ = NULL;
  }

  delete ticker_;
  ticker_ = NULL;

  return log_->Close();
}

} }  // namespace v8::internal
