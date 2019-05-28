// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_
#define V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_

#include "src/wasm/baseline/liftoff-assembler.h"

#include "src/assembler.h"
#include "src/wasm/value-type.h"

namespace v8 {
namespace internal {
namespace wasm {

#define REQUIRE_CPU_FEATURE(name, ...)   \
  if (!CpuFeatures::IsSupported(name)) { \
    bailout("no " #name);                \
    return __VA_ARGS__;                  \
  }                                      \
  CpuFeatureScope feature(this, name);

namespace liftoff {

constexpr Register kScratchRegister2 = r11;
static_assert(kScratchRegister != kScratchRegister2, "collision");
static_assert((kLiftoffAssemblerGpCacheRegs &
               Register::ListOf<kScratchRegister, kScratchRegister2>()) == 0,
              "scratch registers must not be used as cache registers");

constexpr DoubleRegister kScratchDoubleReg2 = xmm14;
static_assert(kScratchDoubleReg != kScratchDoubleReg2, "collision");
static_assert(
    (kLiftoffAssemblerFpCacheRegs &
     DoubleRegister::ListOf<kScratchDoubleReg, kScratchDoubleReg2>()) == 0,
    "scratch registers must not be used as cache registers");

// rbp-8 holds the stack marker, rbp-16 is the instance parameter, first stack
// slot is located at rbp-24.
constexpr int32_t kConstantStackSpace = 16;
constexpr int32_t kFirstStackSlotOffset =
    kConstantStackSpace + LiftoffAssembler::kStackSlotSize;

inline Operand GetStackSlot(uint32_t index) {
  int32_t offset = index * LiftoffAssembler::kStackSlotSize;
  return Operand(rbp, -kFirstStackSlotOffset - offset);
}

// TODO(clemensh): Make this a constexpr variable once Operand is constexpr.
inline Operand GetInstanceOperand() { return Operand(rbp, -16); }

inline Operand GetMemOp(LiftoffAssembler* assm, Register addr, Register offset,
                        uint32_t offset_imm) {
  if (is_uint31(offset_imm)) {
    if (offset == no_reg) return Operand(addr, offset_imm);
    return Operand(addr, offset, times_1, offset_imm);
  }
  // Offset immediate does not fit in 31 bits.
  Register scratch = kScratchRegister;
  assm->movl(scratch, Immediate(offset_imm));
  if (offset != no_reg) {
    assm->addq(scratch, offset);
  }
  return Operand(addr, scratch, times_1, 0);
}

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Operand src,
                 ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->movl(dst.gp(), src);
      break;
    case kWasmI64:
      assm->movq(dst.gp(), src);
      break;
    case kWasmF32:
      assm->Movss(dst.fp(), src);
      break;
    case kWasmF64:
      assm->Movsd(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Operand dst, LiftoffRegister src,
                  ValueType type) {
  switch (type) {
    case kWasmI32:
      assm->movl(dst, src.gp());
      break;
    case kWasmI64:
      assm->movq(dst, src.gp());
      break;
    case kWasmF32:
      assm->Movss(dst, src.fp());
      break;
    case kWasmF64:
      assm->Movsd(dst, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueType type) {
  switch (type) {
    case kWasmI32:
    case kWasmI64:
      assm->pushq(reg.gp());
      break;
    case kWasmF32:
      assm->subq(rsp, Immediate(kSystemPointerSize));
      assm->Movss(Operand(rsp, 0), reg.fp());
      break;
    case kWasmF64:
      assm->subq(rsp, Immediate(kSystemPointerSize));
      assm->Movsd(Operand(rsp, 0), reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

template <typename... Regs>
inline void SpillRegisters(LiftoffAssembler* assm, Regs... regs) {
  for (LiftoffRegister r : {LiftoffRegister(regs)...}) {
    if (assm->cache_state()->is_used(r)) assm->SpillRegister(r);
  }
}

}  // namespace liftoff

int LiftoffAssembler::PrepareStackFrame() {
  int offset = pc_offset();
  sub_sp_32(0);
  return offset;
}

void LiftoffAssembler::PatchPrepareStackFrame(int offset,
                                              uint32_t stack_slots) {
  uint32_t bytes = liftoff::kConstantStackSpace + kStackSlotSize * stack_slots;
  DCHECK_LE(bytes, kMaxInt);
  // We can't run out of space, just pass anything big enough to not cause the
  // assembler to try to grow the buffer.
  constexpr int kAvailableSpace = 64;
  Assembler patching_assembler(
      AssemblerOptions{},
      ExternalAssemblerBuffer(buffer_start_ + offset, kAvailableSpace));
  patching_assembler.sub_sp_32(bytes);
}

void LiftoffAssembler::FinishCode() {}

void LiftoffAssembler::AbortCompilation() {}

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type()) {
    case kWasmI32:
      if (value.to_i32() == 0 && RelocInfo::IsNone(rmode)) {
        xorl(reg.gp(), reg.gp());
      } else {
        movl(reg.gp(), Immediate(value.to_i32(), rmode));
      }
      break;
    case kWasmI64:
      if (RelocInfo::IsNone(rmode)) {
        TurboAssembler::Set(reg.gp(), value.to_i64());
      } else {
        movq(reg.gp(), Immediate64(value.to_i64(), rmode));
      }
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
  movq(dst, liftoff::GetInstanceOperand());
  DCHECK(size == 4 || size == 8);
  if (size == 4) {
    movl(dst, Operand(dst, offset));
  } else {
    movq(dst, Operand(dst, offset));
  }
}

void LiftoffAssembler::LoadTaggedPointerFromInstance(Register dst,
                                                     uint32_t offset) {
  DCHECK_LE(offset, kMaxInt);
  movq(dst, liftoff::GetInstanceOperand());
  LoadTaggedPointerField(dst, Operand(dst, offset));
}

void LiftoffAssembler::SpillInstance(Register instance) {
  movq(liftoff::GetInstanceOperand(), instance);
}

void LiftoffAssembler::FillInstanceInto(Register dst) {
  movq(dst, liftoff::GetInstanceOperand());
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         uint32_t offset_imm,
                                         LiftoffRegList pinned) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);
  LoadTaggedPointerField(dst, src_op);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uint32_t offset_imm,
                            LoadType type, LiftoffRegList pinned,
                            uint32_t* protected_load_pc, bool is_load_mem) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm);
  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      movzxbl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load8S:
      movsxbl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8S:
      movsxbq(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      movzxwl(dst.gp(), src_op);
      break;
    case LoadType::kI32Load16S:
      movsxwl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16S:
      movsxwq(dst.gp(), src_op);
      break;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      movl(dst.gp(), src_op);
      break;
    case LoadType::kI64Load32S:
      movsxlq(dst.gp(), src_op);
      break;
    case LoadType::kI64Load:
      movq(dst.gp(), src_op);
      break;
    case LoadType::kF32Load:
      Movss(dst.fp(), src_op);
      break;
    case LoadType::kF64Load:
      Movsd(dst.fp(), src_op);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uint32_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList /* pinned */,
                             uint32_t* protected_store_pc, bool is_store_mem) {
  if (emit_debug_code() && offset_reg != no_reg) {
    AssertZeroExtended(offset_reg);
  }
  Operand dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI64Store8:
      movb(dst_op, src.gp());
      break;
    case StoreType::kI32Store16:
    case StoreType::kI64Store16:
      movw(dst_op, src.gp());
      break;
    case StoreType::kI32Store:
    case StoreType::kI64Store32:
      movl(dst_op, src.gp());
      break;
    case StoreType::kI64Store:
      movq(dst_op, src.gp());
      break;
    case StoreType::kF32Store:
      Movss(dst_op, src.fp());
      break;
    case StoreType::kF64Store:
      Movsd(dst_op, src.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueType type) {
  Operand src(rbp, kSystemPointerSize * (caller_slot_idx + 1));
  liftoff::Load(this, dst, src, type);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_index, uint32_t src_index,
                                      ValueType type) {
  DCHECK_NE(dst_index, src_index);
  Operand src = liftoff::GetStackSlot(src_index);
  Operand dst = liftoff::GetStackSlot(dst_index);
  if (ValueTypes::ElementSizeLog2Of(type) == 2) {
    movl(kScratchRegister, src);
    movl(dst, kScratchRegister);
  } else {
    DCHECK_EQ(3, ValueTypes::ElementSizeLog2Of(type));
    movq(kScratchRegister, src);
    movq(dst, kScratchRegister);
  }
}

void LiftoffAssembler::Move(Register dst, Register src, ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmI32) {
    movl(dst, src);
  } else {
    DCHECK_EQ(kWasmI64, type);
    movq(dst, src);
  }
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueType type) {
  DCHECK_NE(dst, src);
  if (type == kWasmF32) {
    Movss(dst, src);
  } else {
    DCHECK_EQ(kWasmF64, type);
    Movsd(dst, src);
  }
}

void LiftoffAssembler::Spill(uint32_t index, LiftoffRegister reg,
                             ValueType type) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      movl(dst, reg.gp());
      break;
    case kWasmI64:
      movq(dst, reg.gp());
      break;
    case kWasmF32:
      Movss(dst, reg.fp());
      break;
    case kWasmF64:
      Movsd(dst, reg.fp());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(uint32_t index, WasmValue value) {
  RecordUsedSpillSlot(index);
  Operand dst = liftoff::GetStackSlot(index);
  switch (value.type()) {
    case kWasmI32:
      movl(dst, Immediate(value.to_i32()));
      break;
    case kWasmI64: {
      if (is_int32(value.to_i64())) {
        // Sign extend low word.
        movq(dst, Immediate(static_cast<int32_t>(value.to_i64())));
      } else if (is_uint32(value.to_i64())) {
        // Zero extend low word.
        movl(kScratchRegister, Immediate(static_cast<int32_t>(value.to_i64())));
        movq(dst, kScratchRegister);
      } else {
        movq(kScratchRegister, value.to_i64());
        movq(dst, kScratchRegister);
      }
      break;
    }
    default:
      // We do not track f32 and f64 constants, hence they are unreachable.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, uint32_t index,
                            ValueType type) {
  Operand src = liftoff::GetStackSlot(index);
  switch (type) {
    case kWasmI32:
      movl(reg.gp(), src);
      break;
    case kWasmI64:
      movq(reg.gp(), src);
      break;
    case kWasmF32:
      Movss(reg.fp(), src);
      break;
    case kWasmF64:
      Movsd(reg.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register, uint32_t index, RegPairHalf) {
  UNREACHABLE();
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, Register rhs) {
  if (lhs != dst) {
    leal(dst, Operand(lhs, rhs, times_1, 0));
  } else {
    addl(dst, rhs);
  }
}

void LiftoffAssembler::emit_i32_add(Register dst, Register lhs, int32_t imm) {
  if (lhs != dst) {
    leal(dst, Operand(lhs, imm));
  } else {
    addl(dst, Immediate(imm));
  }
}

void LiftoffAssembler::emit_i32_sub(Register dst, Register lhs, Register rhs) {
  if (dst != rhs) {
    // Default path.
    if (dst != lhs) movl(dst, lhs);
    subl(dst, rhs);
  } else if (lhs == rhs) {
    // Degenerate case.
    xorl(dst, dst);
  } else {
    // Emit {dst = lhs + -rhs} if dst == rhs.
    negl(dst);
    addl(dst, lhs);
  }
}

namespace liftoff {
template <void (Assembler::*op)(Register, Register),
          void (Assembler::*mov)(Register, Register)>
void EmitCommutativeBinOp(LiftoffAssembler* assm, Register dst, Register lhs,
                          Register rhs) {
  if (dst == rhs) {
    (assm->*op)(dst, lhs);
  } else {
    if (dst != lhs) (assm->*mov)(dst, lhs);
    (assm->*op)(dst, rhs);
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imull, &Assembler::movl>(this, dst,
                                                                     lhs, rhs);
}

namespace liftoff {
enum class DivOrRem : uint8_t { kDiv, kRem };
template <typename type, DivOrRem div_or_rem>
void EmitIntDivOrRem(LiftoffAssembler* assm, Register dst, Register lhs,
                     Register rhs, Label* trap_div_by_zero,
                     Label* trap_div_unrepresentable) {
  constexpr bool needs_unrepresentable_check =
      std::is_signed<type>::value && div_or_rem == DivOrRem::kDiv;
  constexpr bool special_case_minus_1 =
      std::is_signed<type>::value && div_or_rem == DivOrRem::kRem;
  DCHECK_EQ(needs_unrepresentable_check, trap_div_unrepresentable != nullptr);

#define iop(name, ...)            \
  do {                            \
    if (sizeof(type) == 4) {      \
      assm->name##l(__VA_ARGS__); \
    } else {                      \
      assm->name##q(__VA_ARGS__); \
    }                             \
  } while (false)

  // For division, the lhs is always taken from {edx:eax}. Thus, make sure that
  // these registers are unused. If {rhs} is stored in one of them, move it to
  // another temporary register.
  // Do all this before any branch, such that the code is executed
  // unconditionally, as the cache state will also be modified unconditionally.
  liftoff::SpillRegisters(assm, rdx, rax);
  if (rhs == rax || rhs == rdx) {
    iop(mov, kScratchRegister, rhs);
    rhs = kScratchRegister;
  }

  // Check for division by zero.
  iop(test, rhs, rhs);
  assm->j(zero, trap_div_by_zero);

  Label done;
  if (needs_unrepresentable_check) {
    // Check for {kMinInt / -1}. This is unrepresentable.
    Label do_div;
    iop(cmp, rhs, Immediate(-1));
    assm->j(not_equal, &do_div);
    // {lhs} is min int if {lhs - 1} overflows.
    iop(cmp, lhs, Immediate(1));
    assm->j(overflow, trap_div_unrepresentable);
    assm->bind(&do_div);
  } else if (special_case_minus_1) {
    // {lhs % -1} is always 0 (needs to be special cased because {kMinInt / -1}
    // cannot be computed).
    Label do_rem;
    iop(cmp, rhs, Immediate(-1));
    assm->j(not_equal, &do_rem);
    // clang-format off
    // (conflicts with presubmit checks because it is confused about "xor")
    iop(xor, dst, dst);
    // clang-format on
    assm->jmp(&done);
    assm->bind(&do_rem);
  }

  // Now move {lhs} into {eax}, then zero-extend or sign-extend into {edx}, then
  // do the division.
  if (lhs != rax) iop(mov, rax, lhs);
  if (std::is_same<int32_t, type>::value) {  // i32
    assm->cdq();
    assm->idivl(rhs);
  } else if (std::is_same<uint32_t, type>::value) {  // u32
    assm->xorl(rdx, rdx);
    assm->divl(rhs);
  } else if (std::is_same<int64_t, type>::value) {  // i64
    assm->cqo();
    assm->idivq(rhs);
  } else {  // u64
    assm->xorq(rdx, rdx);
    assm->divq(rhs);
  }

  // Move back the result (in {eax} or {edx}) into the {dst} register.
  constexpr Register kResultReg = div_or_rem == DivOrRem::kDiv ? rax : rdx;
  if (dst != kResultReg) {
    iop(mov, dst, kResultReg);
  }
  if (special_case_minus_1) assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  liftoff::EmitIntDivOrRem<int32_t, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, trap_div_unrepresentable);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint32_t, liftoff::DivOrRem::kDiv>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<int32_t, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint32_t, liftoff::DivOrRem::kRem>(
      this, dst, lhs, rhs, trap_div_by_zero, nullptr);
}

void LiftoffAssembler::emit_i32_and(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::andl, &Assembler::movl>(this, dst,
                                                                    lhs, rhs);
}

void LiftoffAssembler::emit_i32_or(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::orl, &Assembler::movl>(this, dst,
                                                                   lhs, rhs);
}

void LiftoffAssembler::emit_i32_xor(Register dst, Register lhs, Register rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xorl, &Assembler::movl>(this, dst,
                                                                    lhs, rhs);
}

namespace liftoff {
template <ValueType type>
inline void EmitShiftOperation(LiftoffAssembler* assm, Register dst,
                               Register src, Register amount,
                               void (Assembler::*emit_shift)(Register),
                               LiftoffRegList pinned) {
  // If dst is rcx, compute into the scratch register first, then move to rcx.
  if (dst == rcx) {
    assm->Move(kScratchRegister, src, type);
    if (amount != rcx) assm->Move(rcx, amount, type);
    (assm->*emit_shift)(kScratchRegister);
    assm->Move(rcx, kScratchRegister, type);
    return;
  }

  // Move amount into rcx. If rcx is in use, move its content into the scratch
  // register. If src is rcx, src is now the scratch register.
  bool use_scratch = false;
  if (amount != rcx) {
    use_scratch = src == rcx ||
                  assm->cache_state()->is_used(LiftoffRegister(rcx)) ||
                  pinned.has(LiftoffRegister(rcx));
    if (use_scratch) assm->movq(kScratchRegister, rcx);
    if (src == rcx) src = kScratchRegister;
    assm->Move(rcx, amount, type);
  }

  // Do the actual shift.
  if (dst != src) assm->Move(dst, src, type);
  (assm->*emit_shift)(dst);

  // Restore rcx if needed.
  if (use_scratch) assm->movq(rcx, kScratchRegister);
}
}  // namespace liftoff

void LiftoffAssembler::emit_i32_shl(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI32>(this, dst, src, amount,
                                        &Assembler::shll_cl, pinned);
}

void LiftoffAssembler::emit_i32_sar(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI32>(this, dst, src, amount,
                                        &Assembler::sarl_cl, pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, Register amount,
                                    LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI32>(this, dst, src, amount,
                                        &Assembler::shrl_cl, pinned);
}

void LiftoffAssembler::emit_i32_shr(Register dst, Register src, int amount) {
  if (dst != src) movl(dst, src);
  DCHECK(is_uint5(amount));
  shrl(dst, Immediate(amount));
}

bool LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  testl(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  movl(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get most significant bit set (MSBS).
  bsrl(dst, src);
  // CLZ = 31 - MSBS = MSBS ^ 31.
  xorl(dst, Immediate(31));

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  Label nonzero_input;
  Label continuation;
  testl(src, src);
  j(not_zero, &nonzero_input, Label::kNear);
  movl(dst, Immediate(32));
  jmp(&continuation, Label::kNear);

  bind(&nonzero_input);
  // Get least significant bit set, which equals number of trailing zeros.
  bsfl(dst, src);

  bind(&continuation);
  return true;
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  if (!CpuFeatures::IsSupported(POPCNT)) return false;
  CpuFeatureScope scope(this, POPCNT);
  popcntl(dst, src);
  return true;
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  if (lhs.gp() != dst.gp()) {
    leaq(dst.gp(), Operand(lhs.gp(), rhs.gp(), times_1, 0));
  } else {
    addq(dst.gp(), rhs.gp());
  }
}

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    int32_t imm) {
  if (lhs.gp() != dst.gp()) {
    leaq(dst.gp(), Operand(lhs.gp(), imm));
  } else {
    addq(dst.gp(), Immediate(imm));
  }
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  if (dst.gp() == rhs.gp()) {
    negq(dst.gp());
    addq(dst.gp(), lhs.gp());
  } else {
    if (dst.gp() != lhs.gp()) movq(dst.gp(), lhs.gp());
    subq(dst.gp(), rhs.gp());
  }
}

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::imulq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  liftoff::EmitIntDivOrRem<int64_t, liftoff::DivOrRem::kDiv>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero,
      trap_div_unrepresentable);
  return true;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint64_t, liftoff::DivOrRem::kDiv>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero, nullptr);
  return true;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<int64_t, liftoff::DivOrRem::kRem>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero, nullptr);
  return true;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  liftoff::EmitIntDivOrRem<uint64_t, liftoff::DivOrRem::kRem>(
      this, dst.gp(), lhs.gp(), rhs.gp(), trap_div_by_zero, nullptr);
  return true;
}

void LiftoffAssembler::emit_i64_and(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::andq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_or(LiftoffRegister dst, LiftoffRegister lhs,
                                   LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::orq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_xor(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  liftoff::EmitCommutativeBinOp<&Assembler::xorq, &Assembler::movq>(
      this, dst.gp(), lhs.gp(), rhs.gp());
}

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI64>(this, dst.gp(), src.gp(), amount,
                                        &Assembler::shlq_cl, pinned);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI64>(this, dst.gp(), src.gp(), amount,
                                        &Assembler::sarq_cl, pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount, LiftoffRegList pinned) {
  liftoff::EmitShiftOperation<kWasmI64>(this, dst.gp(), src.gp(), amount,
                                        &Assembler::shrq_cl, pinned);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    int amount) {
  if (dst.gp() != src.gp()) movl(dst.gp(), src.gp());
  DCHECK(is_uint6(amount));
  shrq(dst.gp(), Immediate(amount));
}

void LiftoffAssembler::emit_i32_to_intptr(Register dst, Register src) {
  movsxlq(dst, src);
}

void LiftoffAssembler::emit_f32_add(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vaddss(dst, lhs, rhs);
  } else if (dst == rhs) {
    addss(dst, lhs);
  } else {
    if (dst != lhs) movss(dst, lhs);
    addss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_sub(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsubss(dst, lhs, rhs);
  } else if (dst == rhs) {
    movss(kScratchDoubleReg, rhs);
    movss(dst, lhs);
    subss(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    subss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_mul(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmulss(dst, lhs, rhs);
  } else if (dst == rhs) {
    mulss(dst, lhs);
  } else {
    if (dst != lhs) movss(dst, lhs);
    mulss(dst, rhs);
  }
}

void LiftoffAssembler::emit_f32_div(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vdivss(dst, lhs, rhs);
  } else if (dst == rhs) {
    movss(kScratchDoubleReg, rhs);
    movss(dst, lhs);
    divss(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movss(dst, lhs);
    divss(dst, rhs);
  }
}

namespace liftoff {
enum class MinOrMax : uint8_t { kMin, kMax };
template <typename type>
inline void EmitFloatMinOrMax(LiftoffAssembler* assm, DoubleRegister dst,
                              DoubleRegister lhs, DoubleRegister rhs,
                              MinOrMax min_or_max) {
  Label is_nan;
  Label lhs_below_rhs;
  Label lhs_above_rhs;
  Label done;

#define dop(name, ...)            \
  do {                            \
    if (sizeof(type) == 4) {      \
      assm->name##s(__VA_ARGS__); \
    } else {                      \
      assm->name##d(__VA_ARGS__); \
    }                             \
  } while (false)

  // Check the easy cases first: nan (e.g. unordered), smaller and greater.
  // NaN has to be checked first, because PF=1 implies CF=1.
  dop(Ucomis, lhs, rhs);
  assm->j(parity_even, &is_nan, Label::kNear);   // PF=1
  assm->j(below, &lhs_below_rhs, Label::kNear);  // CF=1
  assm->j(above, &lhs_above_rhs, Label::kNear);  // CF=0 && ZF=0

  // If we get here, then either
  // a) {lhs == rhs},
  // b) {lhs == -0.0} and {rhs == 0.0}, or
  // c) {lhs == 0.0} and {rhs == -0.0}.
  // For a), it does not matter whether we return {lhs} or {rhs}. Check the sign
  // bit of {rhs} to differentiate b) and c).
  dop(Movmskp, kScratchRegister, rhs);
  assm->testl(kScratchRegister, Immediate(1));
  assm->j(zero, &lhs_below_rhs, Label::kNear);
  assm->jmp(&lhs_above_rhs, Label::kNear);

  assm->bind(&is_nan);
  // Create a NaN output.
  dop(Xorp, dst, dst);
  dop(Divs, dst, dst);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_below_rhs);
  DoubleRegister lhs_below_rhs_src = min_or_max == MinOrMax::kMin ? lhs : rhs;
  if (dst != lhs_below_rhs_src) dop(Movs, dst, lhs_below_rhs_src);
  assm->jmp(&done, Label::kNear);

  assm->bind(&lhs_above_rhs);
  DoubleRegister lhs_above_rhs_src = min_or_max == MinOrMax::kMin ? rhs : lhs;
  if (dst != lhs_above_rhs_src) dop(Movs, dst, lhs_above_rhs_src);

  assm->bind(&done);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<float>(this, dst, lhs, rhs,
                                    liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f32_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<float>(this, dst, lhs, rhs,
                                    liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_f32_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  static constexpr int kF32SignBit = 1 << 31;
  Movd(kScratchRegister, lhs);
  andl(kScratchRegister, Immediate(~kF32SignBit));
  Movd(liftoff::kScratchRegister2, rhs);
  andl(liftoff::kScratchRegister2, Immediate(kF32SignBit));
  orl(kScratchRegister, liftoff::kScratchRegister2);
  Movd(dst, kScratchRegister);
}

void LiftoffAssembler::emit_f32_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit - 1);
    Andps(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andps(dst, src);
  }
}

void LiftoffAssembler::emit_f32_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint32_t kSignBit = uint32_t{1} << 31;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit);
    Xorps(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorps(dst, src);
  }
}

bool LiftoffAssembler::emit_f32_ceil(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope feature(this, SSE4_1);
    Roundss(dst, src, kRoundUp);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f32_floor(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope feature(this, SSE4_1);
    Roundss(dst, src, kRoundDown);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f32_trunc(DoubleRegister dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope feature(this, SSE4_1);
    Roundss(dst, src, kRoundToZero);
    return true;
  }
  return false;
}

bool LiftoffAssembler::emit_f32_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope feature(this, SSE4_1);
    Roundss(dst, src, kRoundToNearest);
    return true;
  }
  return false;
}

void LiftoffAssembler::emit_f32_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtss(dst, src);
}

void LiftoffAssembler::emit_f64_add(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vaddsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    addsd(dst, lhs);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    addsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_sub(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsubsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    movsd(kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    subsd(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    subsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_mul(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmulsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    mulsd(dst, lhs);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    mulsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_div(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vdivsd(dst, lhs, rhs);
  } else if (dst == rhs) {
    movsd(kScratchDoubleReg, rhs);
    movsd(dst, lhs);
    divsd(dst, kScratchDoubleReg);
  } else {
    if (dst != lhs) movsd(dst, lhs);
    divsd(dst, rhs);
  }
}

void LiftoffAssembler::emit_f64_min(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMin);
}

void LiftoffAssembler::emit_f64_copysign(DoubleRegister dst, DoubleRegister lhs,
                                         DoubleRegister rhs) {
  // Extract sign bit from {rhs} into {kScratchRegister2}.
  Movq(liftoff::kScratchRegister2, rhs);
  shrq(liftoff::kScratchRegister2, Immediate(63));
  shlq(liftoff::kScratchRegister2, Immediate(63));
  // Reset sign bit of {lhs} (in {kScratchRegister}).
  Movq(kScratchRegister, lhs);
  btrq(kScratchRegister, Immediate(63));
  // Combine both values into {kScratchRegister} and move into {dst}.
  orq(kScratchRegister, liftoff::kScratchRegister2);
  Movq(dst, kScratchRegister);
}

void LiftoffAssembler::emit_f64_max(DoubleRegister dst, DoubleRegister lhs,
                                    DoubleRegister rhs) {
  liftoff::EmitFloatMinOrMax<double>(this, dst, lhs, rhs,
                                     liftoff::MinOrMax::kMax);
}

void LiftoffAssembler::emit_f64_abs(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit - 1);
    Andpd(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit - 1);
    Andpd(dst, src);
  }
}

void LiftoffAssembler::emit_f64_neg(DoubleRegister dst, DoubleRegister src) {
  static constexpr uint64_t kSignBit = uint64_t{1} << 63;
  if (dst == src) {
    TurboAssembler::Move(kScratchDoubleReg, kSignBit);
    Xorpd(dst, kScratchDoubleReg);
  } else {
    TurboAssembler::Move(dst, kSignBit);
    Xorpd(dst, src);
  }
}

bool LiftoffAssembler::emit_f64_ceil(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  Roundsd(dst, src, kRoundUp);
  return true;
}

bool LiftoffAssembler::emit_f64_floor(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  Roundsd(dst, src, kRoundDown);
  return true;
}

bool LiftoffAssembler::emit_f64_trunc(DoubleRegister dst, DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  Roundsd(dst, src, kRoundToZero);
  return true;
}

bool LiftoffAssembler::emit_f64_nearest_int(DoubleRegister dst,
                                            DoubleRegister src) {
  REQUIRE_CPU_FEATURE(SSE4_1, true);
  Roundsd(dst, src, kRoundToNearest);
  return true;
}

void LiftoffAssembler::emit_f64_sqrt(DoubleRegister dst, DoubleRegister src) {
  Sqrtsd(dst, src);
}

namespace liftoff {
// Used for float to int conversions. If the value in {converted_back} equals
// {src} afterwards, the conversion succeeded.
template <typename dst_type, typename src_type>
inline void ConvertFloatToIntAndBack(LiftoffAssembler* assm, Register dst,
                                     DoubleRegister src,
                                     DoubleRegister converted_back) {
  if (std::is_same<double, src_type>::value) {  // f64
    if (std::is_same<int32_t, dst_type>::value) {  // f64 -> i32
      assm->Cvttsd2si(dst, src);
      assm->Cvtlsi2sd(converted_back, dst);
    } else if (std::is_same<uint32_t, dst_type>::value) {  // f64 -> u32
      assm->Cvttsd2siq(dst, src);
      assm->movl(dst, dst);
      assm->Cvtqsi2sd(converted_back, dst);
    } else if (std::is_same<int64_t, dst_type>::value) {  // f64 -> i64
      assm->Cvttsd2siq(dst, src);
      assm->Cvtqsi2sd(converted_back, dst);
    } else {
      UNREACHABLE();
    }
  } else {                                  // f32
    if (std::is_same<int32_t, dst_type>::value) {  // f32 -> i32
      assm->Cvttss2si(dst, src);
      assm->Cvtlsi2ss(converted_back, dst);
    } else if (std::is_same<uint32_t, dst_type>::value) {  // f32 -> u32
      assm->Cvttss2siq(dst, src);
      assm->movl(dst, dst);
      assm->Cvtqsi2ss(converted_back, dst);
    } else if (std::is_same<int64_t, dst_type>::value) {  // f32 -> i64
      assm->Cvttss2siq(dst, src);
      assm->Cvtqsi2ss(converted_back, dst);
    } else {
      UNREACHABLE();
    }
  }
}

template <typename dst_type, typename src_type>
inline bool EmitTruncateFloatToInt(LiftoffAssembler* assm, Register dst,
                                   DoubleRegister src, Label* trap) {
  if (!CpuFeatures::IsSupported(SSE4_1)) {
    assm->bailout("no SSE4.1");
    return true;
  }
  CpuFeatureScope feature(assm, SSE4_1);

  DoubleRegister rounded = kScratchDoubleReg;
  DoubleRegister converted_back = kScratchDoubleReg2;

  if (std::is_same<double, src_type>::value) {  // f64
    assm->Roundsd(rounded, src, kRoundToZero);
  } else {  // f32
    assm->Roundss(rounded, src, kRoundToZero);
  }
  ConvertFloatToIntAndBack<dst_type, src_type>(assm, dst, rounded,
                                               converted_back);
  if (std::is_same<double, src_type>::value) {  // f64
    assm->Ucomisd(converted_back, rounded);
  } else {  // f32
    assm->Ucomiss(converted_back, rounded);
  }

  // Jump to trap if PF is 0 (one of the operands was NaN) or they are not
  // equal.
  assm->j(parity_even, trap);
  assm->j(not_equal, trap);
  return true;
}
}  // namespace liftoff

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      movl(dst.gp(), src.gp());
      return true;
    case kExprI32SConvertF32:
      return liftoff::EmitTruncateFloatToInt<int32_t, float>(this, dst.gp(),
                                                             src.fp(), trap);
    case kExprI32UConvertF32:
      return liftoff::EmitTruncateFloatToInt<uint32_t, float>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI32SConvertF64:
      return liftoff::EmitTruncateFloatToInt<int32_t, double>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI32UConvertF64:
      return liftoff::EmitTruncateFloatToInt<uint32_t, double>(this, dst.gp(),
                                                               src.fp(), trap);
    case kExprI32ReinterpretF32:
      Movd(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      movsxlq(dst.gp(), src.gp());
      return true;
    case kExprI64SConvertF32:
      return liftoff::EmitTruncateFloatToInt<int64_t, float>(this, dst.gp(),
                                                             src.fp(), trap);
    case kExprI64UConvertF32: {
      REQUIRE_CPU_FEATURE(SSE4_1, true);
      Cvttss2uiq(dst.gp(), src.fp(), trap);
      return true;
    }
    case kExprI64SConvertF64:
      return liftoff::EmitTruncateFloatToInt<int64_t, double>(this, dst.gp(),
                                                              src.fp(), trap);
    case kExprI64UConvertF64: {
      REQUIRE_CPU_FEATURE(SSE4_1, true);
      Cvttsd2uiq(dst.gp(), src.fp(), trap);
      return true;
    }
    case kExprI64UConvertI32:
      AssertZeroExtended(src.gp());
      if (dst.gp() != src.gp()) movl(dst.gp(), src.gp());
      return true;
    case kExprI64ReinterpretF64:
      Movq(dst.gp(), src.fp());
      return true;
    case kExprF32SConvertI32:
      Cvtlsi2ss(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI32:
      movl(kScratchRegister, src.gp());
      Cvtqsi2ss(dst.fp(), kScratchRegister);
      return true;
    case kExprF32SConvertI64:
      Cvtqsi2ss(dst.fp(), src.gp());
      return true;
    case kExprF32UConvertI64:
      Cvtqui2ss(dst.fp(), src.gp());
      return true;
    case kExprF32ConvertF64:
      Cvtsd2ss(dst.fp(), src.fp());
      return true;
    case kExprF32ReinterpretI32:
      Movd(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32:
      Cvtlsi2sd(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI32:
      movl(kScratchRegister, src.gp());
      Cvtqsi2sd(dst.fp(), kScratchRegister);
      return true;
    case kExprF64SConvertI64:
      Cvtqsi2sd(dst.fp(), src.gp());
      return true;
    case kExprF64UConvertI64:
      Cvtqui2sd(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      Cvtss2sd(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      Movq(dst.fp(), src.gp());
      return true;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  movsxbl(dst, src);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  movsxwl(dst, src);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  movsxbq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  movsxwq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  movsxlq(dst.gp(), src.gp());
}

void LiftoffAssembler::emit_jump(Label* label) { jmp(label); }

void LiftoffAssembler::emit_jump(Register target) { jmp(target); }

void LiftoffAssembler::emit_cond_jump(Condition cond, Label* label,
                                      ValueType type, Register lhs,
                                      Register rhs) {
  if (rhs != no_reg) {
    switch (type) {
      case kWasmI32:
        cmpl(lhs, rhs);
        break;
      case kWasmI64:
        cmpq(lhs, rhs);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    DCHECK_EQ(type, kWasmI32);
    testl(lhs, lhs);
  }

  j(cond, label);
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  testl(src, src);
  setcc(equal, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_i32_set_cond(Condition cond, Register dst,
                                         Register lhs, Register rhs) {
  cmpl(lhs, rhs);
  setcc(cond, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  testq(src.gp(), src.gp());
  setcc(equal, dst);
  movzxbl(dst, dst);
}

void LiftoffAssembler::emit_i64_set_cond(Condition cond, Register dst,
                                         LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  cmpq(lhs.gp(), rhs.gp());
  setcc(cond, dst);
  movzxbl(dst, dst);
}

namespace liftoff {
template <void (TurboAssembler::*cmp_op)(DoubleRegister, DoubleRegister)>
void EmitFloatSetCond(LiftoffAssembler* assm, Condition cond, Register dst,
                      DoubleRegister lhs, DoubleRegister rhs) {
  Label cont;
  Label not_nan;

  (assm->*cmp_op)(lhs, rhs);
  // If PF is one, one of the operands was NaN. This needs special handling.
  assm->j(parity_odd, &not_nan, Label::kNear);
  // Return 1 for f32.ne, 0 for all other cases.
  if (cond == not_equal) {
    assm->movl(dst, Immediate(1));
  } else {
    assm->xorl(dst, dst);
  }
  assm->jmp(&cont, Label::kNear);
  assm->bind(&not_nan);

  assm->setcc(cond, dst);
  assm->movzxbl(dst, dst);
  assm->bind(&cont);
}
}  // namespace liftoff

void LiftoffAssembler::emit_f32_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&TurboAssembler::Ucomiss>(this, cond, dst, lhs,
                                                      rhs);
}

void LiftoffAssembler::emit_f64_set_cond(Condition cond, Register dst,
                                         DoubleRegister lhs,
                                         DoubleRegister rhs) {
  liftoff::EmitFloatSetCond<&TurboAssembler::Ucomisd>(this, cond, dst, lhs,
                                                      rhs);
}

void LiftoffAssembler::StackCheck(Label* ool_code, Register limit_address) {
  cmpq(rsp, Operand(limit_address, 0));
  j(below_equal, ool_code);
}

void LiftoffAssembler::CallTrapCallbackForTesting() {
  PrepareCallCFunction(0);
  CallCFunction(ExternalReference::wasm_call_trap_callback_for_testing(), 0);
}

void LiftoffAssembler::AssertUnreachable(AbortReason reason) {
  TurboAssembler::AssertUnreachable(reason);
}

void LiftoffAssembler::PushRegisters(LiftoffRegList regs) {
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetFirstRegSet();
    pushq(reg.gp());
    gp_regs.clear(reg);
  }
  LiftoffRegList fp_regs = regs & kFpCacheRegList;
  unsigned num_fp_regs = fp_regs.GetNumRegsSet();
  if (num_fp_regs) {
    subq(rsp, Immediate(num_fp_regs * kStackSlotSize));
    unsigned offset = 0;
    while (!fp_regs.is_empty()) {
      LiftoffRegister reg = fp_regs.GetFirstRegSet();
      Movsd(Operand(rsp, offset), reg.fp());
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
    Movsd(reg.fp(), Operand(rsp, fp_offset));
    fp_regs.clear(reg);
    fp_offset += sizeof(double);
  }
  if (fp_offset) addq(rsp, Immediate(fp_offset));
  LiftoffRegList gp_regs = regs & kGpCacheRegList;
  while (!gp_regs.is_empty()) {
    LiftoffRegister reg = gp_regs.GetLastRegSet();
    popq(reg.gp());
    gp_regs.clear(reg);
  }
}

void LiftoffAssembler::DropStackSlotsAndRet(uint32_t num_stack_slots) {
  DCHECK_LT(num_stack_slots,
            (1 << 16) / kSystemPointerSize);  // 16 bit immediate
  ret(static_cast<int>(num_stack_slots * kSystemPointerSize));
}

void LiftoffAssembler::CallC(wasm::FunctionSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueType out_argument_type, int stack_bytes,
                             ExternalReference ext_ref) {
  subq(rsp, Immediate(stack_bytes));

  int arg_bytes = 0;
  for (ValueType param_type : sig->parameters()) {
    liftoff::Store(this, Operand(rsp, arg_bytes), *args++, param_type);
    arg_bytes += ValueTypes::MemSize(param_type);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  movq(arg_reg_1, rsp);

  constexpr int kNumCCallArgs = 1;

  // Now call the C function.
  PrepareCallCFunction(kNumCCallArgs);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = rax;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_type != kWasmStmt) {
    liftoff::Load(this, *next_result_reg, Operand(rsp, 0), out_argument_type);
  }

  addq(rsp, Immediate(stack_bytes));
}

void LiftoffAssembler::CallNativeWasmCode(Address addr) {
  near_call(addr, RelocInfo::WASM_CALL);
}

void LiftoffAssembler::CallIndirect(wasm::FunctionSig* sig,
                                    compiler::CallDescriptor* call_descriptor,
                                    Register target) {
  if (target == no_reg) {
    popq(kScratchRegister);
    target = kScratchRegister;
  }
  if (FLAG_untrusted_code_mitigations) {
    RetpolineCall(target);
  } else {
    call(target);
  }
}

void LiftoffAssembler::CallRuntimeStub(WasmCode::RuntimeStubId sid) {
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  near_call(static_cast<Address>(sid), RelocInfo::WASM_STUB_CALL);
}

void LiftoffAssembler::AllocateStackSlot(Register addr, uint32_t size) {
  subq(rsp, Immediate(size));
  movq(addr, rsp);
}

void LiftoffAssembler::DeallocateStackSlot(uint32_t size) {
  addq(rsp, Immediate(size));
}

void LiftoffStackSlots::Construct() {
  for (auto& slot : slots_) {
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack:
        if (src.type() == kWasmI32) {
          // Load i32 values to a register first to ensure they are zero
          // extended.
          asm_->movl(kScratchRegister, liftoff::GetStackSlot(slot.src_index_));
          asm_->pushq(kScratchRegister);
        } else {
          // For all other types, just push the whole (8-byte) stack slot.
          // This is also ok for f32 values (even though we copy 4 uninitialized
          // bytes), because f32 and f64 values are clearly distinguished in
          // Turbofan, so the uninitialized bytes are never accessed.
          asm_->pushq(liftoff::GetStackSlot(slot.src_index_));
        }
        break;
      case LiftoffAssembler::VarState::kRegister:
        liftoff::push(asm_, src.reg(), src.type());
        break;
      case LiftoffAssembler::VarState::kIntConst:
        asm_->pushq(Immediate(src.i32_const()));
        break;
    }
  }
}

#undef REQUIRE_CPU_FEATURE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_X64_LIFTOFF_ASSEMBLER_X64_H_
