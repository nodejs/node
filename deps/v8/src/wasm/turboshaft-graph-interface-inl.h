// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_INL_H_
#define V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/wasm/turboshaft-graph-interface.h"

namespace v8::internal::wasm {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <typename Assembler>
inline auto WasmGraphBuilderBase<Assembler>::GetBuiltinPointerTarget(
    Builtin builtin) -> OpIndex {
  static_assert(std::is_same_v<Smi, BuiltinPtr>, "BuiltinPtr must be Smi");
  return __ SmiConstant(Smi::FromInt(static_cast<int>(builtin)));
}

template <typename Assembler>
inline auto WasmGraphBuilderBase<Assembler>::GetTargetForBuiltinCall(
    Builtin builtin, StubCallMode stub_mode) -> V<WordPtr> {
  return stub_mode == StubCallMode::kCallWasmRuntimeStub
             ? __ RelocatableWasmBuiltinCallTarget(builtin)
             : GetBuiltinPointerTarget(builtin);
}

template <typename Assembler>
auto WasmGraphBuilderBase<Assembler>::BuildChangeInt64ToBigInt(
    V<Word64> input, StubCallMode stub_mode) -> V<BigInt> {
  Builtin builtin = Is64() ? Builtin::kI64ToBigInt : Builtin::kI32PairToBigInt;
  V<WordPtr> target = GetTargetForBuiltinCall(builtin, stub_mode);
  CallInterfaceDescriptor interface_descriptor =
      Builtins::CallInterfaceDescriptorFor(builtin);
  const CallDescriptor* call_descriptor =
      compiler::Linkage::GetStubCallDescriptor(
          __ graph_zone(),  // zone
          interface_descriptor,
          0,                         // stack parameter count
          CallDescriptor::kNoFlags,  // flags
          Operator::kNoProperties,   // properties
          stub_mode);
  const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
      call_descriptor, compiler::CanThrow::kNo, compiler::LazyDeoptOnThrow::kNo,
      __ graph_zone());
  if constexpr (Is64()) {
    return V<BigInt>::Cast(__ Call(target, {input}, ts_call_descriptor));
  }
  V<Word32> low_word = __ TruncateWord64ToWord32(input);
  V<Word32> high_word =
      __ TruncateWord64ToWord32(__ Word64ShiftRightLogical(input, 32));
  return V<BigInt>::Cast(
      __ Call(target, {low_word, high_word}, ts_call_descriptor));
}

template <typename Assembler>
auto WasmGraphBuilderBase<Assembler>::BuildImportedFunctionTargetAndImplicitArg(
    ConstOrV<Word32> func_index,
    V<WasmTrustedInstanceData> trusted_instance_data)
    -> std::pair<V<Word32>, V<HeapObject>> {
  V<WasmDispatchTableForImports> dispatch_table =
      LOAD_IMMUTABLE_PROTECTED_INSTANCE_FIELD(trusted_instance_data,
                                              DispatchTableForImports,
                                              WasmDispatchTableForImports);
  // Handle constant indexes specially to reduce graph size, even though later
  // optimization would optimize this to the same result.
  if (func_index.is_constant()) {
    int offset =
        WasmDispatchTableForImports::OffsetOf(func_index.constant_value());
    V<Word32> target =
        __ Load(dispatch_table, LoadOp::Kind::TaggedBase(),
                MemoryRepresentation::Uint32(),
                offset + WasmDispatchTableForImports::kTargetBias);
    V<ExposedTrustedObject> implicit_arg =
        V<ExposedTrustedObject>::Cast(__ LoadProtectedPointerField(
            dispatch_table, LoadOp::Kind::TaggedBase(),
            offset + WasmDispatchTableForImports::kImplicitArgBias));
    return {target, implicit_arg};
  }

  V<WordPtr> dispatch_table_entry_offset =
      __ WordPtrMul(__ ChangeUint32ToUintPtr(func_index.value()),
                    WasmDispatchTableForImports::kEntrySize);
  V<Word32> target =
      __ Load(dispatch_table, dispatch_table_entry_offset,
              LoadOp::Kind::TaggedBase(), MemoryRepresentation::Uint32(),
              WasmDispatchTableForImports::kEntriesOffset +
                  WasmDispatchTableForImports::kTargetBias);
  V<ExposedTrustedObject> implicit_arg =
      V<ExposedTrustedObject>::Cast(__ LoadProtectedPointerField(
          dispatch_table, dispatch_table_entry_offset,
          LoadOp::Kind::TaggedBase(),
          WasmDispatchTableForImports::kEntriesOffset +
              WasmDispatchTableForImports::kImplicitArgBias,
          0));
  return {target, implicit_arg};
}

