// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_MACRO_ASSEMBLER_ARM64_INL_H_
#define V8_ARM64_MACRO_ASSEMBLER_ARM64_INL_H_

#include <ctype.h>

#include "src/globals.h"

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/assembler-arm64.h"
#include "src/arm64/instrument-arm64.h"
#include "src/arm64/macro-assembler-arm64.h"


namespace v8 {
namespace internal {


MemOperand FieldMemOperand(Register object, int offset) {
  return MemOperand(object, offset - kHeapObjectTag);
}


MemOperand UntagSmiFieldMemOperand(Register object, int offset) {
  return UntagSmiMemOperand(object, offset - kHeapObjectTag);
}


MemOperand UntagSmiMemOperand(Register object, int offset) {
  // Assumes that Smis are shifted by 32 bits and little endianness.
  STATIC_ASSERT(kSmiShift == 32);
  return MemOperand(object, offset + (kSmiShift / kBitsPerByte));
}


Handle<Object> MacroAssembler::CodeObject() {
  DCHECK(!code_object_.is_null());
  return code_object_;
}


void MacroAssembler::And(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, AND);
}


void MacroAssembler::Ands(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, ANDS);
}


void MacroAssembler::Tst(const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  LogicalMacro(AppropriateZeroRegFor(rn), rn, operand, ANDS);
}


void MacroAssembler::Bic(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, BIC);
}


void MacroAssembler::Bics(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, BICS);
}


void MacroAssembler::Orr(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, ORR);
}


void MacroAssembler::Orn(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, ORN);
}


void MacroAssembler::Eor(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, EOR);
}


void MacroAssembler::Eon(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  LogicalMacro(rd, rn, operand, EON);
}


void MacroAssembler::Ccmp(const Register& rn,
                          const Operand& operand,
                          StatusFlags nzcv,
                          Condition cond) {
  DCHECK(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.ImmediateValue() < 0)) {
    ConditionalCompareMacro(rn, -operand.ImmediateValue(), nzcv, cond, CCMN);
  } else {
    ConditionalCompareMacro(rn, operand, nzcv, cond, CCMP);
  }
}


void MacroAssembler::Ccmn(const Register& rn,
                          const Operand& operand,
                          StatusFlags nzcv,
                          Condition cond) {
  DCHECK(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.ImmediateValue() < 0)) {
    ConditionalCompareMacro(rn, -operand.ImmediateValue(), nzcv, cond, CCMP);
  } else {
    ConditionalCompareMacro(rn, operand, nzcv, cond, CCMN);
  }
}


void MacroAssembler::Add(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.ImmediateValue() < 0) &&
      IsImmAddSub(-operand.ImmediateValue())) {
    AddSubMacro(rd, rn, -operand.ImmediateValue(), LeaveFlags, SUB);
  } else {
    AddSubMacro(rd, rn, operand, LeaveFlags, ADD);
  }
}

void MacroAssembler::Adds(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.ImmediateValue() < 0) &&
      IsImmAddSub(-operand.ImmediateValue())) {
    AddSubMacro(rd, rn, -operand.ImmediateValue(), SetFlags, SUB);
  } else {
    AddSubMacro(rd, rn, operand, SetFlags, ADD);
  }
}


void MacroAssembler::Sub(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.ImmediateValue() < 0) &&
      IsImmAddSub(-operand.ImmediateValue())) {
    AddSubMacro(rd, rn, -operand.ImmediateValue(), LeaveFlags, ADD);
  } else {
    AddSubMacro(rd, rn, operand, LeaveFlags, SUB);
  }
}


void MacroAssembler::Subs(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.ImmediateValue() < 0) &&
      IsImmAddSub(-operand.ImmediateValue())) {
    AddSubMacro(rd, rn, -operand.ImmediateValue(), SetFlags, ADD);
  } else {
    AddSubMacro(rd, rn, operand, SetFlags, SUB);
  }
}


void MacroAssembler::Cmn(const Register& rn, const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  Adds(AppropriateZeroRegFor(rn), rn, operand);
}


void MacroAssembler::Cmp(const Register& rn, const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  Subs(AppropriateZeroRegFor(rn), rn, operand);
}


void MacroAssembler::Neg(const Register& rd,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  if (operand.IsImmediate()) {
    Mov(rd, -operand.ImmediateValue());
  } else {
    Sub(rd, AppropriateZeroRegFor(rd), operand);
  }
}


void MacroAssembler::Negs(const Register& rd,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  Subs(rd, AppropriateZeroRegFor(rd), operand);
}


void MacroAssembler::Adc(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  AddSubWithCarryMacro(rd, rn, operand, LeaveFlags, ADC);
}


void MacroAssembler::Adcs(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  AddSubWithCarryMacro(rd, rn, operand, SetFlags, ADC);
}


void MacroAssembler::Sbc(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  AddSubWithCarryMacro(rd, rn, operand, LeaveFlags, SBC);
}


