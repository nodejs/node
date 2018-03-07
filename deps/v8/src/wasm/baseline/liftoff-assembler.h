// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_
#define V8_WASM_BASELINE_LIFTOFF_ASSEMBLER_H_

#include <iosfwd>
#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
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
  // TODO(clemensh): Remove this limitation by allocating more stack space if
  // needed.
  static constexpr int kMaxValueStackHeight = 8;

  // Each slot in our stack frame currently has exactly 8 bytes.
  static constexpr uint32_t kStackSlotSize = 8;

  class VarState {
   public:
    enum Location : uint8_t { kStack, kRegister, kI32Const };

    explicit VarState(ValueType type) : loc_(kStack), type_(type) {}
    explicit VarState(ValueType type, LiftoffRegister r)
        : loc_(kRegister), type_(type), reg_(r) {
      DCHECK_EQ(r.reg_class(), reg_class_for(type));
    }
    explicit VarState(ValueType type, uint32_t i32_const)
        : loc_(kI32Const), type_(type), i32_const_(i32_const) {
      DCHECK(type_ == kWasmI32 || type_ == kWasmI64);
    }

    bool operator==(const VarState& other) const {
      if (loc_ != other.loc_) return false;
      switch (loc_) {
        case kStack:
          return true;
        case kRegister:
          return reg_ == other.reg_;
        case kI32Const:
          return i32_const_ == other.i32_const_;
      }
      UNREACHABLE();
    }

    bool is_stack() const { return loc_ == kStack; }
    bool is_gp_reg() const { return loc_ == kRegister && reg_.is_gp(); }
    bool is_fp_reg() const { return loc_ == kRegister && reg_.is_fp(); }
    bool is_reg() const { return loc_ == kRegister; }
    bool is_const() const { return loc_ == kI32Const; }

    ValueType type() const { return type_; }

    Location loc() const { return loc_; }

    uint32_t i32_const() const {
      DCHECK_EQ(loc_, kI32Const);
      return i32_const_;
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
      uint32_t i32_const_;   // used if loc_ == kI32Const
    };
  };

  static_assert(IS_TRIVIALLY_COPYABLE(VarState),
                "VarState should be trivially copyable");

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
      DCHECK(rc == kGpReg || rc == kFpReg);
      LiftoffRegList candidates = GetCacheRegList(rc);
      return unused_register(candidates);
    }

    LiftoffRegister unused_register(LiftoffRegList candidates,
                                    LiftoffRegList pinned = {}) const {
      LiftoffRegList available_regs = candidates & ~used_registers & ~pinned;
      return available_regs.GetFirstRegSet();
    }

    void inc_used(LiftoffRegister reg) {
      used_registers.set(reg);
      DCHECK_GT(kMaxInt, register_use_count[reg.liftoff_code()]);
      ++register_use_count[reg.liftoff_code()];
    }

    // Returns whether this was the last use.
    bool dec_used(LiftoffRegister reg) {
      DCHECK(is_used(reg));
      int code = reg.liftoff_code();
      DCHECK_LT(0, register_use_count[code]);
      if (--register_use_count[code] != 0) return false;
      used_registers.clear(reg);
      return true;
    }

    bool is_used(LiftoffRegister reg) const {
      bool used = used_registers.has(reg);
      DCHECK_EQ(used, register_use_count[reg.liftoff_code()] != 0);
      return used;
    }

    uint32_t get_use_count(LiftoffRegister reg) const {
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

  LiftoffRegister GetBinaryOpTargetRegister(RegClass,
                                            LiftoffRegList pinned = {});
  LiftoffRegister GetUnaryOpTargetRegister(RegClass,
                                           LiftoffRegList pinned = {});

  LiftoffRegister PopToRegister(RegClass, LiftoffRegList pinned = {});

  void PushRegister(ValueType type, LiftoffRegister reg) {
    DCHECK_EQ(reg_class_for(type), reg.reg_class());
    cache_state_.inc_used(reg);
    cache_state_.stack_state.emplace_back(type, reg);
  }

  void SpillRegister(LiftoffRegister);

  uint32_t GetNumUses(LiftoffRegister reg) {
    return cache_state_.get_use_count(reg);
  }

  // Get an unused register for class {rc}, potentially spilling to free one.
  LiftoffRegister GetUnusedRegister(RegClass rc, LiftoffRegList pinned = {}) {
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

  // Load parameters into the right registers / stack slots for the call.
  void PrepareCall(wasm::FunctionSig*, compiler::CallDescriptor*);
  // Process return values of the call.
  void FinishCall(wasm::FunctionSig*, compiler::CallDescriptor*);

  ////////////////////////////////////
  // Platform-specific part.        //
  ////////////////////////////////////

  inline void ReserveStackSpace(uint32_t bytes);

  inline void LoadConstant(LiftoffRegister, WasmValue);
  inline void LoadFromContext(Register dst, uint32_t offset, int size);
  inline void SpillContext(Register context);
  inline void FillContextInto(Register dst);
  inline void Load(LiftoffRegister dst, Register src_addr, Register offset_reg,
                   uint32_t offset_imm, LoadType type, LiftoffRegList pinned,
                   uint32_t* protected_load_pc = nullptr);
  inline void Store(Register dst_addr, Register offset_reg, uint32_t offset_imm,
                    LiftoffRegister src, StoreType type, LiftoffRegList pinned,
                    uint32_t* protected_store_pc = nullptr);
  inline void LoadCallerFrameSlot(LiftoffRegister, uint32_t caller_slot_idx);
  inline void MoveStackValue(uint32_t dst_index, uint32_t src_index);

  inline void MoveToReturnRegister(LiftoffRegister);
  // TODO(clemensh): Pass the type to {Move}, to emit more efficient code.
  inline void Move(LiftoffRegister dst, LiftoffRegister src);

  inline void Spill(uint32_t index, LiftoffRegister);
  inline void Spill(uint32_t index, WasmValue);
  inline void Fill(LiftoffRegister, uint32_t index);

  // i32 binops.
  inline void emit_i32_add(Register dst, Register lhs, Register rhs);
  inline void emit_i32_sub(Register dst, Register lhs, Register rhs);
  inline void emit_i32_mul(Register dst, Register lhs, Register rhs);
  inline void emit_i32_and(Register dst, Register lhs, Register rhs);
  inline void emit_i32_or(Register dst, Register lhs, Register rhs);
  inline void emit_i32_xor(Register dst, Register lhs, Register rhs);
  inline void emit_i32_shl(Register dst, Register lhs, Register rhs);
  inline void emit_i32_sar(Register dst, Register lhs, Register rhs);
  inline void emit_i32_shr(Register dst, Register lhs, Register rhs);

  // i32 unops.
  inline bool emit_i32_eqz(Register dst, Register src);
  inline bool emit_i32_clz(Register dst, Register src);
  inline bool emit_i32_ctz(Register dst, Register src);
  inline bool emit_i32_popcnt(Register dst, Register src);

  inline void emit_ptrsize_add(Register dst, Register lhs, Register rhs);

  inline void emit_f32_add(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_sub(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);
  inline void emit_f32_mul(DoubleRegister dst, DoubleRegister lhs,
                           DoubleRegister rhs);

  inline void emit_i32_test(Register);
  inline void emit_i32_compare(Register, Register);
  inline void emit_jump(Label*);
  inline void emit_cond_jump(Condition, Label*);

  inline void StackCheck(Label* ool_code);

  inline void CallTrapCallbackForTesting();

  inline void AssertUnreachable(AbortReason reason);

  // Push a value to the stack (will become a caller frame slot).
  inline void PushCallerFrameSlot(const VarState& src, uint32_t src_index);
  inline void PushCallerFrameSlot(LiftoffRegister reg);
  inline void PushRegisters(LiftoffRegList);
  inline void PopRegisters(LiftoffRegList);

  inline void DropStackSlotsAndRet(uint32_t num_stack_slots);

  // Push arguments on the stack (in the caller frame), then align the stack.
  // The address of the last argument will be stored to {arg_addr_dst}. Previous
  // arguments will be located at pointer sized buckets above that address.
  inline void PrepareCCall(uint32_t num_params, const Register* args);
  inline void SetCCallRegParamAddr(Register dst, uint32_t param_idx,
                                   uint32_t num_params);
  inline void SetCCallStackParamAddr(uint32_t stack_param_idx,
                                     uint32_t param_idx, uint32_t num_params);
  inline void CallC(ExternalReference ext_ref, uint32_t num_params);

  inline void CallNativeWasmCode(Address addr);

  inline void CallRuntime(Zone* zone, Runtime::FunctionId fid);

  // Reserve space in the current frame, store address to space in {addr}.
  inline void AllocateStackSlot(Register addr, uint32_t size);
  inline void DeallocateStackSlot(uint32_t size);

  ////////////////////////////////////
  // End of platform-specific part. //
  ////////////////////////////////////

  uint32_t num_locals() const { return num_locals_; }
  void set_num_locals(uint32_t num_locals);

  uint32_t GetTotalFrameSlotCount() const;

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

  LiftoffRegister SpillOneRegister(LiftoffRegList candidates,
                                   LiftoffRegList pinned);
};

std::ostream& operator<<(std::ostream& os, LiftoffAssembler::VarState);

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
