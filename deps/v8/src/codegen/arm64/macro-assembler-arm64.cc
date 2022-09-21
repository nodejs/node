// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/codegen/assembler.h"
#include "src/codegen/callable.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/external-reference-table.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/reloc-info.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames-inl.h"
#include "src/heap/memory-chunk.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/snapshot.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/arm64/macro-assembler-arm64.h"
#endif

#define __ ACCESS_MASM(masm)

namespace v8 {
namespace internal {

CPURegList TurboAssembler::DefaultTmpList() { return CPURegList(ip0, ip1); }

CPURegList TurboAssembler::DefaultFPTmpList() {
  return CPURegList(fp_scratch1, fp_scratch2);
}

namespace {

// For WebAssembly we care about the full floating point register. If we are not
// running Wasm, we can get away with saving half of those registers.
#if V8_ENABLE_WEBASSEMBLY
constexpr int kStackSavedSavedFPSizeInBits = kQRegSizeInBits;
#else
constexpr int kStackSavedSavedFPSizeInBits = kDRegSizeInBits;
#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace

void TurboAssembler::PushCPURegList(CPURegList registers) {
  // If LR was stored here, we would need to sign it if
  // V8_ENABLE_CONTROL_FLOW_INTEGRITY is on.
  DCHECK(!registers.IncludesAliasOf(lr));

  int size = registers.RegisterSizeInBytes();
  DCHECK_EQ(0, (size * registers.Count()) % 16);

  // Push up to four registers at a time.
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& src0 = registers.PopHighestIndex();
    const CPURegister& src1 = registers.PopHighestIndex();
    const CPURegister& src2 = registers.PopHighestIndex();
    const CPURegister& src3 = registers.PopHighestIndex();
    int count = count_before - registers.Count();
    PushHelper(count, size, src0, src1, src2, src3);
  }
}

void TurboAssembler::PopCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();
  DCHECK_EQ(0, (size * registers.Count()) % 16);

  // If LR was loaded here, we would need to authenticate it if
  // V8_ENABLE_CONTROL_FLOW_INTEGRITY is on.
  DCHECK(!registers.IncludesAliasOf(lr));

  // Pop up to four registers at a time.
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& dst0 = registers.PopLowestIndex();
    const CPURegister& dst1 = registers.PopLowestIndex();
    const CPURegister& dst2 = registers.PopLowestIndex();
    const CPURegister& dst3 = registers.PopLowestIndex();
    int count = count_before - registers.Count();
    PopHelper(count, size, dst0, dst1, dst2, dst3);
  }
}

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion) const {
  auto list = kCallerSaved;
  list.Remove(exclusion);
  list.Align();

  int bytes = list.TotalSizeInBytes();

  if (fp_mode == SaveFPRegsMode::kSave) {
    auto fp_list = CPURegList::GetCallerSavedV(kStackSavedSavedFPSizeInBits);
    DCHECK_EQ(fp_list.Count() % 2, 0);
    bytes += fp_list.TotalSizeInBytes();
  }
  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode,
                                    Register exclusion) {
  ASM_CODE_COMMENT(this);
  auto list = kCallerSaved;
  list.Remove(exclusion);
  list.Align();

  PushCPURegList(list);

  int bytes = list.TotalSizeInBytes();

  if (fp_mode == SaveFPRegsMode::kSave) {
    auto fp_list = CPURegList::GetCallerSavedV(kStackSavedSavedFPSizeInBits);
    DCHECK_EQ(fp_list.Count() % 2, 0);
    PushCPURegList(fp_list);
    bytes += fp_list.TotalSizeInBytes();
  }
  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion) {
  ASM_CODE_COMMENT(this);
  int bytes = 0;
  if (fp_mode == SaveFPRegsMode::kSave) {
    auto fp_list = CPURegList::GetCallerSavedV(kStackSavedSavedFPSizeInBits);
    DCHECK_EQ(fp_list.Count() % 2, 0);
    PopCPURegList(fp_list);
    bytes += fp_list.TotalSizeInBytes();
  }

  auto list = kCallerSaved;
  list.Remove(exclusion);
  list.Align();

  PopCPURegList(list);
  bytes += list.TotalSizeInBytes();

  return bytes;
}

void TurboAssembler::LogicalMacro(const Register& rd, const Register& rn,
                                  const Operand& operand, LogicalOp op) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);

  if (operand.NeedsRelocation(this)) {
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    Logical(rd, rn, temp, op);

  } else if (operand.IsImmediate()) {
    int64_t immediate = operand.ImmediateValue();
    unsigned reg_size = rd.SizeInBits();

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = ~immediate;
    }

    // Ignore the top 32 bits of an immediate if we're moving to a W register.
    if (rd.Is32Bits()) {
      // Check that the top 32 bits are consistent.
      DCHECK(((immediate >> kWRegSizeInBits) == 0) ||
             ((immediate >> kWRegSizeInBits) == -1));
      immediate &= kWRegMask;
    }

    DCHECK(rd.Is64Bits() || is_uint32(immediate));

    // Special cases for all set or all clear immediates.
    if (immediate == 0) {
      switch (op) {
        case AND:
          Mov(rd, 0);
          return;
        case ORR:  // Fall through.
        case EOR:
          Mov(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    } else if ((rd.Is64Bits() && (immediate == -1L)) ||
               (rd.Is32Bits() && (immediate == 0xFFFFFFFFL))) {
      switch (op) {
        case AND:
          Mov(rd, rn);
          return;
        case ORR:
          Mov(rd, immediate);
          return;
        case EOR:
          Mvn(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    }

    unsigned n, imm_s, imm_r;
    if (IsImmLogical(immediate, reg_size, &n, &imm_s, &imm_r)) {
      // Immediate can be encoded in the instruction.
      LogicalImmediate(rd, rn, n, imm_s, imm_r, op);
    } else {
      // Immediate can't be encoded: synthesize using move immediate.
      Register temp = temps.AcquireSameSizeAs(rn);

      // If the left-hand input is the stack pointer, we can't pre-shift the
      // immediate, as the encoding won't allow the subsequent post shift.
      PreShiftImmMode mode = rn == sp ? kNoShift : kAnyShift;
      Operand imm_operand = MoveImmediateForShiftedOp(temp, immediate, mode);

      if (rd.IsSP()) {
        // If rd is the stack pointer we cannot use it as the destination
        // register so we use the temp register as an intermediate again.
        Logical(temp, rn, imm_operand, op);
        Mov(sp, temp);
      } else {
        Logical(rd, rn, imm_operand, op);
      }
    }

  } else if (operand.IsExtendedRegister()) {
    DCHECK(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports shift <= 4. We want to support exactly the
    // same modes here.
    DCHECK_LE(operand.shift_amount(), 4);
    DCHECK(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    Logical(rd, rn, temp, op);

  } else {
    // The operand can be encoded in the instruction.
    DCHECK(operand.IsShiftedRegister());
    Logical(rd, rn, operand, op);
  }
}

void TurboAssembler::Mov(const Register& rd, uint64_t imm) {
  DCHECK(allow_macro_instructions());
  DCHECK(is_uint32(imm) || is_int32(imm) || rd.Is64Bits());
  DCHECK(!rd.IsZero());

  // TODO(all) extend to support more immediates.
  //
  // Immediates on Aarch64 can be produced using an initial value, and zero to
  // three move keep operations.
  //
  // Initial values can be generated with:
  //  1. 64-bit move zero (movz).
  //  2. 32-bit move inverted (movn).
  //  3. 64-bit move inverted.
  //  4. 32-bit orr immediate.
  //  5. 64-bit orr immediate.
  // Move-keep may then be used to modify each of the 16-bit half-words.
  //
  // The code below supports all five initial value generators, and
  // applying move-keep operations to move-zero and move-inverted initial
  // values.

  // Try to move the immediate in one instruction, and if that fails, switch to
  // using multiple instructions.
  if (!TryOneInstrMoveImmediate(rd, imm)) {
    unsigned reg_size = rd.SizeInBits();

    // Generic immediate case. Imm will be represented by
    //   [imm3, imm2, imm1, imm0], where each imm is 16 bits.
    // A move-zero or move-inverted is generated for the first non-zero or
    // non-0xFFFF immX, and a move-keep for subsequent non-zero immX.

    uint64_t ignored_halfword = 0;
    bool invert_move = false;
    // If the number of 0xFFFF halfwords is greater than the number of 0x0000
    // halfwords, it's more efficient to use move-inverted.
    if (CountSetHalfWords(imm, reg_size) > CountSetHalfWords(~imm, reg_size)) {
      ignored_halfword = 0xFFFFL;
      invert_move = true;
    }

    // Mov instructions can't move immediate values into the stack pointer, so
    // set up a temporary register, if needed.
    UseScratchRegisterScope temps(this);
    Register temp = rd.IsSP() ? temps.AcquireSameSizeAs(rd) : rd;

    // Iterate through the halfwords. Use movn/movz for the first non-ignored
    // halfword, and movk for subsequent halfwords.
    DCHECK_EQ(reg_size % 16, 0);
    bool first_mov_done = false;
    for (int i = 0; i < (rd.SizeInBits() / 16); i++) {
      uint64_t imm16 = (imm >> (16 * i)) & 0xFFFFL;
      if (imm16 != ignored_halfword) {
        if (!first_mov_done) {
          if (invert_move) {
            movn(temp, (~imm16) & 0xFFFFL, 16 * i);
          } else {
            movz(temp, imm16, 16 * i);
          }
          first_mov_done = true;
        } else {
          // Construct a wider constant.
          movk(temp, imm16, 16 * i);
        }
      }
    }
    DCHECK(first_mov_done);

    // Move the temporary if the original destination register was the stack
    // pointer.
    if (rd.IsSP()) {
      mov(rd, temp);
    }
  }
}

void TurboAssembler::Mov(const Register& rd, const Operand& operand,
                         DiscardMoveMode discard_mode) {
  DCHECK(allow_macro_instructions());
  DCHECK(!rd.IsZero());

  // Provide a swap register for instructions that need to write into the
  // system stack pointer (and can't do this inherently).
  UseScratchRegisterScope temps(this);
  Register dst = (rd.IsSP()) ? temps.AcquireSameSizeAs(rd) : rd;

  if (operand.NeedsRelocation(this)) {
    // TODO(jgruber,v8:8887): Also consider a root-relative load when generating
    // non-isolate-independent code. In many cases it might be cheaper than
    // embedding the relocatable value.
    if (root_array_available_ && options().isolate_independent_code) {
      if (operand.ImmediateRMode() == RelocInfo::EXTERNAL_REFERENCE) {
        Address addr = static_cast<Address>(operand.ImmediateValue());
        ExternalReference reference = base::bit_cast<ExternalReference>(addr);
        IndirectLoadExternalReference(rd, reference);
        return;
      } else if (RelocInfo::IsEmbeddedObjectMode(operand.ImmediateRMode())) {
        Handle<HeapObject> x(
            reinterpret_cast<Address*>(operand.ImmediateValue()));
        // TODO(v8:9706): Fix-it! This load will always uncompress the value
        // even when we are loading a compressed embedded object.
        IndirectLoadConstant(rd.X(), x);
        return;
      }
    }
    Ldr(dst, operand);
  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(dst, operand.ImmediateValue());
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Emit a shift instruction if moving a shifted register. This operation
    // could also be achieved using an orr instruction (like orn used by Mvn),
    // but using a shift instruction makes the disassembly clearer.
    EmitShift(dst, operand.reg(), operand.shift(), operand.shift_amount());
  } else if (operand.IsExtendedRegister()) {
    // Emit an extend instruction if moving an extended register. This handles
    // extend with post-shift operations, too.
    EmitExtendShift(dst, operand.reg(), operand.extend(),
                    operand.shift_amount());
  } else {
    // Otherwise, emit a register move only if the registers are distinct, or
    // if they are not X registers.
    //
    // Note that mov(w0, w0) is not a no-op because it clears the top word of
    // x0. A flag is provided (kDiscardForSameWReg) if a move between the same W
    // registers is not required to clear the top word of the X register. In
    // this case, the instruction is discarded.
    //
    // If sp is an operand, add #0 is emitted, otherwise, orr #0.
    if (rd != operand.reg() ||
        (rd.Is32Bits() && (discard_mode == kDontDiscardForSameWReg))) {
      Assembler::mov(rd, operand.reg());
    }
    // This case can handle writes into the system stack pointer directly.
    dst = rd;
  }

  // Copy the result to the system stack pointer.
  if (dst != rd) {
    DCHECK(rd.IsSP());
    Assembler::mov(rd, dst);
  }
}

void TurboAssembler::Mov(const Register& rd, Smi smi) {
  return Mov(rd, Operand(smi));
}

void TurboAssembler::Movi16bitHelper(const VRegister& vd, uint64_t imm) {
  DCHECK(is_uint16(imm));
  int byte1 = (imm & 0xFF);
  int byte2 = ((imm >> 8) & 0xFF);
  if (byte1 == byte2) {
    movi(vd.Is64Bits() ? vd.V8B() : vd.V16B(), byte1);
  } else if (byte1 == 0) {
    movi(vd, byte2, LSL, 8);
  } else if (byte2 == 0) {
    movi(vd, byte1);
  } else if (byte1 == 0xFF) {
    mvni(vd, ~byte2 & 0xFF, LSL, 8);
  } else if (byte2 == 0xFF) {
    mvni(vd, ~byte1 & 0xFF);
  } else {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireW();
    movz(temp, imm);
    dup(vd, temp);
  }
}

void TurboAssembler::Movi32bitHelper(const VRegister& vd, uint64_t imm) {
  DCHECK(is_uint32(imm));

  uint8_t bytes[sizeof(imm)];
  memcpy(bytes, &imm, sizeof(imm));

  // All bytes are either 0x00 or 0xFF.
  {
    bool all0orff = true;
    for (int i = 0; i < 4; ++i) {
      if ((bytes[i] != 0) && (bytes[i] != 0xFF)) {
        all0orff = false;
        break;
      }
    }

    if (all0orff == true) {
      movi(vd.Is64Bits() ? vd.V1D() : vd.V2D(), ((imm << 32) | imm));
      return;
    }
  }

  // Of the 4 bytes, only one byte is non-zero.
  for (int i = 0; i < 4; i++) {
    if ((imm & (0xFF << (i * 8))) == imm) {
      movi(vd, bytes[i], LSL, i * 8);
      return;
    }
  }

  // Of the 4 bytes, only one byte is not 0xFF.
  for (int i = 0; i < 4; i++) {
    uint32_t mask = ~(0xFF << (i * 8));
    if ((imm & mask) == mask) {
      mvni(vd, ~bytes[i] & 0xFF, LSL, i * 8);
      return;
    }
  }

  // Immediate is of the form 0x00MMFFFF.
  if ((imm & 0xFF00FFFF) == 0x0000FFFF) {
    movi(vd, bytes[2], MSL, 16);
    return;
  }

  // Immediate is of the form 0x0000MMFF.
  if ((imm & 0xFFFF00FF) == 0x000000FF) {
    movi(vd, bytes[1], MSL, 8);
    return;
  }

  // Immediate is of the form 0xFFMM0000.
  if ((imm & 0xFF00FFFF) == 0xFF000000) {
    mvni(vd, ~bytes[2] & 0xFF, MSL, 16);
    return;
  }
  // Immediate is of the form 0xFFFFMM00.
  if ((imm & 0xFFFF00FF) == 0xFFFF0000) {
    mvni(vd, ~bytes[1] & 0xFF, MSL, 8);
    return;
  }

  // Top and bottom 16-bits are equal.
  if (((imm >> 16) & 0xFFFF) == (imm & 0xFFFF)) {
    Movi16bitHelper(vd.Is64Bits() ? vd.V4H() : vd.V8H(), imm & 0xFFFF);
    return;
  }

  // Default case.
  {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireW();
    Mov(temp, imm);
    dup(vd, temp);
  }
}

void TurboAssembler::Movi64bitHelper(const VRegister& vd, uint64_t imm) {
  // All bytes are either 0x00 or 0xFF.
  {
    bool all0orff = true;
    for (int i = 0; i < 8; ++i) {
      int byteval = (imm >> (i * 8)) & 0xFF;
      if (byteval != 0 && byteval != 0xFF) {
        all0orff = false;
        break;
      }
    }
    if (all0orff == true) {
      movi(vd, imm);
      return;
    }
  }

  // Top and bottom 32-bits are equal.
  if (((imm >> 32) & 0xFFFFFFFF) == (imm & 0xFFFFFFFF)) {
    Movi32bitHelper(vd.Is64Bits() ? vd.V2S() : vd.V4S(), imm & 0xFFFFFFFF);
    return;
  }

  // Default case.
  {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Mov(temp, imm);
    if (vd.Is1D()) {
      mov(vd.D(), 0, temp);
    } else {
      dup(vd.V2D(), temp);
    }
  }
}

void TurboAssembler::Movi(const VRegister& vd, uint64_t imm, Shift shift,
                          int shift_amount) {
  DCHECK(allow_macro_instructions());
  if (shift_amount != 0 || shift != LSL) {
    movi(vd, imm, shift, shift_amount);
  } else if (vd.Is8B() || vd.Is16B()) {
    // 8-bit immediate.
    DCHECK(is_uint8(imm));
    movi(vd, imm);
  } else if (vd.Is4H() || vd.Is8H()) {
    // 16-bit immediate.
    Movi16bitHelper(vd, imm);
  } else if (vd.Is2S() || vd.Is4S()) {
    // 32-bit immediate.
    Movi32bitHelper(vd, imm);
  } else {
    // 64-bit immediate.
    Movi64bitHelper(vd, imm);
  }
}

void TurboAssembler::Movi(const VRegister& vd, uint64_t hi, uint64_t lo) {
  // TODO(v8:11033): Move 128-bit values in a more efficient way.
  DCHECK(vd.Is128Bits());
  Movi(vd.V2D(), lo);
  if (lo != hi) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Mov(temp, hi);
    Ins(vd.V2D(), 1, temp);
  }
}

void TurboAssembler::Mvn(const Register& rd, const Operand& operand) {
  DCHECK(allow_macro_instructions());

  if (operand.NeedsRelocation(this)) {
    Ldr(rd, operand.immediate());
    mvn(rd, rd);

  } else if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(rd, ~operand.ImmediateValue());

  } else if (operand.IsExtendedRegister()) {
    // Emit two instructions for the extend case. This differs from Mov, as
    // the extend and invert can't be achieved in one instruction.
    EmitExtendShift(rd, operand.reg(), operand.extend(),
                    operand.shift_amount());
    mvn(rd, rd);

  } else {
    mvn(rd, operand);
  }
}

unsigned TurboAssembler::CountSetHalfWords(uint64_t imm, unsigned reg_size) {
  DCHECK_EQ(reg_size % 16, 0);

#define HALFWORD(idx) (((imm >> ((idx)*16)) & 0xFFFF) ? 1u : 0u)
  switch (reg_size / 16) {
    case 1:
      return HALFWORD(0);
    case 2:
      return HALFWORD(0) + HALFWORD(1);
    case 4:
      return HALFWORD(0) + HALFWORD(1) + HALFWORD(2) + HALFWORD(3);
  }
#undef HALFWORD
  UNREACHABLE();
}

// The movz instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits clear, eg. 0x00001234, 0x0000123400000000.
bool TurboAssembler::IsImmMovz(uint64_t imm, unsigned reg_size) {
  DCHECK((reg_size == kXRegSizeInBits) || (reg_size == kWRegSizeInBits));
  return CountSetHalfWords(imm, reg_size) <= 1;
}

// The movn instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits set, eg. 0xFFFF1234, 0xFFFF1234FFFFFFFF.
bool TurboAssembler::IsImmMovn(uint64_t imm, unsigned reg_size) {
  return IsImmMovz(~imm, reg_size);
}

void TurboAssembler::ConditionalCompareMacro(const Register& rn,
                                             const Operand& operand,
                                             StatusFlags nzcv, Condition cond,
                                             ConditionalCompareOp op) {
  DCHECK((cond != al) && (cond != nv));
  if (operand.NeedsRelocation(this)) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    ConditionalCompareMacro(rn, temp, nzcv, cond, op);

  } else if ((operand.IsShiftedRegister() && (operand.shift_amount() == 0)) ||
             (operand.IsImmediate() &&
              IsImmConditionalCompare(operand.ImmediateValue()))) {
    // The immediate can be encoded in the instruction, or the operand is an
    // unshifted register: call the assembler.
    ConditionalCompare(rn, operand, nzcv, cond, op);

  } else {
    // The operand isn't directly supported by the instruction: perform the
    // operation on a temporary register.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    ConditionalCompare(rn, temp, nzcv, cond, op);
  }
}

void TurboAssembler::Csel(const Register& rd, const Register& rn,
                          const Operand& operand, Condition cond) {
  DCHECK(allow_macro_instructions());
  DCHECK(!rd.IsZero());
  DCHECK((cond != al) && (cond != nv));
  if (operand.IsImmediate()) {
    // Immediate argument. Handle special cases of 0, 1 and -1 using zero
    // register.
    int64_t imm = operand.ImmediateValue();
    Register zr = AppropriateZeroRegFor(rn);
    if (imm == 0) {
      csel(rd, rn, zr, cond);
    } else if (imm == 1) {
      csinc(rd, rn, zr, cond);
    } else if (imm == -1) {
      csinv(rd, rn, zr, cond);
    } else {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireSameSizeAs(rn);
      Mov(temp, imm);
      csel(rd, rn, temp, cond);
    }
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() == 0)) {
    // Unshifted register argument.
    csel(rd, rn, operand.reg(), cond);
  } else {
    // All other arguments.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    csel(rd, rn, temp, cond);
  }
}

