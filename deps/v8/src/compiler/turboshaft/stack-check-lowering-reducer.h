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
  // Returns V<None> or V<WordPtr> or V<Tuple<WordPtr, WordPtr>> depending on
  // inputs.
  V<AnyOrNone> REDUCE(WasmStackCheck)(
      OptionalV<WasmTrustedInstanceData> trusted_instance_data,
      OptionalV<WordPtr> memory_start, OptionalV<WordPtr> memory_size,
      WasmStackCheckOp::Kind kind) {
    // TODO(14108): Cache descriptor.
    const CallDescriptor* call_descriptor =
        compiler::Linkage::GetStubCallDescriptor(
            __ graph_zone(),                      // zone
            NoContextDescriptor{},                // descriptor
            0,                                    // stack parameter count
            CallDescriptor::kNoFlags,             // flags
            Operator::kNoProperties,              // properties
            StubCallMode::kCallWasmRuntimeStub);  // stub call mode
    const TSCallDescriptor* ts_call_descriptor =
        TSCallDescriptor::Create(call_descriptor, compiler::CanThrow{false},
                                 LazyDeoptOnThrow{false}, __ graph_zone());

    if (kind == WasmStackCheckOp::Kind::kFunctionEntry) {
      // As an optimization, skip stack checks in leaf functions. Rely on
      // their callers checking the stack height instead.
      if (__ IsLeafFunction()) return V<None>::Invalid();

      if (v8_flags.wasm_growable_stacks) {
        // WasmStackCheck should be lowered by GrowableStacksReducer
        // in a special way.
        return Next::ReduceWasmStackCheck(trusted_instance_data, memory_start,
                                          memory_size, kind);
      }

      // Loads of the stack limit should not be load-eliminated as it can be
      // modified by another thread.
      V<WordPtr> limit = __ Load(
          __ LoadRootRegister(), LoadOp::Kind::RawAligned().NotLoadEliminable(),
          MemoryRepresentation::UintPtr(), IsolateData::jslimit_offset());
      IF_NOT (LIKELY(
                  __ StackPointerGreaterThan(limit, StackCheckKind::kWasm))) {
        V<WordPtr> target =
            __ RelocatableWasmBuiltinCallTarget(Builtin::kWasmStackGuard);
        __ Call(target, {}, ts_call_descriptor);
      }
      DCHECK(!memory_start.valid());
      DCHECK(!memory_size.valid());
      return V<None>::Invalid();
    }

    DCHECK_EQ(kind, WasmStackCheckOp::Kind::kLoop);
    V<WordPtr> unused_initializer = V<WordPtr>::Invalid();
    ScopedVar<WordPtr> new_mem_start(
        this, memory_start.valid() ? memory_start.value() : unused_initializer);
    ScopedVar<WordPtr> new_mem_size(
        this, memory_size.valid() ? memory_size.value() : unused_initializer);

    V<Word32> limit = __ Load(
        __ LoadRootRegister(), LoadOp::Kind::RawAligned().NotLoadEliminable(),
        MemoryRepresentation::Uint8(),
        IsolateData::no_heap_write_interrupt_request_offset());

    IF_NOT (LIKELY(__ Word32Equal(limit, 0))) {
      V<WordPtr> target =
          __ RelocatableWasmBuiltinCallTarget(Builtin::kWasmStackGuardLoop);
      __ Call(target, {}, ts_call_descriptor);

      if (memory_start.valid() || memory_size.valid()) {
        DCHECK(trusted_instance_data.valid());
        if (memory_start.valid()) {
          new_mem_start =
              __ Load(trusted_instance_data.value(), LoadOp::Kind::TaggedBase(),
                      MemoryRepresentation::UintPtr(),
                      WasmTrustedInstanceData::kMemory0StartOffset);
        }
        if (memory_size.valid()) {
          new_mem_size =
              __ Load(trusted_instance_data.value(), LoadOp::Kind::TaggedBase(),
                      MemoryRepresentation::UintPtr(),
                      WasmTrustedInstanceData::kMemory0SizeOffset);
        }
      }
    }
    if (memory_start.valid() && memory_size.valid()) {
      return __ MakeTuple(new_mem_start, new_mem_size);
    } else if (memory_start.valid()) {
      return new_mem_start;
    } else if (memory_size.valid()) {
      return new_mem_size;
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
