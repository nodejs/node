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
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {

namespace {

template <typename FrameType, StackFrame::Type... skipped_frame_types>
class FrameFinder {
  static_assert(sizeof...(skipped_frame_types) > 0,
                "Specify at least one frame to skip");

 public:
  explicit FrameFinder(Isolate* isolate)
      : frame_iterator_(isolate, isolate->thread_local_top()) {
    for (auto type : {skipped_frame_types...}) {
      DCHECK_EQ(type, frame_iterator_.frame()->type());
      USE(type);
      frame_iterator_.Advance();
    }
    // Type check the frame where the iterator stopped now.
    DCHECK_NOT_NULL(frame());
  }

  FrameType* frame() { return FrameType::cast(frame_iterator_.frame()); }

 private:
  StackFrameIterator frame_iterator_;
};

WasmInstanceObject GetWasmInstanceOnStackTop(Isolate* isolate) {
  return FrameFinder<WasmFrame, StackFrame::EXIT>(isolate)
      .frame()
      ->wasm_instance();
}

Context GetNativeContextFromWasmInstanceOnStackTop(Isolate* isolate) {
  return GetWasmInstanceOnStackTop(isolate).native_context();
}

class V8_NODISCARD ClearThreadInWasmScope {
 public:
  ClearThreadInWasmScope() {
    DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                   trap_handler::IsThreadInWasm());
    trap_handler::ClearThreadInWasm();
  }
  ~ClearThreadInWasmScope() {
    DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                   !trap_handler::IsThreadInWasm());
    trap_handler::SetThreadInWasm();
  }
};

Object ThrowWasmError(Isolate* isolate, MessageTemplate message) {
  HandleScope scope(isolate);
  Handle<JSObject> error_obj = isolate->factory()->NewWasmRuntimeError(message);
  JSObject::AddProperty(isolate, error_obj,
                        isolate->factory()->wasm_uncatchable_symbol(),
                        isolate->factory()->true_value(), NONE);
  return isolate->Throw(*error_obj);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmIsValidRefValue) {
  // This code is called from wrappers, so the "thread is wasm" flag is not set.
  DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                 !trap_handler::IsThreadInWasm());
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0)
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  // Make sure ValueType fits properly in a Smi.
  STATIC_ASSERT(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);
  CONVERT_SMI_ARG_CHECKED(raw_type, 2);

  wasm::ValueType type = wasm::ValueType::FromRawBitField(raw_type);
  const char* error_message;

  bool result = internal::wasm::TypecheckJSObject(isolate, instance->module(),
                                                  value, type, &error_message);
  return Smi::FromInt(result);
}

RUNTIME_FUNCTION(Runtime_WasmMemoryGrow) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  // {delta_pages} is checked to be a positive smi in the WasmMemoryGrow builtin
  // which calls this runtime function.
  CONVERT_UINT32_ARG_CHECKED(delta_pages, 1);

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
  ClearThreadInWasmScope clear_wasm_flag;
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  return isolate->StackOverflow();
}

RUNTIME_FUNCTION(Runtime_WasmThrowJSTypeError) {
  // This runtime function is called both from wasm and from e.g. js-to-js
  // functions. Hence the "thread in wasm" flag can be either set or not. Both
  // is OK, since throwing will trigger unwinding anyway, which sets the flag
  // correctly depending on the handler.
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapJSTypeError));
}

RUNTIME_FUNCTION(Runtime_WasmThrowCreate) {
  ClearThreadInWasmScope clear_wasm_flag;
  // TODO(kschimpf): Can this be replaced with equivalent TurboFan code/calls.
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  DCHECK(isolate->context().is_null());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  CONVERT_ARG_CHECKED(WasmExceptionTag, tag_raw, 0);
  CONVERT_SMI_ARG_CHECKED(size, 1);
  // TODO(wasm): Manually box because parameters are not visited yet.
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

RUNTIME_FUNCTION(Runtime_WasmStackGuard) {
  ClearThreadInWasmScope wasm_flag;
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());

  // Check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) return isolate->StackOverflow();

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_WasmCompileLazy) {
  ClearThreadInWasmScope wasm_flag;
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_SMI_ARG_CHECKED(func_index, 1);

#ifdef DEBUG
  FrameFinder<WasmCompileLazyFrame, StackFrame::EXIT> frame_finder(isolate);
  DCHECK_EQ(*instance, frame_finder.frame()->wasm_instance());
#endif

  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());
  Handle<WasmModuleObject> module_object{instance->module_object(), isolate};
  bool success = wasm::CompileLazy(isolate, module_object, func_index);
  if (!success) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }

  Address entrypoint =
      module_object->native_module()->GetCallTargetForFunction(func_index);

  return Object(entrypoint);
}

