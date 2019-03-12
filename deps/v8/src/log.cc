// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/log.h"

#include <cstdarg>
#include <memory>
#include <sstream>

#include "src/api-inl.h"
#include "src/bailout-reason.h"
#include "src/base/platform/platform.h"
#include "src/bootstrapper.h"
#include "src/counters.h"
#include "src/deoptimizer.h"
#include "src/global-handles.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/libsampler/sampler.h"
#include "src/log-inl.h"
#include "src/log-utils.h"
#include "src/macro-assembler.h"
#include "src/memcopy.h"
#include "src/objects/api-callbacks.h"
#include "src/perf-jit.h"
#include "src/profiler/tick-sample.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/embedded-data.h"
#include "src/source-position-table.h"
#include "src/string-stream.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/unicode-inl.h"
#include "src/vm-state-inl.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects-inl.h"

#include "src/version.h"

namespace v8 {
namespace internal {

#define DECLARE_EVENT(ignore1, name) #name,
static const char* kLogEventsNames[CodeEventListener::NUMBER_OF_LOG_EVENTS] = {
    LOG_EVENTS_AND_TAGS_LIST(DECLARE_EVENT)};
#undef DECLARE_EVENT

static v8::CodeEventType GetCodeEventTypeForTag(
    CodeEventListener::LogEventsAndTags tag) {
  switch (tag) {
    case CodeEventListener::NUMBER_OF_LOG_EVENTS:
#define V(Event, _) case CodeEventListener::Event:
      LOG_EVENTS_LIST(V)
#undef V
      return v8::CodeEventType::kUnknownType;
#define V(From, To)             \
  case CodeEventListener::From: \
    return v8::CodeEventType::k##To##Type;
      TAGS_LIST(V)
#undef V
  }
  // The execution should never pass here
  UNREACHABLE();
  // NOTE(mmarchini): Workaround to fix a compiler failure on GCC 4.9
  return v8::CodeEventType::kUnknownType;
}
#define CALL_CODE_EVENT_HANDLER(Call) \
  if (listener_) {                    \
    listener_->Call;                  \
  } else {                            \
    PROFILE(isolate_, Call);          \
  }

static const char* ComputeMarker(SharedFunctionInfo shared, AbstractCode code) {
  switch (code->kind()) {
    case AbstractCode::INTERPRETED_FUNCTION:
      return shared->optimization_disabled() ? "" : "~";
    case AbstractCode::OPTIMIZED_FUNCTION:
      return "*";
    default:
      return "";
  }
}

static const char* ComputeMarker(const wasm::WasmCode* code) {
  switch (code->kind()) {
    case wasm::WasmCode::kFunction:
      return code->is_liftoff() ? "" : "*";
    case wasm::WasmCode::kInterpreterEntry:
      return "~";
    default:
      return "";
  }
}

class CodeEventLogger::NameBuffer {
 public:
  NameBuffer() { Reset(); }

  void Reset() {
    utf8_pos_ = 0;
  }

  void Init(CodeEventListener::LogEventsAndTags tag) {
    Reset();
    AppendBytes(kLogEventsNames[tag]);
    AppendByte(':');
  }

  void AppendName(Name name) {
    if (name->IsString()) {
      AppendString(String::cast(name));
    } else {
      Symbol symbol = Symbol::cast(name);
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

  void AppendString(String str) {
    if (str.is_null()) return;
    int length = 0;
    std::unique_ptr<char[]> c_str =
        str->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, &length);
    AppendBytes(c_str.get(), length);
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
    int space = kUtf8BufferSize - utf8_pos_;
    if (space <= 0) return;
    Vector<char> buffer(utf8_buffer_ + utf8_pos_, space);
    int size = SNPrintF(buffer, "%d", n);
    if (size > 0 && utf8_pos_ + size <= kUtf8BufferSize) {
      utf8_pos_ += size;
    }
  }

  void AppendHex(uint32_t n) {
    int space = kUtf8BufferSize - utf8_pos_;
    if (space <= 0) return;
    Vector<char> buffer(utf8_buffer_ + utf8_pos_, space);
    int size = SNPrintF(buffer, "%x", n);
    if (size > 0 && utf8_pos_ + size <= kUtf8BufferSize) {
      utf8_pos_ += size;
    }
  }

  const char* get() { return utf8_buffer_; }
  int size() const { return utf8_pos_; }

 private:
  static const int kUtf8BufferSize = 512;
  static const int kUtf16BufferSize = kUtf8BufferSize;

  int utf8_pos_;
  char utf8_buffer_[kUtf8BufferSize];
};

CodeEventLogger::CodeEventLogger(Isolate* isolate)
    : isolate_(isolate), name_buffer_(new NameBuffer) {}

CodeEventLogger::~CodeEventLogger() { delete name_buffer_; }

void CodeEventLogger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                      AbstractCode code, const char* comment) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(comment);
  LogRecordedBuffer(code, SharedFunctionInfo(), name_buffer_->get(),
                    name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                      AbstractCode code, Name name) {
  name_buffer_->Init(tag);
  name_buffer_->AppendName(name);
  LogRecordedBuffer(code, SharedFunctionInfo(), name_buffer_->get(),
                    name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                      AbstractCode code,
                                      SharedFunctionInfo shared, Name name) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(ComputeMarker(shared, code));
  name_buffer_->AppendByte(' ');
  name_buffer_->AppendName(name);
  LogRecordedBuffer(code, shared, name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                                      AbstractCode code,
                                      SharedFunctionInfo shared, Name source,
                                      int line, int column) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(ComputeMarker(shared, code));
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

void CodeEventLogger::CodeCreateEvent(LogEventsAndTags tag,
                                      const wasm::WasmCode* code,
                                      wasm::WasmName name) {
  name_buffer_->Init(tag);
  if (name.is_empty()) {
    name_buffer_->AppendBytes("<wasm-unknown>");
  } else {
    name_buffer_->AppendBytes(name.start(), name.length());
  }
  name_buffer_->AppendByte('-');
  if (code->IsAnonymous()) {
    name_buffer_->AppendBytes("<anonymous>");
  } else {
    name_buffer_->AppendInt(code->index());
  }
  LogRecordedBuffer(code, name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::RegExpCodeCreateEvent(AbstractCode code, String source) {
  name_buffer_->Init(CodeEventListener::REG_EXP_TAG);
  name_buffer_->AppendString(source);
  LogRecordedBuffer(code, SharedFunctionInfo(), name_buffer_->get(),
                    name_buffer_->size());
}

// Linux perf tool logging support
class PerfBasicLogger : public CodeEventLogger {
 public:
  explicit PerfBasicLogger(Isolate* isolate);
  ~PerfBasicLogger() override;

  void CodeMoveEvent(AbstractCode from, AbstractCode to) override {}
  void CodeDisableOptEvent(AbstractCode code,
                           SharedFunctionInfo shared) override {}

 private:
  void LogRecordedBuffer(AbstractCode code, SharedFunctionInfo shared,
                         const char* name, int length) override;
  void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                         int length) override;
  void WriteLogRecordedBuffer(uintptr_t address, int size, const char* name,
                              int name_length);

  // Extension added to V8 log file name to get the low-level log name.
  static const char kFilenameFormatString[];
  static const int kFilenameBufferPadding;

  FILE* perf_output_handle_;
};

const char PerfBasicLogger::kFilenameFormatString[] = "/tmp/perf-%d.map";
// Extra space for the PID in the filename
const int PerfBasicLogger::kFilenameBufferPadding = 16;

PerfBasicLogger::PerfBasicLogger(Isolate* isolate)
    : CodeEventLogger(isolate), perf_output_handle_(nullptr) {
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
  setvbuf(perf_output_handle_, nullptr, _IOLBF, 0);
}


PerfBasicLogger::~PerfBasicLogger() {
  fclose(perf_output_handle_);
  perf_output_handle_ = nullptr;
}

void PerfBasicLogger::WriteLogRecordedBuffer(uintptr_t address, int size,
                                             const char* name,
                                             int name_length) {
  // Linux perf expects hex literals without a leading 0x, while some
  // implementations of printf might prepend one when using the %p format
  // for pointers, leading to wrongly formatted JIT symbols maps.
  //
  // Instead, we use V8PRIxPTR format string and cast pointer to uintpr_t,
  // so that we have control over the exact output format.
  base::OS::FPrint(perf_output_handle_, "%" V8PRIxPTR " %x %.*s\n", address,
                   size, name_length, name);
}

void PerfBasicLogger::LogRecordedBuffer(AbstractCode code, SharedFunctionInfo,
                                        const char* name, int length) {
  if (FLAG_perf_basic_prof_only_functions &&
      (code->kind() != AbstractCode::INTERPRETED_FUNCTION &&
       code->kind() != AbstractCode::BUILTIN &&
       code->kind() != AbstractCode::OPTIMIZED_FUNCTION)) {
    return;
  }

  WriteLogRecordedBuffer(static_cast<uintptr_t>(code->InstructionStart()),
                         code->InstructionSize(), name, length);
}

void PerfBasicLogger::LogRecordedBuffer(const wasm::WasmCode* code,
                                        const char* name, int length) {
  WriteLogRecordedBuffer(static_cast<uintptr_t>(code->instruction_start()),
                         code->instructions().length(), name, length);
}

// External CodeEventListener
ExternalCodeEventListener::ExternalCodeEventListener(Isolate* isolate)
    : is_listening_(false), isolate_(isolate), code_event_handler_(nullptr) {}

ExternalCodeEventListener::~ExternalCodeEventListener() {
  if (is_listening_) {
    StopListening();
  }
}

void ExternalCodeEventListener::LogExistingCode() {
  HandleScope scope(isolate_);
  ExistingCodeLogger logger(isolate_, this);
  logger.LogCodeObjects();
  logger.LogCompiledFunctions();
}

void ExternalCodeEventListener::StartListening(
    CodeEventHandler* code_event_handler) {
  if (is_listening_ || code_event_handler == nullptr) {
    return;
  }
  code_event_handler_ = code_event_handler;
  is_listening_ = isolate_->code_event_dispatcher()->AddListener(this);
  if (is_listening_) {
    LogExistingCode();
  }
}

void ExternalCodeEventListener::StopListening() {
  if (!is_listening_) {
    return;
  }

  isolate_->code_event_dispatcher()->RemoveListener(this);
  is_listening_ = false;
}

void ExternalCodeEventListener::CodeCreateEvent(
    CodeEventListener::LogEventsAndTags tag, AbstractCode code,
    const char* comment) {
  CodeEvent code_event;
  code_event.code_start_address =
      static_cast<uintptr_t>(code->InstructionStart());
  code_event.code_size = static_cast<size_t>(code->InstructionSize());
  code_event.function_name = isolate_->factory()->empty_string();
  code_event.script_name = isolate_->factory()->empty_string();
  code_event.script_line = 0;
  code_event.script_column = 0;
  code_event.code_type = GetCodeEventTypeForTag(tag);
  code_event.comment = comment;

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

void ExternalCodeEventListener::CodeCreateEvent(
    CodeEventListener::LogEventsAndTags tag, AbstractCode code, Name name) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate_, Handle<Name>(name, isolate_))
          .ToHandleChecked();

  CodeEvent code_event;
  code_event.code_start_address =
      static_cast<uintptr_t>(code->InstructionStart());
  code_event.code_size = static_cast<size_t>(code->InstructionSize());
  code_event.function_name = name_string;
  code_event.script_name = isolate_->factory()->empty_string();
  code_event.script_line = 0;
  code_event.script_column = 0;
  code_event.code_type = GetCodeEventTypeForTag(tag);
  code_event.comment = "";

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

void ExternalCodeEventListener::CodeCreateEvent(
    CodeEventListener::LogEventsAndTags tag, AbstractCode code,
    SharedFunctionInfo shared, Name name) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate_, Handle<Name>(name, isolate_))
          .ToHandleChecked();

  CodeEvent code_event;
  code_event.code_start_address =
      static_cast<uintptr_t>(code->InstructionStart());
  code_event.code_size = static_cast<size_t>(code->InstructionSize());
  code_event.function_name = name_string;
  code_event.script_name = isolate_->factory()->empty_string();
  code_event.script_line = 0;
  code_event.script_column = 0;
  code_event.code_type = GetCodeEventTypeForTag(tag);
  code_event.comment = "";

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

void ExternalCodeEventListener::CodeCreateEvent(
    CodeEventListener::LogEventsAndTags tag, AbstractCode code,
    SharedFunctionInfo shared, Name source, int line, int column) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate_, Handle<Name>(shared->Name(), isolate_))
          .ToHandleChecked();
  Handle<String> source_string =
      Name::ToFunctionName(isolate_, Handle<Name>(source, isolate_))
          .ToHandleChecked();

