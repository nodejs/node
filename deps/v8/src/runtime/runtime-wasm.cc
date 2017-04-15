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
#include "src/frames-inl.h"
#include "src/objects-inl.h"
#include "src/v8memory.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

namespace {
Handle<WasmInstanceObject> GetWasmInstanceOnStackTop(Isolate* isolate) {
  DisallowHeapAllocation no_allocation;
  const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
  Address pc =
      Memory::Address_at(entry + StandardFrameConstants::kCallerPCOffset);
  Code* code = isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
  DCHECK_EQ(Code::WASM_FUNCTION, code->kind());
  WasmInstanceObject* owning_instance = wasm::GetOwningWasmInstance(code);
  CHECK_NOT_NULL(owning_instance);
  return handle(owning_instance, isolate);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmMemorySize) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<WasmInstanceObject> instance = GetWasmInstanceOnStackTop(isolate);
  return *isolate->factory()->NewNumberFromInt(
      wasm::GetInstanceMemorySize(isolate, instance));
}

RUNTIME_FUNCTION(Runtime_WasmGrowMemory) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 0);
  Handle<WasmInstanceObject> instance = GetWasmInstanceOnStackTop(isolate);
  return *isolate->factory()->NewNumberFromInt(
      wasm::GrowMemory(isolate, instance, delta_pages));
}

Object* ThrowRuntimeError(Isolate* isolate, int message_id, int byte_offset,
                          bool patch_source_position) {
  HandleScope scope(isolate);
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
    DCHECK(stack_elements->Offset(0)->value() >= 0);
    stack_elements->SetOffset(0, Smi::FromInt(-1 - byte_offset));
  }

  // Patch the detailed stack trace (array of JSObjects with various
  // properties).
  Handle<Object> detailed_stack_trace_obj = JSReceiver::GetDataProperty(
      error, isolate->factory()->detailed_stack_trace_symbol());
  if (detailed_stack_trace_obj->IsJSArray()) {
    Handle<FixedArray> stack_elements(
        FixedArray::cast(JSArray::cast(*detailed_stack_trace_obj)->elements()));
    DCHECK_GE(stack_elements->length(), 1);
    Handle<JSObject> top_frame(JSObject::cast(stack_elements->get(0)));
    Handle<String> wasm_offset_key =
        isolate->factory()->InternalizeOneByteString(
            STATIC_CHAR_VECTOR("column"));
    LookupIterator it(top_frame, wasm_offset_key, top_frame,
                      LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
    if (it.IsFound()) {
      DCHECK(JSReceiver::GetDataProperty(&it)->IsSmi());
      // Make column number 1-based here.
      Maybe<bool> data_set = JSReceiver::SetDataProperty(
          &it, handle(Smi::FromInt(byte_offset + 1), isolate));
      DCHECK(data_set.IsJust() && data_set.FromJust() == true);
      USE(data_set);
    }
  }

  return isolate->Throw(*error_obj);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmError) {
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  CONVERT_SMI_ARG_CHECKED(byte_offset, 1);
  return ThrowRuntimeError(isolate, message_id, byte_offset, true);
}

#define DECLARE_ENUM(name)                                                    \
  RUNTIME_FUNCTION(Runtime_ThrowWasm##name) {                                 \
    int message_id = wasm::WasmOpcodes::TrapReasonToMessageId(wasm::k##name); \
    return ThrowRuntimeError(isolate, message_id, 0, false);                  \
  }
FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
#undef DECLARE_ENUM

RUNTIME_FUNCTION(Runtime_WasmThrowTypeError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapTypeError));
}

RUNTIME_FUNCTION(Runtime_WasmThrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_SMI_ARG_CHECKED(lower, 0);
  CONVERT_SMI_ARG_CHECKED(upper, 1);

  const int32_t thrown_value = (upper << 16) | lower;

  return isolate->Throw(*isolate->factory()->NewNumberFromInt(thrown_value));
}

RUNTIME_FUNCTION(Runtime_WasmGetCaughtExceptionValue) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Object* exception = args[0];
  // The unwinder will only deliver exceptions to wasm if the exception is a
  // Number or a Smi (which we have just converted to a Number.) This logic
  // lives in Isolate::is_catchable_by_wasm(Object*).
  CHECK(exception->IsNumber());
  return exception;
}

RUNTIME_FUNCTION(Runtime_WasmRunInterpreter) {
  DCHECK(args.length() == 3);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, instance_obj, 0);
  CONVERT_NUMBER_CHECKED(int32_t, func_index, Int32, args[1]);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg_buffer_obj, 2);
  CHECK(WasmInstanceObject::IsWasmInstanceObject(*instance_obj));

  // The arg buffer is the raw pointer to the caller's stack. It looks like a
  // Smi (lowest bit not set, as checked by IsSmi), but is no valid Smi. We just
  // cast it back to the raw pointer.
  CHECK(!arg_buffer_obj->IsHeapObject());
  CHECK(arg_buffer_obj->IsSmi());
  uint8_t* arg_buffer = reinterpret_cast<uint8_t*>(*arg_buffer_obj);

  Handle<WasmInstanceObject> instance =
      Handle<WasmInstanceObject>::cast(instance_obj);
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  WasmDebugInfo::RunInterpreter(debug_info, func_index, arg_buffer);
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
