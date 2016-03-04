// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

#include "src/mips/assembler-mips.h"

#if V8_TARGET_ARCH_MIPS

#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/mips/assembler-mips-inl.h"

namespace v8 {
namespace internal {

// Get the CPU features enabled by the build. For cross compilation the
// preprocessor symbols CAN_USE_FPU_INSTRUCTIONS
// can be defined to enable FPU instructions when building the
// snapshot.
static unsigned CpuFeaturesImpliedByCompiler() {
  unsigned answer = 0;
#ifdef CAN_USE_FPU_INSTRUCTIONS
  answer |= 1u << FPU;
#endif  // def CAN_USE_FPU_INSTRUCTIONS

  // If the compiler is allowed to use FPU then we can use FPU too in our code
  // generation even when generating snapshots.  This won't work for cross
  // compilation.
#if defined(__mips__) && defined(__mips_hard_float) && __mips_hard_float != 0
  answer |= 1u << FPU;
#endif

  return answer;
}


void CpuFeatures::ProbeImpl(bool cross_compile) {
  supported_ |= CpuFeaturesImpliedByCompiler();

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

  // If the compiler is allowed to use fpu then we can use fpu too in our
  // code generation.
#ifndef __mips__
  // For the simulator build, use FPU.
  supported_ |= 1u << FPU;
#if defined(_MIPS_ARCH_MIPS32R6)
  // FP64 mode is implied on r6.
  supported_ |= 1u << FP64FPU;
#endif
#if defined(FPU_MODE_FP64)
  supported_ |= 1u << FP64FPU;
#endif
#else
  // Probe for additional features at runtime.
  base::CPU cpu;
  if (cpu.has_fpu()) supported_ |= 1u << FPU;
#if defined(FPU_MODE_FPXX)
  if (cpu.is_fp64_mode()) supported_ |= 1u << FP64FPU;
#elif defined(FPU_MODE_FP64)
  supported_ |= 1u << FP64FPU;
#endif
#if defined(_MIPS_ARCH_MIPS32RX)
  if (cpu.architecture() == 6) {
    supported_ |= 1u << MIPSr6;
  } else if (cpu.architecture() == 2) {
    supported_ |= 1u << MIPSr1;
    supported_ |= 1u << MIPSr2;
  } else {
    supported_ |= 1u << MIPSr1;
  }
#endif
#endif
}


void CpuFeatures::PrintTarget() { }
void CpuFeatures::PrintFeatures() { }


int ToNumber(Register reg) {
  DCHECK(reg.is_valid());
  const int kNumbers[] = {
    0,    // zero_reg
    1,    // at
    2,    // v0
    3,    // v1
    4,    // a0
    5,    // a1
    6,    // a2
    7,    // a3
    8,    // t0
    9,    // t1
    10,   // t2
    11,   // t3
    12,   // t4
    13,   // t5
    14,   // t6
    15,   // t7
    16,   // s0
    17,   // s1
    18,   // s2
    19,   // s3
    20,   // s4
    21,   // s5
    22,   // s6
    23,   // s7
    24,   // t8
    25,   // t9
    26,   // k0
    27,   // k1
    28,   // gp
    29,   // sp
    30,   // fp
    31,   // ra
  };
  return kNumbers[reg.code()];
}


Register ToRegister(int num) {
  DCHECK(num >= 0 && num < kNumRegisters);
  const Register kRegisters[] = {
    zero_reg,
    at,
    v0, v1,
    a0, a1, a2, a3,
    t0, t1, t2, t3, t4, t5, t6, t7,
    s0, s1, s2, s3, s4, s5, s6, s7,
    t8, t9,
    k0, k1,
    gp,
    sp,
    fp,
    ra
  };
  return kRegisters[num];
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo.

const int RelocInfo::kApplyMask = RelocInfo::kCodeTargetMask |
                                  1 << RelocInfo::INTERNAL_REFERENCE |
                                  1 << RelocInfo::INTERNAL_REFERENCE_ENCODED;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on MIPS means that it is a lui/ori instruction, and that is
  // always the case inside code objects.
  return true;
}


bool RelocInfo::IsInConstantPool() {
  return false;
}


// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand.
// See assembler-mips-inl.h for inlined constructors.

Operand::Operand(Handle<Object> handle) {
  AllowDeferredHandleDereference using_raw_address;
  rm_ = no_reg;
  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  if (obj->IsHeapObject()) {
    DCHECK(!HeapObject::cast(obj)->GetHeap()->InNewSpace(obj));
    imm32_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // No relocation needed.
    imm32_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE32;
  }
}


MemOperand::MemOperand(Register rm, int32_t offset) : Operand(rm) {
  offset_ = offset;
}


MemOperand::MemOperand(Register rm, int32_t unit, int32_t multiplier,
                       OffsetAddend offset_addend) : Operand(rm) {
  offset_ = unit * multiplier + offset_addend;
}


// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

static const int kNegOffset = 0x00008000;
// addiu(sp, sp, 4) aka Pop() operation or part of Pop(r)
// operations as post-increment of sp.
const Instr kPopInstruction = ADDIU | (Register::kCode_sp << kRsShift) |
                              (Register::kCode_sp << kRtShift) |
                              (kPointerSize & kImm16Mask);  // NOLINT
// addiu(sp, sp, -4) part of Push(r) operation as pre-decrement of sp.
const Instr kPushInstruction = ADDIU | (Register::kCode_sp << kRsShift) |
                               (Register::kCode_sp << kRtShift) |
                               (-kPointerSize & kImm16Mask);  // NOLINT
// sw(r, MemOperand(sp, 0))
const Instr kPushRegPattern =
    SW | (Register::kCode_sp << kRsShift) | (0 & kImm16Mask);  // NOLINT
//  lw(r, MemOperand(sp, 0))
const Instr kPopRegPattern =
    LW | (Register::kCode_sp << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpOffsetPattern =
    LW | (Register::kCode_fp << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kSwRegFpOffsetPattern =
    SW | (Register::kCode_fp << kRsShift) | (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpNegOffsetPattern = LW | (Register::kCode_fp << kRsShift) |
                                       (kNegOffset & kImm16Mask);  // NOLINT

const Instr kSwRegFpNegOffsetPattern = SW | (Register::kCode_fp << kRsShift) |
                                       (kNegOffset & kImm16Mask);  // NOLINT
// A mask for the Rt register for push, pop, lw, sw instructions.
const Instr kRtMask = kRtFieldMask;
const Instr kLwSwInstrTypeMask = 0xffe00000;
const Instr kLwSwInstrArgumentMask  = ~kLwSwInstrTypeMask;
const Instr kLwSwOffsetMask = kImm16Mask;


Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size),
      recorded_ast_id_(TypeFeedbackId::None()),
      positions_recorder_(this) {
  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);

  last_trampoline_pool_end_ = 0;
  no_trampoline_pool_before_ = 0;
  trampoline_pool_blocked_nesting_ = 0;
  // We leave space (16 * kTrampolineSlotsSize)
  // for BlockTrampolinePoolScope buffer.
  next_buffer_check_ = FLAG_force_long_branches
      ? kMaxInt : kMaxBranchOffset - kTrampolineSlotsSize * 16;
  internal_trampoline_exception_ = false;
  last_bound_pos_ = 0;

  trampoline_emitted_ = FLAG_force_long_branches;
  unbound_labels_count_ = 0;
  block_buffer_growth_ = false;

  ClearRecordedAstId();
}


void Assembler::GetCode(CodeDesc* desc) {
  if (IsPrevInstrCompactBranch()) {
    nop();
    ClearCompactBranchState();
  }
  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.
  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc->origin = this;
  desc->constant_pool_size = 0;
}


void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo32(m));
  if (IsPrevInstrCompactBranch()) {
    nop();
    ClearCompactBranchState();
  }
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::CodeTargetAlign() {
  // No advantage to aligning branch/call targets to more than
  // single instruction, that I am aware of.
  Align(4);
}


Register Assembler::GetRtReg(Instr instr) {
  Register rt;
  rt.reg_code = (instr & kRtFieldMask) >> kRtShift;
  return rt;
}


Register Assembler::GetRsReg(Instr instr) {
  Register rs;
  rs.reg_code = (instr & kRsFieldMask) >> kRsShift;
  return rs;
}


Register Assembler::GetRdReg(Instr instr) {
  Register rd;
  rd.reg_code = (instr & kRdFieldMask) >> kRdShift;
  return rd;
}


uint32_t Assembler::GetRt(Instr instr) {
  return (instr & kRtFieldMask) >> kRtShift;
}


uint32_t Assembler::GetRtField(Instr instr) {
  return instr & kRtFieldMask;
}


uint32_t Assembler::GetRs(Instr instr) {
  return (instr & kRsFieldMask) >> kRsShift;
}


uint32_t Assembler::GetRsField(Instr instr) {
  return instr & kRsFieldMask;
}


uint32_t Assembler::GetRd(Instr instr) {
  return  (instr & kRdFieldMask) >> kRdShift;
}


uint32_t Assembler::GetRdField(Instr instr) {
  return  instr & kRdFieldMask;
}


uint32_t Assembler::GetSa(Instr instr) {
  return (instr & kSaFieldMask) >> kSaShift;
}


uint32_t Assembler::GetSaField(Instr instr) {
  return instr & kSaFieldMask;
}


uint32_t Assembler::GetOpcodeField(Instr instr) {
  return instr & kOpcodeMask;
}


uint32_t Assembler::GetFunction(Instr instr) {
  return (instr & kFunctionFieldMask) >> kFunctionShift;
}


uint32_t Assembler::GetFunctionField(Instr instr) {
  return instr & kFunctionFieldMask;
}


uint32_t Assembler::GetImmediate16(Instr instr) {
  return instr & kImm16Mask;
}


uint32_t Assembler::GetLabelConst(Instr instr) {
  return instr & ~kImm16Mask;
}


bool Assembler::IsPop(Instr instr) {
  return (instr & ~kRtMask) == kPopRegPattern;
}


bool Assembler::IsPush(Instr instr) {
  return (instr & ~kRtMask) == kPushRegPattern;
}


bool Assembler::IsSwRegFpOffset(Instr instr) {
  return ((instr & kLwSwInstrTypeMask) == kSwRegFpOffsetPattern);
}


bool Assembler::IsLwRegFpOffset(Instr instr) {
  return ((instr & kLwSwInstrTypeMask) == kLwRegFpOffsetPattern);
}


bool Assembler::IsSwRegFpNegOffset(Instr instr) {
  return ((instr & (kLwSwInstrTypeMask | kNegOffset)) ==
          kSwRegFpNegOffsetPattern);
}


bool Assembler::IsLwRegFpNegOffset(Instr instr) {
  return ((instr & (kLwSwInstrTypeMask | kNegOffset)) ==
          kLwRegFpNegOffsetPattern);
}


// Labels refer to positions in the (to be) generated code.
// There are bound, linked, and unused labels.
//
// Bound labels refer to known positions in the already
// generated code. pos() is the position the label refers to.
//
// Linked labels refer to unknown positions in the code
// to be generated; pos() is the position of the last
// instruction using the label.

// The link chain is terminated by a value in the instruction of -1,
// which is an otherwise illegal value (branch -1 is inf loop).
// The instruction 16-bit offset field addresses 32-bit words, but in
// code is conv to an 18-bit value addressing bytes, hence the -4 value.

const int kEndOfChain = -4;
// Determines the end of the Jump chain (a subset of the label link chain).
const int kEndOfJumpChain = 0;


bool Assembler::IsBranch(Instr instr) {
  uint32_t opcode   = GetOpcodeField(instr);
  uint32_t rt_field = GetRtField(instr);
  uint32_t rs_field = GetRsField(instr);
  // Checks if the instruction is a branch.
  bool isBranch =
      opcode == BEQ || opcode == BNE || opcode == BLEZ || opcode == BGTZ ||
      opcode == BEQL || opcode == BNEL || opcode == BLEZL || opcode == BGTZL ||
      (opcode == REGIMM && (rt_field == BLTZ || rt_field == BGEZ ||
                            rt_field == BLTZAL || rt_field == BGEZAL)) ||
      (opcode == COP1 && rs_field == BC1) ||  // Coprocessor branch.
      (opcode == COP1 && rs_field == BC1EQZ) ||
      (opcode == COP1 && rs_field == BC1NEZ);
  if (!isBranch && IsMipsArchVariant(kMips32r6)) {
    // All the 3 variants of POP10 (BOVC, BEQC, BEQZALC) and
    // POP30 (BNVC, BNEC, BNEZALC) are branch ops.
    isBranch |= opcode == POP10 || opcode == POP30 || opcode == BC ||
                opcode == BALC ||
                (opcode == POP66 && rs_field != 0) ||  // BEQZC
                (opcode == POP76 && rs_field != 0);    // BNEZC
  }
  return isBranch;
}


bool Assembler::IsBc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a BC or BALC.
  return opcode == BC || opcode == BALC;
}


bool Assembler::IsBzc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is BEQZC or BNEZC.
  return (opcode == POP66 && GetRsField(instr) != 0) ||
         (opcode == POP76 && GetRsField(instr) != 0);
}


