// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/memory.h"
#include "src/common/message-template.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {

namespace {

WasmInstanceObject GetWasmInstanceOnStackTop(Isolate* isolate) {
  StackFrameIterator it(isolate, isolate->thread_local_top());
  // On top: C entry stub.
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Next: the wasm compiled frame.
  DCHECK(it.frame()->is_wasm_compiled());
  WasmCompiledFrame* frame = WasmCompiledFrame::cast(it.frame());
  return frame->wasm_instance();
}

Context GetNativeContextFromWasmInstanceOnStackTop(Isolate* isolate) {
  return GetWasmInstanceOnStackTop(isolate).native_context();
}

class ClearThreadInWasmScope {
 public:
  ClearThreadInWasmScope() {
    DCHECK_EQ(trap_handler::IsTrapHandlerEnabled(),
              trap_handler::IsThreadInWasm());
    trap_handler::ClearThreadInWasm();
  }
  ~ClearThreadInWasmScope() {
    DCHECK(!trap_handler::IsThreadInWasm());
    trap_handler::SetThreadInWasm();
  }
};

Object ThrowWasmError(Isolate* isolate, MessageTemplate message) {
  HandleScope scope(isolate);
  Handle<Object> error_obj = isolate->factory()->NewWasmRuntimeError(message);
  return isolate->Throw(*error_obj);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmIsValidFuncRefValue) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, function, 0);

  if (function->IsNull(isolate)) {
    return Smi::FromInt(true);
  }
  if (WasmExternalFunction::IsWasmExternalFunction(*function)) {
    return Smi::FromInt(true);
  }
  return Smi::FromInt(false);
}

RUNTIME_FUNCTION(Runtime_WasmMemoryGrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  // {delta_pages} is checked to be a positive smi in the WasmMemoryGrow builtin
  // which calls this runtime function.
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 1);

  // This runtime function is always being called from wasm code.
  ClearThreadInWasmScope flag_scope;

  int ret = WasmMemoryObject::Grow(
      isolate, handle(instance->memory_object(), isolate), delta_pages);
  // The WasmMemoryGrow builtin which calls this runtime function expects us to
  // always return a Smi.
  return Smi::FromInt(ret);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmError) {
  ClearThreadInWasmScope clear_wasm_flag;
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  return ThrowWasmError(isolate, MessageTemplateFromInt(message_id));
}

