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

inline void Store(LiftoffAssembler* assm, Register base, int32_t offset,
                  LiftoffRegister src, ValueType type) {
  MemOperand dst(base, offset);
  switch (type) {
    case kWasmI32:
      assm->Usw(src.gp(), dst);
      break;
    case kWasmI64:
      assm->Usd(src.gp(), dst);
      break;
    case kWasmF32:
      assm->Uswc1(src.fp(), dst, t8);
      break;
    case kWasmF64:
      assm->Usdc1(src.fp(), dst, t8);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->daddiu(sp, sp, -kPointerSize);
      assm->sw(reg.gp(), MemOperand(sp, 0));
      break;
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

#if defined(V8_TARGET_BIG_ENDIAN)
inline void ChangeEndiannessLoad(LiftoffAssembler* assm, LiftoffRegister dst,
                                 LoadType type, LiftoffRegList pinned) {
  bool is_float = false;
  LiftoffRegister tmp = dst;
  switch (type.value()) {
    case LoadType::kI64Load8U:
    case LoadType::kI64Load8S:
    case LoadType::kI32Load8U:
    case LoadType::kI32Load8S:
      // No need to change endianness for byte size.
      return;
    case LoadType::kF32Load:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI32ReinterpretF32, tmp, dst);
      V8_FALLTHROUGH;
    case LoadType::kI64Load32U:
      assm->TurboAssembler::ByteSwapUnsigned(tmp.gp(), tmp.gp(), 4);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32S:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case LoadType::kI32Load16S:
    case LoadType::kI64Load16S:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      assm->TurboAssembler::ByteSwapUnsigned(tmp.gp(), tmp.gp(), 2);
      break;
    case LoadType::kF64Load:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI64ReinterpretF64, tmp, dst);
      V8_FALLTHROUGH;
    case LoadType::kI64Load:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 8);
      break;
    default:
      UNREACHABLE();
  }

  if (is_float) {
    switch (type.value()) {
      case LoadType::kF32Load:
        assm->emit_type_conversion(kExprF32ReinterpretI32, dst, tmp);
        break;
      case LoadType::kF64Load:
        assm->emit_type_conversion(kExprF64ReinterpretI64, dst, tmp);
        break;
      default:
        UNREACHABLE();
    }
  }
}

inline void ChangeEndiannessStore(LiftoffAssembler* assm, LiftoffRegister src,
                                  StoreType type, LiftoffRegList pinned) {
  bool is_float = false;
  LiftoffRegister tmp = src;
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      // No need to change endianness for byte size.
      return;
    case StoreType::kF32Store:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI32ReinterpretF32, tmp, src);
      V8_FALLTHROUGH;
    case StoreType::kI32Store:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case StoreType::kI32Store16:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
      break;
    case StoreType::kF64Store:
      is_float = true;
      tmp = assm->GetUnusedRegister(kGpReg, pinned);
      assm->emit_type_conversion(kExprI64ReinterpretF64, tmp, src);
      V8_FALLTHROUGH;
    case StoreType::kI64Store:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 8);
      break;
    case StoreType::kI64Store32:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 4);
      break;
    case StoreType::kI64Store16:
      assm->TurboAssembler::ByteSwapSigned(tmp.gp(), tmp.gp(), 2);
      break;
    default:
      UNREACHABLE();
  }

  if (is_float) {
    switch (type.value()) {
      case StoreType::kF32Store:
        assm->emit_type_conversion(kExprF32ReinterpretI32, src, tmp);
        break;
      case StoreType::kF64Store:
        assm->emit_type_conversion(kExprF64ReinterpretI64, src, tmp);
        break;
      default:
        UNREACHABLE();
    }
  }
}
#endif  // V8_TARGET_BIG_ENDIAN

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  // When constant that represents size of stack frame can't be represented
  // as 16bit we need three instructions to add it to sp, so we reserve space
  // for this case.
  daddiu(sp, sp, 0);
  nop();
  nop();
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset,
                                              uint32_t stack_slots) {
  uint64_t bytes = liftoff::kConstantStackSpace + kStackSlotSize * stack_slots;
  DCHECK_LE(bytes, kMaxInt);
  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 256;
  TurboAssembler patching_assembler(nullptr, AssemblerOptions{},
                                    buffer_ + offset, kAvailableSpace,
                                    CodeObjectRequired::kNo);
  // If bytes can be represented as 16bit, daddiu will be generated and two
  // nops will stay untouched. Otherwise, lui-ori sequence will load it to
  // register and, as third instruction, daddu will be generated.
  patching_assembler.Daddu(sp, sp, Operand(-bytes));
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

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
                            uint32_t* protected_load_pc, bool is_load_mem) {
  Register src = no_reg;
  if (offset_reg != no_reg) {
    src = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(src, src_addr, offset_reg);
  }
  MemOperand src_op = (offset_reg != no_reg) ? MemOperand(src, offset_imm)
                                             : MemOperand(src_addr, offset_imm);

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
    case LoadType::kI64Load32U:
      TurboAssembler::Ulwu(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
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

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_load_mem) {
    pinned.set(src_op.rm());
    liftoff::ChangeEndiannessLoad(this, dst, type, pinned);
  }
#endif
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  Register dst = no_reg;
  if (offset_reg != no_reg) {
    dst = GetUnusedRegister(kGpReg, pinned).gp();
    emit_ptrsize_add(dst, dst_addr, offset_reg);
  }
  MemOperand dst_op = (offset_reg != no_reg) ? MemOperand(dst, offset_imm)
                                             : MemOperand(dst_addr, offset_imm);

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_store_mem) {
    pinned.set(dst_op.rm());
    LiftoffRegister tmp = GetUnusedRegister(src.reg_class(), pinned);
    // Save original value.
    Move(tmp, src, type.value_type());

    src = tmp;
    pinned.set(tmp);
    liftoff::ChangeEndiannessStore(this, src, type, pinned);
  }
