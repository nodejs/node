// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/log.h"

#include <cstdarg>
#include <sstream>

#include "src/v8.h"

#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/cpu-profiler.h"
#include "src/deoptimizer.h"
#include "src/global-handles.h"
#include "src/log-inl.h"
#include "src/log-utils.h"
#include "src/macro-assembler.h"
#include "src/perf-jit.h"
#include "src/runtime-profiler.h"
#include "src/string-stream.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


#define DECLARE_EVENT(ignore1, name) name,
static const char* const kLogEventsNames[Logger::NUMBER_OF_LOG_EVENTS] = {
  LOG_EVENTS_AND_TAGS_LIST(DECLARE_EVENT)
};
#undef DECLARE_EVENT


#define CALL_LISTENERS(Call)                    \
for (int i = 0; i < listeners_.length(); ++i) { \
  listeners_[i]->Call;                          \
}

#define PROFILER_LOG(Call)                                \
  do {                                                    \
    CpuProfiler* cpu_profiler = isolate_->cpu_profiler(); \
    if (cpu_profiler->is_profiling()) {                   \
      cpu_profiler->Call;                                 \
    }                                                     \
  } while (false);

// ComputeMarker must only be used when SharedFunctionInfo is known.
static const char* ComputeMarker(Code* code) {
  switch (code->kind()) {
    case Code::FUNCTION: return code->optimizable() ? "~" : "";
    case Code::OPTIMIZED_FUNCTION: return "*";
    default: return "";
  }
}


class CodeEventLogger::NameBuffer {
 public:
  NameBuffer() { Reset(); }

  void Reset() {
    utf8_pos_ = 0;
  }

  void Init(Logger::LogEventsAndTags tag) {
    Reset();
    AppendBytes(kLogEventsNames[tag]);
    AppendByte(':');
  }

  void AppendName(Name* name) {
    if (name->IsString()) {
      AppendString(String::cast(name));
    } else {
      Symbol* symbol = Symbol::cast(name);
      AppendBytes("symbol(");
      if (!symbol->name()->IsUndefined()) {
        AppendBytes("\"");
        AppendString(String::cast(symbol->name()));
        AppendBytes("\" ");
      }
      AppendBytes("hash ");
      AppendHex(symbol->Hash());
      AppendByte(')');
    }
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
    MemCopy(utf8_buffer_ + utf8_pos_, bytes, size);
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
    Vector<char> buffer(utf8_buffer_ + utf8_pos_,
                        kUtf8BufferSize - utf8_pos_);
    int size = SNPrintF(buffer, "%d", n);
    if (size > 0 && utf8_pos_ + size <= kUtf8BufferSize) {
      utf8_pos_ += size;
    }
  }

  void AppendHex(uint32_t n) {
    Vector<char> buffer(utf8_buffer_ + utf8_pos_,
                        kUtf8BufferSize - utf8_pos_);
    int size = SNPrintF(buffer, "%x", n);
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


CodeEventLogger::CodeEventLogger() : name_buffer_(new NameBuffer) { }

CodeEventLogger::~CodeEventLogger() { delete name_buffer_; }


void CodeEventLogger::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                      Code* code,
                                      const char* comment) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(comment);
  LogRecordedBuffer(code, NULL, name_buffer_->get(), name_buffer_->size());
}


void CodeEventLogger::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                      Code* code,
                                      Name* name) {
  name_buffer_->Init(tag);
  name_buffer_->AppendName(name);
  LogRecordedBuffer(code, NULL, name_buffer_->get(), name_buffer_->size());
}


void CodeEventLogger::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                      Code* code,
                                      SharedFunctionInfo* shared,
                                      CompilationInfo* info,
                                      Name* name) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(ComputeMarker(code));
  name_buffer_->AppendName(name);
  LogRecordedBuffer(code, shared, name_buffer_->get(), name_buffer_->size());
}


void CodeEventLogger::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                      Code* code,
                                      SharedFunctionInfo* shared,
                                      CompilationInfo* info,
                                      Name* source, int line, int column) {
  name_buffer_->Init(tag);
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
  LogRecordedBuffer(code, shared, name_buffer_->get(), name_buffer_->size());
}


void CodeEventLogger::CodeCreateEvent(Logger::LogEventsAndTags tag,
                                      Code* code,
                                      int args_count) {
  name_buffer_->Init(tag);
  name_buffer_->AppendInt(args_count);
  LogRecordedBuffer(code, NULL, name_buffer_->get(), name_buffer_->size());
}


void CodeEventLogger::RegExpCodeCreateEvent(Code* code, String* source) {
  name_buffer_->Init(Logger::REG_EXP_TAG);
  name_buffer_->AppendString(source);
  LogRecordedBuffer(code, NULL, name_buffer_->get(), name_buffer_->size());
}


// Linux perf tool logging support
class PerfBasicLogger : public CodeEventLogger {
 public:
  PerfBasicLogger();
  virtual ~PerfBasicLogger();

  virtual void CodeMoveEvent(Address from, Address to) { }
  virtual void CodeDisableOptEvent(Code* code, SharedFunctionInfo* shared) { }
  virtual void CodeDeleteEvent(Address from) { }

 private:
  virtual void LogRecordedBuffer(Code* code,
                                 SharedFunctionInfo* shared,
                                 const char* name,
                                 int length);

  // Extension added to V8 log file name to get the low-level log name.
  static const char kFilenameFormatString[];
  static const int kFilenameBufferPadding;

  // File buffer size of the low-level log. We don't use the default to
  // minimize the associated overhead.
  static const int kLogBufferSize = 2 * MB;

  FILE* perf_output_handle_;
};

