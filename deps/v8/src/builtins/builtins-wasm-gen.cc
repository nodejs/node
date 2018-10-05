// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/code-stub-assembler.h"
#include "src/objects-inl.h"
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
    TNode<IntPtrT> roots = UncheckedCast<IntPtrT>(
        Load(MachineType::Pointer(), instance,
             IntPtrConstant(WasmInstanceObject::kRootsArrayAddressOffset -
                            kHeapObjectTag)));
    TNode<Code> target = UncheckedCast<Code>(Load(
        MachineType::TaggedPointer(), roots,
        IntPtrConstant(Heap::roots_to_builtins_offset() + id * kPointerSize)));
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
    return UncheckedCast<Code>(
        Load(MachineType::AnyTagged(), instance,
             IntPtrConstant(WasmInstanceObject::kCEntryStubOffset -
                            kHeapObjectTag)));
  }
};

TF_BUILTIN(WasmAllocateHeapNumber, WasmBuiltinsAssembler) {
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kAllocateHeapNumber);
  TailCallStub(AllocateHeapNumberDescriptor(), target, NoContextConstant());
}

TF_BUILTIN(WasmCallJavaScript, WasmBuiltinsAssembler) {
  TNode<Object> context = UncheckedParameter(Descriptor::kContext);
  TNode<Object> function = UncheckedParameter(Descriptor::kFunction);
  TNode<Object> argc = UncheckedParameter(Descriptor::kActualArgumentsCount);
  TNode<Code> target = LoadBuiltinFromFrame(Builtins::kCall_ReceiverIsAny);
  TailCallStub(CallTrampolineDescriptor{}, target, context, function, argc);
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

TF_BUILTIN(WasmThrow, WasmBuiltinsAssembler) {
  TNode<Object> exception = UncheckedParameter(Descriptor::kException);
  TNode<Object> instance = LoadInstanceFromFrame();
  TNode<Code> centry = LoadCEntryFromInstance(instance);
  TNode<Object> context = LoadContextFromInstance(instance);
  TailCallRuntimeWithCEntry(Runtime::kThrow, centry, context, exception);
}

TF_BUILTIN(WasmGrowMemory, WasmBuiltinsAssembler) {
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
      Runtime::kWasmGrowMemory, centry, context, instance, num_pages_smi));
  TNode<Int32T> ret = SmiToInt32(ret_smi);
  ReturnRaw(ret);

  BIND(&num_pages_out_of_range);
  ReturnRaw(Int32Constant(-1));
}

#define DECLARE_ENUM(name)                                                    \
  TF_BUILTIN(ThrowWasm##name, WasmBuiltinsAssembler) {                        \
    TNode<Object> instance = LoadInstanceFromFrame();                         \
    TNode<Code> centry = LoadCEntryFromInstance(instance);                    \
    TNode<Object> context = LoadContextFromInstance(instance);                \
    int message_id = wasm::WasmOpcodes::TrapReasonToMessageId(wasm::k##name); \
    TailCallRuntimeWithCEntry(Runtime::kThrowWasmError, centry, context,      \
                              SmiConstant(message_id));                       \
  }
FOREACH_WASM_TRAPREASON(DECLARE_ENUM)
#undef DECLARE_ENUM

}  // namespace internal
}  // namespace v8