bool Assembler::IsEmittedConstant(Instr instr) {
  uint32_t label_constant = GetLabelConst(instr);
  return label_constant == 0;  // Emitted label const in reg-exp engine.
}


bool Assembler::IsBeq(Instr instr) {
  return GetOpcodeField(instr) == BEQ;
}


bool Assembler::IsBne(Instr instr) {
  return GetOpcodeField(instr) == BNE;
}


bool Assembler::IsBeqzc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  return opcode == POP66 && GetRsField(instr) != 0;
}


bool Assembler::IsBnezc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  return opcode == POP76 && GetRsField(instr) != 0;
}


bool Assembler::IsBeqc(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs = GetRsField(instr);
  uint32_t rt = GetRtField(instr);
  return opcode == POP10 && rs != 0 && rs < rt;  // && rt != 0
}


bool Assembler::IsBnec(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t rs = GetRsField(instr);
  uint32_t rt = GetRtField(instr);
  return opcode == POP30 && rs != 0 && rs < rt;  // && rt != 0
}


bool Assembler::IsJump(Instr instr) {
  uint32_t opcode   = GetOpcodeField(instr);
  uint32_t rt_field = GetRtField(instr);
  uint32_t rd_field = GetRdField(instr);
  uint32_t function_field = GetFunctionField(instr);
  // Checks if the instruction is a jump.
  return opcode == J || opcode == JAL ||
      (opcode == SPECIAL && rt_field == 0 &&
      ((function_field == JALR) || (rd_field == 0 && (function_field == JR))));
}


bool Assembler::IsJ(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a jump.
  return opcode == J;
}


bool Assembler::IsJal(Instr instr) {
  return GetOpcodeField(instr) == JAL;
}


bool Assembler::IsJr(Instr instr) {
  if (!IsMipsArchVariant(kMips32r6))  {
    return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JR;
  } else {
    return GetOpcodeField(instr) == SPECIAL &&
        GetRdField(instr) == 0  && GetFunctionField(instr) == JALR;
  }
}


bool Assembler::IsJalr(Instr instr) {
  return GetOpcodeField(instr) == SPECIAL &&
         GetRdField(instr) != 0  && GetFunctionField(instr) == JALR;
}


bool Assembler::IsLui(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a load upper immediate.
  return opcode == LUI;
}


bool Assembler::IsOri(Instr instr) {
  uint32_t opcode = GetOpcodeField(instr);
  // Checks if the instruction is a load upper immediate.
  return opcode == ORI;
}


bool Assembler::IsNop(Instr instr, unsigned int type) {
  // See Assembler::nop(type).
  DCHECK(type < 32);
  uint32_t opcode = GetOpcodeField(instr);
  uint32_t function = GetFunctionField(instr);
  uint32_t rt = GetRt(instr);
  uint32_t rd = GetRd(instr);
  uint32_t sa = GetSa(instr);

  // Traditional mips nop == sll(zero_reg, zero_reg, 0)
  // When marking non-zero type, use sll(zero_reg, at, type)
  // to avoid use of mips ssnop and ehb special encodings
  // of the sll instruction.

  Register nop_rt_reg = (type == 0) ? zero_reg : at;
  bool ret = (opcode == SPECIAL && function == SLL &&
              rd == static_cast<uint32_t>(ToNumber(zero_reg)) &&
              rt == static_cast<uint32_t>(ToNumber(nop_rt_reg)) &&
              sa == type);

  return ret;
}