const char PerfBasicLogger::kFilenameFormatString[] = "/tmp/perf-%d.map";
// Extra space for the PID in the filename
const int PerfBasicLogger::kFilenameBufferPadding = 16;

PerfBasicLogger::PerfBasicLogger()
    : perf_output_handle_(NULL) {
  // Open the perf JIT dump file.
  int bufferSize = sizeof(kFilenameFormatString) + kFilenameBufferPadding;
  ScopedVector<char> perf_dump_name(bufferSize);
  int size = SNPrintF(
      perf_dump_name,
      kFilenameFormatString,
      base::OS::GetCurrentProcessId());
  CHECK_NE(size, -1);
  perf_output_handle_ =
      base::OS::FOpen(perf_dump_name.start(), base::OS::LogFileOpenMode);
  CHECK_NOT_NULL(perf_output_handle_);
  setvbuf(perf_output_handle_, NULL, _IOFBF, kLogBufferSize);
}


PerfBasicLogger::~PerfBasicLogger() {
  fclose(perf_output_handle_);
  perf_output_handle_ = NULL;
}


void PerfBasicLogger::LogRecordedBuffer(Code* code,
                                       SharedFunctionInfo*,
                                       const char* name,
                                       int length) {
  DCHECK(code->instruction_start() == code->address() + Code::kHeaderSize);

  base::OS::FPrint(perf_output_handle_, "%llx %x %.*s\n",
                   reinterpret_cast<uint64_t>(code->instruction_start()),
                   code->instruction_size(), length, name);
}


// Low-level logging support.
#define LL_LOG(Call) if (ll_logger_) ll_logger_->Call;

class LowLevelLogger : public CodeEventLogger {
 public:
  explicit LowLevelLogger(const char* file_name);
  virtual ~LowLevelLogger();

  virtual void CodeMoveEvent(Address from, Address to);
  virtual void CodeDisableOptEvent(Code* code, SharedFunctionInfo* shared) { }
  virtual void CodeDeleteEvent(Address from);
  virtual void SnapshotPositionEvent(Address addr, int pos);
  virtual void CodeMovingGCEvent();

 private:
  virtual void LogRecordedBuffer(Code* code,
                                 SharedFunctionInfo* shared,
                                 const char* name,
                                 int length);

  // Low-level profiling event structures.
  struct CodeCreateStruct {
    static const char kTag = 'C';

    int32_t name_size;
    Address code_address;
    int32_t code_size;
  };


  struct CodeMoveStruct {
    static const char kTag = 'M';

    Address from_address;
    Address to_address;
  };


  struct CodeDeleteStruct {
    static const char kTag = 'D';

    Address address;
  };


  struct SnapshotPositionStruct {
    static const char kTag = 'P';

    Address address;
    int32_t position;
  };


  static const char kCodeMovingGCTag = 'G';


  // Extension added to V8 log file name to get the low-level log name.
  static const char kLogExt[];

  // File buffer size of the low-level log. We don't use the default to
  // minimize the associated overhead.
  static const int kLogBufferSize = 2 * MB;

  void LogCodeInfo();
  void LogWriteBytes(const char* bytes, int size);

  template <typename T>
  void LogWriteStruct(const T& s) {
    char tag = T::kTag;
    LogWriteBytes(reinterpret_cast<const char*>(&tag), sizeof(tag));
    LogWriteBytes(reinterpret_cast<const char*>(&s), sizeof(s));
  }

  FILE* ll_output_handle_;
};

const char LowLevelLogger::kLogExt[] = ".ll";

LowLevelLogger::LowLevelLogger(const char* name)
    : ll_output_handle_(NULL) {
  // Open the low-level log file.
  size_t len = strlen(name);
  ScopedVector<char> ll_name(static_cast<int>(len + sizeof(kLogExt)));
  MemCopy(ll_name.start(), name, len);
  MemCopy(ll_name.start() + len, kLogExt, sizeof(kLogExt));
  ll_output_handle_ =
      base::OS::FOpen(ll_name.start(), base::OS::LogFileOpenMode);
  setvbuf(ll_output_handle_, NULL, _IOFBF, kLogBufferSize);

  LogCodeInfo();
}


LowLevelLogger::~LowLevelLogger() {
  fclose(ll_output_handle_);
  ll_output_handle_ = NULL;
}


void LowLevelLogger::LogCodeInfo() {
#if V8_TARGET_ARCH_IA32
  const char arch[] = "ia32";
#elif V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_64_BIT
  const char arch[] = "x64";
#elif V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
  const char arch[] = "x32";
#elif V8_TARGET_ARCH_ARM
  const char arch[] = "arm";
#elif V8_TARGET_ARCH_PPC
  const char arch[] = "ppc";
#elif V8_TARGET_ARCH_MIPS
  const char arch[] = "mips";
#elif V8_TARGET_ARCH_X87
  const char arch[] = "x87";
#elif V8_TARGET_ARCH_ARM64
  const char arch[] = "arm64";
#else
  const char arch[] = "unknown";
#endif
  LogWriteBytes(arch, sizeof(arch));
}


void LowLevelLogger::LogRecordedBuffer(Code* code,
                                       SharedFunctionInfo*,
                                       const char* name,
                                       int length) {
  CodeCreateStruct event;
  event.name_size = length;
  event.code_address = code->instruction_start();
  DCHECK(event.code_address == code->address() + Code::kHeaderSize);
  event.code_size = code->instruction_size();
  LogWriteStruct(event);
  LogWriteBytes(name, length);
  LogWriteBytes(
      reinterpret_cast<const char*>(code->instruction_start()),
      code->instruction_size());
}


void LowLevelLogger::CodeMoveEvent(Address from, Address to) {
  CodeMoveStruct event;
  event.from_address = from + Code::kHeaderSize;
  event.to_address = to + Code::kHeaderSize;
  LogWriteStruct(event);
}


