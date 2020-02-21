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
  TNode<Object> UncheckedParameter(int index) {
    return UncheckedCast<Object>(Parameter(index));
  }

  TNode<Code> LoadBuiltinFromFrame(Builtins::Name id) {
    TNode<Object> instance = LoadInstanceFromFrame();
    TNode<IntPtrT> isolate_root = UncheckedCast<IntPtrT>(
        Load(MachineType::Pointer(), instance,
             IntPtrConstant(WasmInstanceObject::kIsolateRootOffset -
                            kHeapObjectTag)));
    TNode<Code> target = UncheckedCast<Code>(
        Load(MachineType::TaggedPointer(), isolate_root,
             IntPtrConstant(IsolateData::builtin_slot_offset(id))));
    return target;
  }

  TNode<Object> LoadInstanceFromFrame() {
    return UncheckedCast<Object>(
        LoadFromParentFrame(WasmCompiledFrameConstants::kWasmInstanceOffset));
  }

  TNode<Object> LoadContextFromInstance(TNode<Object> instance) {
    return UncheckedCast<Object>(
        Load(MachineType::AnyTagged(), instance,
             IntPtrConstant(WasmInstanceObject::kNativeContextOffset -
                            kHeapObjectTag)));
  }

  TNode<Code> LoadCEntryFromInstance(TNode<Object> instance) {
    TNode<IntPtrT> isolate_root = UncheckedCast<IntPtrT>(
        Load(MachineType::Pointer(), instance,
             IntPtrConstant(WasmInstanceObject::kIsolateRootOffset -
                            kHeapObjectTag)));
    auto centry_id =
        Builtins::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit;
    TNode<Code> target = UncheckedCast<Code>(
        Load(MachineType::TaggedPointer(), isolate_root,
             IntPtrConstant(IsolateData::builtin_slot_offset(centry_id))));
    return target;
  }
};

TF_BUILTIN(WasmAllocateHeapNumber, WasmBuiltinsAssembler) {
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kAllocateHeapNumber);
  TailCallStub(AllocateHeapNumberDescriptor(), target, NoContextConstant());
}

TF_BUILTIN(WasmRecordWrite, WasmBuiltinsAssembler) {
  TNode<Object> object = UncheckedParameter(Descriptor::kObject);
  TNode<Object> slot = UncheckedParameter(Descriptor::kSlot);
  TNode<Object> remembered = UncheckedParameter(Descriptor::kRememberedSet);
  TNode<Object> fp_mode = UncheckedParameter(Descriptor::kFPMode);
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kRecordWrite);
  TailCallStub(RecordWriteDescriptor{}, target, NoContextConstant(), object,
               slot, remembered, fp_mode);
}

TF_BUILTIN(WasmToNumber, WasmBuiltinsAssembler) {
  TNode<Object> context = UncheckedParameter(Descriptor::kContext);
  TNode<Object> argument = UncheckedParameter(Descriptor::kArgument);
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kToNumber);
  TailCallStub(TypeConversionDescriptor(), target, context, argument);
}

TF_BUILTIN(WasmStackGuard, WasmBuiltinsAssembler) {
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  TailCallRuntimeWithCEntry(Runtime::kWasmStackGuard, centry, context);
}

TF_BUILTIN(WasmStackOverflow, WasmBuiltinsAssembler) {
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  TailCallRuntimeWithCEntry(Runtime::kThrowWasmStackOverflow, centry, context);
}

TF_BUILTIN(WasmThrow, WasmBuiltinsAssembler) {
  TNode<Object> exception = UncheckedParameter(Descriptor::kException);
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  TailCallRuntimeWithCEntry(Runtime::kThrow, centry, context, exception);
}

TF_BUILTIN(WasmRethrow, WasmBuiltinsAssembler) {
  TNode<Object> exception = UncheckedParameter(Descriptor::kException);
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  TailCallRuntimeWithCEntry(Runtime::kReThrow, centry, context, exception);
}

TF_BUILTIN(WasmAtomicNotify, WasmBuiltinsAssembler) {
  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Uint32T> count = UncheckedCast<Uint32T>(Parameter(Descriptor::kCount));

  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);

  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kAllocateHeapNumber);
  TNode<Object> context = LoadContextFromInstance(instance);

  // TODO(aseemgarg): Use SMIs if possible for address and count
  TNode<HeapNumber> address_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(address_heap, ChangeUint32ToFloat64(address));

  TNode<HeapNumber> count_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(count_heap, ChangeUint32ToFloat64(count));

  TNode<Smi> result_smi = UncheckedCast<Smi>(CallRuntimeWithCEntry(
      Runtime::kWasmAtomicNotify, centry, context, instance,
      address_heap, count_heap));
  ReturnRaw(SmiToInt32(result_smi));
}