bool TurboAssembler::TryOneInstrMoveImmediate(const Register& dst,
                                              int64_t imm) {
  unsigned n, imm_s, imm_r;
  int reg_size = dst.SizeInBits();
  if (IsImmMovz(imm, reg_size) && !dst.IsSP()) {
    // Immediate can be represented in a move zero instruction. Movz can't write
    // to the stack pointer.
    movz(dst, imm);
    return true;
  } else if (IsImmMovn(imm, reg_size) && !dst.IsSP()) {
    // Immediate can be represented in a move not instruction. Movn can't write
    // to the stack pointer.
    movn(dst, dst.Is64Bits() ? ~imm : (~imm & kWRegMask));
    return true;
  } else if (IsImmLogical(imm, reg_size, &n, &imm_s, &imm_r)) {
    // Immediate can be represented in a logical orr instruction.
    LogicalImmediate(dst, AppropriateZeroRegFor(dst), n, imm_s, imm_r, ORR);
    return true;
  }
  return false;
}

Operand TurboAssembler::MoveImmediateForShiftedOp(const Register& dst,
                                                  int64_t imm,
                                                  PreShiftImmMode mode) {
  int reg_size = dst.SizeInBits();
  // Encode the immediate in a single move instruction, if possible.
  if (TryOneInstrMoveImmediate(dst, imm)) {
    // The move was successful; nothing to do here.
  } else {
    // Pre-shift the immediate to the least-significant bits of the register.
    int shift_low;
    if (reg_size == 64) {
      shift_low = base::bits::CountTrailingZeros(imm);
    } else {
      DCHECK_EQ(reg_size, 32);
      shift_low = base::bits::CountTrailingZeros(static_cast<uint32_t>(imm));
    }

    if (mode == kLimitShiftForSP) {
      // When applied to the stack pointer, the subsequent arithmetic operation
      // can use the extend form to shift left by a maximum of four bits. Right
      // shifts are not allowed, so we filter them out later before the new
      // immediate is tested.
      shift_low = std::min(shift_low, 4);
    }
    int64_t imm_low = imm >> shift_low;

    // Pre-shift the immediate to the most-significant bits of the register. We
    // insert set bits in the least-significant bits, as this creates a
    // different immediate that may be encodable using movn or orr-immediate.
    // If this new immediate is encodable, the set bits will be eliminated by
    // the post shift on the following instruction.
    int shift_high = CountLeadingZeros(imm, reg_size);
    int64_t imm_high = (imm << shift_high) | ((INT64_C(1) << shift_high) - 1);

    if ((mode != kNoShift) && TryOneInstrMoveImmediate(dst, imm_low)) {
      // The new immediate has been moved into the destination's low bits:
      // return a new leftward-shifting operand.
      return Operand(dst, LSL, shift_low);
    } else if ((mode == kAnyShift) && TryOneInstrMoveImmediate(dst, imm_high)) {
      // The new immediate has been moved into the destination's high bits:
      // return a new rightward-shifting operand.
      return Operand(dst, LSR, shift_high);
    } else {
      // Use the generic move operation to set up the immediate.
      Mov(dst, imm);
    }
  }
  return Operand(dst);
}

void TurboAssembler::AddSubMacro(const Register& rd, const Register& rn,
                                 const Operand& operand, FlagsUpdate S,
                                 AddSubOp op) {
  if (operand.IsZero() && rd == rn && rd.Is64Bits() && rn.Is64Bits() &&
      !operand.NeedsRelocation(this) && (S == LeaveFlags)) {
    // The instruction would be a nop. Avoid generating useless code.
    return;
  }

  if (operand.NeedsRelocation(this)) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    AddSubMacro(rd, rn, temp, S, op);
  } else if ((operand.IsImmediate() &&
              !IsImmAddSub(operand.ImmediateValue())) ||
             (rn.IsZero() && !operand.IsShiftedRegister()) ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(rn);
    if (operand.IsImmediate()) {
      PreShiftImmMode mode = kAnyShift;

      // If the destination or source register is the stack pointer, we can
      // only pre-shift the immediate right by values supported in the add/sub
      // extend encoding.
      if (rd == sp) {
        // If the destination is SP and flags will be set, we can't pre-shift
        // the immediate at all.
        mode = (S == SetFlags) ? kNoShift : kLimitShiftForSP;
      } else if (rn == sp) {
        mode = kLimitShiftForSP;
      }

      Operand imm_operand =
          MoveImmediateForShiftedOp(temp, operand.ImmediateValue(), mode);
      AddSub(rd, rn, imm_operand, S, op);
    } else {
      Mov(temp, operand);
      AddSub(rd, rn, temp, S, op);
    }
  } else {
    AddSub(rd, rn, operand, S, op);
  }
}

void TurboAssembler::AddSubWithCarryMacro(const Register& rd,
                                          const Register& rn,
                                          const Operand& operand, FlagsUpdate S,
                                          AddSubWithCarryOp op) {
  DCHECK(rd.SizeInBits() == rn.SizeInBits());
  UseScratchRegisterScope temps(this);

  if (operand.NeedsRelocation(this)) {
    Register temp = temps.AcquireX();
    Ldr(temp, operand.immediate());
    AddSubWithCarryMacro(rd, rn, temp, S, op);

  } else if (operand.IsImmediate() ||
             (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    // Add/sub with carry (immediate or ROR shifted register.)
    Register temp = temps.AcquireSameSizeAs(rn);
    Mov(temp, operand);
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Add/sub with carry (shifted register).
    DCHECK(operand.reg().SizeInBits() == rd.SizeInBits());
    DCHECK(operand.shift() != ROR);
    DCHECK(is_uintn(operand.shift_amount(), rd.SizeInBits() == kXRegSizeInBits
                                                ? kXRegSizeInBitsLog2
                                                : kWRegSizeInBitsLog2));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitShift(temp, operand.reg(), operand.shift(), operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else if (operand.IsExtendedRegister()) {
    // Add/sub with carry (extended register).
    DCHECK(operand.reg().SizeInBits() <= rd.SizeInBits());
    // Add/sub extended supports a shift <= 4. We want to support exactly the
    // same modes.
    DCHECK_LE(operand.shift_amount(), 4);
    DCHECK(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = temps.AcquireSameSizeAs(rn);
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    AddSubWithCarry(rd, rn, temp, S, op);

  } else {
    // The addressing mode is directly supported by the instruction.
    AddSubWithCarry(rd, rn, operand, S, op);
  }
}

void TurboAssembler::LoadStoreMacro(const CPURegister& rt,
                                    const MemOperand& addr, LoadStoreOp op) {
  int64_t offset = addr.offset();
  unsigned size = CalcLSDataSize(op);

  // Check if an immediate offset fits in the immediate field of the
  // appropriate instruction. If not, emit two instructions to perform
  // the operation.
  if (addr.IsImmediateOffset() && !IsImmLSScaled(offset, size) &&
      !IsImmLSUnscaled(offset)) {
    // Immediate offset that can't be encoded using unsigned or unscaled
    // addressing modes.
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireSameSizeAs(addr.base());
    Mov(temp, addr.offset());
    LoadStore(rt, MemOperand(addr.base(), temp), op);
  } else if (addr.IsPostIndex() && !IsImmLSUnscaled(offset)) {
    // Post-index beyond unscaled addressing range.
    LoadStore(rt, MemOperand(addr.base()), op);
    add(addr.base(), addr.base(), offset);
  } else if (addr.IsPreIndex() && !IsImmLSUnscaled(offset)) {
    // Pre-index beyond unscaled addressing range.
    add(addr.base(), addr.base(), offset);
    LoadStore(rt, MemOperand(addr.base()), op);
  } else {
    // Encodable in one load/store instruction.
    LoadStore(rt, addr, op);
  }
}

void TurboAssembler::LoadStorePairMacro(const CPURegister& rt,
                                        const CPURegister& rt2,
                                        const MemOperand& addr,
                                        LoadStorePairOp op) {
  // TODO(all): Should we support register offset for load-store-pair?
  DCHECK(!addr.IsRegisterOffset());

  int64_t offset = addr.offset();
  unsigned size = CalcLSPairDataSize(op);

  // Check if the offset fits in the immediate field of the appropriate
  // instruction. If not, emit two instructions to perform the operation.
  if (IsImmLSPair(offset, size)) {
    // Encodable in one load/store pair instruction.
    LoadStorePair(rt, rt2, addr, op);
  } else {
    Register base = addr.base();
    if (addr.IsImmediateOffset()) {
      UseScratchRegisterScope temps(this);
      Register temp = temps.AcquireSameSizeAs(base);
      Add(temp, base, offset);
      LoadStorePair(rt, rt2, MemOperand(temp), op);
    } else if (addr.IsPostIndex()) {
      LoadStorePair(rt, rt2, MemOperand(base), op);
      Add(base, base, offset);
    } else {
      DCHECK(addr.IsPreIndex());
      Add(base, base, offset);
      LoadStorePair(rt, rt2, MemOperand(base), op);
    }
  }
}

bool TurboAssembler::NeedExtraInstructionsOrRegisterBranch(
    Label* label, ImmBranchType b_type) {
  bool need_longer_range = false;
  // There are two situations in which we care about the offset being out of
  // range:
  //  - The label is bound but too far away.
  //  - The label is not bound but linked, and the previous branch
  //    instruction in the chain is too far away.
  if (label->is_bound() || label->is_linked()) {
    need_longer_range =
        !Instruction::IsValidImmPCOffset(b_type, label->pos() - pc_offset());
  }
  if (!need_longer_range && !label->is_bound()) {
    int max_reachable_pc = pc_offset() + Instruction::ImmBranchRange(b_type);
    unresolved_branches_.insert(std::pair<int, FarBranchInfo>(
        max_reachable_pc, FarBranchInfo(pc_offset(), label)));
    // Also maintain the next pool check.
    next_veneer_pool_check_ = std::min(
        next_veneer_pool_check_, max_reachable_pc - kVeneerDistanceCheckMargin);
  }
  return need_longer_range;
}

void TurboAssembler::Adr(const Register& rd, Label* label, AdrHint hint) {
  DCHECK(allow_macro_instructions());
  DCHECK(!rd.IsZero());

  if (hint == kAdrNear) {
    adr(rd, label);
    return;
  }

  DCHECK_EQ(hint, kAdrFar);
  if (label->is_bound()) {
    int label_offset = label->pos() - pc_offset();
    if (Instruction::IsValidPCRelOffset(label_offset)) {
      adr(rd, label);
    } else {
      DCHECK_LE(label_offset, 0);
      int min_adr_offset = -(1 << (Instruction::ImmPCRelRangeBitwidth - 1));
      adr(rd, min_adr_offset);
      Add(rd, rd, label_offset - min_adr_offset);
    }
  } else {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();

    InstructionAccurateScope scope(this,
                                   PatchingAssembler::kAdrFarPatchableNInstrs);
    adr(rd, label);
    for (int i = 0; i < PatchingAssembler::kAdrFarPatchableNNops; ++i) {
      nop(ADR_FAR_NOP);
    }
    movz(scratch, 0);
  }
}

void TurboAssembler::B(Label* label, BranchType type, Register reg, int bit) {
  DCHECK((reg == NoReg || type >= kBranchTypeFirstUsingReg) &&
         (bit == -1 || type >= kBranchTypeFirstUsingBit));
  if (kBranchTypeFirstCondition <= type && type <= kBranchTypeLastCondition) {
    B(static_cast<Condition>(type), label);
  } else {
    switch (type) {
      case always:
        B(label);
        break;
      case never:
        break;
      case reg_zero:
        Cbz(reg, label);
        break;
      case reg_not_zero:
        Cbnz(reg, label);
        break;
      case reg_bit_clear:
        Tbz(reg, bit, label);
        break;
      case reg_bit_set:
        Tbnz(reg, bit, label);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void TurboAssembler::B(Label* label, Condition cond) {
  DCHECK(allow_macro_instructions());
  DCHECK((cond != al) && (cond != nv));

  Label done;
  bool need_extra_instructions =
      NeedExtraInstructionsOrRegisterBranch(label, CondBranchType);

  if (need_extra_instructions) {
    b(&done, NegateCondition(cond));
    B(label);
  } else {
    b(label, cond);
  }
  bind(&done);
}

void TurboAssembler::Tbnz(const Register& rt, unsigned bit_pos, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
      NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbz(rt, bit_pos, &done);
    B(label);
  } else {
    tbnz(rt, bit_pos, label);
  }
  bind(&done);
}

void TurboAssembler::Tbz(const Register& rt, unsigned bit_pos, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
      NeedExtraInstructionsOrRegisterBranch(label, TestBranchType);

  if (need_extra_instructions) {
    tbnz(rt, bit_pos, &done);
    B(label);
  } else {
    tbz(rt, bit_pos, label);
  }
  bind(&done);
}

void TurboAssembler::Cbnz(const Register& rt, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
      NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbz(rt, &done);
    B(label);
  } else {
    cbnz(rt, label);
  }
  bind(&done);
}

void TurboAssembler::Cbz(const Register& rt, Label* label) {
  DCHECK(allow_macro_instructions());

  Label done;
  bool need_extra_instructions =
      NeedExtraInstructionsOrRegisterBranch(label, CompareBranchType);

  if (need_extra_instructions) {
    cbnz(rt, &done);
    B(label);
  } else {
    cbz(rt, label);
  }
  bind(&done);
}

// Pseudo-instructions.

void TurboAssembler::Abs(const Register& rd, const Register& rm,
                         Label* is_not_representable, Label* is_representable) {
  DCHECK(allow_macro_instructions());
  DCHECK(AreSameSizeAndType(rd, rm));

  Cmp(rm, 1);
  Cneg(rd, rm, lt);

  // If the comparison sets the v flag, the input was the smallest value
  // representable by rm, and the mathematical result of abs(rm) is not
  // representable using two's complement.
  if ((is_not_representable != nullptr) && (is_representable != nullptr)) {
    B(is_not_representable, vs);
    B(is_representable);
  } else if (is_not_representable != nullptr) {
    B(is_not_representable, vs);
  } else if (is_representable != nullptr) {
    B(is_representable, vc);
  }
}

// Abstracted stack operations.

void TurboAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3,
                          const CPURegister& src4, const CPURegister& src5,
                          const CPURegister& src6, const CPURegister& src7) {
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3, src4, src5, src6, src7));

  int count = 5 + src5.is_valid() + src6.is_valid() + src6.is_valid();
  int size = src0.SizeInBytes();
  DCHECK_EQ(0, (size * count) % 16);

  PushHelper(4, size, src0, src1, src2, src3);
  PushHelper(count - 4, size, src4, src5, src6, src7);
}

void TurboAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3,
                         const CPURegister& dst4, const CPURegister& dst5,
                         const CPURegister& dst6, const CPURegister& dst7) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  DCHECK(!AreAliased(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7));
  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7));
  DCHECK(dst0.is_valid());

  int count = 5 + dst5.is_valid() + dst6.is_valid() + dst7.is_valid();
  int size = dst0.SizeInBytes();
  DCHECK_EQ(0, (size * count) % 16);

  PopHelper(4, size, dst0, dst1, dst2, dst3);
  PopHelper(count - 4, size, dst4, dst5, dst6, dst7);
}