namespace {
void ReplaceWrapper(Isolate* isolate, Handle<WasmInstanceObject> instance,
                    int function_index, Handle<Code> wrapper_code) {
  Handle<WasmExternalFunction> exported_function =
      WasmInstanceObject::GetWasmExternalFunction(isolate, instance,
                                                  function_index)
          .ToHandleChecked();
  exported_function->set_code(*wrapper_code);
  WasmExportedFunctionData function_data =
      exported_function->shared().wasm_exported_function_data();
  function_data.set_wrapper_code(*wrapper_code);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmCompileWrapper) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_ARG_HANDLE_CHECKED(WasmExportedFunctionData, function_data, 1);
  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());

  const wasm::WasmModule* module = instance->module();
  const int function_index = function_data->function_index();
  const wasm::WasmFunction function = module->functions[function_index];
  const wasm::FunctionSig* sig = function.sig;

  // The start function is not guaranteed to be registered as
  // an exported function (although it is called as one).
  // If there is no entry for the start function,
  // the tier-up is abandoned.
  MaybeHandle<WasmExternalFunction> maybe_exported_function =
      WasmInstanceObject::GetWasmExternalFunction(isolate, instance,
                                                  function_index);
  Handle<WasmExternalFunction> exported_function;
  if (!maybe_exported_function.ToHandle(&exported_function)) {
    DCHECK_EQ(function_index, module->start_function_index);
    return ReadOnlyRoots(isolate).undefined_value();
  }

  Handle<Code> wrapper_code =
      wasm::JSToWasmWrapperCompilationUnit::CompileSpecificJSToWasmWrapper(
          isolate, sig, module);

  // Replace the wrapper for the function that triggered the tier-up.
  // This is to verify that the wrapper is replaced, even if the function
  // is implicitly exported and is not part of the export_table.
  ReplaceWrapper(isolate, instance, function_index, wrapper_code);

  // Iterate over all exports to replace eagerly the wrapper for all functions
  // that share the signature of the function that tiered up.
  for (wasm::WasmExport exp : module->export_table) {
    if (exp.kind != wasm::kExternalFunction) {
      continue;
    }
    int index = static_cast<int>(exp.index);
    wasm::WasmFunction function = module->functions[index];
    if (function.sig == sig && index != function_index) {
      ReplaceWrapper(isolate, instance, index, wrapper_code);
    }
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTriggerTierUp) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);

  FrameFinder<WasmFrame, StackFrame::EXIT> frame_finder(isolate);
  int func_index = frame_finder.frame()->function_index();
  auto* native_module = instance->module_object().native_module();

  wasm::TriggerTierUp(isolate, native_module, func_index);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmAtomicNotify) {
  ClearThreadInWasmScope clear_wasm_flag;
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_DOUBLE_ARG_CHECKED(offset_double, 1);
  uintptr_t offset = static_cast<uintptr_t>(offset_double);
  CONVERT_NUMBER_CHECKED(uint32_t, count, Uint32, args[2]);
  Handle<JSArrayBuffer> array_buffer{instance->memory_object().array_buffer(),
                                     isolate};
  // Should have trapped if address was OOB.
  DCHECK_LT(offset, array_buffer->byte_length());
  if (!array_buffer->is_shared()) return Smi::FromInt(0);
  return FutexEmulation::Wake(array_buffer, offset, count);
}