void MacroAssembler::Sbcs(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  AddSubWithCarryMacro(rd, rn, operand, SetFlags, SBC);
}


void MacroAssembler::Ngc(const Register& rd,
                         const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  Register zr = AppropriateZeroRegFor(rd);
  Sbc(rd, zr, operand);
}


void MacroAssembler::Ngcs(const Register& rd,
                          const Operand& operand) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  Register zr = AppropriateZeroRegFor(rd);
  Sbcs(rd, zr, operand);
}


void MacroAssembler::Mvn(const Register& rd, uint64_t imm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  Mov(rd, ~imm);
}


#define DEFINE_FUNCTION(FN, REGTYPE, REG, OP)                         \
void MacroAssembler::FN(const REGTYPE REG, const MemOperand& addr) {  \
  DCHECK(allow_macro_instructions_);                                  \
  LoadStoreMacro(REG, addr, OP);                                      \
}
LS_MACRO_LIST(DEFINE_FUNCTION)
#undef DEFINE_FUNCTION


#define DEFINE_FUNCTION(FN, REGTYPE, REG, REG2, OP)              \
  void MacroAssembler::FN(const REGTYPE REG, const REGTYPE REG2, \
                          const MemOperand& addr) {              \
    DCHECK(allow_macro_instructions_);                           \
    LoadStorePairMacro(REG, REG2, addr, OP);                     \
  }
LSPAIR_MACRO_LIST(DEFINE_FUNCTION)
#undef DEFINE_FUNCTION


void MacroAssembler::Asr(const Register& rd,
                         const Register& rn,
                         unsigned shift) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  asr(rd, rn, shift);
}


void MacroAssembler::Asr(const Register& rd,
                         const Register& rn,
                         const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  asrv(rd, rn, rm);
}


void MacroAssembler::B(Label* label) {
  b(label);
  CheckVeneerPool(false, false);
}


void MacroAssembler::B(Condition cond, Label* label) {
  DCHECK(allow_macro_instructions_);
  B(label, cond);
}


void MacroAssembler::Bfi(const Register& rd,
                         const Register& rn,
                         unsigned lsb,
                         unsigned width) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  bfi(rd, rn, lsb, width);
}


void MacroAssembler::Bfxil(const Register& rd,
                           const Register& rn,
                           unsigned lsb,
                           unsigned width) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  bfxil(rd, rn, lsb, width);
}


void MacroAssembler::Bind(Label* label) {
  DCHECK(allow_macro_instructions_);
  bind(label);
}


void MacroAssembler::Bl(Label* label) {
  DCHECK(allow_macro_instructions_);
  bl(label);
}


void MacroAssembler::Blr(const Register& xn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!xn.IsZero());
  blr(xn);
}


void MacroAssembler::Br(const Register& xn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!xn.IsZero());
  br(xn);
}


void MacroAssembler::Brk(int code) {
  DCHECK(allow_macro_instructions_);
  brk(code);
}


void MacroAssembler::Cinc(const Register& rd,
                          const Register& rn,
                          Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  cinc(rd, rn, cond);
}


void MacroAssembler::Cinv(const Register& rd,
                          const Register& rn,
                          Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  cinv(rd, rn, cond);
}


void MacroAssembler::Cls(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  cls(rd, rn);
}


void MacroAssembler::Clz(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  clz(rd, rn);
}


void MacroAssembler::Cneg(const Register& rd,
                          const Register& rn,
                          Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  cneg(rd, rn, cond);
}


// Conditionally zero the destination register. Only X registers are supported
// due to the truncation side-effect when used on W registers.
void MacroAssembler::CzeroX(const Register& rd,
                            Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsSP() && rd.Is64Bits());
  DCHECK((cond != al) && (cond != nv));
  csel(rd, xzr, rd, cond);
}


// Conditionally move a value into the destination register. Only X registers
// are supported due to the truncation side-effect when used on W registers.
void MacroAssembler::CmovX(const Register& rd,
                           const Register& rn,
                           Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsSP());
  DCHECK(rd.Is64Bits() && rn.Is64Bits());
  DCHECK((cond != al) && (cond != nv));
  if (!rd.is(rn)) {
    csel(rd, rn, rd, cond);
  }
}


void MacroAssembler::Cset(const Register& rd, Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  cset(rd, cond);
}


void MacroAssembler::Csetm(const Register& rd, Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  csetm(rd, cond);
}


void MacroAssembler::Csinc(const Register& rd,
                           const Register& rn,
                           const Register& rm,
                           Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  csinc(rd, rn, rm, cond);
}


void MacroAssembler::Csinv(const Register& rd,
                           const Register& rn,
                           const Register& rm,
                           Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  csinv(rd, rn, rm, cond);
}


void MacroAssembler::Csneg(const Register& rd,
                           const Register& rn,
                           const Register& rm,
                           Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  csneg(rd, rn, rm, cond);
}