template <typename Assembler>
inline auto WasmGraphBuilderBase<Assembler>::BuildFunctionTargetAndImplicitArg(
    V<WasmInternalFunction> internal_function)
    -> std::pair<V<Word32>, V<ExposedTrustedObject>> {
  V<ExposedTrustedObject> implicit_arg =
      V<ExposedTrustedObject>::Cast(__ LoadProtectedPointerField(
          internal_function, LoadOp::Kind::TaggedBase().Immutable(),
          WasmInternalFunction::kProtectedImplicitArgOffset));

  V<Word32> target = __ Load(internal_function, LoadOp::Kind::TaggedBase(),
                             MemoryRepresentation::Uint32(),
                             WasmInternalFunction::kRawCallTargetOffset);

  return {target, implicit_arg};
}

template <typename Assembler>
inline compiler::turboshaft::RegisterRepresentation
WasmGraphBuilderBase<Assembler>::RepresentationFor(ValueTypeBase type) {
  switch (type.kind()) {
    case kI8:
    case kI16:
    case kI32:
      return RegisterRepresentation::Word32();
    case kI64:
      return RegisterRepresentation::Word64();
    case kF16:
    case kF32:
      return RegisterRepresentation::Float32();
    case kF64:
      return RegisterRepresentation::Float64();
    case kRefNull:
    case kRef:
      return RegisterRepresentation::Tagged();
    case kS128:
      return RegisterRepresentation::Simd128();
    case kVoid:
    case kTop:
    case kBottom:
      UNREACHABLE();
  }
}

template <typename Assembler>
inline auto WasmGraphBuilderBase<Assembler>::CallC(
    const MachineSignature* sig, ExternalReference ref,
    std::initializer_list<OpIndex> args) -> OpIndex {
  return WasmGraphBuilderBase::CallC(sig, __ ExternalConstant(ref), args);
}

template <typename Assembler>
inline auto WasmGraphBuilderBase<Assembler>::CallC(
    const MachineSignature* sig, OpIndex function,
    std::initializer_list<OpIndex> args) -> OpIndex {
  DCHECK_LE(sig->return_count(), 1);
  DCHECK_EQ(sig->parameter_count(), args.size());
  const CallDescriptor* call_descriptor =
      compiler::Linkage::GetSimplifiedCDescriptor(__ graph_zone(), sig);
  const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
      call_descriptor, compiler::CanThrow::kNo, compiler::LazyDeoptOnThrow::kNo,
      __ graph_zone());
  return __ Call(function, OpIndex::Invalid(), base::VectorOf(args),
                 ts_call_descriptor);
}

template <typename Assembler>
void WasmGraphBuilderBase<Assembler>::BuildSetNewStackLimit(
    V<WordPtr> old_limit, V<WordPtr> new_limit) {
  // Set the new interrupt limit and real limit. Use a compare-and-swap for
  // the interrupt limit to avoid overwriting a pending interrupt.
  __ AtomicCompareExchange(
      __ IsolateField(IsolateFieldId::kJsLimit), __ UintPtrConstant(0),
      old_limit, new_limit, RegisterRepresentation::WordPtr(),
      MemoryRepresentation::UintPtr(), compiler::MemoryAccessKind::kNormal,
      RegisterRepresentation::WordPtr());
  __ Store(__ LoadRootRegister(), new_limit, StoreOp::Kind::RawAligned(),
           MemoryRepresentation::UintPtr(), compiler::kNoWriteBarrier,
           IsolateData::real_jslimit_offset());
}