RUNTIME_FUNCTION(Runtime_ThrowWasmStackOverflow) {
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
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
  DCHECK_EQ(2, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(WasmExceptionTag, tag_raw, 0);
  CONVERT_SMI_ARG_CHECKED(size, 1);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> tag(tag_raw, isolate);
  Handle<Object> exception = isolate->factory()->NewWasmRuntimeError(
      MessageTemplate::kWasmExceptionError);
  CHECK(!Object::SetProperty(isolate, exception,
                             isolate->factory()->wasm_exception_tag_symbol(),
                             tag, StoreOrigin::kMaybeKeyed,
                             Just(ShouldThrow::kThrowOnError))
             .is_null());
  Handle<FixedArray> values = isolate->factory()->NewFixedArray(size);
  CHECK(!Object::SetProperty(isolate, exception,
                             isolate->factory()->wasm_exception_values_symbol(),
                             values, StoreOrigin::kMaybeKeyed,
                             Just(ShouldThrow::kThrowOnError))
             .is_null());
  return *exception;
}

RUNTIME_FUNCTION(Runtime_WasmExceptionGetTag) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(Object, except_obj_raw, 0);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> except_obj(except_obj_raw, isolate);
  if (!except_obj->IsWasmExceptionPackage(isolate)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  Handle<WasmExceptionPackage> exception =
      Handle<WasmExceptionPackage>::cast(except_obj);
  return *WasmExceptionPackage::GetExceptionTag(isolate, exception);
}

RUNTIME_FUNCTION(Runtime_WasmExceptionGetValues) {
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(Object, except_obj_raw, 0);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> except_obj(except_obj_raw, isolate);
  if (!except_obj->IsWasmExceptionPackage(isolate)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  Handle<WasmExceptionPackage> exception =
      Handle<WasmExceptionPackage>::cast(except_obj);
  return *WasmExceptionPackage::GetExceptionValues(isolate, exception);
}

RUNTIME_FUNCTION(Runtime_WasmRunInterpreter) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_NUMBER_CHECKED(int32_t, func_index, Int32, args[0]);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg_buffer_obj, 1);

  // The arg buffer is the raw pointer to the caller's stack. It looks like a
  // Smi (lowest bit not set, as checked by IsSmi), but is no valid Smi. We just
  // cast it back to the raw pointer.
  CHECK(!arg_buffer_obj->IsHeapObject());
  CHECK(arg_buffer_obj->IsSmi());
  Address arg_buffer = arg_buffer_obj->ptr();

  ClearThreadInWasmScope wasm_flag;

  // Find the frame pointer and instance of the interpreter frame on the stack.
  Handle<WasmInstanceObject> instance;
  Address frame_pointer = 0;
  {
    StackFrameIterator it(isolate, isolate->thread_local_top());
    // On top: C entry stub.
    DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
    it.Advance();
    // Next: the wasm interpreter entry.
    DCHECK_EQ(StackFrame::WASM_INTERPRETER_ENTRY, it.frame()->type());
    instance = handle(
        WasmInterpreterEntryFrame::cast(it.frame())->wasm_instance(), isolate);
    frame_pointer = it.frame()->fp();
  }

  // Reserve buffers for argument and return values.
  DCHECK_GE(instance->module()->functions.size(), func_index);
  wasm::FunctionSig* sig = instance->module()->functions[func_index].sig;
  DCHECK_GE(kMaxInt, sig->parameter_count());
  int num_params = static_cast<int>(sig->parameter_count());
  ScopedVector<wasm::WasmValue> wasm_args(num_params);
  DCHECK_GE(kMaxInt, sig->return_count());
  int num_returns = static_cast<int>(sig->return_count());
  ScopedVector<wasm::WasmValue> wasm_rets(num_returns);

  // Copy the arguments for the {arg_buffer} into a vector of {WasmValue}. This
  // also boxes reference types into handles, which needs to happen before any
  // methods that could trigger a GC are being called.
  Address arg_buf_ptr = arg_buffer;
  for (int i = 0; i < num_params; ++i) {
#define CASE_ARG_TYPE(type, ctype)                                     \
  case wasm::type:                                                     \
    DCHECK_EQ(wasm::ValueTypes::ElementSizeInBytes(sig->GetParam(i)),  \
              sizeof(ctype));                                          \
    wasm_args[i] =                                                     \
        wasm::WasmValue(base::ReadUnalignedValue<ctype>(arg_buf_ptr)); \
    arg_buf_ptr += sizeof(ctype);                                      \
    break;
    switch (sig->GetParam(i)) {
      CASE_ARG_TYPE(kWasmI32, uint32_t)
      CASE_ARG_TYPE(kWasmI64, uint64_t)
      CASE_ARG_TYPE(kWasmF32, float)
      CASE_ARG_TYPE(kWasmF64, double)
#undef CASE_ARG_TYPE
      case wasm::kWasmAnyRef:
      case wasm::kWasmFuncRef:
      case wasm::kWasmExnRef: {
        DCHECK_EQ(wasm::ValueTypes::ElementSizeInBytes(sig->GetParam(i)),
                  kSystemPointerSize);
        Handle<Object> ref(base::ReadUnalignedValue<Object>(arg_buf_ptr),
                           isolate);
        wasm_args[i] = wasm::WasmValue(ref);
        arg_buf_ptr += kSystemPointerSize;
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  // Set the current isolate's context.
  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());

  // Run the function in the interpreter. Note that neither the {WasmDebugInfo}
  // nor the {InterpreterHandle} have to exist, because interpretation might
  // have been triggered by another Isolate sharing the same WasmEngine.
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  bool success = WasmDebugInfo::RunInterpreter(
      isolate, debug_info, frame_pointer, func_index, wasm_args, wasm_rets);

  // Early return on failure.
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }

  // Copy return values from the vector of {WasmValue} into {arg_buffer}. This
  // also un-boxes reference types from handles into raw pointers.
  arg_buf_ptr = arg_buffer;
  for (int i = 0; i < num_returns; ++i) {
#define CASE_RET_TYPE(type, ctype)                                           \
  case wasm::type:                                                           \
    DCHECK_EQ(wasm::ValueTypes::ElementSizeInBytes(sig->GetReturn(i)),       \
              sizeof(ctype));                                                \
    base::WriteUnalignedValue<ctype>(arg_buf_ptr, wasm_rets[i].to<ctype>()); \
    arg_buf_ptr += sizeof(ctype);                                            \
    break;
    switch (sig->GetReturn(i)) {
      CASE_RET_TYPE(kWasmI32, uint32_t)
      CASE_RET_TYPE(kWasmI64, uint64_t)
      CASE_RET_TYPE(kWasmF32, float)
      CASE_RET_TYPE(kWasmF64, double)
#undef CASE_RET_TYPE
      case wasm::kWasmAnyRef:
      case wasm::kWasmFuncRef:
      case wasm::kWasmExnRef: {
        DCHECK_EQ(wasm::ValueTypes::ElementSizeInBytes(sig->GetReturn(i)),
                  kSystemPointerSize);
        base::WriteUnalignedValue<Object>(arg_buf_ptr,
                                          *wasm_rets[i].to_anyref());
        arg_buf_ptr += kSystemPointerSize;
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmStackGuard) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  DCHECK(!trap_handler::IsTrapHandlerEnabled() ||
         trap_handler::IsThreadInWasm());

  ClearThreadInWasmScope wasm_flag;

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

  // This runtime function is always called from wasm code.
  ClearThreadInWasmScope flag_scope;

#ifdef DEBUG
  StackFrameIterator it(isolate, isolate->thread_local_top());
  // On top: C entry stub.
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Next: the wasm lazy compile frame.
  DCHECK_EQ(StackFrame::WASM_COMPILE_LAZY, it.frame()->type());
  DCHECK_EQ(*instance, WasmCompileLazyFrame::cast(it.frame())->wasm_instance());
#endif

  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());
  auto* native_module = instance->module_object().native_module();
  bool success = wasm::CompileLazy(isolate, native_module, func_index);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }

  Address entrypoint = native_module->GetCallTargetForFunction(func_index);

  return Object(entrypoint);
}

// Should be called from within a handle scope
Handle<JSArrayBuffer> getSharedArrayBuffer(Handle<WasmInstanceObject> instance,
                                           Isolate* isolate, uint32_t address) {
  DCHECK(instance->has_memory_object());
  Handle<JSArrayBuffer> array_buffer(instance->memory_object().array_buffer(),
                                     isolate);

  // Validation should have failed if the memory was not shared.
  DCHECK(array_buffer->is_shared());

  // Should have trapped if address was OOB
  DCHECK_LT(address, array_buffer->byte_length());
  return array_buffer;
}

RUNTIME_FUNCTION(Runtime_WasmAtomicNotify) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, address, Uint32, args[1]);
  CONVERT_NUMBER_CHECKED(uint32_t, count, Uint32, args[2]);
  Handle<JSArrayBuffer> array_buffer =
      getSharedArrayBuffer(instance, isolate, address);
  return FutexEmulation::Wake(array_buffer, address, count);
}

