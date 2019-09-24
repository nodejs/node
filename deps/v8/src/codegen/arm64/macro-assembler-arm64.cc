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
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames-inl.h"
#include "src/heap/heap-inl.h"  // For MemoryChunk.
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-code-manager.h"

// Satisfy cpplint check, but don't include platform-specific header. It is
// included recursively via macro-assembler.h.
#if 0
#include "src/codegen/arm64/macro-assembler-arm64.h"
#endif

namespace v8 {
namespace internal {

CPURegList TurboAssembler::DefaultTmpList() { return CPURegList(ip0, ip1); }

CPURegList TurboAssembler::DefaultFPTmpList() {
  return CPURegList(fp_scratch1, fp_scratch2);
}

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion) const {
  auto list = kCallerSaved;
  list.Remove(exclusion);
  list.Align();

  int bytes = list.Count() * kXRegSizeInBits / 8;

  if (fp_mode == kSaveFPRegs) {
    DCHECK_EQ(kCallerSavedV.Count() % 2, 0);
    bytes += kCallerSavedV.Count() * kDRegSizeInBits / 8;
  }
  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode,
                                    Register exclusion) {
  auto list = kCallerSaved;
  list.Remove(exclusion);
  list.Align();

  PushCPURegList(list);

  int bytes = list.Count() * kXRegSizeInBits / 8;

  if (fp_mode == kSaveFPRegs) {
    DCHECK_EQ(kCallerSavedV.Count() % 2, 0);
    PushCPURegList(kCallerSavedV);
    bytes += kCallerSavedV.Count() * kDRegSizeInBits / 8;
  }
  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    DCHECK_EQ(kCallerSavedV.Count() % 2, 0);
    PopCPURegList(kCallerSavedV);
    bytes += kCallerSavedV.Count() * kDRegSizeInBits / 8;
  }

  auto list = kCallerSaved;
  list.Remove(exclusion);
  list.Align();

  PopCPURegList(list);
  bytes += list.Count() * kXRegSizeInBits / 8;

  return bytes;
}

