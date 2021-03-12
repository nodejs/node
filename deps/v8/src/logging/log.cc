// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/logging/log.h"

#include <atomic>
#include <cstdarg>
#include <memory>
#include <sstream>

#include "src/api/api-inl.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/builtins/profile-data-reader.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/source-position-table.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/perf-jit.h"
#include "src/execution/isolate.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/combined-heap.h"
#include "src/heap/heap-inl.h"
#include "src/init/bootstrapper.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/libsampler/sampler.h"
#include "src/logging/counters.h"
#include "src/logging/log-inl.h"
#include "src/logging/log-utils.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/code-kind.h"
#include "src/objects/code.h"
#include "src/profiler/tick-sample.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/strings/string-stream.h"
#include "src/strings/unicode-inl.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/utils/memcopy.h"
#include "src/utils/version.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-objects-inl.h"

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
  if (shared.optimization_disabled() &&
      code.kind() == CodeKind::INTERPRETED_FUNCTION) {
    return "";
  }
  return CodeKindToMarker(code.kind());
}

static const char* ComputeMarker(const wasm::WasmCode* code) {
  switch (code->kind()) {
    case wasm::WasmCode::kFunction:
      return code->is_liftoff() ? "" : "*";
    default:
      return "";
  }
}

class CodeEventLogger::NameBuffer {
 public:
  NameBuffer() { Reset(); }

  void Reset() { utf8_pos_ = 0; }

  void Init(LogEventsAndTags tag) {
    Reset();
    AppendBytes(kLogEventsNames[tag]);
    AppendByte(':');
  }

  void AppendName(Name name) {
    if (name.IsString()) {
      AppendString(String::cast(name));
    } else {
      Symbol symbol = Symbol::cast(name);
      AppendBytes("symbol(");
      if (!symbol.description().IsUndefined()) {
        AppendBytes("\"");
        AppendString(String::cast(symbol.description()));
        AppendBytes("\" ");
      }
      AppendBytes("hash ");
      AppendHex(symbol.hash());
      AppendByte(')');
    }
  }