void LowLevelLogger::CodeDeleteEvent(Address from) {
  CodeDeleteStruct event;
  event.address = from + Code::kHeaderSize;
  LogWriteStruct(event);
}


void LowLevelLogger::SnapshotPositionEvent(Address addr, int pos) {
  SnapshotPositionStruct event;
  event.address = addr + Code::kHeaderSize;
  event.position = pos;
  LogWriteStruct(event);
}


void LowLevelLogger::LogWriteBytes(const char* bytes, int size) {
  size_t rv = fwrite(bytes, 1, size, ll_output_handle_);
  DCHECK(static_cast<size_t>(size) == rv);
  USE(rv);
}


void LowLevelLogger::CodeMovingGCEvent() {
  const char tag = kCodeMovingGCTag;

  LogWriteBytes(&tag, sizeof(tag));
}


#define JIT_LOG(Call) if (jit_logger_) jit_logger_->Call;


class JitLogger : public CodeEventLogger {
 public:
  explicit JitLogger(JitCodeEventHandler code_event_handler);

  virtual void CodeMoveEvent(Address from, Address to);
  virtual void CodeDisableOptEvent(Code* code, SharedFunctionInfo* shared) { }
  virtual void CodeDeleteEvent(Address from);
  virtual void AddCodeLinePosInfoEvent(
      void* jit_handler_data,
      int pc_offset,
      int position,
      JitCodeEvent::PositionType position_type);

  void* StartCodePosInfoEvent();
  void EndCodePosInfoEvent(Code* code, void* jit_handler_data);

 private:
  virtual void LogRecordedBuffer(Code* code,
                                 SharedFunctionInfo* shared,
                                 const char* name,
                                 int length);

  JitCodeEventHandler code_event_handler_;
};


JitLogger::JitLogger(JitCodeEventHandler code_event_handler)
    : code_event_handler_(code_event_handler) {
}


void JitLogger::LogRecordedBuffer(Code* code,
                                  SharedFunctionInfo* shared,
                                  const char* name,
                                  int length) {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADDED;
  event.code_start = code->instruction_start();
  event.code_len = code->instruction_size();
  Handle<SharedFunctionInfo> shared_function_handle;
  if (shared && shared->script()->IsScript()) {
    shared_function_handle = Handle<SharedFunctionInfo>(shared);
  }
  event.script = ToApiHandle<v8::UnboundScript>(shared_function_handle);
  event.name.str = name;
  event.name.len = length;
  code_event_handler_(&event);
}