void MacroAssembler::PushMultipleTimes(CPURegister src, Register count) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireSameSizeAs(count);

  Label loop, leftover2, leftover1, done;

  Subs(temp, count, 4);
  B(mi, &leftover2);

  // Push groups of four first.
  Bind(&loop);
  Subs(temp, temp, 4);
  PushHelper(4, src.SizeInBytes(), src, src, src, src);
  B(pl, &loop);

  // Push groups of two.
  Bind(&leftover2);
  Tbz(count, 1, &leftover1);
  PushHelper(2, src.SizeInBytes(), src, src, NoReg, NoReg);

  // Push the last one (if required).
  Bind(&leftover1);
  Tbz(count, 0, &done);
  PushHelper(1, src.SizeInBytes(), src, NoReg, NoReg, NoReg);

  Bind(&done);
}

void TurboAssembler::PushHelper(int count, int size, const CPURegister& src0,
                                const CPURegister& src1,
                                const CPURegister& src2,
                                const CPURegister& src3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  DCHECK(AreSameSizeAndType(src0, src1, src2, src3));
  DCHECK(size == src0.SizeInBytes());

  // When pushing multiple registers, the store order is chosen such that
  // Push(a, b) is equivalent to Push(a) followed by Push(b).
  switch (count) {
    case 1:
      DCHECK(src1.IsNone() && src2.IsNone() && src3.IsNone());
      str(src0, MemOperand(sp, -1 * size, PreIndex));
      break;
    case 2:
      DCHECK(src2.IsNone() && src3.IsNone());
      stp(src1, src0, MemOperand(sp, -2 * size, PreIndex));
      break;
    case 3:
      DCHECK(src3.IsNone());
      stp(src2, src1, MemOperand(sp, -3 * size, PreIndex));
      str(src0, MemOperand(sp, 2 * size));
      break;
    case 4:
      // Skip over 4 * size, then fill in the gap. This allows four W registers
      // to be pushed using sp, whilst maintaining 16-byte alignment for sp
      // at all times.
      stp(src3, src2, MemOperand(sp, -4 * size, PreIndex));
      stp(src1, src0, MemOperand(sp, 2 * size));
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::PopHelper(int count, int size, const CPURegister& dst0,
                               const CPURegister& dst1, const CPURegister& dst2,
                               const CPURegister& dst3) {
  // Ensure that we don't unintentially modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  DCHECK(size == dst0.SizeInBytes());

  // When popping multiple registers, the load order is chosen such that
  // Pop(a, b) is equivalent to Pop(a) followed by Pop(b).
  switch (count) {
    case 1:
      DCHECK(dst1.IsNone() && dst2.IsNone() && dst3.IsNone());
      ldr(dst0, MemOperand(sp, 1 * size, PostIndex));
      break;
    case 2:
      DCHECK(dst2.IsNone() && dst3.IsNone());
      ldp(dst0, dst1, MemOperand(sp, 2 * size, PostIndex));
      break;
    case 3:
      DCHECK(dst3.IsNone());
      ldr(dst2, MemOperand(sp, 2 * size));
      ldp(dst0, dst1, MemOperand(sp, 3 * size, PostIndex));
      break;
    case 4:
      // Load the higher addresses first, then load the lower addresses and
      // skip the whole block in the second instruction. This allows four W
      // registers to be popped using sp, whilst maintaining 16-byte alignment
      // for sp at all times.
      ldp(dst2, dst3, MemOperand(sp, 2 * size));
      ldp(dst0, dst1, MemOperand(sp, 4 * size, PostIndex));
      break;
    default:
      UNREACHABLE();
  }
}

void TurboAssembler::PokePair(const CPURegister& src1, const CPURegister& src2,
                              int offset) {
  DCHECK(AreSameSizeAndType(src1, src2));
  DCHECK((offset >= 0) && ((offset % src1.SizeInBytes()) == 0));
  Stp(src1, src2, MemOperand(sp, offset));
}

void MacroAssembler::PeekPair(const CPURegister& dst1, const CPURegister& dst2,
                              int offset) {
  DCHECK(AreSameSizeAndType(dst1, dst2));
  DCHECK((offset >= 0) && ((offset % dst1.SizeInBytes()) == 0));
  Ldp(dst1, dst2, MemOperand(sp, offset));
}

void MacroAssembler::PushCalleeSavedRegisters() {
  ASM_CODE_COMMENT(this);
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  MemOperand tos(sp, -2 * static_cast<int>(kXRegSize), PreIndex);

  stp(d14, d15, tos);
  stp(d12, d13, tos);
  stp(d10, d11, tos);
  stp(d8, d9, tos);

  stp(x27, x28, tos);
  stp(x25, x26, tos);
  stp(x23, x24, tos);
  stp(x21, x22, tos);
  stp(x19, x20, tos);

  static_assert(
      EntryFrameConstants::kCalleeSavedRegisterBytesPushedBeforeFpLrPair ==
      18 * kSystemPointerSize);

#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
    // Use the stack pointer's value immediately before pushing the LR as the
    // context for signing it. This is what the StackFrameIterator expects.
    pacibsp();
#endif

    stp(x29, x30, tos);  // fp, lr

    static_assert(
        EntryFrameConstants::kCalleeSavedRegisterBytesPushedAfterFpLrPair == 0);
}

void MacroAssembler::PopCalleeSavedRegisters() {
  ASM_CODE_COMMENT(this);
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  MemOperand tos(sp, 2 * kXRegSize, PostIndex);

  ldp(x29, x30, tos);  // fp, lr

#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
                       // The context (stack pointer value) for authenticating
                       // the LR here must
  // match the one used for signing it (see `PushCalleeSavedRegisters`).
  autibsp();
#endif

    ldp(x19, x20, tos);
    ldp(x21, x22, tos);
    ldp(x23, x24, tos);
    ldp(x25, x26, tos);
    ldp(x27, x28, tos);

    ldp(d8, d9, tos);
    ldp(d10, d11, tos);
    ldp(d12, d13, tos);
    ldp(d14, d15, tos);
}

namespace {

void TailCallOptimizedCodeSlot(MacroAssembler* masm,
                               Register optimized_code_entry,
                               Register scratch) {
  // ----------- S t a t e -------------
  //  -- x0 : actual argument count
  //  -- x3 : new target (preserved for callee if needed, and caller)
  //  -- x1 : target function (preserved for callee if needed, and caller)
  // -----------------------------------
  ASM_CODE_COMMENT(masm);
  DCHECK(!AreAliased(x1, x3, optimized_code_entry, scratch));

  Register closure = x1;
  Label heal_optimized_code_slot;

  // If the optimized code is cleared, go to runtime to update the optimization
  // marker field.
  __ LoadWeakValue(optimized_code_entry, optimized_code_entry,
                   &heal_optimized_code_slot);

  // Check if the optimized code is marked for deopt. If it is, call the
  // runtime to clear it.
  __ AssertCodeT(optimized_code_entry);
  __ JumpIfCodeTIsMarkedForDeoptimization(optimized_code_entry, scratch,
                                          &heal_optimized_code_slot);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  __ ReplaceClosureCodeWithOptimizedCode(optimized_code_entry, closure);
  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  __ Move(x2, optimized_code_entry);
  __ JumpCodeTObject(x2);

  // Optimized code slot contains deoptimized code or code is cleared and
  // optimized code marker isn't updated. Evict the code, update the marker
  // and re-enter the closure's code.
  __ bind(&heal_optimized_code_slot);
  __ GenerateTailCallToReturnedCode(Runtime::kHealOptimizedCodeSlot);
}

}  // namespace

#ifdef V8_ENABLE_DEBUG_CODE
void MacroAssembler::AssertFeedbackVector(Register object, Register scratch) {
  if (v8_flags.debug_code) {
    CompareObjectType(object, scratch, scratch, FEEDBACK_VECTOR_TYPE);
    Assert(eq, AbortReason::kExpectedFeedbackVector);
  }
}
#endif  // V8_ENABLE_DEBUG_CODE

void MacroAssembler::ReplaceClosureCodeWithOptimizedCode(
    Register optimized_code, Register closure) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(optimized_code, closure));
  // Store code entry in the closure.
  AssertCodeT(optimized_code);
  StoreTaggedField(optimized_code,
                   FieldMemOperand(closure, JSFunction::kCodeOffset));
  RecordWriteField(closure, JSFunction::kCodeOffset, optimized_code,
                   kLRHasNotBeenSaved, SaveFPRegsMode::kIgnore,
                   SmiCheck::kOmit);
}

void MacroAssembler::GenerateTailCallToReturnedCode(
    Runtime::FunctionId function_id) {
  ASM_CODE_COMMENT(this);
  // ----------- S t a t e -------------
  //  -- x0 : actual argument count
  //  -- x1 : target function (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(this, StackFrame::INTERNAL);
    // Push a copy of the target function, the new target and the actual
    // argument count.
    SmiTag(kJavaScriptCallArgCountRegister);
    Push(kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
         kJavaScriptCallArgCountRegister, padreg);
    // Push another copy as a parameter to the runtime call.
    PushArgument(kJavaScriptCallTargetRegister);

    CallRuntime(function_id, 1);
    Mov(x2, x0);

    // Restore target function, new target and actual argument count.
    Pop(padreg, kJavaScriptCallArgCountRegister,
        kJavaScriptCallNewTargetRegister, kJavaScriptCallTargetRegister);
    SmiUntag(kJavaScriptCallArgCountRegister);
  }

  static_assert(kJavaScriptCallCodeStartRegister == x2, "ABI mismatch");
  JumpCodeTObject(x2);
}

// Read off the flags in the feedback vector and check if there
// is optimized code or a tiering state that needs to be processed.
void MacroAssembler::LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
    Register flags, Register feedback_vector, CodeKind current_code_kind,
    Label* flags_need_processing) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(flags, feedback_vector));
  DCHECK(CodeKindCanTierUp(current_code_kind));
  Ldrh(flags, FieldMemOperand(feedback_vector, FeedbackVector::kFlagsOffset));
  uint32_t kFlagsMask = FeedbackVector::kFlagsTieringStateIsAnyRequested |
                        FeedbackVector::kFlagsMaybeHasTurbofanCode |
                        FeedbackVector::kFlagsLogNextExecution;
  if (current_code_kind != CodeKind::MAGLEV) {
    kFlagsMask |= FeedbackVector::kFlagsMaybeHasMaglevCode;
  }
  TestAndBranchIfAnySet(flags, kFlagsMask, flags_need_processing);
}

void MacroAssembler::MaybeOptimizeCodeOrTailCallOptimizedCodeSlot(
    Register flags, Register feedback_vector) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(flags, feedback_vector));
  Label maybe_has_optimized_code, maybe_needs_logging;
  // Check if optimized code is available.
  TestAndBranchIfAllClear(flags,
                          FeedbackVector::kFlagsTieringStateIsAnyRequested,
                          &maybe_needs_logging);
  GenerateTailCallToReturnedCode(Runtime::kCompileOptimized);

  bind(&maybe_needs_logging);
  TestAndBranchIfAllClear(flags, FeedbackVector::LogNextExecutionBit::kMask,
                          &maybe_has_optimized_code);
  GenerateTailCallToReturnedCode(Runtime::kFunctionLogNextExecution);

  bind(&maybe_has_optimized_code);
  Register optimized_code_entry = x7;
  LoadAnyTaggedField(
      optimized_code_entry,
      FieldMemOperand(feedback_vector,
                      FeedbackVector::kMaybeOptimizedCodeOffset));
  TailCallOptimizedCodeSlot(this, optimized_code_entry, x4);
}

#ifdef V8_ENABLE_DEBUG_CODE
void TurboAssembler::AssertSpAligned() {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  HardAbortScope hard_abort(this);  // Avoid calls to Abort.
  // Arm64 requires the stack pointer to be 16-byte aligned prior to address
  // calculation.
  UseScratchRegisterScope scope(this);
  Register temp = scope.AcquireX();
  Mov(temp, sp);
  Tst(temp, 15);
  Check(eq, AbortReason::kUnexpectedStackPointer);
}

void TurboAssembler::AssertFPCRState(Register fpcr) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  Label unexpected_mode, done;
  UseScratchRegisterScope temps(this);
  if (fpcr.IsNone()) {
    fpcr = temps.AcquireX();
    Mrs(fpcr, FPCR);
  }

  // Settings left to their default values:
  //   - Assert that flush-to-zero is not set.
  Tbnz(fpcr, FZ_offset, &unexpected_mode);
  //   - Assert that the rounding mode is nearest-with-ties-to-even.
  static_assert(FPTieEven == 0);
  Tst(fpcr, RMode_mask);
  B(eq, &done);

  Bind(&unexpected_mode);
  Abort(AbortReason::kUnexpectedFPCRMode);

  Bind(&done);
}

