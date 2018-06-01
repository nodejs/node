// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_

#include <iosfwd>
#include <memory>

#include "src/base/bits.h"
#include "src/frames.h"
#include "src/macro-assembler.h"
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {
namespace wasm {

// Forward declarations.
struct ModuleEnv;

class LiftoffAssembler : public TurboAssembler {
 public:
  // Each slot in our stack frame currently has exactly 8 bytes.
  static constexpr uint32_t kStackSlotSize = 8;

  static constexpr ValueType kWasmIntPtr =
      kPointerSize == 8 ? kWasmI64 : kWasmI32;

  class VarState {
   public:
    enum Location : uint8_t { kStack, kRegister, KIntConst };

    explicit VarState(ValueType type) : loc_(kStack), type_(type) {}
    explicit VarState(ValueType type, LiftoffRegister r)
        : loc_(kRegister), type_(type), reg_(r) {
      DCHECK_EQ(r.reg_class(), reg_class_for(type));
    }
    explicit VarState(ValueType type, int32_t i32_const)
        : loc_(KIntConst), type_(type), i32_const_(i32_const) {
      DCHECK(type_ == kWasmI32 || type_ == kWasmI64);
    }

    bool operator==(const VarState& other) const {
      if (loc_ != other.loc_) return false;
      if (type_ != other.type_) return false;
      switch (loc_) {
        case kStack:
          return true;
        case kRegister:
          return reg_ == other.reg_;
        case KIntConst:
          return i32_const_ == other.i32_const_;
      }
      UNREACHABLE();
    }

    bool is_stack() const { return loc_ == kStack; }
    bool is_gp_reg() const { return loc_ == kRegister && reg_.is_gp(); }
    bool is_fp_reg() const { return loc_ == kRegister && reg_.is_fp(); }
    bool is_reg() const { return loc_ == kRegister; }
    bool is_const() const { return loc_ == KIntConst; }

    ValueType type() const { return type_; }

    Location loc() const { return loc_; }

    int32_t i32_const() const {
      DCHECK_EQ(loc_, KIntConst);
      return i32_const_;
    }
    WasmValue constant() const {
      DCHECK(type_ == kWasmI32 || type_ == kWasmI64);
      DCHECK_EQ(loc_, KIntConst);
      return type_ == kWasmI32 ? WasmValue(i32_const_)
                               : WasmValue(int64_t{i32_const_});
    }

    Register gp_reg() const { return reg().gp(); }
    DoubleRegister fp_reg() const { return reg().fp(); }
    LiftoffRegister reg() const {
      DCHECK_EQ(loc_, kRegister);
      return reg_;
    }
    RegClass reg_class() const { return reg().reg_class(); }

    void MakeStack() { loc_ = kStack; }

   private:
    Location loc_;
    // TODO(wasm): This is redundant, the decoder already knows the type of each
    // stack value. Try to collapse.
    ValueType type_;

    union {
      LiftoffRegister reg_;  // used if loc_ == kRegister
      int32_t i32_const_;    // used if loc_ == KIntConst
    };
  };

  ASSERT_TRIVIALLY_COPYABLE(VarState);

  struct CacheState {
    // Allow default construction, move construction, and move assignment.
    CacheState() = default;
    CacheState(CacheState&&) = default;
    CacheState& operator=(CacheState&&) = default;

    // TODO(clemensh): Improve memory management here; avoid std::vector.
    std::vector<VarState> stack_state;
    LiftoffRegList used_registers;
    uint32_t register_use_count[kAfterMaxLiftoffRegCode] = {0};
    LiftoffRegList last_spilled_regs;
    // TODO(clemensh): Remove stack_base; use ControlBase::stack_depth.
    uint32_t stack_base = 0;

    bool has_unused_register(RegClass rc, LiftoffRegList pinned = {}) const {
      if (kNeedI64RegPair && rc == kGpRegPair) {
        LiftoffRegList available_regs =
            kGpCacheRegList & ~used_registers & ~pinned;
        return available_regs.GetNumRegsSet() >= 2;
      }
      DCHECK(rc == kGpReg || rc == kFpReg);
      LiftoffRegList candidates = GetCacheRegList(rc);
      return has_unused_register(candidates, pinned);
    }

    bool has_unused_register(LiftoffRegList candidates,
                             LiftoffRegList pinned = {}) const {
      LiftoffRegList available_regs = candidates & ~used_registers & ~pinned;
      return !available_regs.is_empty();
    }

    LiftoffRegister unused_register(RegClass rc,
                                    LiftoffRegList pinned = {}) const {
      if (kNeedI64RegPair && rc == kGpRegPair) {
        Register low = pinned.set(unused_register(kGpReg, pinned)).gp();
        Register high = unused_register(kGpReg, pinned).gp();
        return LiftoffRegister::ForPair(low, high);
      }
      DCHECK(rc == kGpReg || rc == kFpReg);
      LiftoffRegList candidates = GetCacheRegList(rc);
      return unused_register(candidates, pinned);
    }

    LiftoffRegister unused_register(LiftoffRegList candidates,
                                    LiftoffRegList pinned = {}) const {
      LiftoffRegList available_regs = candidates & ~used_registers & ~pinned;
      return available_regs.GetFirstRegSet();
    }

    void inc_used(LiftoffRegister reg) {
      if (reg.is_pair()) {
        inc_used(reg.low());
        inc_used(reg.high());
        return;
      }
      used_registers.set(reg);
      DCHECK_GT(kMaxInt, register_use_count[reg.liftoff_code()]);
      ++register_use_count[reg.liftoff_code()];
    }

    // Returns whether this was the last use.
    void dec_used(LiftoffRegister reg) {
      DCHECK(is_used(reg));
      if (reg.is_pair()) {
        dec_used(reg.low());
        dec_used(reg.high());
        return;
      }
      int code = reg.liftoff_code();
      DCHECK_LT(0, register_use_count[code]);
      if (--register_use_count[code] == 0) used_registers.clear(reg);
    }

    bool is_used(LiftoffRegister reg) const {
      if (reg.is_pair()) return is_used(reg.low()) || is_used(reg.high());
      bool used = used_registers.has(reg);
      DCHECK_EQ(used, register_use_count[reg.liftoff_code()] != 0);
      return used;
    }

    uint32_t get_use_count(LiftoffRegister reg) const {
      if (reg.is_pair()) {
        DCHECK_EQ(register_use_count[reg.low().liftoff_code()],
                  register_use_count[reg.high().liftoff_code()]);
        reg = reg.low();
      }
      DCHECK_GT(arraysize(register_use_count), reg.liftoff_code());
      return register_use_count[reg.liftoff_code()];
    }

    void clear_used(LiftoffRegister reg) {
      register_use_count[reg.liftoff_code()] = 0;
      used_registers.clear(reg);
    }

    bool is_free(LiftoffRegister reg) const { return !is_used(reg); }

    void reset_used_registers() {
      used_registers = {};
      memset(register_use_count, 0, sizeof(register_use_count));
    }

    LiftoffRegister GetNextSpillReg(LiftoffRegList candidates,
                                    LiftoffRegList pinned = {}) {
      LiftoffRegList unpinned = candidates.MaskOut(pinned);
      DCHECK(!unpinned.is_empty());
      // This method should only be called if none of the candidates is free.
      DCHECK(unpinned.MaskOut(used_registers).is_empty());
      LiftoffRegList unspilled = unpinned.MaskOut(last_spilled_regs);
      if (unspilled.is_empty()) {
        unspilled = unpinned;
        last_spilled_regs = {};
      }
      LiftoffRegister reg = unspilled.GetFirstRegSet();
      last_spilled_regs.set(reg);
      return reg;
    }

    // TODO(clemensh): Don't copy the full parent state (this makes us N^2).
    void InitMerge(const CacheState& source, uint32_t num_locals,
                   uint32_t arity);

    void Steal(CacheState& source);

    void Split(const CacheState& source);

    uint32_t stack_height() const {
      return static_cast<uint32_t>(stack_state.size());
    }

   private:
    // Make the copy assignment operator private (to be used from {Split()}).
    CacheState& operator=(const CacheState&) = default;
    // Disallow copy construction.
    CacheState(const CacheState&) = delete;
  };

  explicit LiftoffAssembler(Isolate* isolate);
  ~LiftoffAssembler();

  LiftoffRegister PopToRegister(LiftoffRegList pinned = {});

  void PushRegister(ValueType type, LiftoffRegister reg) {
    DCHECK_EQ(reg_class_for(type), reg.reg_class());
    cache_state_.inc_used(reg);
    cache_state_.stack_state.emplace_back(type, reg);
  }

  void SpillRegister(LiftoffRegister);

  uint32_t GetNumUses(LiftoffRegister reg) {
    return cache_state_.get_use_count(reg);
  }

  // Get an unused register for class {rc}, reusing one of {try_first} if
  // possible.
  LiftoffRegister GetUnusedRegister(
      RegClass rc, std::initializer_list<LiftoffRegister> try_first,
      LiftoffRegList pinned = {}) {
    for (LiftoffRegister reg : try_first) {
      DCHECK_EQ(reg.reg_class(), rc);
      if (cache_state_.is_free(reg)) return reg;
    }
    return GetUnusedRegister(rc, pinned);
  }

  // Get an unused register for class {rc}, potentially spilling to free one.
  LiftoffRegister GetUnusedRegister(RegClass rc, LiftoffRegList pinned = {}) {
    if (kNeedI64RegPair && rc == kGpRegPair) {
      LiftoffRegList candidates = kGpCacheRegList;
      Register low = pinned.set(GetUnusedRegister(candidates, pinned)).gp();
      Register high = GetUnusedRegister(candidates, pinned).gp();
      return LiftoffRegister::ForPair(low, high);
    }
    DCHECK(rc == kGpReg || rc == kFpReg);
    LiftoffRegList candidates = GetCacheRegList(rc);
    return GetUnusedRegister(candidates, pinned);
  }

  // Get an unused register of {candidates}, potentially spilling to free one.
  LiftoffRegister GetUnusedRegister(LiftoffRegList candidates,
                                    LiftoffRegList pinned = {}) {
    if (cache_state_.has_unused_register(candidates, pinned)) {
      return cache_state_.unused_register(candidates, pinned);
    }
    return SpillOneRegister(candidates, pinned);
  }

  void DropStackSlot(VarState* slot) {
    // The only loc we care about is register. Other types don't occupy
    // anything.
    if (!slot->is_reg()) return;
    // Free the register, then set the loc to "stack".
    // No need to write back, the value should be dropped.
    cache_state_.dec_used(slot->reg());
    slot->MakeStack();
  }

  void MergeFullStackWith(CacheState&);
  void MergeStackWith(CacheState&, uint32_t arity);

  void Spill(uint32_t index);
  void SpillLocals();
  void SpillAllRegisters();

  // Call this method whenever spilling something, such that the number of used
  // spill slot can be tracked and the stack frame will be allocated big enough.
  void RecordUsedSpillSlot(uint32_t index) {
    if (index >= num_used_spill_slots_) num_used_spill_slots_ = index + 1;
  }

  // Load parameters into the right registers / stack slots for the call.
  // Move {*target} into another register if needed and update {*target} to that
  // register, or {no_reg} if target was spilled to the stack.
  void PrepareCall(wasm::FunctionSig*, compiler::CallDescriptor*,
                   Register* target = nullptr,
                   LiftoffRegister* target_instance = nullptr);
  // Process return values of the call.
  void FinishCall(wasm::FunctionSig*, compiler::CallDescriptor*);

  // Move {src} into {dst}. {src} and {dst} must be different.
  void Move(LiftoffRegister dst, LiftoffRegister src, ValueType);

  // Parallel register move: For a list of tuples <dst, src, type>, move the
  // {src} register of type {type} into {dst}. If {src} equals {dst}, ignore
  // that tuple.
  struct ParallelRegisterMoveTuple {
    LiftoffRegister dst;
    LiftoffRegister src;
    ValueType type;
  };
  void ParallelRegisterMove(std::initializer_list<ParallelRegisterMoveTuple>);

  ////////////////////////////////////
  // Platform-specific part.        //
  ////////////////////////////////////

  // This function emits machine code to prepare the stack frame, before the
  // size of the stack frame is known. It returns an offset in the machine code
  // which can later be patched (via {PatchPrepareStackFrame)} when the size of
  // the frame is known.
  inline uint32_t PrepareStackFrame();
  inline void PatchPrepareStackFrame(uint32_t offset, uint32_t stack_slots);

  inline void LoadConstant(LiftoffRegister, WasmValue,
                           RelocInfo::Mode rmode = RelocInfo::NONE);
  inline void LoadFromInstance(Register dst, uint32_t offset, int size);
  inline void SpillInstance(Register instance);
  inline void FillInstanceInto(Register dst);
  inline void Load(LiftoffRegister dst, Register src_addr, Register offset_reg,
                   uint32_t offset_imm, LoadType type, LiftoffRegList pinned,
                   uint32_t* protected_load_pc = nullptr);
  inline void Store(Register dst_addr, Register offset_reg, uint32_t offset_imm,
                    LiftoffRegister src, StoreType type, LiftoffRegList pinned,
                    uint32_t* protected_store_pc = nullptr);
  inline void LoadCallerFrameSlot(LiftoffRegister, uint32_t caller_slot_idx,
                                  ValueType);
  inline void MoveStackValue(uint32_t dst_index, uint32_t src_index, ValueType);

  inline void MoveToReturnRegister(LiftoffRegister src, ValueType);
  inline void Move(Register dst, Register src, ValueType);
  inline void Move(DoubleRegister dst, DoubleRegister src, ValueType);

  inline void Spill(uint32_t index, LiftoffRegister, ValueType);
  inline void Spill(uint32_t index, WasmValue);
  inline void Fill(LiftoffRegister, uint32_t index, ValueType);
  // Only used on 32-bit systems: Fill a register from a "half stack slot", i.e.
  // 4 bytes on the stack holding half of a 64-bit value. The two half_indexes
  // corresponding to slot {index} are {2*index} and {2*index-1}.
  inline void FillI64Half(Register, uint32_t half_index);

  // i32 binops.
  inline void emit_i32_add(Register dst, Register lhs, Register rhs);
  inline void emit_i32_sub(Register dst, Register lhs, Register rhs);
  inline void emit_i32_mul(Register dst, Register lhs, Register rhs);
  inline void emit_i32_and(Register dst, Register lhs, Register rhs);
  inline void emit_i32_or(Register dst, Register lhs, Register rhs);
  inline void emit_i32_xor(Register dst, Register lhs, Register rhs);
  inline void emit_i32_shl(Register dst, Register src, Register amount,
                           LiftoffRegList pinned = {});
  inline void emit_i32_sar(Register dst, Register src, Register amount,
                           LiftoffRegList pinned = {});
  inline void emit_i32_shr(Register dst, Register src, Register amount,
                           LiftoffRegList pinned = {});

  // i32 unops.
  inline bool emit_i32_clz(Register dst, Register src);
  inline bool emit_i32_ctz(Register dst, Register src);
  inline bool emit_i32_popcnt(Register dst, Register src);

  // i64 binops.
  inline void emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline void emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline void emit_i64_and(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline void emit_i64_or(LiftoffRegister dst, LiftoffRegister lhs,
                          LiftoffRegister rhs);
  inline void emit_i64_xor(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline void emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                           Register amount, LiftoffRegList pinned = {});
  inline void emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                           Register amount, LiftoffRegList pinned = {});
  inline void emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                           Register amount, LiftoffRegList pinned = {});

  inline void emit_ptrsize_add(Register dst, Register lhs, Register rhs) {
    if (kPointerSize == 8) {
      emit_i64_add(LiftoffRegister(dst), LiftoffRegister(lhs),
                   LiftoffRegister(rhs));
    } else {
      emit_i32_add(dst, lhs, rhs);
    }
  }

  // f32 binops.
  inline void emit_f32_add(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_sub(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_mul(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_div(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  // f32 unops.
  inline void emit_f32_abs(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_neg(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_ceil(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_floor(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_trunc(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_nearest_int(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_sqrt(DoubleRegister dst, DoubleRegister src);

  // f64 binops.
  inline void emit_f64_add(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f64_sub(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f64_mul(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f64_div(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);

  // f64 unops.
  inline void emit_f64_abs(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_neg(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_ceil(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_floor(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_trunc(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_nearest_int(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_sqrt(DoubleRegister dst, DoubleRegister src);

  // type conversions.
  inline bool emit_type_conversion(WasmOpcode opcode, LiftoffRegister dst,
                                   LiftoffRegister src);

  inline void emit_jump(Label*);
  inline void emit_jump(Register);

  inline void emit_cond_jump(Condition, Label*, ValueType value, Register lhs,
                             Register rhs = no_reg);
  // Set {dst} to 1 if condition holds, 0 otherwise.
  inline void emit_i32_eqz(Register dst, Register src);
  inline void emit_i32_set_cond(Condition, Register dst, Register lhs,
                                Register rhs);
  inline void emit_i64_eqz(Register dst, LiftoffRegister src);
  inline void emit_i64_set_cond(Condition condition, Register dst,
                                LiftoffRegister lhs, LiftoffRegister rhs);
  inline void emit_f32_set_cond(Condition condition, Register dst,
                                DoubleRegister lhs, DoubleRegister rhs);
  inline void emit_f64_set_cond(Condition condition, Register dst,
                                DoubleRegister lhs, DoubleRegister rhs);

  inline void StackCheck(Label* ool_code);

  inline void CallTrapCallbackForTesting();

  inline void AssertUnreachable(AbortReason reason);

  // Push a value to the stack (will become a caller frame slot).
  inline void PushCallerFrameSlot(const VarState& src, uint32_t src_index,
                                  RegPairHalf half);
  inline void PushCallerFrameSlot(LiftoffRegister reg, ValueType type);
  inline void PushRegisters(LiftoffRegList);
  inline void PopRegisters(LiftoffRegList);

  inline void DropStackSlotsAndRet(uint32_t num_stack_slots);

  // {PrepareCCall} pushes the arguments on the stack (in the caller frame),
  // then aligns the stack to do a c call. Pointers to the pushed arguments are
  // later loaded to registers or stack slots via {SetCCall*ParamAddr}. After
  // the c call, the output parameter (if it exists) can be loaded via
  // {LoadCCallOutArgument}. {FinishCCall} resets the stack pointer to the state
  // before {PrepareCCall}.
  // The {FunctionSig} passed to {PrepareCCall} describes the types of
  // parameters which are then passed ot the C function via pointers, excluding
  // the out argument.
  inline void PrepareCCall(wasm::FunctionSig* sig, const LiftoffRegister* args,
                           ValueType out_argument_type);
  inline void SetCCallRegParamAddr(Register dst, int param_byte_offset,
                                   ValueType type);
  inline void SetCCallStackParamAddr(int stack_param_idx, int param_byte_offset,
                                     ValueType type);
  inline void LoadCCallOutArgument(LiftoffRegister dst, ValueType type,
                                   int param_byte_offset);
  inline void CallC(ExternalReference ext_ref, uint32_t num_params);
  inline void FinishCCall();

  inline void CallNativeWasmCode(Address addr);
  inline void CallRuntime(Zone* zone, Runtime::FunctionId fid);
  // Indirect call: If {target == no_reg}, then pop the target from the stack.
  inline void CallIndirect(wasm::FunctionSig* sig,
                           compiler::CallDescriptor* call_descriptor,
                           Register target);

  // Reserve space in the current frame, store address to space in {addr}.
  inline void AllocateStackSlot(Register addr, uint32_t size);
  inline void DeallocateStackSlot(uint32_t size);

  ////////////////////////////////////
  // End of platform-specific part. //
  ////////////////////////////////////

  uint32_t num_locals() const { return num_locals_; }
  void set_num_locals(uint32_t num_locals);

  uint32_t GetTotalFrameSlotCount() const {
    return num_locals_ + num_used_spill_slots_;
  }

  ValueType local_type(uint32_t index) {
    DCHECK_GT(num_locals_, index);
    ValueType* locals =
        num_locals_ <= kInlineLocalTypes ? local_types_ : more_local_types_;
    return locals[index];
  }

  void set_local_type(uint32_t index, ValueType type) {
    ValueType* locals =
        num_locals_ <= kInlineLocalTypes ? local_types_ : more_local_types_;
    locals[index] = type;
  }

  CacheState* cache_state() { return &cache_state_; }

  bool did_bailout() { return bailout_reason_ != nullptr; }
  const char* bailout_reason() const { return bailout_reason_; }

 private:
  uint32_t num_locals_ = 0;
  static constexpr uint32_t kInlineLocalTypes = 8;
  union {
    ValueType local_types_[kInlineLocalTypes];
    ValueType* more_local_types_;
  };
  static_assert(sizeof(ValueType) == 1,
                "Reconsider this inlining if ValueType gets bigger");
  CacheState cache_state_;
  uint32_t num_used_spill_slots_ = 0;
  const char* bailout_reason_ = nullptr;

  LiftoffRegister SpillOneRegister(LiftoffRegList candidates,
                                   LiftoffRegList pinned);

  void bailout(const char* reason) {
    if (bailout_reason_ == nullptr) bailout_reason_ = reason;
  }
};

std::ostream& operator<<(std::ostream& os, LiftoffAssembler::VarState);

// =======================================================================
// Partially platform-independent implementations of the platform-dependent
// part.

#ifdef V8_TARGET_ARCH_32_BIT

namespace liftoff {
template <void (LiftoffAssembler::*op)(Register, Register, Register)>
void EmitI64IndependentHalfOperation(LiftoffAssembler* assm,
                                     LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs) {
  // Register pairs are either the same, or they don't overlap at all, so the
  // low and high registers must be disjoint. Just handle them separately.
  DCHECK_EQ(LiftoffRegList{},
            LiftoffRegList::ForRegs(dst.low(), lhs.low(), rhs.low()) &
                LiftoffRegList::ForRegs(dst.high(), lhs.high(), rhs.high()));
  (assm->*op)(dst.low_gp(), lhs.low_gp(), rhs.low_gp());
  (assm->*op)(dst.high_gp(), lhs.high_gp(), rhs.high_gp());
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_and(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitI64IndependentHalfOperation<&LiftoffAssembler::emit_i32_and>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_or(LiftoffRegister dst, LiftoffRegister lhs,
                                   LiftoffRegister rhs) {
  liftoff::EmitI64IndependentHalfOperation<&LiftoffAssembler::emit_i32_or>(
      this, dst, lhs, rhs);
}

void LiftoffAssembler::emit_i64_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitI64IndependentHalfOperation<&LiftoffAssembler::emit_i32_xor>(
      this, dst, lhs, rhs);
}

#endif  // V8_TARGET_ARCH_32_BIT

// End of the partially platform-independent implementations of the
// platform-dependent part.
// =======================================================================

}  // namespace wasm
}  // namespace internal
}  // namespace v8

// Include platform specific implementation.
#if V8_TARGET_ARCH_IA32
#include "src/wasm/baseline/ia32/liftoff-assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/wasm/baseline/x64/liftoff-assembler-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/wasm/baseline/arm64/liftoff-assembler-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/wasm/baseline/arm/liftoff-assembler-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/wasm/baseline/ppc/liftoff-assembler-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/wasm/baseline/mips/liftoff-assembler-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/wasm/baseline/mips64/liftoff-assembler-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/wasm/baseline/s390/liftoff-assembler-s390.h"
#else
#error Unsupported architecture.
#endif

#endif  // V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_
