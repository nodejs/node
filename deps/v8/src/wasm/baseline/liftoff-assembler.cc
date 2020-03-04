// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-assembler.h"

#include <sstream>

#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/utils/ostreams.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

using VarState = LiftoffAssembler::VarState;

namespace {

class StackTransferRecipe {
  struct RegisterMove {
    LiftoffRegister src;
    ValueType type;
    constexpr RegisterMove(LiftoffRegister src, ValueType type)
        : src(src), type(type) {}
  };

  struct RegisterLoad {
    enum LoadKind : uint8_t {
      kConstant,      // load a constant value into a register.
      kStack,         // fill a register from a stack slot.
      kLowHalfStack,  // fill a register from the low half of a stack slot.
      kHighHalfStack  // fill a register from the high half of a stack slot.
    };

    LoadKind kind;
    ValueType type;
    int32_t value;  // i32 constant value or stack offset, depending on kind.

    // Named constructors.
    static RegisterLoad Const(WasmValue constant) {
      if (constant.type() == kWasmI32) {
        return {kConstant, kWasmI32, constant.to_i32()};
      }
      DCHECK_EQ(kWasmI64, constant.type());
      DCHECK_EQ(constant.to_i32_unchecked(), constant.to_i64_unchecked());
      return {kConstant, kWasmI64, constant.to_i32_unchecked()};
    }
    static RegisterLoad Stack(int32_t offset, ValueType type) {
      return {kStack, type, offset};
    }
    static RegisterLoad HalfStack(int32_t offset, RegPairHalf half) {
      return {half == kLowWord ? kLowHalfStack : kHighHalfStack, kWasmI32,
              offset};
    }

   private:
    RegisterLoad(LoadKind kind, ValueType type, int32_t value)
        : kind(kind), type(type), value(value) {}
  };

 public:
  explicit StackTransferRecipe(LiftoffAssembler* wasm_asm) : asm_(wasm_asm) {}
  ~StackTransferRecipe() { Execute(); }

  void Execute() {
    // First, execute register moves. Then load constants and stack values into
    // registers.
    ExecuteMoves();
    DCHECK(move_dst_regs_.is_empty());
    ExecuteLoads();
    DCHECK(load_dst_regs_.is_empty());
  }

  void TransferStackSlot(const VarState& dst, const VarState& src) {
    DCHECK_EQ(dst.type(), src.type());
    switch (dst.loc()) {
      case VarState::kStack:
        switch (src.loc()) {
          case VarState::kStack:
            if (src.offset() == dst.offset()) break;
            asm_->MoveStackValue(dst.offset(), src.offset(), src.type());
            break;
          case VarState::kRegister:
            asm_->Spill(dst.offset(), src.reg(), src.type());
            break;
          case VarState::kIntConst:
            asm_->Spill(dst.offset(), src.constant());
            break;
        }
        break;
      case VarState::kRegister:
        LoadIntoRegister(dst.reg(), src, src.offset());
        break;
      case VarState::kIntConst:
        DCHECK_EQ(dst, src);
        break;
    }
  }

  void LoadIntoRegister(LiftoffRegister dst,
                        const LiftoffAssembler::VarState& src,
                        uint32_t src_offset) {
    switch (src.loc()) {
      case VarState::kStack:
        LoadStackSlot(dst, src_offset, src.type());
        break;
      case VarState::kRegister:
        DCHECK_EQ(dst.reg_class(), src.reg_class());
        if (dst != src.reg()) MoveRegister(dst, src.reg(), src.type());
        break;
      case VarState::kIntConst:
        LoadConstant(dst, src.constant());
        break;
    }
  }

  void LoadI64HalfIntoRegister(LiftoffRegister dst,
                               const LiftoffAssembler::VarState& src,
                               uint32_t offset, RegPairHalf half) {
    // Use CHECK such that the remaining code is statically dead if
    // {kNeedI64RegPair} is false.
    CHECK(kNeedI64RegPair);
    DCHECK_EQ(kWasmI64, src.type());
    switch (src.loc()) {
      case VarState::kStack:
        LoadI64HalfStackSlot(dst, offset, half);
        break;
      case VarState::kRegister: {
        LiftoffRegister src_half =
            half == kLowWord ? src.reg().low() : src.reg().high();
        if (dst != src_half) MoveRegister(dst, src_half, kWasmI32);
        break;
      }
      case VarState::kIntConst:
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
    if (move_dst_regs_.has(dst)) {
      DCHECK_EQ(register_move(dst)->src, src);
      // Non-fp registers can only occur with the exact same type.
      DCHECK_IMPLIES(!dst.is_fp(), register_move(dst)->type == type);
      // It can happen that one fp register holds both the f32 zero and the f64
      // zero, as the initial value for local variables. Move the value as f64
      // in that case.
      if (type == kWasmF64) register_move(dst)->type = kWasmF64;
      return;
    }
    move_dst_regs_.set(dst);
    ++*src_reg_use_count(src);
    *register_move(dst) = {src, type};
  }

  void LoadConstant(LiftoffRegister dst, WasmValue value) {
    DCHECK(!load_dst_regs_.has(dst));
    load_dst_regs_.set(dst);
    if (dst.is_pair()) {
      DCHECK_EQ(kWasmI64, value.type());
      int64_t i64 = value.to_i64();
      *register_load(dst.low()) =
          RegisterLoad::Const(WasmValue(static_cast<int32_t>(i64)));
      *register_load(dst.high()) =
          RegisterLoad::Const(WasmValue(static_cast<int32_t>(i64 >> 32)));
    } else {
      *register_load(dst) = RegisterLoad::Const(value);
    }
  }

  void LoadStackSlot(LiftoffRegister dst, uint32_t stack_offset,
                     ValueType type) {
    if (load_dst_regs_.has(dst)) {
      // It can happen that we spilled the same register to different stack
      // slots, and then we reload them later into the same dst register.
      // In that case, it is enough to load one of the stack slots.
      return;
    }
    load_dst_regs_.set(dst);
    if (dst.is_pair()) {
      DCHECK_EQ(kWasmI64, type);
      *register_load(dst.low()) =
          RegisterLoad::HalfStack(stack_offset, kLowWord);
      *register_load(dst.high()) =
          RegisterLoad::HalfStack(stack_offset, kHighWord);
    } else {
      *register_load(dst) = RegisterLoad::Stack(stack_offset, type);
    }
  }

  void LoadI64HalfStackSlot(LiftoffRegister dst, uint32_t offset,
                            RegPairHalf half) {
    if (load_dst_regs_.has(dst)) {
      // It can happen that we spilled the same register to different stack
      // slots, and then we reload them later into the same dst register.
      // In that case, it is enough to load one of the stack slots.
      return;
    }
    load_dst_regs_.set(dst);
    *register_load(dst) = RegisterLoad::HalfStack(offset, half);
  }

 private:
  using MovesStorage =
      std::aligned_storage<kAfterMaxLiftoffRegCode * sizeof(RegisterMove),
                           alignof(RegisterMove)>::type;
  using LoadsStorage =
      std::aligned_storage<kAfterMaxLiftoffRegCode * sizeof(RegisterLoad),
                           alignof(RegisterLoad)>::type;

  ASSERT_TRIVIALLY_COPYABLE(RegisterMove);
  ASSERT_TRIVIALLY_COPYABLE(RegisterLoad);

  MovesStorage register_moves_;  // uninitialized
  LoadsStorage register_loads_;  // uninitialized
  int src_reg_use_count_[kAfterMaxLiftoffRegCode] = {0};
  LiftoffRegList move_dst_regs_;
  LiftoffRegList load_dst_regs_;
  LiftoffAssembler* const asm_;

  RegisterMove* register_move(LiftoffRegister reg) {
    return reinterpret_cast<RegisterMove*>(&register_moves_) +
           reg.liftoff_code();
  }
  RegisterLoad* register_load(LiftoffRegister reg) {
    return reinterpret_cast<RegisterLoad*>(&register_loads_) +
           reg.liftoff_code();
  }
  int* src_reg_use_count(LiftoffRegister reg) {
    return src_reg_use_count_ + reg.liftoff_code();
  }

  void ExecuteMove(LiftoffRegister dst) {
    RegisterMove* move = register_move(dst);
    DCHECK_EQ(0, *src_reg_use_count(dst));
    asm_->Move(dst, move->src, move->type);
    ClearExecutedMove(dst);
  }

  void ClearExecutedMove(LiftoffRegister dst) {
    DCHECK(move_dst_regs_.has(dst));
    move_dst_regs_.clear(dst);
    RegisterMove* move = register_move(dst);
    DCHECK_LT(0, *src_reg_use_count(move->src));
    if (--*src_reg_use_count(move->src)) return;
    // src count dropped to zero. If this is a destination register, execute
    // that move now.
    if (!move_dst_regs_.has(move->src)) return;
    ExecuteMove(move->src);
  }

  void ExecuteMoves() {
    // Execute all moves whose {dst} is not being used as src in another move.
    // If any src count drops to zero, also (transitively) execute the
    // corresponding move to that register.
    for (LiftoffRegister dst : move_dst_regs_) {
      // Check if already handled via transitivity in {ClearExecutedMove}.
      if (!move_dst_regs_.has(dst)) continue;
      if (*src_reg_use_count(dst)) continue;
      ExecuteMove(dst);
    }

    // All remaining moves are parts of a cycle. Just spill the first one, then
    // process all remaining moves in that cycle. Repeat for all cycles.
    uint32_t last_spill_offset =
        (asm_->cache_state()->stack_state.empty()
             ? 0
             : asm_->cache_state()->stack_state.back().offset());
    while (!move_dst_regs_.is_empty()) {
      // TODO(clemensb): Use an unused register if available.
      LiftoffRegister dst = move_dst_regs_.GetFirstRegSet();
      RegisterMove* move = register_move(dst);
      last_spill_offset += LiftoffAssembler::SlotSizeForType(move->type);
      LiftoffRegister spill_reg = move->src;
      asm_->Spill(last_spill_offset, spill_reg, move->type);
      // Remember to reload into the destination register later.
      LoadStackSlot(dst, last_spill_offset, move->type);
      ClearExecutedMove(dst);
    }
  }

  void ExecuteLoads() {
    for (LiftoffRegister dst : load_dst_regs_) {
      RegisterLoad* load = register_load(dst);
      switch (load->kind) {
        case RegisterLoad::kConstant:
          asm_->LoadConstant(dst, load->type == kWasmI64
                                      ? WasmValue(int64_t{load->value})
                                      : WasmValue(int32_t{load->value}));
          break;
        case RegisterLoad::kStack:
          asm_->Fill(dst, load->value, load->type);
          break;
        case RegisterLoad::kLowHalfStack:
          // Half of a register pair, {dst} must be a gp register.
          asm_->FillI64Half(dst.gp(), load->value, kLowWord);
          break;
        case RegisterLoad::kHighHalfStack:
          // Half of a register pair, {dst} must be a gp register.
          asm_->FillI64Half(dst.gp(), load->value, kHighWord);
          break;
      }
    }
    load_dst_regs_ = {};
  }

  DISALLOW_COPY_AND_ASSIGN(StackTransferRecipe);
};

class RegisterReuseMap {
 public:
  void Add(LiftoffRegister src, LiftoffRegister dst) {
    if (auto previous = Lookup(src)) {
      DCHECK_EQ(previous, dst);
      return;
    }
    map_.emplace_back(src);
    map_.emplace_back(dst);
  }

  base::Optional<LiftoffRegister> Lookup(LiftoffRegister src) {
    for (auto it = map_.begin(), end = map_.end(); it != end; it += 2) {
      if (it->is_pair() == src.is_pair() && *it == src) return *(it + 1);
    }
    return {};
  }

 private:
  // {map_} holds pairs of <src, dst>.
  base::SmallVector<LiftoffRegister, 8> map_;
};

enum MergeKeepStackSlots : bool {
  kKeepStackSlots = true,
  kTurnStackSlotsIntoRegisters = false
};
enum MergeAllowConstants : bool {
  kConstantsAllowed = true,
  kConstantsNotAllowed = false
};
enum ReuseRegisters : bool {
  kReuseRegisters = true,
  kNoReuseRegisters = false
};
void InitMergeRegion(LiftoffAssembler::CacheState* state,
                     const VarState* source, VarState* target, uint32_t count,
                     MergeKeepStackSlots keep_stack_slots,
                     MergeAllowConstants allow_constants,
                     ReuseRegisters reuse_registers, LiftoffRegList used_regs) {
  RegisterReuseMap register_reuse_map;
  for (const VarState* source_end = source + count; source < source_end;
       ++source, ++target) {
    if ((source->is_stack() && keep_stack_slots) ||
        (source->is_const() && allow_constants)) {
      *target = *source;
      continue;
    }
    base::Optional<LiftoffRegister> reg;
    // First try: Keep the same register, if it's free.
    if (source->is_reg() && state->is_free(source->reg())) {
      reg = source->reg();
    }
    // Second try: Use the same register we used before (if we reuse registers).
    if (!reg && reuse_registers) {
      reg = register_reuse_map.Lookup(source->reg());
    }
    // Third try: Use any free register.
    RegClass rc = reg_class_for(source->type());
    if (!reg && state->has_unused_register(rc, used_regs)) {
      reg = state->unused_register(rc, used_regs);
    }
    if (!reg) {
      // No free register; make this a stack slot.
      *target = VarState(source->type(), source->offset());
      continue;
    }
    if (reuse_registers) register_reuse_map.Add(source->reg(), *reg);
    state->inc_used(*reg);
    *target = VarState(source->type(), *reg, source->offset());
  }
}

}  // namespace

// TODO(clemensb): Don't copy the full parent state (this makes us N^2).
void LiftoffAssembler::CacheState::InitMerge(const CacheState& source,
                                             uint32_t num_locals,
                                             uint32_t arity,
                                             uint32_t stack_depth) {
  // |------locals------|---(in between)----|--(discarded)--|----merge----|
  //  <-- num_locals --> <-- stack_depth -->^stack_base      <-- arity -->

  uint32_t stack_base = stack_depth + num_locals;
  uint32_t target_height = stack_base + arity;
  uint32_t discarded = source.stack_height() - target_height;
  DCHECK(stack_state.empty());

  DCHECK_GE(source.stack_height(), stack_base);
  stack_state.resize_no_init(target_height);

  const VarState* source_begin = source.stack_state.data();
  VarState* target_begin = stack_state.data();

  // Try to keep locals and the merge region in their registers. Register used
  // multiple times need to be copied to another free register. Compute the list
  // of used registers.
  LiftoffRegList used_regs;
  for (auto& src : VectorOf(source_begin, num_locals)) {
    if (src.is_reg()) used_regs.set(src.reg());
  }
  for (auto& src : VectorOf(source_begin + stack_base + discarded, arity)) {
    if (src.is_reg()) used_regs.set(src.reg());
  }

  // Initialize the merge region. If this region moves, try to turn stack slots
  // into registers since we need to load the value anyways.
  MergeKeepStackSlots keep_merge_stack_slots =
      discarded == 0 ? kKeepStackSlots : kTurnStackSlotsIntoRegisters;
  InitMergeRegion(this, source_begin + stack_base + discarded,
                  target_begin + stack_base, arity, keep_merge_stack_slots,
                  kConstantsNotAllowed, kNoReuseRegisters, used_regs);

  // Initialize the locals region. Here, stack slots stay stack slots (because
  // they do not move). Try to keep register in registers, but avoid duplicates.
  InitMergeRegion(this, source_begin, target_begin, num_locals, kKeepStackSlots,
                  kConstantsNotAllowed, kNoReuseRegisters, used_regs);
  // Sanity check: All the {used_regs} are really in use now.
  DCHECK_EQ(used_regs, used_registers & used_regs);

  // Last, initialize the section in between. Here, constants are allowed, but
  // registers which are already used for the merge region or locals must be
  // moved to other registers or spilled. If a register appears twice in the
  // source region, ensure to use the same register twice in the target region.
  InitMergeRegion(this, source_begin + num_locals, target_begin + num_locals,
                  stack_depth, kKeepStackSlots, kConstantsAllowed,
                  kReuseRegisters, used_regs);
}

void LiftoffAssembler::CacheState::Steal(const CacheState& source) {
  // Just use the move assignment operator.
  *this = std::move(source);
}

void LiftoffAssembler::CacheState::Split(const CacheState& source) {
  // Call the private copy assignment operator.
  *this = source;
}

namespace {

constexpr AssemblerOptions DefaultLiftoffOptions() {
  return AssemblerOptions{};
}

}  // namespace

LiftoffAssembler::LiftoffAssembler(std::unique_ptr<AssemblerBuffer> buffer)
    : TurboAssembler(nullptr, DefaultLiftoffOptions(), CodeObjectRequired::kNo,
                     std::move(buffer)) {
  set_abort_hard(true);  // Avoid calls to Abort.
}

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
      Fill(reg, slot.offset(), slot.type());
      return reg;
    }
    case VarState::kRegister:
      cache_state_.dec_used(slot.reg());
      return slot.reg();
    case VarState::kIntConst: {
      RegClass rc =
          kNeedI64RegPair && slot.type() == kWasmI64 ? kGpRegPair : kGpReg;
      LiftoffRegister reg = GetUnusedRegister(rc, pinned);
      LoadConstant(reg, slot.constant());
      return reg;
    }
  }
  UNREACHABLE();
}

void LiftoffAssembler::MergeFullStackWith(const CacheState& target,
                                          const CacheState& source) {
  DCHECK_EQ(source.stack_height(), target.stack_height());
  // TODO(clemensb): Reuse the same StackTransferRecipe object to save some
  // allocations.
  StackTransferRecipe transfers(this);
  for (uint32_t i = 0, e = source.stack_height(); i < e; ++i) {
    transfers.TransferStackSlot(target.stack_state[i], source.stack_state[i]);
  }
}

void LiftoffAssembler::MergeStackWith(const CacheState& target,
                                      uint32_t arity) {
  // Before: ----------------|----- (discarded) ----|--- arity ---|
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
    transfers.TransferStackSlot(target.stack_state[i],
                                cache_state_.stack_state[i]);
  }
  for (uint32_t i = 0; i < arity; ++i) {
    transfers.TransferStackSlot(target.stack_state[target_stack_base + i],
                                cache_state_.stack_state[stack_base + i]);
  }
}

void LiftoffAssembler::Spill(VarState* slot) {
  switch (slot->loc()) {
    case VarState::kStack:
      return;
    case VarState::kRegister:
      Spill(slot->offset(), slot->reg(), slot->type());
      cache_state_.dec_used(slot->reg());
      break;
    case VarState::kIntConst:
      Spill(slot->offset(), slot->constant());
      break;
  }
  slot->MakeStack();
}

void LiftoffAssembler::SpillLocals() {
  for (uint32_t i = 0; i < num_locals_; ++i) {
    Spill(&cache_state_.stack_state[i]);
  }
}

void LiftoffAssembler::SpillAllRegisters() {
  for (uint32_t i = 0, e = cache_state_.stack_height(); i < e; ++i) {
    auto& slot = cache_state_.stack_state[i];
    if (!slot.is_reg()) continue;
    Spill(slot.offset(), slot.reg(), slot.type());
    slot.MakeStack();
  }
  cache_state_.reset_used_registers();
}

void LiftoffAssembler::PrepareCall(FunctionSig* sig,
                                   compiler::CallDescriptor* call_descriptor,
                                   Register* target,
                                   Register* target_instance) {
  uint32_t num_params = static_cast<uint32_t>(sig->parameter_count());
  // Input 0 is the call target.
  constexpr size_t kInputShift = 1;

  // Spill all cache slots which are not being used as parameters.
  // Don't update any register use counters, they will be reset later anyway.
  for (uint32_t idx = 0, end = cache_state_.stack_height() - num_params;
       idx < end; ++idx) {
    VarState& slot = cache_state_.stack_state[idx];
    if (!slot.is_reg()) continue;
    Spill(slot.offset(), slot.reg(), slot.type());
    slot.MakeStack();
  }

  LiftoffStackSlots stack_slots(this);
  StackTransferRecipe stack_transfers(this);
  LiftoffRegList param_regs;

  // Move the target instance (if supplied) into the correct instance register.
  compiler::LinkageLocation instance_loc =
      call_descriptor->GetInputLocation(kInputShift);
  DCHECK(instance_loc.IsRegister() && !instance_loc.IsAnyRegister());
  Register instance_reg = Register::from_code(instance_loc.AsRegister());
  param_regs.set(instance_reg);
  if (target_instance && *target_instance != instance_reg) {
    stack_transfers.MoveRegister(LiftoffRegister(instance_reg),
                                 LiftoffRegister(*target_instance),
                                 kWasmIntPtr);
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
    const uint32_t stack_offset = slot.offset();
    // Process both halfs of a register pair separately, because they are passed
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
        int reg_code = loc.AsRegister();
#if V8_TARGET_ARCH_ARM
        // Liftoff assumes a one-to-one mapping between float registers and
        // double registers, and so does not distinguish between f32 and f64
        // registers. The f32 register code must therefore be halved in order to
        // pass the f64 code to Liftoff.
        DCHECK_IMPLIES(type == kWasmF32, (reg_code % 2) == 0);
        LiftoffRegister reg = LiftoffRegister::from_code(
            rc, (type == kWasmF32) ? (reg_code / 2) : reg_code);
#else
        LiftoffRegister reg = LiftoffRegister::from_code(rc, reg_code);
#endif
        param_regs.set(reg);
        if (is_pair) {
          stack_transfers.LoadI64HalfIntoRegister(reg, slot, stack_offset,
                                                  half);
        } else {
          stack_transfers.LoadIntoRegister(reg, slot, stack_offset);
        }
      } else {
        DCHECK(loc.IsCallerFrameSlot());
        stack_slots.Add(slot, stack_offset, half);
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
      stack_slots.Add(LiftoffAssembler::VarState(LiftoffAssembler::kWasmIntPtr,
                                                 LiftoffRegister(*target), 0));
      *target = no_reg;
    }
  }

  // Create all the slots.
  stack_slots.Construct();
  // Execute the stack transfers before filling the instance register.
  stack_transfers.Execute();

  // Pop parameters from the value stack.
  cache_state_.stack_state.pop_back(num_params);

  // Reset register use counters.
  cache_state_.reset_used_registers();

  // Reload the instance from the stack.
  if (!target_instance) {
    FillInstanceInto(instance_reg);
  }
}

void LiftoffAssembler::FinishCall(FunctionSig* sig,
                                  compiler::CallDescriptor* call_descriptor) {
  const size_t return_count = sig->return_count();
  if (return_count != 0) {
    DCHECK_EQ(1, return_count);
    ValueType return_type = sig->GetReturn(0);
    const bool need_pair = kNeedI64RegPair && return_type == kWasmI64;
    DCHECK_EQ(need_pair ? 2 : 1, call_descriptor->ReturnCount());
    RegClass rc = need_pair ? kGpReg : reg_class_for(return_type);
#if V8_TARGET_ARCH_ARM
    // If the return register was not d0 for f32, the code value would have to
    // be halved as is done for the parameter registers.
    DCHECK_EQ(call_descriptor->GetReturnLocation(0).AsRegister(), 0);
#endif
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
    Vector<ParallelRegisterMoveTuple> tuples) {
  StackTransferRecipe stack_transfers(this);
  for (auto tuple : tuples) {
    if (tuple.dst == tuple.src) continue;
    stack_transfers.MoveRegister(tuple.dst, tuple.src, tuple.type);
  }
}

void LiftoffAssembler::MoveToReturnRegisters(FunctionSig* sig) {
  // We do not support multi-value yet.
  DCHECK_EQ(1, sig->return_count());
  ValueType return_type = sig->GetReturn(0);
  StackTransferRecipe stack_transfers(this);
  LiftoffRegister return_reg =
      needs_reg_pair(return_type)
          ? LiftoffRegister::ForPair(kGpReturnRegisters[0],
                                     kGpReturnRegisters[1])
          : reg_class_for(return_type) == kGpReg
                ? LiftoffRegister(kGpReturnRegisters[0])
                : LiftoffRegister(kFpReturnRegisters[0]);
  stack_transfers.LoadIntoRegister(return_reg, cache_state_.stack_state.back(),
                                   cache_state_.stack_state.back().offset());
}

#ifdef ENABLE_SLOW_DCHECKS
bool LiftoffAssembler::ValidateCacheState() const {
  uint32_t register_use_count[kAfterMaxLiftoffRegCode] = {0};
  LiftoffRegList used_regs;
  for (const VarState& var : cache_state_.stack_state) {
    if (!var.is_reg()) continue;
    LiftoffRegister reg = var.reg();
    if (kNeedI64RegPair && reg.is_pair()) {
      ++register_use_count[reg.low().liftoff_code()];
      ++register_use_count[reg.high().liftoff_code()];
    } else {
      ++register_use_count[reg.liftoff_code()];
    }
    used_regs.set(reg);
  }
  bool valid = memcmp(register_use_count, cache_state_.register_use_count,
                      sizeof(register_use_count)) == 0 &&
               used_regs == cache_state_.used_registers;
  if (valid) return true;
  std::ostringstream os;
  os << "Error in LiftoffAssembler::ValidateCacheState().\n";
  os << "expected: used_regs " << used_regs << ", counts "
     << PrintCollection(register_use_count) << "\n";
  os << "found:    used_regs " << cache_state_.used_registers << ", counts "
     << PrintCollection(cache_state_.register_use_count) << "\n";
  os << "Use --trace-wasm-decoder and --trace-liftoff to debug.";
  FATAL("%s", os.str().c_str());
}
#endif

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
    Spill(slot->offset(), slot->reg(), slot->type());
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
  os << ValueTypes::TypeName(slot.type()) << ":";
  switch (slot.loc()) {
    case VarState::kStack:
      return os << "s";
    case VarState::kRegister:
      return os << slot.reg();
    case VarState::kIntConst:
      return os << "c" << slot.i32_const();
  }
  UNREACHABLE();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