template <typename Assembler>
auto WasmGraphBuilderBase<Assembler>::BuildSwitchToTheCentralStack(
    V<WordPtr> old_limit) -> V<compiler::turboshaft::WordPtr> {
  // The switch involves a write to the StackMemory object, so use the
  // privileged external C call if the sandbox hardware support is enabled.
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  auto sig = FixedSizeSignature<MachineType>::Params(MachineType::Pointer(),
                                                     MachineType::Pointer())
                 .Returns(MachineType::Pointer());
  OpIndex central_stack_sp =
      CallC(&sig, ExternalReference::wasm_switch_to_the_central_stack_for_js(),
            {__ ExternalConstant(ExternalReference::isolate_address()),
             __ FramePointer()});

#else
  // Set the is_on_central_stack flag.
  OpIndex isolate_root = __ LoadRootRegister();
  __ Store(isolate_root, __ Word32Constant(1), LoadOp::Kind::RawAligned(),
           MemoryRepresentation::Uint8(), compiler::kNoWriteBarrier,
           IsolateData::is_on_central_stack_flag_offset());

  // Save the old fp and the target sp in the StackMemory's stack switch info.
  // We are not on the main stack, so the active stack must be set.
  V<WordPtr> active_stack = __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                                    MemoryRepresentation::UintPtr(),
                                    IsolateData::active_stack_offset());
  __ Store(active_stack, __ FramePointer(), StoreOp::Kind::RawAligned(),
           MemoryRepresentation::UintPtr(), compiler::kNoWriteBarrier,
           StackMemory::stack_switch_source_fp_offset());
  V<WordPtr> central_stack_sp = __ Load(
      isolate_root, LoadOp::Kind::RawAligned(), MemoryRepresentation::UintPtr(),
      Isolate::central_stack_sp_offset());
  __ Store(active_stack, central_stack_sp, StoreOp::Kind::RawAligned(),
           MemoryRepresentation::UintPtr(), compiler::kNoWriteBarrier,
           StackMemory::stack_switch_target_sp_offset());

  // Switch the stack limit and the stack pointer.
  V<WordPtr> central_stack_limit = __ Load(
      isolate_root, LoadOp::Kind::RawAligned(), MemoryRepresentation::UintPtr(),
      Isolate::central_stack_limit_offset());
  BuildSetNewStackLimit(old_limit, central_stack_limit);
#endif
  OpIndex old_sp = __ LoadStackPointer();
  __ SetStackPointer(central_stack_sp);
  return old_sp;
}

template <typename Assembler>
inline auto
WasmGraphBuilderBase<Assembler>::BuildSwitchToTheCentralStackIfNeeded()
    -> std::pair<V<WordPtr>, V<WordPtr>> {
  V<WordPtr> isolate_root = __ LoadRootRegister();
  V<Word32> is_on_central_stack_flag = __ Load(
      isolate_root, LoadOp::Kind::RawAligned(), MemoryRepresentation::Uint8(),
      IsolateData::is_on_central_stack_flag_offset());
  ScopedVar<WordPtr> old_sp_var(this, __ IntPtrConstant(0));
  ScopedVar<WordPtr> old_limit_var(this, __ IntPtrConstant(0));
  IF_NOT (LIKELY(is_on_central_stack_flag)) {
    V<WordPtr> old_limit = __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                                   MemoryRepresentation::UintPtr(),
                                   IsolateData::real_jslimit_offset());
    V<WordPtr> old_sp = BuildSwitchToTheCentralStack(old_limit);
    old_sp_var = old_sp;
    old_limit_var = old_limit;
  }
  return {old_sp_var, old_limit_var};
}

template <typename Assembler>
void WasmGraphBuilderBase<Assembler>::BuildSwitchBackFromCentralStack(
    V<WordPtr> old_sp, V<WordPtr> old_limit) {
  // The switch involves a write to the StackMemory object, so use the
  // privileged external C call if the sandbox hardware support is enabled.
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  auto sig = FixedSizeSignature<MachineType>::Params(MachineType::Pointer());
  IF_NOT (LIKELY(__ WordPtrEqual(old_sp, __ IntPtrConstant(0)))) {
    CallC(&sig, ExternalReference::wasm_switch_from_the_central_stack_for_js(),
          {__ ExternalConstant(ExternalReference::isolate_address())});
#else
  IF_NOT (LIKELY(__ WordPtrEqual(old_sp, __ IntPtrConstant(0)))) {
    // Reset is_on_central_stack flag.
    V<WordPtr> isolate_root = __ LoadRootRegister();
    __ Store(isolate_root, __ Word32Constant(0), StoreOp::Kind::RawAligned(),
             MemoryRepresentation::Uint8(), compiler::kNoWriteBarrier,
             IsolateData::is_on_central_stack_flag_offset());

    // Clear stack switch info.
    // We are not on the main stack, so the active stack must be set.
    V<WordPtr> active_stack = __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                                      MemoryRepresentation::UintPtr(),
                                      IsolateData::active_stack_offset());
    __ Store(active_stack, __ UintPtrConstant(0), StoreOp::Kind::RawAligned(),
             MemoryRepresentation::UintPtr(), compiler::kNoWriteBarrier,
             StackMemory::stack_switch_source_fp_offset());

    // Restore the old stack limit and stack pointer.
    V<WordPtr> real_jslimit = __ Load(isolate_root, LoadOp::Kind::RawAligned(),
                                      MemoryRepresentation::UintPtr(),
                                      IsolateData::real_jslimit_offset());
    BuildSetNewStackLimit(real_jslimit, old_limit);
#endif
    __ SetStackPointer(old_sp);
  }
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::wasm

#endif  // V8_WASM_TURBOSHAFT_GRAPH_INTERFACE_INL_H_
