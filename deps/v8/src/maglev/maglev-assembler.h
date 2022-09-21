// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_ASSEMBLER_H_
#define V8_MAGLEV_MAGLEV_ASSEMBLER_H_

#include "src/codegen/macro-assembler.h"
#include "src/maglev/maglev-code-gen-state.h"

namespace v8 {
namespace internal {
namespace maglev {

// Label allowed to be passed to deferred code.
class ZoneLabelRef {
 public:
  explicit ZoneLabelRef(Zone* zone) : label_(zone->New<Label>()) {}

  static ZoneLabelRef UnsafeFromLabelPointer(Label* label) {
    // This is an unsafe operation, {label} must be zone allocated.
    return ZoneLabelRef(label);
  }

  Label* operator*() { return label_; }

 private:
  Label* label_;

  // Unsafe constructor. {label} must be zone allocated.
  explicit ZoneLabelRef(Label* label) : label_(label) {}
};

class MaglevAssembler : public MacroAssembler {
 public:
  explicit MaglevAssembler(MaglevCodeGenState* code_gen_state)
      : MacroAssembler(code_gen_state->isolate(), CodeObjectRequired::kNo),
        code_gen_state_(code_gen_state) {}

  inline MemOperand GetStackSlot(const compiler::AllocatedOperand& operand) {
    return MemOperand(rbp, GetFramePointerOffsetForStackSlot(operand));
  }

  inline MemOperand ToMemOperand(const compiler::InstructionOperand& operand) {
    return GetStackSlot(compiler::AllocatedOperand::cast(operand));
  }

  inline MemOperand ToMemOperand(const ValueLocation& location) {
    return ToMemOperand(location.operand());
  }

  inline int GetFramePointerOffsetForStackSlot(
      const compiler::AllocatedOperand& operand) {
    int index = operand.index();
    if (operand.representation() != MachineRepresentation::kTagged) {
      index += code_gen_state()->tagged_slots();
    }
    return GetFramePointerOffsetForStackSlot(index);
  }

  inline void Branch(Condition condition, BasicBlock* if_true,
                     BasicBlock* if_false, BasicBlock* next_block);
  inline void PushInput(const Input& input);
  inline Register FromAnyToRegister(const Input& input, Register scratch);

  inline void DefineLazyDeoptPoint(LazyDeoptInfo* info);
  inline void DefineExceptionHandlerPoint(NodeBase* node);
  inline void DefineExceptionHandlerAndLazyDeoptPoint(NodeBase* node);

  template <typename Function, typename... Args>
  inline DeferredCodeInfo* PushDeferredCode(Function&& deferred_code_gen,
                                            Args&&... args);
  template <typename Function, typename... Args>
  inline void JumpToDeferredIf(Condition cond, Function&& deferred_code_gen,
                               Args&&... args);

  inline void RegisterEagerDeopt(EagerDeoptInfo* deopt_info,
                                 DeoptimizeReason reason);
  template <typename NodeT>
  inline void EmitEagerDeopt(NodeT* node, DeoptimizeReason reason);
  template <typename NodeT>
  inline void EmitEagerDeoptIf(Condition cond, DeoptimizeReason reason,
                               NodeT* node);

  compiler::NativeContextRef native_context() const {
    return code_gen_state()->broker()->target_native_context();
  }

  MaglevCodeGenState* code_gen_state() const { return code_gen_state_; }
  MaglevSafepointTableBuilder* safepoint_table_builder() const {
    return code_gen_state()->safepoint_table_builder();
  }
  MaglevCompilationInfo* compilation_info() const {
    return code_gen_state()->compilation_info();
  }

 private:
  inline constexpr int GetFramePointerOffsetForStackSlot(int index) {
    return StandardFrameConstants::kExpressionsOffset -
           index * kSystemPointerSize;
  }

  MaglevCodeGenState* const code_gen_state_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_ASSEMBLER_H_