  CodeEvent code_event;
  code_event.code_start_address =
      static_cast<uintptr_t>(code->InstructionStart());
  code_event.code_size = static_cast<size_t>(code->InstructionSize());
  code_event.function_name = name_string;
  code_event.script_name = source_string;
  code_event.script_line = line;
  code_event.script_column = column;
  code_event.code_type = GetCodeEventTypeForTag(tag);
  code_event.comment = "";

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

void ExternalCodeEventListener::CodeCreateEvent(LogEventsAndTags tag,
                                                const wasm::WasmCode* code,
                                                wasm::WasmName name) {
  // TODO(mmarchini): handle later
}

void ExternalCodeEventListener::RegExpCodeCreateEvent(AbstractCode code,
                                                      String source) {
  CodeEvent code_event;
  code_event.code_start_address =
      static_cast<uintptr_t>(code->InstructionStart());
  code_event.code_size = static_cast<size_t>(code->InstructionSize());
  code_event.function_name = Handle<String>(source, isolate_);
  code_event.script_name = isolate_->factory()->empty_string();
  code_event.script_line = 0;
  code_event.script_column = 0;
  code_event.code_type = GetCodeEventTypeForTag(CodeEventListener::REG_EXP_TAG);
  code_event.comment = "";

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

// Low-level logging support.
class LowLevelLogger : public CodeEventLogger {
 public:
  LowLevelLogger(Isolate* isolate, const char* file_name);
  ~LowLevelLogger() override;

  void CodeMoveEvent(AbstractCode from, AbstractCode to) override;
  void CodeDisableOptEvent(AbstractCode code,
                           SharedFunctionInfo shared) override {}
  void SnapshotPositionEvent(HeapObject obj, int pos);
  void CodeMovingGCEvent() override;

 private:
  void LogRecordedBuffer(AbstractCode code, SharedFunctionInfo shared,
                         const char* name, int length) override;
  void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                         int length) override;

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


  static const char kCodeMovingGCTag = 'G';


  // Extension added to V8 log file name to get the low-level log name.
  static const char kLogExt[];

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

LowLevelLogger::LowLevelLogger(Isolate* isolate, const char* name)
    : CodeEventLogger(isolate), ll_output_handle_(nullptr) {
  // Open the low-level log file.
  size_t len = strlen(name);
  ScopedVector<char> ll_name(static_cast<int>(len + sizeof(kLogExt)));
  MemCopy(ll_name.start(), name, len);
  MemCopy(ll_name.start() + len, kLogExt, sizeof(kLogExt));
  ll_output_handle_ =
      base::OS::FOpen(ll_name.start(), base::OS::LogFileOpenMode);
  setvbuf(ll_output_handle_, nullptr, _IOLBF, 0);

  LogCodeInfo();
}


LowLevelLogger::~LowLevelLogger() {
  fclose(ll_output_handle_);
  ll_output_handle_ = nullptr;
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
#elif V8_TARGET_ARCH_ARM64
  const char arch[] = "arm64";
#elif V8_TARGET_ARCH_S390
  const char arch[] = "s390";
#else
  const char arch[] = "unknown";
#endif
  LogWriteBytes(arch, sizeof(arch));
}

void LowLevelLogger::LogRecordedBuffer(AbstractCode code, SharedFunctionInfo,
                                       const char* name, int length) {
  CodeCreateStruct event;
  event.name_size = length;
  event.code_address = code->InstructionStart();
  event.code_size = code->InstructionSize();
  LogWriteStruct(event);
  LogWriteBytes(name, length);
  LogWriteBytes(reinterpret_cast<const char*>(code->InstructionStart()),
                code->InstructionSize());
}

void LowLevelLogger::LogRecordedBuffer(const wasm::WasmCode* code,
                                       const char* name, int length) {
  CodeCreateStruct event;
  event.name_size = length;
  event.code_address = code->instruction_start();
  event.code_size = code->instructions().length();
  LogWriteStruct(event);
  LogWriteBytes(name, length);
  LogWriteBytes(reinterpret_cast<const char*>(code->instruction_start()),
                code->instructions().length());
}

void LowLevelLogger::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  CodeMoveStruct event;
  event.from_address = from->InstructionStart();
  event.to_address = to->InstructionStart();
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

class JitLogger : public CodeEventLogger {
 public:
  JitLogger(Isolate* isolate, JitCodeEventHandler code_event_handler);

  void CodeMoveEvent(AbstractCode from, AbstractCode to) override;
  void CodeDisableOptEvent(AbstractCode code,
                           SharedFunctionInfo shared) override {}
  void AddCodeLinePosInfoEvent(void* jit_handler_data, int pc_offset,
                               int position,
                               JitCodeEvent::PositionType position_type);

  void* StartCodePosInfoEvent();
  void EndCodePosInfoEvent(Address start_address, void* jit_handler_data);

 private:
  void LogRecordedBuffer(AbstractCode code, SharedFunctionInfo shared,
                         const char* name, int length) override;
  void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                         int length) override;

  JitCodeEventHandler code_event_handler_;
  base::Mutex logger_mutex_;
};

JitLogger::JitLogger(Isolate* isolate, JitCodeEventHandler code_event_handler)
    : CodeEventLogger(isolate), code_event_handler_(code_event_handler) {}

void JitLogger::LogRecordedBuffer(AbstractCode code, SharedFunctionInfo shared,
                                  const char* name, int length) {
  JitCodeEvent event;
  memset(static_cast<void*>(&event), 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADDED;
  event.code_start = reinterpret_cast<void*>(code->InstructionStart());
  event.code_type =
      code->IsCode() ? JitCodeEvent::JIT_CODE : JitCodeEvent::BYTE_CODE;
  event.code_len = code->InstructionSize();
  Handle<SharedFunctionInfo> shared_function_handle;
  if (!shared.is_null() && shared->script()->IsScript()) {
    shared_function_handle =
        Handle<SharedFunctionInfo>(shared, shared->GetIsolate());
  }
  event.script = ToApiHandle<v8::UnboundScript>(shared_function_handle);
  event.name.str = name;
  event.name.len = length;
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  code_event_handler_(&event);
}

void JitLogger::LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                                  int length) {
  JitCodeEvent event;
  memset(static_cast<void*>(&event), 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADDED;
  event.code_type = JitCodeEvent::JIT_CODE;
  event.code_start = code->instructions().start();
  event.code_len = code->instructions().length();
  event.name.str = name;
  event.name.len = length;
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  code_event_handler_(&event);
}

void JitLogger::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  base::MutexGuard guard(&logger_mutex_);

  JitCodeEvent event;
  event.type = JitCodeEvent::CODE_MOVED;
  event.code_type =
      from->IsCode() ? JitCodeEvent::JIT_CODE : JitCodeEvent::BYTE_CODE;
  event.code_start = reinterpret_cast<void*>(from->InstructionStart());
  event.code_len = from->InstructionSize();
  event.new_code_start = reinterpret_cast<void*>(to->InstructionStart());
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  code_event_handler_(&event);
}

void JitLogger::AddCodeLinePosInfoEvent(
    void* jit_handler_data,
    int pc_offset,
    int position,
    JitCodeEvent::PositionType position_type) {
  JitCodeEvent event;
  memset(static_cast<void*>(&event), 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADD_LINE_POS_INFO;
  event.user_data = jit_handler_data;
  event.line_info.offset = pc_offset;
  event.line_info.pos = position;
  event.line_info.position_type = position_type;
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  code_event_handler_(&event);
}


void* JitLogger::StartCodePosInfoEvent() {
  JitCodeEvent event;
  memset(static_cast<void*>(&event), 0, sizeof(event));
  event.type = JitCodeEvent::CODE_START_LINE_INFO_RECORDING;
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  code_event_handler_(&event);
  return event.user_data;
}

void JitLogger::EndCodePosInfoEvent(Address start_address,
                                    void* jit_handler_data) {
  JitCodeEvent event;
  memset(static_cast<void*>(&event), 0, sizeof(event));
  event.type = JitCodeEvent::CODE_END_LINE_INFO_RECORDING;
  event.code_start = reinterpret_cast<void*>(start_address);
  event.user_data = jit_handler_data;
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  code_event_handler_(&event);
}


// TODO(lpy): Keeping sampling thread inside V8 is a workaround currently,
// the reason is to reduce code duplication during migration to sampler library,
// sampling thread, as well as the sampler, will be moved to D8 eventually.
class SamplingThread : public base::Thread {
 public:
  static const int kSamplingThreadStackSize = 64 * KB;

  SamplingThread(sampler::Sampler* sampler, int interval_microseconds)
      : base::Thread(
            base::Thread::Options("SamplingThread", kSamplingThreadStackSize)),
        sampler_(sampler),
        interval_microseconds_(interval_microseconds) {}
  void Run() override {
    while (sampler_->IsActive()) {
      sampler_->DoSample();
      base::OS::Sleep(
          base::TimeDelta::FromMicroseconds(interval_microseconds_));
    }
  }

 private:
  sampler::Sampler* sampler_;
  const int interval_microseconds_;
};


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
  void Insert(v8::TickSample* sample) {
    if (Succ(head_) == static_cast<int>(base::Relaxed_Load(&tail_))) {
      overflow_ = true;
    } else {
      buffer_[head_] = *sample;
      head_ = Succ(head_);
      buffer_semaphore_.Signal();  // Tell we have an element.
    }
  }

  void Run() override;

 private:
  // Waits for a signal and removes profiling data.
  bool Remove(v8::TickSample* sample) {
    buffer_semaphore_.Wait();  // Wait for an element.
    *sample = buffer_[base::Relaxed_Load(&tail_)];
    bool result = overflow_;
    base::Relaxed_Store(
        &tail_, static_cast<base::Atomic32>(Succ(base::Relaxed_Load(&tail_))));
    overflow_ = false;
    return result;
  }

  // Returns the next index in the cyclic buffer.
  int Succ(int index) { return (index + 1) % kBufferSize; }

  Isolate* isolate_;
  // Cyclic buffer for communicating profiling samples
  // between the signal handler and the worker thread.
  static const int kBufferSize = 128;
  v8::TickSample buffer_[kBufferSize];  // Buffer storage.
  int head_;  // Index to the buffer head.
  base::Atomic32 tail_;             // Index to the buffer tail.
  bool overflow_;  // Tell whether a buffer overflow has occurred.
  // Semaphore used for buffer synchronization.
  base::Semaphore buffer_semaphore_;

  // Tells whether worker thread should continue running.
  base::Atomic32 running_;
};


//
// Ticker used to provide ticks to the profiler and the sliding state
// window.
//
class Ticker: public sampler::Sampler {
 public:
  Ticker(Isolate* isolate, int interval_microseconds)
      : sampler::Sampler(reinterpret_cast<v8::Isolate*>(isolate)),
        sampling_thread_(
            base::make_unique<SamplingThread>(this, interval_microseconds)) {}