double WaitTimeoutInMs(double timeout_ns) {
  return timeout_ns < 0
             ? V8_INFINITY
             : timeout_ns / (base::Time::kNanosecondsPerMicrosecond *
                             base::Time::kMicrosecondsPerMillisecond);
}

RUNTIME_FUNCTION(Runtime_WasmI32AtomicWait) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, address, Uint32, args[1]);
  CONVERT_NUMBER_CHECKED(int32_t, expected_value, Int32, args[2]);
  CONVERT_DOUBLE_ARG_CHECKED(timeout_ns, 3);
  double timeout_ms = WaitTimeoutInMs(timeout_ns);
  Handle<JSArrayBuffer> array_buffer =
      getSharedArrayBuffer(instance, isolate, address);
  return FutexEmulation::Wait32(isolate, array_buffer, address, expected_value,
                                timeout_ms);
}

RUNTIME_FUNCTION(Runtime_WasmI64AtomicWait) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, address, Uint32, args[1]);
  CONVERT_NUMBER_CHECKED(uint32_t, expected_value_high, Uint32, args[2]);
  CONVERT_NUMBER_CHECKED(uint32_t, expected_value_low, Uint32, args[3]);
  CONVERT_DOUBLE_ARG_CHECKED(timeout_ns, 4);
  int64_t expected_value = (static_cast<uint64_t>(expected_value_high) << 32) |
                           static_cast<uint64_t>(expected_value_low);
  double timeout_ms = WaitTimeoutInMs(timeout_ns);
  Handle<JSArrayBuffer> array_buffer =
      getSharedArrayBuffer(instance, isolate, address);
  return FutexEmulation::Wait64(isolate, array_buffer, address, expected_value,
                                timeout_ms);
}

namespace {
Object ThrowTableOutOfBounds(Isolate* isolate,
                             Handle<WasmInstanceObject> instance) {
  // Handle out-of-bounds access here in the runtime call, rather
  // than having the lower-level layers deal with JS exceptions.
  if (isolate->context().is_null()) {
    isolate->set_context(instance->native_context());
  }
  Handle<Object> error_obj = isolate->factory()->NewWasmRuntimeError(
      MessageTemplate::kWasmTrapTableOutOfBounds);
  return isolate->Throw(*error_obj);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmRefFunc) {
  // This runtime function is always being called from wasm code.
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());
  CONVERT_UINT32_ARG_CHECKED(function_index, 0);