void TurboAssembler::LogicalMacro(const Register& rd, const Register& rn,
                                  const Operand& operand, LogicalOp op) {
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
      PreShiftImmMode mode = rn.Is(sp) ? kNoShift : kAnyShift;
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
    if (CountClearHalfWords(~imm, reg_size) >
        CountClearHalfWords(imm, reg_size)) {
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
    if (FLAG_embedded_builtins) {
      if (root_array_available_ && options().isolate_independent_code) {
        if (operand.ImmediateRMode() == RelocInfo::EXTERNAL_REFERENCE) {
          Address addr = static_cast<Address>(operand.ImmediateValue());
          ExternalReference reference = bit_cast<ExternalReference>(addr);
          IndirectLoadExternalReference(rd, reference);
          return;
        } else if (RelocInfo::IsEmbeddedObjectMode(operand.ImmediateRMode())) {
          Handle<HeapObject> x(
              reinterpret_cast<Address*>(operand.ImmediateValue()));
          IndirectLoadConstant(rd, x);
          return;
        }
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
    if (!rd.Is(operand.reg()) ||
        (rd.Is32Bits() && (discard_mode == kDontDiscardForSameWReg))) {
      Assembler::mov(rd, operand.reg());
    }
    // This case can handle writes into the system stack pointer directly.
    dst = rd;
  }

  // Copy the result to the system stack pointer.
  if (!dst.Is(rd)) {
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
  // TODO(all): Move 128-bit values in a more efficient way.
  DCHECK(vd.Is128Bits());
  UseScratchRegisterScope temps(this);
  Movi(vd.V2D(), lo);
  Register temp = temps.AcquireX();
  Mov(temp, hi);
  Ins(vd.V2D(), 1, temp);
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

unsigned TurboAssembler::CountClearHalfWords(uint64_t imm, unsigned reg_size) {
  DCHECK_EQ(reg_size % 8, 0);
  int count = 0;
  for (unsigned i = 0; i < (reg_size / 16); i++) {
    if ((imm & 0xFFFF) == 0) {
      count++;
    }
    imm >>= 16;
  }
  return count;
}

// The movz instruction can generate immediates containing an arbitrary 16-bit
// half-word, with remaining bits clear, eg. 0x00001234, 0x0000123400000000.
bool TurboAssembler::IsImmMovz(uint64_t imm, unsigned reg_size) {
  DCHECK((reg_size == kXRegSizeInBits) || (reg_size == kWRegSizeInBits));
  return CountClearHalfWords(imm, reg_size) >= ((reg_size / 16) - 1);
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
    int shift_low = CountTrailingZeros(imm, reg_size);
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
  if (operand.IsZero() && rd.Is(rn) && rd.Is64Bits() && rn.Is64Bits() &&
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
      if (rd.Is(sp)) {
        // If the destination is SP and flags will be set, we can't pre-shift
        // the immediate at all.
        mode = (S == SetFlags) ? kNoShift : kLimitShiftForSP;
      } else if (rn.Is(sp)) {
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
    next_veneer_pool_check_ = Min(
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
  DCHECK((reg.Is(NoReg) || type >= kBranchTypeFirstUsingReg) &&
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
                          const CPURegister& src2, const CPURegister& src3) {
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3));

  int count = 1 + src1.IsValid() + src2.IsValid() + src3.IsValid();
  int size = src0.SizeInBytes();
  DCHECK_EQ(0, (size * count) % 16);

  PushHelper(count, size, src0, src1, src2, src3);
}

void TurboAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3,
                          const CPURegister& src4, const CPURegister& src5,
                          const CPURegister& src6, const CPURegister& src7) {
  DCHECK(AreSameSizeAndType(src0, src1, src2, src3, src4, src5, src6, src7));

  int count = 5 + src5.IsValid() + src6.IsValid() + src6.IsValid();
  int size = src0.SizeInBytes();
  DCHECK_EQ(0, (size * count) % 16);

  PushHelper(4, size, src0, src1, src2, src3);
  PushHelper(count - 4, size, src4, src5, src6, src7);
}

void TurboAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  DCHECK(!AreAliased(dst0, dst1, dst2, dst3));
  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  DCHECK(dst0.IsValid());

  int count = 1 + dst1.IsValid() + dst2.IsValid() + dst3.IsValid();
  int size = dst0.SizeInBytes();
  DCHECK_EQ(0, (size * count) % 16);

  PopHelper(count, size, dst0, dst1, dst2, dst3);
}

void TurboAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3,
                         const CPURegister& dst4, const CPURegister& dst5,
                         const CPURegister& dst6, const CPURegister& dst7) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  DCHECK(!AreAliased(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7));
  DCHECK(AreSameSizeAndType(dst0, dst1, dst2, dst3, dst4, dst5, dst6, dst7));
  DCHECK(dst0.IsValid());

  int count = 5 + dst5.IsValid() + dst6.IsValid() + dst7.IsValid();
  int size = dst0.SizeInBytes();
  DCHECK_EQ(0, (size * count) % 16);

  PopHelper(4, size, dst0, dst1, dst2, dst3);
  PopHelper(count - 4, size, dst4, dst5, dst6, dst7);
}

void TurboAssembler::Push(const Register& src0, const VRegister& src1) {
  int size = src0.SizeInBytes() + src1.SizeInBytes();
  DCHECK_EQ(0, size % 16);

  // Reserve room for src0 and push src1.
  str(src1, MemOperand(sp, -size, PreIndex));
  // Fill the gap with src0.
  str(src0, MemOperand(sp, src1.SizeInBytes()));
}

void TurboAssembler::PushCPURegList(CPURegList registers) {
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

void TurboAssembler::Poke(const CPURegister& src, const Operand& offset) {
  if (offset.IsImmediate()) {
    DCHECK_GE(offset.ImmediateValue(), 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, AbortReason::kStackAccessBelowStackPointer);
  }

  Str(src, MemOperand(sp, offset));
}

void TurboAssembler::Peek(const CPURegister& dst, const Operand& offset) {
  if (offset.IsImmediate()) {
    DCHECK_GE(offset.ImmediateValue(), 0);
  } else if (emit_debug_code()) {
    Cmp(xzr, offset);
    Check(le, AbortReason::kStackAccessBelowStackPointer);
  }

  Ldr(dst, MemOperand(sp, offset));
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
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  MemOperand tos(sp, -2 * static_cast<int>(kXRegSize), PreIndex);

  stp(d14, d15, tos);
  stp(d12, d13, tos);
  stp(d10, d11, tos);
  stp(d8, d9, tos);

  stp(x29, x30, tos);
#if defined(V8_OS_WIN)
  // kFramePointerOffsetInPushCalleeSavedRegisters is the offset from tos at
  // the end of this function to the saved caller's fp/x29 pointer. It includes
  // registers from x19 to x28, which is 10 pointers defined by below stp
  // instructions.
  STATIC_ASSERT(kFramePointerOffsetInPushCalleeSavedRegisters ==
                10 * kSystemPointerSize);
#endif  // defined(V8_OS_WIN)
  stp(x27, x28, tos);
  stp(x25, x26, tos);
  stp(x23, x24, tos);
  stp(x21, x22, tos);
  stp(x19, x20, tos);
}

void MacroAssembler::PopCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  MemOperand tos(sp, 2 * kXRegSize, PostIndex);

  ldp(x19, x20, tos);
  ldp(x21, x22, tos);
  ldp(x23, x24, tos);
  ldp(x25, x26, tos);
  ldp(x27, x28, tos);
  ldp(x29, x30, tos);

  ldp(d8, d9, tos);
  ldp(d10, d11, tos);
  ldp(d12, d13, tos);
  ldp(d14, d15, tos);
}

void TurboAssembler::AssertSpAligned() {
  if (emit_debug_code()) {
    HardAbortScope hard_abort(this);  // Avoid calls to Abort.
    // Arm64 requires the stack pointer to be 16-byte aligned prior to address
    // calculation.
    UseScratchRegisterScope scope(this);
    Register temp = scope.AcquireX();
    Mov(temp, sp);
    Tst(temp, 15);
    Check(eq, AbortReason::kUnexpectedStackPointer);
  }
}

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
  DCHECK(!AreAliased(dst, src, count));

  if (emit_debug_code()) {
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

  int direction = (mode == kDstLessThanSrc) ? 1 : -1;
  UseScratchRegisterScope scope(this);
  VRegister temp0 = scope.AcquireD();
  VRegister temp1 = scope.AcquireD();

  Label pairs, loop, done;

  Tbz(count, 0, &pairs);
  Ldr(temp0, MemOperand(src, direction * kSystemPointerSize, PostIndex));
  Sub(count, count, 1);
  Str(temp0, MemOperand(dst, direction * kSystemPointerSize, PostIndex));

  Bind(&pairs);
  if (mode == kSrcLessThanDst) {
    // Adjust pointers for post-index ldp/stp with negative offset:
    Sub(dst, dst, kSystemPointerSize);
    Sub(src, src, kSystemPointerSize);
  }
  Bind(&loop);
  Cbz(count, &done);
  Ldp(temp0, temp1,
      MemOperand(src, 2 * direction * kSystemPointerSize, PostIndex));
  Sub(count, count, 2);
  Stp(temp0, temp1,
      MemOperand(dst, 2 * direction * kSystemPointerSize, PostIndex));
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

void TurboAssembler::AssertFPCRState(Register fpcr) {
  if (emit_debug_code()) {
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
    STATIC_ASSERT(FPTieEven == 0);
    Tst(fpcr, RMode_mask);
    B(eq, &done);

    Bind(&unexpected_mode);
    Abort(AbortReason::kUnexpectedFPCRMode);

    Bind(&done);
  }
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
  // TODO(jbramley): Most root values are constants, and can be synthesized
  // without a load. Refer to the ARM back end for details.
  Ldr(destination,
      MemOperand(kRootRegister, RootRegisterOffsetForRootIndex(index)));
}

void MacroAssembler::LoadObject(Register result, Handle<Object> object) {
  AllowDeferredHandleDereference heap_object_check;
  if (object->IsHeapObject()) {
    Mov(result, Handle<HeapObject>::cast(object));
  } else {
    Mov(result, Operand(Smi::cast(*object)));
  }
}

void TurboAssembler::Move(Register dst, Smi src) { Mov(dst, src); }

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
  DCHECK(!lhs.Is(rhs));
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, rhs);
  Mov(rhs, lhs);
  Mov(lhs, temp);
}

void TurboAssembler::Swap(VRegister lhs, VRegister rhs) {
  DCHECK(lhs.IsSameSizeAndType(rhs));
  DCHECK(!lhs.Is(rhs));
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

void TurboAssembler::AssertSmi(Register object, AbortReason reason) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(eq, reason);
  }
}

void MacroAssembler::AssertNotSmi(Register object, AbortReason reason) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    Tst(object, kSmiTagMask);
    Check(ne, reason);
  }
}

void MacroAssembler::AssertConstructor(Register object) {
  if (emit_debug_code()) {
    AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAConstructor);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    LoadTaggedPointerField(temp,
                           FieldMemOperand(object, HeapObject::kMapOffset));
    Ldrb(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
    Tst(temp, Operand(Map::IsConstructorBit::kMask));

    Check(ne, AbortReason::kOperandIsNotAConstructor);
  }
}

