// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/compiler/wasm-compiler.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/frame-constants.h"
#include "src/objects-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/v8memory.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

namespace {

WasmInstanceObject* GetWasmInstanceOnStackTop(Isolate* isolate) {
  DisallowHeapAllocation no_allocation;
  const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
  Address pc =
      Memory::Address_at(entry + StandardFrameConstants::kCallerPCOffset);
  Code* code = isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
  DCHECK_EQ(Code::WASM_FUNCTION, code->kind());
  WasmInstanceObject* owning_instance =
      WasmInstanceObject::GetOwningInstance(code);
  CHECK_NOT_NULL(owning_instance);
  return owning_instance;
}
Context* GetWasmContextOnStackTop(Isolate* isolate) {
  return GetWasmInstanceOnStackTop(isolate)
      ->compiled_module()
      ->ptr_to_native_context();
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmGrowMemory) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 0);
  Handle<WasmInstanceObject> instance(GetWasmInstanceOnStackTop(isolate),
                                      isolate);

  // Set the current isolate's context.
  DCHECK_NULL(isolate->context());
  isolate->set_context(instance->compiled_module()->ptr_to_native_context());

  return *isolate->factory()->NewNumberFromInt(
      WasmInstanceObject::GrowMemory(isolate, instance, delta_pages));
}

Object* ThrowRuntimeError(Isolate* isolate, int message_id, int byte_offset,
                          bool patch_source_position) {
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetWasmContextOnStackTop(isolate));
  Handle<Object> error_obj = isolate->factory()->NewWasmRuntimeError(
      static_cast<MessageTemplate::Template>(message_id));

  if (!patch_source_position) {
    return isolate->Throw(*error_obj);
  }

  // For wasm traps, the byte offset (a.k.a source position) can not be
  // determined from relocation info, since the explicit checks for traps
  // converge in one singe block which calls this runtime function.
  // We hence pass the byte offset explicitely, and patch it into the top-most
  // frame (a wasm frame) on the collected stack trace.
  // TODO(wasm): This implementation is temporary, see bug #5007:
  // https://bugs.chromium.org/p/v8/issues/detail?id=5007
  Handle<JSObject> error = Handle<JSObject>::cast(error_obj);
  Handle<Object> stack_trace_obj = JSReceiver::GetDataProperty(
      error, isolate->factory()->stack_trace_symbol());
  // Patch the stack trace (array of <receiver, function, code, position>).
  if (stack_trace_obj->IsJSArray()) {
    Handle<FrameArray> stack_elements(
        FrameArray::cast(JSArray::cast(*stack_trace_obj)->elements()));
    DCHECK(stack_elements->Code(0)->kind() == AbstractCode::WASM_FUNCTION);
    DCHECK_LE(0, stack_elements->Offset(0)->value());
    stack_elements->SetOffset(0, Smi::FromInt(-1 - byte_offset));
  }

  // Patch the detailed stack trace (array of JSObjects with various
  // properties).
  Handle<Object> detailed_stack_trace_obj = JSReceiver::GetDataProperty(
      error, isolate->factory()->detailed_stack_trace_symbol());
  if (detailed_stack_trace_obj->IsFixedArray()) {
    Handle<FixedArray> stack_elements(
        FixedArray::cast(*detailed_stack_trace_obj));
    DCHECK_GE(stack_elements->length(), 1);
    Handle<StackFrameInfo> top_frame(
        StackFrameInfo::cast(stack_elements->get(0)));
    if (top_frame->column_number()) {
      top_frame->set_column_number(byte_offset + 1);
    }
  }

  return isolate->Throw(*error_obj);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmErrorFromTrapIf) {
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  return ThrowRuntimeError(isolate, message_id, 0, false);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmError) {
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  CONVERT_SMI_ARG_CHECKED(byte_offset, 1);
  return ThrowRuntimeError(isolate, message_id, byte_offset, true);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmStackOverflow) {
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetWasmContextOnStackTop(isolate));
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
  isolate->set_context(GetWasmContextOnStackTop(isolate));
  DCHECK_EQ(2, args.length());
  Handle<Object> exception = isolate->factory()->NewWasmRuntimeError(
      static_cast<MessageTemplate::Template>(
          MessageTemplate::kWasmExceptionError));
  isolate->set_wasm_caught_exception(*exception);
  CONVERT_ARG_HANDLE_CHECKED(Smi, id, 0);
  CHECK(!JSReceiver::SetProperty(exception,
                                 isolate->factory()->InternalizeUtf8String(
                                     wasm::WasmException::kRuntimeIdStr),
                                 id, STRICT)
             .is_null());
  CONVERT_SMI_ARG_CHECKED(size, 1);
  Handle<JSTypedArray> values =
      isolate->factory()->NewJSTypedArray(ElementsKind::UINT16_ELEMENTS, size);
  CHECK(!JSReceiver::SetProperty(exception,
                                 isolate->factory()->InternalizeUtf8String(
                                     wasm::WasmException::kRuntimeValuesStr),
                                 values, STRICT)
             .is_null());
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmThrow) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetWasmContextOnStackTop(isolate));
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
  isolate->set_context(GetWasmContextOnStackTop(isolate));
  Handle<Object> except_obj(isolate->get_wasm_caught_exception(), isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj));
    Handle<Object> tag;
    if (JSReceiver::GetProperty(exception,
                                isolate->factory()->InternalizeUtf8String(
                                    wasm::WasmException::kRuntimeIdStr))
            .ToHandle(&tag)) {
      if (tag->IsSmi()) {
        return *tag;
      }
    }
  }
  return Smi::FromInt(wasm::WasmModule::kInvalidExceptionTag);
}