void MacroAssembler::Dmb(BarrierDomain domain, BarrierType type) {
  DCHECK(allow_macro_instructions_);
  dmb(domain, type);
}


void MacroAssembler::Dsb(BarrierDomain domain, BarrierType type) {
  DCHECK(allow_macro_instructions_);
  dsb(domain, type);
}


void MacroAssembler::Debug(const char* message, uint32_t code, Instr params) {
  DCHECK(allow_macro_instructions_);
  debug(message, code, params);
}


void MacroAssembler::Extr(const Register& rd,
                          const Register& rn,
                          const Register& rm,
                          unsigned lsb) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  extr(rd, rn, rm, lsb);
}


void MacroAssembler::Fabs(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  fabs(fd, fn);
}


void MacroAssembler::Fadd(const FPRegister& fd,
                          const FPRegister& fn,
                          const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fadd(fd, fn, fm);
}


void MacroAssembler::Fccmp(const FPRegister& fn,
                           const FPRegister& fm,
                           StatusFlags nzcv,
                           Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK((cond != al) && (cond != nv));
  fccmp(fn, fm, nzcv, cond);
}


void MacroAssembler::Fcmp(const FPRegister& fn, const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fcmp(fn, fm);
}


void MacroAssembler::Fcmp(const FPRegister& fn, double value) {
  DCHECK(allow_macro_instructions_);
  if (value != 0.0) {
    UseScratchRegisterScope temps(this);
    FPRegister tmp = temps.AcquireSameSizeAs(fn);
    Fmov(tmp, value);
    fcmp(fn, tmp);
  } else {
    fcmp(fn, value);
  }
}


void MacroAssembler::Fcsel(const FPRegister& fd,
                           const FPRegister& fn,
                           const FPRegister& fm,
                           Condition cond) {
  DCHECK(allow_macro_instructions_);
  DCHECK((cond != al) && (cond != nv));
  fcsel(fd, fn, fm, cond);
}


void MacroAssembler::Fcvt(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  fcvt(fd, fn);
}


void MacroAssembler::Fcvtas(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtas(rd, fn);
}


void MacroAssembler::Fcvtau(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtau(rd, fn);
}


void MacroAssembler::Fcvtms(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtms(rd, fn);
}


void MacroAssembler::Fcvtmu(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtmu(rd, fn);
}


void MacroAssembler::Fcvtns(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtns(rd, fn);
}


void MacroAssembler::Fcvtnu(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtnu(rd, fn);
}


void MacroAssembler::Fcvtzs(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtzs(rd, fn);
}
void MacroAssembler::Fcvtzu(const Register& rd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fcvtzu(rd, fn);
}


void MacroAssembler::Fdiv(const FPRegister& fd,
                          const FPRegister& fn,
                          const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fdiv(fd, fn, fm);
}


void MacroAssembler::Fmadd(const FPRegister& fd,
                           const FPRegister& fn,
                           const FPRegister& fm,
                           const FPRegister& fa) {
  DCHECK(allow_macro_instructions_);
  fmadd(fd, fn, fm, fa);
}


void MacroAssembler::Fmax(const FPRegister& fd,
                          const FPRegister& fn,
                          const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fmax(fd, fn, fm);
}


void MacroAssembler::Fmaxnm(const FPRegister& fd,
                            const FPRegister& fn,
                            const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fmaxnm(fd, fn, fm);
}


void MacroAssembler::Fmin(const FPRegister& fd,
                          const FPRegister& fn,
                          const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fmin(fd, fn, fm);
}


void MacroAssembler::Fminnm(const FPRegister& fd,
                            const FPRegister& fn,
                            const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fminnm(fd, fn, fm);
}


void MacroAssembler::Fmov(FPRegister fd, FPRegister fn) {
  DCHECK(allow_macro_instructions_);
  // Only emit an instruction if fd and fn are different, and they are both D
  // registers. fmov(s0, s0) is not a no-op because it clears the top word of
  // d0. Technically, fmov(d0, d0) is not a no-op either because it clears the
  // top of q0, but FPRegister does not currently support Q registers.
  if (!fd.Is(fn) || !fd.Is64Bits()) {
    fmov(fd, fn);
  }
}


void MacroAssembler::Fmov(FPRegister fd, Register rn) {
  DCHECK(allow_macro_instructions_);
  fmov(fd, rn);
}


void MacroAssembler::Fmov(FPRegister fd, double imm) {
  DCHECK(allow_macro_instructions_);
  if (fd.Is32Bits()) {
    Fmov(fd, static_cast<float>(imm));
    return;
  }

  DCHECK(fd.Is64Bits());
  if (IsImmFP64(imm)) {
    fmov(fd, imm);
  } else if ((imm == 0.0) && (copysign(1.0, imm) == 1.0)) {
    fmov(fd, xzr);
  } else {
    Ldr(fd, imm);
  }
}