RUNTIME_FUNCTION(Runtime_WasmI32AtomicWait) {
  ClearThreadInWasmScope clear_wasm_flag;
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_DOUBLE_ARG_CHECKED(offset_double, 1);
  uintptr_t offset = static_cast<uintptr_t>(offset_double);
  CONVERT_NUMBER_CHECKED(int32_t, expected_value, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(BigInt, timeout_ns, 3);

  Handle<JSArrayBuffer> array_buffer{instance->memory_object().array_buffer(),
                                     isolate};
  // Should have trapped if address was OOB.
  DCHECK_LT(offset, array_buffer->byte_length());

  // Trap if memory is not shared.
  if (!array_buffer->is_shared()) {
    return ThrowWasmError(isolate, MessageTemplate::kAtomicsWaitNotAllowed);
  }
  return FutexEmulation::WaitWasm32(isolate, array_buffer, offset,
                                    expected_value, timeout_ns->AsInt64());
}

RUNTIME_FUNCTION(Runtime_WasmI64AtomicWait) {
  ClearThreadInWasmScope clear_wasm_flag;
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_DOUBLE_ARG_CHECKED(offset_double, 1);
  uintptr_t offset = static_cast<uintptr_t>(offset_double);
  CONVERT_ARG_HANDLE_CHECKED(BigInt, expected_value, 2);
  CONVERT_ARG_HANDLE_CHECKED(BigInt, timeout_ns, 3);

  Handle<JSArrayBuffer> array_buffer{instance->memory_object().array_buffer(),
                                     isolate};
  // Should have trapped if address was OOB.
  DCHECK_LT(offset, array_buffer->byte_length());

  // Trap if memory is not shared.
  if (!array_buffer->is_shared()) {
    return ThrowWasmError(isolate, MessageTemplate::kAtomicsWaitNotAllowed);
  }
  return FutexEmulation::WaitWasm64(isolate, array_buffer, offset,
                                    expected_value->AsInt64(),
                                    timeout_ns->AsInt64());
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
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(function_index, 1);

  Handle<WasmExternalFunction> function =
      WasmInstanceObject::GetOrCreateWasmExternalFunction(isolate, instance,
                                                          function_index);

  return *function;
}

RUNTIME_FUNCTION(Runtime_WasmFunctionTableGet) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(table_index, 1);
  CONVERT_UINT32_ARG_CHECKED(entry_index, 2);
  DCHECK_LT(table_index, instance->tables().length());
  auto table = handle(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);
  // We only use the runtime call for lazily initialized function references.
  DCHECK(
      table->instance().IsUndefined()
          ? table->type() == wasm::kWasmFuncRef
          : IsSubtypeOf(table->type(), wasm::kWasmFuncRef,
                        WasmInstanceObject::cast(table->instance()).module()));

  if (!WasmTableObject::IsInBounds(isolate, table, entry_index)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
  }

  return *WasmTableObject::Get(isolate, table, entry_index);
}

