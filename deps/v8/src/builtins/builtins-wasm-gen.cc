// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-wasm-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-objects.h"

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

TF_BUILTIN(JSToWasmLazyDeoptContinuation, WasmBuiltinsAssembler) {
  // Reset thread_in_wasm_flag.
  TNode<ExternalReference> thread_in_wasm_flag_address_address =
      ExternalConstant(
          ExternalReference::thread_in_wasm_flag_address_address(isolate()));
  auto thread_in_wasm_flag_address =
      Load<RawPtrT>(thread_in_wasm_flag_address_address);
  StoreNoWriteBarrier(MachineRepresentation::kWord32,
                      thread_in_wasm_flag_address, Int32Constant(0));

  // Return the argument.
  auto value = Parameter<Object>(Descriptor::kArgument);
  Return(value);
}

}  // namespace internal
}  // namespace v8