#endif

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

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));

  // Check if lhs == kMinInt and rhs == -1, since this case is unrepresentable.
  TurboAssembler::li(kScratchReg, 1);
  TurboAssembler::li(kScratchReg2, 1);
  TurboAssembler::LoadZeroOnCondition(kScratchReg, lhs, Operand(kMinInt), eq);
  TurboAssembler::LoadZeroOnCondition(kScratchReg2, rhs, Operand(-1), eq);
  daddu(kScratchReg, kScratchReg, kScratchReg2);
  TurboAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  TurboAssembler::Div(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Divu(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Mod(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Modu(dst, lhs, rhs);
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
#define I32_SHIFTOP_I(name, instruction)                             \
  I32_SHIFTOP(name, instruction##v)                                  \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register src, \
                                         int amount) {               \
    DCHECK(is_uint5(amount));                                        \
    instruction(dst, src, amount);                                   \
  }

I32_SHIFTOP(shl, sllv)
I32_SHIFTOP(sar, srav)
I32_SHIFTOP_I(shr, srl)

#undef I32_SHIFTOP
#undef I32_SHIFTOP_I

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::Dmul(dst.gp(), lhs.gp(), rhs.gp());
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));

  // Check if lhs == MinInt64 and rhs == -1, since this case is unrepresentable.
  TurboAssembler::li(kScratchReg, 1);
  TurboAssembler::li(kScratchReg2, 1);
  TurboAssembler::LoadZeroOnCondition(
      kScratchReg, lhs.gp(), Operand(std::numeric_limits<int64_t>::min()), eq);
  TurboAssembler::LoadZeroOnCondition(kScratchReg2, rhs.gp(), Operand(-1), eq);
  daddu(kScratchReg, kScratchReg, kScratchReg2);
  TurboAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  TurboAssembler::Ddiv(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  TurboAssembler::Ddivu(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  TurboAssembler::Dmod(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs.gp(), Operand(zero_reg));
  TurboAssembler::Dmodu(dst.gp(), lhs.gp(), rhs.gp());
  return true;
}

#define I64_BINOP(name, instruction)                                   \
  void LiftoffAssembler::emit_i64_##name(                              \
      LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
    instruction(dst.gp(), lhs.gp(), rhs.gp());                         \
  }

// clang-format off
I64_BINOP(add, daddu)
I64_BINOP(sub, dsubu)
I64_BINOP(and, and_)
I64_BINOP(or, or_)
I64_BINOP(xor, xor_)
// clang-format on

#undef I64_BINOP

#define I64_SHIFTOP(name, instruction)                                         \
  void LiftoffAssembler::emit_i64_##name(LiftoffRegister dst,                  \
                                         LiftoffRegister src, Register amount, \
                                         LiftoffRegList pinned) {              \
    instruction(dst.gp(), src.gp(), amount);                                   \
  }
