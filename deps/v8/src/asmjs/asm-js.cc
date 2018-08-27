// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-js.h"

#include "src/asmjs/asm-names.h"
#include "src/asmjs/asm-parser.h"
#include "src/assert-scope.h"
#include "src/ast/ast.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/compiler.h"
#include "src/execution.h"
#include "src/handles.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/parsing/scanner.h"
#include "src/unoptimized-compilation-info.h"

#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

const char* const AsmJs::kSingleFunctionName = "__single_function__";

namespace {
enum WasmDataEntries {
  kWasmDataCompiledModule,
  kWasmDataUsesBitSet,
  kWasmDataEntryCount,
};

Handle<Object> StdlibMathMember(Isolate* isolate, Handle<JSReceiver> stdlib,
                                Handle<Name> name) {
  Handle<Name> math_name(
      isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("Math")));
  Handle<Object> math = JSReceiver::GetDataProperty(stdlib, math_name);
  if (!math->IsJSReceiver()) return isolate->factory()->undefined_value();
  Handle<JSReceiver> math_receiver = Handle<JSReceiver>::cast(math);
  Handle<Object> value = JSReceiver::GetDataProperty(math_receiver, name);
  return value;
}

bool AreStdlibMembersValid(Isolate* isolate, Handle<JSReceiver> stdlib,
                           wasm::AsmJsParser::StdlibSet members,
                           bool* is_typed_array) {
  if (members.Contains(wasm::AsmJsParser::StandardMember::kInfinity)) {
    members.Remove(wasm::AsmJsParser::StandardMember::kInfinity);
    Handle<Name> name = isolate->factory()->Infinity_string();
    Handle<Object> value = JSReceiver::GetDataProperty(stdlib, name);
    if (!value->IsNumber() || !std::isinf(value->Number())) return false;
  }
  if (members.Contains(wasm::AsmJsParser::StandardMember::kNaN)) {
    members.Remove(wasm::AsmJsParser::StandardMember::kNaN);
    Handle<Name> name = isolate->factory()->NaN_string();
    Handle<Object> value = JSReceiver::GetDataProperty(stdlib, name);
    if (!value->IsNaN()) return false;
  }
#define STDLIB_MATH_FUNC(fname, FName, ignore1, ignore2)                    \
  if (members.Contains(wasm::AsmJsParser::StandardMember::kMath##FName)) {  \
    members.Remove(wasm::AsmJsParser::StandardMember::kMath##FName);        \
    Handle<Name> name(isolate->factory()->InternalizeOneByteString(         \
        STATIC_CHAR_VECTOR(#fname)));                                       \
    Handle<Object> value = StdlibMathMember(isolate, stdlib, name);         \
    if (!value->IsJSFunction()) return false;                               \
    SharedFunctionInfo* shared = Handle<JSFunction>::cast(value)->shared(); \
    if (!shared->HasBuiltinId() ||                                          \
        shared->builtin_id() != Builtins::kMath##FName) {                   \
      return false;                                                         \
    }                                                                       \
    DCHECK_EQ(shared->GetCode(),                                            \
              isolate->builtins()->builtin(Builtins::kMath##FName));        \
  }
  STDLIB_MATH_FUNCTION_LIST(STDLIB_MATH_FUNC)
#undef STDLIB_MATH_FUNC
#define STDLIB_MATH_CONST(cname, const_value)                               \
  if (members.Contains(wasm::AsmJsParser::StandardMember::kMath##cname)) {  \
    members.Remove(wasm::AsmJsParser::StandardMember::kMath##cname);        \
    Handle<Name> name(isolate->factory()->InternalizeOneByteString(         \
        STATIC_CHAR_VECTOR(#cname)));                                       \
    Handle<Object> value = StdlibMathMember(isolate, stdlib, name);         \
    if (!value->IsNumber() || value->Number() != const_value) return false; \
  }
  STDLIB_MATH_VALUE_LIST(STDLIB_MATH_CONST)
#undef STDLIB_MATH_CONST
#define STDLIB_ARRAY_TYPE(fname, FName)                                \
  if (members.Contains(wasm::AsmJsParser::StandardMember::k##FName)) { \
    members.Remove(wasm::AsmJsParser::StandardMember::k##FName);       \
    *is_typed_array = true;                                            \
    Handle<Name> name(isolate->factory()->InternalizeOneByteString(    \
        STATIC_CHAR_VECTOR(#FName)));                                  \
    Handle<Object> value = JSReceiver::GetDataProperty(stdlib, name);  \
    if (!value->IsJSFunction()) return false;                          \
    Handle<JSFunction> func = Handle<JSFunction>::cast(value);         \
    if (!func.is_identical_to(isolate->fname())) return false;         \
  }
  STDLIB_ARRAY_TYPE(int8_array_fun, Int8Array)
  STDLIB_ARRAY_TYPE(uint8_array_fun, Uint8Array)
  STDLIB_ARRAY_TYPE(int16_array_fun, Int16Array)
  STDLIB_ARRAY_TYPE(uint16_array_fun, Uint16Array)
  STDLIB_ARRAY_TYPE(int32_array_fun, Int32Array)
  STDLIB_ARRAY_TYPE(uint32_array_fun, Uint32Array)
  STDLIB_ARRAY_TYPE(float32_array_fun, Float32Array)
  STDLIB_ARRAY_TYPE(float64_array_fun, Float64Array)
#undef STDLIB_ARRAY_TYPE
  // All members accounted for.
  DCHECK(members.IsEmpty());
  return true;
}

void Report(Handle<Script> script, int position, Vector<const char> text,
            MessageTemplate::Template message_template,
            v8::Isolate::MessageErrorLevel level) {
  Isolate* isolate = script->GetIsolate();
  MessageLocation location(script, position, position);
  Handle<String> text_object = isolate->factory()->InternalizeUtf8String(text);
  Handle<JSMessageObject> message = MessageHandler::MakeMessageObject(
      isolate, message_template, &location, text_object,
      Handle<FixedArray>::null());
  message->set_error_level(level);
  MessageHandler::ReportMessage(isolate, &location, message);
}

// Hook to report successful execution of {AsmJs::CompileAsmViaWasm} phase.
void ReportCompilationSuccess(Handle<Script> script, int position,
                              double translate_time, double compile_time,
                              size_t module_size) {
  if (FLAG_suppress_asm_messages || !FLAG_trace_asm_time) return;
  EmbeddedVector<char, 100> text;
  int length = SNPrintF(
      text, "success, asm->wasm: %0.3f ms, compile: %0.3f ms, %" PRIuS " bytes",
      translate_time, compile_time, module_size);
  CHECK_NE(-1, length);
  text.Truncate(length);
  Report(script, position, text, MessageTemplate::kAsmJsCompiled,
         v8::Isolate::kMessageInfo);
}

// Hook to report failed execution of {AsmJs::CompileAsmViaWasm} phase.
void ReportCompilationFailure(ParseInfo* parse_info, int position,
                              const char* reason) {
  if (FLAG_suppress_asm_messages) return;
  parse_info->pending_error_handler()->ReportWarningAt(
      position, position, MessageTemplate::kAsmJsInvalid, reason);
}

// Hook to report successful execution of {AsmJs::InstantiateAsmWasm} phase.
void ReportInstantiationSuccess(Handle<Script> script, int position,
                                double instantiate_time) {
  if (FLAG_suppress_asm_messages || !FLAG_trace_asm_time) return;
  EmbeddedVector<char, 50> text;
  int length = SNPrintF(text, "success, %0.3f ms", instantiate_time);
  CHECK_NE(-1, length);
  text.Truncate(length);
  Report(script, position, text, MessageTemplate::kAsmJsInstantiated,
         v8::Isolate::kMessageInfo);
}

// Hook to report failed execution of {AsmJs::InstantiateAsmWasm} phase.
void ReportInstantiationFailure(Handle<Script> script, int position,
                                const char* reason) {
  if (FLAG_suppress_asm_messages) return;
  Vector<const char> text = CStrVector(reason);
  Report(script, position, text, MessageTemplate::kAsmJsLinkingFailed,
         v8::Isolate::kMessageWarning);
}

}  // namespace

// The compilation of asm.js modules is split into two distinct steps:
//  [1] ExecuteJobImpl: The asm.js module source is parsed, validated, and
//      translated to a valid WebAssembly module. The result are two vectors
//      representing the encoded module as well as encoded source position
//      information and a StdlibSet bit set.
//  [2] FinalizeJobImpl: The module is handed to WebAssembly which decodes it
//      into an internal representation and eventually compiles it to machine
//      code.
class AsmJsCompilationJob final : public UnoptimizedCompilationJob {
 public:
  explicit AsmJsCompilationJob(ParseInfo* parse_info, FunctionLiteral* literal,
                               AccountingAllocator* allocator)
      : UnoptimizedCompilationJob(parse_info->stack_limit(), parse_info,
                                  &compilation_info_),
        allocator_(allocator),
        zone_(allocator, ZONE_NAME),
        compilation_info_(&zone_, parse_info, literal),
        module_(nullptr),
        asm_offsets_(nullptr),
        translate_time_(0),
        compile_time_(0),
        module_source_size_(0),
        translate_time_micro_(0),
        translate_zone_size_(0) {}

 protected:
  Status ExecuteJobImpl() final;
  Status FinalizeJobImpl(Handle<SharedFunctionInfo> shared_info,
                         Isolate* isolate) final;

 private:
  void RecordHistograms(Isolate* isolate);

  AccountingAllocator* allocator_;
  Zone zone_;
  UnoptimizedCompilationInfo compilation_info_;
  wasm::ZoneBuffer* module_;
  wasm::ZoneBuffer* asm_offsets_;
  wasm::AsmJsParser::StdlibSet stdlib_uses_;

  double translate_time_;   // Time (milliseconds) taken to execute step [1].
  double compile_time_;     // Time (milliseconds) taken to execute step [2].
  int module_source_size_;  // Module source size in bytes.
  int64_t translate_time_micro_;  // Time (microseconds) taken to translate.
  size_t translate_zone_size_;

  DISALLOW_COPY_AND_ASSIGN(AsmJsCompilationJob);
};

UnoptimizedCompilationJob::Status AsmJsCompilationJob::ExecuteJobImpl() {
  // Step 1: Translate asm.js module to WebAssembly module.
  size_t compile_zone_start = compilation_info()->zone()->allocation_size();
  base::ElapsedTimer translate_timer;
  translate_timer.Start();

  Zone* compile_zone = compilation_info()->zone();
  Zone translate_zone(allocator_, ZONE_NAME);

  Utf16CharacterStream* stream = parse_info()->character_stream();
  base::Optional<AllowHandleDereference> allow_deref;
  if (stream->can_access_heap()) {
    allow_deref.emplace();
  }
  stream->Seek(compilation_info()->literal()->start_position());
  wasm::AsmJsParser parser(&translate_zone, stack_limit(), stream);
  if (!parser.Run()) {
    if (!FLAG_suppress_asm_messages) {
      ReportCompilationFailure(parse_info(), parser.failure_location(),
                               parser.failure_message());
    }
    return FAILED;
  }
  module_ = new (compile_zone) wasm::ZoneBuffer(compile_zone);
  parser.module_builder()->WriteTo(*module_);
  asm_offsets_ = new (compile_zone) wasm::ZoneBuffer(compile_zone);
  parser.module_builder()->WriteAsmJsOffsetTable(*asm_offsets_);
  stdlib_uses_ = *parser.stdlib_uses();

  size_t compile_zone_size =
      compilation_info()->zone()->allocation_size() - compile_zone_start;
  translate_zone_size_ = translate_zone.allocation_size();
  translate_time_ = translate_timer.Elapsed().InMillisecondsF();
  translate_time_micro_ = translate_timer.Elapsed().InMicroseconds();
  module_source_size_ = compilation_info()->literal()->end_position() -
                        compilation_info()->literal()->start_position();
  if (FLAG_trace_asm_parser) {
    PrintF(
        "[asm.js translation successful: time=%0.3fms, "
        "translate_zone=%" PRIuS "KB, compile_zone+=%" PRIuS "KB]\n",
        translate_time_, translate_zone_size_ / KB, compile_zone_size / KB);
  }
  return SUCCEEDED;
}

UnoptimizedCompilationJob::Status AsmJsCompilationJob::FinalizeJobImpl(
    Handle<SharedFunctionInfo> shared_info, Isolate* isolate) {
  // Step 2: Compile and decode the WebAssembly module.
  base::ElapsedTimer compile_timer;
  compile_timer.Start();

  Handle<HeapNumber> uses_bitset =
      isolate->factory()->NewHeapNumberFromBits(stdlib_uses_.ToIntegral());

  wasm::ErrorThrower thrower(isolate, "AsmJs::Compile");
  Handle<WasmModuleObject> compiled =
      isolate->wasm_engine()
          ->SyncCompileTranslatedAsmJs(
              isolate, &thrower,
              wasm::ModuleWireBytes(module_->begin(), module_->end()),
              parse_info()->script(),
              Vector<const byte>(asm_offsets_->begin(), asm_offsets_->size()))
          .ToHandleChecked();
  DCHECK(!thrower.error());
  compile_time_ = compile_timer.Elapsed().InMillisecondsF();

  // The result is a compiled module and serialized standard library uses.
  Handle<FixedArray> result =
      isolate->factory()->NewFixedArray(kWasmDataEntryCount);
  result->set(kWasmDataCompiledModule, *compiled);
  result->set(kWasmDataUsesBitSet, *uses_bitset);
  compilation_info()->SetAsmWasmData(result);

  RecordHistograms(isolate);
  ReportCompilationSuccess(parse_info()->script(),
                           compilation_info()->literal()->position(),
                           translate_time_, compile_time_, module_->size());
  return SUCCEEDED;
}

void AsmJsCompilationJob::RecordHistograms(Isolate* isolate) {
  Counters* counters = isolate->counters();
  counters->asm_wasm_translation_time()->AddSample(
      static_cast<int>(translate_time_micro_));
  counters->asm_wasm_translation_peak_memory_bytes()->AddSample(
      static_cast<int>(translate_zone_size_));
  counters->asm_module_size_bytes()->AddSample(module_source_size_);
  // translation_throughput is not exact (assumes MB == 1000000). But that is ok
  // since the metric is stored in buckets that lose some precision anyways.
  int translation_throughput =
      translate_time_micro_ != 0
          ? static_cast<int>(static_cast<int64_t>(module_source_size_) /
                             translate_time_micro_)
          : 0;
  counters->asm_wasm_translation_throughput()->AddSample(
      translation_throughput);
}

UnoptimizedCompilationJob* AsmJs::NewCompilationJob(
    ParseInfo* parse_info, FunctionLiteral* literal,
    AccountingAllocator* allocator) {
  return new AsmJsCompilationJob(parse_info, literal, allocator);
}

MaybeHandle<Object> AsmJs::InstantiateAsmWasm(Isolate* isolate,
                                              Handle<SharedFunctionInfo> shared,
                                              Handle<FixedArray> wasm_data,
                                              Handle<JSReceiver> stdlib,
                                              Handle<JSReceiver> foreign,
                                              Handle<JSArrayBuffer> memory) {
  base::ElapsedTimer instantiate_timer;
  instantiate_timer.Start();
  Handle<HeapNumber> uses_bitset(
      HeapNumber::cast(wasm_data->get(kWasmDataUsesBitSet)));
  Handle<WasmModuleObject> module(
      WasmModuleObject::cast(wasm_data->get(kWasmDataCompiledModule)));
  Handle<Script> script(Script::cast(shared->script()));
  // TODO(mstarzinger): The position currently points to the module definition
  // but should instead point to the instantiation site (more intuitive).
  int position = shared->StartPosition();

  // Check that all used stdlib members are valid.
  bool stdlib_use_of_typed_array_present = false;
  wasm::AsmJsParser::StdlibSet stdlib_uses(uses_bitset->value_as_bits());
  if (!stdlib_uses.IsEmpty()) {  // No checking needed if no uses.
    if (stdlib.is_null()) {
      ReportInstantiationFailure(script, position, "Requires standard library");
      return MaybeHandle<Object>();
    }
    if (!AreStdlibMembersValid(isolate, stdlib, stdlib_uses,
                               &stdlib_use_of_typed_array_present)) {
      ReportInstantiationFailure(script, position, "Unexpected stdlib member");
      return MaybeHandle<Object>();
    }
  }

  // Check that a valid heap buffer is provided if required.
  if (stdlib_use_of_typed_array_present) {
    if (memory.is_null()) {
      ReportInstantiationFailure(script, position, "Requires heap buffer");
      return MaybeHandle<Object>();
    }
    memory->set_is_growable(false);
    size_t size = NumberToSize(memory->byte_length());
    // TODO(mstarzinger): We currently only limit byte length of the buffer to
    // be a multiple of 8, we should enforce the stricter spec limits here.
    if (size % FixedTypedArrayBase::kMaxElementSize != 0) {
      ReportInstantiationFailure(script, position, "Unexpected heap size");
      return MaybeHandle<Object>();
    }
    // Currently WebAssembly only supports heap sizes within the uint32_t range.
    if (size > std::numeric_limits<uint32_t>::max()) {
      ReportInstantiationFailure(script, position, "Unexpected heap size");
      return MaybeHandle<Object>();
    }
  } else {
    memory = Handle<JSArrayBuffer>::null();
  }

  wasm::ErrorThrower thrower(isolate, "AsmJs::Instantiate");
  MaybeHandle<Object> maybe_module_object =
      isolate->wasm_engine()->SyncInstantiate(isolate, &thrower, module,
                                              foreign, memory);
  if (maybe_module_object.is_null()) {
    // An exception caused by the module start function will be set as pending
    // and bypass the {ErrorThrower}, this happens in case of a stack overflow.
    if (isolate->has_pending_exception()) isolate->clear_pending_exception();
    thrower.Reset();  // Ensure exceptions do not propagate.
    ReportInstantiationFailure(script, position, "Internal wasm failure");
    return MaybeHandle<Object>();
  }
  DCHECK(!thrower.error());
  Handle<Object> module_object = maybe_module_object.ToHandleChecked();

  ReportInstantiationSuccess(script, position,
                             instantiate_timer.Elapsed().InMillisecondsF());

  Handle<Name> single_function_name(
      isolate->factory()->InternalizeUtf8String(AsmJs::kSingleFunctionName));
  MaybeHandle<Object> single_function =
      Object::GetProperty(module_object, single_function_name);
  if (!single_function.is_null() &&
      !single_function.ToHandleChecked()->IsUndefined(isolate)) {
    return single_function;
  }

  Handle<String> exports_name =
      isolate->factory()->InternalizeUtf8String("exports");
  return Object::GetProperty(module_object, exports_name);
}

}  // namespace internal
}  // namespace v8
