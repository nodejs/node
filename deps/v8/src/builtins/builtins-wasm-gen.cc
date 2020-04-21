// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

class WasmBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit WasmBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  TNode<WasmInstanceObject> LoadInstanceFromFrame() {
    return CAST(
        LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset));
  }

  TNode<Context> LoadContextFromInstance(TNode<WasmInstanceObject> instance) {
    return CAST(Load(MachineType::AnyTagged(), instance,
                     IntPtrConstant(WasmInstanceObject::kNativeContextOffset -
                                    kHeapObjectTag)));
  }

  TNode<Smi> SmiFromUint32WithSaturation(TNode<Uint32T> value, uint32_t max) {
    DCHECK_LE(max, static_cast<uint32_t>(Smi::kMaxValue));
    TNode<Uint32T> capped_value = SelectConstant(
        Uint32LessThan(value, Uint32Constant(max)), value, Uint32Constant(max));
    return SmiFromUint32(capped_value);
  }
};

TF_BUILTIN(WasmStackGuard, WasmBuiltinsAssembler) {
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  TailCallRuntime(Runtime::kWasmStackGuard, context);
}

TF_BUILTIN(WasmStackOverflow, WasmBuiltinsAssembler) {
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  TailCallRuntime(Runtime::kThrowWasmStackOverflow, context);
}

TF_BUILTIN(WasmThrow, WasmBuiltinsAssembler) {
  TNode<Object> exception = CAST(Parameter(Descriptor::kException));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  TailCallRuntime(Runtime::kThrow, context, exception);
}

TF_BUILTIN(WasmRethrow, WasmBuiltinsAssembler) {
  TNode<Object> exception = CAST(Parameter(Descriptor::kException));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  Label nullref(this, Label::kDeferred);
  GotoIf(TaggedEqual(NullConstant(), exception), &nullref);

  TailCallRuntime(Runtime::kReThrow, context, exception);

  BIND(&nullref);
  MessageTemplate message_id = MessageTemplate::kWasmTrapRethrowNullRef;
  TailCallRuntime(Runtime::kThrowWasmError, context,
                  SmiConstant(static_cast<int>(message_id)));
}

TF_BUILTIN(WasmTraceMemory, WasmBuiltinsAssembler) {
  TNode<Smi> info = CAST(Parameter(Descriptor::kMemoryTracingInfo));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  TailCallRuntime(Runtime::kWasmTraceMemory, context, info);
}

TF_BUILTIN(WasmAtomicNotify, WasmBuiltinsAssembler) {
  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Uint32T> count = UncheckedCast<Uint32T>(Parameter(Descriptor::kCount));

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Number> address_number = ChangeUint32ToTagged(address);
  TNode<Number> count_number = ChangeUint32ToTagged(count);
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmAtomicNotify, context, instance,
                       address_number, count_number));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI32AtomicWait32, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<Int32T> expected_value =
      UncheckedCast<Int32T>(Parameter(Descriptor::kExpectedValue));
  TNode<Number> expected_value_number = ChangeInt32ToTagged(expected_value);

  TNode<IntPtrT> timeout_low =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutLow));
  TNode<IntPtrT> timeout_high =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutHigh));
  TNode<BigInt> timeout = BigIntFromInt32Pair(timeout_low, timeout_high);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI32AtomicWait, context, instance,
                       address_number, expected_value_number, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI32AtomicWait64, WasmBuiltinsAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<Int32T> expected_value =
      UncheckedCast<Int32T>(Parameter(Descriptor::kExpectedValue));
  TNode<Number> expected_value_number = ChangeInt32ToTagged(expected_value);

  TNode<IntPtrT> timeout_raw =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeout));
  TNode<BigInt> timeout = BigIntFromInt64(timeout_raw);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI32AtomicWait, context, instance,
                       address_number, expected_value_number, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI64AtomicWait32, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<IntPtrT> expected_value_low =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kExpectedValueLow));
  TNode<IntPtrT> expected_value_high =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kExpectedValueHigh));
  TNode<BigInt> expected_value =
      BigIntFromInt32Pair(expected_value_low, expected_value_high);

  TNode<IntPtrT> timeout_low =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutLow));
  TNode<IntPtrT> timeout_high =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeoutHigh));
  TNode<BigInt> timeout = BigIntFromInt32Pair(timeout_low, timeout_high);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI64AtomicWait, context, instance,
                       address_number, expected_value, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmI64AtomicWait64, WasmBuiltinsAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  TNode<IntPtrT> expected_value_raw =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kExpectedValue));
  TNode<BigInt> expected_value = BigIntFromInt64(expected_value_raw);

  TNode<IntPtrT> timeout_raw =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kTimeout));
  TNode<BigInt> timeout = BigIntFromInt64(timeout_raw);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI64AtomicWait, context, instance,
                       address_number, expected_value, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmMemoryGrow, WasmBuiltinsAssembler) {
  TNode<Int32T> num_pages =
      UncheckedCast<Int32T>(Parameter(Descriptor::kNumPages));
  Label num_pages_out_of_range(this, Label::kDeferred);

  TNode<BoolT> num_pages_fits_in_smi =
      IsValidPositiveSmi(ChangeInt32ToIntPtr(num_pages));
  GotoIfNot(num_pages_fits_in_smi, &num_pages_out_of_range);

  TNode<Smi> num_pages_smi = SmiFromInt32(num_pages);
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  TNode<Smi> ret_smi = CAST(
      CallRuntime(Runtime::kWasmMemoryGrow, context, instance, num_pages_smi));
  Return(SmiToInt32(ret_smi));

  BIND(&num_pages_out_of_range);
  Return(Int32Constant(-1));
}

