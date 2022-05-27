// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_CODE_GEN_STATE_H_
#define V8_MAGLEV_MAGLEV_CODE_GEN_STATE_H_

#include "src/codegen/assembler.h"
#include "src/codegen/label.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/safepoint-table.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

class InterpreterFrameState;

class DeferredCodeInfo {
 public:
  virtual void Generate(MaglevCodeGenState* code_gen_state,
                        Label* return_label) = 0;
  Label deferred_code_label;
  Label return_label;
};

class MaglevCodeGenState {
 public:
  MaglevCodeGenState(MaglevCompilationInfo* compilation_info,
                     SafepointTableBuilder* safepoint_table_builder)
      : compilation_info_(compilation_info),
        safepoint_table_builder_(safepoint_table_builder),
        masm_(isolate(), CodeObjectRequired::kNo) {}

  void set_tagged_slots(int slots) { tagged_slots_ = slots; }
  void set_untagged_slots(int slots) { untagged_slots_ = slots; }

  void PushDeferredCode(DeferredCodeInfo* deferred_code) {
    deferred_code_.push_back(deferred_code);
  }
  const std::vector<DeferredCodeInfo*>& deferred_code() const {
    return deferred_code_;
  }
  void PushEagerDeopt(EagerDeoptInfo* info) { eager_deopts_.push_back(info); }
  void PushLazyDeopt(LazyDeoptInfo* info) { lazy_deopts_.push_back(info); }
  const std::vector<EagerDeoptInfo*>& eager_deopts() const {
    return eager_deopts_;
  }
  const std::vector<LazyDeoptInfo*>& lazy_deopts() const {
    return lazy_deopts_;
  }
  inline void DefineSafepointStackSlots(
      SafepointTableBuilder::Safepoint& safepoint) const;
  inline void DefineLazyDeoptPoint(LazyDeoptInfo* info);

  compiler::NativeContextRef native_context() const {
    return broker()->target_native_context();
  }
  Isolate* isolate() const { return compilation_info_->isolate(); }
  compiler::JSHeapBroker* broker() const { return compilation_info_->broker(); }
  MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }
  MacroAssembler* masm() { return &masm_; }
  int stack_slots() const { return untagged_slots_ + tagged_slots_; }
  SafepointTableBuilder* safepoint_table_builder() const {
    return safepoint_table_builder_;
  }
  MaglevCompilationInfo* compilation_info() const { return compilation_info_; }

  // TODO(v8:7700): Clean up after all code paths are supported.
  void set_found_unsupported_code_paths(bool val) {
    found_unsupported_code_paths_ = val;
  }
  bool found_unsupported_code_paths() const {
    return found_unsupported_code_paths_;
  }

  inline int GetFramePointerOffsetForStackSlot(
      const compiler::AllocatedOperand& operand) {
    int index = operand.index();
    if (operand.representation() != MachineRepresentation::kTagged) {
      index += tagged_slots_;
    }
    return GetFramePointerOffsetForStackSlot(index);
  }

  inline MemOperand GetStackSlot(const compiler::AllocatedOperand& operand) {
    return MemOperand(rbp, GetFramePointerOffsetForStackSlot(operand));
  }

  inline MemOperand ToMemOperand(const compiler::InstructionOperand& operand) {
    return GetStackSlot(compiler::AllocatedOperand::cast(operand));
  }

  inline MemOperand ToMemOperand(const ValueLocation& location) {
    return ToMemOperand(location.operand());
  }

  inline MemOperand TopOfStack() {
    return MemOperand(rbp,
                      GetFramePointerOffsetForStackSlot(stack_slots() - 1));
  }

 private:
  inline constexpr int GetFramePointerOffsetForStackSlot(int index) {
    return StandardFrameConstants::kExpressionsOffset -
           index * kSystemPointerSize;
  }

  MaglevCompilationInfo* const compilation_info_;
  SafepointTableBuilder* const safepoint_table_builder_;

  MacroAssembler masm_;
  std::vector<DeferredCodeInfo*> deferred_code_;
  std::vector<EagerDeoptInfo*> eager_deopts_;
  std::vector<LazyDeoptInfo*> lazy_deopts_;
  int untagged_slots_ = 0;
  int tagged_slots_ = 0;

  // Allow marking some codegen paths as unsupported, so that we can test maglev
  // incrementally.
  // TODO(v8:7700): Clean up after all code paths are supported.
  bool found_unsupported_code_paths_ = false;
};

// Some helpers for codegen.
// TODO(leszeks): consider moving this to a separate header.

inline int GetSafepointIndexForStackSlot(int i) {
  // Safepoint tables also contain slots for all fixed frame slots (both
  // above and below the fp).
  return StandardFrameConstants::kFixedSlotCount + i;
}

inline Register ToRegister(const compiler::InstructionOperand& operand) {
  return compiler::AllocatedOperand::cast(operand).GetRegister();
}

inline DoubleRegister ToDoubleRegister(
    const compiler::InstructionOperand& operand) {
  return compiler::AllocatedOperand::cast(operand).GetDoubleRegister();
}

inline Register ToRegister(const ValueLocation& location) {
  return ToRegister(location.operand());
}

inline DoubleRegister ToDoubleRegister(const ValueLocation& location) {
  return ToDoubleRegister(location.operand());
}

inline void MaglevCodeGenState::DefineSafepointStackSlots(
    SafepointTableBuilder::Safepoint& safepoint) const {
  for (int stack_slot = 0; stack_slot < tagged_slots_; stack_slot++) {
    safepoint.DefineTaggedStackSlot(GetSafepointIndexForStackSlot(stack_slot));
  }
}

inline void MaglevCodeGenState::DefineLazyDeoptPoint(LazyDeoptInfo* info) {
  info->deopting_call_return_pc = masm()->pc_offset_for_safepoint();
  PushLazyDeopt(info);
  SafepointTableBuilder::Safepoint safepoint =
      safepoint_table_builder()->DefineSafepoint(masm());
  DefineSafepointStackSlots(safepoint);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_CODE_GEN_STATE_H_