  ~Ticker() override {
    if (IsActive()) Stop();
  }

  void SetProfiler(Profiler* profiler) {
    DCHECK_NULL(profiler_);
    profiler_ = profiler;
    if (!IsActive()) Start();
    sampling_thread_->StartSynchronously();
  }

  void ClearProfiler() {
    profiler_ = nullptr;
    if (IsActive()) Stop();
    sampling_thread_->Join();
  }

  void SampleStack(const v8::RegisterState& state) override {
    if (!profiler_) return;
    Isolate* isolate = reinterpret_cast<Isolate*>(this->isolate());
    TickSample sample;
    sample.Init(isolate, state, TickSample::kIncludeCEntryFrame, true);
    profiler_->Insert(&sample);
  }

 private:
  Profiler* profiler_ = nullptr;
  std::unique_ptr<SamplingThread> sampling_thread_;
};

//
// Profiler implementation when invoking with --prof.
//
Profiler::Profiler(Isolate* isolate)
    : base::Thread(Options("v8:Profiler")),
      isolate_(isolate),
      head_(0),
      overflow_(false),
      buffer_semaphore_(0) {
  base::Relaxed_Store(&tail_, 0);
  base::Relaxed_Store(&running_, 0);
}

void Profiler::Engage() {
  std::vector<base::OS::SharedLibraryAddress> addresses =
      base::OS::GetSharedLibraryAddresses();
  for (const auto& address : addresses) {
    LOG(isolate_, SharedLibraryEvent(address.library_path, address.start,
                                     address.end, address.aslr_slide));
  }

  // Start thread processing the profiler buffer.
  base::Relaxed_Store(&running_, 1);
  Start();

  // Register to get ticks.
  Logger* logger = isolate_->logger();
  logger->ticker_->SetProfiler(this);

  logger->ProfilerBeginEvent();
}


void Profiler::Disengage() {
  // Stop receiving ticks.
  isolate_->logger()->ticker_->ClearProfiler();

  // Terminate the worker thread by setting running_ to false,
  // inserting a fake element in the queue and then wait for
  // the thread to terminate.
  base::Relaxed_Store(&running_, 0);
  v8::TickSample sample;
  Insert(&sample);
  Join();

  LOG(isolate_, UncheckedStringEvent("profiler", "end"));
}


void Profiler::Run() {
  v8::TickSample sample;
  bool overflow = Remove(&sample);
  while (base::Relaxed_Load(&running_)) {
    LOG(isolate_, TickEvent(&sample, overflow));
    overflow = Remove(&sample);
  }
}


//
// Logger class implementation.
//

Logger::Logger(Isolate* isolate)
    : isolate_(isolate),
      log_events_(nullptr),
      is_logging_(false),
      log_(nullptr),
      is_initialized_(false),
      existing_code_logger_(isolate) {}

Logger::~Logger() {
  delete log_;
}

const LogSeparator Logger::kNext = LogSeparator::kSeparator;

void Logger::AddCodeEventListener(CodeEventListener* listener) {
  bool result = isolate_->code_event_dispatcher()->AddListener(listener);
  CHECK(result);
}

void Logger::RemoveCodeEventListener(CodeEventListener* listener) {
  isolate_->code_event_dispatcher()->RemoveListener(listener);
}

void Logger::ProfilerBeginEvent() {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg << "profiler" << kNext << "begin" << kNext << FLAG_prof_sampling_interval;
  msg.WriteToLogFile();
}


void Logger::StringEvent(const char* name, const char* value) {
  if (FLAG_log) UncheckedStringEvent(name, value);
}


void Logger::UncheckedStringEvent(const char* name, const char* value) {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg << name << kNext << value;
  msg.WriteToLogFile();
}


void Logger::IntPtrTEvent(const char* name, intptr_t value) {
  if (FLAG_log) UncheckedIntPtrTEvent(name, value);
}


void Logger::UncheckedIntPtrTEvent(const char* name, intptr_t value) {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg << name << kNext;
  msg.AppendFormatString("%" V8PRIdPTR, value);
  msg.WriteToLogFile();
}

void Logger::HandleEvent(const char* name, Address* location) {
  if (!log_->IsEnabled() || !FLAG_log_handles) return;
  Log::MessageBuilder msg(log_);
  msg << name << kNext << reinterpret_cast<void*>(location);
  msg.WriteToLogFile();
}


void Logger::ApiSecurityCheck() {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  Log::MessageBuilder msg(log_);
  msg << "api" << kNext << "check-security";
  msg.WriteToLogFile();
}

void Logger::SharedLibraryEvent(const std::string& library_path,
                                uintptr_t start, uintptr_t end,
                                intptr_t aslr_slide) {
  if (!log_->IsEnabled() || !FLAG_prof_cpp) return;
  Log::MessageBuilder msg(log_);
  msg << "shared-library" << kNext << library_path.c_str() << kNext
      << reinterpret_cast<void*>(start) << kNext << reinterpret_cast<void*>(end)
      << kNext << aslr_slide;
  msg.WriteToLogFile();
}

void Logger::CodeDeoptEvent(Code code, DeoptimizeKind kind, Address pc,
                            int fp_to_sp_delta) {
  if (!log_->IsEnabled()) return;
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(code, pc);
  Log::MessageBuilder msg(log_);
  msg << "code-deopt" << kNext << timer_.Elapsed().InMicroseconds() << kNext
      << code->CodeSize() << kNext
      << reinterpret_cast<void*>(code->InstructionStart());

  // Deoptimization position.
  std::ostringstream deopt_location;
  int inlining_id = -1;
  int script_offset = -1;
  if (info.position.IsKnown()) {
    info.position.Print(deopt_location, code);
    inlining_id = info.position.InliningId();
    script_offset = info.position.ScriptOffset();
  } else {
    deopt_location << "<unknown>";
  }
  msg << kNext << inlining_id << kNext << script_offset << kNext;
  msg << Deoptimizer::MessageFor(kind) << kNext;
  msg << deopt_location.str().c_str() << kNext
      << DeoptimizeReasonToString(info.deopt_reason);
  msg.WriteToLogFile();
}


void Logger::CurrentTimeEvent() {
  if (!log_->IsEnabled()) return;
  DCHECK(FLAG_log_internal_timer_events);
  Log::MessageBuilder msg(log_);
  msg << "current-time" << kNext << timer_.Elapsed().InMicroseconds();
  msg.WriteToLogFile();
}


void Logger::TimerEvent(Logger::StartEnd se, const char* name) {
  if (!log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  switch (se) {
    case START:
      msg << "timer-event-start";
      break;
    case END:
      msg << "timer-event-end";
      break;
    case STAMP:
      msg << "timer-event";
  }
  msg << kNext << name << kNext << timer_.Elapsed().InMicroseconds();
  msg.WriteToLogFile();
}

// static
void Logger::EnterExternal(Isolate* isolate) {
  DCHECK(FLAG_log_internal_timer_events);
  LOG(isolate, TimerEvent(START, TimerEventExternal::name()));
  DCHECK(isolate->current_vm_state() == JS);
  isolate->set_current_vm_state(EXTERNAL);
}

// static
void Logger::LeaveExternal(Isolate* isolate) {
  DCHECK(FLAG_log_internal_timer_events);
  LOG(isolate, TimerEvent(END, TimerEventExternal::name()));
  DCHECK(isolate->current_vm_state() == EXTERNAL);
  isolate->set_current_vm_state(JS);
}

// Instantiate template methods.
#define V(TimerName, expose)                                           \
  template void TimerEventScope<TimerEvent##TimerName>::LogTimerEvent( \
      Logger::StartEnd se);
TIMER_EVENTS_LIST(V)
#undef V

void Logger::ApiNamedPropertyAccess(const char* tag, JSObject holder,
                                    Object property_name) {
  DCHECK(property_name->IsName());
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  Log::MessageBuilder msg(log_);
  msg << "api" << kNext << tag << kNext << holder->class_name() << kNext
      << Name::cast(property_name);
  msg.WriteToLogFile();
}

void Logger::ApiIndexedPropertyAccess(const char* tag, JSObject holder,
                                      uint32_t index) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  Log::MessageBuilder msg(log_);
  msg << "api" << kNext << tag << kNext << holder->class_name() << kNext
      << index;
  msg.WriteToLogFile();
}

void Logger::ApiObjectAccess(const char* tag, JSObject object) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  Log::MessageBuilder msg(log_);
  msg << "api" << kNext << tag << kNext << object->class_name();
  msg.WriteToLogFile();
}