int32_t Assembler::GetBranchOffset(Instr instr) {
  DCHECK(IsBranch(instr));
  return (static_cast<int16_t>(instr & kImm16Mask)) << 2;
}


bool Assembler::IsLw(Instr instr) {
  return (static_cast<uint32_t>(instr & kOpcodeMask) == LW);
}


int16_t Assembler::GetLwOffset(Instr instr) {
  DCHECK(IsLw(instr));
  return ((instr & kImm16Mask));
}


Instr Assembler::SetLwOffset(Instr instr, int16_t offset) {
  DCHECK(IsLw(instr));

  // We actually create a new lw instruction based on the original one.
  Instr temp_instr = LW | (instr & kRsFieldMask) | (instr & kRtFieldMask)
      | (offset & kImm16Mask);

  return temp_instr;
}


bool Assembler::IsSw(Instr instr) {
  return (static_cast<uint32_t>(instr & kOpcodeMask) == SW);
}


Instr Assembler::SetSwOffset(Instr instr, int16_t offset) {
  DCHECK(IsSw(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAddImmediate(Instr instr) {
  return ((instr & kOpcodeMask) == ADDIU);
}


Instr Assembler::SetAddImmediateOffset(Instr instr, int16_t offset) {
  DCHECK(IsAddImmediate(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAndImmediate(Instr instr) {
  return GetOpcodeField(instr) == ANDI;
}


static Assembler::OffsetSize OffsetSizeInBits(Instr instr) {
  if (IsMipsArchVariant(kMips32r6)) {
    if (Assembler::IsBc(instr)) {
      return Assembler::OffsetSize::kOffset26;
    } else if (Assembler::IsBzc(instr)) {
      return Assembler::OffsetSize::kOffset21;
    }
  }
  return Assembler::OffsetSize::kOffset16;
}


static inline int32_t AddBranchOffset(int pos, Instr instr) {
  int bits = OffsetSizeInBits(instr);
  const int32_t mask = (1 << bits) - 1;
  bits = 32 - bits;

  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmetic shifts for signed integers.
  int32_t imm = ((instr & mask) << bits) >> (bits - 2);

  if (imm == kEndOfChain) {
    // EndOfChain sentinel is returned directly, not relative to pc or pos.
    return kEndOfChain;
  } else {
    return pos + Assembler::kBranchPCOffset + imm;
  }
}


int Assembler::target_at(int pos, bool is_internal) {
  Instr instr = instr_at(pos);
  if (is_internal) {
    if (instr == 0) {
      return kEndOfChain;
    } else {
      int32_t instr_address = reinterpret_cast<int32_t>(buffer_ + pos);
      int delta = static_cast<int>(instr_address - instr);
      DCHECK(pos > delta);
      return pos - delta;
    }
  }
  if ((instr & ~kImm16Mask) == 0) {
    // Emitted label constant, not part of a branch.
    if (instr == 0) {
       return kEndOfChain;
     } else {
       int32_t imm18 =((instr & static_cast<int32_t>(kImm16Mask)) << 16) >> 14;
       return (imm18 + pos);
     }
  }
  // Check we have a branch or jump instruction.
  DCHECK(IsBranch(instr) || IsLui(instr));
  if (IsBranch(instr)) {
    return AddBranchOffset(pos, instr);
  } else {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    int32_t imm = (instr_lui & static_cast<int32_t>(kImm16Mask)) << kLuiShift;
    imm |= (instr_ori & static_cast<int32_t>(kImm16Mask));

    if (imm == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint32_t instr_address = reinterpret_cast<int32_t>(buffer_ + pos);
      int32_t delta = instr_address - imm;
      DCHECK(pos > delta);
      return pos - delta;
    }
  }
  return 0;
}


static inline Instr SetBranchOffset(int32_t pos, int32_t target_pos,
                                    Instr instr) {
  int32_t bits = OffsetSizeInBits(instr);
  int32_t imm = target_pos - (pos + Assembler::kBranchPCOffset);
  DCHECK((imm & 3) == 0);
  imm >>= 2;

  const int32_t mask = (1 << bits) - 1;
  instr &= ~mask;
  DCHECK(is_intn(imm, bits));

  return instr | (imm & mask);
}


void Assembler::target_at_put(int32_t pos, int32_t target_pos,
                              bool is_internal) {
  Instr instr = instr_at(pos);

  if (is_internal) {
    uint32_t imm = reinterpret_cast<uint32_t>(buffer_) + target_pos;
    instr_at_put(pos, imm);
    return;
  }
  if ((instr & ~kImm16Mask) == 0) {
    DCHECK(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    return;
  }

  DCHECK(IsBranch(instr) || IsLui(instr));
  if (IsBranch(instr)) {
    instr = SetBranchOffset(pos, target_pos, instr);
    instr_at_put(pos, instr);
  } else {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    uint32_t imm = reinterpret_cast<uint32_t>(buffer_) + target_pos;
    DCHECK((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;

    instr_at_put(pos + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm & kHiMask) >> kLuiShift));
    instr_at_put(pos + 1 * Assembler::kInstrSize,
                 instr_ori | (imm & kImm16Mask));
  }
}


void Assembler::print(Label* L) {
  if (L->is_unused()) {
    PrintF("unused label\n");
  } else if (L->is_bound()) {
    PrintF("bound label to %d\n", L->pos());
  } else if (L->is_linked()) {
    Label l = *L;
    PrintF("unbound label");
    while (l.is_linked()) {
      PrintF("@ %d ", l.pos());
      Instr instr = instr_at(l.pos());
      if ((instr & ~kImm16Mask) == 0) {
        PrintF("value\n");
      } else {
        PrintF("%d\n", instr);
      }
      next(&l, internal_reference_positions_.find(l.pos()) !=
                   internal_reference_positions_.end());
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}


void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // Must have valid binding position.
  int32_t trampoline_pos = kInvalidSlotPos;
  bool is_internal = false;
  if (L->is_linked() && !trampoline_emitted_) {
    unbound_labels_count_--;
    next_buffer_check_ += kTrampolineSlotsSize;
  }

  while (L->is_linked()) {
    int32_t fixup_pos = L->pos();
    int32_t dist = pos - fixup_pos;
    is_internal = internal_reference_positions_.find(fixup_pos) !=
                  internal_reference_positions_.end();
    next(L, is_internal);  // Call next before overwriting link with target at
                           // fixup_pos.
    Instr instr = instr_at(fixup_pos);
    if (is_internal) {
      target_at_put(fixup_pos, pos, is_internal);
    } else {
      if (IsBranch(instr)) {
        int branch_offset = BranchOffset(instr);
        if (dist > branch_offset) {
          if (trampoline_pos == kInvalidSlotPos) {
            trampoline_pos = get_trampoline_entry(fixup_pos);
            CHECK(trampoline_pos != kInvalidSlotPos);
          }
          CHECK((trampoline_pos - fixup_pos) <= branch_offset);
          target_at_put(fixup_pos, trampoline_pos, false);
          fixup_pos = trampoline_pos;
          dist = pos - fixup_pos;
        }
        target_at_put(fixup_pos, pos, false);
      } else {
        target_at_put(fixup_pos, pos, false);
      }
    }
  }
  L->bind_to(pos);

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_)
    last_bound_pos_ = pos;
}


void Assembler::bind(Label* L) {
  DCHECK(!L->is_bound());  // Label can only be bound once.
  bind_to(L, pc_offset());
}


void Assembler::next(Label* L, bool is_internal) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos(), is_internal);
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    DCHECK(link >= 0);
    L->link_to(link);
  }
}


bool Assembler::is_near(Label* L) {
  DCHECK(L->is_bound());
  return pc_offset() - L->pos() < kMaxBranchOffset - 4 * kInstrSize;
}


bool Assembler::is_near(Label* L, OffsetSize bits) {
  if (L == nullptr || !L->is_bound()) return true;
  return pc_offset() - L->pos() < (1 << (bits + 2 - 1)) - 1 - 5 * kInstrSize;
}


bool Assembler::is_near_branch(Label* L) {
  DCHECK(L->is_bound());
  return IsMipsArchVariant(kMips32r6) ? is_near_r6(L) : is_near_pre_r6(L);
}


int Assembler::BranchOffset(Instr instr) {
  // At pre-R6 and for other R6 branches the offset is 16 bits.
  int bits = OffsetSize::kOffset16;

  if (IsMipsArchVariant(kMips32r6)) {
    uint32_t opcode = GetOpcodeField(instr);
    switch (opcode) {
      // Checks BC or BALC.
      case BC:
      case BALC:
        bits = OffsetSize::kOffset26;
        break;

      // Checks BEQZC or BNEZC.
      case POP66:
      case POP76:
        if (GetRsField(instr) != 0) bits = OffsetSize::kOffset21;
        break;
      default:
        break;
    }
  }

  return (1 << (bits + 2 - 1)) - 1;
}


// We have to use a temporary register for things that can be relocated even
// if they can be encoded in the MIPS's 16 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Assembler::MustUseReg(RelocInfo::Mode rmode) {
  return !RelocInfo::IsNone(rmode);
}

void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 Register rd,
                                 uint16_t sa,
                                 SecondaryField func) {
  DCHECK(rd.is_valid() && rs.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 uint16_t msb,
                                 uint16_t lsb,
                                 SecondaryField func) {
  DCHECK(rs.is_valid() && rt.is_valid() && is_uint5(msb) && is_uint5(lsb));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (msb << kRdShift) | (lsb << kSaShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | fmt | (ft.code() << kFtShift) | (fs.code() << kFsShift)
      | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 FPURegister fr,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fr.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | (fr.code() << kFrShift) | (ft.code() << kFtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  DCHECK(fd.is_valid() && fs.is_valid() && rt.is_valid());
  Instr instr = opcode | fmt | (rt.code() << kRtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPUControlRegister fs,
                                 SecondaryField func) {
  DCHECK(fs.is_valid() && rt.is_valid());
  Instr instr =
      opcode | fmt | (rt.code() << kRtShift) | (fs.code() << kFsShift) | func;
  emit(instr);
}


// Instructions with immediate value.
// Registers are in the order of the instruction encoding, from left to right.
void Assembler::GenInstrImmediate(Opcode opcode, Register rs, Register rt,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && rt.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, SecondaryField SF,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | SF | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, FPURegister ft,
                                  int32_t j,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && ft.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (ft.code() << kFtShift)
      | (j & kImm16Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs, int32_t offset21,
                                  CompactBranchType is_compact_branch) {
  DCHECK(rs.is_valid() && (is_int21(offset21)));
  Instr instr = opcode | (rs.code() << kRsShift) | (offset21 & kImm21Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrImmediate(Opcode opcode, Register rs,
                                  uint32_t offset21) {
  DCHECK(rs.is_valid() && (is_uint21(offset21)));
  Instr instr = opcode | (rs.code() << kRsShift) | (offset21 & kImm21Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode, int32_t offset26,
                                  CompactBranchType is_compact_branch) {
  DCHECK(is_int26(offset26));
  Instr instr = opcode | (offset26 & kImm26Mask);
  emit(instr, is_compact_branch);
}


void Assembler::GenInstrJump(Opcode opcode,
                             uint32_t address) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(is_uint26(address));
  Instr instr = opcode | address;
  emit(instr);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


// Returns the next free trampoline entry.
int32_t Assembler::get_trampoline_entry(int32_t pos) {
  int32_t trampoline_entry = kInvalidSlotPos;

  if (!internal_trampoline_exception_) {
    if (trampoline_.start() > pos) {
     trampoline_entry = trampoline_.take_slot();
    }

    if (kInvalidSlotPos == trampoline_entry) {
      internal_trampoline_exception_ = true;
    }
  }
  return trampoline_entry;
}


uint32_t Assembler::jump_address(Label* L) {
  int32_t target_pos;

  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      return kEndOfJumpChain;
    }
  }

  uint32_t imm = reinterpret_cast<uint32_t>(buffer_) + target_pos;
  DCHECK((imm & 3) == 0);

  return imm;
}


int32_t Assembler::branch_offset_helper(Label* L, OffsetSize bits) {
  int32_t target_pos;
  int32_t pad = IsPrevInstrCompactBranch() ? kInstrSize : 0;

  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset() + pad);
    } else {
      L->link_to(pc_offset() + pad);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - (pc_offset() + kBranchPCOffset + pad);
  DCHECK(is_intn(offset, bits + 2));
  DCHECK((offset & 3) == 0);

  return offset;
}


void Assembler::label_at_put(Label* L, int at_offset) {
  int target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
    instr_at_put(at_offset, target_pos + (Code::kHeaderSize - kHeapObjectTag));
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link.
      int32_t imm18 = target_pos - at_offset;
      DCHECK((imm18 & 3) == 0);
      int32_t imm16 = imm18 >> 2;
      DCHECK(is_int16(imm16));
      instr_at_put(at_offset, (imm16 & kImm16Mask));
    } else {
      target_pos = kEndOfChain;
      instr_at_put(at_offset, 0);
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
    }
    L->link_to(at_offset);
  }
}


//------- Branch and jump instructions --------

void Assembler::b(int16_t offset) {
  beq(zero_reg, zero_reg, offset);
}


void Assembler::bal(int16_t offset) {
  positions_recorder()->WriteRecordedPositions();
  bgezal(zero_reg, offset);
}


void Assembler::bc(int32_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrImmediate(BC, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::balc(int32_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(BALC, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beq(Register rs, Register rt, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BEQ, rs, rt, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BGEZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgezc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZL, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgeuc(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZ, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgec(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZL, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezal(Register rs, int16_t offset) {
  DCHECK(!IsMipsArchVariant(kMips32r6) || rs.is(zero_reg));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(REGIMM, rs, BGEZAL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgtz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BGTZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bgtzc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZL, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::blez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BLEZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::blezc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZL, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltzc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!rt.is(zero_reg));
  GenInstrImmediate(BGTZL, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltuc(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZ, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltc(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!rs.is(zero_reg));
  DCHECK(!rt.is(zero_reg));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZL, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bltz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzal(Register rs, int16_t offset) {
  DCHECK(!IsMipsArchVariant(kMips32r6) || rs.is(zero_reg));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(REGIMM, rs, BLTZAL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bne(Register rs, Register rt, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BNE, rs, rt, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bovc(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!rs.is(zero_reg));
  if (rs.code() >= rt.code()) {
    GenInstrImmediate(ADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(ADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::bnvc(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!rs.is(zero_reg));
  if (rs.code() >= rt.code()) {
    GenInstrImmediate(DADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(DADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::blezalc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(BLEZ, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezalc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(BLEZ, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgezall(Register rs, int16_t offset) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  DCHECK(!(rs.is(zero_reg)));
  BlockTrampolinePoolScope block_trampoline_pool(this);
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(REGIMM, rs, BGEZALL, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzalc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(BGTZ, rt, rt, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bgtzalc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(BGTZ, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beqzalc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(ADDI, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bnezalc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rt.is(zero_reg)));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(DADDI, zero_reg, rt, offset,
                    CompactBranchType::COMPACT_BRANCH);
}


void Assembler::beqc(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(rs.code() != rt.code() && rs.code() != 0 && rt.code() != 0);
  if (rs.code() < rt.code()) {
    GenInstrImmediate(ADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(ADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::beqzc(Register rs, int32_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rs.is(zero_reg)));
  GenInstrImmediate(POP66, rs, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::bnec(Register rs, Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(rs.code() != rt.code() && rs.code() != 0 && rt.code() != 0);
  if (rs.code() < rt.code()) {
    GenInstrImmediate(DADDI, rs, rt, offset, CompactBranchType::COMPACT_BRANCH);
  } else {
    GenInstrImmediate(DADDI, rt, rs, offset, CompactBranchType::COMPACT_BRANCH);
  }
}


void Assembler::bnezc(Register rs, int32_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(!(rs.is(zero_reg)));
  GenInstrImmediate(POP76, rs, offset, CompactBranchType::COMPACT_BRANCH);
}


void Assembler::j(int32_t target) {
#if DEBUG
  // Get pc of delay slot.
  uint32_t ipc = reinterpret_cast<uint32_t>(pc_ + 1 * kInstrSize);
  bool in_range = ((ipc ^ static_cast<uint32_t>(target)) >>
                   (kImm26Bits + kImmFieldShift)) == 0;
  DCHECK(in_range && ((target & 3) == 0));
#endif
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrJump(J, (target >> 2) & kImm26Mask);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jr(Register rs) {
  if (!IsMipsArchVariant(kMips32r6)) {
    BlockTrampolinePoolScope block_trampoline_pool(this);
    if (rs.is(ra)) {
      positions_recorder()->WriteRecordedPositions();
    }
    GenInstrRegister(SPECIAL, rs, zero_reg, zero_reg, 0, JR);
    BlockTrampolinePoolFor(1);  // For associated delay slot.
  } else {
    jalr(rs, zero_reg);
  }
}


void Assembler::jal(int32_t target) {
#ifdef DEBUG
  // Get pc of delay slot.
  uint32_t ipc = reinterpret_cast<uint32_t>(pc_ + 1 * kInstrSize);
  bool in_range = ((ipc ^ static_cast<uint32_t>(target)) >>
                   (kImm26Bits + kImmFieldShift)) == 0;
  DCHECK(in_range && ((target & 3) == 0));
#endif
  BlockTrampolinePoolScope block_trampoline_pool(this);
  positions_recorder()->WriteRecordedPositions();
  GenInstrJump(JAL, (target >> 2) & kImm26Mask);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jalr(Register rs, Register rd) {
  DCHECK(rs.code() != rd.code());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  positions_recorder()->WriteRecordedPositions();
  GenInstrRegister(SPECIAL, rs, zero_reg, rd, 0, JALR);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jic(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrImmediate(POP66, zero_reg, rt, offset);
}


void Assembler::jialc(Register rt, int16_t offset) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  positions_recorder()->WriteRecordedPositions();
  GenInstrImmediate(POP76, zero_reg, rt, offset);
}


// -------Data-processing-instructions---------

// Arithmetic.

void Assembler::addu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, ADDU);
}


void Assembler::addiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(ADDIU, rs, rd, j);
}


void Assembler::subu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SUBU);
}


void Assembler::mul(Register rd, Register rs, Register rt) {
  if (!IsMipsArchVariant(kMips32r6)) {
    GenInstrRegister(SPECIAL2, rs, rt, rd, 0, MUL);
  } else {
    GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH);
  }
}


void Assembler::mulu(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH_U);
}


void Assembler::muh(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH);
}


void Assembler::muhu(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH_U);
}


void Assembler::mod(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD);
}


void Assembler::modu(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD_U);
}


void Assembler::mult(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULT);
}


void Assembler::multu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULTU);
}


void Assembler::div(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIV);
}


void Assembler::div(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD);
}


void Assembler::divu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIVU);
}


void Assembler::divu(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD_U);
}


// Logical.

void Assembler::and_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, AND);
}


void Assembler::andi(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(ANDI, rs, rt, j);
}


void Assembler::or_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, OR);
}


void Assembler::ori(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(ORI, rs, rt, j);
}


void Assembler::xor_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, XOR);
}


void Assembler::xori(Register rt, Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(XORI, rs, rt, j);
}


void Assembler::nor(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, NOR);
}


// Shifts.
void Assembler::sll(Register rd,
                    Register rt,
                    uint16_t sa,
                    bool coming_from_nop) {
  // Don't allow nop instructions in the form sll zero_reg, zero_reg to be
  // generated using the sll instruction. They must be generated using
  // nop(int/NopMarkerTypes) or MarkCode(int/NopMarkerTypes) pseudo
  // instructions.
  DCHECK(coming_from_nop || !(rd.is(zero_reg) && rt.is(zero_reg)));
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SLL);
}


void Assembler::sllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLLV);
}


void Assembler::srl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SRL);
}


void Assembler::srlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRLV);
}