void TurboAssembler::AssertSmi(Register object, AbortReason reason) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  static_assert(kSmiTag == 0);
  Tst(object, kSmiTagMask);
  Check(eq, reason);
}

void MacroAssembler::AssertNotSmi(Register object, AbortReason reason) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  static_assert(kSmiTag == 0);
  Tst(object, kSmiTagMask);
  Check(ne, reason);
}

void MacroAssembler::AssertCodeT(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  AssertNotSmi(object, AbortReason::kOperandIsNotACodeT);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();

  CompareObjectType(object, temp, temp, CODET_TYPE);
  Check(eq, AbortReason::kOperandIsNotACodeT);
}

void MacroAssembler::AssertConstructor(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAConstructor);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();

  LoadMap(temp, object);
  Ldrb(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  Tst(temp, Operand(Map::Bits1::IsConstructorBit::kMask));

  Check(ne, AbortReason::kOperandIsNotAConstructor);
}

void MacroAssembler::AssertFunction(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAFunction);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  LoadMap(temp, object);
  CompareInstanceTypeRange(temp, temp, FIRST_JS_FUNCTION_TYPE,
                           LAST_JS_FUNCTION_TYPE);
  Check(ls, AbortReason::kOperandIsNotAFunction);
}

void MacroAssembler::AssertCallableFunction(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAFunction);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  LoadMap(temp, object);
  CompareInstanceTypeRange(temp, temp, FIRST_CALLABLE_JS_FUNCTION_TYPE,
                           LAST_CALLABLE_JS_FUNCTION_TYPE);
  Check(ls, AbortReason::kOperandIsNotACallableFunction);
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotABoundFunction);

  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();

  CompareObjectType(object, temp, temp, JS_BOUND_FUNCTION_TYPE);
  Check(eq, AbortReason::kOperandIsNotABoundFunction);
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  LoadMap(temp, object);

  // Load instance type and check if JSGeneratorObject
  CompareInstanceTypeRange(temp, temp, FIRST_JS_GENERATOR_OBJECT_TYPE,
                           LAST_JS_GENERATOR_OBJECT_TYPE);
  // Restore generator object to register and perform assertion
  Check(ls, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  Label done_checking;
  AssertNotSmi(object);
  JumpIfRoot(object, RootIndex::kUndefinedValue, &done_checking);
  LoadMap(scratch, object);
  CompareInstanceType(scratch, scratch, ALLOCATION_SITE_TYPE);
  Assert(eq, AbortReason::kExpectedUndefinedOrCell);
  Bind(&done_checking);
}

void TurboAssembler::AssertPositiveOrZero(Register value) {
  if (!v8_flags.debug_code) return;
  ASM_CODE_COMMENT(this);
  Label done;
  int sign_bit = value.Is64Bits() ? kXSignBit : kWSignBit;
  Tbz(value, sign_bit, &done);
  Abort(AbortReason::kUnexpectedNegativeValue);
  Bind(&done);
}

void TurboAssembler::Assert(Condition cond, AbortReason reason) {
  if (v8_flags.debug_code) {
    Check(cond, reason);
  }
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (v8_flags.debug_code) Abort(reason);
}
#endif  // V8_ENABLE_DEBUG_CODE

void TurboAssembler::CopySlots(int dst, Register src, Register slot_count) {
  DCHECK(!src.IsZero());
  UseScratchRegisterScope scope(this);
  Register dst_reg = scope.AcquireX();
  SlotAddress(dst_reg, dst);
  SlotAddress(src, src);
  CopyDoubleWords(dst_reg, src, slot_count);
}

void TurboAssembler::CopySlots(Register dst, Register src,
                               Register slot_count) {
  DCHECK(!dst.IsZero() && !src.IsZero());
  SlotAddress(dst, dst);
  SlotAddress(src, src);
  CopyDoubleWords(dst, src, slot_count);
}

void TurboAssembler::CopyDoubleWords(Register dst, Register src, Register count,
                                     CopyDoubleWordsMode mode) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(dst, src, count));

  if (v8_flags.debug_code) {
    Register pointer1 = dst;
    Register pointer2 = src;
    if (mode == kSrcLessThanDst) {
      pointer1 = src;
      pointer2 = dst;
    }
    // Copy requires pointer1 < pointer2 || (pointer1 - pointer2) >= count.
    Label pointer1_below_pointer2;
    Subs(pointer1, pointer1, pointer2);
    B(lt, &pointer1_below_pointer2);
    Cmp(pointer1, count);
    Check(ge, AbortReason::kOffsetOutOfRange);
    Bind(&pointer1_below_pointer2);
    Add(pointer1, pointer1, pointer2);
  }
  static_assert(kSystemPointerSize == kDRegSize,
                "pointers must be the same size as doubles");

  if (mode == kDstLessThanSrcAndReverse) {
    Add(src, src, Operand(count, LSL, kSystemPointerSizeLog2));
    Sub(src, src, kSystemPointerSize);
  }

  int src_direction = (mode == kDstLessThanSrc) ? 1 : -1;
  int dst_direction = (mode == kSrcLessThanDst) ? -1 : 1;

  UseScratchRegisterScope scope(this);
  VRegister temp0 = scope.AcquireD();
  VRegister temp1 = scope.AcquireD();

  Label pairs, loop, done;

  Tbz(count, 0, &pairs);
  Ldr(temp0, MemOperand(src, src_direction * kSystemPointerSize, PostIndex));
  Sub(count, count, 1);
  Str(temp0, MemOperand(dst, dst_direction * kSystemPointerSize, PostIndex));

  Bind(&pairs);
  if (mode == kSrcLessThanDst) {
    // Adjust pointers for post-index ldp/stp with negative offset:
    Sub(dst, dst, kSystemPointerSize);
    Sub(src, src, kSystemPointerSize);
  } else if (mode == kDstLessThanSrcAndReverse) {
    Sub(src, src, kSystemPointerSize);
  }
  Bind(&loop);
  Cbz(count, &done);
  Ldp(temp0, temp1,
      MemOperand(src, 2 * src_direction * kSystemPointerSize, PostIndex));
  Sub(count, count, 2);
  if (mode == kDstLessThanSrcAndReverse) {
    Stp(temp1, temp0,
        MemOperand(dst, 2 * dst_direction * kSystemPointerSize, PostIndex));
  } else {
    Stp(temp0, temp1,
        MemOperand(dst, 2 * dst_direction * kSystemPointerSize, PostIndex));
  }
  B(&loop);

  // TODO(all): large copies may benefit from using temporary Q registers
  // to copy four double words per iteration.

  Bind(&done);
}

void TurboAssembler::SlotAddress(Register dst, int slot_offset) {
  Add(dst, sp, slot_offset << kSystemPointerSizeLog2);
}

void TurboAssembler::SlotAddress(Register dst, Register slot_offset) {
  Add(dst, sp, Operand(slot_offset, LSL, kSystemPointerSizeLog2));
}

void TurboAssembler::CanonicalizeNaN(const VRegister& dst,
                                     const VRegister& src) {
  AssertFPCRState();

  // Subtracting 0.0 preserves all inputs except for signalling NaNs, which
  // become quiet NaNs. We use fsub rather than fadd because fsub preserves -0.0
  // inputs: -0.0 + 0.0 = 0.0, but -0.0 - 0.0 = -0.0.
  Fsub(dst, src, fp_zero);
}

void TurboAssembler::LoadRoot(Register destination, RootIndex index) {
  ASM_CODE_COMMENT(this);
  // TODO(jbramley): Most root values are constants, and can be synthesized
  // without a load. Refer to the ARM back end for details.
  Ldr(destination,
      MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
}

void TurboAssembler::PushRoot(RootIndex index) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register tmp = temps.AcquireX();
  LoadRoot(tmp, index);
  Push(tmp);
}

void TurboAssembler::Move(Register dst, Smi src) { Mov(dst, src); }
void TurboAssembler::Move(Register dst, MemOperand src) { Ldr(dst, src); }
void TurboAssembler::Move(Register dst, Register src) {
  if (dst == src) return;
  Mov(dst, src);
}

void TurboAssembler::MovePair(Register dst0, Register src0, Register dst1,
                              Register src1) {
  DCHECK_NE(dst0, dst1);
  if (dst0 != src1) {
    Mov(dst0, src0);
    Mov(dst1, src1);
  } else if (dst1 != src0) {
    // Swap the order of the moves to resolve the overlap.
    Mov(dst1, src1);
    Mov(dst0, src0);
  } else {
    // Worse case scenario, this is a swap.
    Swap(dst0, src0);
  }
}

void TurboAssembler::Swap(Register lhs, Register rhs) {
  DCHECK(lhs.IsSameSizeAndType(rhs));
  DCHECK_NE(lhs, rhs);
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, rhs);
  Mov(rhs, lhs);
  Mov(lhs, temp);
}

void TurboAssembler::Swap(VRegister lhs, VRegister rhs) {
  DCHECK(lhs.IsSameSizeAndType(rhs));
  DCHECK_NE(lhs, rhs);
  UseScratchRegisterScope temps(this);
  VRegister temp = VRegister::no_reg();
  if (lhs.IsS()) {
    temp = temps.AcquireS();
  } else if (lhs.IsD()) {
    temp = temps.AcquireD();
  } else {
    DCHECK(lhs.IsQ());
    temp = temps.AcquireQ();
  }
  Mov(temp, rhs);
  Mov(rhs, lhs);
  Mov(lhs, temp);
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  ASM_CODE_COMMENT(this);
  // All arguments must be on the stack before this function is called.
  // x0 holds the return value after the call.

  // Check that the number of arguments matches what the function expects.
  // If f->nargs is -1, the function can accept a variable number of arguments.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // Place the necessary arguments.
  Mov(x0, num_arguments);
  Mov(x1, ExternalReference::Create(f));

  Handle<CodeT> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  ASM_CODE_COMMENT(this);
  Mov(x1, builtin);
  Handle<CodeT> code =
      CodeFactory::CEntry(isolate(), 1, SaveFPRegsMode::kIgnore,
                          ArgvMode::kStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToOffHeapInstructionStream(Address entry) {
  Ldr(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Br(kOffHeapTrampolineRegister);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  ASM_CODE_COMMENT(this);
  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    // TODO(1236192): Most runtime routines don't need the number of
    // arguments passed in because it is constant. At some point we
    // should remove this need and make the runtime routine entry code
    // smarter.
    Mov(x0, function->nargs);
  }
  JumpToExternalReference(ExternalReference::Create(fid));
}

int TurboAssembler::ActivationFrameAlignment() {
#if V8_HOST_ARCH_ARM64
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one ARM
  // platform for another ARM platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else   // V8_HOST_ARCH_ARM64
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return v8_flags.sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM64
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args) {
  CallCFunction(function, num_of_reg_args, 0);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, function);
  CallCFunction(temp, num_of_reg_args, num_of_double_args);
}

static const int kRegisterPassedArguments = 8;
static const int kFPRegisterPassedArguments = 8;

void TurboAssembler::CallCFunction(Register function, int num_of_reg_args,
                                   int num_of_double_args) {
  ASM_CODE_COMMENT(this);
  DCHECK_LE(num_of_reg_args + num_of_double_args, kMaxCParameters);
  DCHECK(has_frame());

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  Register pc_scratch = x4;
  Register addr_scratch = x5;
  Push(pc_scratch, addr_scratch);

  Label get_pc;
  Bind(&get_pc);
  Adr(pc_scratch, &get_pc);

  // See x64 code for reasoning about how to address the isolate data fields.
  if (root_array_available()) {
    Str(pc_scratch,
        MemOperand(kRootRegister, IsolateData::fast_c_call_caller_pc_offset()));
    Str(fp,
        MemOperand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset()));
  } else {
    DCHECK_NOT_NULL(isolate());
    Mov(addr_scratch,
        ExternalReference::fast_c_call_caller_pc_address(isolate()));
    Str(pc_scratch, MemOperand(addr_scratch));
    Mov(addr_scratch,
        ExternalReference::fast_c_call_caller_fp_address(isolate()));
    Str(fp, MemOperand(addr_scratch));
  }

  Pop(addr_scratch, pc_scratch);

  // Call directly. The function called cannot cause a GC, or allow preemption,
  // so the return address in the link register stays correct.
  Call(function);

  // We don't unset the PC; the FP is the source of truth.
  if (root_array_available()) {
    Str(xzr,
        MemOperand(kRootRegister, IsolateData::fast_c_call_caller_fp_offset()));
  } else {
    DCHECK_NOT_NULL(isolate());
    Push(addr_scratch, xzr);
    Mov(addr_scratch,
        ExternalReference::fast_c_call_caller_fp_address(isolate()));
    Str(xzr, MemOperand(addr_scratch));
    Pop(xzr, addr_scratch);
  }

  if (num_of_reg_args > kRegisterPassedArguments) {
    // Drop the register passed arguments.
    int claim_slots = RoundUp(num_of_reg_args - kRegisterPassedArguments, 2);
    Drop(claim_slots);
  }

  if (num_of_double_args > kFPRegisterPassedArguments) {
    // Drop the register passed arguments.
    int claim_slots =
        RoundUp(num_of_double_args - kFPRegisterPassedArguments, 2);
    Drop(claim_slots);
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
  ASM_CODE_COMMENT(this);
  DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kBuiltinsConstantsTable));
  LoadRoot(destination, RootIndex::kBuiltinsConstantsTable);
  LoadTaggedPointerField(
      destination, FieldMemOperand(destination, FixedArray::OffsetOfElementAt(
                                                    constant_index)));
}

void TurboAssembler::LoadRootRelative(Register destination, int32_t offset) {
  Ldr(destination, MemOperand(kRootRegister, offset));
}

void TurboAssembler::LoadRootRegisterOffset(Register destination,
                                            intptr_t offset) {
  if (offset == 0) {
    Mov(destination, kRootRegister);
  } else {
    Add(destination, kRootRegister, offset);
  }
}

MemOperand TurboAssembler::ExternalReferenceAsOperand(
    ExternalReference reference, Register scratch) {
  if (root_array_available_ && options().enable_root_relative_access) {
    int64_t offset =
        RootRegisterOffsetForExternalReference(isolate(), reference);
    if (is_int32(offset)) {
      return MemOperand(kRootRegister, static_cast<int32_t>(offset));
    }
  }
  if (root_array_available_ && options().isolate_independent_code) {
    if (IsAddressableThroughRootRegister(isolate(), reference)) {
      // Some external references can be efficiently loaded as an offset from
      // kRootRegister.
      intptr_t offset =
          RootRegisterOffsetForExternalReference(isolate(), reference);
      CHECK(is_int32(offset));
      return MemOperand(kRootRegister, static_cast<int32_t>(offset));
    } else {
      // Otherwise, do a memory load from the external reference table.
      Ldr(scratch, MemOperand(kRootRegister,
                              RootRegisterOffsetForExternalReferenceTableEntry(
                                  isolate(), reference)));
      return MemOperand(scratch, 0);
    }
  }
  Mov(scratch, reference);
  return MemOperand(scratch, 0);
}

void TurboAssembler::Jump(Register target, Condition cond) {
  if (cond == nv) return;
  Label done;
  if (cond != al) B(NegateCondition(cond), &done);
  Br(target);
  Bind(&done);
}

void TurboAssembler::JumpHelper(int64_t offset, RelocInfo::Mode rmode,
                                Condition cond) {
  if (cond == nv) return;
  Label done;
  if (cond != al) B(NegateCondition(cond), &done);
  if (CanUseNearCallOrJump(rmode)) {
    DCHECK(IsNearCallOffset(offset));
    near_jump(static_cast<int>(offset), rmode);
  } else {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    uint64_t imm = reinterpret_cast<uint64_t>(pc_) + offset * kInstrSize;
    Mov(temp, Immediate(imm, rmode));
    Br(temp);
  }
  Bind(&done);
}