void Logger::ApiEntryCall(const char* name) {
  if (!log_->IsEnabled() || !FLAG_log_api) return;
  Log::MessageBuilder msg(log_);
  msg << "api" << kNext << name;
  msg.WriteToLogFile();
}


void Logger::NewEvent(const char* name, void* object, size_t size) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg << "new" << kNext << name << kNext << object << kNext
      << static_cast<unsigned int>(size);
  msg.WriteToLogFile();
}


void Logger::DeleteEvent(const char* name, void* object) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg << "delete" << kNext << name << kNext << object;
  msg.WriteToLogFile();
}

void Logger::CallbackEventInternal(const char* prefix, Name name,
                                   Address entry_point) {
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg << kLogEventsNames[CodeEventListener::CODE_CREATION_EVENT] << kNext
      << kLogEventsNames[CodeEventListener::CALLBACK_TAG] << kNext << -2
      << kNext << timer_.Elapsed().InMicroseconds() << kNext
      << reinterpret_cast<void*>(entry_point) << kNext << 1 << kNext << prefix
      << name;
  msg.WriteToLogFile();
}

void Logger::CallbackEvent(Name name, Address entry_point) {
  CallbackEventInternal("", name, entry_point);
}

void Logger::GetterCallbackEvent(Name name, Address entry_point) {
  CallbackEventInternal("get ", name, entry_point);
}

void Logger::SetterCallbackEvent(Name name, Address entry_point) {
  CallbackEventInternal("set ", name, entry_point);
}