RUNTIME_FUNCTION(Runtime_WasmExceptionGetElement) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetWasmContextOnStackTop(isolate));
  DCHECK_EQ(1, args.length());
  Handle<Object> except_obj(isolate->get_wasm_caught_exception(), isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj));
    Handle<Object> values_obj;
    if (JSReceiver::GetProperty(exception,
                                isolate->factory()->InternalizeUtf8String(
                                    wasm::WasmException::kRuntimeValuesStr))
            .ToHandle(&values_obj)) {
      if (values_obj->IsJSTypedArray()) {
        Handle<JSTypedArray> values = Handle<JSTypedArray>::cast(values_obj);
        CHECK_EQ(values->type(), kExternalUint16Array);
        CONVERT_SMI_ARG_CHECKED(index, 0);
        CHECK_LT(index, Smi::ToInt(values->length()));
        auto* vals =
            reinterpret_cast<uint16_t*>(values->GetBuffer()->allocation_base());
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
  isolate->set_context(GetWasmContextOnStackTop(isolate));
  Handle<Object> except_obj(isolate->get_wasm_caught_exception(), isolate);
  if (!except_obj.is_null() && except_obj->IsJSReceiver()) {
    Handle<JSReceiver> exception(JSReceiver::cast(*except_obj));
    Handle<Object> values_obj;
    if (JSReceiver::GetProperty(exception,
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
            reinterpret_cast<uint16_t*>(values->GetBuffer()->allocation_base());
        vals[index] = static_cast<uint16_t>(value);
      }
    }
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmRunInterpreter) {
  DCHECK_EQ(3, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_NUMBER_CHECKED(int32_t, func_index, Int32, args[1]);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg_buffer_obj, 2);

  // The arg buffer is the raw pointer to the caller's stack. It looks like a
  // Smi (lowest bit not set, as checked by IsSmi), but is no valid Smi. We just
  // cast it back to the raw pointer.
  CHECK(!arg_buffer_obj->IsHeapObject());
  CHECK(arg_buffer_obj->IsSmi());
  uint8_t* arg_buffer = reinterpret_cast<uint8_t*>(*arg_buffer_obj);

  // Set the current isolate's context.
  DCHECK_NULL(isolate->context());
  isolate->set_context(instance->compiled_module()->ptr_to_native_context());

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

  bool success = instance->debug_info()->RunInterpreter(frame_pointer,
                                                        func_index, arg_buffer);

  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return isolate->heap()->exception();
  }
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmStackGuard) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  DCHECK(!trap_handler::UseTrapHandler() || trap_handler::IsThreadInWasm());

  struct ClearAndRestoreThreadInWasm {
    ClearAndRestoreThreadInWasm() { trap_handler::ClearThreadInWasm(); }

    ~ClearAndRestoreThreadInWasm() { trap_handler::SetThreadInWasm(); }
  } restore_thread_in_wasm;

  // Set the current isolate's context.
  DCHECK_NULL(isolate->context());
  isolate->set_context(GetWasmContextOnStackTop(isolate));

  // Check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) return isolate->StackOverflow();

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_WasmCompileLazy) {
  DCHECK_EQ(0, args.length());
  HandleScope scope(isolate);

  return *wasm::CompileLazy(isolate);
}

}  // namespace internal
}  // namespace v8