#define I64_SHIFTOP_I(name, instruction)                                    \
  I64_SHIFTOP(name, instruction##v)                                         \
  void LiftoffAssembler::emit_i64_##name(LiftoffRegister dst,               \
                                         LiftoffRegister src, int amount) { \
    DCHECK(is_uint6(amount));                                               \
    instruction(dst.gp(), src.gp(), amount);                                \
  }

I64_SHIFTOP(shl, dsllv)
I64_SHIFTOP(sar, dsrav)
I64_SHIFTOP_I(shr, dsrl)

#undef I64_SHIFTOP
#undef I64_SHIFTOP_I

void LiftoffAssembler::emit_i32_to_intptr(Register dst, Register src) {
  addu(dst, src, zero_reg);
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  TurboAssembler::Neg_s(dst, src);
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  TurboAssembler::Neg_d(dst, src);
}

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float32Min(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float32MinOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float32Max(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float32MaxOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("f32_copysign");
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float64Min(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float64MinOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  Label ool, done;
  TurboAssembler::Float64Max(dst, lhs, rhs, &ool);
  Branch(&done);

  bind(&ool);
  TurboAssembler::Float64MaxOutOfLine(dst, lhs, rhs);
  bind(&done);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  BAILOUT("f64_copysign");
}

#define FP_BINOP(name, instruction)                                          \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister lhs, \
                                     DoubleRegister rhs) {                   \
    instruction(dst, lhs, rhs);                                              \
  }
#define FP_UNOP(name, instruction)                                             \
  void LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst, src);                                                     \
  }
#define FP_UNOP_RETURN_TRUE(name, instruction)                                 \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    instruction(dst, src);                                                     \
    return true;                                                               \
  }

FP_BINOP(f32_add, add_s)
FP_BINOP(f32_sub, sub_s)
FP_BINOP(f32_mul, mul_s)
FP_BINOP(f32_div, div_s)
FP_UNOP(f32_abs, abs_s)
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
FP_UNOP_RETURN_TRUE(f64_ceil, Ceil_d_d)
FP_UNOP_RETURN_TRUE(f64_floor, Floor_d_d)
FP_UNOP_RETURN_TRUE(f64_trunc, Trunc_d_d)
FP_UNOP_RETURN_TRUE(f64_nearest_int, Round_d_d)
FP_UNOP(f64_sqrt, sqrt_d)