TF_BUILTIN(WasmI32AtomicWait, WasmBuiltinsAssembler) {
  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Int32T> expected_value =
      UncheckedCast<Int32T>(Parameter(Descriptor::kExpectedValue));
  TNode<Float64T> timeout =
      UncheckedCast<Float64T>(Parameter(Descriptor::kTimeout));

  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);

  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kAllocateHeapNumber);
  TNode<Object> context = LoadContextFromInstance(instance);

  // TODO(aseemgarg): Use SMIs if possible for address and expected_value
  TNode<HeapNumber> address_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(address_heap, ChangeUint32ToFloat64(address));

  TNode<HeapNumber> expected_value_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(expected_value_heap,
                       ChangeInt32ToFloat64(expected_value));

  TNode<HeapNumber> timeout_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(timeout_heap, timeout);

  TNode<Smi> result_smi = UncheckedCast<Smi>(CallRuntimeWithCEntry(
      Runtime::kWasmI32AtomicWait, centry, context, instance,
      address_heap, expected_value_heap, timeout_heap));
  ReturnRaw(SmiToInt32(result_smi));
}

TF_BUILTIN(WasmI64AtomicWait, WasmBuiltinsAssembler) {
  TNode<Uint32T> address =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kAddress));
  TNode<Uint32T> expected_value_high =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kExpectedValueHigh));
  TNode<Uint32T> expected_value_low =
      UncheckedCast<Uint32T>(Parameter(Descriptor::kExpectedValueLow));
  TNode<Float64T> timeout =
      UncheckedCast<Float64T>(Parameter(Descriptor::kTimeout));

  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);

  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kAllocateHeapNumber);
  TNode<Object> context = LoadContextFromInstance(instance);

  // TODO(aseemgarg): Use SMIs if possible for address and expected_value
  TNode<HeapNumber> address_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(address_heap, ChangeUint32ToFloat64(address));

  TNode<HeapNumber> expected_value_high_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(expected_value_high_heap,
                       ChangeUint32ToFloat64(expected_value_high));

  TNode<HeapNumber> expected_value_low_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(expected_value_low_heap,
                       ChangeUint32ToFloat64(expected_value_low));

  TNode<HeapNumber> timeout_heap = UncheckedCast<HeapNumber>(
      CallStub(AllocateHeapNumberDescriptor(), target, context));
  StoreHeapNumberValue(timeout_heap, timeout);

  TNode<Smi> result_smi = UncheckedCast<Smi>(CallRuntimeWithCEntry(
      Runtime::kWasmI64AtomicWait, centry, context, instance,
      address_heap, expected_value_high_heap, expected_value_low_heap,
      timeout_heap));
  ReturnRaw(SmiToInt32(result_smi));
}

TF_BUILTIN(WasmMemoryGrow, WasmBuiltinsAssembler) {
  TNode<Int32T> num_pages =
      UncheckedCast<Int32T>(Parameter(Descriptor::kNumPages));
  Label num_pages_out_of_range(this, Label::kDeferred);

  TNode<BoolT> num_pages_fits_in_smi =
      IsValidPositiveSmi(ChangeInt32ToIntPtr(num_pages));
  GotoIfNot(num_pages_fits_in_smi, &num_pages_out_of_range);

  TNode<Smi> num_pages_smi = SmiFromInt32(num_pages);
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  TNode<Smi> ret_smi = UncheckedCast<Smi>(CallRuntimeWithCEntry(
      Runtime::kWasmMemoryGrow, centry, context, instance, num_pages_smi));
  TNode<Int32T> ret = SmiToInt32(ret_smi);
  ReturnRaw(ret);

  BIND(&num_pages_out_of_range);
  ReturnRaw(Int32Constant(-1));
}