// The calculated offset is either:
// * the 'target' input unmodified if this is a Wasm call, or
// * the offset of the target from the current PC, in instructions, for any
//   other type of call.
int64_t TurboAssembler::CalculateTargetOffset(Address target,
                                              RelocInfo::Mode rmode, byte* pc) {
  int64_t offset = static_cast<int64_t>(target);
  if (rmode == RelocInfo::WASM_CALL || rmode == RelocInfo::WASM_STUB_CALL) {
    // The target of WebAssembly calls is still an index instead of an actual
    // address at this point, and needs to be encoded as-is.
    return offset;
  }
  offset -= reinterpret_cast<int64_t>(pc);
  DCHECK_EQ(offset % kInstrSize, 0);
  offset = offset / static_cast<int>(kInstrSize);
  return offset;
}

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  int64_t offset = CalculateTargetOffset(target, rmode, pc_);
  JumpHelper(offset, rmode, cond);
}

void TurboAssembler::Jump(Handle<CodeT> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  Builtin builtin = Builtin::kNoBuiltinId;
  if (isolate()->builtins()->IsBuiltinHandle(code, &builtin)) {
    TailCallBuiltin(builtin, cond);
    return;
  }
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  if (CanUseNearCallOrJump(rmode)) {
    EmbeddedObjectIndex index = AddEmbeddedObject(code);
    DCHECK(is_int32(index));
    JumpHelper(static_cast<int64_t>(index), rmode, cond);
  } else {
    Jump(code.address(), rmode, cond);
  }
}

void TurboAssembler::Jump(const ExternalReference& reference) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  Mov(scratch, reference);
  Jump(scratch);
}

void TurboAssembler::Call(Register target) {
  BlockPoolsScope scope(this);
  Blr(target);
}

void TurboAssembler::Call(Address target, RelocInfo::Mode rmode) {
  BlockPoolsScope scope(this);
  if (CanUseNearCallOrJump(rmode)) {
    int64_t offset = CalculateTargetOffset(target, rmode, pc_);
    DCHECK(IsNearCallOffset(offset));
    near_call(static_cast<int>(offset), rmode);
  } else {
    IndirectCall(target, rmode);
  }
}

void TurboAssembler::Call(Handle<CodeT> code, RelocInfo::Mode rmode) {
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));
  BlockPoolsScope scope(this);

  Builtin builtin = Builtin::kNoBuiltinId;
  if (isolate()->builtins()->IsBuiltinHandle(code, &builtin)) {
    CallBuiltin(builtin);
    return;
  }

  DCHECK(FromCodeT(*code).IsExecutable());
  DCHECK(RelocInfo::IsCodeTarget(rmode));

  if (CanUseNearCallOrJump(rmode)) {
    EmbeddedObjectIndex index = AddEmbeddedObject(code);
    DCHECK(is_int32(index));
    near_call(static_cast<int32_t>(index), rmode);
  } else {
    IndirectCall(code.address(), rmode);
  }
}

void TurboAssembler::Call(ExternalReference target) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, target);
  Call(temp);
}

void TurboAssembler::LoadEntryFromBuiltinIndex(Register builtin_index) {
  ASM_CODE_COMMENT(this);
  // The builtin_index register contains the builtin index as a Smi.
  // Untagging is folded into the indexing operand below.
  if (SmiValuesAre32Bits()) {
    Asr(builtin_index, builtin_index, kSmiShift - kSystemPointerSizeLog2);
    Add(builtin_index, builtin_index,
        IsolateData::builtin_entry_table_offset());
    Ldr(builtin_index, MemOperand(kRootRegister, builtin_index));
  } else {
    DCHECK(SmiValuesAre31Bits());
    if (COMPRESS_POINTERS_BOOL) {
      Add(builtin_index, kRootRegister,
          Operand(builtin_index.W(), SXTW, kSystemPointerSizeLog2 - kSmiShift));
    } else {
      Add(builtin_index, kRootRegister,
          Operand(builtin_index, LSL, kSystemPointerSizeLog2 - kSmiShift));
    }
    Ldr(builtin_index,
        MemOperand(builtin_index, IsolateData::builtin_entry_table_offset()));
  }
}

void TurboAssembler::LoadEntryFromBuiltin(Builtin builtin,
                                          Register destination) {
  Ldr(destination, EntryFromBuiltinAsOperand(builtin));
}

MemOperand TurboAssembler::EntryFromBuiltinAsOperand(Builtin builtin) {
  ASM_CODE_COMMENT(this);
  DCHECK(root_array_available());
  return MemOperand(kRootRegister,
                    IsolateData::BuiltinEntrySlotOffset(builtin));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  ASM_CODE_COMMENT(this);
  LoadEntryFromBuiltinIndex(builtin_index);
  Call(builtin_index);
}

void TurboAssembler::CallBuiltin(Builtin builtin) {
  ASM_CODE_COMMENT_STRING(this, CommentForOffHeapTrampoline("call", builtin));
  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.AcquireX();
      Ldr(scratch, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
      Call(scratch);
      break;
    }
    case BuiltinCallJumpMode::kPCRelative:
      near_call(static_cast<int>(builtin), RelocInfo::NEAR_BUILTIN_ENTRY);
      break;
    case BuiltinCallJumpMode::kIndirect: {
      UseScratchRegisterScope temps(this);
      Register scratch = temps.AcquireX();
      LoadEntryFromBuiltin(builtin, scratch);
      Call(scratch);
      break;
    }
    case BuiltinCallJumpMode::kForMksnapshot: {
      if (options().use_pc_relative_calls_and_jumps_for_mksnapshot) {
        Handle<CodeT> code = isolate()->builtins()->code_handle(builtin);
        EmbeddedObjectIndex index = AddEmbeddedObject(code);
        DCHECK(is_int32(index));
        near_call(static_cast<int32_t>(index), RelocInfo::CODE_TARGET);
      } else {
        UseScratchRegisterScope temps(this);
        Register scratch = temps.AcquireX();
        LoadEntryFromBuiltin(builtin, scratch);
        Call(scratch);
      }
      break;
    }
  }
}

// TODO(ishell): remove cond parameter from here to simplify things.
void TurboAssembler::TailCallBuiltin(Builtin builtin, Condition cond) {
  ASM_CODE_COMMENT_STRING(this,
                          CommentForOffHeapTrampoline("tail call", builtin));

  // The control flow integrity (CFI) feature allows us to "sign" code entry
  // points as a target for calls, jumps or both. Arm64 has special
  // instructions for this purpose, so-called "landing pads" (see
  // TurboAssembler::CallTarget(), TurboAssembler::JumpTarget() and
  // TurboAssembler::JumpOrCallTarget()). Currently, we generate "Call"
  // landing pads for CPP builtins. In order to allow tail calling to those
  // builtins we have to use a workaround.
  // x17 is used to allow using "Call" (i.e. `bti c`) rather than "Jump"
  // (i.e. `bti j`) landing pads for the tail-called code.
  Register temp = x17;

  switch (options().builtin_call_jump_mode) {
    case BuiltinCallJumpMode::kAbsolute: {
      Ldr(temp, Operand(BuiltinEntry(builtin), RelocInfo::OFF_HEAP_TARGET));
      Jump(temp, cond);
      break;
    }
    case BuiltinCallJumpMode::kPCRelative: {
      if (cond != nv) {
        Label done;
        if (cond != al) B(NegateCondition(cond), &done);
        near_jump(static_cast<int>(builtin), RelocInfo::NEAR_BUILTIN_ENTRY);
        Bind(&done);
      }
      break;
    }
    case BuiltinCallJumpMode::kIndirect: {
      LoadEntryFromBuiltin(builtin, temp);
      Jump(temp, cond);
      break;
    }
    case BuiltinCallJumpMode::kForMksnapshot: {
      if (options().use_pc_relative_calls_and_jumps_for_mksnapshot) {
        Handle<CodeT> code = isolate()->builtins()->code_handle(builtin);
        EmbeddedObjectIndex index = AddEmbeddedObject(code);
        DCHECK(is_int32(index));
        JumpHelper(static_cast<int64_t>(index), RelocInfo::CODE_TARGET, cond);
      } else {
        LoadEntryFromBuiltin(builtin, temp);
        Jump(temp, cond);
      }
      break;
    }
  }
}

void TurboAssembler::LoadCodeObjectEntry(Register destination,
                                         Register code_object) {
  ASM_CODE_COMMENT(this);
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    LoadCodeDataContainerEntry(destination, code_object);
    return;
  }

  // Code objects are called differently depending on whether we are generating
  // builtin code (which will later be embedded into the binary) or compiling
  // user JS code at runtime.
  // * Builtin code runs in --jitless mode and thus must not call into on-heap
  //   Code targets. Instead, we dispatch through the builtins entry table.
  // * Codegen at runtime does not have this restriction and we can use the
  //   shorter, branchless instruction sequence. The assumption here is that
  //   targets are usually generated code and not builtin Code objects.

  if (options().isolate_independent_code) {
    DCHECK(root_array_available());
    Label if_code_is_off_heap, out;

    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();

    DCHECK(!AreAliased(destination, scratch));
    DCHECK(!AreAliased(code_object, scratch));

    // Check whether the Code object is an off-heap trampoline. If so, call its
    // (off-heap) entry point directly without going through the (on-heap)
    // trampoline.  Otherwise, just call the Code object as always.

    Ldr(scratch.W(), FieldMemOperand(code_object, Code::kFlagsOffset));
    TestAndBranchIfAnySet(scratch.W(), Code::IsOffHeapTrampoline::kMask,
                          &if_code_is_off_heap);

    // Not an off-heap trampoline object, the entry point is at
    // Code::raw_instruction_start().
    Add(destination, code_object, Code::kHeaderSize - kHeapObjectTag);
    B(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    Ldrsw(scratch, FieldMemOperand(code_object, Code::kBuiltinIndexOffset));
    Add(destination, kRootRegister,
        Operand(scratch, LSL, kSystemPointerSizeLog2));
    Ldr(destination,
        MemOperand(destination, IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    Add(destination, code_object, Code::kHeaderSize - kHeapObjectTag);
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  ASM_CODE_COMMENT(this);
  LoadCodeObjectEntry(code_object, code_object);
  Call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object, JumpMode jump_mode) {
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(JumpMode::kJump, jump_mode);
  LoadCodeObjectEntry(code_object, code_object);

  UseScratchRegisterScope temps(this);
  if (code_object != x17) {
    temps.Exclude(x17);
    Mov(x17, code_object);
  }
  Jump(x17);
}

void TurboAssembler::LoadCodeDataContainerEntry(
    Register destination, Register code_data_container_object) {
  ASM_CODE_COMMENT(this);
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);

  Ldr(destination, FieldMemOperand(code_data_container_object,
                                   CodeDataContainer::kCodeEntryPointOffset));
}

void TurboAssembler::LoadCodeDataContainerCodeNonBuiltin(
    Register destination, Register code_data_container_object) {
  ASM_CODE_COMMENT(this);
  CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
  // Given the fields layout we can read the Code reference as a full word.
  static_assert(!V8_EXTERNAL_CODE_SPACE_BOOL ||
                (CodeDataContainer::kCodeCageBaseUpper32BitsOffset ==
                 CodeDataContainer::kCodeOffset + kTaggedSize));
  Ldr(destination, FieldMemOperand(code_data_container_object,
                                   CodeDataContainer::kCodeOffset));
}

void TurboAssembler::CallCodeDataContainerObject(
    Register code_data_container_object) {
  ASM_CODE_COMMENT(this);
  LoadCodeDataContainerEntry(code_data_container_object,
                             code_data_container_object);
  Call(code_data_container_object);
}

void TurboAssembler::JumpCodeDataContainerObject(
    Register code_data_container_object, JumpMode jump_mode) {
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(JumpMode::kJump, jump_mode);
  LoadCodeDataContainerEntry(code_data_container_object,
                             code_data_container_object);
  UseScratchRegisterScope temps(this);
  if (code_data_container_object != x17) {
    temps.Exclude(x17);
    Mov(x17, code_data_container_object);
  }
  Jump(x17);
}

void TurboAssembler::LoadCodeTEntry(Register destination, Register code) {
  ASM_CODE_COMMENT(this);
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    LoadCodeDataContainerEntry(destination, code);
  } else {
    Add(destination, code, Operand(Code::kHeaderSize - kHeapObjectTag));
  }
}

void TurboAssembler::CallCodeTObject(Register code) {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    CallCodeDataContainerObject(code);
  } else {
    CallCodeObject(code);
  }
}

void TurboAssembler::JumpCodeTObject(Register code, JumpMode jump_mode) {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    JumpCodeDataContainerObject(code, jump_mode);
  } else {
    JumpCodeObject(code, jump_mode);
  }
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  ASM_CODE_COMMENT(this);
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.

  UseScratchRegisterScope temps(this);
  temps.Exclude(x16, x17);

  Label return_location;
  Adr(x17, &return_location);
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  Add(x16, sp, kSystemPointerSize);
  Pacib1716();
#endif
  Poke(x17, 0);

  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Verify fp[kSPOffset]-8");
    // Verify that the slot below fp[kSPOffset]-8 points to the signed return
    // location.
    Ldr(x16, MemOperand(fp, ExitFrameConstants::kSPOffset));
    Ldr(x16, MemOperand(x16, -static_cast<int64_t>(kXRegSize)));
    Cmp(x16, x17);
    Check(eq, AbortReason::kReturnAddressNotFoundInFrame);
  }

  Blr(target);
  Bind(&return_location);
}

void TurboAssembler::IndirectCall(Address target, RelocInfo::Mode rmode) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, Immediate(target, rmode));
  Blr(temp);
}

bool TurboAssembler::IsNearCallOffset(int64_t offset) {
  return is_int26(offset);
}

void TurboAssembler::CallForDeoptimization(
    Builtin target, int deopt_id, Label* exit, DeoptimizeKind kind, Label* ret,
    Label* jump_deoptimization_entry_label) {
  ASM_CODE_COMMENT(this);
  BlockPoolsScope scope(this);
  bl(jump_deoptimization_entry_label);
  DCHECK_EQ(SizeOfCodeGeneratedSince(exit),
            (kind == DeoptimizeKind::kLazy) ? Deoptimizer::kLazyDeoptExitSize
                                            : Deoptimizer::kEagerDeoptExitSize);
}

void MacroAssembler::LoadStackLimit(Register destination, StackLimitKind kind) {
  ASM_CODE_COMMENT(this);
  DCHECK(root_array_available());
  Isolate* isolate = this->isolate();
  ExternalReference limit =
      kind == StackLimitKind::kRealStackLimit
          ? ExternalReference::address_of_real_jslimit(isolate)
          : ExternalReference::address_of_jslimit(isolate);
  DCHECK(TurboAssembler::IsAddressableThroughRootRegister(isolate, limit));

  intptr_t offset =
      TurboAssembler::RootRegisterOffsetForExternalReference(isolate, limit);
  Ldr(destination, MemOperand(kRootRegister, offset));
}

void MacroAssembler::StackOverflowCheck(Register num_args,
                                        Label* stack_overflow) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();

  // Check the stack for overflow.
  // We are not trying to catch interruptions (e.g. debug break and
  // preemption) here, so the "real stack limit" is checked.

  LoadStackLimit(scratch, StackLimitKind::kRealStackLimit);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  Sub(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  Cmp(scratch, Operand(num_args, LSL, kSystemPointerSizeLog2));
  B(le, stack_overflow);
}

