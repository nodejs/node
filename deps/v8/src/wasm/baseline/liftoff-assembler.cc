// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/counters.h"
#include "src/macro-assembler-inl.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

using VarState = LiftoffAssembler::VarState;

namespace {

#define __ asm_->

#define TRACE(...)                                            \
  do {                                                        \
    if (FLAG_trace_liftoff) PrintF("[liftoff] " __VA_ARGS__); \
  } while (false)

class StackTransferRecipe {
  struct RegisterMove {
    LiftoffRegister dst;
    LiftoffRegister src;
    constexpr RegisterMove(LiftoffRegister dst, LiftoffRegister src)
        : dst(dst), src(src) {}
  };
  struct RegisterLoad {
    LiftoffRegister dst;
    bool is_constant_load;  // otherwise load it from the stack.
    union {
      uint32_t stack_slot;
      WasmValue constant;
    };
    RegisterLoad(LiftoffRegister dst, WasmValue constant)
        : dst(dst), is_constant_load(true), constant(constant) {}
    RegisterLoad(LiftoffRegister dst, uint32_t stack_slot)
        : dst(dst), is_constant_load(false), stack_slot(stack_slot) {}
  };

 public:
  explicit StackTransferRecipe(LiftoffAssembler* wasm_asm) : asm_(wasm_asm) {}
  ~StackTransferRecipe() { Execute(); }

  void Execute() {
    // First, execute register moves. Then load constants and stack values into
    // registers.

    if ((move_dst_regs & move_src_regs).is_empty()) {
      // No overlap in src and dst registers. Just execute the moves in any
      // order.
      for (RegisterMove& rm : register_moves) asm_->Move(rm.dst, rm.src);
      register_moves.clear();
    } else {
      // Keep use counters of src registers.
      uint32_t src_reg_use_count[kAfterMaxLiftoffRegCode] = {0};
      for (RegisterMove& rm : register_moves) {
        ++src_reg_use_count[rm.src.liftoff_code()];
      }
      // Now repeatedly iterate the list of register moves, and execute those
      // whose dst register does not appear as src any more. The remaining moves
      // are compacted during this iteration.
      // If no more moves can be executed (because of a cycle), spill one
      // register to the stack, add a RegisterLoad to reload it later, and
      // continue.
      uint32_t next_spill_slot = asm_->cache_state()->stack_height();
      while (!register_moves.empty()) {
        int executed_moves = 0;
        for (auto& rm : register_moves) {
          if (src_reg_use_count[rm.dst.liftoff_code()] == 0) {
            asm_->Move(rm.dst, rm.src);
            ++executed_moves;
            DCHECK_LT(0, src_reg_use_count[rm.src.liftoff_code()]);
            --src_reg_use_count[rm.src.liftoff_code()];
          } else if (executed_moves) {
            // Compaction: Move not-executed moves to the beginning of the list.
            (&rm)[-executed_moves] = rm;
          }
        }
        if (executed_moves == 0) {
          // There is a cycle. Spill one register, then continue.
          // TODO(clemensh): Use an unused register if available.
          LiftoffRegister spill_reg = register_moves.back().src;
          asm_->Spill(next_spill_slot, spill_reg);
          // Remember to reload into the destination register later.
          LoadStackSlot(register_moves.back().dst, next_spill_slot);
          DCHECK_EQ(1, src_reg_use_count[spill_reg.liftoff_code()]);
          src_reg_use_count[spill_reg.liftoff_code()] = 0;
          ++next_spill_slot;
          executed_moves = 1;
        }
        register_moves.erase(register_moves.end() - executed_moves,
                             register_moves.end());
      }
    }

    for (RegisterLoad& rl : register_loads) {
      if (rl.is_constant_load) {
        asm_->LoadConstant(rl.dst, rl.constant);
      } else {
        asm_->Fill(rl.dst, rl.stack_slot);
      }
    }
    register_loads.clear();
  }

  void TransferStackSlot(const LiftoffAssembler::CacheState& dst_state,
                         uint32_t dst_index, uint32_t src_index) {
    const VarState& dst = dst_state.stack_state[dst_index];
    const VarState& src = __ cache_state()->stack_state[src_index];
    switch (dst.loc()) {
      case VarState::kStack:
        switch (src.loc()) {
          case VarState::kStack:
            if (src_index == dst_index) break;
            asm_->MoveStackValue(dst_index, src_index);
            break;
          case VarState::kRegister:
            asm_->Spill(dst_index, src.reg());
            break;
          case VarState::kI32Const:
            asm_->Spill(dst_index, WasmValue(src.i32_const()));
            break;
        }
        break;
      case VarState::kRegister:
        LoadIntoRegister(dst.reg(), src, src_index);
        break;
      case VarState::kI32Const:
        DCHECK_EQ(dst, src);
        break;
    }
  }