namespace {

void AppendCodeCreateHeader(Log::MessageBuilder& msg,
                            CodeEventListener::LogEventsAndTags tag,
                            AbstractCode::Kind kind, uint8_t* address, int size,
                            base::ElapsedTimer* timer) {
  msg << kLogEventsNames[CodeEventListener::CODE_CREATION_EVENT]
      << Logger::kNext << kLogEventsNames[tag] << Logger::kNext << kind
      << Logger::kNext << timer->Elapsed().InMicroseconds() << Logger::kNext
      << reinterpret_cast<void*>(address) << Logger::kNext << size
      << Logger::kNext;
}

void AppendCodeCreateHeader(Log::MessageBuilder& msg,
                            CodeEventListener::LogEventsAndTags tag,
                            AbstractCode code, base::ElapsedTimer* timer) {
  AppendCodeCreateHeader(msg, tag, code->kind(),
                         reinterpret_cast<uint8_t*>(code->InstructionStart()),
                         code->InstructionSize(), timer);
}

}  // namespace

void Logger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                             AbstractCode code, const char* comment) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(msg, tag, code, &timer_);
  msg << comment;
  msg.WriteToLogFile();
}

void Logger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                             AbstractCode code, Name name) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(msg, tag, code, &timer_);
  msg << name;
  msg.WriteToLogFile();
}

void Logger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                             AbstractCode code, SharedFunctionInfo shared,
                             Name name) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  if (code == AbstractCode::cast(
                  isolate_->builtins()->builtin(Builtins::kCompileLazy))) {
    return;
  }

  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(msg, tag, code, &timer_);
  msg << name << kNext << reinterpret_cast<void*>(shared->address()) << kNext
      << ComputeMarker(shared, code);
  msg.WriteToLogFile();
}

void Logger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                             const wasm::WasmCode* code, wasm::WasmName name) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(msg, tag, AbstractCode::Kind::WASM_FUNCTION,
                         code->instructions().start(),
                         code->instructions().length(), &timer_);
  if (name.is_empty()) {
    msg << "<unknown wasm>";
  } else {
    msg.AppendString(name);
  }
  // We have to add two extra fields that allow the tick processor to group
  // events for the same wasm function, even if it gets compiled again. For
  // normal JS functions, we use the shared function info. For wasm, the pointer
  // to the native module + function index works well enough.
  // TODO(herhut) Clean up the tick processor code instead.
  void* tag_ptr =
      reinterpret_cast<byte*>(code->native_module()) + code->index();
  msg << kNext << tag_ptr << kNext << ComputeMarker(code);
  msg.WriteToLogFile();
}

// Although, it is possible to extract source and line from
// the SharedFunctionInfo object, we left it to caller
// to leave logging functions free from heap allocations.
void Logger::CodeCreateEvent(CodeEventListener::LogEventsAndTags tag,
                             AbstractCode code, SharedFunctionInfo shared,
                             Name source, int line, int column) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  {
    Log::MessageBuilder msg(log_);
    AppendCodeCreateHeader(msg, tag, code, &timer_);
    msg << shared->DebugName() << " " << source << ":" << line << ":" << column
        << kNext << reinterpret_cast<void*>(shared->address()) << kNext
        << ComputeMarker(shared, code);
    msg.WriteToLogFile();
  }

  if (!FLAG_log_source_code) return;
  Object script_object = shared->script();
  if (!script_object->IsScript()) return;
  Script script = Script::cast(script_object);
  if (!EnsureLogScriptSource(script)) return;

  // We log source code information in the form:
  //
  // code-source-info <addr>,<script>,<start>,<end>,<pos>,<inline-pos>,<fns>
  //
  // where
  //   <addr> is code object address
  //   <script> is script id
  //   <start> is the starting position inside the script
  //   <end> is the end position inside the script
  //   <pos> is source position table encoded in the string,
  //      it is a sequence of C<code-offset>O<script-offset>[I<inlining-id>]
  //      where
  //        <code-offset> is the offset within the code object
  //        <script-offset> is the position within the script
  //        <inlining-id> is the offset in the <inlining> table
  //   <inlining> table is a sequence of strings of the form
  //      F<function-id>O<script-offset>[I<inlining-id>]
  //      where
  //         <function-id> is an index into the <fns> function table
  //   <fns> is the function table encoded as a sequence of strings
  //      S<shared-function-info-address>
  Log::MessageBuilder msg(log_);
  msg << "code-source-info" << kNext
      << reinterpret_cast<void*>(code->InstructionStart()) << kNext
      << script->id() << kNext << shared->StartPosition() << kNext
      << shared->EndPosition() << kNext;

  SourcePositionTableIterator iterator(code->source_position_table());
  bool hasInlined = false;
  for (; !iterator.done(); iterator.Advance()) {
    SourcePosition pos = iterator.source_position();
    msg << "C" << iterator.code_offset() << "O" << pos.ScriptOffset();
    if (pos.isInlined()) {
      msg << "I" << pos.InliningId();
      hasInlined = true;
    }
  }
  msg << kNext;
  int maxInlinedId = -1;
  if (hasInlined) {
    PodArray<InliningPosition> inlining_positions =
        DeoptimizationData::cast(Code::cast(code)->deoptimization_data())
            ->InliningPositions();
    for (int i = 0; i < inlining_positions->length(); i++) {
      InliningPosition inlining_pos = inlining_positions->get(i);
      msg << "F";
      if (inlining_pos.inlined_function_id != -1) {
        msg << inlining_pos.inlined_function_id;
        if (inlining_pos.inlined_function_id > maxInlinedId) {
          maxInlinedId = inlining_pos.inlined_function_id;
        }
      }
      SourcePosition pos = inlining_pos.position;
      msg << "O" << pos.ScriptOffset();
      if (pos.isInlined()) {
        msg << "I" << pos.InliningId();
      }
    }
  }
  msg << kNext;
  if (hasInlined) {
    DeoptimizationData deopt_data =
        DeoptimizationData::cast(Code::cast(code)->deoptimization_data());

    msg << std::hex;
    for (int i = 0; i <= maxInlinedId; i++) {
      msg << "S"
          << reinterpret_cast<void*>(
                 deopt_data->GetInlinedFunction(i)->address());
    }
    msg << std::dec;
  }
  msg.WriteToLogFile();
}

void Logger::CodeDisableOptEvent(AbstractCode code, SharedFunctionInfo shared) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg << kLogEventsNames[CodeEventListener::CODE_DISABLE_OPT_EVENT] << kNext
      << shared->DebugName() << kNext
      << GetBailoutReason(shared->disable_optimization_reason());
  msg.WriteToLogFile();
}

void Logger::CodeMovingGCEvent() {
  if (!is_listening_to_code_events()) return;
  if (!log_->IsEnabled() || !FLAG_ll_prof) return;
  base::OS::SignalCodeMovingGC();
}

void Logger::RegExpCodeCreateEvent(AbstractCode code, String source) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  AppendCodeCreateHeader(msg, CodeEventListener::REG_EXP_TAG, code, &timer_);
  msg << source;
  msg.WriteToLogFile();
}

void Logger::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  if (!is_listening_to_code_events()) return;
  MoveEventInternal(CodeEventListener::CODE_MOVE_EVENT, from->address(),
                    to->address());
}

namespace {

void CodeLinePosEvent(JitLogger* jit_logger, Address code_start,
                      SourcePositionTableIterator& iter) {
  if (jit_logger) {
    void* jit_handler_data = jit_logger->StartCodePosInfoEvent();
    for (; !iter.done(); iter.Advance()) {
      if (iter.is_statement()) {
        jit_logger->AddCodeLinePosInfoEvent(
            jit_handler_data, iter.code_offset(),
            iter.source_position().ScriptOffset(),
            JitCodeEvent::STATEMENT_POSITION);
      }
      jit_logger->AddCodeLinePosInfoEvent(jit_handler_data, iter.code_offset(),
                                          iter.source_position().ScriptOffset(),
                                          JitCodeEvent::POSITION);
    }
    jit_logger->EndCodePosInfoEvent(code_start, jit_handler_data);
  }
}

}  // namespace

void Logger::CodeLinePosInfoRecordEvent(Address code_start,
                                        ByteArray source_position_table) {
  SourcePositionTableIterator iter(source_position_table);
  CodeLinePosEvent(jit_logger_.get(), code_start, iter);
}

void Logger::CodeLinePosInfoRecordEvent(
    Address code_start, Vector<const byte> source_position_table) {
  SourcePositionTableIterator iter(source_position_table);
  CodeLinePosEvent(jit_logger_.get(), code_start, iter);
}