void MacroAssembler::InvokePrologue(Register formal_parameter_count,
                                    Register actual_argument_count, Label* done,
                                    InvokeType type) {
  ASM_CODE_COMMENT(this);
  //  x0: actual arguments count.
  //  x1: function (passed through to callee).
  //  x2: expected arguments count.
  //  x3: new target
  Label regular_invoke;
  DCHECK_EQ(actual_argument_count, x0);
  DCHECK_EQ(formal_parameter_count, x2);

  // If the formal parameter count is equal to the adaptor sentinel, no need
  // to push undefined value as arguments.
  if (kDontAdaptArgumentsSentinel != 0) {
    Cmp(formal_parameter_count, Operand(kDontAdaptArgumentsSentinel));
    B(eq, &regular_invoke);
  }

  // If overapplication or if the actual argument count is equal to the
  // formal parameter count, no need to push extra undefined values.
  Register extra_argument_count = x2;
  Subs(extra_argument_count, formal_parameter_count, actual_argument_count);
  B(le, &regular_invoke);

  // The stack pointer in arm64 needs to be 16-byte aligned. We might need to
  // (1) add an extra padding or (2) remove (re-use) the extra padding already
  // in the stack. Let {slots_to_copy} be the number of slots (arguments) to
  // move up in the stack and let {slots_to_claim} be the number of extra stack
  // slots to claim.
  Label even_extra_count, skip_move;
  Register slots_to_copy = x4;
  Register slots_to_claim = x5;

  Mov(slots_to_copy, actual_argument_count);
  Mov(slots_to_claim, extra_argument_count);
  Tbz(extra_argument_count, 0, &even_extra_count);

  // Calculate {slots_to_claim} when {extra_argument_count} is odd.
  // If {actual_argument_count} is even, we need one extra padding slot
  // {slots_to_claim = extra_argument_count + 1}.
  // If {actual_argument_count} is odd, we know that the
  // original arguments will have a padding slot that we can reuse
  // {slots_to_claim = extra_argument_count - 1}.
  {
    Register scratch = x11;
    Add(slots_to_claim, extra_argument_count, 1);
    And(scratch, actual_argument_count, 1);
    Sub(slots_to_claim, slots_to_claim, Operand(scratch, LSL, 1));
  }

  Bind(&even_extra_count);
  Cbz(slots_to_claim, &skip_move);

  Label stack_overflow;
  StackOverflowCheck(slots_to_claim, &stack_overflow);
  Claim(slots_to_claim);

  // Move the arguments already in the stack including the receiver.
  {
    Register src = x6;
    Register dst = x7;
    SlotAddress(src, slots_to_claim);
    SlotAddress(dst, 0);
    CopyDoubleWords(dst, src, slots_to_copy);
  }

  Bind(&skip_move);
  Register pointer_next_value = x5;

  // Copy extra arguments as undefined values.
  {
    Label loop;
    Register undefined_value = x6;
    Register count = x7;
    LoadRoot(undefined_value, RootIndex::kUndefinedValue);
    SlotAddress(pointer_next_value, actual_argument_count);
    Mov(count, extra_argument_count);
    Bind(&loop);
    Str(undefined_value,
        MemOperand(pointer_next_value, kSystemPointerSize, PostIndex));
    Subs(count, count, 1);
    Cbnz(count, &loop);
  }

  // Set padding if needed.
  {
    Label skip;
    Register total_args_slots = x4;
    Add(total_args_slots, actual_argument_count, extra_argument_count);
    Tbz(total_args_slots, 0, &skip);
    Str(padreg, MemOperand(pointer_next_value));
    Bind(&skip);
  }
  B(&regular_invoke);

  bind(&stack_overflow);
  {
    FrameScope frame(
        this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);
    CallRuntime(Runtime::kThrowStackOverflow);
    Unreachable();
  }

  Bind(&regular_invoke);
}

void MacroAssembler::CallDebugOnFunctionCall(Register fun, Register new_target,
                                             Register expected_parameter_count,
                                             Register actual_parameter_count) {
  ASM_CODE_COMMENT(this);
  // Load receiver to pass it later to DebugOnFunctionCall hook.
  Peek(x4, ReceiverOperand(actual_parameter_count));
  FrameScope frame(
      this, has_frame() ? StackFrame::NO_FRAME_TYPE : StackFrame::INTERNAL);

  if (!new_target.is_valid()) new_target = padreg;

  // Save values on stack.
  SmiTag(expected_parameter_count);
  SmiTag(actual_parameter_count);
  Push(expected_parameter_count, actual_parameter_count, new_target, fun);
  Push(fun, x4);
  CallRuntime(Runtime::kDebugOnFunctionCall);

  // Restore values from stack.
  Pop(fun, new_target, actual_parameter_count, expected_parameter_count);
  SmiUntag(actual_parameter_count);
  SmiUntag(expected_parameter_count);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        Register expected_parameter_count,
                                        Register actual_parameter_count,
                                        InvokeType type) {
  ASM_CODE_COMMENT(this);
  // You can't call a function without a valid frame.
  DCHECK_IMPLIES(type == InvokeType::kCall, has_frame());
  DCHECK_EQ(function, x1);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == x3);

  // On function call, call into the debugger if necessary.
  Label debug_hook, continue_after_hook;
  {
    Mov(x4, ExternalReference::debug_hook_on_function_call_address(isolate()));
    Ldrsb(x4, MemOperand(x4));
    Cbnz(x4, &debug_hook);
  }
  bind(&continue_after_hook);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(x3, RootIndex::kUndefinedValue);
  }

  Label done;
  InvokePrologue(expected_parameter_count, actual_parameter_count, &done, type);

  // If actual != expected, InvokePrologue will have handled the call through
  // the argument adaptor mechanism.
  // The called function expects the call kind in x5.
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  Register code = kJavaScriptCallCodeStartRegister;
  LoadTaggedPointerField(code,
                         FieldMemOperand(function, JSFunction::kCodeOffset));
  switch (type) {
    case InvokeType::kCall:
      CallCodeTObject(code);
      break;
    case InvokeType::kJump:
      JumpCodeTObject(code);
      break;
  }
  B(&done);

  // Deferred debug hook.
  bind(&debug_hook);
  CallDebugOnFunctionCall(function, new_target, expected_parameter_count,
                          actual_parameter_count);
  B(&continue_after_hook);

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  Bind(&done);
}

void MacroAssembler::JumpIfCodeTIsMarkedForDeoptimization(
    Register codet, Register scratch, Label* if_marked_for_deoptimization) {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    Ldr(scratch.W(),
        FieldMemOperand(codet, CodeDataContainer::kKindSpecificFlagsOffset));
    Tbnz(scratch.W(), Code::kMarkedForDeoptimizationBit,
         if_marked_for_deoptimization);

  } else {
    LoadTaggedPointerField(
        scratch, FieldMemOperand(codet, Code::kCodeDataContainerOffset));
    Ldr(scratch.W(),
        FieldMemOperand(scratch, CodeDataContainer::kKindSpecificFlagsOffset));
    Tbnz(scratch.W(), Code::kMarkedForDeoptimizationBit,
         if_marked_for_deoptimization);
  }
}

Operand MacroAssembler::ClearedValue() const {
  return Operand(
      static_cast<int32_t>(HeapObjectReference::ClearedValue(isolate()).ptr()));
}

Operand MacroAssembler::ReceiverOperand(Register arg_count) {
  return Operand(0);
}

void MacroAssembler::InvokeFunctionWithNewTarget(
    Register function, Register new_target, Register actual_parameter_count,
    InvokeType type) {
  ASM_CODE_COMMENT(this);
  // You can't call a function without a valid frame.
  DCHECK(type == InvokeType::kJump || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK_EQ(function, x1);

  Register expected_parameter_count = x2;

  LoadTaggedPointerField(cp,
                         FieldMemOperand(function, JSFunction::kContextOffset));
  // The number of arguments is stored as an int32_t, and -1 is a marker
  // (kDontAdaptArgumentsSentinel), so we need sign
  // extension to correctly handle it.
  LoadTaggedPointerField(
      expected_parameter_count,
      FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
  Ldrh(expected_parameter_count,
       FieldMemOperand(expected_parameter_count,
                       SharedFunctionInfo::kFormalParameterCountOffset));

  InvokeFunctionCode(function, new_target, expected_parameter_count,
                     actual_parameter_count, type);
}

void MacroAssembler::InvokeFunction(Register function,
                                    Register expected_parameter_count,
                                    Register actual_parameter_count,
                                    InvokeType type) {
  ASM_CODE_COMMENT(this);
  // You can't call a function without a valid frame.
  DCHECK(type == InvokeType::kJump || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK_EQ(function, x1);

  // Set up the context.
  LoadTaggedPointerField(cp,
                         FieldMemOperand(function, JSFunction::kContextOffset));

  InvokeFunctionCode(function, no_reg, expected_parameter_count,
                     actual_parameter_count, type);
}

void TurboAssembler::TryConvertDoubleToInt64(Register result,
                                             DoubleRegister double_input,
                                             Label* done) {
  ASM_CODE_COMMENT(this);
  // Try to convert with an FPU convert instruction. It's trivial to compute
  // the modulo operation on an integer register so we convert to a 64-bit
  // integer.
  //
  // Fcvtzs will saturate to INT64_MIN (0x800...00) or INT64_MAX (0x7FF...FF)
  // when the double is out of range. NaNs and infinities will be converted to 0
  // (as ECMA-262 requires).
  Fcvtzs(result.X(), double_input);

  // The values INT64_MIN (0x800...00) or INT64_MAX (0x7FF...FF) are not
  // representable using a double, so if the result is one of those then we know
  // that saturation occurred, and we need to manually handle the conversion.
  //
  // It is easy to detect INT64_MIN and INT64_MAX because adding or subtracting
  // 1 will cause signed overflow.
  Cmp(result.X(), 1);
  Ccmp(result.X(), -1, VFlag, vc);

  B(vc, done);
}

void TurboAssembler::TruncateDoubleToI(Isolate* isolate, Zone* zone,
                                       Register result,
                                       DoubleRegister double_input,
                                       StubCallMode stub_mode,
                                       LinkRegisterStatus lr_status) {
  ASM_CODE_COMMENT(this);
  if (CpuFeatures::IsSupported(JSCVT)) {
    Fjcvtzs(result.W(), double_input);
    return;
  }

  Label done;

  // Try to convert the double to an int64. If successful, the bottom 32 bits
  // contain our truncated int32 result.
  TryConvertDoubleToInt64(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  if (lr_status == kLRHasNotBeenSaved) {
    Push<TurboAssembler::kSignLR>(lr, double_input);
  } else {
    Push<TurboAssembler::kDontStoreLR>(xzr, double_input);
  }

  // DoubleToI preserves any registers it needs to clobber.
#if V8_ENABLE_WEBASSEMBLY
  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
#else
  // For balance.
  if (false) {
#endif  // V8_ENABLE_WEBASSEMBLY
  } else {
    CallBuiltin(Builtin::kDoubleToI);
  }
  Ldr(result, MemOperand(sp, 0));

  DCHECK_EQ(xzr.SizeInBytes(), double_input.SizeInBytes());

  if (lr_status == kLRHasNotBeenSaved) {
    // Pop into xzr here to drop the double input on the stack:
    Pop<TurboAssembler::kAuthLR>(xzr, lr);
  } else {
    Drop(2);
  }

  Bind(&done);
  // Keep our invariant that the upper 32 bits are zero.
  Uxtw(result.W(), result.W());
}

void TurboAssembler::Prologue() {
  ASM_CODE_COMMENT(this);
  Push<TurboAssembler::kSignLR>(lr, fp);
  mov(fp, sp);
  static_assert(kExtraSlotClaimedByPrologue == 1);
  Push(cp, kJSFunctionRegister, kJavaScriptCallArgCountRegister, padreg);
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  UseScratchRegisterScope temps(this);

  if (StackFrame::IsJavaScript(type)) {
    // Just push a minimal "machine frame", saving the frame pointer and return
    // address, without any markers.
    Push<TurboAssembler::kSignLR>(lr, fp);
    Mov(fp, sp);
    // sp[1] : lr
    // sp[0] : fp
  } else {
      Register type_reg = temps.AcquireX();
      Mov(type_reg, StackFrame::TypeToMarker(type));
      Register fourth_reg = no_reg;
      if (type == StackFrame::CONSTRUCT) {
        fourth_reg = cp;
#if V8_ENABLE_WEBASSEMBLY
      } else if (type == StackFrame::WASM ||
                type == StackFrame::WASM_COMPILE_LAZY ||
                type == StackFrame::WASM_EXIT) {
        fourth_reg = kWasmInstanceRegister;
#endif  // V8_ENABLE_WEBASSEMBLY
      } else {
        fourth_reg = padreg;
      }
      Push<TurboAssembler::kSignLR>(lr, fp, type_reg, fourth_reg);
      static constexpr int kSPToFPDelta  = 2 * kSystemPointerSize;
      Add(fp, sp, kSPToFPDelta);
      // sp[3] : lr
      // sp[2] : fp
      // sp[1] : type
      // sp[0] : cp | wasm instance | for alignment
  }
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  ASM_CODE_COMMENT(this);
  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer and return address.
  Mov(sp, fp);
  Pop<TurboAssembler::kAuthLR>(fp, lr);
}

void MacroAssembler::ExitFramePreserveFPRegs() {
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(kCallerSavedV.Count() % 2, 0);
  PushCPURegList(kCallerSavedV);
}

void MacroAssembler::ExitFrameRestoreFPRegs() {
  // Read the registers from the stack without popping them. The stack pointer
  // will be reset as part of the unwinding process.
  ASM_CODE_COMMENT(this);
  CPURegList saved_fp_regs = kCallerSavedV;
  DCHECK_EQ(saved_fp_regs.Count() % 2, 0);

  int offset = ExitFrameConstants::kLastExitFrameField;
  while (!saved_fp_regs.IsEmpty()) {
    const CPURegister& dst0 = saved_fp_regs.PopHighestIndex();
    const CPURegister& dst1 = saved_fp_regs.PopHighestIndex();
    offset -= 2 * kDRegSize;
    Ldp(dst1, dst0, MemOperand(fp, offset));
  }
}

void MacroAssembler::EnterExitFrame(bool save_doubles, const Register& scratch,
                                    int extra_space,
                                    StackFrame::Type frame_type) {
  ASM_CODE_COMMENT(this);
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the new stack frame.
  Push<TurboAssembler::kSignLR>(lr, fp);
  Mov(fp, sp);
  Mov(scratch, StackFrame::TypeToMarker(frame_type));
  Push(scratch, xzr);
  //          fp[8]: CallerPC (lr)
  //    fp -> fp[0]: CallerFP (old fp)
  //          fp[-8]: STUB marker
  //    sp -> fp[-16]: Space reserved for SPOffset.
  static_assert((2 * kSystemPointerSize) ==
                ExitFrameConstants::kCallerSPOffset);
  static_assert((1 * kSystemPointerSize) ==
                ExitFrameConstants::kCallerPCOffset);
  static_assert((0 * kSystemPointerSize) ==
                ExitFrameConstants::kCallerFPOffset);
  static_assert((-2 * kSystemPointerSize) == ExitFrameConstants::kSPOffset);

  // Save the frame pointer and context pointer in the top frame.
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  Str(fp, MemOperand(scratch));
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Str(cp, MemOperand(scratch));

  static_assert((-2 * kSystemPointerSize) ==
                ExitFrameConstants::kLastExitFrameField);
  if (save_doubles) {
    ExitFramePreserveFPRegs();
  }

  // Round the number of space we need to claim to a multiple of two.
  int slots_to_claim = RoundUp(extra_space + 1, 2);

  // Reserve space for the return address and for user requested memory.
  // We do this before aligning to make sure that we end up correctly
  // aligned with the minimum of wasted space.
  Claim(slots_to_claim, kXRegSize);
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[-8]: STUB marker
  //         fp[-16]: Space reserved for SPOffset.
  //         fp[-16 - fp_size]: Saved doubles (if save_doubles is true).
  //         sp[8]: Extra space reserved for caller (if extra_space != 0).
  //   sp -> sp[0]: Space reserved for the return address.

  // ExitFrame::GetStateForFramePointer expects to find the return address at
  // the memory address immediately below the pointer stored in SPOffset.
  // It is not safe to derive much else from SPOffset, because the size of the
  // padding can vary.
  Add(scratch, sp, kXRegSize);
  Str(scratch, MemOperand(fp, ExitFrameConstants::kSPOffset));
}

// Leave the current exit frame.
void MacroAssembler::LeaveExitFrame(bool restore_doubles,
                                    const Register& scratch,
                                    const Register& scratch2) {
  ASM_CODE_COMMENT(this);
  if (restore_doubles) {
    ExitFrameRestoreFPRegs();
  }

  // Restore the context pointer from the top frame.
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Ldr(cp, MemOperand(scratch));

  if (v8_flags.debug_code) {
    // Also emit debug code to clear the cp in the top frame.
    Mov(scratch2, Operand(Context::kInvalidContext));
    Mov(scratch, ExternalReference::Create(IsolateAddressId::kContextAddress,
                                           isolate()));
    Str(scratch2, MemOperand(scratch));
  }
  // Clear the frame pointer from the top frame.
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  Str(xzr, MemOperand(scratch));

  // Pop the exit frame.
  //         fp[8]: CallerPC (lr)
  //   fp -> fp[0]: CallerFP (old fp)
  //         fp[...]: The rest of the frame.
  Mov(sp, fp);
  Pop<TurboAssembler::kAuthLR>(fp, lr);
}

void MacroAssembler::LoadGlobalProxy(Register dst) {
  ASM_CODE_COMMENT(this);
  LoadNativeContextSlot(dst, Context::GLOBAL_PROXY_INDEX);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  ASM_CODE_COMMENT(this);
  CompareAndBranch(in.W(), Operand(kClearedWeakHeapObjectLower32), eq,
                   target_if_cleared);

  and_(out, in, Operand(~kWeakHeapObjectMask));
}

void MacroAssembler::EmitIncrementCounter(StatsCounter* counter, int value,
                                          Register scratch1,
                                          Register scratch2) {
  ASM_CODE_COMMENT(this);
  DCHECK_NE(value, 0);
  if (v8_flags.native_code_counters && counter->Enabled()) {
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Mov(scratch2, ExternalReference::Create(counter));
    Ldr(scratch1.W(), MemOperand(scratch2));
    Add(scratch1.W(), scratch1.W(), value);
    Str(scratch1.W(), MemOperand(scratch2));
  }
}

void MacroAssembler::JumpIfObjectType(Register object, Register map,
                                      Register type_reg, InstanceType type,
                                      Label* if_cond_pass, Condition cond) {
  ASM_CODE_COMMENT(this);
  CompareObjectType(object, map, type_reg, type);
  B(cond, if_cond_pass);
}

// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  ASM_CODE_COMMENT(this);
  LoadMap(map, object);
  CompareInstanceType(map, type_reg, type);
}

void TurboAssembler::LoadMap(Register dst, Register object) {
  ASM_CODE_COMMENT(this);
  LoadTaggedPointerField(dst, FieldMemOperand(object, HeapObject::kMapOffset));
}

// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  ASM_CODE_COMMENT(this);
  Ldrh(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Cmp(type_reg, type);
}

// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareInstanceTypeRange(Register map, Register type_reg,
                                              InstanceType lower_limit,
                                              InstanceType higher_limit) {
  ASM_CODE_COMMENT(this);
  DCHECK_LT(lower_limit, higher_limit);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  Ldrh(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Sub(scratch, type_reg, Operand(lower_limit));
  Cmp(scratch, Operand(higher_limit - lower_limit));
}

void MacroAssembler::LoadElementsKindFromMap(Register result, Register map) {
  ASM_CODE_COMMENT(this);
  // Load the map's "bit field 2".
  Ldrb(result, FieldMemOperand(map, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  DecodeField<Map::Bits2::ElementsKindBits>(result);
}

void MacroAssembler::CompareRoot(const Register& obj, RootIndex index) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  DCHECK(!AreAliased(obj, temp));
  LoadRoot(temp, index);
  CmpTagged(obj, temp);
}

void MacroAssembler::JumpIfRoot(const Register& obj, RootIndex index,
                                Label* if_equal) {
  CompareRoot(obj, index);
  B(eq, if_equal);
}

void MacroAssembler::JumpIfNotRoot(const Register& obj, RootIndex index,
                                   Label* if_not_equal) {
  CompareRoot(obj, index);
  B(ne, if_not_equal);
}

void MacroAssembler::JumpIfIsInRange(const Register& value,
                                     unsigned lower_limit,
                                     unsigned higher_limit,
                                     Label* on_in_range) {
  ASM_CODE_COMMENT(this);
  if (lower_limit != 0) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireW();
    Sub(scratch, value, Operand(lower_limit));
    CompareAndBranch(scratch, Operand(higher_limit - lower_limit), ls,
                     on_in_range);
  } else {
    CompareAndBranch(value, Operand(higher_limit - lower_limit), ls,
                     on_in_range);
  }
}

void TurboAssembler::LoadTaggedPointerField(const Register& destination,
                                            const MemOperand& field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedPointer(destination, field_operand);
  } else {
    Ldr(destination, field_operand);
  }
}

void TurboAssembler::LoadAnyTaggedField(const Register& destination,
                                        const MemOperand& field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressAnyTagged(destination, field_operand);
  } else {
    Ldr(destination, field_operand);
  }
}

void TurboAssembler::LoadTaggedSignedField(const Register& destination,
                                           const MemOperand& field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    DecompressTaggedSigned(destination, field_operand);
  } else {
    Ldr(destination, field_operand);
  }
}