void Assembler::sra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa & 0x1F, SRA);
}


void Assembler::srav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRAV);
}


void Assembler::rotr(Register rd, Register rt, uint16_t sa) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | SRL;
  emit(instr);
}


void Assembler::rotrv(Register rd, Register rt, Register rs) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
     | (rd.code() << kRdShift) | (1 << kSaShift) | SRLV;
  emit(instr);
}


void Assembler::lsa(Register rd, Register rt, Register rs, uint8_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid());
  DCHECK(sa < 5 && sa > 0);
  DCHECK(IsMipsArchVariant(kMips32r6));
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift) |
                (rd.code() << kRdShift) | (sa - 1) << kSaShift | LSA;
  emit(instr);
}


// ------------Memory-instructions-------------

// Helper for base-reg + offset, when offset is larger than int16.
void Assembler::LoadRegPlusOffsetToAt(const MemOperand& src) {
  DCHECK(!src.rm().is(at));
  lui(at, (src.offset_ >> kLuiShift) & kImm16Mask);
  ori(at, at, src.offset_ & kImm16Mask);  // Load 32-bit offset.
  addu(at, at, src.rm());  // Add base register.
}


void Assembler::lb(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LB, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LB, at, rd, 0);  // Equiv to lb(rd, MemOperand(at, 0));
  }
}