TF_BUILTIN(WasmTableGet, WasmBuiltinsAssembler) {
  TNode<Int32T> entry_index =
      UncheckedCast<Int32T>(Parameter(Descriptor::kEntryIndex));
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  Label entry_index_out_of_range(this, Label::kDeferred);

  TNode<BoolT> entry_index_fits_in_smi =
      IsValidPositiveSmi(ChangeInt32ToIntPtr(entry_index));
  GotoIfNot(entry_index_fits_in_smi, &entry_index_out_of_range);

  TNode<Smi> entry_index_smi = SmiFromInt32(entry_index);
  TNode<Smi> table_index_smi =
      UncheckedCast<Smi>(Parameter(Descriptor::kTableIndex));

  TailCallRuntimeWithCEntry(Runtime::kWasmFunctionTableGet, centry, context,
                            instance, table_index_smi, entry_index_smi);

  BIND(&entry_index_out_of_range);
  MessageTemplate message_id =
      wasm::WasmOpcodes::TrapReasonToMessageId(wasm::kTrapTableOutOfBounds);
  TailCallRuntimeWithCEntry(Runtime::kThrowWasmError, centry, context,
                            SmiConstant(static_cast<int>(message_id)));
}

TF_BUILTIN(WasmTableSet, WasmBuiltinsAssembler) {
  TNode<Int32T> entry_index =
      UncheckedCast<Int32T>(Parameter(Descriptor::kEntryIndex));
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  Label entry_index_out_of_range(this, Label::kDeferred);

  TNode<BoolT> entry_index_fits_in_smi =
      IsValidPositiveSmi(ChangeInt32ToIntPtr(entry_index));
  GotoIfNot(entry_index_fits_in_smi, &entry_index_out_of_range);

  TNode<Smi> entry_index_smi = SmiFromInt32(entry_index);
  TNode<Smi> table_index_smi =
      UncheckedCast<Smi>(Parameter(Descriptor::kTableIndex));
  TNode<Object> value = UncheckedCast<Object>(Parameter(Descriptor::kValue));
  TailCallRuntimeWithCEntry(Runtime::kWasmFunctionTableSet, centry, context,
                            instance, table_index_smi, entry_index_smi, value);

  BIND(&entry_index_out_of_range);
  MessageTemplate message_id =
      wasm::WasmOpcodes::TrapReasonToMessageId(wasm::kTrapTableOutOfBounds);
  TailCallRuntimeWithCEntry(Runtime::kThrowWasmError, centry, context,
                            SmiConstant(static_cast<int>(message_id)));
}

TF_BUILTIN(WasmI64ToBigInt, WasmBuiltinsAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kI64ToBigInt);
  TNode<IntPtrT> argument =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kArgument));

  TailCallStub(I64ToBigIntDescriptor(), target, NoContextConstant(), argument);
}

TF_BUILTIN(WasmI32PairToBigInt, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kI32PairToBigInt);
  TNode<IntPtrT> low = UncheckedCast<IntPtrT>(Parameter(Descriptor::kLow));
  TNode<IntPtrT> high = UncheckedCast<IntPtrT>(Parameter(Descriptor::kHigh));

  TailCallStub(I32PairToBigIntDescriptor(), target, NoContextConstant(), low,
               high);
}

TF_BUILTIN(WasmBigIntToI64, WasmBuiltinsAssembler) {
  if (!Is64()) {
    Unreachable();
    return;
  }

  TNode<Object> context =
      UncheckedCast<Object>(Parameter(Descriptor::kContext));
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kBigIntToI64);
  TNode<IntPtrT> argument =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kArgument));

  TailCallStub(BigIntToI64Descriptor(), target, context, argument);
}

TF_BUILTIN(WasmBigIntToI32Pair, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  TNode<Object> context =
      UncheckedCast<Object>(Parameter(Descriptor::kContext));
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kBigIntToI32Pair);
  TNode<IntPtrT> argument =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kArgument));

  TailCallStub(BigIntToI32PairDescriptor(), target, context, argument);
}

#define DECLARE_ENUM(name)                                                \
  TF_BUILTIN(ThrowWasm##name, WasmBuiltinsAssembler) {                    \
    TNode<Object> instance = LoadInstanceFromFrame();                     \
    TNode<Code> centry = LoadCEntryFromInstance(instance);                \
    TNode<Object> context = LoadContextFromInstance(instance);            \
    MessageTemplate message_id =                                          \
        wasm::WasmOpcodes::TrapReasonToMessageId(wasm::k##name);          \
    TailCallRuntimeWithCEntry(Runtime::kThrowWasmError, centry, context,  \
                              SmiConstant(static_cast<int>(message_id))); \
  }
FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
#undef DECLARE_ENUM

}  // namespace internal
}  // namespace v8
