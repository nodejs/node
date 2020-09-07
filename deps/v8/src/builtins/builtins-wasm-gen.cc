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
  TNode<Float32T> val = UncheckedCast<Float32T>(Parameter(Descriptor::kValue));
  Return(ChangeFloat32ToTagged(val));
}

TF_BUILTIN(WasmFloat64ToNumber, WasmBuiltinsAssembler) {
  TNode<Float64T> val = UncheckedCast<Float64T>(Parameter(Descriptor::kValue));
  Return(ChangeFloat64ToTagged(val));
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

TF_BUILTIN(WasmAllocateArray, WasmBuiltinsAssembler) {
  TNode<WasmInstanceObject> instance = LoadInstanceFromFrame();
  TNode<Smi> map_index = CAST(Parameter(Descriptor::kMapIndex));
  TNode<Smi> length = CAST(Parameter(Descriptor::kLength));
  TNode<Smi> element_size = CAST(Parameter(Descriptor::kElementSize));
  TNode<FixedArray> maps_list = LoadObjectField<FixedArray>(
      instance, WasmInstanceObject::kManagedObjectMapsOffset);
  TNode<Map> map = CAST(LoadFixedArrayElement(maps_list, map_index));
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

}  // namespace internal
}  // namespace v8