void MacroAssembler::Fmov(FPRegister fd, float imm) {
  DCHECK(allow_macro_instructions_);
  if (fd.Is64Bits()) {
    Fmov(fd, static_cast<double>(imm));
    return;
  }

  DCHECK(fd.Is32Bits());
  if (IsImmFP32(imm)) {
    fmov(fd, imm);
  } else if ((imm == 0.0) && (copysign(1.0, imm) == 1.0)) {
    fmov(fd, wzr);
  } else {
    UseScratchRegisterScope temps(this);
    Register tmp = temps.AcquireW();
    // TODO(all): Use Assembler::ldr(const FPRegister& ft, float imm).
    Mov(tmp, float_to_rawbits(imm));
    Fmov(fd, tmp);
  }
}


void MacroAssembler::Fmov(Register rd, FPRegister fn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  fmov(rd, fn);
}


void MacroAssembler::Fmsub(const FPRegister& fd,
                           const FPRegister& fn,
                           const FPRegister& fm,
                           const FPRegister& fa) {
  DCHECK(allow_macro_instructions_);
  fmsub(fd, fn, fm, fa);
}


void MacroAssembler::Fmul(const FPRegister& fd,
                          const FPRegister& fn,
                          const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fmul(fd, fn, fm);
}


void MacroAssembler::Fneg(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  fneg(fd, fn);
}


void MacroAssembler::Fnmadd(const FPRegister& fd,
                            const FPRegister& fn,
                            const FPRegister& fm,
                            const FPRegister& fa) {
  DCHECK(allow_macro_instructions_);
  fnmadd(fd, fn, fm, fa);
}


void MacroAssembler::Fnmsub(const FPRegister& fd,
                            const FPRegister& fn,
                            const FPRegister& fm,
                            const FPRegister& fa) {
  DCHECK(allow_macro_instructions_);
  fnmsub(fd, fn, fm, fa);
}


void MacroAssembler::Frinta(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  frinta(fd, fn);
}


void MacroAssembler::Frintm(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  frintm(fd, fn);
}


void MacroAssembler::Frintn(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  frintn(fd, fn);
}


void MacroAssembler::Frintz(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  frintz(fd, fn);
}


void MacroAssembler::Fsqrt(const FPRegister& fd, const FPRegister& fn) {
  DCHECK(allow_macro_instructions_);
  fsqrt(fd, fn);
}


void MacroAssembler::Fsub(const FPRegister& fd,
                          const FPRegister& fn,
                          const FPRegister& fm) {
  DCHECK(allow_macro_instructions_);
  fsub(fd, fn, fm);
}


void MacroAssembler::Hint(SystemHint code) {
  DCHECK(allow_macro_instructions_);
  hint(code);
}


void MacroAssembler::Hlt(int code) {
  DCHECK(allow_macro_instructions_);
  hlt(code);
}


void MacroAssembler::Isb() {
  DCHECK(allow_macro_instructions_);
  isb();
}


void MacroAssembler::Ldnp(const CPURegister& rt,
                          const CPURegister& rt2,
                          const MemOperand& src) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!AreAliased(rt, rt2));
  ldnp(rt, rt2, src);
}


void MacroAssembler::Ldr(const CPURegister& rt, const Immediate& imm) {
  DCHECK(allow_macro_instructions_);
  ldr(rt, imm);
}


void MacroAssembler::Ldr(const CPURegister& rt, double imm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(rt.Is64Bits());
  ldr(rt, Immediate(double_to_rawbits(imm)));
}


void MacroAssembler::Lsl(const Register& rd,
                         const Register& rn,
                         unsigned shift) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  lsl(rd, rn, shift);
}


void MacroAssembler::Lsl(const Register& rd,
                         const Register& rn,
                         const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  lslv(rd, rn, rm);
}


void MacroAssembler::Lsr(const Register& rd,
                         const Register& rn,
                         unsigned shift) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  lsr(rd, rn, shift);
}


void MacroAssembler::Lsr(const Register& rd,
                         const Register& rn,
                         const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  lsrv(rd, rn, rm);
}


void MacroAssembler::Madd(const Register& rd,
                          const Register& rn,
                          const Register& rm,
                          const Register& ra) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  madd(rd, rn, rm, ra);
}


void MacroAssembler::Mneg(const Register& rd,
                          const Register& rn,
                          const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  mneg(rd, rn, rm);
}


void MacroAssembler::Mov(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  // Emit a register move only if the registers are distinct, or if they are
  // not X registers. Note that mov(w0, w0) is not a no-op because it clears
  // the top word of x0.
  if (!rd.Is(rn) || !rd.Is64Bits()) {
    Assembler::mov(rd, rn);
  }
}


void MacroAssembler::Movk(const Register& rd, uint64_t imm, int shift) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  movk(rd, imm, shift);
}


void MacroAssembler::Mrs(const Register& rt, SystemRegister sysreg) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rt.IsZero());
  mrs(rt, sysreg);
}


