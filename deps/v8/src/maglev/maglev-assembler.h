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

class MaglevAssembler;

// Label allowed to be passed to deferred code.
class ZoneLabelRef {
 public:
  explicit ZoneLabelRef(Zone* zone) : label_(zone->New<Label>()) {}
  explicit inline ZoneLabelRef(MaglevAssembler* masm);

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
  explicit MaglevAssembler(Isolate* isolate, MaglevCodeGenState* code_gen_state)
      : MacroAssembler(isolate, CodeObjectRequired::kNo),
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

  void Allocate(RegisterSnapshot& register_snapshot, Register result,
                int size_in_bytes,
                AllocationType alloc_type = AllocationType::kYoung,
                AllocationAlignment alignment = kTaggedAligned);

  void AllocateTwoByteString(RegisterSnapshot register_snapshot,
                             Register result, int length);

  void LoadSingleCharacterString(Register result, int char_code);
  void LoadSingleCharacterString(Register result, Register char_code,
                                 Register scratch);

  inline void Branch(Condition condition, BasicBlock* if_true,
                     BasicBlock* if_false, BasicBlock* next_block);
  inline void PushInput(const Input& input);
  inline Register FromAnyToRegister(const Input& input, Register scratch);

  // Warning: Input registers {string} and {index} will be scratched.
  // {result} is allowed to alias with one the other 3 input registers.
  // {result} is an int32.
  void StringCharCodeAt(RegisterSnapshot& register_snapshot, Register result,
                        Register string, Register index, Register scratch,
                        Label* result_fits_one_byte);
  // Warning: Input {char_code} will be scratched.
  void StringFromCharCode(RegisterSnapshot register_snapshot,
                          Label* char_code_fits_one_byte, Register result,
                          Register char_code, Register scratch);

  void ToBoolean(Register value, ZoneLabelRef is_true, ZoneLabelRef is_false,
                 bool fallthrough_when_true);

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

class SaveRegisterStateForCall {
 public:
  SaveRegisterStateForCall(MaglevAssembler* masm, RegisterSnapshot snapshot)
      : masm(masm), snapshot_(snapshot) {
    masm->PushAll(snapshot_.live_registers);
    masm->PushAll(snapshot_.live_double_registers, kDoubleSize);
  }

  ~SaveRegisterStateForCall() {
    masm->PopAll(snapshot_.live_double_registers, kDoubleSize);
    masm->PopAll(snapshot_.live_registers);
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepoint() {
    // TODO(leszeks): Avoid emitting safepoints when there are no registers to
    // save.
    auto safepoint = masm->safepoint_table_builder()->DefineSafepoint(masm);
    int pushed_reg_index = 0;
    for (Register reg : snapshot_.live_registers) {
      if (snapshot_.live_tagged_registers.has(reg)) {
        safepoint.DefineTaggedRegister(pushed_reg_index);
      }
      pushed_reg_index++;
    }
    int num_pushed_double_reg = snapshot_.live_double_registers.Count();
    safepoint.SetNumPushedRegisters(pushed_reg_index + num_pushed_double_reg);
    return safepoint;
  }

  MaglevSafepointTableBuilder::Safepoint DefineSafepointWithLazyDeopt(
      LazyDeoptInfo* lazy_deopt_info) {
    lazy_deopt_info->set_deopting_call_return_pc(
        masm->pc_offset_for_safepoint());
    masm->code_gen_state()->PushLazyDeopt(lazy_deopt_info);
    return DefineSafepoint();
  }

 private:
  MaglevAssembler* masm;
  RegisterSnapshot snapshot_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_ASSEMBLER_H_