  void AppendString(String str) {
    if (str.is_null()) return;
    int length = 0;
    std::unique_ptr<char[]> c_str =
        str.ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL, &length);
    AppendBytes(c_str.get(), length);
  }

  void AppendBytes(const char* bytes, int size) {
    size = std::min(size, kUtf8BufferSize - utf8_pos_);
    MemCopy(utf8_buffer_ + utf8_pos_, bytes, size);
    utf8_pos_ += size;
  }

  void AppendBytes(const char* bytes) {
    size_t len = strlen(bytes);
    DCHECK_GE(kMaxInt, len);
    AppendBytes(bytes, static_cast<int>(len));
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
    : isolate_(isolate), name_buffer_(std::make_unique<NameBuffer>()) {}

CodeEventLogger::~CodeEventLogger() = default;

void CodeEventLogger::CodeCreateEvent(LogEventsAndTags tag,
                                      Handle<AbstractCode> code,
                                      const char* comment) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(comment);
  LogRecordedBuffer(code, MaybeHandle<SharedFunctionInfo>(),
                    name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(LogEventsAndTags tag,
                                      Handle<AbstractCode> code,
                                      Handle<Name> name) {
  name_buffer_->Init(tag);
  name_buffer_->AppendName(*name);
  LogRecordedBuffer(code, MaybeHandle<SharedFunctionInfo>(),
                    name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(LogEventsAndTags tag,
                                      Handle<AbstractCode> code,
                                      Handle<SharedFunctionInfo> shared,
                                      Handle<Name> script_name) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(ComputeMarker(*shared, *code));
  name_buffer_->AppendByte(' ');
  name_buffer_->AppendName(*script_name);
  LogRecordedBuffer(code, shared, name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(LogEventsAndTags tag,
                                      Handle<AbstractCode> code,
                                      Handle<SharedFunctionInfo> shared,
                                      Handle<Name> script_name, int line,
                                      int column) {
  name_buffer_->Init(tag);
  name_buffer_->AppendBytes(ComputeMarker(*shared, *code));
  name_buffer_->AppendBytes(shared->DebugNameCStr().get());
  name_buffer_->AppendByte(' ');
  if (script_name->IsString()) {
    name_buffer_->AppendString(String::cast(*script_name));
  } else {
    name_buffer_->AppendBytes("symbol(hash ");
    name_buffer_->AppendHex(Name::cast(*script_name).hash());
    name_buffer_->AppendByte(')');
  }
  name_buffer_->AppendByte(':');
  name_buffer_->AppendInt(line);
  LogRecordedBuffer(code, shared, name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::CodeCreateEvent(LogEventsAndTags tag,
                                      const wasm::WasmCode* code,
                                      wasm::WasmName name,
                                      const char* source_url,
                                      int /*code_offset*/, int /*script_id*/) {
  name_buffer_->Init(tag);
  DCHECK(!name.empty());
  name_buffer_->AppendBytes(name.begin(), name.length());
  name_buffer_->AppendByte('-');
  if (code->IsAnonymous()) {
    name_buffer_->AppendBytes("<anonymous>");
  } else {
    name_buffer_->AppendInt(code->index());
  }
  name_buffer_->AppendByte('-');
  name_buffer_->AppendBytes(ExecutionTierToString(code->tier()));
  LogRecordedBuffer(code, name_buffer_->get(), name_buffer_->size());
}

void CodeEventLogger::RegExpCodeCreateEvent(Handle<AbstractCode> code,
                                            Handle<String> source) {
  name_buffer_->Init(CodeEventListener::REG_EXP_TAG);
  name_buffer_->AppendString(*source);
  LogRecordedBuffer(code, MaybeHandle<SharedFunctionInfo>(),
                    name_buffer_->get(), name_buffer_->size());
}

// Linux perf tool logging support.
#if V8_OS_LINUX
class PerfBasicLogger : public CodeEventLogger {
 public:
  explicit PerfBasicLogger(Isolate* isolate);
  ~PerfBasicLogger() override;

  void CodeMoveEvent(AbstractCode from, AbstractCode to) override {}
  void CodeDisableOptEvent(Handle<AbstractCode> code,
                           Handle<SharedFunctionInfo> shared) override {}

 private:
  void LogRecordedBuffer(Handle<AbstractCode> code,
                         MaybeHandle<SharedFunctionInfo> maybe_shared,
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
  int size = SNPrintF(perf_dump_name, kFilenameFormatString,
                      base::OS::GetCurrentProcessId());
  CHECK_NE(size, -1);
  perf_output_handle_ =
      base::OS::FOpen(perf_dump_name.begin(), base::OS::LogFileOpenMode);
  CHECK_NOT_NULL(perf_output_handle_);
  setvbuf(perf_output_handle_, nullptr, _IOLBF, 0);
}

PerfBasicLogger::~PerfBasicLogger() {
  base::Fclose(perf_output_handle_);
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

void PerfBasicLogger::LogRecordedBuffer(Handle<AbstractCode> code,
                                        MaybeHandle<SharedFunctionInfo>,
                                        const char* name, int length) {
  if (FLAG_perf_basic_prof_only_functions &&
      CodeKindIsBuiltinOrJSFunction(code->kind())) {
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
#endif  // V8_OS_LINUX

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

void ExternalCodeEventListener::CodeCreateEvent(LogEventsAndTags tag,
                                                Handle<AbstractCode> code,
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

void ExternalCodeEventListener::CodeCreateEvent(LogEventsAndTags tag,
                                                Handle<AbstractCode> code,
                                                Handle<Name> name) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate_, name).ToHandleChecked();

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
    LogEventsAndTags tag, Handle<AbstractCode> code,
    Handle<SharedFunctionInfo> shared, Handle<Name> name) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate_, name).ToHandleChecked();

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
    LogEventsAndTags tag, Handle<AbstractCode> code,
    Handle<SharedFunctionInfo> shared, Handle<Name> source, int line,
    int column) {
  Handle<String> name_string =
      Name::ToFunctionName(isolate_, handle(shared->Name(), isolate_))
          .ToHandleChecked();
  Handle<String> source_string =
      Name::ToFunctionName(isolate_, source).ToHandleChecked();

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

void ExternalCodeEventListener::CodeCreateEvent(
    LogEventsAndTags tag, const wasm::WasmCode* code, wasm::WasmName name,
    const char* source_url, int code_offset, int script_id) {
  // TODO(mmarchini): handle later
}

void ExternalCodeEventListener::RegExpCodeCreateEvent(Handle<AbstractCode> code,
                                                      Handle<String> source) {
  CodeEvent code_event;
  code_event.code_start_address =
      static_cast<uintptr_t>(code->InstructionStart());
  code_event.code_size = static_cast<size_t>(code->InstructionSize());
  code_event.function_name = source;
  code_event.script_name = isolate_->factory()->empty_string();
  code_event.script_line = 0;
  code_event.script_column = 0;
  code_event.code_type = GetCodeEventTypeForTag(CodeEventListener::REG_EXP_TAG);
  code_event.comment = "";

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

void ExternalCodeEventListener::CodeMoveEvent(AbstractCode from,
                                              AbstractCode to) {
  CodeEvent code_event;
  code_event.previous_code_start_address =
      static_cast<uintptr_t>(from.InstructionStart());
  code_event.code_start_address = static_cast<uintptr_t>(to.InstructionStart());
  code_event.code_size = static_cast<size_t>(to.InstructionSize());
  code_event.function_name = isolate_->factory()->empty_string();
  code_event.script_name = isolate_->factory()->empty_string();
  code_event.script_line = 0;
  code_event.script_column = 0;
  code_event.code_type = v8::CodeEventType::kRelocationType;
  code_event.comment = "";

  code_event_handler_->Handle(reinterpret_cast<v8::CodeEvent*>(&code_event));
}

// Low-level logging support.
class LowLevelLogger : public CodeEventLogger {
 public:
  LowLevelLogger(Isolate* isolate, const char* file_name);
  ~LowLevelLogger() override;

  void CodeMoveEvent(AbstractCode from, AbstractCode to) override;
  void CodeDisableOptEvent(Handle<AbstractCode> code,
                           Handle<SharedFunctionInfo> shared) override {}
  void SnapshotPositionEvent(HeapObject obj, int pos);
  void CodeMovingGCEvent() override;

 private:
  void LogRecordedBuffer(Handle<AbstractCode> code,
                         MaybeHandle<SharedFunctionInfo> maybe_shared,
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
  MemCopy(ll_name.begin(), name, len);
  MemCopy(ll_name.begin() + len, kLogExt, sizeof(kLogExt));
  ll_output_handle_ =
      base::OS::FOpen(ll_name.begin(), base::OS::LogFileOpenMode);
  setvbuf(ll_output_handle_, nullptr, _IOLBF, 0);

  LogCodeInfo();
}

LowLevelLogger::~LowLevelLogger() {
  base::Fclose(ll_output_handle_);
  ll_output_handle_ = nullptr;
}

void LowLevelLogger::LogCodeInfo() {
#if V8_TARGET_ARCH_IA32
  const char arch[] = "ia32";
#elif V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_64_BIT
  const char arch[] = "x64";
#elif V8_TARGET_ARCH_ARM
  const char arch[] = "arm";
#elif V8_TARGET_ARCH_PPC
  const char arch[] = "ppc";
#elif V8_TARGET_ARCH_PPC64
  const char arch[] = "ppc64";
#elif V8_TARGET_ARCH_MIPS
  const char arch[] = "mips";
#elif V8_TARGET_ARCH_ARM64
  const char arch[] = "arm64";
#elif V8_TARGET_ARCH_S390
  const char arch[] = "s390";
#elif V8_TARGET_ARCH_RISCV64
  const char arch[] = "riscv64";
#else
  const char arch[] = "unknown";
#endif
  LogWriteBytes(arch, sizeof(arch));
}

void LowLevelLogger::LogRecordedBuffer(Handle<AbstractCode> code,
                                       MaybeHandle<SharedFunctionInfo>,
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
  event.from_address = from.InstructionStart();
  event.to_address = to.InstructionStart();
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
  void CodeDisableOptEvent(Handle<AbstractCode> code,
                           Handle<SharedFunctionInfo> shared) override {}
  void AddCodeLinePosInfoEvent(void* jit_handler_data, int pc_offset,
                               int position,
                               JitCodeEvent::PositionType position_type);

  void* StartCodePosInfoEvent();
  void EndCodePosInfoEvent(Address start_address, void* jit_handler_data);

 private:
  void LogRecordedBuffer(Handle<AbstractCode> code,
                         MaybeHandle<SharedFunctionInfo> maybe_shared,
                         const char* name, int length) override;
  void LogRecordedBuffer(const wasm::WasmCode* code, const char* name,
                         int length) override;

  JitCodeEventHandler code_event_handler_;
  base::Mutex logger_mutex_;
};

JitLogger::JitLogger(Isolate* isolate, JitCodeEventHandler code_event_handler)
    : CodeEventLogger(isolate), code_event_handler_(code_event_handler) {}

void JitLogger::LogRecordedBuffer(Handle<AbstractCode> code,
                                  MaybeHandle<SharedFunctionInfo> maybe_shared,
                                  const char* name, int length) {
  JitCodeEvent event;
  memset(static_cast<void*>(&event), 0, sizeof(event));
  event.type = JitCodeEvent::CODE_ADDED;
  event.code_start = reinterpret_cast<void*>(code->InstructionStart());
  event.code_type =
      code->IsCode() ? JitCodeEvent::JIT_CODE : JitCodeEvent::BYTE_CODE;
  event.code_len = code->InstructionSize();
  Handle<SharedFunctionInfo> shared;
  if (maybe_shared.ToHandle(&shared) && shared->script().IsScript()) {
    event.script = ToApiHandle<v8::UnboundScript>(shared);
  } else {
    event.script = Local<v8::UnboundScript>();
  }
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
  event.code_start = code->instructions().begin();
  event.code_len = code->instructions().length();
  event.name.str = name;
  event.name.len = length;
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  wasm::WasmModuleSourceMap* source_map =
      code->native_module()->GetWasmSourceMap();
  wasm::WireBytesRef code_ref =
      code->native_module()->module()->functions[code->index()].code;
  uint32_t code_offset = code_ref.offset();
  uint32_t code_end_offset = code_ref.end_offset();

  std::vector<v8::JitCodeEvent::line_info_t> mapping_info;
  std::string filename;
  std::unique_ptr<JitCodeEvent::wasm_source_info_t> wasm_source_info;

  if (source_map && source_map->IsValid() &&
      source_map->HasSource(code_offset, code_end_offset)) {
    size_t last_line_number = 0;

    for (SourcePositionTableIterator iterator(code->source_positions());
         !iterator.done(); iterator.Advance()) {
      uint32_t offset = iterator.source_position().ScriptOffset() + code_offset;
      if (!source_map->HasValidEntry(code_offset, offset)) continue;
      if (filename.empty()) {
        filename = source_map->GetFilename(offset);
      }
      mapping_info.push_back({static_cast<size_t>(iterator.code_offset()),
                              last_line_number, JitCodeEvent::POSITION});
      last_line_number = source_map->GetSourceLine(offset) + 1;
    }

    wasm_source_info = std::make_unique<JitCodeEvent::wasm_source_info_t>();
    wasm_source_info->filename = filename.c_str();
    wasm_source_info->filename_size = filename.size();
    wasm_source_info->line_number_table_size = mapping_info.size();
    wasm_source_info->line_number_table = mapping_info.data();

    event.wasm_source_info = wasm_source_info.get();
  }
  code_event_handler_(&event);
}

void JitLogger::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  base::MutexGuard guard(&logger_mutex_);

  JitCodeEvent event;
  event.type = JitCodeEvent::CODE_MOVED;
  event.code_type =
      from.IsCode() ? JitCodeEvent::JIT_CODE : JitCodeEvent::BYTE_CODE;
  event.code_start = reinterpret_cast<void*>(from.InstructionStart());
  event.code_len = from.InstructionSize();
  event.new_code_start = reinterpret_cast<void*>(to.InstructionStart());
  event.isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  code_event_handler_(&event);
}

void JitLogger::AddCodeLinePosInfoEvent(
    void* jit_handler_data, int pc_offset, int position,
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
class Profiler : public base::Thread {
 public:
  explicit Profiler(Isolate* isolate);
  void Engage();
  void Disengage();

  // Inserts collected profiling data into buffer.
  void Insert(TickSample* sample) {
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
  bool Remove(TickSample* sample) {
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
  TickSample buffer_[kBufferSize];  // Buffer storage.
  int head_;                        // Index to the buffer head.
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
class Ticker : public sampler::Sampler {
 public:
  Ticker(Isolate* isolate, int interval_microseconds)
      : sampler::Sampler(reinterpret_cast<v8::Isolate*>(isolate)),
        sampling_thread_(
            std::make_unique<SamplingThread>(this, interval_microseconds)),
        perThreadData_(isolate->FindPerThreadDataForThisThread()) {}

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
    if (v8::Locker::IsActive() && (!isolate->thread_manager()->IsLockedByThread(
                                       perThreadData_->thread_id()) ||
                                   perThreadData_->thread_state() != nullptr))
      return;
    TickSample sample;
    sample.Init(isolate, state, TickSample::kIncludeCEntryFrame, true);
    profiler_->Insert(&sample);
  }

 private:
  Profiler* profiler_ = nullptr;
  std::unique_ptr<SamplingThread> sampling_thread_;
  Isolate::PerIsolateThreadData* perThreadData_;
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
  CHECK(Start());

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
  TickSample sample;
  Insert(&sample);
  Join();

  LOG(isolate_, UncheckedStringEvent("profiler", "end"));
}

void Profiler::Run() {
  TickSample sample;
  bool overflow = Remove(&sample);
  while (base::Relaxed_Load(&running_)) {
    LOG(isolate_, TickEvent(&sample, overflow));
    overflow = Remove(&sample);
  }
}

//
// Logger class implementation.
//
#define MSG_BUILDER()                                                       \
  std::unique_ptr<Log::MessageBuilder> msg_ptr = log_->NewMessageBuilder(); \
  if (!msg_ptr) return;                                                     \
  Log::MessageBuilder& msg = *msg_ptr.get();

Logger::Logger(Isolate* isolate)
    : isolate_(isolate),
      is_logging_(false),
      is_initialized_(false),
      existing_code_logger_(isolate) {}

Logger::~Logger() = default;

const LogSeparator Logger::kNext = LogSeparator::kSeparator;

int64_t Logger::Time() {
  if (V8_UNLIKELY(FLAG_verify_predictable)) {
    return isolate_->heap()->MonotonicallyIncreasingTimeInMs() * 1000;
  }
  return timer_.Elapsed().InMicroseconds();
}

void Logger::AddCodeEventListener(CodeEventListener* listener) {
  bool result = isolate_->code_event_dispatcher()->AddListener(listener);
  CHECK(result);
}

void Logger::RemoveCodeEventListener(CodeEventListener* listener) {
  isolate_->code_event_dispatcher()->RemoveListener(listener);
}

void Logger::ProfilerBeginEvent() {
  MSG_BUILDER();
  msg << "profiler" << kNext << "begin" << kNext << FLAG_prof_sampling_interval;
  msg.WriteToLogFile();
}

void Logger::StringEvent(const char* name, const char* value) {
  if (FLAG_log) UncheckedStringEvent(name, value);
}

void Logger::UncheckedStringEvent(const char* name, const char* value) {
  MSG_BUILDER();
  msg << name << kNext << value;
  msg.WriteToLogFile();
}

void Logger::IntPtrTEvent(const char* name, intptr_t value) {
  if (!FLAG_log) return;
  MSG_BUILDER();
  msg << name << kNext;
  msg.AppendFormatString("%" V8PRIdPTR, value);
  msg.WriteToLogFile();
}

void Logger::HandleEvent(const char* name, Address* location) {
  if (!FLAG_log_handles) return;
  MSG_BUILDER();
  msg << name << kNext << reinterpret_cast<void*>(location);
  msg.WriteToLogFile();
}

void Logger::ApiSecurityCheck() {
  if (!FLAG_log_api) return;
  MSG_BUILDER();
  msg << "api" << kNext << "check-security";
  msg.WriteToLogFile();
}

void Logger::SharedLibraryEvent(const std::string& library_path,
                                uintptr_t start, uintptr_t end,
                                intptr_t aslr_slide) {
  if (!FLAG_prof_cpp) return;
  MSG_BUILDER();
  msg << "shared-library" << kNext << library_path.c_str() << kNext
      << reinterpret_cast<void*>(start) << kNext << reinterpret_cast<void*>(end)
      << kNext << aslr_slide;
  msg.WriteToLogFile();
}

void Logger::CurrentTimeEvent() {
  DCHECK(FLAG_log_internal_timer_events);
  MSG_BUILDER();
  msg << "current-time" << kNext << Time();
  msg.WriteToLogFile();
}

void Logger::TimerEvent(Logger::StartEnd se, const char* name) {
  MSG_BUILDER();
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
  msg << kNext << name << kNext << Time();
  msg.WriteToLogFile();
}

void Logger::BasicBlockCounterEvent(const char* name, int block_id,
                                    uint32_t count) {
  if (!FLAG_turbo_profiling_log_builtins) return;
  MSG_BUILDER();
  msg << ProfileDataFromFileConstants::kBlockCounterMarker << kNext << name
      << kNext << block_id << kNext << count;
  msg.WriteToLogFile();
}

void Logger::BuiltinHashEvent(const char* name, int hash) {
  if (!FLAG_turbo_profiling_log_builtins) return;
  MSG_BUILDER();
  msg << ProfileDataFromFileConstants::kBuiltinHashMarker << kNext << name
      << kNext << hash;
  msg.WriteToLogFile();
}

bool Logger::is_logging() {
  // Disable logging while the CPU profiler is running.
  if (isolate_->is_profiling()) return false;
  return is_logging_.load(std::memory_order_relaxed);
}

// Instantiate template methods.
#define V(TimerName, expose)                                           \
  template void TimerEventScope<TimerEvent##TimerName>::LogTimerEvent( \
      Logger::StartEnd se);
TIMER_EVENTS_LIST(V)
#undef V

void Logger::ApiNamedPropertyAccess(const char* tag, JSObject holder,
                                    Object property_name) {
  DCHECK(property_name.IsName());
  if (!FLAG_log_api) return;
  MSG_BUILDER();
  msg << "api" << kNext << tag << kNext << holder.class_name() << kNext
      << Name::cast(property_name);
  msg.WriteToLogFile();
}

void Logger::ApiIndexedPropertyAccess(const char* tag, JSObject holder,
                                      uint32_t index) {
  if (!FLAG_log_api) return;
  MSG_BUILDER();
  msg << "api" << kNext << tag << kNext << holder.class_name() << kNext
      << index;
  msg.WriteToLogFile();
}

void Logger::ApiObjectAccess(const char* tag, JSReceiver object) {
  if (!FLAG_log_api) return;
  MSG_BUILDER();
  msg << "api" << kNext << tag << kNext << object.class_name();
  msg.WriteToLogFile();
}

void Logger::ApiEntryCall(const char* name) {
  if (!FLAG_log_api) return;
  MSG_BUILDER();
  msg << "api" << kNext << name;
  msg.WriteToLogFile();
}

void Logger::NewEvent(const char* name, void* object, size_t size) {
  if (!FLAG_log) return;
  MSG_BUILDER();
  msg << "new" << kNext << name << kNext << object << kNext
      << static_cast<unsigned int>(size);
  msg.WriteToLogFile();
}

void Logger::DeleteEvent(const char* name, void* object) {
  if (!FLAG_log) return;
  MSG_BUILDER();
  msg << "delete" << kNext << name << kNext << object;
  msg.WriteToLogFile();
}

namespace {

void AppendCodeCreateHeader(
    Log::MessageBuilder& msg,  // NOLINT(runtime/references)
    CodeEventListener::LogEventsAndTags tag, CodeKind kind, uint8_t* address,
    int size, uint64_t time) {
  msg << kLogEventsNames[CodeEventListener::CODE_CREATION_EVENT]
      << Logger::kNext << kLogEventsNames[tag] << Logger::kNext
      << static_cast<int>(kind) << Logger::kNext << time << Logger::kNext
      << reinterpret_cast<void*>(address) << Logger::kNext << size
      << Logger::kNext;
}

void AppendCodeCreateHeader(
    Log::MessageBuilder& msg,  // NOLINT(runtime/references)
    CodeEventListener::LogEventsAndTags tag, AbstractCode code, uint64_t time) {
  AppendCodeCreateHeader(msg, tag, code.kind(),
                         reinterpret_cast<uint8_t*>(code.InstructionStart()),
                         code.InstructionSize(), time);
}

}  // namespace
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

void Logger::LogSourceCodeInformation(Handle<AbstractCode> code,
                                      Handle<SharedFunctionInfo> shared) {
  Object script_object = shared->script();
  if (!script_object.IsScript()) return;
  Script script = Script::cast(script_object);
  EnsureLogScriptSource(script);

  MSG_BUILDER();
  msg << "code-source-info" << Logger::kNext
      << reinterpret_cast<void*>(code->InstructionStart()) << Logger::kNext
      << script.id() << Logger::kNext << shared->StartPosition()
      << Logger::kNext << shared->EndPosition() << Logger::kNext;
  // TODO(v8:11429): Clean-up baseline-replated code in source position
  // iteration.
  bool hasInlined = false;
  if (code->kind() != CodeKind::BASELINE) {
    SourcePositionTableIterator iterator(code->source_position_table());
    for (; !iterator.done(); iterator.Advance()) {
      SourcePosition pos = iterator.source_position();
      msg << "C" << iterator.code_offset() << "O" << pos.ScriptOffset();
      if (pos.isInlined()) {
        msg << "I" << pos.InliningId();
        hasInlined = true;
      }
    }
  }
  msg << Logger::kNext;
  int maxInlinedId = -1;
  if (hasInlined) {
    PodArray<InliningPosition> inlining_positions =
        DeoptimizationData::cast(
            Handle<Code>::cast(code)->deoptimization_data())
            .InliningPositions();
    for (int i = 0; i < inlining_positions.length(); i++) {
      InliningPosition inlining_pos = inlining_positions.get(i);
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
  msg << Logger::kNext;
  if (hasInlined) {
    DeoptimizationData deopt_data = DeoptimizationData::cast(
        Handle<Code>::cast(code)->deoptimization_data());
    msg << std::hex;
    for (int i = 0; i <= maxInlinedId; i++) {
      msg << "S"
          << reinterpret_cast<void*>(
                 deopt_data.GetInlinedFunction(i).address());
    }
    msg << std::dec;
  }
  msg.WriteToLogFile();
}

void Logger::LogCodeDisassemble(Handle<AbstractCode> code) {
  if (!FLAG_log_code_disassemble) return;
  MSG_BUILDER();
  msg << "code-disassemble" << Logger::kNext
      << reinterpret_cast<void*>(code->InstructionStart()) << Logger::kNext
      << CodeKindToString(code->kind()) << Logger::kNext;
  {
    std::ostringstream stream;
    if (code->IsCode()) {
#ifdef ENABLE_DISASSEMBLER
      Code::cast(*code).Disassemble(nullptr, stream, isolate_);
#endif
    } else {
      BytecodeArray::cast(*code).Disassemble(stream);
    }
    std::string string = stream.str();
    msg.AppendString(string.c_str(), string.length());
  }
  msg.WriteToLogFile();
}

// Builtins and Bytecode handlers
void Logger::CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                             const char* name) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  {
    MSG_BUILDER();
    AppendCodeCreateHeader(msg, tag, *code, Time());
    msg << name;
    msg.WriteToLogFile();
  }
  LogCodeDisassemble(code);
}

void Logger::CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                             Handle<Name> name) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  {
    MSG_BUILDER();
    AppendCodeCreateHeader(msg, tag, *code, Time());
    msg << *name;
    msg.WriteToLogFile();
  }
  LogCodeDisassemble(code);
}

// Scripts
void Logger::CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                             Handle<SharedFunctionInfo> shared,
                             Handle<Name> script_name) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  if (*code == AbstractCode::cast(
                   isolate_->builtins()->builtin(Builtins::kCompileLazy))) {
    return;
  }
  {
    MSG_BUILDER();
    AppendCodeCreateHeader(msg, tag, *code, Time());
    msg << *script_name << kNext << reinterpret_cast<void*>(shared->address())
        << kNext << ComputeMarker(*shared, *code);
    msg.WriteToLogFile();
  }
  LogSourceCodeInformation(code, shared);
  LogCodeDisassemble(code);
}

// Functions
// Although, it is possible to extract source and line from
// the SharedFunctionInfo object, we left it to caller
// to leave logging functions free from heap allocations.
void Logger::CodeCreateEvent(LogEventsAndTags tag, Handle<AbstractCode> code,
                             Handle<SharedFunctionInfo> shared,
                             Handle<Name> script_name, int line, int column) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  {
    MSG_BUILDER();
    AppendCodeCreateHeader(msg, tag, *code, Time());
    msg << shared->DebugNameCStr().get() << " " << *script_name << ":" << line
        << ":" << column << kNext << reinterpret_cast<void*>(shared->address())
        << kNext << ComputeMarker(*shared, *code);

    msg.WriteToLogFile();
  }
  LogSourceCodeInformation(code, shared);
  LogCodeDisassemble(code);
}

void Logger::CodeCreateEvent(LogEventsAndTags tag, const wasm::WasmCode* code,
                             wasm::WasmName name, const char* /*source_url*/,
                             int /*code_offset*/, int /*script_id*/) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  MSG_BUILDER();
  AppendCodeCreateHeader(msg, tag, CodeKind::WASM_FUNCTION,
                         code->instructions().begin(),
                         code->instructions().length(), Time());
  DCHECK(!name.empty());
  msg.AppendString(name);

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

void Logger::CallbackEventInternal(const char* prefix, Handle<Name> name,
                                   Address entry_point) {
  if (!FLAG_log_code) return;
  MSG_BUILDER();
  msg << kLogEventsNames[CodeEventListener::CODE_CREATION_EVENT] << kNext
      << kLogEventsNames[CodeEventListener::CALLBACK_TAG] << kNext << -2
      << kNext << Time() << kNext << reinterpret_cast<void*>(entry_point)
      << kNext << 1 << kNext << prefix << *name;
  msg.WriteToLogFile();
}

void Logger::CallbackEvent(Handle<Name> name, Address entry_point) {
  CallbackEventInternal("", name, entry_point);
}

void Logger::GetterCallbackEvent(Handle<Name> name, Address entry_point) {
  CallbackEventInternal("get ", name, entry_point);
}

void Logger::SetterCallbackEvent(Handle<Name> name, Address entry_point) {
  CallbackEventInternal("set ", name, entry_point);
}

void Logger::RegExpCodeCreateEvent(Handle<AbstractCode> code,
                                   Handle<String> source) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  MSG_BUILDER();
  AppendCodeCreateHeader(msg, CodeEventListener::REG_EXP_TAG, *code, Time());
  msg << *source;
  msg.WriteToLogFile();
}

void Logger::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  if (!is_listening_to_code_events()) return;
  MoveEventInternal(CodeEventListener::CODE_MOVE_EVENT, from.InstructionStart(),
                    to.InstructionStart());
}

void Logger::SharedFunctionInfoMoveEvent(Address from, Address to) {
  if (!is_listening_to_code_events()) return;
  MoveEventInternal(CodeEventListener::SHARED_FUNC_MOVE_EVENT, from, to);
}

void Logger::CodeMovingGCEvent() {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_ll_prof) return;
  base::OS::SignalCodeMovingGC();
}

void Logger::CodeDisableOptEvent(Handle<AbstractCode> code,
                                 Handle<SharedFunctionInfo> shared) {
  if (!is_listening_to_code_events()) return;
  if (!FLAG_log_code) return;
  MSG_BUILDER();
  msg << kLogEventsNames[CodeEventListener::CODE_DISABLE_OPT_EVENT] << kNext
      << shared->DebugNameCStr().get() << kNext
      << GetBailoutReason(shared->disable_optimization_reason());
  msg.WriteToLogFile();
}

void Logger::ProcessDeoptEvent(Handle<Code> code, SourcePosition position,
                               const char* kind, const char* reason) {
  MSG_BUILDER();
  msg << "code-deopt" << kNext << Time() << kNext << code->CodeSize() << kNext
      << reinterpret_cast<void*>(code->InstructionStart());

  std::ostringstream deopt_location;
  int inlining_id = -1;
  int script_offset = -1;
  if (position.IsKnown()) {
    position.Print(deopt_location, *code);
    inlining_id = position.InliningId();
    script_offset = position.ScriptOffset();
  } else {
    deopt_location << "<unknown>";
  }
  msg << kNext << inlining_id << kNext << script_offset << kNext;
  msg << kind << kNext;
  msg << deopt_location.str().c_str() << kNext << reason;
  msg.WriteToLogFile();
}

void Logger::CodeDeoptEvent(Handle<Code> code, DeoptimizeKind kind, Address pc,
                            int fp_to_sp_delta, bool reuse_code) {
  if (!is_logging() || !FLAG_log_deopt) return;
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(*code, pc);
  ProcessDeoptEvent(code, info.position,
                    Deoptimizer::MessageFor(kind, reuse_code),
                    DeoptimizeReasonToString(info.deopt_reason));
}

void Logger::CodeDependencyChangeEvent(Handle<Code> code,
                                       Handle<SharedFunctionInfo> sfi,
                                       const char* reason) {
  if (!is_logging() || !FLAG_log_deopt) return;
  SourcePosition position(sfi->StartPosition(), -1);
  ProcessDeoptEvent(code, position, "dependency-change", reason);
}

namespace {

void CodeLinePosEvent(
    JitLogger& jit_logger, Address code_start,
    SourcePositionTableIterator& iter) {  // NOLINT(runtime/references)
  void* jit_handler_data = jit_logger.StartCodePosInfoEvent();
  for (; !iter.done(); iter.Advance()) {
    if (iter.is_statement()) {
      jit_logger.AddCodeLinePosInfoEvent(jit_handler_data, iter.code_offset(),
                                         iter.source_position().ScriptOffset(),
                                         JitCodeEvent::STATEMENT_POSITION);
    }
    jit_logger.AddCodeLinePosInfoEvent(jit_handler_data, iter.code_offset(),
                                       iter.source_position().ScriptOffset(),
                                       JitCodeEvent::POSITION);
  }
  jit_logger.EndCodePosInfoEvent(code_start, jit_handler_data);
}

}  // namespace

void Logger::CodeLinePosInfoRecordEvent(Address code_start,
                                        ByteArray source_position_table) {
  if (!jit_logger_) return;
  SourcePositionTableIterator iter(source_position_table);
  CodeLinePosEvent(*jit_logger_, code_start, iter);
}

void Logger::CodeLinePosInfoRecordEvent(
    Address code_start, Vector<const byte> source_position_table) {
  if (!jit_logger_) return;
  SourcePositionTableIterator iter(source_position_table);
  CodeLinePosEvent(*jit_logger_, code_start, iter);
}

void Logger::CodeNameEvent(Address addr, int pos, const char* code_name) {
  if (code_name == nullptr) return;  // Not a code object.
  if (!is_listening_to_code_events()) return;
  MSG_BUILDER();
  msg << kLogEventsNames[CodeEventListener::SNAPSHOT_CODE_NAME_EVENT] << kNext
      << pos << kNext << code_name;
  msg.WriteToLogFile();
}

void Logger::MoveEventInternal(LogEventsAndTags event, Address from,
                               Address to) {
  if (!FLAG_log_code) return;
  MSG_BUILDER();
  msg << kLogEventsNames[event] << kNext << reinterpret_cast<void*>(from)
      << kNext << reinterpret_cast<void*>(to);
  msg.WriteToLogFile();
}

void Logger::ResourceEvent(const char* name, const char* tag) {
  if (!FLAG_log) return;
  MSG_BUILDER();
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
  if (!FLAG_log_suspect) return;
  MSG_BUILDER();
  String class_name = obj.IsJSObject() ? JSObject::cast(obj).class_name()
                                       : ReadOnlyRoots(isolate_).empty_string();
  msg << "suspect-read" << kNext << class_name << kNext << name;
  msg.WriteToLogFile();
}

namespace {
void AppendFunctionMessage(
    Log::MessageBuilder& msg,  // NOLINT(runtime/references)
    const char* reason, int script_id, double time_delta, int start_position,
    int end_position, uint64_t time) {
  msg << "function" << Logger::kNext << reason << Logger::kNext << script_id
      << Logger::kNext << start_position << Logger::kNext << end_position
      << Logger::kNext;
  if (V8_UNLIKELY(FLAG_predictable)) {
    msg << 0.1;
  } else {
    msg << time_delta;
  }
  msg << Logger::kNext << time << Logger::kNext;
}
}  // namespace

void Logger::FunctionEvent(const char* reason, int script_id, double time_delta,
                           int start_position, int end_position,
                           String function_name) {
  if (!FLAG_log_function_events) return;
  MSG_BUILDER();
  AppendFunctionMessage(msg, reason, script_id, time_delta, start_position,
                        end_position, Time());
  if (!function_name.is_null()) msg << function_name;
  msg.WriteToLogFile();
}

void Logger::FunctionEvent(const char* reason, int script_id, double time_delta,
                           int start_position, int end_position,
                           const char* function_name,
                           size_t function_name_length, bool is_one_byte) {
  if (!FLAG_log_function_events) return;
  MSG_BUILDER();
  AppendFunctionMessage(msg, reason, script_id, time_delta, start_position,
                        end_position, Time());
  if (function_name_length > 0) {
    msg.AppendString(function_name, function_name_length, is_one_byte);
  }
  msg.WriteToLogFile();
}

void Logger::CompilationCacheEvent(const char* action, const char* cache_type,
                                   SharedFunctionInfo sfi) {
  if (!FLAG_log_function_events) return;
  MSG_BUILDER();
  int script_id = -1;
  if (sfi.script().IsScript()) {
    script_id = Script::cast(sfi.script()).id();
  }
  msg << "compilation-cache" << Logger::kNext << action << Logger::kNext
      << cache_type << Logger::kNext << script_id << Logger::kNext
      << sfi.StartPosition() << Logger::kNext << sfi.EndPosition()
      << Logger::kNext << Time();
  msg.WriteToLogFile();
}

void Logger::ScriptEvent(ScriptEventType type, int script_id) {
  if (!FLAG_log_function_events) return;
  MSG_BUILDER();
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
  msg << Logger::kNext << script_id << Logger::kNext << Time();
  msg.WriteToLogFile();
}

void Logger::ScriptDetails(Script script) {
  if (!FLAG_log_function_events) return;
  {
    MSG_BUILDER();
    msg << "script-details" << Logger::kNext << script.id() << Logger::kNext;
    if (script.name().IsString()) {
      msg << String::cast(script.name());
    }
    msg << Logger::kNext << script.line_offset() << Logger::kNext
        << script.column_offset() << Logger::kNext;
    if (script.source_mapping_url().IsString()) {
      msg << String::cast(script.source_mapping_url());
    }
    msg.WriteToLogFile();
  }
  EnsureLogScriptSource(script);
}

bool Logger::EnsureLogScriptSource(Script script) {
  // Make sure the script is written to the log file.
  int script_id = script.id();
  if (logged_source_code_.find(script_id) != logged_source_code_.end()) {
    return true;
  }
  // This script has not been logged yet.
  logged_source_code_.insert(script_id);
  Object source_object = script.source();
  if (!source_object.IsString()) return false;

  std::unique_ptr<Log::MessageBuilder> msg_ptr = log_->NewMessageBuilder();
  if (!msg_ptr) return false;
  Log::MessageBuilder& msg = *msg_ptr.get();

  String source_code = String::cast(source_object);
  msg << "script-source" << kNext << script_id << kNext;

  // Log the script name.
  if (script.name().IsString()) {
    msg << String::cast(script.name()) << kNext;
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
  MSG_BUILDER();
  msg << "active-runtime-timer" << kNext << counter->name();
  msg.WriteToLogFile();
}

void Logger::TickEvent(TickSample* sample, bool overflow) {
  if (!FLAG_prof_cpp) return;
  if (V8_UNLIKELY(TracingFlags::runtime_stats.load(std::memory_order_relaxed) ==
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE)) {
    RuntimeCallTimerEvent();
  }
  MSG_BUILDER();
  msg << kLogEventsNames[CodeEventListener::TICK_EVENT] << kNext
      << reinterpret_cast<void*>(sample->pc) << kNext << Time();
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

void Logger::ICEvent(const char* type, bool keyed, Handle<Map> map,
                     Handle<Object> key, char old_state, char new_state,
                     const char* modifier, const char* slow_stub_reason) {
  if (!FLAG_log_ic) return;
  MSG_BUILDER();
  if (keyed) msg << "Keyed";
  int line;
  int column;
  Address pc = isolate_->GetAbstractPC(&line, &column);
  msg << type << kNext << reinterpret_cast<void*>(pc) << kNext << Time()
      << kNext << line << kNext << column << kNext << old_state << kNext
      << new_state << kNext
      << AsHex::Address(map.is_null() ? kNullAddress : map->ptr()) << kNext;
  if (key->IsSmi()) {
    msg << Smi::ToInt(*key);
  } else if (key->IsNumber()) {
    msg << key->Number();
  } else if (key->IsName()) {
    msg << Name::cast(*key);
  }
  msg << kNext << modifier << kNext;
  if (slow_stub_reason != nullptr) {
    msg << slow_stub_reason;
  }
  msg.WriteToLogFile();
}

void Logger::MapEvent(const char* type, Handle<Map> from, Handle<Map> to,
                      const char* reason, Handle<HeapObject> name_or_sfi) {
  if (!FLAG_log_maps) return;
  if (!to.is_null()) MapDetails(*to);
  int line = -1;
  int column = -1;
  Address pc = 0;

  if (!isolate_->bootstrapper()->IsActive()) {
    pc = isolate_->GetAbstractPC(&line, &column);
  }
  MSG_BUILDER();
  msg << "map" << kNext << type << kNext << Time() << kNext
      << AsHex::Address(from.is_null() ? kNullAddress : from->ptr()) << kNext
      << AsHex::Address(to.is_null() ? kNullAddress : to->ptr()) << kNext
      << AsHex::Address(pc) << kNext << line << kNext << column << kNext
      << reason << kNext;

  if (!name_or_sfi.is_null()) {
    if (name_or_sfi->IsName()) {
      msg << Name::cast(*name_or_sfi);
    } else if (name_or_sfi->IsSharedFunctionInfo()) {
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(*name_or_sfi);
      msg << sfi.DebugNameCStr().get();
#if V8_SFI_HAS_UNIQUE_ID
      msg << " " << sfi.unique_id();
#endif  // V8_SFI_HAS_UNIQUE_ID
    }
  }
  msg.WriteToLogFile();
}

void Logger::MapCreate(Map map) {
  if (!FLAG_log_maps) return;
  DisallowGarbageCollection no_gc;
  MSG_BUILDER();
  msg << "map-create" << kNext << Time() << kNext << AsHex::Address(map.ptr());
  msg.WriteToLogFile();
}

void Logger::MapDetails(Map map) {
  if (!FLAG_log_maps) return;
  DisallowGarbageCollection no_gc;
  MSG_BUILDER();
  msg << "map-details" << kNext << Time() << kNext << AsHex::Address(map.ptr())
      << kNext;
  if (FLAG_log_maps_details) {
    std::ostringstream buffer;
    map.PrintMapDetails(buffer);
    msg << buffer.str().c_str();
  }
  msg.WriteToLogFile();
}

static std::vector<std::pair<Handle<SharedFunctionInfo>, Handle<AbstractCode>>>
EnumerateCompiledFunctions(Heap* heap) {
  HeapObjectIterator iterator(heap);
  DisallowGarbageCollection no_gc;
  std::vector<std::pair<Handle<SharedFunctionInfo>, Handle<AbstractCode>>>
      compiled_funcs;
  Isolate* isolate = heap->isolate();

  // Iterate the heap to find JSFunctions and record their optimized code.
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (obj.IsSharedFunctionInfo()) {
      SharedFunctionInfo sfi = SharedFunctionInfo::cast(obj);
      if (sfi.is_compiled() && !sfi.IsInterpreted()) {
        compiled_funcs.emplace_back(
            handle(sfi, isolate),
            handle(AbstractCode::cast(sfi.abstract_code(isolate)), isolate));
      }
    } else if (obj.IsJSFunction()) {
      // Given that we no longer iterate over all optimized JSFunctions, we need
      // to take care of this here.
      JSFunction function = JSFunction::cast(obj);
      // TODO(jarin) This leaves out deoptimized code that might still be on the
      // stack. Also note that we will not log optimized code objects that are
      // only on a type feedback vector. We should make this mroe precise.
      if (function.HasAttachedOptimizedCode() &&
          Script::cast(function.shared().script()).HasValidSource()) {
        compiled_funcs.emplace_back(
            handle(function.shared(), isolate),
            handle(AbstractCode::cast(function.code()), isolate));
      }
    }
  }

  Script::Iterator script_iterator(heap->isolate());
  for (Script script = script_iterator.Next(); !script.is_null();
       script = script_iterator.Next()) {
    if (!script.HasValidSource()) continue;

    SharedFunctionInfo::ScriptIterator sfi_iterator(heap->isolate(), script);
    for (SharedFunctionInfo sfi = sfi_iterator.Next(); !sfi.is_null();
         sfi = sfi_iterator.Next()) {
      if (sfi.is_compiled()) {
        compiled_funcs.emplace_back(
            handle(sfi, isolate),
            handle(AbstractCode::cast(sfi.abstract_code(isolate)), isolate));
      }
    }
  }

  return compiled_funcs;
}

static std::vector<Handle<WasmModuleObject>> EnumerateWasmModuleObjects(
    Heap* heap) {
  HeapObjectIterator iterator(heap);
  DisallowGarbageCollection no_gc;
  std::vector<Handle<WasmModuleObject>> module_objects;

  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (obj.IsWasmModuleObject()) {
      WasmModuleObject module = WasmModuleObject::cast(obj);
      module_objects.emplace_back(module, Isolate::FromHeap(heap));
    }
  }
  return module_objects;
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
  HeapObjectIterator iterator(heap);
  DisallowGarbageCollection no_gc;
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (!obj.IsAccessorInfo()) continue;
    AccessorInfo ai = AccessorInfo::cast(obj);
    if (!ai.name().IsName()) continue;
    Address getter_entry = v8::ToCData<Address>(ai.getter());
    HandleScope scope(isolate_);
    Handle<Name> name(Name::cast(ai.name()), isolate_);
    if (getter_entry != 0) {
#if USES_FUNCTION_DESCRIPTORS
      getter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(getter_entry);
#endif
      PROFILE(isolate_, GetterCallbackEvent(name, getter_entry));
    }
    Address setter_entry = v8::ToCData<Address>(ai.setter());
    if (setter_entry != 0) {
#if USES_FUNCTION_DESCRIPTORS
      setter_entry = *FUNCTION_ENTRYPOINT_ADDRESS(setter_entry);
#endif
      PROFILE(isolate_, SetterCallbackEvent(name, setter_entry));
    }
  }
}

void Logger::LogAllMaps() {
  DisallowGarbageCollection no_gc;
  Heap* heap = isolate_->heap();
  CombinedHeapObjectIterator iterator(heap);
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (!obj.IsMap()) continue;
    Map map = Map::cast(obj);
    MapCreate(map);
    MapDetails(map);
  }
}

static void AddIsolateIdIfNeeded(std::ostream& os, Isolate* isolate) {
  if (!FLAG_logfile_per_isolate) return;
  os << "isolate-" << isolate << "-" << base::OS::GetCurrentProcessId() << "-";
}

static void PrepareLogFileName(std::ostream& os, Isolate* isolate,
                               const char* file_name) {
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
  PrepareLogFileName(log_file_name, isolate, FLAG_logfile);
  log_ = std::make_unique<Log>(this, log_file_name.str());

#if V8_OS_LINUX
  if (FLAG_perf_basic_prof) {
    perf_basic_logger_ = std::make_unique<PerfBasicLogger>(isolate);
    AddCodeEventListener(perf_basic_logger_.get());
  }

  if (FLAG_perf_prof) {
    perf_jit_logger_ = std::make_unique<PerfJitLogger>(isolate);
    AddCodeEventListener(perf_jit_logger_.get());
  }
#else
  static_assert(
      !FLAG_perf_prof,
      "--perf-prof should be statically disabled on non-Linux platforms");
  static_assert(
      !FLAG_perf_basic_prof,
      "--perf-basic-prof should be statically disabled on non-Linux platforms");
#endif

  if (FLAG_ll_prof) {
    ll_logger_ =
        std::make_unique<LowLevelLogger>(isolate, log_file_name.str().c_str());
    AddCodeEventListener(ll_logger_.get());
  }
  ticker_ = std::make_unique<Ticker>(isolate, FLAG_prof_sampling_interval);
  if (FLAG_log) UpdateIsLogging(true);
  timer_.Start();
  if (FLAG_prof_cpp) {
    CHECK(FLAG_log);
    CHECK(is_logging());
    profiler_ = std::make_unique<Profiler>(isolate);
    profiler_->Engage();
  }
  if (is_logging_) AddCodeEventListener(this);
  return true;
}

void Logger::SetCodeEventHandler(uint32_t options,
                                 JitCodeEventHandler event_handler) {
  if (jit_logger_) {
    RemoveCodeEventListener(jit_logger_.get());
    jit_logger_.reset();
  }

  if (event_handler) {
    if (isolate_->wasm_engine() != nullptr) {
      isolate_->wasm_engine()->EnableCodeLogging(isolate_);
    }
    jit_logger_ = std::make_unique<JitLogger>(isolate_, event_handler);
    AddCodeEventListener(jit_logger_.get());
    if (options & kJitCodeEventEnumExisting) {
      HandleScope scope(isolate_);
      LogCodeObjects();
      LogCompiledFunctions();
    }
  }
}

sampler::Sampler* Logger::sampler() { return ticker_.get(); }
std::string Logger::file_name() const { return log_.get()->file_name(); }

void Logger::StopProfilerThread() {
  if (profiler_ != nullptr) {
    profiler_->Disengage();
    profiler_.reset();
  }
}

FILE* Logger::TearDownAndGetLogFile() {
  if (!is_initialized_) return nullptr;
  is_initialized_ = false;
  UpdateIsLogging(false);

  // Stop the profiler thread before closing the file.
  StopProfilerThread();

  ticker_.reset();
  timer_.Stop();

#if V8_OS_LINUX
  if (perf_basic_logger_) {
    RemoveCodeEventListener(perf_basic_logger_.get());
    perf_basic_logger_.reset();
  }

  if (perf_jit_logger_) {
    RemoveCodeEventListener(perf_jit_logger_.get());
    perf_jit_logger_.reset();
  }
#endif

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

void Logger::UpdateIsLogging(bool value) {
  base::MutexGuard guard(log_->mutex());
  // Relaxed atomic to avoid locking the mutex for the most common case: when
  // logging is disabled.
  is_logging_.store(value, std::memory_order_relaxed);
}

void ExistingCodeLogger::LogCodeObject(Object object) {
  HandleScope scope(isolate_);
  Handle<AbstractCode> abstract_code(AbstractCode::cast(object), isolate_);
  CodeEventListener::LogEventsAndTags tag = CodeEventListener::STUB_TAG;
  const char* description = "Unknown code from before profiling";
  switch (abstract_code->kind()) {
    case CodeKind::INTERPRETED_FUNCTION:
    case CodeKind::TURBOFAN:
    case CodeKind::BASELINE:
    case CodeKind::NATIVE_CONTEXT_INDEPENDENT:
    case CodeKind::TURBOPROP:
      return;  // We log this later using LogCompiledFunctions.
    case CodeKind::BYTECODE_HANDLER:
      return;  // We log it later by walking the dispatch table.
    case CodeKind::FOR_TESTING:
      description = "STUB code";
      tag = CodeEventListener::STUB_TAG;
      break;
    case CodeKind::REGEXP:
      description = "Regular expression code";
      tag = CodeEventListener::REG_EXP_TAG;
      break;
    case CodeKind::BUILTIN:
      if (Code::cast(object).is_interpreter_trampoline_builtin() &&
          Code::cast(object) !=
              *BUILTIN_CODE(isolate_, InterpreterEntryTrampoline)) {
        return;
      }
      description =
          isolate_->builtins()->name(abstract_code->GetCode().builtin_index());
      tag = CodeEventListener::BUILTIN_TAG;
      break;
    case CodeKind::WASM_FUNCTION:
      description = "A Wasm function";
      tag = CodeEventListener::FUNCTION_TAG;
      break;
    case CodeKind::JS_TO_WASM_FUNCTION:
      description = "A JavaScript to Wasm adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case CodeKind::JS_TO_JS_FUNCTION:
      description = "A WebAssembly.Function adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case CodeKind::WASM_TO_CAPI_FUNCTION:
      description = "A Wasm to C-API adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case CodeKind::WASM_TO_JS_FUNCTION:
      description = "A Wasm to JavaScript adapter";
      tag = CodeEventListener::STUB_TAG;
      break;
    case CodeKind::C_WASM_ENTRY:
      description = "A C to Wasm entry stub";
      tag = CodeEventListener::STUB_TAG;
      break;
  }
  CALL_CODE_EVENT_HANDLER(CodeCreateEvent(tag, abstract_code, description))
}

void ExistingCodeLogger::LogCodeObjects() {
  Heap* heap = isolate_->heap();
  HeapObjectIterator iterator(heap);
  DisallowGarbageCollection no_gc;
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    if (obj.IsCode()) LogCodeObject(obj);
    if (obj.IsBytecodeArray()) LogCodeObject(obj);
  }
}

void ExistingCodeLogger::LogCompiledFunctions() {
  Heap* heap = isolate_->heap();
  HandleScope scope(isolate_);
  std::vector<std::pair<Handle<SharedFunctionInfo>, Handle<AbstractCode>>>
      compiled_funcs = EnumerateCompiledFunctions(heap);

  // During iteration, there can be heap allocation due to
  // GetScriptLineNumber call.
  for (auto& pair : compiled_funcs) {
    Handle<SharedFunctionInfo> shared = pair.first;
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate_, shared);
    if (shared->HasInterpreterData()) {
      LogExistingFunction(
          shared,
          Handle<AbstractCode>(
              AbstractCode::cast(shared->InterpreterTrampoline()), isolate_),
          CodeEventListener::INTERPRETED_FUNCTION_TAG);
    }
    if (shared->HasBaselineData()) {
      // TODO(v8:11429): Add a tag for baseline code. Or use CodeKind?
      LogExistingFunction(
          shared,
          Handle<AbstractCode>(
              AbstractCode::cast(shared->baseline_data().baseline_code()),
              isolate_),
          CodeEventListener::INTERPRETED_FUNCTION_TAG);
    }
    if (pair.second.is_identical_to(BUILTIN_CODE(isolate_, CompileLazy)))
      continue;
    LogExistingFunction(pair.first, pair.second);
  }

  const std::vector<Handle<WasmModuleObject>> wasm_module_objects =
      EnumerateWasmModuleObjects(heap);
  for (auto& module_object : wasm_module_objects) {
    module_object->native_module()->LogWasmCodes(isolate_,
                                                 module_object->script());
  }
}

void ExistingCodeLogger::LogExistingFunction(
    Handle<SharedFunctionInfo> shared, Handle<AbstractCode> code,
    CodeEventListener::LogEventsAndTags tag) {
  if (shared->script().IsScript()) {
    Handle<Script> script(Script::cast(shared->script()), isolate_);
    int line_num = Script::GetLineNumber(script, shared->StartPosition()) + 1;
    int column_num =
        Script::GetColumnNumber(script, shared->StartPosition()) + 1;
    if (script->name().IsString()) {
      Handle<String> script_name(String::cast(script->name()), isolate_);
      if (line_num > 0) {
        CALL_CODE_EVENT_HANDLER(
            CodeCreateEvent(Logger::ToNativeByScript(tag, *script), code,
                            shared, script_name, line_num, column_num))
      } else {
        // Can't distinguish eval and script here, so always use Script.
        CALL_CODE_EVENT_HANDLER(CodeCreateEvent(
            Logger::ToNativeByScript(CodeEventListener::SCRIPT_TAG, *script),
            code, shared, script_name))
      }
    } else {
      CALL_CODE_EVENT_HANDLER(CodeCreateEvent(
          Logger::ToNativeByScript(tag, *script), code, shared,
          ReadOnlyRoots(isolate_).empty_string_handle(), line_num, column_num))
    }
  } else if (shared->IsApiFunction()) {
    // API function.
    Handle<FunctionTemplateInfo> fun_data =
        handle(shared->get_api_func_data(), isolate_);
    Object raw_call_data = fun_data->call_code(kAcquireLoad);
    if (!raw_call_data.IsUndefined(isolate_)) {
      CallHandlerInfo call_data = CallHandlerInfo::cast(raw_call_data);
      Object callback_obj = call_data.callback();
      Address entry_point = v8::ToCData<Address>(callback_obj);
#if USES_FUNCTION_DESCRIPTORS
      entry_point = *FUNCTION_ENTRYPOINT_ADDRESS(entry_point);
#endif
      Handle<String> fun_name = SharedFunctionInfo::DebugName(shared);
      CALL_CODE_EVENT_HANDLER(CallbackEvent(fun_name, entry_point))

      // Fast API function.
      Address c_function = v8::ToCData<Address>(fun_data->GetCFunction());
      if (c_function != kNullAddress) {
        CALL_CODE_EVENT_HANDLER(CallbackEvent(fun_name, c_function))
      }
    }
  }
}

#undef CALL_CODE_EVENT_HANDLER
#undef MSG_BUILDER

}  // namespace internal
}  // namespace v8