#undef FP_BINOP
#undef FP_UNOP
#undef FP_UNOP_RETURN_TRUE

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      TurboAssembler::Ext(dst.gp(), src.gp(), 0, 32);
      return true;
    case kExprI32SConvertF32: {
      LiftoffRegister rounded =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src));
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src, rounded));

      // Real conversion.
      TurboAssembler::Trunc_s_s(rounded.fp(), src.fp());
      trunc_w_s(kScratchDoubleReg, rounded.fp());
      mfc1(dst.gp(), kScratchDoubleReg);
      // Avoid INT32_MAX as an overflow indicator and use INT32_MIN instead,
      // because INT32_MIN allows easier out-of-bounds detection.
      TurboAssembler::Addu(kScratchReg, dst.gp(), 1);
      TurboAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      TurboAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      mtc1(dst.gp(), kScratchDoubleReg);
      cvt_s_w(converted_back.fp(), kScratchDoubleReg);
      TurboAssembler::CompareF32(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32UConvertF32: {
      LiftoffRegister rounded =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src));
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src, rounded));

      // Real conversion.
      TurboAssembler::Trunc_s_s(rounded.fp(), src.fp());
      TurboAssembler::Trunc_uw_s(dst.gp(), rounded.fp(), kScratchDoubleReg);
      // Avoid UINT32_MAX as an overflow indicator and use 0 instead,
      // because 0 allows easier out-of-bounds detection.
      TurboAssembler::Addu(kScratchReg, dst.gp(), 1);
      TurboAssembler::Movz(dst.gp(), zero_reg, kScratchReg);

      // Checking if trap.
      TurboAssembler::Cvt_d_uw(converted_back.fp(), dst.gp());
      cvt_s_d(converted_back.fp(), converted_back.fp());
      TurboAssembler::CompareF32(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32SConvertF64: {
      LiftoffRegister rounded =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src));
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src, rounded));

      // Real conversion.
      TurboAssembler::Trunc_d_d(rounded.fp(), src.fp());
      trunc_w_d(kScratchDoubleReg, rounded.fp());
      mfc1(dst.gp(), kScratchDoubleReg);

      // Checking if trap.
      cvt_d_w(converted_back.fp(), kScratchDoubleReg);
      TurboAssembler::CompareF64(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32UConvertF64: {
      LiftoffRegister rounded =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src));
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src, rounded));

      // Real conversion.
      TurboAssembler::Trunc_d_d(rounded.fp(), src.fp());
      TurboAssembler::Trunc_uw_d(dst.gp(), rounded.fp(), kScratchDoubleReg);

      // Checking if trap.
      TurboAssembler::Cvt_d_uw(converted_back.fp(), dst.gp());
      TurboAssembler::CompareF64(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI32ReinterpretF32:
      TurboAssembler::FmoveLow(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      sll(dst.gp(), src.gp(), 0);
      return true;
    case kExprI64UConvertI32:
      TurboAssembler::Dext(dst.gp(), src.gp(), 0, 32);
      return true;
    case kExprI64SConvertF32: {
      LiftoffRegister rounded =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src));
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src, rounded));

      // Real conversion.
      TurboAssembler::Trunc_s_s(rounded.fp(), src.fp());
      trunc_l_s(kScratchDoubleReg, rounded.fp());
      dmfc1(dst.gp(), kScratchDoubleReg);
      // Avoid INT64_MAX as an overflow indicator and use INT64_MIN instead,
      // because INT64_MIN allows easier out-of-bounds detection.
      TurboAssembler::Daddu(kScratchReg, dst.gp(), 1);
      TurboAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      TurboAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      dmtc1(dst.gp(), kScratchDoubleReg);
      cvt_s_l(converted_back.fp(), kScratchDoubleReg);
      TurboAssembler::CompareF32(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI64UConvertF32: {
      // Real conversion.
      TurboAssembler::Trunc_ul_s(dst.gp(), src.fp(), kScratchDoubleReg,
                                 kScratchReg);

      // Checking if trap.
      TurboAssembler::Branch(trap, eq, kScratchReg, Operand(zero_reg));
      return true;
    }
    case kExprI64SConvertF64: {
      LiftoffRegister rounded =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src));
      LiftoffRegister converted_back =
          GetUnusedRegister(kFpReg, LiftoffRegList::ForRegs(src, rounded));

      // Real conversion.
      TurboAssembler::Trunc_d_d(rounded.fp(), src.fp());
      trunc_l_d(kScratchDoubleReg, rounded.fp());
      dmfc1(dst.gp(), kScratchDoubleReg);
      // Avoid INT64_MAX as an overflow indicator and use INT64_MIN instead,
      // because INT64_MIN allows easier out-of-bounds detection.
      TurboAssembler::Daddu(kScratchReg, dst.gp(), 1);
      TurboAssembler::Slt(kScratchReg2, kScratchReg, dst.gp());
      TurboAssembler::Movn(dst.gp(), kScratchReg, kScratchReg2);

      // Checking if trap.
      dmtc1(dst.gp(), kScratchDoubleReg);
      cvt_d_l(converted_back.fp(), kScratchDoubleReg);
      TurboAssembler::CompareF64(EQ, rounded.fp(), converted_back.fp());
      TurboAssembler::BranchFalseF(trap);
      return true;
    }
    case kExprI64UConvertF64: {
      // Real conversion.
      TurboAssembler::Trunc_ul_d(dst.gp(), src.fp(), kScratchDoubleReg,
                                 kScratchReg);

      // Checking if trap.
      TurboAssembler::Branch(trap, eq, kScratchReg, Operand(zero_reg));
      return true;
    }
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

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  BAILOUT("emit_i32_signextend_i8");
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  BAILOUT("emit_i32_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  BAILOUT("emit_i64_signextend_i8");
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  BAILOUT("emit_i64_signextend_i16");
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  BAILOUT("emit_i64_signextend_i32");
}

void LiftoffAssembler::emit_jump(Label* label) {
  TurboAssembler::Branch(label);
}

void LiftoffAssembler::emit_jump(Register target) {
  TurboAssembler::Jump(target);
}

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