void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAFunction);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    CompareObjectType(object, temp, temp, JS_FUNCTION_TYPE);
    Check(eq, AbortReason::kOperandIsNotAFunction);
  }
}

void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotABoundFunction);

    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    CompareObjectType(object, temp, temp, JS_BOUND_FUNCTION_TYPE);
    Check(eq, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  AssertNotSmi(object, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  LoadTaggedPointerField(temp, FieldMemOperand(object, HeapObject::kMapOffset));

  Label do_check;
  // Load instance type and check if JSGeneratorObject
  CompareInstanceType(temp, temp, JS_GENERATOR_OBJECT_TYPE);
  B(eq, &do_check);

  // Check if JSAsyncFunctionObject
  Cmp(temp, JS_ASYNC_FUNCTION_OBJECT_TYPE);
  B(eq, &do_check);

  // Check if JSAsyncGeneratorObject
  Cmp(temp, JS_ASYNC_GENERATOR_OBJECT_TYPE);

  bind(&do_check);
  // Restore generator object to register and perform assertion
  Check(eq, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();
    Label done_checking;
    AssertNotSmi(object);
    JumpIfRoot(object, RootIndex::kUndefinedValue, &done_checking);
    LoadTaggedPointerField(scratch,
                           FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(scratch, scratch, ALLOCATION_SITE_TYPE);
    Assert(eq, AbortReason::kExpectedUndefinedOrCell);
    Bind(&done_checking);
  }
}

void TurboAssembler::AssertPositiveOrZero(Register value) {
  if (emit_debug_code()) {
    Label done;
    int sign_bit = value.Is64Bits() ? kXSignBit : kWSignBit;
    Tbz(value, sign_bit, &done);
    Abort(AbortReason::kUnexpectedNegativeValue);
    Bind(&done);
  }
}

void TurboAssembler::CallRuntimeWithCEntry(Runtime::FunctionId fid,
                                           Register centry) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Mov(x0, f->nargs);
  Mov(x1, ExternalReference::Create(f));
  DCHECK(!AreAliased(centry, x0, x1));
  CallCodeObject(centry);
}

void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All arguments must be on the stack before this function is called.
  // x0 holds the return value after the call.

  // Check that the number of arguments matches what the function expects.
  // If f->nargs is -1, the function can accept a variable number of arguments.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // Place the necessary arguments.
  Mov(x0, num_arguments);
  Mov(x1, ExternalReference::Create(f));

  Handle<Code> code =
      CodeFactory::CEntry(isolate(), f->result_size, save_doubles);
  Call(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             bool builtin_exit_frame) {
  Mov(x1, builtin);
  Handle<Code> code = CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs,
                                          kArgvOnStack, builtin_exit_frame);
  Jump(code, RelocInfo::CODE_TARGET);
}

void MacroAssembler::JumpToInstructionStream(Address entry) {
  Ldr(kOffHeapTrampolineRegister, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Br(kOffHeapTrampolineRegister);
}

void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
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
  return FLAG_sim_stack_alignment;
#endif  // V8_HOST_ARCH_ARM64
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args) {
  CallCFunction(function, num_of_reg_args, 0);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_of_reg_args,
                                   int num_of_double_args) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, function);
  CallCFunction(temp, num_of_reg_args, num_of_double_args);
}

static const int kRegisterPassedArguments = 8;

void TurboAssembler::CallCFunction(Register function, int num_of_reg_args,
                                   int num_of_double_args) {
  DCHECK_LE(num_of_reg_args + num_of_double_args, kMaxCParameters);
  DCHECK(has_frame());

  // If we're passing doubles, we're limited to the following prototypes
  // (defined by ExternalReference::Type):
  //  BUILTIN_COMPARE_CALL:  int f(double, double)
  //  BUILTIN_FP_FP_CALL:    double f(double, double)
  //  BUILTIN_FP_CALL:       double f(double)
  //  BUILTIN_FP_INT_CALL:   double f(double, int)
  if (num_of_double_args > 0) {
    DCHECK_LE(num_of_reg_args, 1);
    DCHECK_LE(num_of_double_args + num_of_reg_args, 2);
  }

  // Save the frame pointer and PC so that the stack layout remains iterable,
  // even without an ExitFrame which normally exists between JS and C frames.
  if (isolate() != nullptr) {
    Register scratch1 = x4;
    Register scratch2 = x5;
    Push(scratch1, scratch2);

    Label get_pc;
    Bind(&get_pc);
    Adr(scratch2, &get_pc);

    Mov(scratch1, ExternalReference::fast_c_call_caller_pc_address(isolate()));
    Str(scratch2, MemOperand(scratch1));
    Mov(scratch1, ExternalReference::fast_c_call_caller_fp_address(isolate()));
    Str(fp, MemOperand(scratch1));

    Pop(scratch2, scratch1);
  }

  // Call directly. The function called cannot cause a GC, or allow preemption,
  // so the return address in the link register stays correct.
  Call(function);

  if (isolate() != nullptr) {
    // We don't unset the PC; the FP is the source of truth.
    Register scratch = x4;
    Push(scratch, xzr);
    Mov(scratch, ExternalReference::fast_c_call_caller_fp_address(isolate()));
    Str(xzr, MemOperand(scratch));
    Pop(xzr, scratch);
  }

  if (num_of_reg_args > kRegisterPassedArguments) {
    // Drop the register passed arguments.
    int claim_slots = RoundUp(num_of_reg_args - kRegisterPassedArguments, 2);
    Drop(claim_slots);
  }
}

void TurboAssembler::LoadFromConstantsTable(Register destination,
                                            int constant_index) {
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

namespace {

// The calculated offset is either:
// * the 'target' input unmodified if this is a WASM call, or
// * the offset of the target from the current PC, in instructions, for any
//   other type of call.
static int64_t CalculateTargetOffset(Address target, RelocInfo::Mode rmode,
                                     byte* pc) {
  int64_t offset = static_cast<int64_t>(target);
  // The target of WebAssembly calls is still an index instead of an actual
  // address at this point, and needs to be encoded as-is.
  if (rmode != RelocInfo::WASM_CALL && rmode != RelocInfo::WASM_STUB_CALL) {
    offset -= reinterpret_cast<int64_t>(pc);
    DCHECK_EQ(offset % kInstrSize, 0);
    offset = offset / static_cast<int>(kInstrSize);
  }
  return offset;
}
}  // namespace

void TurboAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  JumpHelper(CalculateTargetOffset(target, rmode, pc_), rmode, cond);
}

void TurboAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));

  if (options().inline_offheap_trampolines) {
    int builtin_index = Builtins::kNoBuiltinId;
    if (isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
        Builtins::IsIsolateIndependent(builtin_index)) {
      // Inline the trampoline.
      RecordCommentForOffHeapTrampoline(builtin_index);
      CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
      UseScratchRegisterScope temps(this);
      Register scratch = temps.AcquireX();
      EmbeddedData d = EmbeddedData::FromBlob();
      Address entry = d.InstructionStartOfBuiltin(builtin_index);
      Ldr(scratch, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
      Jump(scratch, cond);
      return;
    }
  }

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

void TurboAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode) {
  DCHECK_IMPLIES(options().isolate_independent_code,
                 Builtins::IsIsolateIndependentBuiltin(*code));
  BlockPoolsScope scope(this);

  if (options().inline_offheap_trampolines) {
    int builtin_index = Builtins::kNoBuiltinId;
    if (isolate()->builtins()->IsBuiltinHandle(code, &builtin_index) &&
        Builtins::IsIsolateIndependent(builtin_index)) {
      // Inline the trampoline.
      CallBuiltin(builtin_index);
      return;
    }
  }

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
  STATIC_ASSERT(kSystemPointerSize == 8);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);

  // The builtin_index register contains the builtin index as a Smi.
  // Untagging is folded into the indexing operand below.
#if defined(V8_COMPRESS_POINTERS) || defined(V8_31BIT_SMIS_ON_64BIT_ARCH)
  STATIC_ASSERT(kSmiShiftSize == 0);
  Lsl(builtin_index, builtin_index, kSystemPointerSizeLog2 - kSmiShift);
#else
  STATIC_ASSERT(kSmiShiftSize == 31);
  Asr(builtin_index, builtin_index, kSmiShift - kSystemPointerSizeLog2);
#endif
  Add(builtin_index, builtin_index, IsolateData::builtin_entry_table_offset());
  Ldr(builtin_index, MemOperand(kRootRegister, builtin_index));
}

void TurboAssembler::CallBuiltinByIndex(Register builtin_index) {
  LoadEntryFromBuiltinIndex(builtin_index);
  Call(builtin_index);
}

void TurboAssembler::CallBuiltin(int builtin_index) {
  DCHECK(Builtins::IsBuiltinId(builtin_index));
  DCHECK(FLAG_embedded_builtins);
  RecordCommentForOffHeapTrampoline(builtin_index);
  CHECK_NE(builtin_index, Builtins::kNoBuiltinId);
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  EmbeddedData d = EmbeddedData::FromBlob();
  Address entry = d.InstructionStartOfBuiltin(builtin_index);
  Ldr(scratch, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  Call(scratch);
}

void TurboAssembler::LoadCodeObjectEntry(Register destination,
                                         Register code_object) {
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

    Ldrsw(scratch, FieldMemOperand(code_object, Code::kFlagsOffset));
    Tst(scratch, Operand(Code::IsOffHeapTrampoline::kMask));
    B(ne, &if_code_is_off_heap);

    // Not an off-heap trampoline object, the entry point is at
    // Code::raw_instruction_start().
    Add(destination, code_object, Code::kHeaderSize - kHeapObjectTag);
    B(&out);

    // An off-heap trampoline, the entry point is loaded from the builtin entry
    // table.
    bind(&if_code_is_off_heap);
    Ldrsw(scratch, FieldMemOperand(code_object, Code::kBuiltinIndexOffset));
    Lsl(destination, scratch, kSystemPointerSizeLog2);
    Add(destination, destination, kRootRegister);
    Ldr(destination,
        MemOperand(destination, IsolateData::builtin_entry_table_offset()));

    bind(&out);
  } else {
    Add(destination, code_object, Code::kHeaderSize - kHeapObjectTag);
  }
}

void TurboAssembler::CallCodeObject(Register code_object) {
  LoadCodeObjectEntry(code_object, code_object);
  Call(code_object);
}

void TurboAssembler::JumpCodeObject(Register code_object) {
  LoadCodeObjectEntry(code_object, code_object);
  Jump(code_object);
}

void TurboAssembler::StoreReturnAddressAndCall(Register target) {
  // This generates the final instruction sequence for calls to C functions
  // once an exit frame has been constructed.
  //
  // Note that this assumes the caller code (i.e. the Code object currently
  // being generated) is immovable or that the callee function cannot trigger
  // GC, since the callee function will return to it.

  UseScratchRegisterScope temps(this);
  Register scratch1 = temps.AcquireX();

  Label return_location;
  Adr(scratch1, &return_location);
  Poke(scratch1, 0);

  if (emit_debug_code()) {
    // Verify that the slot below fp[kSPOffset]-8 points to the return location.
    Register scratch2 = temps.AcquireX();
    Ldr(scratch2, MemOperand(fp, ExitFrameConstants::kSPOffset));
    Ldr(scratch2, MemOperand(scratch2, -static_cast<int64_t>(kXRegSize)));
    Cmp(scratch2, scratch1);
    Check(eq, AbortReason::kReturnAddressNotFoundInFrame);
  }

  Blr(target);
  Bind(&return_location);
}

void TurboAssembler::IndirectCall(Address target, RelocInfo::Mode rmode) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  Mov(temp, Immediate(target, rmode));
  Blr(temp);
}

bool TurboAssembler::IsNearCallOffset(int64_t offset) {
  return is_int26(offset);
}

void TurboAssembler::CallForDeoptimization(Address target, int deopt_id) {
  BlockPoolsScope scope(this);
#ifdef DEBUG
  Label start;
  bind(&start);
#endif
  int64_t offset = static_cast<int64_t>(target) -
                   static_cast<int64_t>(options().code_range_start);
  DCHECK_EQ(offset % kInstrSize, 0);
  offset = offset / static_cast<int>(kInstrSize);
  DCHECK(IsNearCallOffset(offset));
  near_call(static_cast<int>(offset), RelocInfo::RUNTIME_ENTRY);
  DCHECK_EQ(SizeOfCodeGeneratedSince(&start), Deoptimizer::kDeoptExitSize);
}

void TurboAssembler::PrepareForTailCall(const ParameterCount& callee_args_count,
                                        Register caller_args_count_reg,
                                        Register scratch0, Register scratch1) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