void JitLogger::CodeMoveEvent(Address from, Address to) {
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


void JitLogger::CodeDeleteEvent(Address from) {
  Code* from_code = Code::cast(HeapObject::FromAddress(from));

  JitCodeEvent event;
  event.type = JitCodeEvent::CODE_REMOVED;
  event.code_start = from_code->instruction_start();
  event.code_len = from_code->instruction_size();

  code_event_handler_(&event);
}

void JitLogger::AddCodeLinePosInfoEvent(
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


void* JitLogger::StartCodePosInfoEvent() {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_START_LINE_INFO_RECORDING;

  code_event_handler_(&event);
  return event.user_data;
}


void JitLogger::EndCodePosInfoEvent(Code* code, void* jit_handler_data) {
  JitCodeEvent event;
  memset(&event, 0, sizeof(event));
  event.type = JitCodeEvent::CODE_END_LINE_INFO_RECORDING;
  event.code_start = code->instruction_start();
  event.user_data = jit_handler_data;

  code_event_handler_(&event);
}


// The Profiler samples pc and sp values for the main thread.
// Each sample is appended to a circular buffer.
// An independent thread removes data and writes it to the log.
// This design minimizes the time spent in the sampler.
//
class Profiler: public base::Thread {
 public:
  explicit Profiler(Isolate* isolate);
  void Engage();
  void Disengage();

  // Inserts collected profiling data into buffer.
  void Insert(TickSample* sample) {
    if (paused_)
      return;

    if (Succ(head_) == static_cast<int>(base::NoBarrier_Load(&tail_))) {
      overflow_ = true;
    } else {
      buffer_[head_] = *sample;
      head_ = Succ(head_);
      buffer_semaphore_.Signal();  // Tell we have an element.
    }
  }

  virtual void Run();

  // Pause and Resume TickSample data collection.
  void pause() { paused_ = true; }
  void resume() { paused_ = false; }

 private:
  // Waits for a signal and removes profiling data.
  bool Remove(TickSample* sample) {
    buffer_semaphore_.Wait();  // Wait for an element.
    *sample = buffer_[base::NoBarrier_Load(&tail_)];
    bool result = overflow_;
    base::NoBarrier_Store(&tail_, static_cast<base::Atomic32>(
                                      Succ(base::NoBarrier_Load(&tail_))));
    overflow_ = false;
    return result;
  }

  // Returns the next index in the cyclic buffer.
  int Succ(int index) { return (index + 1) % kBufferSize; }

  Isolate* isolate_;
  // Cyclic buffer for communicating profiling samples
  // between the signal handler and the worker thread.
  static const int kBufferSize = 128;
  TickSample buffer_[kBufferSize];  // Buffer storage.
  int head_;  // Index to the buffer head.
  base::Atomic32 tail_;             // Index to the buffer tail.
  bool overflow_;  // Tell whether a buffer overflow has occurred.
  // Sempahore used for buffer synchronization.
  base::Semaphore buffer_semaphore_;

  // Tells whether profiler is engaged, that is, processing thread is stated.
  bool engaged_;

  // Tells whether worker thread should continue running.
  base::Atomic32 running_;

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
    DCHECK(profiler_ == NULL);
    profiler_ = profiler;
    IncreaseProfilingDepth();
    if (!IsActive()) Start();
  }

  void ClearProfiler() {
    profiler_ = NULL;
    if (IsActive()) Stop();
    DecreaseProfilingDepth();
  }

 private:
  Profiler* profiler_;
};


//
// Profiler implementation.
//
Profiler::Profiler(Isolate* isolate)
    : base::Thread(Options("v8:Profiler")),
      isolate_(isolate),
      head_(0),
      overflow_(false),
      buffer_semaphore_(0),
      engaged_(false),
      paused_(false) {
  base::NoBarrier_Store(&tail_, 0);
  base::NoBarrier_Store(&running_, 0);
}


void Profiler::Engage() {
  if (engaged_) return;
  engaged_ = true;

  std::vector<base::OS::SharedLibraryAddress> addresses =
      base::OS::GetSharedLibraryAddresses();
  for (size_t i = 0; i < addresses.size(); ++i) {
    LOG(isolate_, SharedLibraryEvent(
        addresses[i].library_path, addresses[i].start, addresses[i].end));
  }

  // Start thread processing the profiler buffer.
  base::NoBarrier_Store(&running_, 1);
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
  base::NoBarrier_Store(&running_, 0);
  TickSample sample;
  // Reset 'paused_' flag, otherwise semaphore may not be signalled.
  resume();
  Insert(&sample);
  Join();

  LOG(isolate_, UncheckedStringEvent("profiler", "end"));
}


void Profiler::Run() {
  TickSample sample;
  bool overflow = Remove(&sample);
  while (base::NoBarrier_Load(&running_)) {
    LOG(isolate_, TickEvent(&sample, overflow));
    overflow = Remove(&sample);
  }
}


//
// Logger class implementation.
//

Logger::Logger(Isolate* isolate)
  : isolate_(isolate),
    ticker_(NULL),
    profiler_(NULL),
    log_events_(NULL),
    is_logging_(false),
    log_(new Log(this)),
    perf_basic_logger_(NULL),
    perf_jit_logger_(NULL),
    ll_logger_(NULL),
    jit_logger_(NULL),
    listeners_(5),
    is_initialized_(false) {
}


Logger::~Logger() {
  delete log_;
}


void Logger::addCodeEventListener(CodeEventListener* listener) {
  DCHECK(!hasCodeEventListener(listener));
  listeners_.Add(listener);
}


void Logger::removeCodeEventListener(CodeEventListener* listener) {
  DCHECK(hasCodeEventListener(listener));
  listeners_.RemoveElement(listener);
}


bool Logger::hasCodeEventListener(CodeEventListener* listener) {
  return listeners_.Contains(listener);
}


void Logger::ProfilerBeginEvent() {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("profiler,\"begin\",%d", kSamplingIntervalMs);
  msg.WriteToLogFile();
}


void Logger::StringEvent(const char* name, const char* value) {
  if (FLAG_log) UncheckedStringEvent(name, value);
}


void Logger::UncheckedStringEvent(const char* name, const char* value) {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,\"%s\"", name, value);
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
  Log::MessageBuilder msg(log_);
  msg.Append("%s,%d", name, value);
  msg.WriteToLogFile();
}


void Logger::UncheckedIntPtrTEvent(const char* name, intptr_t value) {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,%" V8_PTR_PREFIX "d", name, value);
  msg.WriteToLogFile();
}


void Logger::HandleEvent(const char* name, Object** location) {
  if (!log_->IsEnabled() || !FLAG_log_handles) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,0x%" V8PRIxPTR, name, location);
  msg.WriteToLogFile();
}


// ApiEvent is private so all the calls come from the Logger class.  It is the
// caller's responsibility to ensure that log is enabled and that
// FLAG_log_api is true.
void Logger::ApiEvent(const char* format, ...) {
  DCHECK(log_->IsEnabled() && FLAG_log_api);
  Log::MessageBuilder msg(log_);
  va_list ap;
  va_start(ap, format);
  msg.AppendVA(format, ap);
  va_end(ap);
  msg.WriteToLogFile();
}


void Logger::ApiSecurityCheck() {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,check-security");
}


void Logger::SharedLibraryEvent(const std::string& library_path,
                                uintptr_t start,
                                uintptr_t end) {
  if (!log_->IsEnabled() || !FLAG_prof_cpp) return;
  Log::MessageBuilder msg(log_);
  msg.Append("shared-library,\"%s\",0x%08" V8PRIxPTR ",0x%08" V8PRIxPTR,
             library_path.c_str(), start, end);
  msg.WriteToLogFile();
}


void Logger::CodeDeoptEvent(Code* code, Address pc, int fp_to_sp_delta) {
  PROFILER_LOG(CodeDeoptEvent(code, pc, fp_to_sp_delta));
  if (!log_->IsEnabled() || !FLAG_log_internal_timer_events) return;
  Log::MessageBuilder msg(log_);
  int since_epoch = static_cast<int>(timer_.Elapsed().InMicroseconds());
  msg.Append("code-deopt,%ld,%d", since_epoch, code->CodeSize());
  msg.WriteToLogFile();
}


void Logger::CurrentTimeEvent() {
  if (!log_->IsEnabled()) return;
  DCHECK(FLAG_log_internal_timer_events);
  Log::MessageBuilder msg(log_);
  int since_epoch = static_cast<int>(timer_.Elapsed().InMicroseconds());
  msg.Append("current-time,%ld", since_epoch);
  msg.WriteToLogFile();
}