namespace liftoff {

inline FPUCondition ConditionToConditionCmpFPU(bool& predicate,
                                               Condition condition) {
  switch (condition) {
    case kEqual:
      predicate = true;
      return EQ;
    case kUnequal:
      predicate = false;
      return EQ;
    case kUnsignedLessThan:
      predicate = true;
      return OLT;
    case kUnsignedGreaterEqual:
      predicate = false;
      return OLT;
    case kUnsignedLessEqual:
      predicate = true;
      return OLE;
    case kUnsignedGreaterThan:
      predicate = false;
      return OLE;
    default:
      predicate = true;
      break;
  }
  UNREACHABLE();
}

};  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Label not_nan, cont;
  TurboAssembler::CompareIsNanF32(lhs, rhs);
  TurboAssembler::BranchFalseF(&not_nan);
  // If one of the operands is NaN, return 1 for f32.ne, else 0.
  if (cond == ne) {
    TurboAssembler::li(dst, 1);
  } else {
    TurboAssembler::Move(dst, zero_reg);
  }
  TurboAssembler::Branch(&cont);

  bind(&not_nan);

  TurboAssembler::li(dst, 1);
  bool predicate;
  FPUCondition fcond = liftoff::ConditionToConditionCmpFPU(predicate, cond);
  TurboAssembler::CompareF32(fcond, lhs, rhs);
  if (predicate) {
    TurboAssembler::LoadZeroIfNotFPUCondition(dst);
  } else {
    TurboAssembler::LoadZeroIfFPUCondition(dst);
  }

  bind(&cont);
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  Label not_nan, cont;
  TurboAssembler::CompareIsNanF64(lhs, rhs);
  TurboAssembler::BranchFalseF(&not_nan);
  // If one of the operands is NaN, return 1 for f64.ne, else 0.
  if (cond == ne) {
    TurboAssembler::li(dst, 1);
  } else {
    TurboAssembler::Move(dst, zero_reg);
  }
  TurboAssembler::Branch(&cont);

  bind(&not_nan);

  TurboAssembler::li(dst, 1);
  bool predicate;
  FPUCondition fcond = liftoff::ConditionToConditionCmpFPU(predicate, cond);
  TurboAssembler::CompareF64(fcond, lhs, rhs);
  if (predicate) {
    TurboAssembler::LoadZeroIfNotFPUCondition(dst);
  } else {
    TurboAssembler::LoadZeroIfFPUCondition(dst);
  }

  bind(&cont);
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  TurboAssembler::Uld(limit_address, MemOperand(limit_address));
  TurboAssembler::Branch(ool_code, ule, sp, Operand(limit_address));
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0, GetUnusedRegister(kGpReg).gp());
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
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

void LiftoffAssembler::CallC(wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  daddiu(sp, sp, -stack_bytes);

  int arg_bytes = 0;
  for (ValueType param_type : sig->parameters()) {
    liftoff::Store(this, sp, arg_bytes, *args++, param_type);
    arg_bytes += ValueTypes::MemSize(param_type);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  // On mips, the first argument is passed in {a0}.
  constexpr Register kFirstArgReg = a0;
  mov(kFirstArgReg, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, kScratchReg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = v0;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_type != kWasmStmt) {
    liftoff::Load(this, *next_result_reg, MemOperand(sp, 0), out_argument_type);
  }

  daddiu(sp, sp, stack_bytes);
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  Call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    pop(kScratchReg);
    Call(kScratchReg);
  } else {
    Call(target);
  }
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  Call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  daddiu(sp, sp, -size);
  TurboAssembler::Move(addr, sp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  daddiu(sp, sp, size);
}

void LiftoffStackSlots::Construct() {
  for (auto& slot : slots_) {
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack:
        asm_->ld(kScratchReg, liftoff::GetStackSlot(slot.src_index_));
        asm_->push(kScratchReg);
        break;
      case LiftoffAssembler::VarState::kRegister:
        liftoff::push(asm_, src.reg(), src.type());
        break;
      case LiftoffAssembler::VarState::KIntConst: {
        asm_->li(kScratchReg, Operand(src.i32_const()));
        asm_->push(kScratchReg);
        break;
      }
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#undef BAILOUT

#endif  // V8_WASM_BASELINE_MIPS64_LIFTOFF_ASSEMBLER_MIPS64_H_