RUNTIME_FUNCTION(Runtime_WasmFunctionTableSet) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(table_index, 1);
  CONVERT_UINT32_ARG_CHECKED(entry_index, 2);
  CONVERT_ARG_CHECKED(Object, element_raw, 3);
  // TODO(wasm): Manually box because parameters are not visited yet.
  Handle<Object> element(element_raw, isolate);
  DCHECK_LT(table_index, instance->tables().length());
  auto table = handle(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);
  // We only use the runtime call for function references.
  DCHECK(
      table->instance().IsUndefined()
          ? table->type() == wasm::kWasmFuncRef
          : IsSubtypeOf(table->type(), wasm::kWasmFuncRef,
                        WasmInstanceObject::cast(table->instance()).module()));

  if (!WasmTableObject::IsInBounds(isolate, table, entry_index)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
  }
  WasmTableObject::Set(isolate, table, entry_index, element);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableInit) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(table_index, 1);
  CONVERT_UINT32_ARG_CHECKED(elem_segment_index, 2);
  static_assert(
      wasm::kV8MaxWasmTableSize < kSmiMaxValue,
      "Make sure clamping to Smi range doesn't make an invalid call valid");
  CONVERT_UINT32_ARG_CHECKED(dst, 3);
  CONVERT_UINT32_ARG_CHECKED(src, 4);
  CONVERT_UINT32_ARG_CHECKED(count, 5);

  DCHECK(!isolate->context().is_null());

  bool oob = !WasmInstanceObject::InitTableEntries(
      isolate, instance, table_index, elem_segment_index, dst, src, count);
  if (oob) return ThrowTableOutOfBounds(isolate, instance);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableCopy) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  CONVERT_ARG_HANDLE_CHECKED(WasmInstanceObject, instance, 0);
  CONVERT_UINT32_ARG_CHECKED(table_dst_index, 1);
  CONVERT_UINT32_ARG_CHECKED(table_src_index, 2);
  static_assert(
      wasm::kV8MaxWasmTableSize < kSmiMaxValue,
      "Make sure clamping to Smi range doesn't make an invalid call valid");
  CONVERT_UINT32_ARG_CHECKED(dst, 3);
  CONVERT_UINT32_ARG_CHECKED(src, 4);
  CONVERT_UINT32_ARG_CHECKED(count, 5);

  DCHECK(!isolate->context().is_null());

  bool oob = !WasmInstanceObject::CopyTableEntries(
      isolate, instance, table_dst_index, table_src_index, dst, src, count);
  if (oob) return ThrowTableOutOfBounds(isolate, instance);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableGrow) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  CONVERT_UINT32_ARG_CHECKED(table_index, 0);
  CONVERT_ARG_CHECKED(Object, value_raw, 1);
  // TODO(wasm): Manually box because parameters are not visited yet.
  Handle<Object> value(value_raw, isolate);
  CONVERT_UINT32_ARG_CHECKED(delta, 2);

  Handle<WasmTableObject> table(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);
  int result = WasmTableObject::Grow(isolate, table, delta, value);

  return Smi::FromInt(result);
}

RUNTIME_FUNCTION(Runtime_WasmTableFill) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  auto instance =
      Handle<WasmInstanceObject>(GetWasmInstanceOnStackTop(isolate), isolate);
  CONVERT_UINT32_ARG_CHECKED(table_index, 0);
  CONVERT_UINT32_ARG_CHECKED(start, 1);
  CONVERT_ARG_CHECKED(Object, value_raw, 2);
  // TODO(wasm): Manually box because parameters are not visited yet.
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
  if (fill_count < count) {
    return ThrowTableOutOfBounds(isolate, instance);
  }
  WasmTableObject::Fill(isolate, table, start, value, fill_count);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmDebugBreak) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  FrameFinder<WasmFrame, StackFrame::EXIT, StackFrame::WASM_DEBUG_BREAK>
      frame_finder(isolate);
  auto instance = handle(frame_finder.frame()->wasm_instance(), isolate);
  int position = frame_finder.frame()->position();
  isolate->set_context(instance->native_context());

  // Enter the debugger.
  DebugScope debug_scope(isolate->debug());

  WasmFrame* frame = frame_finder.frame();
  auto* debug_info = frame->native_module()->GetDebugInfo();
  if (debug_info->IsStepping(frame)) {
    debug_info->ClearStepping(isolate);
    StepAction stepAction = isolate->debug()->last_step_action();
    isolate->debug()->ClearStepping();
    isolate->debug()->OnDebugBreak(isolate->factory()->empty_fixed_array(),
                                   stepAction);
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Check whether we hit a breakpoint.
  Handle<Script> script(instance->module_object().script(), isolate);
  Handle<FixedArray> breakpoints;
  if (WasmScript::CheckBreakPoints(isolate, script, position)
          .ToHandle(&breakpoints)) {
    debug_info->ClearStepping(isolate);
    StepAction stepAction = isolate->debug()->last_step_action();
    isolate->debug()->ClearStepping();
    if (isolate->debug()->break_points_active()) {
      // We hit one or several breakpoints. Notify the debug listeners.
      isolate->debug()->OnDebugBreak(breakpoints, stepAction);
    }
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmAllocateRtt) {
  ClearThreadInWasmScope flag_scope;
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_UINT32_ARG_CHECKED(type_index, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, parent, 1);
  Handle<WasmInstanceObject> instance(GetWasmInstanceOnStackTop(isolate),
                                      isolate);
  return *wasm::AllocateSubRtt(isolate, instance, type_index, parent);
}

}  // namespace internal
}  // namespace v8