void Logger::TimerEvent(Logger::StartEnd se, const char* name) {
  if (!log_->IsEnabled()) return;
  DCHECK(FLAG_log_internal_timer_events);
  Log::MessageBuilder msg(log_);
  int since_epoch = static_cast<int>(timer_.Elapsed().InMicroseconds());
  const char* format = (se == START) ? "timer-event-start,\"%s\",%ld"
                                     : "timer-event-end,\"%s\",%ld";
  msg.Append(format, name, since_epoch);
  msg.WriteToLogFile();
}


void Logger::EnterExternal(Isolate* isolate) {
  LOG(isolate, TimerEvent(START, TimerEventExternal::name()));
  DCHECK(isolate->current_vm_state() == JS);
  isolate->set_current_vm_state(EXTERNAL);
}


void Logger::LeaveExternal(Isolate* isolate) {
  LOG(isolate, TimerEvent(END, TimerEventExternal::name()));
  DCHECK(isolate->current_vm_state() == EXTERNAL);
  isolate->set_current_vm_state(JS);
}


template <class TimerEvent>
void TimerEventScope<TimerEvent>::LogTimerEvent(Logger::StartEnd se) {
  Logger::CallEventLogger(isolate_, TimerEvent::name(), se,
                          TimerEvent::expose_to_api());
}


// Instantiate template methods.
#define V(TimerName, expose)                                           \
  template void TimerEventScope<TimerEvent##TimerName>::LogTimerEvent( \
      Logger::StartEnd se);
TIMER_EVENTS_LIST(V)
#undef V


void Logger::LogRegExpSource(Handle<JSRegExp> regexp) {
  // Prints "/" + re.source + "/" +
  //      (re.global?"g":"") + (re.ignorecase?"i":"") + (re.multiline?"m":"")
  Log::MessageBuilder msg(log_);

  Handle<Object> source = Object::GetProperty(
      isolate_, regexp, "source").ToHandleChecked();
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
  Handle<Object> global = Object::GetProperty(
      isolate_, regexp, "global").ToHandleChecked();
  if (global->IsTrue()) {
    msg.Append('g');
  }
  // ignorecase flag
  Handle<Object> ignorecase = Object::GetProperty(
      isolate_, regexp, "ignoreCase").ToHandleChecked();
  if (ignorecase->IsTrue()) {
    msg.Append('i');
  }
  // multiline flag
  Handle<Object> multiline = Object::GetProperty(
      isolate_, regexp, "multiline").ToHandleChecked();
  if (multiline->IsTrue()) {
    msg.Append('m');
  }

  msg.WriteToLogFile();
}


void Logger::RegExpCompileEvent(Handle<JSRegExp> regexp, bool in_cache) {
  if (!log_->IsEnabled() || !FLAG_log_regexp) return;
  Log::MessageBuilder msg(log_);
  msg.Append("regexp-compile,");
  LogRegExpSource(regexp);
  msg.Append(in_cache ? ",hit" : ",miss");
  msg.WriteToLogFile();
}


void Logger::ApiNamedPropertyAccess(const char* tag,
                                    JSObject* holder,
                                    Object* name) {
  DCHECK(name->IsName());
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = holder->class_name();
  SmartArrayPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  if (name->IsString()) {
    SmartArrayPointer<char> property_name =
        String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    ApiEvent("api,%s,\"%s\",\"%s\"", tag, class_name.get(),
             property_name.get());
  } else {
    Symbol* symbol = Symbol::cast(name);
    uint32_t hash = symbol->Hash();
    if (symbol->name()->IsUndefined()) {
      ApiEvent("api,%s,\"%s\",symbol(hash %x)", tag, class_name.get(), hash);
    } else {
      SmartArrayPointer<char> str = String::cast(symbol->name())->ToCString(
          DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
      ApiEvent("api,%s,\"%s\",symbol(\"%s\" hash %x)", tag, class_name.get(),
               str.get(), hash);
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
  ApiEvent("api,%s,\"%s\",%u", tag, class_name.get(), index);
}


void Logger::ApiObjectAccess(const char* tag, JSObject* object) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  String* class_name_obj = object->class_name();
  SmartArrayPointer<char> class_name =
      class_name_obj->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  ApiEvent("api,%s,\"%s\"", tag, class_name.get());
}


void Logger::ApiEntryCall(const char* name) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  ApiEvent("api,%s", name);
}


void Logger::NewEvent(const char* name, void* object, size_t size) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg.Append("new,%s,0x%" V8PRIxPTR ",%u", name, object,
             static_cast<unsigned int>(size));
  msg.WriteToLogFile();
}


void Logger::DeleteEvent(const char* name, void* object) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg.Append("delete,%s,0x%" V8PRIxPTR, name, object);
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
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,%s,-2,",
             kLogEventsNames[CODE_CREATION_EVENT],
             kLogEventsNames[CALLBACK_TAG]);
  msg.AppendAddress(entry_point);
  if (name->IsString()) {
    SmartArrayPointer<char> str =
        String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    msg.Append(",1,\"%s%s\"", prefix, str.get());
  } else {
    Symbol* symbol = Symbol::cast(name);
    if (symbol->name()->IsUndefined()) {
      msg.Append(",1,symbol(hash %x)", prefix, symbol->Hash());
    } else {
      SmartArrayPointer<char> str = String::cast(symbol->name())->ToCString(
          DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
      msg.Append(",1,symbol(\"%s\" hash %x)", prefix, str.get(),
                 symbol->Hash());
    }
  }
  msg.WriteToLogFile();
}


void Logger::CallbackEvent(Name* name, Address entry_point) {
  PROFILER_LOG(CallbackEvent(name, entry_point));
  CallbackEventInternal("", name, entry_point);
}


