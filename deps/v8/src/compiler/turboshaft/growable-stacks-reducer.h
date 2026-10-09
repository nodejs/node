// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_GROWABLE_STACKS_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_GROWABLE_STACKS_REDUCER_H_

#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/uniform-reducer-adapter.h"
#include "src/execution/isolate-data.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class GrowableStacksReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(GrowableStacks)

  GrowableStacksReducer() {
    if (!__ data()->wasm_module_sig() || !v8_flags.wasm_growable_stacks) {
      // We are not compiling a wasm function if there is no signature.
      skip_reducer_ = true;
      return;
    }
    call_descriptor_ = compiler::GetWasmCallDescriptor(
        __ graph_zone(), __ data()->wasm_module_sig());
#if V8_TARGET_ARCH_32_BIT
    call_descriptor_ =
        compiler::GetI32WasmCallDescriptor(__ graph_zone(), call_descriptor_);
#endif
  }

  V<None> REDUCE(WasmStackCheck)(WasmStackCheckOp::Kind kind) {
    CHECK_EQ(kind, WasmStackCheckOp::Kind::kFunctionEntry);
    if (skip_reducer_) {
      return Next::ReduceWasmStackCheck(kind);
    }
    // Loads of the stack limit should not be load-eliminated as it can be
    // modified by another thread.
    V<WordPtr> limit = __ Load(
        __ LoadRootRegister(), LoadOp::Kind::RawAligned().NotLoadEliminable(),
        MemoryRepresentation::UintPtr(), IsolateData::jslimit_offset());

    IF_NOT (LIKELY(__ StackPointerGreaterThan(limit, StackCheckKind::kWasm))) {
      const int stack_parameter_count = 0;
      const CallDescriptor* stub_call_descriptor =
          compiler::Linkage::GetStubCallDescriptor(
              __ graph_zone(), WasmGrowableStackGuardDescriptor{},
              stack_parameter_count, CallDescriptor::kNoFlags,
              Operator::kNoProperties, StubCallMode::kCallWasmRuntimeStub);
      const TSCallDescriptor* ts_stub_call_descriptor =
          TSCallDescriptor::Create(stub_call_descriptor,
                                   compiler::CanThrow{true},
                                   LazyDeoptOnThrow{false}, __ graph_zone());
      V<WordPtr> builtin =
          __ RelocatableWasmBuiltinCallTarget(Builtin::kWasmGrowableStackGuard);
      auto stack_param_and_return_slots_size =
          __ IntPtrConstant((call_descriptor_->ParameterSlotCount() +
                             call_descriptor_->ReturnSlotCount()) *
                            kSystemPointerSize);
      V<WordPtr> gap =
          __ ChangeInt32ToIntPtr(__ UntagSmi(__ StackCheckOffset()));
      auto parameter_slots_size = __ IntPtrConstant(
          call_descriptor_->ParameterSlotCount() * kSystemPointerSize);
      __ Call(builtin,
              {stack_param_and_return_slots_size, gap, parameter_slots_size},
              ts_stub_call_descriptor);
    }

    return V<None>::Invalid();
  }

 private:
  bool skip_reducer_ = false;
  CallDescriptor* call_descriptor_ = nullptr;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_GROWABLE_STACKS_REDUCER_H_