void MacroAssembler::Msr(SystemRegister sysreg, const Register& rt) {
  DCHECK(allow_macro_instructions_);
  msr(sysreg, rt);
}


void MacroAssembler::Msub(const Register& rd,
                          const Register& rn,
                          const Register& rm,
                          const Register& ra) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  msub(rd, rn, rm, ra);
}


void MacroAssembler::Mul(const Register& rd,
                         const Register& rn,
                         const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  mul(rd, rn, rm);
}


void MacroAssembler::Rbit(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  rbit(rd, rn);
}


void MacroAssembler::Ret(const Register& xn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!xn.IsZero());
  ret(xn);
  CheckVeneerPool(false, false);
}


void MacroAssembler::Rev(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  rev(rd, rn);
}


void MacroAssembler::Rev16(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  rev16(rd, rn);
}


void MacroAssembler::Rev32(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  rev32(rd, rn);
}


void MacroAssembler::Ror(const Register& rd,
                         const Register& rs,
                         unsigned shift) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  ror(rd, rs, shift);
}


void MacroAssembler::Ror(const Register& rd,
                         const Register& rn,
                         const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  rorv(rd, rn, rm);
}


void MacroAssembler::Sbfiz(const Register& rd,
                           const Register& rn,
                           unsigned lsb,
                           unsigned width) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  sbfiz(rd, rn, lsb, width);
}


void MacroAssembler::Sbfx(const Register& rd,
                          const Register& rn,
                          unsigned lsb,
                          unsigned width) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  sbfx(rd, rn, lsb, width);
}


void MacroAssembler::Scvtf(const FPRegister& fd,
                           const Register& rn,
                           unsigned fbits) {
  DCHECK(allow_macro_instructions_);
  scvtf(fd, rn, fbits);
}


void MacroAssembler::Sdiv(const Register& rd,
                          const Register& rn,
                          const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  sdiv(rd, rn, rm);
}


void MacroAssembler::Smaddl(const Register& rd,
                            const Register& rn,
                            const Register& rm,
                            const Register& ra) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  smaddl(rd, rn, rm, ra);
}


void MacroAssembler::Smsubl(const Register& rd,
                            const Register& rn,
                            const Register& rm,
                            const Register& ra) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  smsubl(rd, rn, rm, ra);
}


void MacroAssembler::Smull(const Register& rd,
                           const Register& rn,
                           const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  smull(rd, rn, rm);
}


void MacroAssembler::Smulh(const Register& rd,
                           const Register& rn,
                           const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  smulh(rd, rn, rm);
}


void MacroAssembler::Stnp(const CPURegister& rt,
                          const CPURegister& rt2,
                          const MemOperand& dst) {
  DCHECK(allow_macro_instructions_);
  stnp(rt, rt2, dst);
}


void MacroAssembler::Sxtb(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  sxtb(rd, rn);
}


void MacroAssembler::Sxth(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  sxth(rd, rn);
}


void MacroAssembler::Sxtw(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  sxtw(rd, rn);
}


void MacroAssembler::Ubfiz(const Register& rd,
                           const Register& rn,
                           unsigned lsb,
                           unsigned width) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  ubfiz(rd, rn, lsb, width);
}


void MacroAssembler::Ubfx(const Register& rd,
                          const Register& rn,
                          unsigned lsb,
                          unsigned width) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  ubfx(rd, rn, lsb, width);
}


void MacroAssembler::Ucvtf(const FPRegister& fd,
                           const Register& rn,
                           unsigned fbits) {
  DCHECK(allow_macro_instructions_);
  ucvtf(fd, rn, fbits);
}


void MacroAssembler::Udiv(const Register& rd,
                          const Register& rn,
                          const Register& rm) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  udiv(rd, rn, rm);
}


void MacroAssembler::Umaddl(const Register& rd,
                            const Register& rn,
                            const Register& rm,
                            const Register& ra) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  umaddl(rd, rn, rm, ra);
}


void MacroAssembler::Umsubl(const Register& rd,
                            const Register& rn,
                            const Register& rm,
                            const Register& ra) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  umsubl(rd, rn, rm, ra);
}


void MacroAssembler::Uxtb(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  uxtb(rd, rn);
}


void MacroAssembler::Uxth(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  uxth(rd, rn);
}


void MacroAssembler::Uxtw(const Register& rd, const Register& rn) {
  DCHECK(allow_macro_instructions_);
  DCHECK(!rd.IsZero());
  uxtw(rd, rn);
}