TF_BUILTIN(WasmTableInit, WasmBuiltinsAssembler) {
  TNode<Uint32T> dst_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kDestination));
  // We cap {dst}, {src}, and {size} by {wasm::kV8MaxWasmTableSize + 1} to make
  // sure that the values fit into a Smi.
  STATIC_ASSERT(static_cast<size_t>(Smi::kMaxValue) >=
                wasm::kV8MaxWasmTableSize + 1);
  constexpr uint32_t kCap =
      static_cast<uint32_t>(wasm::kV8MaxWasmTableSize + 1);
  TNode<Smi> dst = SmiFromUint32WithSaturation(dst_raw, kCap);
  TNode<Uint32T> src_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSource));
  TNode<Smi> src = SmiFromUint32WithSaturation(src_raw, kCap);
  TNode<Uint32T> size_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSize));
  TNode<Smi> size = SmiFromUint32WithSaturation(size_raw, kCap);
  TNode<Smi> table_index =
      UncheckedCast<Smi>(Parameter(Descriptor::kTableIndex));
  TNode<Smi> segment_index =
      UncheckedCast<Smi>(Parameter(Descriptor::kSegmentIndex));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TailCallRuntime(Runtime::kWasmTableInit, context, instance, table_index,
                  segment_index, dst, src, size);
}

TF_BUILTIN(WasmTableCopy, WasmBuiltinsAssembler) {
  // We cap {dst}, {src}, and {size} by {wasm::kV8MaxWasmTableSize + 1} to make
  // sure that the values fit into a Smi.
  STATIC_ASSERT(static_cast<size_t>(Smi::kMaxValue) >=
                wasm::kV8MaxWasmTableSize + 1);
  constexpr uint32_t kCap =
      static_cast<uint32_t>(wasm::kV8MaxWasmTableSize + 1);

  TNode<Uint32T> dst_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kDestination));
  TNode<Smi> dst = SmiFromUint32WithSaturation(dst_raw, kCap);

  TNode<Uint32T> src_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSource));
  TNode<Smi> src = SmiFromUint32WithSaturation(src_raw, kCap);

  TNode<Uint32T> size_raw =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kSize));
  TNode<Smi> size = SmiFromUint32WithSaturation(size_raw, kCap);

  TNode<Smi> dst_table =
      UncheckedCast<Smi>(Parameter(Descriptor::kDestinationTable));

  TNode<Smi> src_table =
      UncheckedCast<Smi>(Parameter(Descriptor::kSourceTable));

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TailCallRuntime(Runtime::kWasmTableCopy, context, instance, dst_table,
                  src_table, dst, src, size);
}

TF_BUILTIN(WasmTableGet, WasmBuiltinsAssembler) {
  TNode<Int32T> entry_index =
      UncheckedCast<Int32T>(Parameter(Descriptor::kEntryIndex));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  Label entry_index_out_of_range(this, Label::kDeferred);

  TNode<BoolT> entry_index_fits_in_smi =
      IsValidPositiveSmi(ChangeInt32ToIntPtr(entry_index));
  GotoIfNot(entry_index_fits_in_smi, &entry_index_out_of_range);

  TNode<Smi> entry_index_smi = SmiFromInt32(entry_index);
  TNode<Smi> table_index_smi = CAST(Parameter(Descriptor::kTableIndex));

  TailCallRuntime(Runtime::kWasmFunctionTableGet, context, instance,
                  table_index_smi, entry_index_smi);

  BIND(&entry_index_out_of_range);
  MessageTemplate message_id =
      wasm::WasmOpcodes::TrapReasonToMessageId(wasm::kTrapTableOutOfBounds);
  TailCallRuntime(Runtime::kThrowWasmError, context,
                  SmiConstant(static_cast<int>(message_id)));
}

TF_BUILTIN(WasmTableSet, WasmBuiltinsAssembler) {
  TNode<Int32T> entry_index =
      UncheckedCast<Int32T>(Parameter(Descriptor::kEntryIndex));
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);
  Label entry_index_out_of_range(this, Label::kDeferred);

  TNode<BoolT> entry_index_fits_in_smi =
      IsValidPositiveSmi(ChangeInt32ToIntPtr(entry_index));
  GotoIfNot(entry_index_fits_in_smi, &entry_index_out_of_range);

  TNode<Smi> entry_index_smi = SmiFromInt32(entry_index);
  TNode<Smi> table_index_smi = CAST(Parameter(Descriptor::kTableIndex));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TailCallRuntime(Runtime::kWasmFunctionTableSet, context, instance,
                  table_index_smi, entry_index_smi, value);

  BIND(&entry_index_out_of_range);
  MessageTemplate message_id =
      wasm::WasmOpcodes::TrapReasonToMessageId(wasm::kTrapTableOutOfBounds);
  TailCallRuntime(Runtime::kThrowWasmError, context,
                  SmiConstant(static_cast<int>(message_id)));
}

#define DECLARE_THROW_RUNTIME_FN(name)                            \
  TF_BUILTIN(ThrowWasm##name, WasmBuiltinsAssembler) {            \
    TNode<WasmInstanceObject> instance = LoadInstanceFromFrame(); \
    TNode<Context> context = LoadContextFromInstance(instance);   \
    MessageTemplate message_id =                                  \
        wasm::WasmOpcodes::TrapReasonToMessageId(wasm::k##name);  \
    TailCallRuntime(Runtime::kThrowWasmError, context,            \
                    SmiConstant(static_cast<int>(message_id)));   \
  }
FOREACH_WASM_TRAPREASON(DECLARE_THROW_RUNTIME_FN)
#undef DECLARE_THROW_RUNTIME_FN

}  // namespace internal
}  // namespace v8