void Logger::GetterCallbackEvent(Name* name, Address entry_point) {
  PROFILER_LOG(GetterCallbackEvent(name, entry_point));
  CallbackEventInternal("get ", name, entry_point);
}


void Logger::SetterCallbackEvent(Name* name, Address entry_point) {
  PROFILER_LOG(SetterCallbackEvent(name, entry_point));
  CallbackEventInternal("set ", name, entry_point);
}


static void AppendCodeCreateHeader(Log::MessageBuilder* msg,
                                   Logger::LogEventsAndTags tag,
                                   Code* code) {
  DCHECK(msg);
  msg->Append("%s,%s,%d,",
              kLogEventsNames[Logger::CODE_CREATION_EVENT],
              kLogEventsNames[tag],
              code->kind());
  msg->AppendAddress(code->address());
  msg->Append(",%d,", code->ExecutableSize());
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             const char* comment) {
  PROFILER_LOG(CodeCreateEvent(tag, code, comment));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeCreateEvent(tag, code, comment));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(&msg, tag, code);
  msg.AppendDoubleQuotedString(comment);
  msg.WriteToLogFile();
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             Name* name) {
  PROFILER_LOG(CodeCreateEvent(tag, code, name));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeCreateEvent(tag, code, name));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(&msg, tag, code);
  if (name->IsString()) {
    msg.Append('"');
    msg.AppendDetailed(String::cast(name), false);
    msg.Append('"');
  } else {
    msg.AppendSymbolName(Symbol::cast(name));
  }
  msg.WriteToLogFile();
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             SharedFunctionInfo* shared,
                             CompilationInfo* info,
                             Name* name) {
  PROFILER_LOG(CodeCreateEvent(tag, code, shared, info, name));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeCreateEvent(tag, code, shared, info, name));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  if (code == isolate_->builtins()->builtin(Builtins::kCompileLazy)) return;

  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(&msg, tag, code);
  if (name->IsString()) {
    SmartArrayPointer<char> str =
        String::cast(name)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    msg.Append("\"%s\"", str.get());
  } else {
    msg.AppendSymbolName(Symbol::cast(name));
  }
  msg.Append(',');
  msg.AppendAddress(shared->address());
  msg.Append(",%s", ComputeMarker(code));
  msg.WriteToLogFile();
}


// Although, it is possible to extract source and line from
// the SharedFunctionInfo object, we left it to caller
// to leave logging functions free from heap allocations.
void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             SharedFunctionInfo* shared,
                             CompilationInfo* info,
                             Name* source, int line, int column) {
  PROFILER_LOG(CodeCreateEvent(tag, code, shared, info, source, line, column));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeCreateEvent(tag, code, shared, info, source, line,
                                 column));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(&msg, tag, code);
  SmartArrayPointer<char> name =
      shared->DebugName()->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("\"%s ", name.get());
  if (source->IsString()) {
    SmartArrayPointer<char> sourcestr =
       String::cast(source)->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    msg.Append("%s", sourcestr.get());
  } else {
    msg.AppendSymbolName(Symbol::cast(source));
  }
  msg.Append(":%d:%d\",", line, column);
  msg.AppendAddress(shared->address());
  msg.Append(",%s", ComputeMarker(code));
  msg.WriteToLogFile();
}


void Logger::CodeCreateEvent(LogEventsAndTags tag,
                             Code* code,
                             int args_count) {
  PROFILER_LOG(CodeCreateEvent(tag, code, args_count));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeCreateEvent(tag, code, args_count));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(&msg, tag, code);
  msg.Append("\"args_count: %d\"", args_count);
  msg.WriteToLogFile();
}


void Logger::CodeDisableOptEvent(Code* code,
                                 SharedFunctionInfo* shared) {
  PROFILER_LOG(CodeDisableOptEvent(code, shared));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeDisableOptEvent(code, shared));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,", kLogEventsNames[CODE_DISABLE_OPT_EVENT]);
  SmartArrayPointer<char> name =
      shared->DebugName()->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  msg.Append("\"%s\",", name.get());
  msg.Append("\"%s\"", GetBailoutReason(shared->disable_optimization_reason()));
  msg.WriteToLogFile();
}


void Logger::CodeMovingGCEvent() {
  PROFILER_LOG(CodeMovingGCEvent());

  if (!is_logging_code_events()) return;
  if (!log_->IsEnabled() || !FLAG_ll_prof) return;
  CALL_LISTENERS(CodeMovingGCEvent());
  base::OS::SignalCodeMovingGC();
}


void Logger::RegExpCodeCreateEvent(Code* code, String* source) {
  PROFILER_LOG(RegExpCodeCreateEvent(code, source));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(RegExpCodeCreateEvent(code, source));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(&msg, REG_EXP_TAG, code);
  msg.Append('"');
  msg.AppendDetailed(source, false);
  msg.Append('"');
  msg.WriteToLogFile();
}


void Logger::CodeMoveEvent(Address from, Address to) {
  PROFILER_LOG(CodeMoveEvent(from, to));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeMoveEvent(from, to));
  MoveEventInternal(CODE_MOVE_EVENT, from, to);
}


void Logger::CodeDeleteEvent(Address from) {
  PROFILER_LOG(CodeDeleteEvent(from));

  if (!is_logging_code_events()) return;
  CALL_LISTENERS(CodeDeleteEvent(from));

  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,", kLogEventsNames[CODE_DELETE_EVENT]);
  msg.AppendAddress(from);
  msg.WriteToLogFile();
}


void Logger::CodeLinePosInfoAddPositionEvent(void* jit_handler_data,
                                     int pc_offset,
                                     int position) {
  JIT_LOG(AddCodeLinePosInfoEvent(jit_handler_data,
                                  pc_offset,
                                  position,
                                  JitCodeEvent::POSITION));
}