void Logger::CodeNameEvent(Address addr, int pos, const char* code_name) {
  if (code_name == nullptr) return;  // Not a code object.
  Log::MessageBuilder msg(log_);
  msg << kLogEventsNames[CodeEventListener::SNAPSHOT_CODE_NAME_EVENT] << kNext
      << pos << kNext << code_name;
  msg.WriteToLogFile();
}


void Logger::SharedFunctionInfoMoveEvent(Address from, Address to) {
  if (!is_listening_to_code_events()) return;
  MoveEventInternal(CodeEventListener::SHARED_FUNC_MOVE_EVENT, from, to);
}

void Logger::MoveEventInternal(CodeEventListener::LogEventsAndTags event,
                               Address from, Address to) {
  if (!FLAG_log_code || !log_->IsEnabled()) return;
  Log::MessageBuilder msg(log_);
  msg << kLogEventsNames[event] << kNext << reinterpret_cast<void*>(from)
      << kNext << reinterpret_cast<void*>(to);
  msg.WriteToLogFile();
}


void Logger::ResourceEvent(const char* name, const char* tag) {
  if (!log_->IsEnabled() || !FLAG_log) return;
  Log::MessageBuilder msg(log_);
  msg << name << kNext << tag << kNext;

  uint32_t sec, usec;
  if (base::OS::GetUserTime(&sec, &usec) != -1) {
    msg << sec << kNext << usec << kNext;
  }
  msg.AppendFormatString("%.0f",
                         V8::GetCurrentPlatform()->CurrentClockTimeMillis());
  msg.WriteToLogFile();
}

void Logger::SuspectReadEvent(Name name, Object obj) {
  if (!log_->IsEnabled() || !FLAG_log_suspect) return;
  Log::MessageBuilder msg(log_);
  String class_name = obj->IsJSObject()
                          ? JSObject::cast(obj)->class_name()
                          : ReadOnlyRoots(isolate_).empty_string();
  msg << "suspect-read" << kNext << class_name << kNext << name;
  msg.WriteToLogFile();
}

namespace {
void AppendFunctionMessage(Log::MessageBuilder& msg, const char* reason,
                           int script_id, double time_delta, int start_position,
                           int end_position, base::ElapsedTimer* timer) {
  msg << "function" << Logger::kNext << reason << Logger::kNext << script_id
      << Logger::kNext << start_position << Logger::kNext << end_position
      << Logger::kNext << time_delta << Logger::kNext
      << timer->Elapsed().InMicroseconds() << Logger::kNext;
}
}  // namespace

void Logger::FunctionEvent(const char* reason, int script_id, double time_delta,
                           int start_position, int end_position,
                           String function_name) {
  if (!log_->IsEnabled() || !FLAG_log_function_events) return;
  Log::MessageBuilder msg(log_);
  AppendFunctionMessage(msg, reason, script_id, time_delta, start_position,
                        end_position, &timer_);
  if (!function_name.is_null()) msg << function_name;
  msg.WriteToLogFile();
}

void Logger::FunctionEvent(const char* reason, int script_id, double time_delta,
                           int start_position, int end_position,
                           const char* function_name,
                           size_t function_name_length) {
  if (!log_->IsEnabled() || !FLAG_log_function_events) return;
  Log::MessageBuilder msg(log_);
  AppendFunctionMessage(msg, reason, script_id, time_delta, start_position,
                        end_position, &timer_);
  if (function_name_length > 0) {
    msg.AppendString(function_name, function_name_length);
  }
  msg.WriteToLogFile();
}

void Logger::CompilationCacheEvent(const char* action, const char* cache_type,
                                   SharedFunctionInfo sfi) {
  if (!log_->IsEnabled() || !FLAG_log_function_events) return;
  Log::MessageBuilder msg(log_);
  int script_id = -1;
  if (sfi->script()->IsScript()) {
    script_id = Script::cast(sfi->script())->id();
  }
  msg << "compilation-cache" << Logger::kNext << action << Logger::kNext
      << cache_type << Logger::kNext << script_id << Logger::kNext
      << sfi->StartPosition() << Logger::kNext << sfi->EndPosition()
      << Logger::kNext << timer_.Elapsed().InMicroseconds();
  msg.WriteToLogFile();
}

void Logger::ScriptEvent(ScriptEventType type, int script_id) {
  if (!log_->IsEnabled() || !FLAG_log_function_events) return;
  Log::MessageBuilder msg(log_);
  msg << "script" << Logger::kNext;
  switch (type) {
    case ScriptEventType::kReserveId:
      msg << "reserve-id";
      break;
    case ScriptEventType::kCreate:
      msg << "create";
      break;
    case ScriptEventType::kDeserialize:
      msg << "deserialize";
      break;
    case ScriptEventType::kBackgroundCompile:
      msg << "background-compile";
      break;
    case ScriptEventType::kStreamingCompile:
      msg << "streaming-compile";
      break;
  }
  msg << Logger::kNext << script_id << Logger::kNext
      << timer_.Elapsed().InMicroseconds();
  msg.WriteToLogFile();
}

void Logger::ScriptDetails(Script script) {
  if (!log_->IsEnabled() || !FLAG_log_function_events) return;
  {
    Log::MessageBuilder msg(log_);
    msg << "script-details" << Logger::kNext << script->id() << Logger::kNext;
    if (script->name()->IsString()) {
      msg << String::cast(script->name());
    }
    msg << Logger::kNext << script->line_offset() << Logger::kNext
        << script->column_offset() << Logger::kNext;
    if (script->source_mapping_url()->IsString()) {
      msg << String::cast(script->source_mapping_url());
    }
    msg.WriteToLogFile();
  }
  EnsureLogScriptSource(script);
}

bool Logger::EnsureLogScriptSource(Script script) {
  if (!log_->IsEnabled()) return false;
  Log::MessageBuilder msg(log_);
  // Make sure the script is written to the log file.
  int script_id = script->id();
  if (logged_source_code_.find(script_id) != logged_source_code_.end()) {
    return true;
  }
  // This script has not been logged yet.
  logged_source_code_.insert(script_id);
  Object source_object = script->source();
  if (!source_object->IsString()) return false;
  String source_code = String::cast(source_object);
  msg << "script-source" << kNext << script_id << kNext;

  // Log the script name.
  if (script->name()->IsString()) {
    msg << String::cast(script->name()) << kNext;
  } else {
    msg << "<unknown>" << kNext;
  }

  // Log the source code.
  msg << source_code;
  msg.WriteToLogFile();
  return true;
}

void Logger::RuntimeCallTimerEvent() {
  RuntimeCallStats* stats = isolate_->counters()->runtime_call_stats();
  RuntimeCallCounter* counter = stats->current_counter();
  if (counter == nullptr) return;
  Log::MessageBuilder msg(log_);
  msg << "active-runtime-timer" << kNext << counter->name();
  msg.WriteToLogFile();
}

void Logger::TickEvent(v8::TickSample* sample, bool overflow) {
  if (!log_->IsEnabled() || !FLAG_prof_cpp) return;
  if (V8_UNLIKELY(FLAG_runtime_stats ==
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE)) {
    RuntimeCallTimerEvent();
  }
  Log::MessageBuilder msg(log_);
  msg << kLogEventsNames[CodeEventListener::TICK_EVENT] << kNext
      << reinterpret_cast<void*>(sample->pc) << kNext
      << timer_.Elapsed().InMicroseconds();
  if (sample->has_external_callback) {
    msg << kNext << 1 << kNext
        << reinterpret_cast<void*>(sample->external_callback_entry);
  } else {
    msg << kNext << 0 << kNext << reinterpret_cast<void*>(sample->tos);
  }
  msg << kNext << static_cast<int>(sample->state);
  if (overflow) msg << kNext << "overflow";
  for (unsigned i = 0; i < sample->frames_count; ++i) {
    msg << kNext << reinterpret_cast<void*>(sample->stack[i]);
  }
  msg.WriteToLogFile();
}

