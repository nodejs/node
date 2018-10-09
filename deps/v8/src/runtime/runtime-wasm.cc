// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/frame-constants.h"
#include "src/heap/factory.h"
#include "src/objects-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/trap-handler/trap-handler.h"
#include "src/v8memory.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

namespace {

WasmInstanceObject* GetWasmInstanceOnStackTop(Isolate* isolate) {
  StackFrameIterator it(isolate, isolate->thread_local_top());
  // On top: C entry stub.
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Next: the wasm (compiled or interpreted) frame.
  WasmInstanceObject* result = nullptr;
  if (it.frame()->is_wasm_compiled()) {
    result = WasmCompiledFrame::cast(it.frame())->wasm_instance();
  } else {
    DCHECK(it.frame()->is_wasm_interpreter_entry());
    result = WasmInterpreterEntryFrame::cast(it.frame())->wasm_instance();
  }
  return result;
}

Context* GetNativeContextFromWasmInstanceOnStackTop(Isolate* isolate) {
  return GetWasmInstanceOnStackTop(isolate)->native_context();
}

class ClearThreadInWasmScope {
 public:
  explicit ClearThreadInWasmScope(bool coming_from_wasm)
      : coming_from_wasm_(coming_from_wasm) {
    DCHECK_EQ(trap_handler::IsTrapHandlerEnabled() && coming_from_wasm,
              trap_handler::IsThreadInWasm());
    if (coming_from_wasm) trap_handler::ClearThreadInWasm();
  }
  ~ClearThreadInWasmScope() {
    DCHECK(!trap_handler::IsThreadInWasm());
    if (coming_from_wasm_) trap_handler::SetThreadInWasm();
  }

 private:
  const bool coming_from_wasm_;
};

}  // namespace

RUNTIME_FUNCTION(Runtime_WasmGrowMemory) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  // {delta_pages} is checked to be a positive smi in the WasmGrowMemory builtin
  // which calls this runtime function.
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 1);

  // This runtime function is always being called from wasm code.
  ClearThreadInWasmScope flag_scope(true);

  // Set the current isolate's context.
  DCHECK_NULL(isolate->context());
  isolate->set_context(instance->native_context());

  int ret = WasmMemoryObject::Grow(
      isolate, handle(instance->memory_object(), isolate), delta_pages);
  // The WasmGrowMemory builtin which calls this runtime function expects us to
  // always return a Smi.
  return Smi::FromInt(ret);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmError) {
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  ClearThreadInWasmScope clear_wasm_flag(isolate->context() == nullptr);

  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  Handle<Object> error_obj = isolate->factory()->NewWasmRuntimeError(
      static_cast<MessageTemplate::Template>(message_id));
  return isolate->Throw(*error_obj);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmStackOverflow) {
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  return isolate->StackOverflow();
}

RUNTIME_FUNCTION(Runtime_WasmThrowTypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapTypeError));
}

RUNTIME_FUNCTION(Runtime_WasmThrowCreate) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  DCHECK_EQ(2, args.length());
  Handle<Object> exception = isolate->factory()->NewWasmRuntimeError(
      static_cast<MessageTemplate::Template>(
          MessageTemplate::kWasmExceptionError));
  isolate->set_wasm_caught_exception(*exception);
  CONVERT_ARG_HANDLE_CHECKED(Smi, id, 0);
  CHECK(!JSReceiver::SetProperty(isolate, exception,
                                 isolate->factory()->InternalizeUtf8String(
                                     wasm::WasmException::kRuntimeIdStr),
                                 id, LanguageMode::kStrict)
             .is_null());
  CONVERT_SMI_ARG_CHECKED(size, 1);
  Handle<JSTypedArray> values =
      isolate->factory()->NewJSTypedArray(ElementsKind::UINT16_ELEMENTS, size);
  CHECK(!JSReceiver::SetProperty(isolate, exception,
                                 isolate->factory()->InternalizeUtf8String(
                                     wasm::WasmException::kRuntimeValuesStr),
                                 values, LanguageMode::kStrict)
             .is_null());
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmThrow) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  DCHECK_EQ(0, args.length());
  Handle<Object> exception(isolate->get_wasm_caught_exception(), isolate);
  CHECK(!exception.is_null());
  isolate->clear_wasm_caught_exception();
  return isolate->Throw(*exception);
}

RUNTIME_FUNCTION(Runtime_WasmGetExceptionRuntimeId) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  Handle<Object> except_obj(isolate->get_wasm_caught_exception(), isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj), isolate);
    Handle<Object> tag;
    if (JSReceiver::GetProperty(isolate, exception,
                                isolate->factory()->InternalizeUtf8String(
                                    wasm::WasmException::kRuntimeIdStr))
            .ToHandle(&tag)) {
      if (tag->IsSmi()) {
        return *tag;
      }
    }
  }
  return Smi::FromInt(wasm::kInvalidExceptionTag);
}

