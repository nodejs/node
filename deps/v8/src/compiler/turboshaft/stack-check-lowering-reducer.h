// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_STACK_CHECK_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_STACK_CHECK_LOWERING_REDUCER_H_

#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/uniform-reducer-adapter.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <class Next>
class StackCheckLoweringReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(StackCheckLowering)

  OpIndex REDUCE(StackCheck)(StackCheckOp::Kind kind) {
#ifdef V8_ENABLE_WEBASSEMBLY
    if (kind == StackCheckOp::Kind::kWasmFunctionHeader &&
        __ IsLeafFunction()) {
      return OpIndex::Invalid();
    }
#endif  // V8_ENABLE_WEBASSEMBLY
    // Loads of the stack limit should not be load-eliminated as it can be
    // modified by another thread.
    V<WordPtr> limit = __ Load(
        __ LoadRootRegister(), LoadOp::Kind::RawAligned().NotLoadEliminable(),
        MemoryRepresentation::UintPtr(), IsolateData::jslimit_offset());
    compiler::StackCheckKind check_kind =
        kind == StackCheckOp::Kind::kJSFunctionHeader
            ? compiler::StackCheckKind::kJSFunctionEntry
            : compiler::StackCheckKind::kWasm;
    V<Word32> check = __ StackPointerGreaterThan(limit, check_kind);
    IF_NOT (LIKELY(check)) {
      if (kind == StackCheckOp::Kind::kJSFunctionHeader) {
        __ CallRuntime_StackGuardWithGap(isolate(), __ NoContextConstant(),
                                         __ StackCheckOffset());
      }
#ifdef V8_ENABLE_WEBASSEMBLY
      else {
        DCHECK(kind == StackCheckOp::Kind::kWasmFunctionHeader ||
               kind == StackCheckOp::Kind::kWasmLoop);
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
        // Pass custom effects to the `Call` node to mark it as non-writing.
        __ Call(builtin, {}, ts_call_descriptor,
                OpEffects()
                    .CanReadMemory()
                    .RequiredWhenUnused()
                    .CanCreateIdentity());
      }
#endif  // V8_ENABLE_WEBASSEMBLY
    }

    return OpIndex::Invalid();
  }

  OpIndex REDUCE(JSLoopStackCheck)(V<Context> context,
                                   V<FrameState> frame_state) {
    V<Word32> limit = __ Load(
        __ ExternalConstant(
            ExternalReference::address_of_no_heap_write_interrupt_request(
                isolate())),
        LoadOp::Kind::RawAligned().NotLoadEliminable(),
        MemoryRepresentation::Uint8());

    IF_NOT (LIKELY(__ Word32Equal(limit, 0))) {
      __ CallRuntime_HandleNoHeapWritesInterrupts(isolate(), frame_state,
                                                  context);
    }

    return OpIndex::Invalid();
  }

 private:
  Isolate* isolate() {
    if (!isolate_) isolate_ = __ data() -> isolate();
    return isolate_;
  }

  Isolate* isolate_ = nullptr;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
#endif  // V8_COMPILER_TURBOSHAFT_STACK_CHECK_LOWERING_REDUCER_H_
