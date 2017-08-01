// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-js.h"

#include "src/api-natives.h"
#include "src/api.h"
#include "src/asmjs/asm-names.h"
#include "src/asmjs/asm-parser.h"
#include "src/assert-scope.h"
#include "src/ast/ast.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/compilation-info.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

const char* const AsmJs::kSingleFunctionName = "__single_function__";

namespace {
enum WasmDataEntries {
  kWasmDataCompiledModule,
  kWasmDataUsesArray,
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

bool IsStdlibMemberValid(Isolate* isolate, Handle<JSReceiver> stdlib,
                         wasm::AsmJsParser::StandardMember member,
                         bool* is_typed_array) {
  switch (member) {
    case wasm::AsmJsParser::StandardMember::kInfinity: {
      Handle<Name> name = isolate->factory()->infinity_string();
      Handle<Object> value = JSReceiver::GetDataProperty(stdlib, name);
      return value->IsNumber() && std::isinf(value->Number());
    }
    case wasm::AsmJsParser::StandardMember::kNaN: {
      Handle<Name> name = isolate->factory()->nan_string();
      Handle<Object> value = JSReceiver::GetDataProperty(stdlib, name);
      return value->IsNaN();
    }
#define STDLIB_MATH_FUNC(fname, FName, ignore1, ignore2)            \
  case wasm::AsmJsParser::StandardMember::kMath##FName: {           \
    Handle<Name> name(isolate->factory()->InternalizeOneByteString( \
        STATIC_CHAR_VECTOR(#fname)));                               \
    Handle<Object> value = StdlibMathMember(isolate, stdlib, name); \
    if (!value->IsJSFunction()) return false;                       \
    Handle<JSFunction> func = Handle<JSFunction>::cast(value);      \
    return func->shared()->code() ==                                \
           isolate->builtins()->builtin(Builtins::kMath##FName);    \
  }
      STDLIB_MATH_FUNCTION_LIST(STDLIB_MATH_FUNC)
#undef STDLIB_MATH_FUNC
#define STDLIB_MATH_CONST(cname, const_value)                       \
  case wasm::AsmJsParser::StandardMember::kMath##cname: {           \
    Handle<Name> name(isolate->factory()->InternalizeOneByteString( \
        STATIC_CHAR_VECTOR(#cname)));                               \
    Handle<Object> value = StdlibMathMember(isolate, stdlib, name); \
    return value->IsNumber() && value->Number() == const_value;     \
  }
      STDLIB_MATH_VALUE_LIST(STDLIB_MATH_CONST)
#undef STDLIB_MATH_CONST
#define STDLIB_ARRAY_TYPE(fname, FName)                               \
  case wasm::AsmJsParser::StandardMember::k##FName: {                 \
    *is_typed_array = true;                                           \
    Handle<Name> name(isolate->factory()->InternalizeOneByteString(   \
        STATIC_CHAR_VECTOR(#FName)));                                 \
    Handle<Object> value = JSReceiver::GetDataProperty(stdlib, name); \
    if (!value->IsJSFunction()) return false;                         \
    Handle<JSFunction> func = Handle<JSFunction>::cast(value);        \
    return func.is_identical_to(isolate->fname());                    \
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
  }
  UNREACHABLE();
  return false;
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
void ReportCompilationFailure(Handle<Script> script, int position,
                              const char* reason) {
  if (FLAG_suppress_asm_messages) return;
  Vector<const char> text = CStrVector(reason);
  Report(script, position, text, MessageTemplate::kAsmJsInvalid,
         v8::Isolate::kMessageWarning);
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

MaybeHandle<FixedArray> AsmJs::CompileAsmViaWasm(CompilationInfo* info) {
  wasm::ZoneBuffer* module = nullptr;
  wasm::ZoneBuffer* asm_offsets = nullptr;
  Handle<FixedArray> uses_array;
  Handle<WasmModuleObject> compiled;

  // The compilation of asm.js modules is split into two distinct steps:
  //  [1] The asm.js module source is parsed, validated, and translated to a
  //      valid WebAssembly module. The result are two vectors representing the
  //      encoded module as well as encoded source position information.
  //  [2] The module is handed to WebAssembly which decodes it into an internal
  //      representation and eventually compiles it to machine code.
  double translate_time;  // Time (milliseconds) taken to execute step [1].
  double compile_time;    // Time (milliseconds) taken to execute step [2].

  // Step 1: Translate asm.js module to WebAssembly module.
  {
    HistogramTimerScope translate_time_scope(
        info->isolate()->counters()->asm_wasm_translation_time());
    size_t compile_zone_start = info->zone()->allocation_size();
    base::ElapsedTimer translate_timer;
    translate_timer.Start();

    Zone* compile_zone = info->zone();
    Zone translate_zone(info->isolate()->allocator(), ZONE_NAME);
    wasm::AsmJsParser parser(info->isolate(), &translate_zone, info->script(),
                             info->literal()->start_position(),
                             info->literal()->end_position());
    if (!parser.Run()) {
      DCHECK(!info->isolate()->has_pending_exception());
      ReportCompilationFailure(info->script(), parser.failure_location(),
                               parser.failure_message());
      return MaybeHandle<FixedArray>();
    }
    module = new (compile_zone) wasm::ZoneBuffer(compile_zone);
    parser.module_builder()->WriteTo(*module);
    asm_offsets = new (compile_zone) wasm::ZoneBuffer(compile_zone);
    parser.module_builder()->WriteAsmJsOffsetTable(*asm_offsets);
    uses_array = info->isolate()->factory()->NewFixedArray(
        static_cast<int>(parser.stdlib_uses()->size()));
    int count = 0;
    for (auto i : *parser.stdlib_uses()) {
      uses_array->set(count++, Smi::FromInt(i));
    }
    size_t compile_zone_size =
        info->zone()->allocation_size() - compile_zone_start;
    size_t translate_zone_size = translate_zone.allocation_size();
    info->isolate()
        ->counters()
        ->asm_wasm_translation_peak_memory_bytes()
        ->AddSample(static_cast<int>(translate_zone_size));
    translate_time = translate_timer.Elapsed().InMillisecondsF();
    if (FLAG_trace_asm_parser) {
      PrintF(
          "[asm.js translation successful: time=%0.3fms, "
          "translate_zone=%" PRIuS "KB, compile_zone+=%" PRIuS "KB]\n",
          translate_time, translate_zone_size / KB, compile_zone_size / KB);
    }
  }

  // Step 2: Compile and decode the WebAssembly module.
  {
    base::ElapsedTimer compile_timer;
    compile_timer.Start();
    wasm::ErrorThrower thrower(info->isolate(), "AsmJs::Compile");
    MaybeHandle<WasmModuleObject> maybe_compiled = SyncCompileTranslatedAsmJs(
        info->isolate(), &thrower,
        wasm::ModuleWireBytes(module->begin(), module->end()), info->script(),
        Vector<const byte>(asm_offsets->begin(), asm_offsets->size()));
    DCHECK(!maybe_compiled.is_null());
    DCHECK(!thrower.error());
    compile_time = compile_timer.Elapsed().InMillisecondsF();
    compiled = maybe_compiled.ToHandleChecked();
  }

  // The result is a compiled module and serialized standard library uses.
  Handle<FixedArray> result =
      info->isolate()->factory()->NewFixedArray(kWasmDataEntryCount);
  result->set(kWasmDataCompiledModule, *compiled);
  result->set(kWasmDataUsesArray, *uses_array);
  ReportCompilationSuccess(info->script(), info->literal()->position(),
                           translate_time, compile_time, module->size());
  return result;
}

MaybeHandle<Object> AsmJs::InstantiateAsmWasm(Isolate* isolate,
                                              Handle<SharedFunctionInfo> shared,
                                              Handle<FixedArray> wasm_data,
                                              Handle<JSReceiver> stdlib,
                                              Handle<JSReceiver> foreign,
                                              Handle<JSArrayBuffer> memory) {
  base::ElapsedTimer instantiate_timer;
  instantiate_timer.Start();
  Handle<FixedArray> stdlib_uses(
      FixedArray::cast(wasm_data->get(kWasmDataUsesArray)));
  Handle<WasmModuleObject> module(
      WasmModuleObject::cast(wasm_data->get(kWasmDataCompiledModule)));
  Handle<Script> script(Script::cast(shared->script()));
  // TODO(mstarzinger): The position currently points to the module definition
  // but should instead point to the instantiation site (more intuitive).
  int position = shared->start_position();

  // Check that all used stdlib members are valid.
  bool stdlib_use_of_typed_array_present = false;
  for (int i = 0; i < stdlib_uses->length(); ++i) {
    if (stdlib.is_null()) {
      ReportInstantiationFailure(script, position, "Requires standard library");
      return MaybeHandle<Object>();
    }
    int member_id = Smi::cast(stdlib_uses->get(i))->value();
    wasm::AsmJsParser::StandardMember member =
        static_cast<wasm::AsmJsParser::StandardMember>(member_id);
    if (!IsStdlibMemberValid(isolate, stdlib, member,
                             &stdlib_use_of_typed_array_present)) {
      ReportInstantiationFailure(script, position, "Unexpected stdlib member");
      return MaybeHandle<Object>();
    }
  }

  // Create the ffi object for foreign functions {"": foreign}.
  Handle<JSObject> ffi_object;
  if (!foreign.is_null()) {
    Handle<JSFunction> object_function = Handle<JSFunction>(
        isolate->native_context()->object_function(), isolate);
    ffi_object = isolate->factory()->NewJSObject(object_function);
    JSObject::AddProperty(ffi_object, isolate->factory()->empty_string(),
                          foreign, NONE);
  }

  // Check that a valid heap buffer is provided if required.
  if (stdlib_use_of_typed_array_present) {
    if (memory.is_null()) {
      ReportInstantiationFailure(script, position, "Requires heap buffer");
      return MaybeHandle<Object>();
    }
    size_t size = NumberToSize(memory->byte_length());
    // TODO(mstarzinger): We currently only limit byte length of the buffer to
    // be a multiple of 8, we should enforce the stricter spec limits here.
    if (size % FixedTypedArrayBase::kMaxElementSize != 0) {
      ReportInstantiationFailure(script, position, "Unexpected heap size");
      return MaybeHandle<Object>();
    }
  }

  wasm::ErrorThrower thrower(isolate, "AsmJs::Instantiate");
  MaybeHandle<Object> maybe_module_object =
      wasm::SyncInstantiate(isolate, &thrower, module, ffi_object, memory);
  if (maybe_module_object.is_null()) {
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