void MacroAssembler::BumpSystemStackPointer(const Operand& space) {
  DCHECK(!csp.Is(sp_));
  if (!TmpList()->IsEmpty()) {
    if (CpuFeatures::IsSupported(ALWAYS_ALIGN_CSP)) {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireX();
      Sub(temp, StackPointer(), space);
      Bic(csp, temp, 0xf);
    } else {
      Sub(csp, StackPointer(), space);
    }
  } else {
    // TODO(jbramley): Several callers rely on this not using scratch
    // registers, so we use the assembler directly here. However, this means
    // that large immediate values of 'space' cannot be handled cleanly. (Only
    // 24-bits immediates or values of 'space' that can be encoded in one
    // instruction are accepted.) Once we implement our flexible scratch
    // register idea, we could greatly simplify this function.
    InstructionAccurateScope scope(this);
    DCHECK(space.IsImmediate());
    // Align to 16 bytes.
    uint64_t imm = RoundUp(space.ImmediateValue(), 0x10);
    DCHECK(is_uint24(imm));

    Register source = StackPointer();
    if (CpuFeatures::IsSupported(ALWAYS_ALIGN_CSP)) {
      bic(csp, source, 0xf);
      source = csp;
    }
    if (!is_uint12(imm)) {
      int64_t imm_top_12_bits = imm >> 12;
      sub(csp, source, imm_top_12_bits << 12);
      source = csp;
      imm -= imm_top_12_bits << 12;
    }
    if (imm > 0) {
      sub(csp, source, imm);
    }
  }
  AssertStackConsistency();
}


void MacroAssembler::SyncSystemStackPointer() {
  DCHECK(emit_debug_code());
  DCHECK(!csp.Is(sp_));
  { InstructionAccurateScope scope(this);
    if (CpuFeatures::IsSupported(ALWAYS_ALIGN_CSP)) {
      bic(csp, StackPointer(), 0xf);
    } else {
      mov(csp, StackPointer());
    }
  }
  AssertStackConsistency();
}


void MacroAssembler::InitializeRootRegister() {
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(isolate());
  Mov(root, Operand(roots_array_start));
}


void MacroAssembler::SmiTag(Register dst, Register src) {
  STATIC_ASSERT(kXRegSizeInBits ==
                static_cast<unsigned>(kSmiShift + kSmiValueSize));
  DCHECK(dst.Is64Bits() && src.Is64Bits());
  Lsl(dst, src, kSmiShift);
}


void MacroAssembler::SmiTag(Register smi) { SmiTag(smi, smi); }


void MacroAssembler::SmiUntag(Register dst, Register src) {
  STATIC_ASSERT(kXRegSizeInBits ==
                static_cast<unsigned>(kSmiShift + kSmiValueSize));
  DCHECK(dst.Is64Bits() && src.Is64Bits());
  if (FLAG_enable_slow_asserts) {
    AssertSmi(src);
  }
  Asr(dst, src, kSmiShift);
}


void MacroAssembler::SmiUntag(Register smi) { SmiUntag(smi, smi); }


void MacroAssembler::SmiUntagToDouble(FPRegister dst,
                                      Register src,
                                      UntagMode mode) {
  DCHECK(dst.Is64Bits() && src.Is64Bits());
  if (FLAG_enable_slow_asserts && (mode == kNotSpeculativeUntag)) {
    AssertSmi(src);
  }
  Scvtf(dst, src, kSmiShift);
}


void MacroAssembler::SmiUntagToFloat(FPRegister dst,
                                     Register src,
                                     UntagMode mode) {
  DCHECK(dst.Is32Bits() && src.Is64Bits());
  if (FLAG_enable_slow_asserts && (mode == kNotSpeculativeUntag)) {
    AssertSmi(src);
  }
  Scvtf(dst, src, kSmiShift);
}


void MacroAssembler::SmiTagAndPush(Register src) {
  STATIC_ASSERT((static_cast<unsigned>(kSmiShift) == kWRegSizeInBits) &&
                (static_cast<unsigned>(kSmiValueSize) == kWRegSizeInBits) &&
                (kSmiTag == 0));
  Push(src.W(), wzr);
}


void MacroAssembler::SmiTagAndPush(Register src1, Register src2) {
  STATIC_ASSERT((static_cast<unsigned>(kSmiShift) == kWRegSizeInBits) &&
                (static_cast<unsigned>(kSmiValueSize) == kWRegSizeInBits) &&
                (kSmiTag == 0));
  Push(src1.W(), wzr, src2.W(), wzr);
}


void MacroAssembler::JumpIfSmi(Register value,
                               Label* smi_label,
                               Label* not_smi_label) {
  STATIC_ASSERT((kSmiTagSize == 1) && (kSmiTag == 0));
  // Check if the tag bit is set.
  if (smi_label) {
    Tbz(value, 0, smi_label);
    if (not_smi_label) {
      B(not_smi_label);
    }
  } else {
    DCHECK(not_smi_label);
    Tbnz(value, 0, not_smi_label);
  }
}


void MacroAssembler::JumpIfNotSmi(Register value, Label* not_smi_label) {
  JumpIfSmi(value, NULL, not_smi_label);
}