  void LoadIntoRegister(LiftoffRegister dst,
                        const LiftoffAssembler::VarState& src,
                        uint32_t src_index) {
    switch (src.loc()) {
      case VarState::kStack:
        LoadStackSlot(dst, src_index);
        break;
      case VarState::kRegister:
        DCHECK_EQ(dst.reg_class(), src.reg_class());
        if (dst != src.reg()) MoveRegister(dst, src.reg());
        break;
      case VarState::kI32Const:
        LoadConstant(dst, WasmValue(src.i32_const()));
        break;
    }
  }

  void MoveRegister(LiftoffRegister dst, LiftoffRegister src) {
    DCHECK_NE(dst, src);
    DCHECK(!move_dst_regs.has(dst));
    move_dst_regs.set(dst);
    move_src_regs.set(src);
    register_moves.emplace_back(dst, src);
  }

  void LoadConstant(LiftoffRegister dst, WasmValue value) {
    register_loads.emplace_back(dst, value);
  }

  void LoadStackSlot(LiftoffRegister dst, uint32_t stack_index) {
    register_loads.emplace_back(dst, stack_index);
  }

 private:
  // TODO(clemensh): Avoid unconditionally allocating on the heap.
  std::vector<RegisterMove> register_moves;
  std::vector<RegisterLoad> register_loads;
  LiftoffRegList move_dst_regs;
  LiftoffRegList move_src_regs;
  LiftoffAssembler* const asm_;
};

}  // namespace

// TODO(clemensh): Don't copy the full parent state (this makes us N^2).
void LiftoffAssembler::CacheState::InitMerge(const CacheState& source,
                                             uint32_t num_locals,
                                             uint32_t arity) {
  DCHECK(stack_state.empty());
  DCHECK_GE(source.stack_height(), stack_base);
  stack_state.resize(stack_base + arity, VarState(kWasmStmt));

  // |------locals------|--(in between)--|--(discarded)--|----merge----|
  //  <-- num_locals -->                 ^stack_base      <-- arity -->

  // First, initialize merge slots and locals. Keep them in the registers which
  // are being used in {source}, but avoid using a register multiple times. Use
  // unused registers where necessary and possible.
  for (int range = 0; range < 2; ++range) {
    auto src_idx = range ? 0 : source.stack_state.size() - arity;
    auto src_end = range ? num_locals : source.stack_state.size();
    auto dst_idx = range ? 0 : stack_state.size() - arity;
    for (; src_idx < src_end; ++src_idx, ++dst_idx) {
      auto& dst = stack_state[dst_idx];
      auto& src = source.stack_state[src_idx];
      // Just initialize to any register; will be overwritten before use.
      LiftoffRegister reg(Register::from_code<0>());
      RegClass rc = src.is_reg() ? src.reg_class() : reg_class_for(src.type());
      if (src.is_reg() && is_free(src.reg())) {
        reg = src.reg();
      } else if (has_unused_register(rc)) {
        reg = unused_register(rc);
      } else {
        // Make this a stack slot.
        dst = VarState(src.type());
        continue;
      }
      dst = VarState(src.type(), reg);
      inc_used(reg);
    }
  }
  // Last, initialize the section in between. Here, constants are allowed, but
  // registers which are already used for the merge region or locals must be
  // spilled.
  for (uint32_t i = num_locals; i < stack_base; ++i) {
    auto& dst = stack_state[i];
    auto& src = source.stack_state[i];
    if (src.is_reg()) {
      if (is_used(src.reg())) {
        // Make this a stack slot.
        dst = VarState(src.type());
      } else {
        dst = VarState(src.type(), src.reg());
        inc_used(src.reg());
      }
    } else if (src.is_const()) {
      dst = src;
    } else {
      DCHECK(src.is_stack());
      // Make this a stack slot.
      dst = VarState(src.type());
    }
  }
  last_spilled_regs = source.last_spilled_regs;
}

void LiftoffAssembler::CacheState::Steal(CacheState& source) {
  // Just use the move assignment operator.
  *this = std::move(source);
}

void LiftoffAssembler::CacheState::Split(const CacheState& source) {
  // Call the private copy assignment operator.
  *this = source;
}

// TODO(clemensh): Provide a reasonably sized buffer, based on wasm function
// size.
LiftoffAssembler::LiftoffAssembler(Isolate* isolate)
    : TurboAssembler(isolate, nullptr, 0, CodeObjectRequired::kYes) {}

LiftoffAssembler::~LiftoffAssembler() {
  if (num_locals_ > kInlineLocalTypes) {
    free(more_local_types_);
  }
}