void Assembler::lbu(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LBU, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LBU, at, rd, 0);  // Equiv to lbu(rd, MemOperand(at, 0));
  }
}


void Assembler::lh(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LH, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LH, at, rd, 0);  // Equiv to lh(rd, MemOperand(at, 0));
  }
}


void Assembler::lhu(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LHU, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LHU, at, rd, 0);  // Equiv to lhu(rd, MemOperand(at, 0));
  }
}


void Assembler::lw(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LW, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LW, at, rd, 0);  // Equiv to lw(rd, MemOperand(at, 0));
  }
}


void Assembler::lwl(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LWL, rs.rm(), rd, rs.offset_);
}


void Assembler::lwr(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LWR, rs.rm(), rd, rs.offset_);
}


void Assembler::sb(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SB, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(SB, at, rd, 0);  // Equiv to sb(rd, MemOperand(at, 0));
  }
}


void Assembler::sh(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SH, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(SH, at, rd, 0);  // Equiv to sh(rd, MemOperand(at, 0));
  }
}


void Assembler::sw(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SW, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(SW, at, rd, 0);  // Equiv to sw(rd, MemOperand(at, 0));
  }
}


void Assembler::swl(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SWL, rs.rm(), rd, rs.offset_);
}


void Assembler::swr(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SWR, rs.rm(), rd, rs.offset_);
}


void Assembler::lui(Register rd, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, zero_reg, rd, j);
}


void Assembler::aui(Register rt, Register rs, int32_t j) {
  // This instruction uses same opcode as 'lui'. The difference in encoding is
  // 'lui' has zero reg. for rs field.
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, rs, rt, j);
}


// ---------PC-Relative instructions-----------