void MacroAssembler::JumpIfBothSmi(Register value1,
                                   Register value2,
                                   Label* both_smi_label,
                                   Label* not_smi_label) {
  STATIC_ASSERT((kSmiTagSize == 1) && (kSmiTag == 0));
  UseScratchRegisterScope temps(this);
  Register tmp = temps.AcquireX();
  // Check if both tag bits are clear.
  Orr(tmp, value1, value2);
  JumpIfSmi(tmp, both_smi_label, not_smi_label);
}


void MacroAssembler::JumpIfEitherSmi(Register value1,
                                     Register value2,
                                     Label* either_smi_label,
                                     Label* not_smi_label) {
  STATIC_ASSERT((kSmiTagSize == 1) && (kSmiTag == 0));
  UseScratchRegisterScope temps(this);
  Register tmp = temps.AcquireX();
  // Check if either tag bit is clear.
  And(tmp, value1, value2);
  JumpIfSmi(tmp, either_smi_label, not_smi_label);
}


void MacroAssembler::JumpIfEitherNotSmi(Register value1,
                                        Register value2,
                                        Label* not_smi_label) {
  JumpIfBothSmi(value1, value2, NULL, not_smi_label);
}


void MacroAssembler::JumpIfBothNotSmi(Register value1,
                                      Register value2,
                                      Label* not_smi_label) {
  JumpIfEitherSmi(value1, value2, NULL, not_smi_label);
}


void MacroAssembler::ObjectTag(Register tagged_obj, Register obj) {
  STATIC_ASSERT(kHeapObjectTag == 1);
  if (emit_debug_code()) {
    Label ok;
    Tbz(obj, 0, &ok);
    Abort(kObjectTagged);
    Bind(&ok);
  }
  Orr(tagged_obj, obj, kHeapObjectTag);
}


void MacroAssembler::ObjectUntag(Register untagged_obj, Register obj) {
  STATIC_ASSERT(kHeapObjectTag == 1);
  if (emit_debug_code()) {
    Label ok;
    Tbnz(obj, 0, &ok);
    Abort(kObjectNotTagged);
    Bind(&ok);
  }
  Bic(untagged_obj, obj, kHeapObjectTag);
}


void MacroAssembler::IsObjectNameType(Register object,
                                      Register type,
                                      Label* fail) {
  CompareObjectType(object, type, type, LAST_NAME_TYPE);
  B(hi, fail);
}