RUNTIME_FUNCTION(Runtime_WasmExceptionGetElement) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  DCHECK_EQ(1, args.length());
  Handle<Object> except_obj(isolate->get_wasm_caught_exception(), isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj), isolate);
    Handle<Object> values_obj;
    if (JSReceiver::GetProperty(isolate, exception,
                                isolate->factory()->InternalizeUtf8String(
                                    wasm::WasmException::kRuntimeValuesStr))
            .ToHandle(&values_obj)) {
      if (values_obj->IsJSTypedArray()) {
        Handle<JSTypedArray> values = Handle<JSTypedArray>::cast(values_obj);
        CHECK_EQ(values->type(), kExternalUint16Array);
        CONVERT_SMI_ARG_CHECKED(index, 0);
        CHECK_LT(index, Smi::ToInt(values->length()));
        auto* vals =
            reinterpret_cast<uint16_t*>(values->GetBuffer()->backing_store());
        return Smi::FromInt(vals[index]);
      }
    }
  }
  return Smi::FromInt(0);
}

RUNTIME_FUNCTION(Runtime_WasmExceptionSetElement) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  Handle<Object> except_obj(isolate->get_wasm_caught_exception(), isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj), isolate);
    Handle<Object> values_obj;
    if (JSReceiver::GetProperty(isolate, exception,
                                isolate->factory()->InternalizeUtf8String(
                                    wasm::WasmException::kRuntimeValuesStr))
            .ToHandle(&values_obj)) {
      if (values_obj->IsJSTypedArray()) {
        Handle<JSTypedArray> values = Handle<JSTypedArray>::cast(values_obj);
        CHECK_EQ(values->type(), kExternalUint16Array);
        CONVERT_SMI_ARG_CHECKED(index, 0);
        CHECK_LT(index, Smi::ToInt(values->length()));
        CONVERT_SMI_ARG_CHECKED(value, 1);
        auto* vals =
            reinterpret_cast<uint16_t*>(values->GetBuffer()->backing_store());
        vals[index] = static_cast<uint16_t>(value);
      }
    }
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmRunInterpreter) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_NUMBER_CHECKED(int32_t, func_index, Int32, args[0]);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg_buffer_obj, 1);
  Handle<WasmInstanceObject> instance(GetWasmInstanceOnStackTop(isolate),
                                      isolate);

  // The arg buffer is the raw pointer to the caller's stack. It looks like a
  // Smi (lowest bit not set, as checked by IsSmi), but is no valid Smi. We just
  // cast it back to the raw pointer.
  CHECK(!arg_buffer_obj->IsHeapObject());
  CHECK(arg_buffer_obj->IsSmi());
  Address arg_buffer = reinterpret_cast<Address>(*arg_buffer_obj);

  ClearThreadInWasmScope wasm_flag(true);

  // Set the current isolate's context.
  DCHECK_NULL(isolate->context());
  isolate->set_context(instance->native_context());

  // Find the frame pointer of the interpreter entry.
  Address frame_pointer = 0;
  {
    StackFrameIterator it(isolate, isolate->thread_local_top());
    // On top: C entry stub.
    DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
    it.Advance();
    // Next: the wasm interpreter entry.
    DCHECK_EQ(StackFrame::WASM_INTERPRETER_ENTRY, it.frame()->type());
    frame_pointer = it.frame()->fp();
  }

  // Run the function in the interpreter. Note that neither the {WasmDebugInfo}
  // nor the {InterpreterHandle} have to exist, because interpretation might
  // have been triggered by another Isolate sharing the same WasmEngine.
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  bool success = WasmDebugInfo::RunInterpreter(
      isolate, debug_info, frame_pointer, func_index, arg_buffer);

  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmStackGuard) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  DCHECK(!trap_handler::IsTrapHandlerEnabled() ||
         trap_handler::IsThreadInWasm());

  ClearThreadInWasmScope wasm_flag(true);

  // Set the current isolate's context.
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));

  // Check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) return isolate->StackOverflow();

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_WasmCompileLazy) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_SMI_ARG_CHECKED(func_index, 1);

  ClearThreadInWasmScope wasm_flag(true);

#ifdef DEBUG
  StackFrameIterator it(isolate, isolate->thread_local_top());
  // On top: C entry stub.
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Next: the wasm lazy compile frame.
  DCHECK_EQ(StackFrame::WASM_COMPILE_LAZY, it.frame()->type());
  DCHECK_EQ(*instance, WasmCompileLazyFrame::cast(it.frame())->wasm_instance());
#endif

  Address entrypoint = wasm::CompileLazy(
      isolate, instance->module_object()->native_module(), func_index);
  return reinterpret_cast<Object*>(entrypoint);
}

}  // namespace internal
}  // namespace v8