void Logger::CodeLinePosInfoAddStatementPositionEvent(void* jit_handler_data,
                                                      int pc_offset,
                                                      int position) {
  JIT_LOG(AddCodeLinePosInfoEvent(jit_handler_data,
                                  pc_offset,
                                  position,
                                  JitCodeEvent::STATEMENT_POSITION));
}


void Logger::CodeStartLinePosInfoRecordEvent(PositionsRecorder* pos_recorder) {
  if (jit_logger_ != NULL) {
      pos_recorder->AttachJITHandlerData(jit_logger_->StartCodePosInfoEvent());
  }
}


void Logger::CodeEndLinePosInfoRecordEvent(Code* code,
                                           void* jit_handler_data) {
  JIT_LOG(EndCodePosInfoEvent(code, jit_handler_data));
}


void Logger::CodeNameEvent(Address addr, int pos, const char* code_name) {
  if (code_name == NULL) return;  // Not a code object.
  Log::MessageBuilder msg(log_);
  msg.Append("%s,%d,", kLogEventsNames[SNAPSHOT_CODE_NAME_EVENT], pos);
  msg.AppendDoubleQuotedString(code_name);
  msg.WriteToLogFile();
}


void Logger::SnapshotPositionEvent(Address addr, int pos) {
  if (!log_->IsEnabled()) return;
  LL_LOG(SnapshotPositionEvent(addr, pos));
  if (!FLAG_log_snapshot_positions) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,", kLogEventsNames[SNAPSHOT_POSITION_EVENT]);
  msg.AppendAddress(addr);
  msg.Append(",%d", pos);
  msg.WriteToLogFile();
}


void Logger::SharedFunctionInfoMoveEvent(Address from, Address to) {
  if (!is_logging_code_events()) return;
  MoveEventInternal(SHARED_FUNC_MOVE_EVENT, from, to);
}


void Logger::MoveEventInternal(LogEventsAndTags event,
                               Address from,
                               Address to) {
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,", kLogEventsNames[event]);
  msg.AppendAddress(from);
  msg.Append(',');
  msg.AppendAddress(to);
  msg.WriteToLogFile();
}


void Logger::ResourceEvent(const char* name, const char* tag) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,%s,", name, tag);

  uint32_t sec, usec;
  if (base::OS::GetUserTime(&sec, &usec) != -1) {
    msg.Append("%d,%d,", sec, usec);
  }
  msg.Append("%.0f", base::OS::TimeCurrentMillis());
  msg.WriteToLogFile();
}


void Logger::SuspectReadEvent(Name* name, Object* obj) {
  if (!log_->IsEnabled() || !FLAG_log_suspect) return;
  Log::MessageBuilder msg(log_);
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
    msg.AppendSymbolName(Symbol::cast(name));
  }
  msg.WriteToLogFile();
}


void Logger::HeapSampleBeginEvent(const char* space, const char* kind) {
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  Log::MessageBuilder msg(log_);
  // Using non-relative system time in order to be able to synchronize with
  // external memory profiling events (e.g. DOM memory size).
  msg.Append("heap-sample-begin,\"%s\",\"%s\",%.0f", space, kind,
             base::OS::TimeCurrentMillis());
  msg.WriteToLogFile();
}


void Logger::HeapSampleEndEvent(const char* space, const char* kind) {
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  Log::MessageBuilder msg(log_);
  msg.Append("heap-sample-end,\"%s\",\"%s\"", space, kind);
  msg.WriteToLogFile();
}


void Logger::HeapSampleItemEvent(const char* type, int number, int bytes) {
  if (!log_->IsEnabled() || !FLAG_log_gc) return;
  Log::MessageBuilder msg(log_);
  msg.Append("heap-sample-item,%s,%d,%d", type, number, bytes);
  msg.WriteToLogFile();
}


void Logger::DebugTag(const char* call_site_tag) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg.Append("debug-tag,%s", call_site_tag);
  msg.WriteToLogFile();
}


void Logger::DebugEvent(const char* event_type, Vector<uint16_t> parameter) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  StringBuilder s(parameter.length() + 1);
  for (int i = 0; i < parameter.length(); ++i) {
    s.AddCharacter(static_cast<char>(parameter[i]));
  }
  char* parameter_string = s.Finalize();
  Log::MessageBuilder msg(log_);
  msg.Append("debug-queue-event,%s,%15.3f,%s", event_type,
             base::OS::TimeCurrentMillis(), parameter_string);
  DeleteArray(parameter_string);
  msg.WriteToLogFile();
}


void Logger::TickEvent(TickSample* sample, bool overflow) {
  if (!log_->IsEnabled() || !FLAG_prof_cpp) return;
  Log::MessageBuilder msg(log_);
  msg.Append("%s,", kLogEventsNames[TICK_EVENT]);
  msg.AppendAddress(sample->pc);
  msg.Append(",%ld", static_cast<int>(timer_.Elapsed().InMicroseconds()));
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
  for (unsigned i = 0; i < sample->frames_count; ++i) {
    msg.Append(',');
    msg.AppendAddress(sample->stack[i]);
  }
  msg.WriteToLogFile();
}


void Logger::StopProfiler() {
  if (!log_->IsEnabled()) return;
  if (profiler_ != NULL) {
    profiler_->pause();
    is_logging_ = false;
  }
}


