// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-wasm-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {

TNode<WasmInstanceObject> WasmBuiltinsAssembler::LoadInstanceFromFrame() {
  return CAST(LoadFromParentFrame(WasmFrameConstants::kWasmInstanceOffset));
}

TNode<NativeContext> WasmBuiltinsAssembler::LoadContextFromInstance(
    TNode<WasmInstanceObject> instance) {
  return CAST(Load(MachineType::AnyTagged(), instance,
                   IntPtrConstant(WasmInstanceObject::kNativeContextOffset -
                                  kHeapObjectTag)));
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadTablesFromInstance(
    TNode<WasmInstanceObject> instance) {
  return LoadObjectField<FixedArray>(instance,
                                     WasmInstanceObject::kTablesOffset);
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadExternalFunctionsFromInstance(
    TNode<WasmInstanceObject> instance) {
  return LoadObjectField<FixedArray>(
      instance, WasmInstanceObject::kWasmExternalFunctionsOffset);
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadManagedObjectMapsFromInstance(
    TNode<WasmInstanceObject> instance) {
  return LoadObjectField<FixedArray>(
      instance, WasmInstanceObject::kManagedObjectMapsOffset);
}

TF_BUILTIN(WasmFloat32ToNumber, WasmBuiltinsAssembler) {
  auto val = UncheckedParameter<Float32T>(Descriptor::kValue);
  Return(ChangeFloat32ToTagged(val));
}

TF_BUILTIN(WasmFloat64ToNumber, WasmBuiltinsAssembler) {
  auto val = UncheckedParameter<Float64T>(Descriptor::kValue);
  Return(ChangeFloat64ToTagged(val));
}

TF_BUILTIN(WasmI32AtomicWait32, WasmBuiltinsAssembler) {
  if (!Is32()) {
    Unreachable();
    return;
  }

  auto address = UncheckedParameter<Uint32T>(Descriptor::kAddress);
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  auto expected_value = UncheckedParameter<Int32T>(Descriptor::kExpectedValue);
  TNode<Number> expected_value_number = ChangeInt32ToTagged(expected_value);

  auto timeout_low = UncheckedParameter<IntPtrT>(Descriptor::kTimeoutLow);
  auto timeout_high = UncheckedParameter<IntPtrT>(Descriptor::kTimeoutHigh);
  TNode<BigInt> timeout = BigIntFromInt32Pair(timeout_low, timeout_high);

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

  auto address = UncheckedParameter<Uint32T>(Descriptor::kAddress);
  TNode<Number> address_number = ChangeUint32ToTagged(address);

  auto expected_value_low =
      UncheckedParameter<IntPtrT>(Descriptor::kExpectedValueLow);
  auto expected_value_high =
      UncheckedParameter<IntPtrT>(Descriptor::kExpectedValueHigh);
  TNode<BigInt> expected_value =
      BigIntFromInt32Pair(expected_value_low, expected_value_high);

  auto timeout_low = UncheckedParameter<IntPtrT>(Descriptor::kTimeoutLow);
  auto timeout_high = UncheckedParameter<IntPtrT>(Descriptor::kTimeoutHigh);
  TNode<BigInt> timeout = BigIntFromInt32Pair(timeout_low, timeout_high);

  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Context> context = LoadContextFromInstance(instance);

  TNode<Smi> result_smi =
      CAST(CallRuntime(Runtime::kWasmI64AtomicWait, context, instance,
                       address_number, expected_value, timeout));
  Return(Unsigned(SmiToInt32(result_smi)));
}

TF_BUILTIN(WasmAllocateArrayWithRtt, WasmBuiltinsAssembler) {
  auto map = Parameter<Map>(Descriptor::kMap);
  auto length = Parameter<Smi>(Descriptor::kLength);
  auto element_size = Parameter<Smi>(Descriptor::kElementSize);
  TNode<IntPtrT> untagged_length = SmiUntag(length);
  // instance_size = WasmArray::kHeaderSize
  //               + RoundUp(element_size * length, kObjectAlignment)
  TNode<IntPtrT> raw_size = IntPtrMul(SmiUntag(element_size), untagged_length);
  TNode<IntPtrT> rounded_size =
      WordAnd(IntPtrAdd(raw_size, IntPtrConstant(kObjectAlignmentMask)),
              IntPtrConstant(~kObjectAlignmentMask));
  TNode<IntPtrT> instance_size =
      IntPtrAdd(IntPtrConstant(WasmArray::kHeaderSize), rounded_size);
  TNode<WasmArray> result = UncheckedCast<WasmArray>(Allocate(instance_size));
  StoreMap(result, map);
  StoreObjectFieldNoWriteBarrier(result, WasmArray::kLengthOffset,
                                 TruncateIntPtrToInt32(untagged_length));
  Return(result);
}

TF_BUILTIN(WasmAllocatePair, WasmBuiltinsAssembler) {
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<HeapObject> value1 = Parameter<HeapObject>(Descriptor::kValue1);
  TNode<HeapObject> value2 = Parameter<HeapObject>(Descriptor::kValue2);

  TNode<IntPtrT> roots = LoadObjectField<IntPtrT>(
      instance, WasmInstanceObject::kIsolateRootOffset);
  TNode<Map> map = CAST(Load(
      MachineType::AnyTagged(), roots,
      IntPtrConstant(IsolateData::root_slot_offset(RootIndex::kTuple2Map))));

  TNode<IntPtrT> instance_size =
      TimesTaggedSize(LoadMapInstanceSizeInWords(map));
  TNode<Tuple2> result = UncheckedCast<Tuple2>(Allocate(instance_size));

  StoreMap(result, map);
  StoreObjectField(result, Tuple2::kValue1Offset, value1);
  StoreObjectField(result, Tuple2::kValue2Offset, value2);

  Return(result);
}

}  // namespace internal
}  // namespace v8