void Assembler::addiupc(Register rs, int32_t imm19) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(rs.is_valid() && is_int19(imm19));
  uint32_t imm21 = ADDIUPC << kImm19Bits | (imm19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::lwpc(Register rs, int32_t offset19) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(rs.is_valid() && is_int19(offset19));
  uint32_t imm21 = LWPC << kImm19Bits | (offset19 & kImm19Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::auipc(Register rs, int16_t imm16) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(rs.is_valid());
  uint32_t imm21 = AUIPC << kImm16Bits | (imm16 & kImm16Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


void Assembler::aluipc(Register rs, int16_t imm16) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(rs.is_valid());
  uint32_t imm21 = ALUIPC << kImm16Bits | (imm16 & kImm16Mask);
  GenInstrImmediate(PCREL, rs, imm21);
}


// -------------Misc-instructions--------------

// Break / Trap instructions.
void Assembler::break_(uint32_t code, bool break_as_stop) {
  DCHECK((code & ~0xfffff) == 0);
  // We need to invalidate breaks that could be stops as well because the
  // simulator expects a char pointer after the stop instruction.
  // See constants-mips.h for explanation.
  DCHECK((break_as_stop &&
          code <= kMaxStopCode &&
          code > kMaxWatchpointCode) ||
         (!break_as_stop &&
          (code > kMaxStopCode ||
           code <= kMaxWatchpointCode)));
  Instr break_instr = SPECIAL | BREAK | (code << 6);
  emit(break_instr);
}


void Assembler::stop(const char* msg, uint32_t code) {
  DCHECK(code > kMaxWatchpointCode);
  DCHECK(code <= kMaxStopCode);
#if V8_HOST_ARCH_MIPS
  break_(0x54321);
#else  // V8_HOST_ARCH_MIPS
  BlockTrampolinePoolFor(2);
  // The Simulator will handle the stop instruction and get the message address.
  // On MIPS stop() is just a special kind of break_().
  break_(code, true);
  emit(reinterpret_cast<Instr>(msg));
#endif
}


void Assembler::tge(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr = SPECIAL | TGE | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tgeu(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr = SPECIAL | TGEU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tlt(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TLT | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tltu(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TLTU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::teq(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TEQ | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tne(Register rs, Register rt, uint16_t code) {
  DCHECK(is_uint10(code));
  Instr instr =
      SPECIAL | TNE | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


// Move from HI/LO register.

void Assembler::mfhi(Register rd) {
  GenInstrRegister(SPECIAL, zero_reg, zero_reg, rd, 0, MFHI);
}


void Assembler::mflo(Register rd) {
  GenInstrRegister(SPECIAL, zero_reg, zero_reg, rd, 0, MFLO);
}


// Set on less than instructions.
void Assembler::slt(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLT);
}


void Assembler::sltu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLTU);
}


void Assembler::slti(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(SLTI, rs, rt, j);
}


void Assembler::sltiu(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(SLTIU, rs, rt, j);
}


// Conditional move.
void Assembler::movz(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVZ);
}


void Assembler::movn(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVN);
}


void Assembler::movt(Register rd, Register rs, uint16_t cc) {
  Register rt;
  rt.reg_code = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::movf(Register rd, Register rs, uint16_t cc) {
  Register rt;
  rt.reg_code = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::seleqz(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELEQZ_S);
}


// Bit twiddling.
void Assembler::clz(Register rd, Register rs) {
  if (!IsMipsArchVariant(kMips32r6)) {
    // Clz instr requires same GPR number in 'rd' and 'rt' fields.
    GenInstrRegister(SPECIAL2, rs, rd, rd, 0, CLZ);
  } else {
    GenInstrRegister(SPECIAL, rs, zero_reg, rd, 1, CLZ_R6);
  }
}


void Assembler::ins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ins.
  // Ins instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, INS);
}


void Assembler::ext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ext.
  // Ext instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, EXT);
}


void Assembler::bitswap(Register rd, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL3, zero_reg, rt, rd, 0, BSHFL);
}


void Assembler::pref(int32_t hint, const MemOperand& rs) {
  DCHECK(!IsMipsArchVariant(kLoongson));
  DCHECK(is_uint5(hint) && is_uint16(rs.offset_));
  Instr instr = PREF | (rs.rm().code() << kRsShift) | (hint << kRtShift)
      | (rs.offset_);
  emit(instr);
}


void Assembler::align(Register rd, Register rs, Register rt, uint8_t bp) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK(is_uint3(bp));
  uint16_t sa = (ALIGN << kBp2Bits) | bp;
  GenInstrRegister(SPECIAL3, rs, rt, rd, sa, BSHFL);
}


// --------Coprocessor-instructions----------------

// Load, store, move.
void Assembler::lwc1(FPURegister fd, const MemOperand& src) {
  if (is_int16(src.offset_)) {
    GenInstrImmediate(LWC1, src.rm(), fd, src.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(src);
    GenInstrImmediate(LWC1, at, fd, 0);
  }
}


void Assembler::ldc1(FPURegister fd, const MemOperand& src) {
  // Workaround for non-8-byte alignment of HeapNumber, convert 64-bit
  // load to two 32-bit loads.
  DCHECK(!src.rm().is(at));
  if (IsFp64Mode()) {
    if (is_int16(src.offset_) && is_int16(src.offset_ + kIntSize)) {
      GenInstrImmediate(LWC1, src.rm(), fd,
                        src.offset_ + Register::kMantissaOffset);
      GenInstrImmediate(LW, src.rm(), at,
                        src.offset_ + Register::kExponentOffset);
      mthc1(at, fd);
    } else {  // Offset > 16 bits, use multiple instructions to load.
      LoadRegPlusOffsetToAt(src);
      GenInstrImmediate(LWC1, at, fd, Register::kMantissaOffset);
      GenInstrImmediate(LW, at, at, Register::kExponentOffset);
      mthc1(at, fd);
    }
  } else {  // fp32 mode.
    if (is_int16(src.offset_) && is_int16(src.offset_ + kIntSize)) {
      GenInstrImmediate(LWC1, src.rm(), fd,
                        src.offset_ + Register::kMantissaOffset);
      FPURegister nextfpreg;
      nextfpreg.setcode(fd.code() + 1);
      GenInstrImmediate(LWC1, src.rm(), nextfpreg,
                        src.offset_ + Register::kExponentOffset);
    } else {  // Offset > 16 bits, use multiple instructions to load.
      LoadRegPlusOffsetToAt(src);
      GenInstrImmediate(LWC1, at, fd, Register::kMantissaOffset);
      FPURegister nextfpreg;
      nextfpreg.setcode(fd.code() + 1);
      GenInstrImmediate(LWC1, at, nextfpreg, Register::kExponentOffset);
    }
  }
}


void Assembler::swc1(FPURegister fd, const MemOperand& src) {
  if (is_int16(src.offset_)) {
    GenInstrImmediate(SWC1, src.rm(), fd, src.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(src);
    GenInstrImmediate(SWC1, at, fd, 0);
  }
}


void Assembler::sdc1(FPURegister fd, const MemOperand& src) {
  // Workaround for non-8-byte alignment of HeapNumber, convert 64-bit
  // store to two 32-bit stores.
  DCHECK(!src.rm().is(at));
  DCHECK(!src.rm().is(t8));
  if (IsFp64Mode()) {
    if (is_int16(src.offset_) && is_int16(src.offset_ + kIntSize)) {
      GenInstrImmediate(SWC1, src.rm(), fd,
                        src.offset_ + Register::kMantissaOffset);
      mfhc1(at, fd);
      GenInstrImmediate(SW, src.rm(), at,
                        src.offset_ + Register::kExponentOffset);
    } else {  // Offset > 16 bits, use multiple instructions to load.
      LoadRegPlusOffsetToAt(src);
      GenInstrImmediate(SWC1, at, fd, Register::kMantissaOffset);
      mfhc1(t8, fd);
      GenInstrImmediate(SW, at, t8, Register::kExponentOffset);
    }
  } else {  // fp32 mode.
    if (is_int16(src.offset_) && is_int16(src.offset_ + kIntSize)) {
      GenInstrImmediate(SWC1, src.rm(), fd,
                        src.offset_ + Register::kMantissaOffset);
      FPURegister nextfpreg;
      nextfpreg.setcode(fd.code() + 1);
      GenInstrImmediate(SWC1, src.rm(), nextfpreg,
                        src.offset_ + Register::kExponentOffset);
    } else {  // Offset > 16 bits, use multiple instructions to load.
      LoadRegPlusOffsetToAt(src);
      GenInstrImmediate(SWC1, at, fd, Register::kMantissaOffset);
      FPURegister nextfpreg;
      nextfpreg.setcode(fd.code() + 1);
      GenInstrImmediate(SWC1, at, nextfpreg, Register::kExponentOffset);
    }
  }
}


void Assembler::mtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTC1, rt, fs, f0);
}


void Assembler::mthc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTHC1, rt, fs, f0);
}


void Assembler::mfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFC1, rt, fs, f0);
}


void Assembler::mfhc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFHC1, rt, fs, f0);
}


void Assembler::ctc1(Register rt, FPUControlRegister fs) {
  GenInstrRegister(COP1, CTC1, rt, fs);
}


void Assembler::cfc1(Register rt, FPUControlRegister fs) {
  GenInstrRegister(COP1, CFC1, rt, fs);
}


void Assembler::DoubleAsTwoUInt32(double d, uint32_t* lo, uint32_t* hi) {
  uint64_t i;
  memcpy(&i, &d, 8);

  *lo = i & 0xffffffff;
  *hi = i >> 32;
}


void Assembler::movn_s(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, S, rt, fs, fd, MOVN_C);
}


void Assembler::movn_d(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, D, rt, fs, fd, MOVN_C);
}


void Assembler::sel(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));

  GenInstrRegister(COP1, fmt, ft, fs, fd, SEL);
}


void Assembler::sel_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  sel(S, fd, fs, ft);
}


void Assembler::sel_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  sel(D, fd, fs, ft);
}


void Assembler::seleqz(SecondaryField fmt, FPURegister fd, FPURegister fs,
                       FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, SELEQZ_C);
}