  Handle<WasmExternalFunction> function =
      WasmInstanceObject::GetOrCreateWasmExternalFunction(isolate, instance,
                                                          function_index);

  return *function;
}

RUNTIME_FUNCTION(Runtime_WasmFunctionTableGet) {
  // This runtime function is always being called from wasm code.
  ClearThreadInWasmScope flag_scope;

  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(table_index, 1);
  CONVERT_UINT32_ARG_CHECKED(entry_index, 2);
  DCHECK_LT(table_index, instance->tables().length());
  auto table = handle(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);

  if (!WasmTableObject::IsInBounds(isolate, table, entry_index)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
  }

  return *WasmTableObject::Get(isolate, table, entry_index);
}

RUNTIME_FUNCTION(Runtime_WasmFunctionTableSet) {
  // This runtime function is always being called from wasm code.
  ClearThreadInWasmScope flag_scope;

  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(table_index, 1);
  CONVERT_UINT32_ARG_CHECKED(entry_index, 2);
  CONVERT_ARG_CHECKED(Object, element_raw, 3);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> element(element_raw, isolate);
  DCHECK_LT(table_index, instance->tables().length());
  auto table = handle(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);

  if (!WasmTableObject::IsInBounds(isolate, table, entry_index)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
  }
  WasmTableObject::Set(isolate, table, entry_index, element);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableInit) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  CONVERT_UINT32_ARG_CHECKED(table_index, 0);
  CONVERT_UINT32_ARG_CHECKED(elem_segment_index, 1);
  CONVERT_UINT32_ARG_CHECKED(dst, 2);
  CONVERT_UINT32_ARG_CHECKED(src, 3);
  CONVERT_UINT32_ARG_CHECKED(count, 4);

  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());

  bool oob = !WasmInstanceObject::InitTableEntries(
      isolate, instance, table_index, elem_segment_index, dst, src, count);
  if (oob) return ThrowTableOutOfBounds(isolate, instance);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableCopy) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  CONVERT_UINT32_ARG_CHECKED(table_dst_index, 0);
  CONVERT_UINT32_ARG_CHECKED(table_src_index, 1);
  CONVERT_UINT32_ARG_CHECKED(dst, 2);
  CONVERT_UINT32_ARG_CHECKED(src, 3);
  CONVERT_UINT32_ARG_CHECKED(count, 4);

  bool oob = !WasmInstanceObject::CopyTableEntries(
      isolate, instance, table_dst_index, table_src_index, dst, src, count);
  if (oob) return ThrowTableOutOfBounds(isolate, instance);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableGrow) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  CONVERT_UINT32_ARG_CHECKED(table_index, 0);
  CONVERT_ARG_CHECKED(Object, value_raw, 1);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> value(value_raw, isolate);
  CONVERT_UINT32_ARG_CHECKED(delta, 2);

  Handle<WasmTableObject> table(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);
  int result = WasmTableObject::Grow(isolate, table, delta, value);

  return Smi::FromInt(result);
}

RUNTIME_FUNCTION(Runtime_WasmTableFill) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  CONVERT_UINT32_ARG_CHECKED(table_index, 0);
  CONVERT_UINT32_ARG_CHECKED(start, 1);
  CONVERT_ARG_CHECKED(Object, value_raw, 2);
  // TODO(mstarzinger): Manually box because parameters are not visited yet.
  Handle<Object> value(value_raw, isolate);
  CONVERT_UINT32_ARG_CHECKED(count, 3);

  Handle<WasmTableObject> table(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);

  uint32_t table_size = table->current_length();

  if (start > table_size) {
    return ThrowTableOutOfBounds(isolate, instance);
  }

  // Even when table.fill goes out-of-bounds, as many entries as possible are
  // put into the table. Only afterwards we trap.
  uint32_t fill_count = std::min(count, table_size - start);
  WasmTableObject::Fill(isolate, table, start, value, fill_count);

  if (fill_count < count) {
    return ThrowTableOutOfBounds(isolate, instance);
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmNewMultiReturnFixedArray) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_INT32_ARG_CHECKED(size, 0);
  Handle<FixedArray> fixed_array = isolate->factory()->NewFixedArray(size);
  return *fixed_array;
}

RUNTIME_FUNCTION(Runtime_WasmNewMultiReturnJSArray) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(!isolate->context().is_null());
  CONVERT_ARG_CHECKED(FixedArray, fixed_array, 0);
  Handle<FixedArray> fixed_array_handle(fixed_array, isolate);
  Handle<JSArray> array = isolate->factory()->NewJSArrayWithElements(
      fixed_array_handle, PACKED_ELEMENTS);
  return *array;
}
}  // namespace internal
}  // namespace v8
