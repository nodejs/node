// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-js.h"

#include "src/api-natives.h"
#include "src/api.h"
#include "src/asmjs/asm-typer.h"
#include "src/asmjs/asm-wasm-builder.h"
#include "src/assert-scope.h"
#include "src/compilation-info.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/parsing/parse-info.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

typedef uint8_t byte;

using v8::internal::wasm::ErrorThrower;

namespace v8 {
namespace internal {

namespace {
enum WasmDataEntries {
  kWasmDataCompiledModule,
  kWasmDataForeignGlobals,
  kWasmDataUsesArray,
  kWasmDataScript,
  kWasmDataScriptPosition,
  kWasmDataEntryCount,
};

Handle<i::Object> StdlibMathMember(i::Isolate* isolate,
                                   Handle<JSReceiver> stdlib,
                                   Handle<Name> name) {
  if (stdlib.is_null()) {
    return Handle<i::Object>();
  }
  Handle<i::Name> math_name(
      isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("Math")));
  MaybeHandle<i::Object> maybe_math = i::Object::GetProperty(stdlib, math_name);
  if (maybe_math.is_null()) {
    return Handle<i::Object>();
  }
  Handle<i::Object> math = maybe_math.ToHandleChecked();
  if (!math->IsJSReceiver()) {
    return Handle<i::Object>();
  }
  MaybeHandle<i::Object> maybe_value = i::Object::GetProperty(math, name);
  if (maybe_value.is_null()) {
    return Handle<i::Object>();
  }
  return maybe_value.ToHandleChecked();
}

bool IsStdlibMemberValid(i::Isolate* isolate, Handle<JSReceiver> stdlib,
                         Handle<i::Object> member_id) {
  int32_t member_kind;
  if (!member_id->ToInt32(&member_kind)) {
    UNREACHABLE();
  }
  switch (member_kind) {
    case wasm::AsmTyper::StandardMember::kNone:
    case wasm::AsmTyper::StandardMember::kModule:
    case wasm::AsmTyper::StandardMember::kStdlib:
    case wasm::AsmTyper::StandardMember::kHeap:
    case wasm::AsmTyper::StandardMember::kFFI: {
      // Nothing to check for these.
      return true;
    }
    case wasm::AsmTyper::StandardMember::kInfinity: {
      if (stdlib.is_null()) {
        return false;
      }
      Handle<i::Name> name(isolate->factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("Infinity")));
      MaybeHandle<i::Object> maybe_value = i::Object::GetProperty(stdlib, name);
      if (maybe_value.is_null()) {
        return false;
      }
      Handle<i::Object> value = maybe_value.ToHandleChecked();
      return value->IsNumber() && std::isinf(value->Number());
    }
    case wasm::AsmTyper::StandardMember::kNaN: {
      if (stdlib.is_null()) {
        return false;
      }
      Handle<i::Name> name(isolate->factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("NaN")));
      MaybeHandle<i::Object> maybe_value = i::Object::GetProperty(stdlib, name);
      if (maybe_value.is_null()) {
        return false;
      }
      Handle<i::Object> value = maybe_value.ToHandleChecked();
      return value->IsNaN();
    }
#define STDLIB_MATH_FUNC(CamelName, fname)                             \
  case wasm::AsmTyper::StandardMember::k##CamelName: {                 \
    Handle<i::Name> name(isolate->factory()->InternalizeOneByteString( \
        STATIC_CHAR_VECTOR(#fname)));                                  \
    Handle<i::Object> value = StdlibMathMember(isolate, stdlib, name); \
    if (value.is_null() || !value->IsJSFunction()) {                   \
      return false;                                                    \
    }                                                                  \
    Handle<i::JSFunction> func(i::JSFunction::cast(*value));           \
    return func->shared()->code() ==                                   \
           isolate->builtins()->builtin(Builtins::k##CamelName);       \
  }
      STDLIB_MATH_FUNC(MathAcos, acos)
      STDLIB_MATH_FUNC(MathAsin, asin)
      STDLIB_MATH_FUNC(MathAtan, atan)
      STDLIB_MATH_FUNC(MathCos, cos)
      STDLIB_MATH_FUNC(MathSin, sin)
      STDLIB_MATH_FUNC(MathTan, tan)
      STDLIB_MATH_FUNC(MathExp, exp)
      STDLIB_MATH_FUNC(MathLog, log)
      STDLIB_MATH_FUNC(MathCeil, ceil)
      STDLIB_MATH_FUNC(MathFloor, floor)
      STDLIB_MATH_FUNC(MathSqrt, sqrt)
      STDLIB_MATH_FUNC(MathAbs, abs)
      STDLIB_MATH_FUNC(MathClz32, clz32)
      STDLIB_MATH_FUNC(MathMin, min)
      STDLIB_MATH_FUNC(MathMax, max)
      STDLIB_MATH_FUNC(MathAtan2, atan2)
      STDLIB_MATH_FUNC(MathPow, pow)
      STDLIB_MATH_FUNC(MathImul, imul)
      STDLIB_MATH_FUNC(MathFround, fround)
#undef STDLIB_MATH_FUNC
#define STDLIB_MATH_CONST(cname, const_value)                             \
  case wasm::AsmTyper::StandardMember::kMath##cname: {                    \
    i::Handle<i::Name> name(isolate->factory()->InternalizeOneByteString( \
        STATIC_CHAR_VECTOR(#cname)));                                     \
    i::Handle<i::Object> value = StdlibMathMember(isolate, stdlib, name); \
    return !value.is_null() && value->IsNumber() &&                       \
           value->Number() == const_value;                                \
  }
      STDLIB_MATH_CONST(E, 2.718281828459045)
      STDLIB_MATH_CONST(LN10, 2.302585092994046)
      STDLIB_MATH_CONST(LN2, 0.6931471805599453)
      STDLIB_MATH_CONST(LOG2E, 1.4426950408889634)
      STDLIB_MATH_CONST(LOG10E, 0.4342944819032518)
      STDLIB_MATH_CONST(PI, 3.141592653589793)
      STDLIB_MATH_CONST(SQRT1_2, 0.7071067811865476)
      STDLIB_MATH_CONST(SQRT2, 1.4142135623730951)
#undef STDLIB_MATH_CONST
    default: { UNREACHABLE(); }
  }
  return false;
}

}  // namespace

MaybeHandle<FixedArray> AsmJs::CompileAsmViaWasm(CompilationInfo* info) {
  ErrorThrower thrower(info->isolate(), "Asm.js -> WebAssembly conversion");
  base::ElapsedTimer asm_wasm_timer;
  asm_wasm_timer.Start();
  wasm::AsmWasmBuilder builder(info);
  Handle<FixedArray> foreign_globals;
  auto asm_wasm_result = builder.Run(&foreign_globals);
  if (!asm_wasm_result.success) {
    DCHECK(!info->isolate()->has_pending_exception());
    if (!FLAG_suppress_asm_messages) {
      MessageHandler::ReportMessage(info->isolate(),
                                    builder.typer()->message_location(),
                                    builder.typer()->error_message());
    }
    return MaybeHandle<FixedArray>();
  }
  double asm_wasm_time = asm_wasm_timer.Elapsed().InMillisecondsF();

  wasm::ZoneBuffer* module = asm_wasm_result.module_bytes;
  wasm::ZoneBuffer* asm_offsets = asm_wasm_result.asm_offset_table;
  Vector<const byte> asm_offsets_vec(asm_offsets->begin(),
                                     static_cast<int>(asm_offsets->size()));

  base::ElapsedTimer compile_timer;
  compile_timer.Start();
  MaybeHandle<JSObject> compiled = wasm::CreateModuleObjectFromBytes(
      info->isolate(), module->begin(), module->end(), &thrower,
      internal::wasm::kAsmJsOrigin, info->script(), asm_offsets_vec);
  DCHECK(!compiled.is_null());
  double compile_time = compile_timer.Elapsed().InMillisecondsF();

  wasm::AsmTyper::StdlibSet uses = builder.typer()->StdlibUses();
  Handle<FixedArray> uses_array =
      info->isolate()->factory()->NewFixedArray(static_cast<int>(uses.size()));
  int count = 0;
  for (auto i : uses) {
    uses_array->set(count++, Smi::FromInt(i));
  }

  Handle<FixedArray> result =
      info->isolate()->factory()->NewFixedArray(kWasmDataEntryCount);
  result->set(kWasmDataCompiledModule, *compiled.ToHandleChecked());
  result->set(kWasmDataForeignGlobals, *foreign_globals);
  result->set(kWasmDataUsesArray, *uses_array);
  result->set(kWasmDataScript, *info->script());
  result->set(kWasmDataScriptPosition,
              Smi::FromInt(info->literal()->position()));

  MessageLocation location(info->script(), info->literal()->position(),
                           info->literal()->position());
  char text[100];
  int length;
  if (FLAG_predictable) {
    length = base::OS::SNPrintF(text, arraysize(text), "success");
  } else {
    length =
        base::OS::SNPrintF(text, arraysize(text),
                           "success, asm->wasm: %0.3f ms, compile: %0.3f ms",
                           asm_wasm_time, compile_time);
  }
  DCHECK_NE(-1, length);
  USE(length);
  Handle<String> stext(info->isolate()->factory()->InternalizeUtf8String(text));
  Handle<JSMessageObject> message = MessageHandler::MakeMessageObject(
      info->isolate(), MessageTemplate::kAsmJsCompiled, &location, stext,
      Handle<JSArray>::null());
  message->set_error_level(v8::Isolate::kMessageInfo);
  if (!FLAG_suppress_asm_messages && FLAG_trace_asm_time) {
    MessageHandler::ReportMessage(info->isolate(), &location, message);
  }

  return result;
}

bool AsmJs::IsStdlibValid(i::Isolate* isolate, Handle<FixedArray> wasm_data,
                          Handle<JSReceiver> stdlib) {
  i::Handle<i::FixedArray> uses(
      i::FixedArray::cast(wasm_data->get(kWasmDataUsesArray)));
  for (int i = 0; i < uses->length(); ++i) {
    if (!IsStdlibMemberValid(isolate, stdlib,
                             uses->GetValueChecked<i::Object>(isolate, i))) {
      return false;
    }
  }
  return true;
}

MaybeHandle<Object> AsmJs::InstantiateAsmWasm(i::Isolate* isolate,
                                              Handle<FixedArray> wasm_data,
                                              Handle<JSArrayBuffer> memory,
                                              Handle<JSReceiver> foreign) {
  base::ElapsedTimer instantiate_timer;
  instantiate_timer.Start();
  i::Handle<i::WasmModuleObject> module(
      i::WasmModuleObject::cast(wasm_data->get(kWasmDataCompiledModule)));
  i::Handle<i::FixedArray> foreign_globals(
      i::FixedArray::cast(wasm_data->get(kWasmDataForeignGlobals)));

  ErrorThrower thrower(isolate, "Asm.js -> WebAssembly instantiation");

  // Create the ffi object for foreign functions {"": foreign}.
  Handle<JSObject> ffi_object;
  if (!foreign.is_null()) {
    Handle<JSFunction> object_function = Handle<JSFunction>(
        isolate->native_context()->object_function(), isolate);
    ffi_object = isolate->factory()->NewJSObject(object_function);
    JSObject::AddProperty(ffi_object, isolate->factory()->empty_string(),
                          foreign, NONE);
  }

  i::MaybeHandle<i::JSObject> maybe_module_object =
      i::wasm::WasmModule::Instantiate(isolate, &thrower, module, ffi_object,
                                       memory);
  if (maybe_module_object.is_null()) {
    return MaybeHandle<Object>();
  }

  i::Handle<i::Name> init_name(isolate->factory()->InternalizeUtf8String(
      wasm::AsmWasmBuilder::foreign_init_name));

  i::Handle<i::Object> module_object = maybe_module_object.ToHandleChecked();
  i::MaybeHandle<i::Object> maybe_init =
      i::Object::GetProperty(module_object, init_name);
  DCHECK(!maybe_init.is_null());

  i::Handle<i::Object> init = maybe_init.ToHandleChecked();
  i::Handle<i::Object> undefined(isolate->heap()->undefined_value(), isolate);
  i::Handle<i::Object>* foreign_args_array =
      new i::Handle<i::Object>[foreign_globals->length()];
  for (int j = 0; j < foreign_globals->length(); j++) {
    if (!foreign.is_null()) {
      i::MaybeHandle<i::Name> name = i::Object::ToName(
          isolate, i::Handle<i::Object>(foreign_globals->get(j), isolate));
      if (!name.is_null()) {
        i::MaybeHandle<i::Object> val =
            i::Object::GetProperty(foreign, name.ToHandleChecked());
        if (!val.is_null()) {
          foreign_args_array[j] = val.ToHandleChecked();
          continue;
        }
      }
    }
    foreign_args_array[j] = undefined;
  }
  i::MaybeHandle<i::Object> retval = i::Execution::Call(
      isolate, init, undefined, foreign_globals->length(), foreign_args_array);
  delete[] foreign_args_array;
  DCHECK(!retval.is_null());

  i::Handle<i::Name> single_function_name(
      isolate->factory()->InternalizeUtf8String(
          wasm::AsmWasmBuilder::single_function_name));
  i::MaybeHandle<i::Object> single_function =
      i::Object::GetProperty(module_object, single_function_name);
  if (!single_function.is_null() &&
      !single_function.ToHandleChecked()->IsUndefined(isolate)) {
    return single_function;
  }

  i::Handle<i::Script> script(i::Script::cast(wasm_data->get(kWasmDataScript)));
  int32_t position = 0;
  if (!wasm_data->get(kWasmDataScriptPosition)->ToInt32(&position)) {
    UNREACHABLE();
  }
  MessageLocation location(script, position, position);
  char text[50];
  int length;
  if (FLAG_predictable) {
    length = base::OS::SNPrintF(text, arraysize(text), "success");
  } else {
    length = base::OS::SNPrintF(text, arraysize(text), "success, %0.3f ms",
                                instantiate_timer.Elapsed().InMillisecondsF());
  }
  DCHECK_NE(-1, length);
  USE(length);
  Handle<String> stext(isolate->factory()->InternalizeUtf8String(text));
  Handle<JSMessageObject> message = MessageHandler::MakeMessageObject(
      isolate, MessageTemplate::kAsmJsInstantiated, &location, stext,
      Handle<JSArray>::null());
  message->set_error_level(v8::Isolate::kMessageInfo);
  if (!FLAG_suppress_asm_messages && FLAG_trace_asm_time) {
    MessageHandler::ReportMessage(isolate, &location, message);
  }

  return module_object;
}

}  // namespace internal
}  // namespace v8