void Assembler::selnez(Register rd, Register rs, Register rt) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELNEZ_S);
}


void Assembler::selnez(SecondaryField fmt, FPURegister fd, FPURegister fs,
                       FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, SELNEZ_C);
}


void Assembler::seleqz_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  seleqz(D, fd, fs, ft);
}


void Assembler::seleqz_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  seleqz(S, fd, fs, ft);
}


void Assembler::selnez_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  selnez(D, fd, fs, ft);
}


void Assembler::selnez_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  selnez(S, fd, fs, ft);
}


void Assembler::movz_s(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, S, rt, fs, fd, MOVZ_C);
}


void Assembler::movz_d(FPURegister fd, FPURegister fs, Register rt) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, D, rt, fs, fd, MOVZ_C);
}


void Assembler::movt_s(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(COP1, S, ft, fs, fd, MOVF);
}


void Assembler::movt_d(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(COP1, D, ft, fs, fd, MOVF);
}


void Assembler::movf_s(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(COP1, S, ft, fs, fd, MOVF);
}


void Assembler::movf_d(FPURegister fd, FPURegister fs, uint16_t cc) {
  DCHECK(!IsMipsArchVariant(kMips32r6));
  FPURegister ft;
  ft.reg_code = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(COP1, D, ft, fs, fd, MOVF);
}


// Arithmetic.

void Assembler::add_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, ADD_S);
}


void Assembler::add_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, ADD_D);
}


void Assembler::sub_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, SUB_S);
}


void Assembler::sub_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, SUB_D);
}


void Assembler::mul_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, MUL_S);
}


void Assembler::mul_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, MUL_D);
}


void Assembler::madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
    FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r2));
  GenInstrRegister(COP1X, fr, ft, fs, fd, MADD_D);
}


void Assembler::div_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, S, ft, fs, fd, DIV_S);
}


void Assembler::div_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, DIV_D);
}


void Assembler::abs_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ABS_S);
}


void Assembler::abs_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ABS_D);
}


void Assembler::mov_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, MOV_D);
}


void Assembler::mov_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, MOV_S);
}


void Assembler::neg_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, NEG_S);
}


void Assembler::neg_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, NEG_D);
}


void Assembler::sqrt_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, SQRT_S);
}


void Assembler::sqrt_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, SQRT_D);
}


void Assembler::rsqrt_s(FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, S, f0, fs, fd, RSQRT_S);
}


void Assembler::rsqrt_d(FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, D, f0, fs, fd, RSQRT_D);
}


void Assembler::recip_d(FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, D, f0, fs, fd, RECIP_D);
}


void Assembler::recip_s(FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, S, f0, fs, fd, RECIP_S);
}


// Conversions.

void Assembler::cvt_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_W_S);
}


void Assembler::cvt_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_W_D);
}


void Assembler::trunc_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_W_S);
}


void Assembler::trunc_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_W_D);
}


void Assembler::round_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_W_S);
}


void Assembler::round_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_W_D);
}


void Assembler::floor_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_W_S);
}


void Assembler::floor_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_W_D);
}


void Assembler::ceil_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_W_S);
}


void Assembler::ceil_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_W_D);
}


void Assembler::rint_s(FPURegister fd, FPURegister fs) { rint(S, fd, fs); }


void Assembler::rint(SecondaryField fmt, FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, f0, fs, fd, RINT);
}


void Assembler::rint_d(FPURegister fd, FPURegister fs) { rint(D, fd, fs); }


void Assembler::cvt_l_s(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_L_S);
}


void Assembler::cvt_l_d(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_L_D);
}


void Assembler::trunc_l_s(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_L_S);
}


void Assembler::trunc_l_d(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_L_D);
}


void Assembler::round_l_s(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_L_S);
}


void Assembler::round_l_d(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_L_D);
}


void Assembler::floor_l_s(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_L_S);
}


void Assembler::floor_l_d(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_L_D);
}


void Assembler::ceil_l_s(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_L_S);
}


void Assembler::ceil_l_d(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_L_D);
}


void Assembler::class_s(FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, S, f0, fs, fd, CLASS_S);
}


void Assembler::class_d(FPURegister fd, FPURegister fs) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  GenInstrRegister(COP1, D, f0, fs, fd, CLASS_D);
}


void Assembler::min(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MIN);
}


void Assembler::mina(SecondaryField fmt, FPURegister fd, FPURegister fs,
                     FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MINA);
}


void Assembler::max(SecondaryField fmt, FPURegister fd, FPURegister fs,
                    FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAX);
}


void Assembler::maxa(SecondaryField fmt, FPURegister fd, FPURegister fs,
                     FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAXA);
}


void Assembler::min_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  min(S, fd, fs, ft);
}


void Assembler::min_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  min(D, fd, fs, ft);
}


void Assembler::max_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  max(S, fd, fs, ft);
}


void Assembler::max_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  max(D, fd, fs, ft);
}


void Assembler::mina_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  mina(S, fd, fs, ft);
}


void Assembler::mina_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  mina(D, fd, fs, ft);
}


void Assembler::maxa_s(FPURegister fd, FPURegister fs, FPURegister ft) {
  maxa(S, fd, fs, ft);
}


void Assembler::maxa_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  maxa(D, fd, fs, ft);
}


void Assembler::cvt_s_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_S_W);
}


void Assembler::cvt_s_l(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_S_L);
}


void Assembler::cvt_s_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_S_D);
}


void Assembler::cvt_d_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_D_W);
}


void Assembler::cvt_d_l(FPURegister fd, FPURegister fs) {
  DCHECK((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
         IsFp64Mode());
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_D_L);
}


void Assembler::cvt_d_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_D_S);
}