#endif

  // Calculate the end of destination area where we will put the arguments
  // after we drop current frame. We add kSystemPointerSize to count the
  // receiver argument which is not included into formal parameters count.
  Register dst_reg = scratch0;
  Add(dst_reg, fp, Operand(caller_args_count_reg, LSL, kSystemPointerSizeLog2));
  Add(dst_reg, dst_reg,
      StandardFrameConstants::kCallerSPOffset + kSystemPointerSize);
  // Round dst_reg up to a multiple of 16 bytes, so that we overwrite any
  // potential padding.
  Add(dst_reg, dst_reg, 15);
  Bic(dst_reg, dst_reg, 15);

  Register src_reg = caller_args_count_reg;
  // Calculate the end of source area. +kSystemPointerSize is for the receiver.
  if (callee_args_count.is_reg()) {
    Add(src_reg, sp,
        Operand(callee_args_count.reg(), LSL, kSystemPointerSizeLog2));
    Add(src_reg, src_reg, kSystemPointerSize);
  } else {
    Add(src_reg, sp, (callee_args_count.immediate() + 1) * kSystemPointerSize);
  }

  // Round src_reg up to a multiple of 16 bytes, so we include any potential
  // padding in the copy.
  Add(src_reg, src_reg, 15);
  Bic(src_reg, src_reg, 15);

  if (FLAG_debug_code) {
    Cmp(src_reg, dst_reg);
    Check(lo, AbortReason::kStackAccessBelowStackPointer);
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  Ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  Ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch1;
  Label loop, entry;
  B(&entry);
  bind(&loop);
  Ldr(tmp_reg, MemOperand(src_reg, -kSystemPointerSize, PreIndex));
  Str(tmp_reg, MemOperand(dst_reg, -kSystemPointerSize, PreIndex));
  bind(&entry);
  Cmp(sp, src_reg);
  B(ne, &loop);

  // Leave current frame.
  Mov(sp, dst_reg);
}

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual, Label* done,
                                    InvokeFlag flag,
                                    bool* definitely_mismatches) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  x0: actual arguments count.
  //  x1: function (passed through to callee).
  //  x2: expected arguments count.

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  DCHECK(actual.is_immediate() || actual.reg().is(x0));
  DCHECK(expected.is_immediate() || expected.reg().is(x2));

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    Mov(x0, actual.immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;

    } else {
      if (expected.immediate() ==
          SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        // Set up x2 for the argument adaptor.
        Mov(x2, expected.immediate());
      }
    }

  } else {  // expected is a register.
    Operand actual_op = actual.is_immediate() ? Operand(actual.immediate())
                                              : Operand(actual.reg());
    Mov(x0, actual_op);
    // If actual == expected perform a regular invocation.
    Cmp(expected.reg(), actual_op);
    B(eq, &regular_invoke);
  }

  // If the argument counts may mismatch, generate a call to the argument
  // adaptor.
  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      Call(adaptor);
      if (!*definitely_mismatches) {
        // If the arg counts don't match, no extra code is emitted by
        // MAsm::InvokeFunctionCode and we can just fall through.
        B(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
  }
  Bind(&regular_invoke);
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual) {
  Label skip_hook;

  Mov(x4, ExternalReference::debug_hook_on_function_call_address(isolate()));
  Ldrsb(x4, MemOperand(x4));
  Cbz(x4, &skip_hook);

  {
    // Load receiver to pass it later to DebugOnFunctionCall hook.
    Operand actual_op = actual.is_immediate() ? Operand(actual.immediate())
                                              : Operand(actual.reg());
    Mov(x4, actual_op);
    Ldr(x4, MemOperand(sp, x4, LSL, kSystemPointerSizeLog2));
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);

    Register expected_reg = padreg;
    Register actual_reg = padreg;
    if (expected.is_reg()) expected_reg = expected.reg();
    if (actual.is_reg()) actual_reg = actual.reg();
    if (!new_target.is_valid()) new_target = padreg;

    // Save values on stack.
    SmiTag(expected_reg);
    SmiTag(actual_reg);
    Push(expected_reg, actual_reg, new_target, fun);
    Push(fun, x4);
    CallRuntime(Runtime::kDebugOnFunctionCall);

    // Restore values from stack.
    Pop(fun, new_target, actual_reg, expected_reg);
    SmiUntag(actual_reg);
    SmiUntag(expected_reg);
  }
  Bind(&skip_hook);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function.is(x1));
  DCHECK_IMPLIES(new_target.is_valid(), new_target.is(x3));

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(x3, RootIndex::kUndefinedValue);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, flag, &definitely_mismatches);

  // If we are certain that actual != expected, then we know InvokePrologue will
  // have handled the call through the argument adaptor mechanism.
  // The called function expects the call kind in x5.
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Register code = kJavaScriptCallCodeStartRegister;
    LoadTaggedPointerField(code,
                           FieldMemOperand(function, JSFunction::kCodeOffset));
    if (flag == CALL_FUNCTION) {
      CallCodeObject(code);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      JumpCodeObject(code);
    }
  }

  // Continue here if InvokePrologue does handle the invocation due to
  // mismatched parameter counts.
  Bind(&done);
}

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK(function.is(x1));

  Register expected_reg = x2;

  LoadTaggedPointerField(cp,
                         FieldMemOperand(function, JSFunction::kContextOffset));
  // The number of arguments is stored as an int32_t, and -1 is a marker
  // (SharedFunctionInfo::kDontAdaptArgumentsSentinel), so we need sign
  // extension to correctly handle it.
  LoadTaggedPointerField(
      expected_reg,
      FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
  Ldrh(expected_reg,
       FieldMemOperand(expected_reg,
                       SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(expected_reg);
  InvokeFunctionCode(function, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in x1.
  // (See FullCodeGenerator::Generate().)
  DCHECK(function.Is(x1));

  // Set up the context.
  LoadTaggedPointerField(cp,
                         FieldMemOperand(function, JSFunction::kContextOffset));

  InvokeFunctionCode(function, no_reg, expected, actual, flag);
}

void TurboAssembler::TryConvertDoubleToInt64(Register result,
                                             DoubleRegister double_input,
                                             Label* done) {
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
                                       StubCallMode stub_mode) {
  Label done;

  // Try to convert the double to an int64. If successful, the bottom 32 bits
  // contain our truncated int32 result.
  TryConvertDoubleToInt64(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  Push(lr, double_input);

  // DoubleToI preserves any registers it needs to clobber.
  if (stub_mode == StubCallMode::kCallWasmRuntimeStub) {
    Call(wasm::WasmCode::kDoubleToI, RelocInfo::WASM_STUB_CALL);
  } else if (options().inline_offheap_trampolines) {
    CallBuiltin(Builtins::kDoubleToI);
  } else {
    Call(BUILTIN_CODE(isolate, DoubleToI), RelocInfo::CODE_TARGET);
  }
  Ldr(result, MemOperand(sp, 0));

  DCHECK_EQ(xzr.SizeInBytes(), double_input.SizeInBytes());
  Pop(xzr, lr);  // xzr to drop the double input on the stack.

  Bind(&done);
  // Keep our invariant that the upper 32 bits are zero.
  Uxtw(result.W(), result.W());
}

void TurboAssembler::Prologue() {
  Push(lr, fp, cp, x1);
  Add(fp, sp, StandardFrameConstants::kFixedFrameSizeFromFp);
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  UseScratchRegisterScope temps(this);

  if (type == StackFrame::INTERNAL) {
    Register type_reg = temps.AcquireX();
    Mov(type_reg, StackFrame::TypeToMarker(type));
    // type_reg pushed twice for alignment.
    Push(lr, fp, type_reg, type_reg);
    const int kFrameSize =
        TypedFrameConstants::kFixedFrameSizeFromFp + kSystemPointerSize;
    Add(fp, sp, kFrameSize);
    // sp[3] : lr
    // sp[2] : fp
    // sp[1] : type
    // sp[0] : for alignment
  } else if (type == StackFrame::WASM_COMPILED ||
             type == StackFrame::WASM_COMPILE_LAZY ||
             type == StackFrame::WASM_EXIT) {
    Register type_reg = temps.AcquireX();
    Mov(type_reg, StackFrame::TypeToMarker(type));
    Push(lr, fp);
    Mov(fp, sp);
    Push(type_reg, padreg);
    // sp[3] : lr
    // sp[2] : fp
    // sp[1] : type
    // sp[0] : for alignment
  } else {
    DCHECK_EQ(type, StackFrame::CONSTRUCT);
    Register type_reg = temps.AcquireX();
    Mov(type_reg, StackFrame::TypeToMarker(type));

    // Users of this frame type push a context pointer after the type field,
    // so do it here to keep the stack pointer aligned.
    Push(lr, fp, type_reg, cp);

    // The context pointer isn't part of the fixed frame, so add an extra slot
    // to account for it.
    Add(fp, sp,
        TypedFrameConstants::kFixedFrameSizeFromFp + kSystemPointerSize);
    // sp[3] : lr
    // sp[2] : fp
    // sp[1] : type
    // sp[0] : cp
  }
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer and return address.
  Mov(sp, fp);
  Pop(fp, lr);
}

void MacroAssembler::ExitFramePreserveFPRegs() {
  DCHECK_EQ(kCallerSavedV.Count() % 2, 0);
  PushCPURegList(kCallerSavedV);
}

void MacroAssembler::ExitFrameRestoreFPRegs() {
  // Read the registers from the stack without popping them. The stack pointer
  // will be reset as part of the unwinding process.
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
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the new stack frame.
  Push(lr, fp);
  Mov(fp, sp);
  Mov(scratch, StackFrame::TypeToMarker(frame_type));
  Push(scratch, xzr);
  //          fp[8]: CallerPC (lr)
  //    fp -> fp[0]: CallerFP (old fp)
  //          fp[-8]: STUB marker
  //    sp -> fp[-16]: Space reserved for SPOffset.
  STATIC_ASSERT((2 * kSystemPointerSize) ==
                ExitFrameConstants::kCallerSPOffset);
  STATIC_ASSERT((1 * kSystemPointerSize) ==
                ExitFrameConstants::kCallerPCOffset);
  STATIC_ASSERT((0 * kSystemPointerSize) ==
                ExitFrameConstants::kCallerFPOffset);
  STATIC_ASSERT((-2 * kSystemPointerSize) == ExitFrameConstants::kSPOffset);

  // Save the frame pointer and context pointer in the top frame.
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate()));
  Str(fp, MemOperand(scratch));
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Str(cp, MemOperand(scratch));

  STATIC_ASSERT((-2 * kSystemPointerSize) ==
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
  if (restore_doubles) {
    ExitFrameRestoreFPRegs();
  }

  // Restore the context pointer from the top frame.
  Mov(scratch,
      ExternalReference::Create(IsolateAddressId::kContextAddress, isolate()));
  Ldr(cp, MemOperand(scratch));

  if (emit_debug_code()) {
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
  Pop(fp, lr);
}

void MacroAssembler::LoadGlobalProxy(Register dst) {
  LoadNativeContextSlot(Context::GLOBAL_PROXY_INDEX, dst);
}

void MacroAssembler::LoadWeakValue(Register out, Register in,
                                   Label* target_if_cleared) {
  CompareAndBranch(in.W(), Operand(kClearedWeakHeapObjectLower32), eq,
                   target_if_cleared);

  and_(out, in, Operand(~kWeakHeapObjectMask));
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK_NE(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Mov(scratch2, ExternalReference::Create(counter));
    Ldr(scratch1.W(), MemOperand(scratch2));
    Add(scratch1.W(), scratch1.W(), value);
    Str(scratch1.W(), MemOperand(scratch2));
  }
}

void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  IncrementCounter(counter, -value, scratch1, scratch2);
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  Mov(x1, ExternalReference::debug_restart_fp_address(isolate()));
  Ldr(x1, MemOperand(x1));
  Tst(x1, x1);
  Jump(BUILTIN_CODE(isolate(), FrameDropperTrampoline), RelocInfo::CODE_TARGET,
       ne);
}

void MacroAssembler::JumpIfObjectType(Register object, Register map,
                                      Register type_reg, InstanceType type,
                                      Label* if_cond_pass, Condition cond) {
  CompareObjectType(object, map, type_reg, type);
  B(cond, if_cond_pass);
}

// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  LoadTaggedPointerField(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, type_reg, type);
}

// Sets condition flags based on comparison, and returns type in type_reg.
void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  Ldrh(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Cmp(type_reg, type);
}

void MacroAssembler::LoadElementsKindFromMap(Register result, Register map) {
  // Load the map's "bit field 2".
  Ldrb(result, FieldMemOperand(map, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  DecodeField<Map::ElementsKindBits>(result);
}

void MacroAssembler::CompareRoot(const Register& obj, RootIndex index) {
  UseScratchRegisterScope temps(this);
  Register temp = temps.AcquireX();
  DCHECK(!AreAliased(obj, temp));
  LoadRoot(temp, index);
  Cmp(obj, temp);
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
#ifdef V8_COMPRESS_POINTERS
  DecompressTaggedPointer(destination, field_operand);
#else
  Ldr(destination, field_operand);
#endif
}

void TurboAssembler::LoadAnyTaggedField(const Register& destination,
                                        const MemOperand& field_operand) {
#ifdef V8_COMPRESS_POINTERS
  DecompressAnyTagged(destination, field_operand);
#else
  Ldr(destination, field_operand);
#endif
}

void TurboAssembler::SmiUntagField(Register dst, const MemOperand& src) {
  SmiUntag(dst, src);
}

void TurboAssembler::StoreTaggedField(const Register& value,
                                      const MemOperand& dst_field_operand) {
#ifdef V8_COMPRESS_POINTERS
  RecordComment("[ StoreTagged");
  Str(value.W(), dst_field_operand);
  RecordComment("]");
#else
  Str(value, dst_field_operand);
#endif
}

void TurboAssembler::DecompressTaggedSigned(const Register& destination,
                                            const MemOperand& field_operand) {
  RecordComment("[ DecompressTaggedSigned");
  Ldrsw(destination, field_operand);
  RecordComment("]");
}

void TurboAssembler::DecompressTaggedSigned(const Register& destination,
                                            const Register& source) {
  RecordComment("[ DecompressTaggedSigned");
  Sxtw(destination, source);
  RecordComment("]");
}

void TurboAssembler::DecompressTaggedPointer(const Register& destination,
                                             const MemOperand& field_operand) {
  RecordComment("[ DecompressTaggedPointer");
  Ldrsw(destination, field_operand);
  Add(destination, kRootRegister, destination);
  RecordComment("]");
}

void TurboAssembler::DecompressTaggedPointer(const Register& destination,
                                             const Register& source) {
  RecordComment("[ DecompressTaggedPointer");
  Add(destination, kRootRegister, Operand(source, SXTW));
  RecordComment("]");
}

void TurboAssembler::DecompressAnyTagged(const Register& destination,
                                         const MemOperand& field_operand) {
  RecordComment("[ DecompressAnyTagged");
  Ldrsw(destination, field_operand);
  if (kUseBranchlessPtrDecompressionInGeneratedCode) {
    UseScratchRegisterScope temps(this);
    // Branchlessly compute |masked_root|:
    // masked_root = HAS_SMI_TAG(destination) ? 0 : kRootRegister;
    STATIC_ASSERT((kSmiTagSize == 1) && (kSmiTag == 0));
    Register masked_root = temps.AcquireX();
    // Sign extend tag bit to entire register.
    Sbfx(masked_root, destination, 0, kSmiTagSize);
    And(masked_root, masked_root, kRootRegister);
    // Now this add operation will either leave the value unchanged if it is a
    // smi or add the isolate root if it is a heap object.
    Add(destination, masked_root, destination);
  } else {
    Label done;
    JumpIfSmi(destination, &done);
    Add(destination, kRootRegister, destination);
    bind(&done);
  }
  RecordComment("]");
}

void TurboAssembler::DecompressAnyTagged(const Register& destination,
                                         const Register& source) {
  RecordComment("[ DecompressAnyTagged");
  if (kUseBranchlessPtrDecompressionInGeneratedCode) {
    UseScratchRegisterScope temps(this);
    // Branchlessly compute |masked_root|:
    // masked_root = HAS_SMI_TAG(destination) ? 0 : kRootRegister;
    STATIC_ASSERT((kSmiTagSize == 1) && (kSmiTag == 0));
    Register masked_root = temps.AcquireX();
    // Sign extend tag bit to entire register.
    Sbfx(masked_root, source, 0, kSmiTagSize);
    And(masked_root, masked_root, kRootRegister);
    // Now this add operation will either leave the value unchanged if it is a
    // smi or add the isolate root if it is a heap object.
    Add(destination, masked_root, Operand(source, SXTW));
  } else {
    Label done;
    Sxtw(destination, source);
    JumpIfSmi(destination, &done);
    Add(destination, kRootRegister, destination);
    bind(&done);
  }
  RecordComment("]");
}

int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // Make sure the safepoint registers list is what we expect.
  DCHECK_EQ(CPURegList::GetSafepointSavedRegisters().list(), 0x6FFCFFFF);

  // Safepoint registers are stored contiguously on the stack, but not all the
  // registers are saved. The following registers are excluded:
  //  - x16 and x17 (ip0 and ip1) because they shouldn't be preserved outside of
  //    the macro assembler.
  //  - x31 (sp) because the system stack pointer doesn't need to be included
  //    in safepoint registers.
  //
  // This function implements the mapping of register code to index into the
  // safepoint register slots.
  if ((reg_code >= 0) && (reg_code <= 15)) {
    return reg_code;
  } else if ((reg_code >= 18) && (reg_code <= 30)) {
    // Skip ip0 and ip1.
    return reg_code - 2;
  } else {
    // This register has no safepoint register slot.
    UNREACHABLE();
  }
}

void TurboAssembler::CheckPageFlag(const Register& object, int mask,
                                   Condition cc, Label* condition_met) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.AcquireX();
  And(scratch, object, ~kPageAlignmentMask);
  Ldr(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
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
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip the barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so offset must be a multiple of kTaggedSize.
  DCHECK(IsAligned(offset, kTaggedSize));

  if (emit_debug_code()) {
    Label ok;
    UseScratchRegisterScope temps(this);
    Register scratch = temps.AcquireX();
    Add(scratch, object, offset - kHeapObjectTag);
    Tst(scratch, kTaggedSize - 1);
    B(eq, &ok);
    Abort(AbortReason::kUnalignedCellInWriteBarrier);
    Bind(&ok);
  }

  RecordWrite(object, Operand(offset - kHeapObjectTag), value, lr_status,
              save_fp, remembered_set_action, OMIT_SMI_CHECK);

  Bind(&done);
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  CPURegList regs(lr);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs.Combine(Register::XRegFromCode(i));
    }
  }

  PushCPURegList(regs);
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  CPURegList regs(lr);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      regs.Combine(Register::XRegFromCode(i));
    }
  }

  PopCPURegList(regs);
}

