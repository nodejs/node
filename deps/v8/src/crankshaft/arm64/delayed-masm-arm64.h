// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_ARM64_DELAYED_MASM_ARM64_H_
#define V8_CRANKSHAFT_ARM64_DELAYED_MASM_ARM64_H_

#include "src/crankshaft/lithium.h"

namespace v8 {
namespace internal {

class LCodeGen;

// This class delays the generation of some instructions. This way, we have a
// chance to merge two instructions in one (with load/store pair).
// Each instruction must either:
//  - merge with the pending instruction and generate just one instruction.
//  - emit the pending instruction and then generate the instruction (or set the
//    pending instruction).
class DelayedMasm BASE_EMBEDDED {
 public:
  inline DelayedMasm(LCodeGen* owner, MacroAssembler* masm,
                     const Register& scratch_register);

  ~DelayedMasm() {
    DCHECK(!scratch_register_acquired_);
    DCHECK(!scratch_register_used_);
    DCHECK(!pending());
  }
  inline void EndDelayedUse();

  const Register& ScratchRegister() {
    scratch_register_used_ = true;
    return scratch_register_;
  }
  bool IsScratchRegister(const CPURegister& reg) {
    return reg.Is(scratch_register_);
  }
  bool scratch_register_used() const { return scratch_register_used_; }
  void reset_scratch_register_used() { scratch_register_used_ = false; }
  // Acquire/Release scratch register for use outside this class.
  void AcquireScratchRegister() {
    EmitPending();
    ResetSavedValue();
#ifdef DEBUG
    DCHECK(!scratch_register_acquired_);
    scratch_register_acquired_ = true;
#endif
  }
  void ReleaseScratchRegister() {
#ifdef DEBUG
    DCHECK(scratch_register_acquired_);
    scratch_register_acquired_ = false;
#endif
  }
  bool pending() { return pending_ != kNone; }

  // Extra layer over the macro-assembler instructions (which emits the
  // potential pending instruction).
  inline void Mov(const Register& rd,
                  const Operand& operand,
                  DiscardMoveMode discard_mode = kDontDiscardForSameWReg);
  inline void Fmov(FPRegister fd, FPRegister fn);
  inline void Fmov(FPRegister fd, double imm);
  inline void LoadObject(Register result, Handle<Object> object);
  // Instructions which try to merge which the pending instructions.
  void StackSlotMove(LOperand* src, LOperand* dst);
  // StoreConstant can only be used if the scratch register is not acquired.
  void StoreConstant(uint64_t value, const MemOperand& operand);
  void Load(const CPURegister& rd, const MemOperand& operand);
  void Store(const CPURegister& rd, const MemOperand& operand);
  // Emit the potential pending instruction.
  void EmitPending();
  // Reset the pending state.
  void ResetPending() {
    pending_ = kNone;
#ifdef DEBUG
    pending_register_ = no_reg;
    MemOperand tmp;
    pending_address_src_ = tmp;
    pending_address_dst_ = tmp;
    pending_value_ = 0;
    pending_pc_ = 0;
#endif
  }
  inline void InitializeRootRegister();

 private:
  // Set the saved value and load the ScratchRegister with it.
  void SetSavedValue(uint64_t saved_value) {
    DCHECK(saved_value != 0);
    if (saved_value_ != saved_value) {
      masm_->Mov(ScratchRegister(), saved_value);
      saved_value_ = saved_value;
    }
  }
  // Reset the saved value (i.e. the value of ScratchRegister is no longer
  // known).
  void ResetSavedValue() {
    saved_value_ = 0;
  }

  LCodeGen* cgen_;
  MacroAssembler* masm_;

  // Register used to store a constant.
  Register scratch_register_;
  bool scratch_register_used_;

  // Sometimes we store or load two values in two contiguous stack slots.
  // In this case, we try to use the ldp/stp instructions to reduce code size.
  // To be able to do that, instead of generating directly the instructions,
  // we register with the following fields that an instruction needs to be
  // generated. Then with the next instruction, if the instruction is
  // consistent with the pending one for stp/ldp we generate ldp/stp. Else,
  // if they are not consistent, we generate the pending instruction and we
  // register the new instruction (which becomes pending).

  // Enumeration of instructions which can be pending.
  enum Pending {
    kNone,
    kStoreConstant,
    kLoad, kStore,
    kStackSlotMove
  };
  // The pending instruction.
  Pending pending_;
  // For kLoad, kStore: register which must be loaded/stored.
  CPURegister pending_register_;
  // For kLoad, kStackSlotMove: address of the load.
  MemOperand pending_address_src_;
  // For kStoreConstant, kStore, kStackSlotMove: address of the store.
  MemOperand pending_address_dst_;
  // For kStoreConstant: value to be stored.
  uint64_t pending_value_;
  // Value held into the ScratchRegister if the saved_value_ is not 0.
  // For 0, we use xzr.
  uint64_t saved_value_;
#ifdef DEBUG
  // Address where the pending instruction must be generated. It's only used to
  // check that nothing else has been generated since we set the pending
  // instruction.
  int pending_pc_;
  // If true, the scratch register has been acquired outside this class. The
  // scratch register can no longer be used for constants.
  bool scratch_register_acquired_;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_ARM64_DELAYED_MASM_ARM64_H_