// Conditions for >= MIPSr6.
void Assembler::cmp(FPUCondition cond, SecondaryField fmt,
    FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  DCHECK((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | (0 << 5) | cond;
  emit(instr);
}


void Assembler::cmp_s(FPUCondition cond, FPURegister fd, FPURegister fs,
                      FPURegister ft) {
  cmp(cond, W, fd, fs, ft);
}

void Assembler::cmp_d(FPUCondition cond, FPURegister fd, FPURegister fs,
                      FPURegister ft) {
  cmp(cond, L, fd, fs, ft);
}


void Assembler::bc1eqz(int16_t offset, FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  Instr instr = COP1 | BC1EQZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1nez(int16_t offset, FPURegister ft) {
  DCHECK(IsMipsArchVariant(kMips32r6));
  Instr instr = COP1 | BC1NEZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


// Conditions for < MIPSr6.
void Assembler::c(FPUCondition cond, SecondaryField fmt,
    FPURegister fs, FPURegister ft, uint16_t cc) {
  DCHECK(is_uint3(cc));
  DCHECK(fmt == S || fmt == D);
  DCHECK((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << 16 | fs.code() << kFsShift
      | cc << 8 | 3 << 4 | cond;
  emit(instr);
}


void Assembler::c_s(FPUCondition cond, FPURegister fs, FPURegister ft,
                    uint16_t cc) {
  c(cond, S, fs, ft, cc);
}


void Assembler::c_d(FPUCondition cond, FPURegister fs, FPURegister ft,
                    uint16_t cc) {
  c(cond, D, fs, ft, cc);
}


void Assembler::fcmp(FPURegister src1, const double src2,
      FPUCondition cond) {
  DCHECK(src2 == 0.0);
  mtc1(zero_reg, f14);
  cvt_d_w(f14, f14);
  c(cond, D, src1, f14, 0);
}


void Assembler::bc1f(int16_t offset, uint16_t cc) {
  DCHECK(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 0 << 16 | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1t(int16_t offset, uint16_t cc) {
  DCHECK(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 1 << 16 | (offset & kImm16Mask);
  emit(instr);
}


int Assembler::RelocateInternalReference(RelocInfo::Mode rmode, byte* pc,
                                         intptr_t pc_delta) {
  Instr instr = instr_at(pc);

  if (RelocInfo::IsInternalReference(rmode)) {
    int32_t* p = reinterpret_cast<int32_t*>(pc);
    if (*p == 0) {
      return 0;  // Number of instructions patched.
    }
    *p += pc_delta;
    return 1;  // Number of instructions patched.
  } else {
    DCHECK(RelocInfo::IsInternalReferenceEncoded(rmode));
    if (IsLui(instr)) {
      Instr instr_lui = instr_at(pc + 0 * Assembler::kInstrSize);
      Instr instr_ori = instr_at(pc + 1 * Assembler::kInstrSize);
      DCHECK(IsOri(instr_ori));
      int32_t imm = (instr_lui & static_cast<int32_t>(kImm16Mask)) << kLuiShift;
      imm |= (instr_ori & static_cast<int32_t>(kImm16Mask));
      if (imm == kEndOfJumpChain) {
        return 0;  // Number of instructions patched.
      }
      imm += pc_delta;
      DCHECK((imm & 3) == 0);

      instr_lui &= ~kImm16Mask;
      instr_ori &= ~kImm16Mask;

      instr_at_put(pc + 0 * Assembler::kInstrSize,
                   instr_lui | ((imm >> kLuiShift) & kImm16Mask));
      instr_at_put(pc + 1 * Assembler::kInstrSize,
                   instr_ori | (imm & kImm16Mask));
      return 2;  // Number of instructions patched.
    } else {
      UNREACHABLE();
      return 0;
    }
  }
}


void Assembler::GrowBuffer() {
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // The new buffer.
  if (buffer_size_ < 1 * MB) {
    desc.buffer_size = 2*buffer_size_;
  } else {
    desc.buffer_size = buffer_size_ + 1*MB;
  }
  CHECK_GT(desc.buffer_size, 0);  // No overflow.

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;

  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();

  // Copy the data.
  int pc_delta = desc.buffer - buffer_;
  int rc_delta = (desc.buffer + desc.buffer_size) - (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(reloc_info_writer.pos() + rc_delta, reloc_info_writer.pos(),
          desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate runtime entries.
  for (RelocIterator it(desc); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::INTERNAL_REFERENCE_ENCODED ||
        rmode == RelocInfo::INTERNAL_REFERENCE) {
      byte* p = reinterpret_cast<byte*>(it.rinfo()->pc());
      RelocateInternalReference(rmode, p, pc_delta);
    }
  }
  DCHECK(!overflow());
}


void Assembler::db(uint8_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dd(uint32_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dq(uint64_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}


void Assembler::dd(Label* label) {
  uint32_t data;
  CheckForEmitInForbiddenSlot();
  if (label->is_bound()) {
    data = reinterpret_cast<uint32_t>(buffer_ + label->pos());
  } else {
    data = jump_address(label);
    internal_reference_positions_.insert(label->pos());
  }
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  EmitHelper(data);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(isolate(), pc_, rmode, data, NULL);
  if (rmode >= RelocInfo::COMMENT &&
      rmode <= RelocInfo::DEBUG_BREAK_SLOT_AT_CALL) {
    // Adjust code for new modes.
    DCHECK(RelocInfo::IsDebugBreakSlot(rmode)
           || RelocInfo::IsComment(rmode)
           || RelocInfo::IsPosition(rmode));
    // These modes do not need an entry in the constant pool.
  }
  if (!RelocInfo::IsNone(rinfo.rmode())) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
        !serializer_enabled() && !emit_debug_code()) {
      return;
    }
    DCHECK(buffer_space() >= kMaxRelocSize);  // Too late to grow buffer here.
    if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
      RelocInfo reloc_info_with_ast_id(isolate(), pc_, rmode,
                                       RecordedAstId().ToInt(), NULL);
      ClearRecordedAstId();
      reloc_info_writer.Write(&reloc_info_with_ast_id);
    } else {
      reloc_info_writer.Write(&rinfo);
    }
  }
}


void Assembler::BlockTrampolinePoolFor(int instructions) {
  CheckTrampolinePoolQuick(instructions);
  BlockTrampolinePoolBefore(pc_offset() + instructions * kInstrSize);
}


void Assembler::CheckTrampolinePool() {
  // Some small sequences of instructions must not be broken up by the
  // insertion of a trampoline pool; such sequences are protected by setting
  // either trampoline_pool_blocked_nesting_ or no_trampoline_pool_before_,
  // which are both checked here. Also, recursive calls to CheckTrampolinePool
  // are blocked by trampoline_pool_blocked_nesting_.
  if ((trampoline_pool_blocked_nesting_ > 0) ||
      (pc_offset() < no_trampoline_pool_before_)) {
    // Emission is currently blocked; make sure we try again as soon as
    // possible.
    if (trampoline_pool_blocked_nesting_ > 0) {
      next_buffer_check_ = pc_offset() + kInstrSize;
    } else {
      next_buffer_check_ = no_trampoline_pool_before_;
    }
    return;
  }

  DCHECK(!trampoline_emitted_);
  DCHECK(unbound_labels_count_ >= 0);
  if (unbound_labels_count_ > 0) {
    // First we emit jump (2 instructions), then we emit trampoline pool.
    { BlockTrampolinePoolScope block_trampoline_pool(this);
      Label after_pool;
      if (IsMipsArchVariant(kMips32r6)) {
        bc(&after_pool);
      } else {
        b(&after_pool);
        nop();
      }

      int pool_start = pc_offset();
      for (int i = 0; i < unbound_labels_count_; i++) {
        uint32_t imm32;
        imm32 = jump_address(&after_pool);
        { BlockGrowBufferScope block_buf_growth(this);
          // Buffer growth (and relocation) must be blocked for internal
          // references until associated instructions are emitted and available
          // to be patched.
          RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE_ENCODED);
          lui(at, (imm32 & kHiMask) >> kLuiShift);
          ori(at, at, (imm32 & kImm16Mask));
        }
        jr(at);
        nop();
      }
      bind(&after_pool);
      trampoline_ = Trampoline(pool_start, unbound_labels_count_);

      trampoline_emitted_ = true;
      // As we are only going to emit trampoline once, we need to prevent any
      // further emission.
      next_buffer_check_ = kMaxInt;
    }
  } else {
    // Number of branches to unbound label at this point is zero, so we can
    // move next buffer check to maximum.
    next_buffer_check_ = pc_offset() +
        kMaxBranchOffset - kTrampolineSlotsSize * 16;
  }
  return;
}


Address Assembler::target_address_at(Address pc) {
  Instr instr1 = instr_at(pc);
  Instr instr2 = instr_at(pc + kInstrSize);
  // Interpret 2 instructions generated by li: lui/ori
  if ((GetOpcodeField(instr1) == LUI) && (GetOpcodeField(instr2) == ORI)) {
    // Assemble the 32 bit value.
    return reinterpret_cast<Address>(
        (GetImmediate16(instr1) << 16) | GetImmediate16(instr2));
  }

  // We should never get here, force a bad address if we do.
  UNREACHABLE();
  return (Address)0x0;
}


// MIPS and ia32 use opposite encoding for qNaN and sNaN, such that ia32
// qNaN is a MIPS sNaN, and ia32 sNaN is MIPS qNaN. If running from a heap
// snapshot generated on ia32, the resulting MIPS sNaN must be quieted.
// OS::nan_value() returns a qNaN.
void Assembler::QuietNaN(HeapObject* object) {
  HeapNumber::cast(object)->set_value(std::numeric_limits<double>::quiet_NaN());
}


// On Mips, a target address is stored in a lui/ori instruction pair, each
// of which load 16 bits of the 32-bit address to a register.
// Patching the address must replace both instr, and flush the i-cache.
//
// There is an optimization below, which emits a nop when the address
// fits in just 16 bits. This is unlikely to help, and should be benchmarked,
// and possibly removed.
void Assembler::set_target_address_at(Isolate* isolate, Address pc,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  Instr instr2 = instr_at(pc + kInstrSize);
  uint32_t rt_code = GetRtField(instr2);
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
  uint32_t itarget = reinterpret_cast<uint32_t>(target);

#ifdef DEBUG
  // Check we have the result from a li macro-instruction, using instr pair.
  Instr instr1 = instr_at(pc);
  CHECK((GetOpcodeField(instr1) == LUI && GetOpcodeField(instr2) == ORI));
#endif

  // Must use 2 instructions to insure patchable code => just use lui and ori.
  // lui rt, upper-16.
  // ori rt rt, lower-16.
  *p = LUI | rt_code | ((itarget & kHiMask) >> kLuiShift);
  *(p + 1) = ORI | rt_code | (rt_code << 5) | (itarget & kImm16Mask);


  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(isolate, pc, 2 * sizeof(int32_t));
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS
