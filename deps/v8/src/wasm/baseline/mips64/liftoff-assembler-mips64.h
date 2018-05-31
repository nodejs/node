// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_MIPS64_LIFTOFF_ASSEMBLER_MIPS64_H_
#define V8_WASM_BASELINE_MIPS64_LIFTOFF_ASSEMBLER_MIPS64_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#define BAILOUT(reason) bailout("mips64 " reason)

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

// fp-8 holds the stack marker, fp-16 is the instance parameter, first stack
// slot is located at fp-24.
constexpr int32_t kConstantStackSpace = 16;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline MemOperand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return MemOperand(fp, -kFirstStackSlotOffset - offset);
}

inline MemOperand GetInstanceOperand() { return MemOperand(fp, -16); }

// Use this register to store the address of the last argument pushed on the
// stack for a call to C. This register must be callee saved according to the c
// calling convention.
static constexpr Register kCCallLastArgAddrReg = s1;

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, MemOperand src,
                 ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->lw(dst.gp(), src);
      break;
    case kWasmI64:
      assm->ld(dst.gp(), src);
      break;
    case kWasmF32:
      assm->lwc1(dst.fp(), src);
      break;
    case kWasmF64:
      assm->Ldc1(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type) {
    case kWasmI32:
    case kWasmI64:
      assm->push(reg.gp());
      break;
    case kWasmF32:
      assm->daddiu(sp, sp, -kPointerSize);
      assm->swc1(reg.fp(), MemOperand(sp, 0));
      break;
    case kWasmF64:
      assm->daddiu(sp, sp, -kPointerSize);
      assm->Sdc1(reg.fp(), MemOperand(sp, 0));
      break;
    default:
      UNREACHABLE();
  }
}

}  // namespace liftoff

uint32_t LiftoffAssembler::PrepareStackFrame() {
  uint32_t offset = static_cast<uint32_t>(pc_offset());
  // When constant that represents size of stack frame can't be represented
  // as 16bit we need three instructions to add it to sp, so we reserve space
  // for this case.
  daddiu(sp, sp, 0);
  nop();
  nop();
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(uint32_t offset,
                                              uint32_t stack_slots) {
  uint64_t bytes = liftoff::kConstantStackSpace + kStackSlotSize * stack_slots;
  DCHECK_LE(bytes, kMaxInt);
  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 256;
  TurboAssembler patching_assembler(isolate(), buffer_ + offset,
                                    kAvailableSpace, CodeObjectRequired::kNo);
  // If bytes can be represented as 16bit, daddiu will be generated and two
  // nops will stay untouched. Otherwise, lui-ori sequence will load it to
  // register and, as third instruction, daddu will be generated.
  patching_assembler.Daddu(sp, sp, Operand(-bytes));
}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type()) {
    case kWasmI32:
      TurboAssembler::li(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kWasmI64:
      TurboAssembler::li(reg.gp(), Operand(value.to_i64(), rmode));
      break;
    case kWasmF32:
      TurboAssembler::Move(reg.fp(), value.to_f32_boxed().get_bits());
      break;
    case kWasmF64:
      TurboAssembler::Move(reg.fp(), value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadFromInstance(Register dst, uint32_t offset,
                                        int size) {
  DCHECK_LE(offset, kMaxInt);
  ld(dst, liftoff::GetInstanceOperand());
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    lw(dst, MemOperand(dst, offset));
  } else {
    ld(dst, MemOperand(dst, offset));
  }
}

void LiftoffAssembler::SpillInstance(Register instance) {
  sd(instance, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  ld(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc) {
  // TODO(ksreten): Add check if unaligned memory access
  MemOperand src_op(src_addr, offset_imm);
  if (offset_reg != no_reg) {
    Register src = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(src, src_addr, offset_reg);
    src_op = MemOperand(src, offset_imm);
  }

  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      lbu(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
    case LoadType::kI64Load8S:
      lb(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      TurboAssembler::Ulhu(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      TurboAssembler::Ulh(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      TurboAssembler::Ulwu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32S:
      TurboAssembler::Ulw(dst.gp(), src_op);
      break;
    case LoadType::kI64Load:
      TurboAssembler::Uld(dst.gp(), src_op);
      break;
    case LoadType::kF32Load:
      TurboAssembler::Ulwc1(dst.fp(), src_op, t8);
      break;
    case LoadType::kF64Load:
      TurboAssembler::Uldc1(dst.fp(), src_op, t8);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc) {
  // TODO(ksreten): Add check if unaligned memory access
  Register dst = no_reg;
  if (offset_reg != no_reg) {
    dst = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(dst, dst_addr, offset_reg);
  }
  MemOperand dst_op = (offset_reg != no_reg) ? MemOperand(dst, offset_imm)
                                             : MemOperand(dst_addr, offset_imm);

  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      sb(src.gp(), dst_op);
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      TurboAssembler::Ush(src.gp(), dst_op, t8);
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      TurboAssembler::Usw(src.gp(), dst_op);
      break;
    case StoreType::kI64Store:
      TurboAssembler::Usd(src.gp(), dst_op);
      break;
    case StoreType::kF32Store:
      TurboAssembler::Uswc1(src.fp(), dst_op, t8);
      break;
    case StoreType::kF64Store:
      TurboAssembler::Usdc1(src.fp(), dst_op, t8);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  MemOperand src(fp, kPointerSize * (caller_slot_idx + 1));
  liftoff::Load(this, dst, src, type);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  LiftoffRegister reg = GetUnusedRegister(reg_class_for(type));
  Fill(reg, src_index, type);
  Spill(dst_index, reg, type);
}

void LiftoffAssembler::MoveToReturnRegister(LiftoffRegister reg,
                                            ValueType type) {
  LiftoffRegister dst = reg.is_gp() ? LiftoffRegister(v0) : LiftoffRegister(f2);
  if (reg != dst) Move(dst, reg, type);
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  // TODO(ksreten): Handle different sizes here.
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  MemOperand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      sw(reg.gp(), dst);
      break;
    case kWasmI64:
      sd(reg.gp(), dst);
      break;
    case kWasmF32:
      swc1(reg.fp(), dst);
      break;
    case kWasmF64:
      TurboAssembler::Sdc1(reg.fp(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  RecordUsedSpillSlot(index);
  MemOperand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg);
      TurboAssembler::li(tmp.gp(), Operand(value.to_i32()));
      sw(tmp.gp(), dst);
      break;
    }
    case kWasmI64: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg);
      TurboAssembler::li(tmp.gp(), value.to_i64());
      sd(tmp.gp(), dst);
      break;
    }
    default:
      // kWasmF32 and kWasmF64 are unreachable, since those
      // constants are not tracked.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  MemOperand src = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      lw(reg.gp(), src);
      break;
    case kWasmI64:
      ld(reg.gp(), src);
      break;
    case kWasmF32:
      lwc1(reg.fp(), src);
      break;
    case kWasmF64:
      TurboAssembler::Ldc1(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register, uint32_t half_index) {
  UNREACHABLE();
}

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  TurboAssembler::Mul(dst, lhs, rhs);
}

#define I32_BINOP(name, instruction)                                 \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    instruction(dst, lhs, rhs);                                      \
  }

// clang-format off
I32_BINOP(add, addu)
I32_BINOP(sub, subu)
I32_BINOP(and, and_)
I32_BINOP(or, or_)
I32_BINOP(xor, xor_)
// clang-format on

#undef I32_BINOP

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  TurboAssembler::Clz(dst, src);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  TurboAssembler::Ctz(dst, src);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  TurboAssembler::Popcnt(dst, src);
  return true;
}

#define I32_SHIFTOP(name, instruction)                                      \
  void LiftoffAssembler::emit_i32_##name(                                   \
      Register dst, Register src, Register amount, LiftoffRegList pinned) { \
    instruction(dst, src, amount);                                          \
  }

I32_SHIFTOP(shl, sllv)
I32_SHIFTOP(sar, srav)
I32_SHIFTOP(shr, srlv)

#undef I32_SHIFTOP

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::Daddu(dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  BAILOUT("i64_sub");
}

#define I64_BINOP(name)                                                \
  void LiftoffAssembler::emit_i64_##name(                              \
      LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
    BAILOUT("i64_" #name);                                             \
  }

// clang-format off
I64_BINOP(and)
I64_BINOP(or)
I64_BINOP(xor)
// clang-format on

#undef I64_BINOP

#define I64_SHIFTOP(name, instruction)                                         \
  void LiftoffAssembler::emit_i64_##name(LiftoffRegister dst,                  \
                                         LiftoffRegister src, Register amount, \
                                         LiftoffRegList pinned) {              \
    instruction(dst.gp(), src.gp(), amount);                                   \
  }

I64_SHIFTOP(shl, dsllv)
I64_SHIFTOP(sar, dsrav)
I64_SHIFTOP(shr, dsrlv)

#undef I64_SHIFTOP

#define FP_BINOP(name, instruction)                                          \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst, lhs, rhs);                                              \
  }
#define FP_UNOP(name, instruction)                                             \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst, src);                                                     \
  }

FP_BINOP(f32_add, add_s)
FP_BINOP(f32_sub, sub_s)
FP_BINOP(f32_mul, mul_s)
FP_BINOP(f32_div, div_s)
FP_UNOP(f32_abs, abs_s)
FP_UNOP(f32_neg, neg_s)
FP_UNOP(f32_ceil, Ceil_s_s)
FP_UNOP(f32_floor, Floor_s_s)
FP_UNOP(f32_trunc, Trunc_s_s)
FP_UNOP(f32_nearest_int, Round_s_s)
FP_UNOP(f32_sqrt, sqrt_s)
FP_BINOP(f64_add, add_d)
FP_BINOP(f64_sub, sub_d)
FP_BINOP(f64_mul, mul_d)
FP_BINOP(f64_div, div_d)
FP_UNOP(f64_abs, abs_d)
FP_UNOP(f64_neg, neg_d)
FP_UNOP(f64_ceil, Ceil_d_d)
FP_UNOP(f64_floor, Floor_d_d)
FP_UNOP(f64_trunc, Trunc_d_d)
FP_UNOP(f64_nearest_int, Round_d_d)
FP_UNOP(f64_sqrt, sqrt_d)

#undef FP_BINOP
#undef FP_UNOP

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src) {
  switch (opcode) {
    case kExprI32ConvertI64:
      TurboAssembler::Ext(dst.gp(), src.gp(), 0, 32);
      return true;
    case kExprI32ReinterpretF32:
      TurboAssembler::FmoveLow(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      sll(dst.gp(), src.gp(), 0);
      return true;
    case kExprI64UConvertI32:
      TurboAssembler::Dext(dst.gp(), src.gp(), 0, 32);
      return true;
    case kExprI64ReinterpretF64:
      dmfc1(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32: {
      LiftoffRegister scratch =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(dst));
      mtc1(src.gp(), scratch.fp());
      cvt_s_w(dst.fp(), scratch.fp());
      return true;
    }
    case kExprF32UConvertI32:
      TurboAssembler::Cvt_s_uw(dst.fp(), src.gp());
      return true;
    case kExprF32ConvertF64:
      cvt_s_d(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      TurboAssembler::FmoveLow(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32: {
      LiftoffRegister scratch =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(dst));
      mtc1(src.gp(), scratch.fp());
      cvt_d_w(dst.fp(), scratch.fp());
      return true;
    }
    case kExprF64UConvertI32:
      TurboAssembler::Cvt_d_uw(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      cvt_d_s(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      dmtc1(src.gp(), dst.fp());
      return true;
    default:
      return false;
  }
}

void LiftoffAssembler::emit_jump(Label* label) {
  TurboAssembler::Branch(label);
}

void LiftoffAssembler::emit_jump(Register target) { BAILOUT("emit_jump"); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    TurboAssembler::Branch(label, cond, lhs, Operand(rhs));
  } else {
    TurboAssembler::Branch(label, cond, lhs, Operand(zero_reg));
  }
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  sltiu(dst, src, 1);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  Register tmp = dst;
  if (dst == lhs || dst == rhs) {
    tmp = GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(lhs, rhs)).gp();
  }
  // Write 1 as result.
  TurboAssembler::li(tmp, 1);

  // If negative condition is true, write 0 as result.
  Condition neg_cond = NegateCondition(cond);
  TurboAssembler::LoadZeroOnCondition(tmp, lhs, Operand(rhs), neg_cond);

  // If tmp != dst, result will be moved.
  TurboAssembler::Move(dst, tmp);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  sltiu(dst, src.gp(), 1);
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  Register tmp = dst;
  if (dst == lhs.gp() || dst == rhs.gp()) {
    tmp = GetUnusedRegister(kGpReg, LiftoffRegList::ForRegs(lhs, rhs)).gp();
  }
  // Write 1 as result.
  TurboAssembler::li(tmp, 1);

  // If negative condition is true, write 0 as result.
  Condition neg_cond = NegateCondition(cond);
  TurboAssembler::LoadZeroOnCondition(tmp, lhs.gp(), Operand(rhs.gp()),
                                      neg_cond);

  // If tmp != dst, result will be moved.
  TurboAssembler::Move(dst, tmp);
}

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("emit_f32_set_cond");
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("emit_f64_set_cond");
}

void LiftoffAssembler::StackCheck(Label* ool_code) {
  LiftoffRegister tmp = GetUnusedRegister(kGpReg);
  TurboAssembler::li(
      tmp.gp(), Operand(ExternalReference::address_of_stack_limit(isolate())));
  TurboAssembler::Uld(tmp.gp(), MemOperand(tmp.gp()));
  TurboAssembler::Branch(ool_code, ule, sp, Operand(tmp.gp()));
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg).gp());
  CallCFunction(
      ExternalReference::wasm_call_trap_callback_for_testing(isolate()), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void LiftoffAssembler::PushCallerFrameSlot(const VarState& src,
                                           uint32_t src_index,
                                           RegPairHalf half) {
  switch (src.loc()) {
    case VarState::kStack:
      ld(at, liftoff::GetStackSlot(src_index));
      push(at);
      break;
    case VarState::kRegister:
      PushCallerFrameSlot(src.reg(), src.type());
      break;
    case VarState::KIntConst: {
      li(at, Operand(src.i32_const()));
      push(at);
      break;
    }
  }
}

void LiftoffAssembler::PushCallerFrameSlot(LiftoffRegister reg,
                                           ValueType type) {
  liftoff::push(this, reg, type);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  unsigned num_gp_regs = gp_regs.GetNumRegsSet();
  if (num_gp_regs) {
    unsigned offset = num_gp_regs * kPointerSize;
    daddiu(sp, sp, -offset);
    while (!gp_regs.is_empty()) {
      LiftoffRegister reg = gp_regs.GetFirstRegSet();
      offset -= kPointerSize;
      sd(reg.gp(), MemOperand(sp, offset));
      gp_regs.clear(reg);
    }
    DCHECK_EQ(offset, 0);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    daddiu(sp, sp, -(num_fp_regs * kStackSlotSize));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      TurboAssembler::Sdc1(reg.fp(), MemOperand(sp, offset));
      fp_regs.clear(reg);
      offset += sizeof(double);
    }
    DCHECK_EQ(offset, num_fp_regs * sizeof(double));
  }
}

void LiftoffAssembler::PopRegisters(LiftoffRegList regs) {
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned fp_offset = 0;
  while (!fp_regs.is_empty()) {
    LiftoffRegister reg = fp_regs.GetFirstRegSet();
    TurboAssembler::Ldc1(reg.fp(), MemOperand(sp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) daddiu(sp, sp, fp_offset);
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  unsigned gp_offset = 0;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    ld(reg.gp(), MemOperand(sp, gp_offset));
    gp_regs.clear(reg);
    gp_offset += kPointerSize;
  }
  daddiu(sp, sp, gp_offset);
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots, (1 << 16) / kPointerSize);  // 16 bit immediate
  TurboAssembler::DropAndRet(static_cast<int>(num_stack_slots));
}

void LiftoffAssembler::PrepareCCall(wasm::FunctionSig* sig,
                                    const LiftoffRegister* args,
                                    ValueType out_argument_type) {
  for (ValueType param_type : sig->parameters()) {
    liftoff::push(this, *args++, param_type);
  }
  if (out_argument_type != kWasmStmt) {
    daddiu(sp, sp, -kPointerSize);
  }
  // Save the original sp (before the first push), such that we can later
  // compute pointers to the pushed values. Do this only *after* pushing the
  // values, because {kCCallLastArgAddrReg} might collide with an arg register.
  int num_c_call_arguments = static_cast<int>(sig->parameter_count()) +
                             (out_argument_type != kWasmStmt);
  int pushed_bytes = kPointerSize * num_c_call_arguments;
  daddiu(liftoff::kCCallLastArgAddrReg, sp, pushed_bytes);
  constexpr Register kScratch = at;
  static_assert(kScratch != liftoff::kCCallLastArgAddrReg, "collision");
  PrepareCallCFunction(num_c_call_arguments, kScratch);
}

void LiftoffAssembler::SetCCallRegParamAddr(Register dst, int param_byte_offset,
                                            ValueType type) {
  // Check that we don't accidentally override kCCallLastArgAddrReg.
  DCHECK_NE(liftoff::kCCallLastArgAddrReg, dst);
  daddiu(dst, liftoff::kCCallLastArgAddrReg, -param_byte_offset);
}

void LiftoffAssembler::SetCCallStackParamAddr(int stack_param_idx,
                                              int param_byte_offset,
                                              ValueType type) {
  static constexpr Register kScratch = at;
  SetCCallRegParamAddr(kScratch, param_byte_offset, type);
  sd(kScratch, MemOperand(sp, stack_param_idx * kPointerSize));
}

void LiftoffAssembler::LoadCCallOutArgument(LiftoffRegister dst, ValueType type,
                                            int param_byte_offset) {
  // Check that we don't accidentally override kCCallLastArgAddrReg.
  DCHECK_NE(LiftoffRegister(liftoff::kCCallLastArgAddrReg), dst);
  MemOperand src(liftoff::kCCallLastArgAddrReg, -param_byte_offset);
  liftoff::Load(this, dst, src, type);
}

void LiftoffAssembler::CallC(ExternalReference ext_ref, uint32_t num_params) {
  CallCFunction(ext_ref, static_cast<int>(num_params));
}

void LiftoffAssembler::FinishCCall() {
  TurboAssembler::Move(sp, liftoff::kCCallLastArgAddrReg);
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  Call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallRuntime(Zone* zone, Runtime::FunctionId fid) {
  // Set instance to zero.
  TurboAssembler::Move(cp, zero_reg);
  CallRuntimeDelayed(zone, fid);
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    pop(at);
    Call(at);
  } else {
    Call(target);
  }
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  daddiu(sp, sp, -size);
  TurboAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  daddiu(sp, sp, size);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef BAILOUT

#endif  // V8_WASM_BASELINE_MIPS64_LIFTOFF_ASSEMBLER_MIPS64_H_