void TurboAssembler::CallEphemeronKeyBarrier(Register object, Operand offset,
                                             SaveFPRegsMode fp_mode) {
  EphemeronKeyBarrierDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(
      descriptor.GetRegisterParameter(EphemeronKeyBarrierDescriptor::kObject));
  Register slot_parameter(descriptor.GetRegisterParameter(
      EphemeronKeyBarrierDescriptor::kSlotAddress));
  Register fp_mode_parameter(
      descriptor.GetRegisterParameter(EphemeronKeyBarrierDescriptor::kFPMode));

  MoveObjectAndSlot(object_parameter, slot_parameter, object, offset);

  Mov(fp_mode_parameter, Smi::FromEnum(fp_mode));
  Call(isolate()->builtins()->builtin_handle(Builtins::kEphemeronKeyBarrier),
       RelocInfo::CODE_TARGET);
  RestoreRegisters(registers);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Operand offset, RememberedSetAction remembered_set_action,
    SaveFPRegsMode fp_mode) {
  CallRecordWriteStub(
      object, offset, remembered_set_action, fp_mode,
      isolate()->builtins()->builtin_handle(Builtins::kRecordWrite),
      kNullAddress);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Operand offset, RememberedSetAction remembered_set_action,
    SaveFPRegsMode fp_mode, Address wasm_target) {
  CallRecordWriteStub(object, offset, remembered_set_action, fp_mode,
                      Handle<Code>::null(), wasm_target);
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Operand offset, RememberedSetAction remembered_set_action,
    SaveFPRegsMode fp_mode, Handle<Code> code_target, Address wasm_target) {
  DCHECK_NE(code_target.is_null(), wasm_target == kNullAddress);
  // TODO(albertnetymk): For now we ignore remembered_set_action and fp_mode,
  // i.e. always emit remember set and save FP registers in RecordWriteStub. If
  // large performance regression is observed, we should use these values to
  // avoid unnecessary work.

  RecordWriteDescriptor descriptor;
  RegList registers = descriptor.allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kObject));
  Register slot_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register remembered_set_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(
      descriptor.GetRegisterParameter(RecordWriteDescriptor::kFPMode));

  MoveObjectAndSlot(object_parameter, slot_parameter, object, offset);

  Mov(remembered_set_parameter, Smi::FromEnum(remembered_set_action));
  Mov(fp_mode_parameter, Smi::FromEnum(fp_mode));
  if (code_target.is_null()) {
    Call(wasm_target, RelocInfo::WASM_STUB_CALL);
  } else {
    Call(code_target, RelocInfo::CODE_TARGET);
  }

  RestoreRegisters(registers);
}