void MacroAssembler::IsObjectJSObjectType(Register heap_object,
                                          Register map,
                                          Register scratch,
                                          Label* fail) {
  Ldr(map, FieldMemOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map,
                                            Register scratch,
                                            Label* fail) {
  Ldrb(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  // If cmp result is lt, the following ccmp will clear all flags.
  // Z == 0, N == V implies gt condition.
  Cmp(scratch, FIRST_NONCALLABLE_SPEC_OBJECT_TYPE);
  Ccmp(scratch, LAST_NONCALLABLE_SPEC_OBJECT_TYPE, NoFlag, ge);

  // If we didn't get a valid label object just fall through and leave the
  // flags updated.
  if (fail != NULL) {
    B(gt, fail);
  }
}


void MacroAssembler::IsObjectJSStringType(Register object,
                                          Register type,
                                          Label* not_string,
                                          Label* string) {
  Ldr(type, FieldMemOperand(object, HeapObject::kMapOffset));
  Ldrb(type.W(), FieldMemOperand(type, Map::kInstanceTypeOffset));

  STATIC_ASSERT(kStringTag == 0);
  DCHECK((string != NULL) || (not_string != NULL));
  if (string == NULL) {
    TestAndBranchIfAnySet(type.W(), kIsNotStringMask, not_string);
  } else if (not_string == NULL) {
    TestAndBranchIfAllClear(type.W(), kIsNotStringMask, string);
  } else {
    TestAndBranchIfAnySet(type.W(), kIsNotStringMask, not_string);
    B(string);
  }
}


void MacroAssembler::Push(Handle<Object> handle) {
  UseScratchRegisterScope temps(this);
  Register tmp = temps.AcquireX();
  Mov(tmp, Operand(handle));
  Push(tmp);
}


void MacroAssembler::Claim(uint64_t count, uint64_t unit_size) {
  uint64_t size = count * unit_size;

  if (size == 0) {
    return;
  }

  if (csp.Is(StackPointer())) {
    DCHECK(size % 16 == 0);
  } else {
    BumpSystemStackPointer(size);
  }

  Sub(StackPointer(), StackPointer(), size);
}


void MacroAssembler::Claim(const Register& count, uint64_t unit_size) {
  if (unit_size == 0) return;
  DCHECK(IsPowerOf2(unit_size));

  const int shift = CountTrailingZeros(unit_size, kXRegSizeInBits);
  const Operand size(count, LSL, shift);

  if (size.IsZero()) {
    return;
  }

  if (!csp.Is(StackPointer())) {
    BumpSystemStackPointer(size);
  }

  Sub(StackPointer(), StackPointer(), size);
}


void MacroAssembler::ClaimBySMI(const Register& count_smi, uint64_t unit_size) {
  DCHECK(unit_size == 0 || IsPowerOf2(unit_size));
  const int shift = CountTrailingZeros(unit_size, kXRegSizeInBits) - kSmiShift;
  const Operand size(count_smi,
                     (shift >= 0) ? (LSL) : (LSR),
                     (shift >= 0) ? (shift) : (-shift));

  if (size.IsZero()) {
    return;
  }

  if (!csp.Is(StackPointer())) {
    BumpSystemStackPointer(size);
  }

  Sub(StackPointer(), StackPointer(), size);
}


void MacroAssembler::Drop(uint64_t count, uint64_t unit_size) {
  uint64_t size = count * unit_size;

  if (size == 0) {
    return;
  }

  Add(StackPointer(), StackPointer(), size);

  if (csp.Is(StackPointer())) {
    DCHECK(size % 16 == 0);
  } else if (emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    SyncSystemStackPointer();
  }
}


void MacroAssembler::Drop(const Register& count, uint64_t unit_size) {
  if (unit_size == 0) return;
  DCHECK(IsPowerOf2(unit_size));

  const int shift = CountTrailingZeros(unit_size, kXRegSizeInBits);
  const Operand size(count, LSL, shift);

  if (size.IsZero()) {
    return;
  }

  Add(StackPointer(), StackPointer(), size);

  if (!csp.Is(StackPointer()) && emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    SyncSystemStackPointer();
  }
}


void MacroAssembler::DropBySMI(const Register& count_smi, uint64_t unit_size) {
  DCHECK(unit_size == 0 || IsPowerOf2(unit_size));
  const int shift = CountTrailingZeros(unit_size, kXRegSizeInBits) - kSmiShift;
  const Operand size(count_smi,
                     (shift >= 0) ? (LSL) : (LSR),
                     (shift >= 0) ? (shift) : (-shift));

  if (size.IsZero()) {
    return;
  }

  Add(StackPointer(), StackPointer(), size);

  if (!csp.Is(StackPointer()) && emit_debug_code()) {
    // It is safe to leave csp where it is when unwinding the JavaScript stack,
    // but if we keep it matching StackPointer, the simulator can detect memory
    // accesses in the now-free part of the stack.
    SyncSystemStackPointer();
  }
}


void MacroAssembler::CompareAndBranch(const Register& lhs,
                                      const Operand& rhs,
                                      Condition cond,
                                      Label* label) {
  if (rhs.IsImmediate() && (rhs.ImmediateValue() == 0) &&
      ((cond == eq) || (cond == ne))) {
    if (cond == eq) {
      Cbz(lhs, label);
    } else {
      Cbnz(lhs, label);
    }
  } else {
    Cmp(lhs, rhs);
    B(cond, label);
  }
}


void MacroAssembler::TestAndBranchIfAnySet(const Register& reg,
                                           const uint64_t bit_pattern,
                                           Label* label) {
  int bits = reg.SizeInBits();
  DCHECK(CountSetBits(bit_pattern, bits) > 0);
  if (CountSetBits(bit_pattern, bits) == 1) {
    Tbnz(reg, MaskToBit(bit_pattern), label);
  } else {
    Tst(reg, bit_pattern);
    B(ne, label);
  }
}


void MacroAssembler::TestAndBranchIfAllClear(const Register& reg,
                                             const uint64_t bit_pattern,
                                             Label* label) {
  int bits = reg.SizeInBits();
  DCHECK(CountSetBits(bit_pattern, bits) > 0);
  if (CountSetBits(bit_pattern, bits) == 1) {
    Tbz(reg, MaskToBit(bit_pattern), label);
  } else {
    Tst(reg, bit_pattern);
    B(eq, label);
  }
}


void MacroAssembler::InlineData(uint64_t data) {
  DCHECK(is_uint16(data));
  InstructionAccurateScope scope(this, 1);
  movz(xzr, data);
}


void MacroAssembler::EnableInstrumentation() {
  InstructionAccurateScope scope(this, 1);
  movn(xzr, InstrumentStateEnable);
}


void MacroAssembler::DisableInstrumentation() {
  InstructionAccurateScope scope(this, 1);
  movn(xzr, InstrumentStateDisable);
}


void MacroAssembler::AnnotateInstrumentation(const char* marker_name) {
  DCHECK(strlen(marker_name) == 2);

  // We allow only printable characters in the marker names. Unprintable
  // characters are reserved for controlling features of the instrumentation.
  DCHECK(isprint(marker_name[0]) && isprint(marker_name[1]));

  InstructionAccurateScope scope(this, 1);
  movn(xzr, (marker_name[1] << 8) | marker_name[0]);
}

} }  // namespace v8::internal

#endif  // V8_ARM64_MACRO_ASSEMBLER_ARM64_INL_H_
