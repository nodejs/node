// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_STACK_CHECK_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_STACK_CHECK_LOWERING_REDUCER_H_

#include "src/codegen/interface-descriptors.h"
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

  V<None> REDUCE(JSStackCheck)(V<Context> context,
                               OptionalV<LazyFrameState> frame_state,
                               JSStackCheckOp::Kind kind) {
    if (v8_flags.verify_write_barriers) {
      // The stack check/safepoint might trigger GC, so write barriers cannot be
      // eliminated across it.
      __ StoreOffHeap(__ IsolateField(IsolateFieldId::kLastYoungAllocation),
                      __ IntPtrConstant(0), MemoryRepresentation::UintPtr());
    }

    switch (kind) {
      case JSStackCheckOp::Kind::kFunctionEntry: {
        // Loads of the stack limit should not be load-eliminated as it can be
        // modified by another thread.
        V<WordPtr> limit =
            __ Load(__ ExternalConstant(
                        ExternalReference::address_of_jslimit(isolate())),
                    LoadOp::Kind::RawAligned().NotLoadEliminable(),
                    MemoryRepresentation::UintPtr());

        IF_NOT (LIKELY(__ StackPointerGreaterThan(
                    limit, StackCheckKind::kJSFunctionEntry))) {
          __ template CallRuntime<runtime::StackGuardWithGap>(
              frame_state.value(), context, {.gap = __ StackCheckOffset()},
              LazyDeoptOnThrow{false});
        }
        break;
      }
      case JSStackCheckOp::Kind::kBuiltinEntry: {
        V<WordPtr> stack_limit = __ LoadOffHeap(
            __ ExternalConstant(
                ExternalReference::address_of_jslimit(isolate())),
            MemoryRepresentation::UintPtr());
        IF_NOT (LIKELY(__ StackPointerGreaterThan(
                    stack_limit, StackCheckKind::kCodeStubAssembler))) {
          __ template CallRuntime<runtime::StackGuard>(context, {});
        }
        break;
      }
      case JSStackCheckOp::Kind::kLoop: {
        V<Word32> limit = __ Load(
            __ ExternalConstant(
                ExternalReference::address_of_no_heap_write_interrupt_request(
                    isolate())),
            LoadOp::Kind::RawAligned().NotLoadEliminable(),
            MemoryRepresentation::Uint8());

        IF_NOT (LIKELY(__ Word32Equal(limit, 0))) {
          __ template CallRuntime<runtime::HandleNoHeapWritesInterrupts>(
              frame_state.value(), context, {}, LazyDeoptOnThrow{false});
        }
        break;
      }
    }

    return V<None>::Invalid();
  }

#ifdef V8_ENABLE_WEBASSEMBLY
  V<None> REDUCE(WasmStackCheck)(
      OptionalV<WasmTrustedInstanceData> trusted_instance_data,
      WasmStackCheckOp::Kind kind) {
    if (kind == WasmStackCheckOp::Kind::kFunctionEntry) {
      // As an optimization, skip stack checks in leaf functions. Rely on
      // their callers checking the stack height instead.
      // However, if the function contains Liftoff deoptimization targets
      // (indicated by kLiftoffFunction frame states), it may deoptimize into
      // unoptimized Liftoff frames. Materializing these larger Liftoff frames
      // during deoptimization requires extra stack space (the stack check gap).
      // We must not eliminate the stack check in this case, even if it is a
      // leaf function, to ensure enough stack space is reserved for deopt.
      bool has_liftoff_frame = false;
      for (const Operation& op : __ input_graph().AllOperations()) {
        if (const FrameStateOp* frame_state = op.TryCast<FrameStateOp>()) {
          if (frame_state->data->frame_state_info.type() ==
              FrameStateType::kLiftoffFunction) {
            has_liftoff_frame = true;
            break;
          }
        }
      }
      if (__ IsLeafFunction() && !has_liftoff_frame) {
        return V<None>::Invalid();
      }

      if (v8_flags.wasm_growable_stacks) {
        // WasmStackCheck should be lowered by GrowableStacksReducer
        // in a special way.
        return Next::ReduceWasmStackCheck(trusted_instance_data, kind);
      }

      const CallDescriptor* entry_call_descriptor =
          compiler::Linkage::GetStubCallDescriptor(
              __ graph_zone(),                      // zone
              WasmStackGuardDescriptor{},           // descriptor
              0,                                    // stack parameter count
              CallDescriptor::kNoFlags,             // flags
              Operator::kNoProperties,              // properties
              StubCallMode::kCallWasmRuntimeStub);  // stub call mode
      const TSCallDescriptor* entry_ts_call_descriptor =
          TSCallDescriptor::Create(entry_call_descriptor,
                                   compiler::CanThrow{true},
                                   LazyDeoptOnThrow{false}, __ graph_zone());

      // Loads of the stack limit should not be load-eliminated as it can be
      // modified by another thread.
      V<WordPtr> limit = __ Load(
          __ LoadRootRegister(), LoadOp::Kind::RawAligned().NotLoadEliminable(),
          MemoryRepresentation::UintPtr(), IsolateData::jslimit_offset());
      V<WordPtr> gap =
          __ ChangeInt32ToIntPtr(__ UntagSmi(__ StackCheckOffset()));
      IF_NOT (LIKELY(
                  __ StackPointerGreaterThan(limit, StackCheckKind::kWasm))) {
        V<WordPtr> target =
            __ RelocatableWasmBuiltinCallTarget(Builtin::kWasmStackGuard);
        __ Call(target, {gap}, entry_ts_call_descriptor);
      }
      return V<None>::Invalid();
    }

    DCHECK_EQ(kind, WasmStackCheckOp::Kind::kLoop);

    // TODO(14108): Cache descriptor.
    const CallDescriptor* loop_call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            __ graph_zone(),                      // zone
            NoContextDescriptor{},                // descriptor
            0,                                    // stack parameter count
            CallDescriptor::kNoFlags,             // flags
            Operator::kNoProperties,              // properties
            StubCallMode::kCallWasmRuntimeStub);  // stub call mode
    const TSCallDescriptor* loop_ts_call_descriptor = TSCallDescriptor::Create(
        loop_call_descriptor, compiler::CanThrow{false},
        LazyDeoptOnThrow{false}, __ graph_zone());

    V<Word32> limit = __ Load(
        __ LoadRootRegister(), LoadOp::Kind::RawAligned().NotLoadEliminable(),
        MemoryRepresentation::Uint8(),
        IsolateData::no_heap_write_interrupt_request_offset());

    IF_NOT (LIKELY(__ Word32Equal(limit, 0))) {
      V<WordPtr> target =
          __ RelocatableWasmBuiltinCallTarget(Builtin::kWasmStackGuardLoop);
      __ Call(target, {}, loop_ts_call_descriptor);
    }
    return V<None>::Invalid();
  }
#endif  // V8_ENABLE_WEBASSEMBLY

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