void TurboAssembler::MoveObjectAndSlot(Register dst_object, Register dst_slot,
                                       Register object, Operand offset) {
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
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASM_LOCATION_IN_ASSEMBLER("MacroAssembler::RecordWrite");
  DCHECK(!AreAliased(object, value));

  if (emit_debug_code()) {
    UseScratchRegisterScope temps(this);
    Register temp = temps.AcquireX();

    Add(temp, object, offset);
    LoadTaggedPointerField(temp, MemOperand(temp));
    Cmp(temp, value);
    Check(eq, AbortReason::kWrongAddressOrValuePassedToRecordWrite);
  }

  if ((remembered_set_action == OMIT_REMEMBERED_SET &&
       !FLAG_incremental_marking) ||
      FLAG_disable_write_barriers) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    DCHECK_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }
  CheckPageFlag(value, MemoryChunk::kPointersToHereAreInterestingMask, ne,
                &done);

  CheckPageFlag(object, MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    Push(padreg, lr);
  }
  CallRecordWriteStub(object, offset, remembered_set_action, fp_mode);
  if (lr_status == kLRHasNotBeenSaved) {
    Pop(lr, padreg);
  }

  Bind(&done);
}

void TurboAssembler::Assert(Condition cond, AbortReason reason) {
  if (emit_debug_code()) {
    Check(cond, reason);
  }
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void MacroAssembler::AssertRegisterIsRoot(Register reg, RootIndex index,
                                          AbortReason reason) {
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, reason);
  }
}