void TurboAssembler::SmiUntagField(Register dst, const MemOperand& src) {
  SmiUntag(dst, src);
}

void TurboAssembler::StoreTaggedField(const Register& value,
                                      const MemOperand& dst_field_operand) {
  if (COMPRESS_POINTERS_BOOL) {
    Str(value.W(), dst_field_operand);
  } else {
    Str(value, dst_field_operand);
  }
}

void TurboAssembler::AtomicStoreTaggedField(const Register& value,
                                            const Register& dst_base,
                                            const Register& dst_index,
                                            const Register& temp) {
  Add(temp, dst_base, dst_index);
  if (COMPRESS_POINTERS_BOOL) {
    Stlr(value.W(), temp);
  } else {
    Stlr(value, temp);
  }
}

void TurboAssembler::DecompressTaggedSigned(const Register& destination,
                                            const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Ldr(destination.W(), field_operand);
  if (v8_flags.debug_code) {
    // Corrupt the top 32 bits. Made up of 16 fixed bits and 16 pc offset bits.
    Add(destination, destination,
        ((kDebugZapValue << 16) | (pc_offset() & 0xffff)) << 32);
  }
}

void TurboAssembler::DecompressTaggedPointer(const Register& destination,
                                             const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Ldr(destination.W(), field_operand);
  Add(destination, kPtrComprCageBaseRegister, destination);
}

void TurboAssembler::DecompressTaggedPointer(const Register& destination,
                                             const Register& source) {
  ASM_CODE_COMMENT(this);
  Add(destination, kPtrComprCageBaseRegister, Operand(source, UXTW));
}

void TurboAssembler::DecompressAnyTagged(const Register& destination,
                                         const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Ldr(destination.W(), field_operand);
  Add(destination, kPtrComprCageBaseRegister, destination);
}

void TurboAssembler::AtomicDecompressTaggedSigned(const Register& destination,
                                                  const Register& base,
                                                  const Register& index,
                                                  const Register& temp) {
  ASM_CODE_COMMENT(this);
  Add(temp, base, index);
  Ldar(destination.W(), temp);
  if (v8_flags.debug_code) {
    // Corrupt the top 32 bits. Made up of 16 fixed bits and 16 pc offset bits.
    Add(destination, destination,
        ((kDebugZapValue << 16) | (pc_offset() & 0xffff)) << 32);
  }
}

void TurboAssembler::AtomicDecompressTaggedPointer(const Register& destination,
                                                   const Register& base,
                                                   const Register& index,
                                                   const Register& temp) {
  ASM_CODE_COMMENT(this);
  Add(temp, base, index);
  Ldar(destination.W(), temp);
  Add(destination, kPtrComprCageBaseRegister, destination);
}

void TurboAssembler::AtomicDecompressAnyTagged(const Register& destination,
                                               const Register& base,
                                               const Register& index,
                                               const Register& temp) {
  ASM_CODE_COMMENT(this);
  Add(temp, base, index);
  Ldar(destination.W(), temp);
  Add(destination, kPtrComprCageBaseRegister, destination);
}

void TurboAssembler::CheckPageFlag(const Register& object, int mask,
                                   Condition cc, Label* condition_met) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  And(scratch, object, ~kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, BasicMemoryChunk::kFlagsOffset));
  if (cc == eq) {
    TestAndBranchIfAnySet(scratch, mask, condition_met);
  } else {
    DCHECK_EQ(cc, ne);
    TestAndBranchIfAllClear(scratch, mask, condition_met);
  }
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value,
                                      LinkRegisterStatus lr_status,
                                      SaveFPRegsMode save_fp,
                                      SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  DCHECK(!AreAliased(object, value));
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip the barrier if writing a smi.
  if (smi_check == SmiCheck::kInline) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so offset must be a multiple of kTaggedSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Verify slot_address");
    Label ok;
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();
    DCHECK(!AreAliased(object, value, scratch));
    Add(scratch, object, offset - kHeapObjectTag);
    Tst(scratch, kTaggedSize - 1);
    B(eq, &ok);
    Abort(AbortReason::kUnalignedCellInWriteBarrier);
    Bind(&ok);
  }

  RecordWrite(object, Operand(offset - kHeapObjectTag), value, lr_status,
              save_fp, SmiCheck::kOmit);

  Bind(&done);
}

void TurboAssembler::EncodeSandboxedPointer(const Register& value) {
  ASM_CODE_COMMENT(this);
#ifdef V8_ENABLE_SANDBOX
  Sub(value, value, kPtrComprCageBaseRegister);
  Mov(value, Operand(value, LSL, kSandboxedPointerShift));
#else
  UNREACHABLE();
#endif
}

void TurboAssembler::DecodeSandboxedPointer(const Register& value) {
  ASM_CODE_COMMENT(this);
#ifdef V8_ENABLE_SANDBOX
  Add(value, kPtrComprCageBaseRegister,
      Operand(value, LSR, kSandboxedPointerShift));
#else
  UNREACHABLE();
#endif
}

void TurboAssembler::LoadSandboxedPointerField(
    const Register& destination, const MemOperand& field_operand) {
  ASM_CODE_COMMENT(this);
  Ldr(destination, field_operand);
  DecodeSandboxedPointer(destination);
}

void TurboAssembler::StoreSandboxedPointerField(
    const Register& value, const MemOperand& dst_field_operand) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  Mov(scratch, value);
  EncodeSandboxedPointer(scratch);
  Str(scratch, dst_field_operand);
}

void TurboAssembler::LoadExternalPointerField(Register destination,
                                              MemOperand field_operand,
                                              ExternalPointerTag tag,
                                              Register isolate_root) {
  DCHECK(!AreAliased(destination, isolate_root));
  ASM_CODE_COMMENT(this);
#ifdef V8_ENABLE_SANDBOX
  if (IsSandboxedExternalPointerType(tag)) {
    DCHECK_NE(kExternalPointerNullTag, tag);
    DCHECK(!IsSharedExternalPointerType(tag));
    UseScratchRegisterScope temps(this);
    Register external_table = temps.AcquireX();
    if (isolate_root == no_reg) {
      DCHECK(root_array_available_);
      isolate_root = kRootRegister;
    }
    Ldr(external_table,
        MemOperand(isolate_root,
                   IsolateData::external_pointer_table_offset() +
                       Internals::kExternalPointerTableBufferOffset));
    Ldr(destination.W(), field_operand);
    // MemOperand doesn't support LSR currently (only LSL), so here we do the
    // offset computation separately first.
    static_assert(kExternalPointerIndexShift > kSystemPointerSizeLog2);
    int shift_amount = kExternalPointerIndexShift - kSystemPointerSizeLog2;
    Mov(destination, Operand(destination, LSR, shift_amount));
    Ldr(destination, MemOperand(external_table, destination));
    And(destination, destination, Immediate(~tag));
    return;
  }
#endif  // V8_ENABLE_SANDBOX
  Ldr(destination, field_operand);
}

void TurboAssembler::MaybeSaveRegisters(RegList registers) {
  if (registers.is_empty()) return;
  ASM_CODE_COMMENT(this);
  CPURegList regs(kXRegSizeInBits, registers);
  // If we were saving LR, we might need to sign it.
  DCHECK(!regs.IncludesAliasOf(lr));
  regs.Align();
  PushCPURegList(regs);
}

void TurboAssembler::MaybeRestoreRegisters(RegList registers) {
  if (registers.is_empty()) return;
  ASM_CODE_COMMENT(this);
  CPURegList regs(kXRegSizeInBits, registers);
  // If we were saving LR, we might need to sign it.
  DCHECK(!regs.IncludesAliasOf(lr));
  regs.Align();
  PopCPURegList(regs);
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object, Operand offset,
                                             SaveFPRegsMode fp_mode) {
  ASM_CODE_COMMENT(this);
  RegList registers = WriteBarrierDescriptor::ComputeSavedRegisters(object);
  MaybeSaveRegisters(registers);

  MoveObjectAndSlot(WriteBarrierDescriptor::ObjectRegister(),
                    WriteBarrierDescriptor::SlotAddressRegister(), object,
                    offset);

  Call(isolate()->builtins()->code_handle(
           Builtins::GetEphemeronKeyBarrierStub(fp_mode)),
       RelocInfo::CODE_TARGET);
  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStubSaveRegisters(Register object,
                                                      Operand offset,
                                                      SaveFPRegsMode fp_mode,
                                                      StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  RegList registers = WriteBarrierDescriptor::ComputeSavedRegisters(object);
  MaybeSaveRegisters(registers);

  Register object_parameter = WriteBarrierDescriptor::ObjectRegister();
  Register slot_address_parameter =
      WriteBarrierDescriptor::SlotAddressRegister();
  MoveObjectAndSlot(object_parameter, slot_address_parameter, object, offset);

  CallRecordWriteStub(object_parameter, slot_address_parameter, fp_mode, mode);

  MaybeRestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(Register object, Register slot_address,
                                         SaveFPRegsMode fp_mode,
                                         StubCallMode mode) {
  ASM_CODE_COMMENT(this);
  DCHECK_EQ(WriteBarrierDescriptor::ObjectRegister(), object);
  DCHECK_EQ(WriteBarrierDescriptor::SlotAddressRegister(), slot_address);
#if V8_ENABLE_WEBASSEMBLY
  if (mode == StubCallMode::kCallWasmRuntimeStub) {
    auto wasm_target = wasm::WasmCode::GetRecordWriteStub(fp_mode);
    Call(wasm_target, RelocInfo::WASM_STUB_CALL);
#else
  if (false) {
#endif
  } else {
    Builtin builtin = Builtins::GetRecordWriteStub(fp_mode);
    CallBuiltin(builtin);
  }
}

void TurboAssembler::MoveObjectAndSlot(Register dst_object, Register dst_slot,
                                       Register object, Operand offset) {
  ASM_CODE_COMMENT(this);
  DCHECK_NE(dst_object, dst_slot);
  // If `offset` is a register, it cannot overlap with `object`.
  DCHECK_IMPLIES(!offset.IsImmediate(), offset.reg() != object);

  // If the slot register does not overlap with the object register, we can
  // overwrite it.
  if (dst_slot != object) {
    Add(dst_slot, object, offset);
    Mov(dst_object, object);
    return;
  }

  DCHECK_EQ(dst_slot, object);

  // If the destination object register does not overlap with the offset
  // register, we can overwrite it.
  if (offset.IsImmediate() || (offset.reg() != dst_object)) {
    Mov(dst_object, dst_slot);
    Add(dst_slot, dst_slot, offset);
    return;
  }

  DCHECK_EQ(dst_object, offset.reg());

  // We only have `dst_slot` and `dst_object` left as distinct registers so we
  // have to swap them. We write this as a add+sub sequence to avoid using a
  // scratch register.
  Add(dst_slot, dst_slot, dst_object);
  Sub(dst_object, dst_slot, dst_object);
}

// If lr_status is kLRHasBeenSaved, lr will be clobbered.
//
// The register 'object' contains a heap object pointer. The heap object tag is
// shifted away.
void MacroAssembler::RecordWrite(Register object, Operand offset,
                                 Register value, LinkRegisterStatus lr_status,
                                 SaveFPRegsMode fp_mode, SmiCheck smi_check) {
  ASM_CODE_COMMENT(this);
  ASM_LOCATION_IN_ASSEMBLER("MacroAssembler::RecordWrite");
  DCHECK(!AreAliased(object, value));

  if (v8_flags.debug_code) {
    ASM_CODE_COMMENT_STRING(this, "Verify slot_address");
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();
    DCHECK(!AreAliased(object, value, temp));
    Add(temp, object, offset);
    LoadTaggedPointerField(temp, MemOperand(temp));
    Cmp(temp, value);
    Check(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite);
  }

  if (v8_flags.disable_write_barriers) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == SmiCheck::kInline) {
    DCHECK_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }
  CheckPageFlag(value,
                MemoryChunk::kPointersToHereAreInterestingOrInSharedHeapMask,
                ne, &done);

  CheckPageFlag(object, MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    Push<TurboAssembler::kSignLR>(padreg, lr);
  }
  Register slot_address = WriteBarrierDescriptor::SlotAddressRegister();
  DCHECK(!AreAliased(object, slot_address, value));
  // TODO(cbruni): Turn offset into int.
  DCHECK(offset.IsImmediate());
  Add(slot_address, object, offset);
  CallRecordWriteStub(object, slot_address, fp_mode);
  if (lr_status == kLRHasNotBeenSaved) {
    Pop<TurboAssembler::kAuthLR>(lr, padreg);
  }
  if (v8_flags.debug_code) Mov(slot_address, Operand(kZapValue));

  Bind(&done);
}

void TurboAssembler::Check(Condition cond, AbortReason reason) {
  Label ok;
  B(cond, &ok);
  Abort(reason);
  // Will not return here.
  Bind(&ok);
}

void TurboAssembler::Trap() { Brk(0); }
void TurboAssembler::DebugBreak() { Debug("DebugBreak", 0, BREAK); }

void TurboAssembler::Abort(AbortReason reason) {
  ASM_CODE_COMMENT(this);
  if (v8_flags.code_comments) {
    RecordComment("Abort message: ");
    RecordComment(GetAbortReason(reason));
  }

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    Brk(0);
    return;
  }

  // We need some scratch registers for the MacroAssembler, so make sure we have
  // some. This is safe here because Abort never returns.
  uint64_t old_tmp_list = TmpList()->bits();
  TmpList()->Combine(MacroAssembler::DefaultTmpList());

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NO_FRAME_TYPE);
    Mov(w0, static_cast<int>(reason));
    Call(ExternalReference::abort_with_reason());
    return;
  }

  // Avoid infinite recursion; Push contains some assertions that use Abort.
  HardAbortScope hard_aborts(this);

  Mov(x1, Smi::FromInt(static_cast<int>(reason)));

  {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NO_FRAME_TYPE);
    if (root_array_available()) {
      // Generate an indirect call via builtins entry table here in order to
      // ensure that the interpreter_entry_return_pc_offset is the same for
      // InterpreterEntryTrampoline and InterpreterEntryTrampolineForProfiling
      // when v8_flags.debug_code is enabled.
      UseScratchRegisterScope temps(this);
      Register scratch = temps.AcquireX();
      LoadEntryFromBuiltin(Builtin::kAbort, scratch);
      Call(scratch);
    } else {
      Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
    }
  }

  TmpList()->set_bits(old_tmp_list);
}

void MacroAssembler::LoadNativeContextSlot(Register dst, int index) {
  LoadMap(dst, cp);
  LoadTaggedPointerField(
      dst, FieldMemOperand(
               dst, Map::kConstructorOrBackPointerOrNativeContextOffset));
  LoadTaggedPointerField(dst, MemOperand(dst, Context::SlotOffset(index)));
}

// This is the main Printf implementation. All other Printf variants call
// PrintfNoPreserve after setting up one or more PreserveRegisterScopes.
void TurboAssembler::PrintfNoPreserve(const char* format,
                                      const CPURegister& arg0,
                                      const CPURegister& arg1,
                                      const CPURegister& arg2,
                                      const CPURegister& arg3) {
  ASM_CODE_COMMENT(this);
  // We cannot handle a caller-saved stack pointer. It doesn't make much sense
  // in most cases anyway, so this restriction shouldn't be too serious.
  DCHECK(!kCallerSaved.IncludesAliasOf(sp));

  // The provided arguments, and their proper procedure-call standard registers.
  CPURegister args[kPrintfMaxArgCount] = {arg0, arg1, arg2, arg3};
  CPURegister pcs[kPrintfMaxArgCount] = {NoReg, NoReg, NoReg, NoReg};

  int arg_count = kPrintfMaxArgCount;

  // The PCS varargs registers for printf. Note that x0 is used for the printf
  // format string.
  static const CPURegList kPCSVarargs =
      CPURegList(CPURegister::kRegister, kXRegSizeInBits, 1, arg_count);
  static const CPURegList kPCSVarargsFP =
      CPURegList(CPURegister::kVRegister, kDRegSizeInBits, 0, arg_count - 1);

  // We can use caller-saved registers as scratch values, except for the
  // arguments and the PCS registers where they might need to go.
  CPURegList tmp_list = kCallerSaved;
  tmp_list.Remove(x0);  // Used to pass the format string.
  tmp_list.Remove(kPCSVarargs);
  tmp_list.Remove(arg0, arg1, arg2, arg3);

  CPURegList fp_tmp_list = kCallerSavedV;
  fp_tmp_list.Remove(kPCSVarargsFP);
  fp_tmp_list.Remove(arg0, arg1, arg2, arg3);

  // Override the TurboAssembler's scratch register list. The lists will be
  // reset automatically at the end of the UseScratchRegisterScope.
  UseScratchRegisterScope temps(this);
  TmpList()->set_bits(tmp_list.bits());
  FPTmpList()->set_bits(fp_tmp_list.bits());

  // Copies of the printf vararg registers that we can pop from.
  CPURegList pcs_varargs = kPCSVarargs;
#ifndef V8_OS_WIN
  CPURegList pcs_varargs_fp = kPCSVarargsFP;
#endif

  // Place the arguments. There are lots of clever tricks and optimizations we
  // could use here, but Printf is a debug tool so instead we just try to keep
  // it simple: Move each input that isn't already in the right place to a
  // scratch register, then move everything back.
  for (unsigned i = 0; i < kPrintfMaxArgCount; i++) {
    // Work out the proper PCS register for this argument.
    if (args[i].IsRegister()) {
      pcs[i] = pcs_varargs.PopLowestIndex().X();
      // We might only need a W register here. We need to know the size of the
      // argument so we can properly encode it for the simulator call.
      if (args[i].Is32Bits()) pcs[i] = pcs[i].W();
    } else if (args[i].IsVRegister()) {
      // In C, floats are always cast to doubles for varargs calls.
#ifdef V8_OS_WIN
      // In case of variadic functions SIMD and Floating-point registers
      // aren't used. The general x0-x7 should be used instead.
      // https://docs.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions
      pcs[i] = pcs_varargs.PopLowestIndex().X();
#else
      pcs[i] = pcs_varargs_fp.PopLowestIndex().D();
#endif
    } else {
      DCHECK(args[i].IsNone());
      arg_count = i;
      break;
    }

    // If the argument is already in the right place, leave it where it is.
    if (args[i].Aliases(pcs[i])) continue;

    // Otherwise, if the argument is in a PCS argument register, allocate an
    // appropriate scratch register and then move it out of the way.
    if (kPCSVarargs.IncludesAliasOf(args[i]) ||
        kPCSVarargsFP.IncludesAliasOf(args[i])) {
      if (args[i].IsRegister()) {
        Register old_arg = args[i].Reg();
        Register new_arg = temps.AcquireSameSizeAs(old_arg);
        Mov(new_arg, old_arg);
        args[i] = new_arg;
      } else {
        VRegister old_arg = args[i].VReg();
        VRegister new_arg = temps.AcquireSameSizeAs(old_arg);
        Fmov(new_arg, old_arg);
        args[i] = new_arg;
      }
    }
  }

  // Do a second pass to move values into their final positions and perform any
  // conversions that may be required.
  for (int i = 0; i < arg_count; i++) {
#ifdef V8_OS_WIN
    if (args[i].IsVRegister()) {
      if (pcs[i].SizeInBytes() != args[i].SizeInBytes()) {
        // If the argument is half- or single-precision
        // converts to double-precision before that is
        // moved into the one of X scratch register.
        VRegister temp0 = temps.AcquireD();
        Fcvt(temp0.VReg(), args[i].VReg());
        Fmov(pcs[i].Reg(), temp0);
      } else {
        Fmov(pcs[i].Reg(), args[i].VReg());
      }
    } else {
      Mov(pcs[i].Reg(), args[i].Reg(), kDiscardForSameWReg);
    }
#else
    DCHECK(pcs[i].type() == args[i].type());
    if (pcs[i].IsRegister()) {
      Mov(pcs[i].Reg(), args[i].Reg(), kDiscardForSameWReg);
    } else {
      DCHECK(pcs[i].IsVRegister());
      if (pcs[i].SizeInBytes() == args[i].SizeInBytes()) {
        Fmov(pcs[i].VReg(), args[i].VReg());
      } else {
        Fcvt(pcs[i].VReg(), args[i].VReg());
      }
    }
#endif
  }

  // Load the format string into x0, as per the procedure-call standard.
  //
  // To make the code as portable as possible, the format string is encoded
  // directly in the instruction stream. It might be cleaner to encode it in a
  // literal pool, but since Printf is usually used for debugging, it is
  // beneficial for it to be minimally dependent on other features.
  Label format_address;
  Adr(x0, &format_address);

  // Emit the format string directly in the instruction stream.
  {
    BlockPoolsScope scope(this);
    Label after_data;
    B(&after_data);
    Bind(&format_address);
    EmitStringData(format);
    Unreachable();
    Bind(&after_data);
  }

  CallPrintf(arg_count, pcs);
}

void TurboAssembler::CallPrintf(int arg_count, const CPURegister* args) {
  ASM_CODE_COMMENT(this);
  // A call to printf needs special handling for the simulator, since the system
  // printf function will use a different instruction set and the procedure-call
  // standard will not be compatible.
  if (options().enable_simulator_code) {
    InstructionAccurateScope scope(this, kPrintfLength / kInstrSize);
    hlt(kImmExceptionIsPrintf);
    dc32(arg_count);  // kPrintfArgCountOffset

    // Determine the argument pattern.
    uint32_t arg_pattern_list = 0;
    for (int i = 0; i < arg_count; i++) {
      uint32_t arg_pattern;
      if (args[i].IsRegister()) {
        arg_pattern = args[i].Is32Bits() ? kPrintfArgW : kPrintfArgX;
      } else {
        DCHECK(args[i].Is64Bits());
        arg_pattern = kPrintfArgD;
      }
      DCHECK(arg_pattern < (1 << kPrintfArgPatternBits));
      arg_pattern_list |= (arg_pattern << (kPrintfArgPatternBits * i));
    }
    dc32(arg_pattern_list);  // kPrintfArgPatternListOffset
    return;
  }

  Call(ExternalReference::printf_function());
}

void TurboAssembler::Printf(const char* format, CPURegister arg0,
                            CPURegister arg1, CPURegister arg2,
                            CPURegister arg3) {
  ASM_CODE_COMMENT(this);
  // Printf is expected to preserve all registers, so make sure that none are
  // available as scratch registers until we've preserved them.
  uint64_t old_tmp_list = TmpList()->bits();
  uint64_t old_fp_tmp_list = FPTmpList()->bits();
  TmpList()->set_bits(0);
  FPTmpList()->set_bits(0);

  CPURegList saved_registers = kCallerSaved;
  saved_registers.Align();

  // Preserve all caller-saved registers as well as NZCV.
  // PushCPURegList asserts that the size of each list is a multiple of 16
  // bytes.
  PushCPURegList(saved_registers);
  PushCPURegList(kCallerSavedV);

  // We can use caller-saved registers as scratch values (except for argN).
  CPURegList tmp_list = saved_registers;
  CPURegList fp_tmp_list = kCallerSavedV;
  tmp_list.Remove(arg0, arg1, arg2, arg3);
  fp_tmp_list.Remove(arg0, arg1, arg2, arg3);
  TmpList()->set_bits(tmp_list.bits());
  FPTmpList()->set_bits(fp_tmp_list.bits());

  {
    UseScratchRegisterScope temps(this);
    // If any of the arguments are the current stack pointer, allocate a new
    // register for them, and adjust the value to compensate for pushing the
    // caller-saved registers.
    bool arg0_sp = arg0.is_valid() && sp.Aliases(arg0);
    bool arg1_sp = arg1.is_valid() && sp.Aliases(arg1);
    bool arg2_sp = arg2.is_valid() && sp.Aliases(arg2);
    bool arg3_sp = arg3.is_valid() && sp.Aliases(arg3);
    if (arg0_sp || arg1_sp || arg2_sp || arg3_sp) {
      // Allocate a register to hold the original stack pointer value, to pass
      // to PrintfNoPreserve as an argument.
      Register arg_sp = temps.AcquireX();
      Add(arg_sp, sp,
          saved_registers.TotalSizeInBytes() +
              kCallerSavedV.TotalSizeInBytes());
      if (arg0_sp) arg0 = Register::Create(arg_sp.code(), arg0.SizeInBits());
      if (arg1_sp) arg1 = Register::Create(arg_sp.code(), arg1.SizeInBits());
      if (arg2_sp) arg2 = Register::Create(arg_sp.code(), arg2.SizeInBits());
      if (arg3_sp) arg3 = Register::Create(arg_sp.code(), arg3.SizeInBits());
    }

    // Preserve NZCV.
    {
      UseScratchRegisterScope temps(this);
      Register tmp = temps.AcquireX();
      Mrs(tmp, NZCV);
      Push(tmp, xzr);
    }

    PrintfNoPreserve(format, arg0, arg1, arg2, arg3);

    // Restore NZCV.
    {
      UseScratchRegisterScope temps(this);
      Register tmp = temps.AcquireX();
      Pop(xzr, tmp);
      Msr(NZCV, tmp);
    }
  }

  PopCPURegList(kCallerSavedV);
  PopCPURegList(saved_registers);

  TmpList()->set_bits(old_tmp_list);
  FPTmpList()->set_bits(old_fp_tmp_list);
}

UseScratchRegisterScope::~UseScratchRegisterScope() {
  available_->set_bits(old_available_);
  availablefp_->set_bits(old_availablefp_);
}

Register UseScratchRegisterScope::AcquireSameSizeAs(const Register& reg) {
  int code = AcquireNextAvailable(available_).code();
  return Register::Create(code, reg.SizeInBits());
}

VRegister UseScratchRegisterScope::AcquireSameSizeAs(const VRegister& reg) {
  int code = AcquireNextAvailable(availablefp_).code();
  return VRegister::Create(code, reg.SizeInBits());
}

CPURegister UseScratchRegisterScope::AcquireNextAvailable(
    CPURegList* available) {
  CHECK(!available->IsEmpty());
  CPURegister result = available->PopLowestIndex();
  DCHECK(!AreAliased(result, xzr, sp));
  return result;
}

void TurboAssembler::ComputeCodeStartAddress(const Register& rd) {
  // We can use adr to load a pc relative location.
  adr(rd, -pc_offset());
}

void TurboAssembler::RestoreFPAndLR() {
  static_assert(StandardFrameConstants::kCallerFPOffset + kSystemPointerSize ==
                    StandardFrameConstants::kCallerPCOffset,
                "Offsets must be consecutive for ldp!");
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  // Make sure we can use x16 and x17.
  UseScratchRegisterScope temps(this);
  temps.Exclude(x16, x17);
  // We can load the return address directly into x17.
  Add(x16, fp, StandardFrameConstants::kCallerSPOffset);
  Ldp(fp, x17, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  Autib1716();
  Mov(lr, x17);
#else
  Ldp(fp, lr, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
#endif
}

#if V8_ENABLE_WEBASSEMBLY
void TurboAssembler::StoreReturnAddressInWasmExitFrame(Label* return_location) {
  UseScratchRegisterScope temps(this);
  temps.Exclude(x16, x17);
  Adr(x17, return_location);
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  Add(x16, fp, WasmExitFrameConstants::kCallingPCOffset + kSystemPointerSize);
  Pacib1716();
#endif
  Str(x17, MemOperand(fp, WasmExitFrameConstants::kCallingPCOffset));
}
#endif  // V8_ENABLE_WEBASSEMBLY

void TurboAssembler::PopcntHelper(Register dst, Register src) {
  UseScratchRegisterScope temps(this);
  VRegister scratch = temps.AcquireV(kFormat8B);
  VRegister tmp = src.Is32Bits() ? scratch.S() : scratch.D();
  Fmov(tmp, src);
  Cnt(scratch, scratch);
  Addv(scratch.B(), scratch);
  Fmov(dst, tmp);
}

void TurboAssembler::I64x2BitMask(Register dst, VRegister src) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope scope(this);
  VRegister tmp1 = scope.AcquireV(kFormat2D);
  Register tmp2 = scope.AcquireX();
  Ushr(tmp1.V2D(), src.V2D(), 63);
  Mov(dst.X(), tmp1.D(), 0);
  Mov(tmp2.X(), tmp1.D(), 1);
  Add(dst.W(), dst.W(), Operand(tmp2.W(), LSL, 1));
}

void TurboAssembler::I64x2AllTrue(Register dst, VRegister src) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope scope(this);
  VRegister tmp = scope.AcquireV(kFormat2D);
  Cmeq(tmp.V2D(), src.V2D(), 0);
  Addp(tmp.D(), tmp);
  Fcmp(tmp.D(), tmp.D());
  Cset(dst, eq);
}

}  // namespace internal
}  // namespace v8

#undef __

#endif  // V8_TARGET_ARCH_ARM64