void Logger::ICEvent(const char* type, bool keyed, Map map, Object key,
                     char old_state, char new_state, const char* modifier,
                     const char* slow_stub_reason) {
  if (!log_->IsEnabled() || !FLAG_trace_ic) return;
  Log::MessageBuilder msg(log_);
  if (keyed) msg << "Keyed";
  int line;
  int column;
  Address pc = isolate_->GetAbstractPC(&line, &column);
  msg << type << kNext << reinterpret_cast<void*>(pc) << kNext << line << kNext
      << column << kNext << old_state << kNext << new_state << kNext
      << reinterpret_cast<void*>(map.ptr()) << kNext;
  if (key->IsSmi()) {
    msg << Smi::ToInt(key);
  } else if (key->IsNumber()) {
    msg << key->Number();
  } else if (key->IsName()) {
    msg << Name::cast(key);
  }
  msg << kNext << modifier << kNext;
  if (slow_stub_reason != nullptr) {
    msg << slow_stub_reason;
  }
  msg.WriteToLogFile();
}

void Logger::MapEvent(const char* type, Map from, Map to, const char* reason,
                      HeapObject name_or_sfi) {
  DisallowHeapAllocation no_gc;
  if (!log_->IsEnabled() || !FLAG_trace_maps) return;
  if (!to.is_null()) MapDetails(to);
  int line = -1;
  int column = -1;
  Address pc = 0;

  if (!isolate_->bootstrapper()->IsActive()) {
    pc = isolate_->GetAbstractPC(&line, &column);
  }
  Log::MessageBuilder msg(log_);
  msg << "map" << kNext << type << kNext << timer_.Elapsed().InMicroseconds()
      << kNext << reinterpret_cast<void*>(from.ptr()) << kNext
      << reinterpret_cast<void*>(to.ptr()) << kNext
      << reinterpret_cast<void*>(pc) << kNext << line << kNext << column
      << kNext << reason << kNext;

  if (!name_or_sfi.is_null()) {
    if (name_or_sfi->IsName()) {
      msg << Name::cast(name_or_sfi);
    } else if (name_or_sfi->IsSharedFunctionInfo()) {
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(name_or_sfi);
      msg << sfi->DebugName();
#if V8_SFI_HAS_UNIQUE_ID
      msg << " " << sfi->unique_id();
#endif  // V8_SFI_HAS_UNIQUE_ID
    }
  }
  msg.WriteToLogFile();
}

void Logger::MapCreate(Map map) {
  if (!log_->IsEnabled() || !FLAG_trace_maps) return;
  DisallowHeapAllocation no_gc;
  Log::MessageBuilder msg(log_);
  msg << "map-create" << kNext << timer_.Elapsed().InMicroseconds() << kNext
      << reinterpret_cast<void*>(map.ptr());
  msg.WriteToLogFile();
}

void Logger::MapDetails(Map map) {
  if (!log_->IsEnabled() || !FLAG_trace_maps) return;
  DisallowHeapAllocation no_gc;
  Log::MessageBuilder msg(log_);
  msg << "map-details" << kNext << timer_.Elapsed().InMicroseconds() << kNext
      << reinterpret_cast<void*>(map.ptr()) << kNext;
  if (FLAG_trace_maps_details) {
    std::ostringstream buffer;
    map->PrintMapDetails(buffer);
    msg << buffer.str().c_str();
  }
  msg.WriteToLogFile();
}

static void AddFunctionAndCode(SharedFunctionInfo sfi, AbstractCode code_object,
                               Handle<SharedFunctionInfo>* sfis,
                               Handle<AbstractCode>* code_objects, int offset) {
  if (sfis != nullptr) {
    sfis[offset] = Handle<SharedFunctionInfo>(sfi, sfi->GetIsolate());
  }
  if (code_objects != nullptr) {
    code_objects[offset] = Handle<AbstractCode>(code_object, sfi->GetIsolate());
  }
}

static int EnumerateCompiledFunctions(Heap* heap,
                                      Handle<SharedFunctionInfo>* sfis,
                                      Handle<AbstractCode>* code_objects) {
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  int compiled_funcs_count = 0;

  // Iterate the heap to find shared function info objects and record
  // the unoptimized code for them.
  for (HeapObject obj = iterator.next(); !obj.is_null();
       obj = iterator.next()) {
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(obj);
      if (sfi->is_compiled() &&
          (!sfi->script()->IsScript() ||
           Script::cast(sfi->script())->HasValidSource())) {
        AddFunctionAndCode(sfi, AbstractCode::cast(sfi->abstract_code()), sfis,
                           code_objects, compiled_funcs_count);
        ++compiled_funcs_count;
      }
    } else if (obj->IsJSFunction()) {
      // Given that we no longer iterate over all optimized JSFunctions, we need
      // to take care of this here.
      JSFunction function = JSFunction::cast(obj);
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(function->shared());
      Object maybe_script = sfi->script();
      if (maybe_script->IsScript() &&
          !Script::cast(maybe_script)->HasValidSource()) {
        continue;
      }
      // TODO(jarin) This leaves out deoptimized code that might still be on the
      // stack. Also note that we will not log optimized code objects that are
      // only on a type feedback vector. We should make this mroe precise.
      if (function->IsOptimized()) {
        AddFunctionAndCode(sfi, AbstractCode::cast(function->code()), sfis,
                           code_objects, compiled_funcs_count);
        ++compiled_funcs_count;
      }
    }
  }
  return compiled_funcs_count;
}

static int EnumerateWasmModuleObjects(
    Heap* heap, Handle<WasmModuleObject>* module_objects) {
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  int module_objects_count = 0;

  for (HeapObject obj = iterator.next(); !obj.is_null();
       obj = iterator.next()) {
    if (obj->IsWasmModuleObject()) {
      WasmModuleObject module = WasmModuleObject::cast(obj);
      if (module_objects != nullptr) {
        module_objects[module_objects_count] = handle(module, heap->isolate());
      }
      module_objects_count++;
    }
  }
  return module_objects_count;
}

void Logger::LogCodeObject(Object object) {
  existing_code_logger_.LogCodeObject(object);
}

void Logger::LogCodeObjects() { existing_code_logger_.LogCodeObjects(); }

void Logger::LogExistingFunction(Handle<SharedFunctionInfo> shared,
                                 Handle<AbstractCode> code) {
  existing_code_logger_.LogExistingFunction(shared, code);
}

void Logger::LogCompiledFunctions() {
  existing_code_logger_.LogCompiledFunctions();
}

void Logger::LogAccessorCallbacks() {
  Heap* heap = isolate_->heap();
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  for (HeapObject obj = iterator.next(); !obj.is_null();
       obj = iterator.next()) {
    if (!obj->IsAccessorInfo()) continue;
    AccessorInfo ai = AccessorInfo::cast(obj);
    if (!ai->name()->IsName()) continue;
    Address getter_entry = v8::ToCData<Address>(ai->getter());
    Name name = Name::cast(ai->name());
    if (getter_entry != 0) {
#if USES_FUNCTION_DESCRIPTORS
      getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(getter_entry);
#endif
      PROFILE(isolate_, GetterCallbackEvent(name, getter_entry));
    }
    Address setter_entry = v8::ToCData<Address>(ai->setter());
    if (setter_entry != 0) {
#if USES_FUNCTION_DESCRIPTORS
      setter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(setter_entry);
#endif
      PROFILE(isolate_, SetterCallbackEvent(name, setter_entry));
    }
  }
}

void Logger::LogAllMaps() {
  DisallowHeapAllocation no_gc;
  Heap* heap = isolate_->heap();
  HeapIterator iterator(heap);
  for (HeapObject obj = iterator.next(); !obj.is_null();
       obj = iterator.next()) {
    if (!obj->IsMap()) continue;
    Map map = Map::cast(obj);
    MapCreate(map);
    MapDetails(map);
  }
}

static void AddIsolateIdIfNeeded(std::ostream& os,  // NOLINT
                                 Isolate* isolate) {
  if (FLAG_logfile_per_isolate) {
    os << "isolate-" << isolate << "-" << base::OS::GetCurrentProcessId()
       << "-";
  }
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
          os << static_cast<int64_t>(
              V8::GetCurrentPlatform()->CurrentClockTimeMillis());
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

  std::ostringstream log_file_name;
  std::ostringstream source_log_file_name;
  PrepareLogFileName(log_file_name, isolate, FLAG_logfile);
  log_ = new Log(this, log_file_name.str().c_str());

  if (FLAG_perf_basic_prof) {
    perf_basic_logger_.reset(new PerfBasicLogger(isolate));
    AddCodeEventListener(perf_basic_logger_.get());
  }

  if (FLAG_perf_prof) {
    perf_jit_logger_.reset(new PerfJitLogger(isolate));
    AddCodeEventListener(perf_jit_logger_.get());
  }

  if (FLAG_ll_prof) {
    ll_logger_.reset(new LowLevelLogger(isolate, log_file_name.str().c_str()));
    AddCodeEventListener(ll_logger_.get());
  }

  ticker_.reset(new Ticker(isolate, FLAG_prof_sampling_interval));

  if (Log::InitLogAtStart()) {
    is_logging_ = true;
  }

  timer_.Start();

  if (FLAG_prof_cpp) {
    profiler_.reset(new Profiler(isolate));
    is_logging_ = true;
    profiler_->Engage();
  }

  if (is_logging_) {
    AddCodeEventListener(this);
  }

  return true;
}


