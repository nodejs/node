// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_

#include <iosfwd>
#include <memory>

#include "src/base/bits.h"
#include "src/base/small-vector.h"
#include "src/frames.h"
#include "src/macro-assembler.h"
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {

// Forward declarations.
namespace compiler {
class CallDescriptor;
}

namespace wasm {

class LiftoffAssembler : public TurboAssembler {
 public:
  // Each slot in our stack frame currently has exactly 8 bytes.
  static constexpr uint32_t kStackSlotSize = 8;

  static constexpr ValueType kWasmIntPtr =
      kSystemPointerSize == 8 ? kWasmI64 : kWasmI32;

  class VarState {
   public:
    enum Location : uint8_t { kStack, kRegister, kIntConst };

    explicit VarState(ValueType type) : loc_(kStack), type_(type) {}
    explicit VarState(ValueType type, LiftoffRegister r)
        : loc_(kRegister), type_(type), reg_(r) {
      DCHECK_EQ(r.reg_class(), reg_class_for(type));
    }
    explicit VarState(ValueType type, int32_t i32_const)
        : loc_(kIntConst), type_(type), i32_const_(i32_const) {
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
        case kIntConst:
          return i32_const_ == other.i32_const_;
      }
      UNREACHABLE();
    }

    bool is_stack() const { return loc_ == kStack; }
    bool is_gp_reg() const { return loc_ == kRegister && reg_.is_gp(); }
    bool is_fp_reg() const { return loc_ == kRegister && reg_.is_fp(); }
    bool is_reg() const { return loc_ == kRegister; }
    bool is_const() const { return loc_ == kIntConst; }

    ValueType type() const { return type_; }

    Location loc() const { return loc_; }

    int32_t i32_const() const {
      DCHECK_EQ(loc_, kIntConst);
      return i32_const_;
    }
    WasmValue constant() const {
      DCHECK(type_ == kWasmI32 || type_ == kWasmI64);
      DCHECK_EQ(loc_, kIntConst);
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
      int32_t i32_const_;    // used if loc_ == kIntConst
    };
  };

  ASSERT_TRIVIALLY_COPYABLE(VarState);

  struct CacheState {
    // Allow default construction, move construction, and move assignment.
    CacheState() = default;
    CacheState(CacheState&&) V8_NOEXCEPT = default;
    CacheState& operator=(CacheState&&) V8_NOEXCEPT = default;

    base::SmallVector<VarState, 8> stack_state;
    LiftoffRegList used_registers;
    uint32_t register_use_count[kAfterMaxLiftoffRegCode] = {0};
    LiftoffRegList last_spilled_regs;

    bool has_unused_register(RegClass rc, LiftoffRegList pinned = {}) const {
      if (kNeedI64RegPair && rc == kGpRegPair) {
        LiftoffRegList available_regs =
            kGpCacheRegList.MaskOut(used_registers).MaskOut(pinned);
        return available_regs.GetNumRegsSet() >= 2;
      }
      DCHECK(rc == kGpReg || rc == kFpReg);
      LiftoffRegList candidates = GetCacheRegList(rc);
      return has_unused_register(candidates, pinned);
    }

    bool has_unused_register(LiftoffRegList candidates,
                             LiftoffRegList pinned = {}) const {
      LiftoffRegList available_regs =
          candidates.MaskOut(used_registers).MaskOut(pinned);
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
      LiftoffRegList available_regs =
          candidates.MaskOut(used_registers).MaskOut(pinned);
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
                   uint32_t arity, uint32_t stack_depth);

    void Steal(const CacheState& source);

    void Split(const CacheState& source);

    uint32_t stack_height() const {
      return static_cast<uint32_t>(stack_state.size());
    }

   private:
    // Make the copy assignment operator private (to be used from {Split()}).
    CacheState& operator=(const CacheState&) V8_NOEXCEPT = default;
    // Disallow copy construction.
    CacheState(const CacheState&) = delete;
  };

  explicit LiftoffAssembler(std::unique_ptr<AssemblerBuffer>);
  ~LiftoffAssembler() override;

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

  void MergeFullStackWith(const CacheState& target, const CacheState& source);
  void MergeStackWith(const CacheState& target, uint32_t arity);

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
  void PrepareCall(FunctionSig*, compiler::CallDescriptor*,
                   Register* target = nullptr,
                   Register* target_instance = nullptr);
  // Process return values of the call.
  void FinishCall(FunctionSig*, compiler::CallDescriptor*);

  // Move {src} into {dst}. {src} and {dst} must be different.
  void Move(LiftoffRegister dst, LiftoffRegister src, ValueType);

  // Parallel register move: For a list of tuples <dst, src, type>, move the
  // {src} register of type {type} into {dst}. If {src} equals {dst}, ignore
  // that tuple.
  struct ParallelRegisterMoveTuple {
    LiftoffRegister dst;
    LiftoffRegister src;
    ValueType type;
    template <typename Dst, typename Src>
    ParallelRegisterMoveTuple(Dst dst, Src src, ValueType type)
        : dst(dst), src(src), type(type) {}
  };
  void ParallelRegisterMove(Vector<ParallelRegisterMoveTuple>);

  void MoveToReturnRegisters(FunctionSig*);

#ifdef ENABLE_SLOW_DCHECKS
  // Validate that the register use counts reflect the state of the cache.
  bool ValidateCacheState() const;
#endif

  ////////////////////////////////////
  // Platform-specific part.        //
  ////////////////////////////////////

  // This function emits machine code to prepare the stack frame, before the
  // size of the stack frame is known. It returns an offset in the machine code
  // which can later be patched (via {PatchPrepareStackFrame)} when the size of
  // the frame is known.
  inline int PrepareStackFrame();
  inline void PatchPrepareStackFrame(int offset, uint32_t stack_slots);
  inline void FinishCode();
  inline void AbortCompilation();

  inline void LoadConstant(LiftoffRegister, WasmValue,
                           RelocInfo::Mode rmode = RelocInfo::NONE);
  inline void LoadFromInstance(Register dst, uint32_t offset, int size);
  inline void LoadTaggedPointerFromInstance(Register dst, uint32_t offset);
  inline void SpillInstance(Register instance);
  inline void FillInstanceInto(Register dst);
  inline void LoadTaggedPointer(Register dst, Register src_addr,
                                Register offset_reg, uint32_t offset_imm,
                                LiftoffRegList pinned);
  inline void Load(LiftoffRegister dst, Register src_addr, Register offset_reg,
                   uint32_t offset_imm, LoadType type, LiftoffRegList pinned,
                   uint32_t* protected_load_pc = nullptr,
                   bool is_load_mem = false);
  inline void Store(Register dst_addr, Register offset_reg, uint32_t offset_imm,
                    LiftoffRegister src, StoreType type, LiftoffRegList pinned,
                    uint32_t* protected_store_pc = nullptr,
                    bool is_store_mem = false);
  inline void LoadCallerFrameSlot(LiftoffRegister, uint32_t caller_slot_idx,
                                  ValueType);
  inline void MoveStackValue(uint32_t dst_index, uint32_t src_index, ValueType);

  inline void Move(Register dst, Register src, ValueType);
  inline void Move(DoubleRegister dst, DoubleRegister src, ValueType);

  inline void Spill(uint32_t index, LiftoffRegister, ValueType);
  inline void Spill(uint32_t index, WasmValue);
  inline void Fill(LiftoffRegister, uint32_t index, ValueType);
  // Only used on 32-bit systems: Fill a register from a "half stack slot", i.e.
  // 4 bytes on the stack holding half of a 64-bit value.
  inline void FillI64Half(Register, uint32_t index, RegPairHalf);

  // i32 binops.
  inline void emit_i32_add(Register dst, Register lhs, Register rhs);
  inline void emit_i32_sub(Register dst, Register lhs, Register rhs);
  inline void emit_i32_mul(Register dst, Register lhs, Register rhs);
  inline void emit_i32_divs(Register dst, Register lhs, Register rhs,
                            Label* trap_div_by_zero,
                            Label* trap_div_unrepresentable);
  inline void emit_i32_divu(Register dst, Register lhs, Register rhs,
                            Label* trap_div_by_zero);
  inline void emit_i32_rems(Register dst, Register lhs, Register rhs,
                            Label* trap_rem_by_zero);
  inline void emit_i32_remu(Register dst, Register lhs, Register rhs,
                            Label* trap_rem_by_zero);
  inline void emit_i32_and(Register dst, Register lhs, Register rhs);
  inline void emit_i32_or(Register dst, Register lhs, Register rhs);
  inline void emit_i32_xor(Register dst, Register lhs, Register rhs);
  inline void emit_i32_shl(Register dst, Register src, Register amount,
                           LiftoffRegList pinned = {});
  inline void emit_i32_sar(Register dst, Register src, Register amount,
                           LiftoffRegList pinned = {});
  inline void emit_i32_shr(Register dst, Register src, Register amount,
                           LiftoffRegList pinned = {});
  inline void emit_i32_shr(Register dst, Register src, int amount);

  // i32 unops.
  inline bool emit_i32_clz(Register dst, Register src);
  inline bool emit_i32_ctz(Register dst, Register src);
  inline bool emit_i32_popcnt(Register dst, Register src);

  // i64 binops.
  inline void emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline void emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline void emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs);
  inline bool emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                            LiftoffRegister rhs, Label* trap_div_by_zero,
                            Label* trap_div_unrepresentable);
  inline bool emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                            LiftoffRegister rhs, Label* trap_div_by_zero);
  inline bool emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                            LiftoffRegister rhs, Label* trap_rem_by_zero);
  inline bool emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                            LiftoffRegister rhs, Label* trap_rem_by_zero);
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
  inline void emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                           int amount);

  inline void emit_i32_to_intptr(Register dst, Register src);

  inline void emit_ptrsize_add(Register dst, Register lhs, Register rhs) {
    if (kSystemPointerSize == 8) {
      emit_i64_add(LiftoffRegister(dst), LiftoffRegister(lhs),
                   LiftoffRegister(rhs));
    } else {
      emit_i32_add(dst, lhs, rhs);
    }
  }
  inline void emit_ptrsize_sub(Register dst, Register lhs, Register rhs) {
    if (kSystemPointerSize == 8) {
      emit_i64_sub(LiftoffRegister(dst), LiftoffRegister(lhs),
                   LiftoffRegister(rhs));
    } else {
      emit_i32_sub(dst, lhs, rhs);
    }
  }
  inline void emit_ptrsize_and(Register dst, Register lhs, Register rhs) {
    if (kSystemPointerSize == 8) {
      emit_i64_and(LiftoffRegister(dst), LiftoffRegister(lhs),
                   LiftoffRegister(rhs));
    } else {
      emit_i32_and(dst, lhs, rhs);
    }
  }
  inline void emit_ptrsize_shr(Register dst, Register src, int amount) {
    if (kSystemPointerSize == 8) {
      emit_i64_shr(LiftoffRegister(dst), LiftoffRegister(src), amount);
    } else {
      emit_i32_shr(dst, src, amount);
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
  inline void emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                DoubleRegister rhs);

  // f32 unops.
  inline void emit_f32_abs(DoubleRegister dst, DoubleRegister src);
  inline void emit_f32_neg(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f32_ceil(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f32_floor(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f32_trunc(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f32_nearest_int(DoubleRegister dst, DoubleRegister src);
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
  inline void emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                DoubleRegister rhs);

  // f64 unops.
  inline void emit_f64_abs(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_neg(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f64_ceil(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f64_floor(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f64_trunc(DoubleRegister dst, DoubleRegister src);
  inline bool emit_f64_nearest_int(DoubleRegister dst, DoubleRegister src);
  inline void emit_f64_sqrt(DoubleRegister dst, DoubleRegister src);

  inline bool emit_type_conversion(WasmOpcode opcode, LiftoffRegister dst,
                                   LiftoffRegister src, Label* trap = nullptr);

  inline void emit_i32_signextend_i8(Register dst, Register src);
  inline void emit_i32_signextend_i16(Register dst, Register src);
  inline void emit_i64_signextend_i8(LiftoffRegister dst, LiftoffRegister src);
  inline void emit_i64_signextend_i16(LiftoffRegister dst, LiftoffRegister src);
  inline void emit_i64_signextend_i32(LiftoffRegister dst, LiftoffRegister src);

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

  inline void StackCheck(Label* ool_code, Register limit_address);

  inline void CallTrapCallbackForTesting();

  inline void AssertUnreachable(AbortReason reason);

  inline void PushRegisters(LiftoffRegList);
  inline void PopRegisters(LiftoffRegList);

  inline void DropStackSlotsAndRet(uint32_t num_stack_slots);

  // Execute a C call. Arguments are pushed to the stack and a pointer to this
  // region is passed to the C function. If {out_argument_type != kWasmStmt},
  // this is the return value of the C function, stored in {rets[0]}. Further
  // outputs (specified in {sig->returns()}) are read from the buffer and stored
  // in the remaining {rets} registers.
  inline void CallC(FunctionSig* sig, const LiftoffRegister* args,
                    const LiftoffRegister* rets, ValueType out_argument_type,
                    int stack_bytes, ExternalReference ext_ref);

  inline void CallNativeWasmCode(Address addr);
  // Indirect call: If {target == no_reg}, then pop the target from the stack.
  inline void CallIndirect(FunctionSig* sig,
                           compiler::CallDescriptor* call_descriptor,
                           Register target);
  inline void CallRuntimeStub(WasmCode::RuntimeStubId sid);

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
  const CacheState* cache_state() const { return &cache_state_; }

  bool did_bailout() { return bailout_reason_ != nullptr; }
  const char* bailout_reason() const { return bailout_reason_; }

  void bailout(const char* reason) {
    if (bailout_reason_ != nullptr) return;
    AbortCompilation();
    bailout_reason_ = reason;
  }

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
  // If {dst.low_gp()} does not overlap with {lhs.high_gp()} or {rhs.high_gp()},
  // just first compute the lower half, then the upper half.
  if (dst.low() != lhs.high() && dst.low() != rhs.high()) {
    (assm->*op)(dst.low_gp(), lhs.low_gp(), rhs.low_gp());
    (assm->*op)(dst.high_gp(), lhs.high_gp(), rhs.high_gp());
    return;
  }
  // If {dst.high_gp()} does not overlap with {lhs.low_gp()} or {rhs.low_gp()},
  // we can compute this the other way around.
  if (dst.high() != lhs.low() && dst.high() != rhs.low()) {
    (assm->*op)(dst.high_gp(), lhs.high_gp(), rhs.high_gp());
    (assm->*op)(dst.low_gp(), lhs.low_gp(), rhs.low_gp());
    return;
  }
  // Otherwise, we need a temporary register.
  Register tmp =
      assm->GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(lhs, rhs)).gp();
  (assm->*op)(tmp, lhs.low_gp(), rhs.low_gp());
  (assm->*op)(dst.high_gp(), lhs.high_gp(), rhs.high_gp());
  assm->Move(dst.low_gp(), tmp, kWasmI32);
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

class LiftoffStackSlots {
 public:
  explicit LiftoffStackSlots(LiftoffAssembler* wasm_asm) : asm_(wasm_asm) {}

  void Add(const LiftoffAssembler::VarState& src, uint32_t src_index,
           RegPairHalf half) {
    slots_.emplace_back(src, src_index, half);
  }
  void Add(const LiftoffAssembler::VarState& src) { slots_.emplace_back(src); }

  inline void Construct();

 private:
  struct Slot {
    // Allow move construction.
    Slot(Slot&&) V8_NOEXCEPT = default;
    Slot(const LiftoffAssembler::VarState& src, uint32_t src_index,
         RegPairHalf half)
        : src_(src), src_index_(src_index), half_(half) {}
    explicit Slot(const LiftoffAssembler::VarState& src)
        : src_(src), half_(kLowWord) {}

    const LiftoffAssembler::VarState src_;
    uint32_t src_index_ = 0;
    RegPairHalf half_;
  };

  base::SmallVector<Slot, 8> slots_;
  LiftoffAssembler* const asm_;

  DISALLOW_COPY_AND_ASSIGN(LiftoffStackSlots);
};

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