LiftoffRegister LiftoffAssembler::GetBinaryOpTargetRegister(
    RegClass rc, LiftoffRegList pinned) {
  auto& slot_lhs = *(cache_state_.stack_state.end() - 2);
  if (slot_lhs.is_reg() && GetNumUses(slot_lhs.reg()) == 1) {
    DCHECK_EQ(rc, slot_lhs.reg().reg_class());
    return slot_lhs.reg();
  }
  auto& slot_rhs = *(cache_state_.stack_state.end() - 1);
  if (slot_rhs.is_reg() && GetNumUses(slot_rhs.reg()) == 1) {
    DCHECK_EQ(rc, slot_rhs.reg().reg_class());
    return slot_rhs.reg();
  }
  return GetUnusedRegister(rc, pinned);
}

LiftoffRegister LiftoffAssembler::GetUnaryOpTargetRegister(
    RegClass rc, LiftoffRegList pinned) {
  auto& slot_src = cache_state_.stack_state.back();
  if (slot_src.is_reg() && GetNumUses(slot_src.reg()) == 1) {
    DCHECK_EQ(rc, slot_src.reg().reg_class());
    return slot_src.reg();
  }
  return GetUnusedRegister(rc, pinned);
}

LiftoffRegister LiftoffAssembler::PopToRegister(RegClass rc,
                                                LiftoffRegList pinned) {
  DCHECK(!cache_state_.stack_state.empty());
  VarState slot = cache_state_.stack_state.back();
  cache_state_.stack_state.pop_back();
  switch (slot.loc()) {
    case VarState::kStack: {
      LiftoffRegister reg = GetUnusedRegister(rc, pinned);
      Fill(reg, cache_state_.stack_height());
      return reg;
    }
    case VarState::kRegister:
      DCHECK_EQ(rc, slot.reg_class());
      cache_state_.dec_used(slot.reg());
      return slot.reg();
    case VarState::kI32Const: {
      LiftoffRegister reg = GetUnusedRegister(rc, pinned);
      LoadConstant(reg, WasmValue(slot.i32_const()));
      return reg;
    }
  }
  UNREACHABLE();
}

void LiftoffAssembler::MergeFullStackWith(CacheState& target) {
  DCHECK_EQ(cache_state_.stack_height(), target.stack_height());
  // TODO(clemensh): Reuse the same StackTransferRecipe object to save some
  // allocations.
  StackTransferRecipe transfers(this);
  for (uint32_t i = 0, e = cache_state_.stack_height(); i < e; ++i) {
    transfers.TransferStackSlot(target, i, i);
  }
}

void LiftoffAssembler::MergeStackWith(CacheState& target, uint32_t arity) {
  // Before: ----------------|------ pop_count -----|--- arity ---|
  //                         ^target_stack_height   ^stack_base   ^stack_height
  // After:  ----|-- arity --|
  //             ^           ^target_stack_height
  //             ^target_stack_base
  uint32_t stack_height = cache_state_.stack_height();
  uint32_t target_stack_height = target.stack_height();
  uint32_t stack_base = stack_height - arity;
  uint32_t target_stack_base = target_stack_height - arity;
  StackTransferRecipe transfers(this);
  for (uint32_t i = 0; i < target_stack_base; ++i) {
    transfers.TransferStackSlot(target, i, i);
  }
  for (uint32_t i = 0; i < arity; ++i) {
    transfers.TransferStackSlot(target, target_stack_base + i, stack_base + i);
  }
}

void LiftoffAssembler::Spill(uint32_t index) {
  auto& slot = cache_state_.stack_state[index];
  switch (slot.loc()) {
    case VarState::kStack:
      return;
    case VarState::kRegister:
      Spill(index, slot.reg());
      cache_state_.dec_used(slot.reg());
      break;
    case VarState::kI32Const:
      Spill(index, WasmValue(slot.i32_const()));
      break;
  }
  slot.MakeStack();
}

void LiftoffAssembler::SpillLocals() {
  for (uint32_t i = 0; i < num_locals_; ++i) {
    Spill(i);
  }
}

void LiftoffAssembler::SpillAllRegisters() {
  for (uint32_t i = 0, e = cache_state_.stack_height(); i < e; ++i) {
    auto& slot = cache_state_.stack_state[i];
    if (!slot.is_reg()) continue;
    Spill(i, slot.reg());
    slot.MakeStack();
  }
  cache_state_.reset_used_registers();
}