void Logger::SetCodeEventHandler(uint32_t options,
                                 JitCodeEventHandler event_handler) {
  if (jit_logger_) {
    RemoveCodeEventListener(jit_logger_.get());
    jit_logger_.reset();
  }

  if (event_handler) {
    jit_logger_.reset(new JitLogger(isolate_, event_handler));
    AddCodeEventListener(jit_logger_.get());
    if (options & kJitCodeEventEnumExisting) {
      HandleScope scope(isolate_);
      LogCodeObjects();
      LogCompiledFunctions();
    }
  }
}

sampler::Sampler* Logger::sampler() { return ticker_.get(); }

void Logger::StopProfilerThread() {
  if (profiler_ != nullptr) {
    profiler_->Disengage();
    profiler_.reset();
  }
}

FILE* Logger::TearDown() {
  if (!is_initialized_) return nullptr;
  is_initialized_ = false;

  // Stop the profiler thread before closing the file.
  StopProfilerThread();

  ticker_.reset();

  if (perf_basic_logger_) {
    RemoveCodeEventListener(perf_basic_logger_.get());
    perf_basic_logger_.reset();
  }

  if (perf_jit_logger_) {
    RemoveCodeEventListener(perf_jit_logger_.get());
    perf_jit_logger_.reset();
  }

  if (ll_logger_) {
    RemoveCodeEventListener(ll_logger_.get());
    ll_logger_.reset();
  }

  if (jit_logger_) {
    RemoveCodeEventListener(jit_logger_.get());
    jit_logger_.reset();
  }

  return log_->Close();
}

void ExistingCodeLogger::LogCodeObject(Object object) {
  AbstractCode abstract_code = AbstractCode::cast(object);
  CodeEventListener::LogEventsAndTags tag = CodeEventListener::STUB_TAG;
  const char* description = "Unknown code from before profiling";
  switch (abstract_code->kind()) {
    case AbstractCode::INTERPRETED_FUNCTION:
    case AbstractCode::OPTIMIZED_FUNCTION:
      return;  // We log this later using LogCompiledFunctions.
    case AbstractCode::BYTECODE_HANDLER:
      return;  // We log it later by walking the dispatch table.
    case AbstractCode::STUB:
      description = "STUB code";
      tag = CodeEventListener::STUB_TAG;
      break;
    case AbstractCode::REGEXP:
      description = "Regular expression code";
      tag = CodeEventListener::REG_EXP_TAG;
      break;
    case AbstractCode::BUILTIN:
      if (Code::cast(object)->is_interpreter_trampoline_builtin() &&
          Code::cast(object) !=
              *BUILTIN_CODE(isolate_, InterpreterEntryTrampoline)) {
        return;
      }
      description =
          isolate_->builtins()->name(abstract_code->GetCode()->builtin_index());
      tag = CodeEventListener::BUILTIN_TAG;
      break;
    case AbstractCode::WASM_FUNCTION:
      description = "A Wasm function";
      tag = CodeEventListener::FUNCTION_TAG;
      break;
    case AbstractCode::JS_TO_WASM_FUNCTION:
      description = "A JavaScript to Wasm adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case AbstractCode::WASM_TO_JS_FUNCTION:
      description = "A Wasm to JavaScript adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case AbstractCode::WASM_INTERPRETER_ENTRY:
      description = "A Wasm to Interpreter adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case AbstractCode::C_WASM_ENTRY:
      description = "A C to Wasm entry stub";
      tag = CodeEventListener::STUB_TAG;
      break;
    case AbstractCode::NUMBER_OF_KINDS:
      UNIMPLEMENTED();
  }
  CALL_CODE_EVENT_HANDLER(CodeCreateEvent(tag, abstract_code, description))
}

void ExistingCodeLogger::LogCodeObjects() {
  Heap* heap = isolate_->heap();
  HeapIterator iterator(heap);
  DisallowHeapAllocation no_gc;
  for (HeapObject obj = iterator.next(); !obj.is_null();
       obj = iterator.next()) {
    if (obj->IsCode()) LogCodeObject(obj);
    if (obj->IsBytecodeArray()) LogCodeObject(obj);
  }
}

void ExistingCodeLogger::LogCompiledFunctions() {
  Heap* heap = isolate_->heap();
  HandleScope scope(isolate_);
  const int compiled_funcs_count =
      EnumerateCompiledFunctions(heap, nullptr, nullptr);
  ScopedVector<Handle<SharedFunctionInfo>> sfis(compiled_funcs_count);
  ScopedVector<Handle<AbstractCode>> code_objects(compiled_funcs_count);
  EnumerateCompiledFunctions(heap, sfis.start(), code_objects.start());

  // During iteration, there can be heap allocation due to
  // GetScriptLineNumber call.
  for (int i = 0; i < compiled_funcs_count; ++i) {
    if (sfis[i]->function_data()->IsInterpreterData()) {
      LogExistingFunction(
          sfis[i],
          Handle<AbstractCode>(
              AbstractCode::cast(sfis[i]->InterpreterTrampoline()), isolate_),
          CodeEventListener::INTERPRETED_FUNCTION_TAG);
    }
    if (code_objects[i].is_identical_to(BUILTIN_CODE(isolate_, CompileLazy)))
      continue;
    LogExistingFunction(sfis[i], code_objects[i]);
  }

  const int wasm_module_objects_count =
      EnumerateWasmModuleObjects(heap, nullptr);
  std::unique_ptr<Handle<WasmModuleObject>[]> module_objects(
      new Handle<WasmModuleObject>[wasm_module_objects_count]);
  EnumerateWasmModuleObjects(heap, module_objects.get());
  for (int i = 0; i < wasm_module_objects_count; ++i) {
    module_objects[i]->native_module()->LogWasmCodes(isolate_);
  }
}

void ExistingCodeLogger::LogExistingFunction(
    Handle<SharedFunctionInfo> shared, Handle<AbstractCode> code,
    CodeEventListener::LogEventsAndTags tag) {
  if (shared->script()->IsScript()) {
    Handle<Script> script(Script::cast(shared->script()), isolate_);
    int line_num = Script::GetLineNumber(script, shared->StartPosition()) + 1;
    int column_num =
        Script::GetColumnNumber(script, shared->StartPosition()) + 1;
    if (script->name()->IsString()) {
      Handle<String> script_name(String::cast(script->name()), isolate_);
      if (line_num > 0) {
        CALL_CODE_EVENT_HANDLER(
            CodeCreateEvent(Logger::ToNativeByScript(tag, *script), *code,
                            *shared, *script_name, line_num, column_num))
      } else {
        // Can't distinguish eval and script here, so always use Script.
        CALL_CODE_EVENT_HANDLER(CodeCreateEvent(
            Logger::ToNativeByScript(CodeEventListener::SCRIPT_TAG, *script),
            *code, *shared, *script_name))
      }
    } else {
      CALL_CODE_EVENT_HANDLER(CodeCreateEvent(
          Logger::ToNativeByScript(tag, *script), *code, *shared,
          ReadOnlyRoots(isolate_).empty_string(), line_num, column_num))
    }
  } else if (shared->IsApiFunction()) {
    // API function.
    FunctionTemplateInfo fun_data = shared->get_api_func_data();
    Object raw_call_data = fun_data->call_code();
    if (!raw_call_data->IsUndefined(isolate_)) {
      CallHandlerInfo call_data = CallHandlerInfo::cast(raw_call_data);
      Object callback_obj = call_data->callback();
      Address entry_point = v8::ToCData<Address>(callback_obj);
#if USES_FUNCTION_DESCRIPTORS
      entry_point = *FUNCTION_ENTRYPOINT_ADDRESS(entry_point);
#endif
      CALL_CODE_EVENT_HANDLER(CallbackEvent(shared->DebugName(), entry_point))
    }
  }
}

#undef CALL_CODE_EVENT_HANDLER

}  // namespace internal
}  // namespace v8