// This function can be called when Log's mutex is acquired,
// either from main or Profiler's thread.
void Logger::LogFailure() {
  StopProfiler();
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
      DCHECK(function->code()->kind() == Code::OPTIMIZED_FUNCTION);
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
    case Code::BINARY_OP_IC:
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
    case Code::REGEXP:
      description = "Regular expression code";
      tag = Logger::REG_EXP_TAG;
      break;
    case Code::BUILTIN:
      description = "A builtin from the snapshot";
      tag = Logger::BUILTIN_TAG;
      break;
    case Code::HANDLER:
      description = "An IC handler from the snapshot";
      tag = Logger::HANDLER_TAG;
      break;
    case Code::KEYED_LOAD_IC:
      description = "A keyed load IC from the snapshot";
      tag = Logger::KEYED_LOAD_IC_TAG;
      break;
    case Code::LOAD_IC:
      description = "A load IC from the snapshot";
      tag = Logger::LOAD_IC_TAG;
      break;
    case Code::CALL_IC:
      description = "A call IC from the snapshot";
      tag = Logger::CALL_IC_TAG;
      break;
    case Code::STORE_IC:
      description = "A store IC from the snapshot";
      tag = Logger::STORE_IC_TAG;
      break;
    case Code::KEYED_STORE_IC:
      description = "A keyed store IC from the snapshot";
      tag = Logger::KEYED_STORE_IC_TAG;
      break;
    case Code::NUMBER_OF_KINDS:
      break;
  }
  PROFILE(isolate_, CodeCreateEvent(tag, code_object, description));
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
    int line_num = Script::GetLineNumber(script, shared->start_position()) + 1;
    int column_num =
        Script::GetColumnNumber(script, shared->start_position()) + 1;
    if (script->name()->IsString()) {
      Handle<String> script_name(String::cast(script->name()));
      if (line_num > 0) {
        PROFILE(isolate_,
                CodeCreateEvent(
                    Logger::ToNativeByScript(Logger::LAZY_COMPILE_TAG, *script),
                    *code, *shared, NULL,
                    *script_name, line_num, column_num));
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
                  *code, *shared, NULL,
                  isolate_->heap()->empty_string(), line_num, column_num));
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
    if (code_objects[i].is_identical_to(isolate_->builtins()->CompileLazy()))
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


static void AddIsolateIdIfNeeded(std::ostream& os,  // NOLINT
                                 Isolate* isolate) {
  if (FLAG_logfile_per_isolate) os << "isolate-" << isolate << "-";
}


static void PrepareLogFileName(std::ostream& os,  // NOLINT
                               Isolate* isolate, const char* file_name) {
  int dir_separator_count = 0;
  for (const char* p = file_name; *p; p++) {
    if (base::OS::isDirectorySeparator(*p)) dir_separator_count++;
  }

  for (const char* p = file_name; *p; p++) {
    if (dir_separator_count == 0) {
      AddIsolateIdIfNeeded(os, isolate);
      dir_separator_count--;
    }
    if (*p == '%') {
      p++;
      switch (*p) {
        case '\0':
          // If there's a % at the end of the string we back up
          // one character so we can escape the loop properly.
          p--;
          break;
        case 'p':
          os << base::OS::GetCurrentProcessId();
          break;
        case 't':
          // %t expands to the current time in milliseconds.
          os << static_cast<int64_t>(base::OS::TimeCurrentMillis());
          break;
        case '%':
          // %% expands (contracts really) to %.
          os << '%';
          break;
        default:
          // All other %'s expand to themselves.
          os << '%' << *p;
          break;
      }
    } else {
      if (base::OS::isDirectorySeparator(*p)) dir_separator_count--;
      os << *p;
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

  std::ostringstream log_file_name;
  PrepareLogFileName(log_file_name, isolate, FLAG_logfile);
  log_->Initialize(log_file_name.str().c_str());


  if (FLAG_perf_basic_prof) {
    perf_basic_logger_ = new PerfBasicLogger();
    addCodeEventListener(perf_basic_logger_);
  }

  if (FLAG_perf_jit_prof) {
    perf_jit_logger_ = new PerfJitLogger();
    addCodeEventListener(perf_jit_logger_);
  }

  if (FLAG_ll_prof) {
    ll_logger_ = new LowLevelLogger(log_file_name.str().c_str());
    addCodeEventListener(ll_logger_);
  }

  ticker_ = new Ticker(isolate, kSamplingIntervalMs);

  if (Log::InitLogAtStart()) {
    is_logging_ = true;
  }

  if (FLAG_log_internal_timer_events || FLAG_prof_cpp) timer_.Start();

  if (FLAG_prof_cpp) {
    profiler_ = new Profiler(isolate);
    is_logging_ = true;
    profiler_->Engage();
  }

  return true;
}


void Logger::SetCodeEventHandler(uint32_t options,
                                 JitCodeEventHandler event_handler) {
  if (jit_logger_) {
      removeCodeEventListener(jit_logger_);
      delete jit_logger_;
      jit_logger_ = NULL;
  }

  if (event_handler) {
    jit_logger_ = new JitLogger(event_handler);
    addCodeEventListener(jit_logger_);
    if (options & kJitCodeEventEnumExisting) {
      HandleScope scope(isolate_);
      LogCodeObjects();
      LogCompiledFunctions();
    }
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

  if (perf_basic_logger_) {
    removeCodeEventListener(perf_basic_logger_);
    delete perf_basic_logger_;
    perf_basic_logger_ = NULL;
  }

  if (perf_jit_logger_) {
    removeCodeEventListener(perf_jit_logger_);
    delete perf_jit_logger_;
    perf_jit_logger_ = NULL;
  }

  if (ll_logger_) {
    removeCodeEventListener(ll_logger_);
    delete ll_logger_;
    ll_logger_ = NULL;
  }

  if (jit_logger_) {
    removeCodeEventListener(jit_logger_);
    delete jit_logger_;
    jit_logger_ = NULL;
  }

  return log_->Close();
}

} }  // namespace v8::internal