void TurboAssembler::Check(Condition cond, AbortReason reason) {
  Label ok;
  B(cond, &ok);
  Abort(reason);
  // Will not return here.
  Bind(&ok);
}

void TurboAssembler::Abort(AbortReason reason) {
#ifdef DEBUG
  RecordComment("Abort message: ");
  RecordComment(GetAbortReason(reason));
#endif

  // Avoid emitting call to builtin if requested.
  if (trap_on_abort()) {
    Brk(0);
    return;
  }

  // We need some scratch registers for the MacroAssembler, so make sure we have
  // some. This is safe here because Abort never returns.
  RegList old_tmp_list = TmpList()->list();
  TmpList()->Combine(MacroAssembler::DefaultTmpList());

  if (should_abort_hard()) {
    // We don't care if we constructed a frame. Just pretend we did.
    FrameScope assume_frame(this, StackFrame::NONE);
    Mov(w0, static_cast<int>(reason));
    Call(ExternalReference::abort_with_reason());
    return;
  }

  // Avoid infinite recursion; Push contains some assertions that use Abort.
  HardAbortScope hard_aborts(this);

  Mov(x1, Smi::FromInt(static_cast<int>(reason)));

  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }

  TmpList()->set_list(old_tmp_list);
}

void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  LoadTaggedPointerField(dst, NativeContextMemOperand());
  LoadTaggedPointerField(dst, ContextMemOperand(dst, index));
}

// This is the main Printf implementation. All other Printf variants call
// PrintfNoPreserve after setting up one or more PreserveRegisterScopes.
void MacroAssembler::PrintfNoPreserve(const char* format,
                                      const CPURegister& arg0,
                                      const CPURegister& arg1,
                                      const CPURegister& arg2,
                                      const CPURegister& arg3) {
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

  // Override the MacroAssembler's scratch register list. The lists will be
  // reset automatically at the end of the UseScratchRegisterScope.
  UseScratchRegisterScope temps(this);
  TmpList()->set_list(tmp_list.list());
  FPTmpList()->set_list(fp_tmp_list.list());

  // Copies of the printf vararg registers that we can pop from.
  CPURegList pcs_varargs = kPCSVarargs;
  CPURegList pcs_varargs_fp = kPCSVarargsFP;

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
      pcs[i] = pcs_varargs_fp.PopLowestIndex().D();
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
// A call to printf needs special handling for the simulator, since the system
// printf function will use a different instruction set and the procedure-call
// standard will not be compatible.
#ifdef USE_SIMULATOR
  {
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
  }
#else
  Call(ExternalReference::printf_function());
#endif
}

void MacroAssembler::Printf(const char* format, CPURegister arg0,
                            CPURegister arg1, CPURegister arg2,
                            CPURegister arg3) {
  // Printf is expected to preserve all registers, so make sure that none are
  // available as scratch registers until we've preserved them.
  RegList old_tmp_list = TmpList()->list();
  RegList old_fp_tmp_list = FPTmpList()->list();
  TmpList()->set_list(0);
  FPTmpList()->set_list(0);

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
  TmpList()->set_list(tmp_list.list());
  FPTmpList()->set_list(fp_tmp_list.list());

  {
    UseScratchRegisterScope temps(this);
    // If any of the arguments are the current stack pointer, allocate a new
    // register for them, and adjust the value to compensate for pushing the
    // caller-saved registers.
    bool arg0_sp = sp.Aliases(arg0);
    bool arg1_sp = sp.Aliases(arg1);
    bool arg2_sp = sp.Aliases(arg2);
    bool arg3_sp = sp.Aliases(arg3);
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

  TmpList()->set_list(old_tmp_list);
  FPTmpList()->set_list(old_fp_tmp_list);
}

UseScratchRegisterScope::~UseScratchRegisterScope() {
  available_->set_list(old_available_);
  availablefp_->set_list(old_availablefp_);
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

MemOperand ContextMemOperand(Register context, int index) {
  return MemOperand(context, Context::SlotOffset(index));
}

MemOperand NativeContextMemOperand() {
  return ContextMemOperand(cp, Context::NATIVE_CONTEXT_INDEX);
}

void TurboAssembler::ComputeCodeStartAddress(const Register& rd) {
  // We can use adr to load a pc relative location.
  adr(rd, -pc_offset());
}

void TurboAssembler::ResetSpeculationPoisonRegister() {
  Mov(kSpeculationPoisonRegister, -1);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
