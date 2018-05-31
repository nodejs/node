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
    ValueType type;
    constexpr RegisterMove(LiftoffRegister dst, LiftoffRegister src,
                           ValueType type)
        : dst(dst), src(src), type(type) {}
  };
  struct RegisterLoad {
    enum LoadKind : uint8_t {
      kConstant,  // load a constant value into a register.
      kStack,     // fill a register from a stack slot.
      kHalfStack  // fill one half of a register pair from half a stack slot.
    };

    LiftoffRegister dst;
    LoadKind kind;
    ValueType type;
    int32_t value;  // i32 constant value or stack index, depending on kind.

    // Named constructors.
    static RegisterLoad Const(LiftoffRegister dst, WasmValue constant) {
      if (constant.type() == kWasmI32) {
        return {dst, kConstant, kWasmI32, constant.to_i32()};
      }
      DCHECK_EQ(kWasmI64, constant.type());
      DCHECK_EQ(constant.to_i32_unchecked(), constant.to_i64_unchecked());
      return {dst, kConstant, kWasmI64, constant.to_i32_unchecked()};
    }
    static RegisterLoad Stack(LiftoffRegister dst, int32_t stack_index,
                              ValueType type) {
      return {dst, kStack, type, stack_index};
    }
    static RegisterLoad HalfStack(LiftoffRegister dst,
                                  int32_t half_stack_index) {
      return {dst, kHalfStack, kWasmI32, half_stack_index};
    }

   private:
    RegisterLoad(LiftoffRegister dst, LoadKind kind, ValueType type,
                 int32_t value)
        : dst(dst), kind(kind), type(type), value(value) {}
  };

 public:
  explicit StackTransferRecipe(LiftoffAssembler* wasm_asm) : asm_(wasm_asm) {}
  ~StackTransferRecipe() { Execute(); }

  void Execute() {
    // First, execute register moves. Then load constants and stack values into
    // registers.

    if ((move_dst_regs_ & move_src_regs_).is_empty()) {
      // No overlap in src and dst registers. Just execute the moves in any
      // order.
      for (RegisterMove& rm : register_moves_) {
        asm_->Move(rm.dst, rm.src, rm.type);
      }
      register_moves_.clear();
    } else {
      // Keep use counters of src registers.
      uint32_t src_reg_use_count[kAfterMaxLiftoffRegCode] = {0};
      for (RegisterMove& rm : register_moves_) {
        ++src_reg_use_count[rm.src.liftoff_code()];
      }
      // Now repeatedly iterate the list of register moves, and execute those
      // whose dst register does not appear as src any more. The remaining moves
      // are compacted during this iteration.
      // If no more moves can be executed (because of a cycle), spill one
      // register to the stack, add a RegisterLoad to reload it later, and
      // continue.
      uint32_t next_spill_slot = asm_->cache_state()->stack_height();
      while (!register_moves_.empty()) {
        int executed_moves = 0;
        for (auto& rm : register_moves_) {
          if (src_reg_use_count[rm.dst.liftoff_code()] == 0) {
            asm_->Move(rm.dst, rm.src, rm.type);
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
          RegisterMove& rm = register_moves_.back();
          LiftoffRegister spill_reg = rm.src;
          asm_->Spill(next_spill_slot, spill_reg, rm.type);
          // Remember to reload into the destination register later.
          LoadStackSlot(register_moves_.back().dst, next_spill_slot, rm.type);
          DCHECK_EQ(1, src_reg_use_count[spill_reg.liftoff_code()]);
          src_reg_use_count[spill_reg.liftoff_code()] = 0;
          ++next_spill_slot;
          executed_moves = 1;
        }
        register_moves_.erase(register_moves_.end() - executed_moves,
                              register_moves_.end());
      }
    }

    for (RegisterLoad& rl : register_loads_) {
      switch (rl.kind) {
        case RegisterLoad::kConstant:
          asm_->LoadConstant(rl.dst, rl.type == kWasmI64
                                         ? WasmValue(int64_t{rl.value})
                                         : WasmValue(int32_t{rl.value}));
          break;
        case RegisterLoad::kStack:
          asm_->Fill(rl.dst, rl.value, rl.type);
          break;
        case RegisterLoad::kHalfStack:
          // As half of a register pair, {rl.dst} must be a gp register.
          asm_->FillI64Half(rl.dst.gp(), rl.value);
          break;
      }
    }
    register_loads_.clear();
  }

  void TransferStackSlot(const LiftoffAssembler::CacheState& dst_state,
                         uint32_t dst_index, uint32_t src_index) {
    const VarState& dst = dst_state.stack_state[dst_index];
    const VarState& src = __ cache_state()->stack_state[src_index];
    DCHECK_EQ(dst.type(), src.type());
    switch (dst.loc()) {
      case VarState::kStack:
        switch (src.loc()) {
          case VarState::kStack:
            if (src_index == dst_index) break;
            asm_->MoveStackValue(dst_index, src_index, src.type());
            break;
          case VarState::kRegister:
            asm_->Spill(dst_index, src.reg(), src.type());
            break;
          case VarState::KIntConst:
            asm_->Spill(dst_index, src.constant());
            break;
        }
        break;
      case VarState::kRegister:
        LoadIntoRegister(dst.reg(), src, src_index);
        break;
      case VarState::KIntConst:
        DCHECK_EQ(dst, src);
        break;
    }
  }

  void LoadIntoRegister(LiftoffRegister dst,
                        const LiftoffAssembler::VarState& src,
                        uint32_t src_index) {
    switch (src.loc()) {
      case VarState::kStack:
        LoadStackSlot(dst, src_index, src.type());
        break;
      case VarState::kRegister:
        DCHECK_EQ(dst.reg_class(), src.reg_class());
        if (dst != src.reg()) MoveRegister(dst, src.reg(), src.type());
        break;
      case VarState::KIntConst:
        LoadConstant(dst, src.constant());
        break;
    }
  }

  void LoadI64HalfIntoRegister(LiftoffRegister dst,
                               const LiftoffAssembler::VarState& src,
                               uint32_t index, RegPairHalf half) {
    // Use CHECK such that the remaining code is statically dead if
    // {kNeedI64RegPair} is false.
    CHECK(kNeedI64RegPair);
    DCHECK_EQ(kWasmI64, src.type());
    switch (src.loc()) {
      case VarState::kStack:
        LoadI64HalfStackSlot(dst, 2 * index - (half == kLowWord ? 0 : 1));
        break;
      case VarState::kRegister: {
        LiftoffRegister src_half =
            half == kLowWord ? src.reg().low() : src.reg().high();
        if (dst != src_half) MoveRegister(dst, src_half, kWasmI32);
        break;
      }
      case VarState::KIntConst:
        int32_t value = src.i32_const();
        // The high word is the sign extension of the low word.
        if (half == kHighWord) value = value >> 31;
        LoadConstant(dst, WasmValue(value));
        break;
    }
  }

  void MoveRegister(LiftoffRegister dst, LiftoffRegister src, ValueType type) {
    DCHECK_NE(dst, src);
    DCHECK_EQ(dst.reg_class(), src.reg_class());
    DCHECK_EQ(reg_class_for(type), src.reg_class());
    if (src.is_pair()) {
      DCHECK_EQ(kWasmI64, type);
      if (dst.low() != src.low()) MoveRegister(dst.low(), src.low(), kWasmI32);
      if (dst.high() != src.high())
        MoveRegister(dst.high(), src.high(), kWasmI32);
      return;
    }
    DCHECK(!move_dst_regs_.has(dst));
    move_dst_regs_.set(dst);
    move_src_regs_.set(src);
    register_moves_.emplace_back(dst, src, type);
  }

  void LoadConstant(LiftoffRegister dst, WasmValue value) {
    register_loads_.push_back(RegisterLoad::Const(dst, value));
  }

  void LoadStackSlot(LiftoffRegister dst, uint32_t stack_index,
                     ValueType type) {
    register_loads_.push_back(RegisterLoad::Stack(dst, stack_index, type));
  }

  void LoadI64HalfStackSlot(LiftoffRegister dst, uint32_t half_stack_index) {
    register_loads_.push_back(RegisterLoad::HalfStack(dst, half_stack_index));
  }

 private:
  // TODO(clemensh): Avoid unconditionally allocating on the heap.
  std::vector<RegisterMove> register_moves_;
  std::vector<RegisterLoad> register_loads_;
  LiftoffRegList move_dst_regs_;
  LiftoffRegList move_src_regs_;
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
    : TurboAssembler(isolate, nullptr, 0, CodeObjectRequired::kNo) {}

LiftoffAssembler::~LiftoffAssembler() {
  if (num_locals_ > kInlineLocalTypes) {
    free(more_local_types_);
  }
}

LiftoffRegister LiftoffAssembler::PopToRegister(LiftoffRegList pinned) {
  DCHECK(!cache_state_.stack_state.empty());
  VarState slot = cache_state_.stack_state.back();
  cache_state_.stack_state.pop_back();
  switch (slot.loc()) {
    case VarState::kStack: {
      LiftoffRegister reg =
          GetUnusedRegister(reg_class_for(slot.type()), pinned);
      Fill(reg, cache_state_.stack_height(), slot.type());
      return reg;
    }
    case VarState::kRegister:
      cache_state_.dec_used(slot.reg());
      return slot.reg();
    case VarState::KIntConst: {
      RegClass rc =
          kNeedI64RegPair && slot.type() == kWasmI64 ? kGpRegPair : kGpReg;
      LiftoffRegister reg = GetUnusedRegister(rc, pinned);
      LoadConstant(reg, slot.constant());
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
  DCHECK_LE(target_stack_height, stack_height);
  DCHECK_LE(arity, target_stack_height);
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
      Spill(index, slot.reg(), slot.type());
      cache_state_.dec_used(slot.reg());
      break;
    case VarState::KIntConst:
      Spill(index, slot.constant());
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
    Spill(i, slot.reg(), slot.type());
    slot.MakeStack();
  }
  cache_state_.reset_used_registers();
}

void LiftoffAssembler::PrepareCall(wasm::FunctionSig* sig,
                                   compiler::CallDescriptor* call_descriptor,
                                   Register* target,
                                   LiftoffRegister* target_instance) {
  uint32_t num_params = static_cast<uint32_t>(sig->parameter_count());
  // Input 0 is the call target.
  constexpr size_t kInputShift = 1;

  // Spill all cache slots which are not being used as parameters.
  // Don't update any register use counters, they will be reset later anyway.
  for (uint32_t idx = 0, end = cache_state_.stack_height() - num_params;
       idx < end; ++idx) {
    VarState& slot = cache_state_.stack_state[idx];
    if (!slot.is_reg()) continue;
    Spill(idx, slot.reg(), slot.type());
    slot.MakeStack();
  }

  StackTransferRecipe stack_transfers(this);
  LiftoffRegList param_regs;

  // Move the target instance (if supplied) into the correct instance register.
  compiler::LinkageLocation instance_loc =
      call_descriptor->GetInputLocation(kInputShift);
  DCHECK(instance_loc.IsRegister() && !instance_loc.IsAnyRegister());
  LiftoffRegister instance_reg(Register::from_code(instance_loc.AsRegister()));
  param_regs.set(instance_reg);
  if (target_instance && *target_instance != instance_reg) {
    stack_transfers.MoveRegister(instance_reg, *target_instance, kWasmIntPtr);
  }

  // Now move all parameter values into the right slot for the call.
  // Don't pop values yet, such that the stack height is still correct when
  // executing the {stack_transfers}.
  // Process parameters backwards, such that pushes of caller frame slots are
  // in the correct order.
  uint32_t param_base = cache_state_.stack_height() - num_params;
  uint32_t call_desc_input_idx =
      static_cast<uint32_t>(call_descriptor->InputCount());
  for (uint32_t i = num_params; i > 0; --i) {
    const uint32_t param = i - 1;
    ValueType type = sig->GetParam(param);
    const bool is_pair = kNeedI64RegPair && type == kWasmI64;
    const int num_lowered_params = is_pair ? 2 : 1;
    const uint32_t stack_idx = param_base + param;
    const VarState& slot = cache_state_.stack_state[stack_idx];
    // Process both halfs of register pair separately, because they are passed
    // as separate parameters. One or both of them could end up on the stack.
    for (int lowered_idx = 0; lowered_idx < num_lowered_params; ++lowered_idx) {
      const RegPairHalf half =
          is_pair && lowered_idx == 0 ? kHighWord : kLowWord;
      --call_desc_input_idx;
      compiler::LinkageLocation loc =
          call_descriptor->GetInputLocation(call_desc_input_idx);
      if (loc.IsRegister()) {
        DCHECK(!loc.IsAnyRegister());
        RegClass rc = is_pair ? kGpReg : reg_class_for(type);
        LiftoffRegister reg = LiftoffRegister::from_code(rc, loc.AsRegister());
        param_regs.set(reg);
        if (is_pair) {
          stack_transfers.LoadI64HalfIntoRegister(reg, slot, stack_idx, half);
        } else {
          stack_transfers.LoadIntoRegister(reg, slot, stack_idx);
        }
      } else {
        DCHECK(loc.IsCallerFrameSlot());
        PushCallerFrameSlot(slot, stack_idx, half);
      }
    }
  }
  // {call_desc_input_idx} should point after the instance parameter now.
  DCHECK_EQ(call_desc_input_idx, kInputShift + 1);

  // If the target register overlaps with a parameter register, then move the
  // target to another free register, or spill to the stack.
  if (target && param_regs.has(LiftoffRegister(*target))) {
    // Try to find another free register.
    LiftoffRegList free_regs = kGpCacheRegList.MaskOut(param_regs);
    if (!free_regs.is_empty()) {
      LiftoffRegister new_target = free_regs.GetFirstRegSet();
      stack_transfers.MoveRegister(new_target, LiftoffRegister(*target),
                                   kWasmIntPtr);
      *target = new_target.gp();
    } else {
      PushCallerFrameSlot(LiftoffRegister(*target), kWasmIntPtr);
      *target = no_reg;
    }
  }

  // Execute the stack transfers before filling the instance register.
  stack_transfers.Execute();

  // Pop parameters from the value stack.
  auto stack_end = cache_state_.stack_state.end();
  cache_state_.stack_state.erase(stack_end - num_params, stack_end);

  // Reset register use counters.
  cache_state_.reset_used_registers();

  // Reload the instance from the stack.
  if (!target_instance) {
    FillInstanceInto(instance_reg.gp());
  }
}

void LiftoffAssembler::FinishCall(wasm::FunctionSig* sig,
                                  compiler::CallDescriptor* call_descriptor) {
  const size_t return_count = sig->return_count();
  if (return_count != 0) {
    DCHECK_EQ(1, return_count);
    ValueType return_type = sig->GetReturn(0);
    const bool need_pair = kNeedI64RegPair && return_type == kWasmI64;
    DCHECK_EQ(need_pair ? 2 : 1, call_descriptor->ReturnCount());
    RegClass rc = need_pair ? kGpReg : reg_class_for(return_type);
    LiftoffRegister return_reg = LiftoffRegister::from_code(
        rc, call_descriptor->GetReturnLocation(0).AsRegister());
    DCHECK(GetCacheRegList(rc).has(return_reg));
    if (need_pair) {
      LiftoffRegister high_reg = LiftoffRegister::from_code(
          rc, call_descriptor->GetReturnLocation(1).AsRegister());
      DCHECK(GetCacheRegList(rc).has(high_reg));
      return_reg = LiftoffRegister::ForPair(return_reg.gp(), high_reg.gp());
    }
    DCHECK(!cache_state_.is_used(return_reg));
    PushRegister(return_type, return_reg);
  }
}

void LiftoffAssembler::Move(LiftoffRegister dst, LiftoffRegister src,
                            ValueType type) {
  DCHECK_EQ(dst.reg_class(), src.reg_class());
  DCHECK_NE(dst, src);
  if (kNeedI64RegPair && dst.is_pair()) {
    // Use the {StackTransferRecipe} to move pairs, as the registers in the
    // pairs might overlap.
    StackTransferRecipe(this).MoveRegister(dst, src, type);
  } else if (dst.is_gp()) {
    Move(dst.gp(), src.gp(), type);
  } else {
    Move(dst.fp(), src.fp(), type);
  }
}

void LiftoffAssembler::ParallelRegisterMove(
    std::initializer_list<ParallelRegisterMoveTuple> tuples) {
  StackTransferRecipe stack_transfers(this);
  for (auto tuple : tuples) {
    if (tuple.dst == tuple.src) continue;
    stack_transfers.MoveRegister(tuple.dst, tuple.src, tuple.type);
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
    if (!slot->is_reg() || !slot->reg().overlaps(reg)) continue;
    if (slot->reg().is_pair()) {
      // Make sure to decrement *both* registers in a pair, because the
      // {clear_used} call below only clears one of them.
      cache_state_.dec_used(slot->reg().low());
      cache_state_.dec_used(slot->reg().high());
    }
    Spill(idx, slot->reg(), slot->type());
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

std::ostream& operator<<(std::ostream& os, VarState slot) {
  os << WasmOpcodes::TypeName(slot.type()) << ":";
  switch (slot.loc()) {
    case VarState::kStack:
      return os << "s";
    case VarState::kRegister:
      return os << slot.reg();
    case VarState::KIntConst:
      return os << "c" << slot.i32_const();
  }
  UNREACHABLE();
}

#undef __
#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8
