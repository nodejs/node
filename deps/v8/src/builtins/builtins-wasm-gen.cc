// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-wasm-gen.h"

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler-inl.h"
#include "src/codegen/interface-descriptors.h"
#include "src/objects/map-inl.h"
#include "src/objects/objects-inl.h"
#include "src/wasm/wasm-objects.h"

namespace v8::internal {

#include "src/codegen/define-code-stub-assembler-macros.inc"

TNode<WasmTrustedInstanceData>
WasmBuiltinsAssembler::LoadInstanceDataFromFrame() {
  return CAST(LoadFromParentFrame(WasmFrameConstants::kWasmInstanceDataOffset));
}

TNode<WasmTrustedInstanceData>
WasmBuiltinsAssembler::LoadTrustedDataFromInstance(
    TNode<WasmInstanceObject> instance_object) {
  return CAST(LoadTrustedPointerFromObject(
      instance_object, WasmInstanceObject::kTrustedDataOffset,
      kWasmTrustedInstanceDataIndirectPointerTag));
}

TNode<NativeContext> WasmBuiltinsAssembler::LoadContextFromWasmOrJsFrame() {
  static_assert(BuiltinFrameConstants::kFunctionOffset ==
                WasmFrameConstants::kWasmInstanceDataOffset);
  TVARIABLE(NativeContext, context_result);
  TNode<HeapObject> function_or_instance =
      CAST(LoadFromParentFrame(WasmFrameConstants::kWasmInstanceDataOffset));
  Label is_js_function(this);
  Label is_import_data(this);
  Label done(this);
  TNode<Uint16T> instance_type =
      LoadMapInstanceType(LoadMap(function_or_instance));
  GotoIf(IsJSFunctionInstanceType(instance_type), &is_js_function);
  GotoIf(Word32Equal(instance_type, Int32Constant(WASM_IMPORT_DATA_TYPE)),
         &is_import_data);
  context_result = LoadContextFromInstanceData(CAST(function_or_instance));
  Goto(&done);

  BIND(&is_js_function);
  TNode<JSFunction> function = CAST(function_or_instance);
  TNode<Context> context =
      LoadObjectField<Context>(function, JSFunction::kContextOffset);
  context_result = LoadNativeContext(context);
  Goto(&done);

  BIND(&is_import_data);
  TNode<WasmImportData> import_data = CAST(function_or_instance);
  context_result = LoadObjectField<NativeContext>(
      import_data, WasmImportData::kNativeContextOffset);
  Goto(&done);

  BIND(&done);
  return context_result.value();
}

TNode<NativeContext> WasmBuiltinsAssembler::LoadContextFromInstanceData(
    TNode<WasmTrustedInstanceData> trusted_data) {
  return CAST(
      Load(MachineType::AnyTagged(), trusted_data,
           IntPtrConstant(WasmTrustedInstanceData::kNativeContextOffset -
                          kHeapObjectTag)));
}

TNode<WasmTrustedInstanceData>
WasmBuiltinsAssembler::LoadSharedPartFromInstanceData(
    TNode<WasmTrustedInstanceData> trusted_data) {
  return CAST(LoadProtectedPointerFromObject(
      trusted_data,
      IntPtrConstant(WasmTrustedInstanceData::kProtectedSharedPartOffset -
                     kHeapObjectTag)));
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadTablesFromInstanceData(
    TNode<WasmTrustedInstanceData> trusted_data) {
  return LoadObjectField<FixedArray>(trusted_data,
                                     WasmTrustedInstanceData::kTablesOffset);
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadFuncRefsFromInstanceData(
    TNode<WasmTrustedInstanceData> trusted_data) {
  return LoadObjectField<FixedArray>(trusted_data,
                                     WasmTrustedInstanceData::kFuncRefsOffset);
}

TNode<FixedArray> WasmBuiltinsAssembler::LoadManagedObjectMapsFromInstanceData(
    TNode<WasmTrustedInstanceData> trusted_data) {
  return LoadObjectField<FixedArray>(
      trusted_data, WasmTrustedInstanceData::kManagedObjectMapsOffset);
}

TNode<Float64T> WasmBuiltinsAssembler::StringToFloat64(TNode<String> input) {
#ifdef V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
  TNode<ExternalReference> string_to_float64 =
      ExternalConstant(ExternalReference::wasm_string_to_f64());
  return TNode<Float64T>::UncheckedCast(
      CallCFunction(string_to_float64, MachineType::Float64(),
                    std::make_pair(MachineType::AnyTagged(), input)));
#else
  // We could support the fast path by passing the float via a stackslot, see
  // MachineOperatorBuilder::StackSlot.
  TNode<Object> result =
      CallRuntime(Runtime::kStringParseFloat, NoContextConstant(), input);
  return ChangeNumberToFloat64(CAST(result));
#endif
}

TF_BUILTIN(WasmFloat32ToNumber, WasmBuiltinsAssembler) {
  auto val = UncheckedParameter<Float32T>(Descriptor::kValue);
  Return(ChangeFloat32ToTagged(val));
}

TF_BUILTIN(WasmFloat64ToNumber, WasmBuiltinsAssembler) {
  auto val = UncheckedParameter<Float64T>(Descriptor::kValue);
  Return(ChangeFloat64ToTagged(val));
}

TF_BUILTIN(WasmFloat64ToString, WasmBuiltinsAssembler) {
  TNode<Float64T> val = UncheckedParameter<Float64T>(Descriptor::kValue);
  Return(Float64ToString(val));
}

TF_BUILTIN(JSToWasmLazyDeoptContinuation, WasmBuiltinsAssembler) {
  // Return the argument.
  auto value = Parameter<Object>(Descriptor::kArgument);
  Return(value);
}

TF_BUILTIN(WasmToJsWrapperCSA, WasmBuiltinsAssembler) {
  TorqueStructWasmToJSResult result = WasmToJSWrapper(
      UncheckedParameter<WasmImportData>(Descriptor::kWasmImportData));
  PopAndReturn(result.popCount, result.result0, result.result1, result.result2,
               result.result3);
}

TF_BUILTIN(WasmToJsWrapperInvalidSig, WasmBuiltinsAssembler) {
  TNode<WasmImportData> data =
      UncheckedParameter<WasmImportData>(Descriptor::kWasmImportData);
  TNode<Context> context =
      LoadObjectField<Context>(data, WasmImportData::kNativeContextOffset);

  CallRuntime(Runtime::kWasmThrowJSTypeError, context);
  Unreachable();
}

// Suppose we wanted to generate JavaScript constructor functions that wrap
// exported Wasm functions as follows:
//
//   function MakeConstructor(wasm_instance, name) {
//     let wasm_func = wasm_instance.exports[name];
//     return function(...args) {
//       return wasm_func(...args);
//     }
//   }
//   let Foo = MakeConstructor(...);
//   let foo = new Foo(1, 2, 3);
//
// This builtin models the code that these functions would have: it fetches the
// target Wasm function from a Context slot and tail-calls to it with the
// existing arguments on the stack. So when mass-creating such constructors,
// we don't need to compile any bytecode, we only need to allocate an
// appropriate Context and use this builtin as the code.
TF_BUILTIN(WasmConstructorWrapper, WasmBuiltinsAssembler) {
  auto argc = UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount);
  TNode<Context> context = Parameter<Context>(Descriptor::kContext);
  static constexpr int kSlot = wasm::kConstructorFunctionContextSlot;
  TNode<JSFunction> target = CAST(LoadContextElementNoCell(context, kSlot));
  TailCallBuiltin(Builtin::kCallFunction_ReceiverIsNullOrUndefined, context,
                  target, argc);
}

// Similar, but for exported Wasm functions that can be called as methods,
// i.e. that pass their JS-side receiver as their Wasm-side first parameter.
// To wrap a Wasm function like the following:
//
//   (func (export "bar") (param $recv externref) (param $other ...)
//     (do-something-with $recv)
//   )
//
// we create wrappers here that behave as if they were created by this
// JS snippet:
//
//   function MakeMethod(wasm_instance, name) {
//     let wasm_func = wasm_instance.exports[name];
//     return function(...args) {
//       return wasm_func(this, ...args);
//     }
//   }
//   Foo.prototype.bar = MakeMethod(..., "bar");
//
// So that when called like this:
//
//   let foo = new Foo();
//   foo.bar("other");
//
// the Wasm function receives {foo} as $recv and "other" as $other.
TF_BUILTIN(WasmMethodWrapper, WasmBuiltinsAssembler) {
  auto argc = UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount);
  CodeStubArguments args(this, argc);
  TNode<Context> context = Parameter<Context>(Descriptor::kContext);
  static constexpr int kSlot = wasm::kMethodWrapperContextSlot;
  TNode<JSFunction> target = CAST(LoadContextElementNoCell(context, kSlot));
  TNode<Int32T> start_index = Int32Constant(0);
  TNode<Object> receiver = args.GetReceiver();
  // We push the receiver twice: once into the usual receiver slot, where
  // the Wasm function callee ignores it; once more as the first parameter.
  TNode<Int32T> already_on_stack = Int32Constant(2);
  TNode<Object> result =
      CallBuiltin(Builtin::kCallFunctionForwardVarargs, context, target,
                  already_on_stack, start_index, receiver, receiver);
  args.PopAndReturn(CAST(result));
}

TNode<BoolT> WasmBuiltinsAssembler::InSharedSpace(TNode<HeapObject> object) {
  TNode<IntPtrT> address = BitcastTaggedToWord(object);
  return IsPageFlagSet(address, MemoryChunk::kInSharedHeap);
}

#include "src/codegen/undef-code-stub-assembler-macros.inc"

}  // namespace v8::internal
