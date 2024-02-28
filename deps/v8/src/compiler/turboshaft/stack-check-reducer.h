// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_STACK_CHECK_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_STACK_CHECK_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/uniform-reducer-adapter.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class StackCheckReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  OpIndex REDUCE(Parameter)(int32_t parameter_index, RegisterRepresentation rep,
                            const char* debug_name = "") {
    OpIndex result = Next::ReduceParameter(parameter_index, rep, debug_name);
    if (parameter_index == 0) {
      param0_ = result;
    }
    return result;
  }

  OpIndex REDUCE(StackCheck)(StackCheckOp::CheckOrigin origin,
                             StackCheckOp::CheckKind kind) {
#ifdef V8_ENABLE_WEBASSEMBLY
    if (origin == StackCheckOp::CheckOrigin::kFromWasm) {
      V<Tagged> instance_node = param0_;
      DCHECK(instance_node.valid());
      V<WordPtr> limit_address =
          __ Load(instance_node, LoadOp ::Kind ::TaggedBase().Immutable(),
                  MemoryRepresentation ::PointerSized(),
                  WasmInstanceObject ::kStackLimitAddressOffset);
      V<WordPtr> limit = __ Load(limit_address, LoadOp::Kind::RawAligned(),
                                 MemoryRepresentation::PointerSized(), 0);
      V<Word32> check =
          __ StackPointerGreaterThan(limit, compiler::StackCheckKind::kWasm);
      IF_NOT (LIKELY(check)) {
        // TODO(14108): Cache descriptor.
        V<WordPtr> builtin =
            __ RelocatableWasmBuiltinCallTarget(Builtin::kWasmStackGuard);
        const CallDescriptor* call_descriptor =
            compiler::Linkage::GetStubCallDescriptor(
                __ graph_zone(),                      // zone
                NoContextDescriptor{},                // descriptor
                0,                                    // stack parameter count
                CallDescriptor::kNoFlags,             // flags
                Operator::kNoProperties,              // properties
                StubCallMode::kCallWasmRuntimeStub);  // stub call mode
        const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
            call_descriptor, compiler::CanThrow::kNo, __ graph_zone());
        __ Call(builtin, {}, ts_call_descriptor);
      }
      END_IF
      return OpIndex::Invalid();
    }
#endif  // V8_ENABLE_WEBASSEMBLY
    // TODO(turboshaft): Implement stack checks for JavaScript.
    UNIMPLEMENTED();
  }

 private:
  // For WebAssembly, param0 is the instance, we store it to use it for the
  // stack check.
  OpIndex param0_ = OpIndex::Invalid();
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
#endif  // V8_COMPILER_TURBOSHAFT_STACK_CHECK_REDUCER_H_