void LiftoffAssembler::PrepareCall(wasm::FunctionSig* sig,
                                   compiler::CallDescriptor* call_desc) {
  uint32_t num_params = static_cast<uint32_t>(sig->parameter_count());
  // Parameter 0 is the wasm context.
  constexpr size_t kFirstActualParameter = 1;
  DCHECK_EQ(kFirstActualParameter + num_params, call_desc->ParameterCount());

  // Input 0 is the call target.
  constexpr size_t kInputShift = 1;

  // Spill all cache slots which are not being used as parameters.
  // Don't update any register use counters, they will be reset later anyway.
  for (uint32_t idx = 0, end = cache_state_.stack_height() - num_params;
       idx < end; ++idx) {
    VarState& slot = cache_state_.stack_state[idx];
    if (!slot.is_reg()) continue;
    Spill(idx, slot.reg());
    slot.MakeStack();
  }

  StackTransferRecipe stack_transfers(this);

  // Now move all parameter values into the right slot for the call.
  // Process parameters backward, such that we can just pop values from the
  // stack.
  for (uint32_t i = num_params; i > 0; --i) {
    uint32_t param = i - 1;
    ValueType type = sig->GetParam(param);
    RegClass rc = reg_class_for(type);
    compiler::LinkageLocation loc = call_desc->GetInputLocation(
        param + kFirstActualParameter + kInputShift);
    const VarState& slot = cache_state_.stack_state.back();
    uint32_t stack_idx = cache_state_.stack_height() - 1;
    if (loc.IsRegister()) {
      DCHECK(!loc.IsAnyRegister());
      int reg_code = loc.AsRegister();
      LiftoffRegister reg = LiftoffRegister::from_code(rc, reg_code);
      stack_transfers.LoadIntoRegister(reg, slot, stack_idx);
    } else {
      DCHECK(loc.IsCallerFrameSlot());
      PushCallerFrameSlot(slot, stack_idx);
    }
    cache_state_.stack_state.pop_back();
  }

  // Execute the stack transfers before filling the context register.
  stack_transfers.Execute();

  // Reset register use counters.
  cache_state_.reset_used_registers();

  // Fill the wasm context into the right register.
  compiler::LinkageLocation context_loc =
      call_desc->GetInputLocation(kInputShift);
  DCHECK(context_loc.IsRegister() && !context_loc.IsAnyRegister());
  int context_reg_code = context_loc.AsRegister();
  LiftoffRegister context_reg(Register::from_code(context_reg_code));
  FillContextInto(context_reg.gp());
}

void LiftoffAssembler::FinishCall(wasm::FunctionSig* sig,
                                  compiler::CallDescriptor* call_desc) {
  size_t return_count = call_desc->ReturnCount();
  DCHECK_EQ(return_count, sig->return_count());
  if (return_count != 0) {
    DCHECK_EQ(1, return_count);
    compiler::LinkageLocation return_loc = call_desc->GetReturnLocation(0);
    int return_reg_code = return_loc.AsRegister();
    ValueType return_type = sig->GetReturn(0);
    LiftoffRegister return_reg =
        LiftoffRegister::from_code(reg_class_for(return_type), return_reg_code);
    DCHECK(!cache_state_.is_used(return_reg));
    PushRegister(return_type, return_reg);
  }
}

LiftoffRegister LiftoffAssembler::SpillOneRegister(LiftoffRegList candidates,
                                                   LiftoffRegList pinned) {
  // Spill one cached value to free a register.
  LiftoffRegister spill_reg = cache_state_.GetNextSpillReg(candidates, pinned);
  SpillRegister(spill_reg);
  return spill_reg;
}

void LiftoffAssembler::SpillRegister(LiftoffRegister reg) {
  int remaining_uses = cache_state_.get_use_count(reg);
  DCHECK_LT(0, remaining_uses);
  for (uint32_t idx = cache_state_.stack_height() - 1;; --idx) {
    DCHECK_GT(cache_state_.stack_height(), idx);
    auto* slot = &cache_state_.stack_state[idx];
    if (!slot->is_reg() || slot->reg() != reg) continue;
    Spill(idx, reg);
    slot->MakeStack();
    if (--remaining_uses == 0) break;
  }
  cache_state_.clear_used(reg);
}

void LiftoffAssembler::set_num_locals(uint32_t num_locals) {
  DCHECK_EQ(0, num_locals_);  // only call this once.
  num_locals_ = num_locals;
  if (num_locals > kInlineLocalTypes) {
    more_local_types_ =
        reinterpret_cast<ValueType*>(malloc(num_locals * sizeof(ValueType)));
    DCHECK_NOT_NULL(more_local_types_);
  }
}

uint32_t LiftoffAssembler::GetTotalFrameSlotCount() const {
  return num_locals() + kMaxValueStackHeight;
}

std::ostream& operator<<(std::ostream& os, VarState slot) {
  os << WasmOpcodes::TypeName(slot.type()) << ":";
  switch (slot.loc()) {
    case VarState::kStack:
      return os << "s";
    case VarState::kRegister:
      return os << slot.reg();
    case VarState::kI32Const:
      return os << "c" << slot.i32_const();
  }
  UNREACHABLE();
}

#undef __
#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
