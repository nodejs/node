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


#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/base/cpu.h"
#include "src/mips64/assembler-mips64-inl.h"
#include "src/serialize.h"

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


const char* DoubleRegister::AllocationIndexToString(int index) {
  DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
  const char* const names[] = {
    "f0",
    "f2",
    "f4",
    "f6",
    "f8",
    "f10",
    "f12",
    "f14",
    "f16",
    "f18",
    "f20",
    "f22",
    "f24",
    "f26"
  };
  return names[index];
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
#else
  // Probe for additional features at runtime.
  base::CPU cpu;
  if (cpu.has_fpu()) supported_ |= 1u << FPU;
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
    8,    // a4
    9,    // a5
    10,   // a6
    11,   // a7
    12,   // t0
    13,   // t1
    14,   // t2
    15,   // t3
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
    a0, a1, a2, a3, a4, a5, a6, a7,
    t0, t1, t2, t3,
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
                                  1 << RelocInfo::INTERNAL_REFERENCE;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on MIPS means that it is a lui/ori instruction, and that is
  // always the case inside code objects.
  return true;
}


bool RelocInfo::IsInConstantPool() {
  return false;
}


// Patch the code at the current address with the supplied instructions.
void RelocInfo::PatchCode(byte* instructions, int instruction_count) {
  Instr* pc = reinterpret_cast<Instr*>(pc_);
  Instr* instr = reinterpret_cast<Instr*>(instructions);
  for (int i = 0; i < instruction_count; i++) {
    *(pc + i) = *(instr + i);
  }

  // Indicate that code has changed.
  CpuFeatures::FlushICache(pc_, instruction_count * Assembler::kInstrSize);
}


// Patch the code at the current PC with a call to the target address.
// Additional guard instructions can be added if required.
void RelocInfo::PatchCodeWithCall(Address target, int guard_bytes) {
  // Patch the code at the current address with a call to the target.
  UNIMPLEMENTED_MIPS();
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
    imm64_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // No relocation needed.
    imm64_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE64;
  }
}


MemOperand::MemOperand(Register rm, int64_t offset) : Operand(rm) {
  offset_ = offset;
}


MemOperand::MemOperand(Register rm, int64_t unit, int64_t multiplier,
                       OffsetAddend offset_addend) : Operand(rm) {
  offset_ = unit * multiplier + offset_addend;
}


// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

static const int kNegOffset = 0x00008000;
// daddiu(sp, sp, 8) aka Pop() operation or part of Pop(r)
// operations as post-increment of sp.
const Instr kPopInstruction = DADDIU | (kRegister_sp_Code << kRsShift)
      | (kRegister_sp_Code << kRtShift)
      | (kPointerSize & kImm16Mask);  // NOLINT
// daddiu(sp, sp, -8) part of Push(r) operation as pre-decrement of sp.
const Instr kPushInstruction = DADDIU | (kRegister_sp_Code << kRsShift)
      | (kRegister_sp_Code << kRtShift)
      | (-kPointerSize & kImm16Mask);  // NOLINT
// sd(r, MemOperand(sp, 0))
const Instr kPushRegPattern = SD | (kRegister_sp_Code << kRsShift)
      |  (0 & kImm16Mask);  // NOLINT
//  ld(r, MemOperand(sp, 0))
const Instr kPopRegPattern = LD | (kRegister_sp_Code << kRsShift)
      |  (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpOffsetPattern = LW | (kRegister_fp_Code << kRsShift)
      |  (0 & kImm16Mask);  // NOLINT

const Instr kSwRegFpOffsetPattern = SW | (kRegister_fp_Code << kRsShift)
      |  (0 & kImm16Mask);  // NOLINT

const Instr kLwRegFpNegOffsetPattern = LW | (kRegister_fp_Code << kRsShift)
      |  (kNegOffset & kImm16Mask);  // NOLINT

const Instr kSwRegFpNegOffsetPattern = SW | (kRegister_fp_Code << kRsShift)
      |  (kNegOffset & kImm16Mask);  // NOLINT
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
  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.
  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc->origin = this;
}


void Assembler::Align(int m) {
  DCHECK(m >= 4 && base::bits::IsPowerOfTwo32(m));
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
  rt.code_ = (instr & kRtFieldMask) >> kRtShift;
  return rt;
}


Register Assembler::GetRsReg(Instr instr) {
  Register rs;
  rs.code_ = (instr & kRsFieldMask) >> kRsShift;
  return rs;
}


Register Assembler::GetRdReg(Instr instr) {
  Register rd;
  rd.code_ = (instr & kRdFieldMask) >> kRdShift;
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
  return opcode == BEQ ||
      opcode == BNE ||
      opcode == BLEZ ||
      opcode == BGTZ ||
      opcode == BEQL ||
      opcode == BNEL ||
      opcode == BLEZL ||
      opcode == BGTZL ||
      (opcode == REGIMM && (rt_field == BLTZ || rt_field == BGEZ ||
                            rt_field == BLTZAL || rt_field == BGEZAL)) ||
      (opcode == COP1 && rs_field == BC1) ||  // Coprocessor branch.
      (opcode == COP1 && rs_field == BC1EQZ) ||
      (opcode == COP1 && rs_field == BC1NEZ);
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
  return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JR;
}


bool Assembler::IsJalr(Instr instr) {
  return GetOpcodeField(instr) == SPECIAL && GetFunctionField(instr) == JALR;
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
  return ((instr & kOpcodeMask) == LW);
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
  return ((instr & kOpcodeMask) == SW);
}


Instr Assembler::SetSwOffset(Instr instr, int16_t offset) {
  DCHECK(IsSw(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAddImmediate(Instr instr) {
  return ((instr & kOpcodeMask) == ADDIU || (instr & kOpcodeMask) == DADDIU);
}


Instr Assembler::SetAddImmediateOffset(Instr instr, int16_t offset) {
  DCHECK(IsAddImmediate(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAndImmediate(Instr instr) {
  return GetOpcodeField(instr) == ANDI;
}


int64_t Assembler::target_at(int64_t pos) {
  Instr instr = instr_at(pos);
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
  DCHECK(IsBranch(instr) || IsJ(instr) || IsLui(instr));
  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmetic shifts for signed integers.
  if (IsBranch(instr)) {
    int32_t imm18 = ((instr & static_cast<int32_t>(kImm16Mask)) << 16) >> 14;
    if (imm18 == kEndOfChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      return pos + kBranchPCOffset + imm18;
    }
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pos + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));

    // TODO(plind) create named constants for shift values.
    int64_t imm = static_cast<int64_t>(instr_lui & kImm16Mask) << 48;
    imm |= static_cast<int64_t>(instr_ori & kImm16Mask) << 32;
    imm |= static_cast<int64_t>(instr_ori2 & kImm16Mask) << 16;
    // Sign extend address;
    imm >>= 16;

    if (imm == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint64_t instr_address = reinterpret_cast<int64_t>(buffer_ + pos);
      int64_t delta = instr_address - imm;
      DCHECK(pos > delta);
      return pos - delta;
    }
  } else {
    int32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    if (imm28 == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint64_t instr_address = reinterpret_cast<int64_t>(buffer_ + pos);
      instr_address &= kImm28Mask;
      int64_t delta = instr_address - imm28;
      DCHECK(pos > delta);
      return pos - delta;
    }
  }
}


void Assembler::target_at_put(int64_t pos, int64_t target_pos) {
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    DCHECK(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    return;
  }

  DCHECK(IsBranch(instr) || IsJ(instr) || IsLui(instr));
  if (IsBranch(instr)) {
    int32_t imm18 = target_pos - (pos + kBranchPCOffset);
    DCHECK((imm18 & 3) == 0);

    instr &= ~kImm16Mask;
    int32_t imm16 = imm18 >> 2;
    DCHECK(is_int16(imm16));

    instr_at_put(pos, instr | (imm16 & kImm16Mask));
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pos + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));

    uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
    DCHECK((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;
    instr_ori2 &= ~kImm16Mask;

    instr_at_put(pos + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> 32) & kImm16Mask));
    instr_at_put(pos + 1 * Assembler::kInstrSize,
                 instr_ori | ((imm >> 16) & kImm16Mask));
    instr_at_put(pos + 3 * Assembler::kInstrSize,
                 instr_ori2 | (imm & kImm16Mask));
  } else {
    uint64_t imm28 = reinterpret_cast<uint64_t>(buffer_) + target_pos;
    imm28 &= kImm28Mask;
    DCHECK((imm28 & 3) == 0);

    instr &= ~kImm26Mask;
    uint32_t imm26 = imm28 >> 2;
    DCHECK(is_uint26(imm26));

    instr_at_put(pos, instr | (imm26 & kImm26Mask));
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
      next(&l);
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}


void Assembler::bind_to(Label* L, int pos) {
  DCHECK(0 <= pos && pos <= pc_offset());  // Must have valid binding position.
  int32_t trampoline_pos = kInvalidSlotPos;
  if (L->is_linked() && !trampoline_emitted_) {
    unbound_labels_count_--;
    next_buffer_check_ += kTrampolineSlotsSize;
  }

  while (L->is_linked()) {
    int32_t fixup_pos = L->pos();
    int32_t dist = pos - fixup_pos;
    next(L);  // Call next before overwriting link with target at fixup_pos.
    Instr instr = instr_at(fixup_pos);
    if (IsBranch(instr)) {
      if (dist > kMaxBranchOffset) {
        if (trampoline_pos == kInvalidSlotPos) {
          trampoline_pos = get_trampoline_entry(fixup_pos);
          CHECK(trampoline_pos != kInvalidSlotPos);
        }
        DCHECK((trampoline_pos - fixup_pos) <= kMaxBranchOffset);
        target_at_put(fixup_pos, trampoline_pos);
        fixup_pos = trampoline_pos;
        dist = pos - fixup_pos;
      }
      target_at_put(fixup_pos, pos);
    } else {
      DCHECK(IsJ(instr) || IsLui(instr) || IsEmittedConstant(instr));
      target_at_put(fixup_pos, pos);
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


void Assembler::next(Label* L) {
  DCHECK(L->is_linked());
  int link = target_at(L->pos());
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    DCHECK(link >= 0);
    L->link_to(link);
  }
}


bool Assembler::is_near(Label* L) {
  if (L->is_bound()) {
    return ((pc_offset() - L->pos()) < kMaxBranchOffset - 4 * kInstrSize);
  }
  return false;
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
void Assembler::GenInstrImmediate(Opcode opcode,
                                  Register rs,
                                  Register rt,
                                  int32_t j) {
  DCHECK(rs.is_valid() && rt.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (j & kImm16Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode,
                                  Register rs,
                                  SecondaryField SF,
                                  int32_t j) {
  DCHECK(rs.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | SF | (j & kImm16Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode,
                                  Register rs,
                                  FPURegister ft,
                                  int32_t j) {
  DCHECK(rs.is_valid() && ft.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (ft.code() << kFtShift)
      | (j & kImm16Mask);
  emit(instr);
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


uint64_t Assembler::jump_address(Label* L) {
  int64_t target_pos;
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

  uint64_t imm = reinterpret_cast<uint64_t>(buffer_) + target_pos;
  DCHECK((imm & 3) == 0);

  return imm;
}


int32_t Assembler::branch_offset(Label* L, bool jump_elimination_allowed) {
  int32_t target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - (pc_offset() + kBranchPCOffset);
  DCHECK((offset & 3) == 0);
  DCHECK(is_int16(offset >> 2));

  return offset;
}


int32_t Assembler::branch_offset_compact(Label* L,
    bool jump_elimination_allowed) {
  int32_t target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - pc_offset();
  DCHECK((offset & 3) == 0);
  DCHECK(is_int16(offset >> 2));

  return offset;
}


int32_t Assembler::branch_offset21(Label* L, bool jump_elimination_allowed) {
  int32_t target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - (pc_offset() + kBranchPCOffset);
  DCHECK((offset & 3) == 0);
  DCHECK(((offset >> 2) & 0xFFE00000) == 0);  // Offset is 21bit width.

  return offset;
}


int32_t Assembler::branch_offset21_compact(Label* L,
    bool jump_elimination_allowed) {
  int32_t target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();
      L->link_to(pc_offset());
    } else {
      L->link_to(pc_offset());
      if (!trampoline_emitted_) {
        unbound_labels_count_++;
        next_buffer_check_ -= kTrampolineSlotsSize;
      }
      return kEndOfChain;
    }
  }

  int32_t offset = target_pos - pc_offset();
  DCHECK((offset & 3) == 0);
  DCHECK(((offset >> 2) & 0xFFE00000) == 0);  // Offset is 21bit width.

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
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZL, rt, rt, offset);
}


void Assembler::bgeuc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZ, rs, rt, offset);
}


void Assembler::bgec(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BLEZL, rs, rt, offset);
}


void Assembler::bgezal(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6 || rs.is(zero_reg));
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
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZL, zero_reg, rt, offset);
}


void Assembler::blez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BLEZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::blezc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZL, zero_reg, rt, offset);
}


void Assembler::bltzc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZL, rt, rt, offset);
}


void Assembler::bltuc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZ, rs, rt, offset);
}


void Assembler::bltc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(!(rt.is(zero_reg)));
  DCHECK(rs.code() != rt.code());
  GenInstrImmediate(BGTZL, rs, rt, offset);
}


void Assembler::bltz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzal(Register rs, int16_t offset) {
  DCHECK(kArchVariant != kMips64r6 || rs.is(zero_reg));
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
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(rs.code() >= rt.code());
  GenInstrImmediate(ADDI, rs, rt, offset);
}


void Assembler::bnvc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  DCHECK(rs.code() >= rt.code());
  GenInstrImmediate(DADDI, rs, rt, offset);
}


void Assembler::blezalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZ, zero_reg, rt, offset);
}


void Assembler::bgezalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BLEZ, rt, rt, offset);
}


void Assembler::bgezall(Register rs, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  GenInstrImmediate(REGIMM, rs, BGEZALL, offset);
}


void Assembler::bltzalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZ, rt, rt, offset);
}


void Assembler::bgtzalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(BGTZ, zero_reg, rt, offset);
}


void Assembler::beqzalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(ADDI, zero_reg, rt, offset);
}


void Assembler::bnezalc(Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rt.is(zero_reg)));
  GenInstrImmediate(DADDI, zero_reg, rt, offset);
}


void Assembler::beqc(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.code() < rt.code());
  GenInstrImmediate(ADDI, rs, rt, offset);
}


void Assembler::beqzc(Register rs, int32_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  Instr instr = BEQZC | (rs.code() << kRsShift) | offset;
  emit(instr);
}


void Assembler::bnec(Register rs, Register rt, int16_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(rs.code() < rt.code());
  GenInstrImmediate(DADDI, rs, rt, offset);
}


void Assembler::bnezc(Register rs, int32_t offset) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(!(rs.is(zero_reg)));
  Instr instr = BNEZC | (rs.code() << kRsShift) | offset;
  emit(instr);
}


void Assembler::j(int64_t target) {
#if DEBUG
  // Get pc of delay slot.
  uint64_t ipc = reinterpret_cast<uint64_t>(pc_ + 1 * kInstrSize);
  bool in_range = (ipc ^ static_cast<uint64_t>(target) >>
                  (kImm26Bits + kImmFieldShift)) == 0;
  DCHECK(in_range && ((target & 3) == 0));
#endif
  GenInstrJump(J, target >> 2);
}


void Assembler::jr(Register rs) {
  if (kArchVariant != kMips64r6) {
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


void Assembler::jal(int64_t target) {
#ifdef DEBUG
  // Get pc of delay slot.
  uint64_t ipc = reinterpret_cast<uint64_t>(pc_ + 1 * kInstrSize);
  bool in_range = (ipc ^ static_cast<uint64_t>(target) >>
                  (kImm26Bits + kImmFieldShift)) == 0;
  DCHECK(in_range && ((target & 3) == 0));
#endif
  positions_recorder()->WriteRecordedPositions();
  GenInstrJump(JAL, target >> 2);
}


void Assembler::jalr(Register rs, Register rd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  positions_recorder()->WriteRecordedPositions();
  GenInstrRegister(SPECIAL, rs, zero_reg, rd, 0, JALR);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::j_or_jr(int64_t target, Register rs) {
  // Get pc of delay slot.
  uint64_t ipc = reinterpret_cast<uint64_t>(pc_ + 1 * kInstrSize);
  bool in_range = (ipc ^ static_cast<uint64_t>(target) >>
                  (kImm26Bits + kImmFieldShift)) == 0;
  if (in_range) {
      j(target);
  } else {
      jr(t9);
  }
}


void Assembler::jal_or_jalr(int64_t target, Register rs) {
  // Get pc of delay slot.
  uint64_t ipc = reinterpret_cast<uint64_t>(pc_ + 1 * kInstrSize);
  bool in_range = (ipc ^ static_cast<uint64_t>(target) >>
                  (kImm26Bits+kImmFieldShift)) == 0;
  if (in_range) {
      jal(target);
  } else {
      jalr(t9);
  }
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
  if (kArchVariant == kMips64r6) {
      GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH);
  } else {
      GenInstrRegister(SPECIAL2, rs, rt, rd, 0, MUL);
  }
}


void Assembler::muh(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH);
}


void Assembler::mulu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, MUL_MUH_U);
}


void Assembler::muhu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, MUL_MUH_U);
}


void Assembler::dmul(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, D_MUL_MUH);
}


void Assembler::dmuh(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, D_MUL_MUH);
}


void Assembler::dmulu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUL_OP, D_MUL_MUH_U);
}


void Assembler::dmuhu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MUH_OP, D_MUL_MUH_U);
}


void Assembler::mult(Register rs, Register rt) {
  DCHECK(kArchVariant != kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULT);
}


void Assembler::multu(Register rs, Register rt) {
  DCHECK(kArchVariant != kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, MULTU);
}


void Assembler::daddiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(DADDIU, rs, rd, j);
}


void Assembler::div(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIV);
}


void Assembler::div(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD);
}


void Assembler::mod(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD);
}


void Assembler::divu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIVU);
}


void Assembler::divu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, DIV_MOD_U);
}


void Assembler::modu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, DIV_MOD_U);
}


void Assembler::daddu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DADDU);
}


void Assembler::dsubu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSUBU);
}


void Assembler::dmult(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DMULT);
}


void Assembler::dmultu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DMULTU);
}


void Assembler::ddiv(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DDIV);
}


void Assembler::ddiv(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, D_DIV_MOD);
}


void Assembler::dmod(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, D_DIV_MOD);
}


void Assembler::ddivu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DDIVU);
}


void Assembler::ddivu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, DIV_OP, D_DIV_MOD_U);
}


void Assembler::dmodu(Register rd, Register rs, Register rt) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, MOD_OP, D_DIV_MOD_U);
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
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, SLL);
}


void Assembler::sllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SLLV);
}


void Assembler::srl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, SRL);
}


void Assembler::srlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRLV);
}


void Assembler::sra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, SRA);
}


void Assembler::srav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SRAV);
}


void Assembler::rotr(Register rd, Register rt, uint16_t sa) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  DCHECK(kArchVariant == kMips64r2);
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | SRL;
  emit(instr);
}


void Assembler::rotrv(Register rd, Register rt, Register rs) {
  // Should be called via MacroAssembler::Ror.
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid() );
  DCHECK(kArchVariant == kMips64r2);
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
     | (rd.code() << kRdShift) | (1 << kSaShift) | SRLV;
  emit(instr);
}


void Assembler::dsll(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, DSLL);
}


void Assembler::dsllv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSLLV);
}


void Assembler::dsrl(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, DSRL);
}


void Assembler::dsrlv(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSRLV);
}


void Assembler::drotr(Register rd, Register rt, uint16_t sa) {
  DCHECK(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | DSRL;
  emit(instr);
}


void Assembler::drotrv(Register rd, Register rt, Register rs) {
  DCHECK(rd.is_valid() && rt.is_valid() && rs.is_valid() );
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (1 << kSaShift) | DSRLV;
  emit(instr);
}


void Assembler::dsra(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, DSRA);
}


void Assembler::dsrav(Register rd, Register rt, Register rs) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, DSRAV);
}


void Assembler::dsll32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, DSLL32);
}


void Assembler::dsrl32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, DSRL32);
}


void Assembler::dsra32(Register rd, Register rt, uint16_t sa) {
  GenInstrRegister(SPECIAL, zero_reg, rt, rd, sa, DSRA32);
}


// ------------Memory-instructions-------------

// Helper for base-reg + offset, when offset is larger than int16.
void Assembler::LoadRegPlusOffsetToAt(const MemOperand& src) {
  DCHECK(!src.rm().is(at));
  DCHECK(is_int32(src.offset_));
  daddiu(at, zero_reg, (src.offset_ >> kLuiShift) & kImm16Mask);
  dsll(at, at, kLuiShift);
  ori(at, at, src.offset_ & kImm16Mask);  // Load 32-bit offset.
  daddu(at, at, src.rm());  // Add base register.
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


void Assembler::lwu(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LWU, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LWU, at, rd, 0);  // Equiv to lwu(rd, MemOperand(at, 0));
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


void Assembler::aui(Register rs, Register rt, int32_t j) {
  // This instruction uses same opcode as 'lui'. The difference in encoding is
  // 'lui' has zero reg. for rs field.
  DCHECK(is_uint16(j));
  GenInstrImmediate(LUI, rs, rt, j);
}


void Assembler::daui(Register rs, Register rt, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(DAUI, rs, rt, j);
}


void Assembler::dahi(Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(REGIMM, rs, DAHI, j);
}


void Assembler::dati(Register rs, int32_t j) {
  DCHECK(is_uint16(j));
  GenInstrImmediate(REGIMM, rs, DATI, j);
}


void Assembler::ldl(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LDL, rs.rm(), rd, rs.offset_);
}


void Assembler::ldr(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LDR, rs.rm(), rd, rs.offset_);
}


void Assembler::sdl(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SDL, rs.rm(), rd, rs.offset_);
}


void Assembler::sdr(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SDR, rs.rm(), rd, rs.offset_);
}


void Assembler::ld(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(LD, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to load.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(LD, at, rd, 0);  // Equiv to lw(rd, MemOperand(at, 0));
  }
}


void Assembler::sd(Register rd, const MemOperand& rs) {
  if (is_int16(rs.offset_)) {
    GenInstrImmediate(SD, rs.rm(), rd, rs.offset_);
  } else {  // Offset > 16 bits, use multiple instructions to store.
    LoadRegPlusOffsetToAt(rs);
    GenInstrImmediate(SD, at, rd, 0);  // Equiv to sw(rd, MemOperand(at, 0));
  }
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
#if defined(V8_HOST_ARCH_MIPS) || defined(V8_HOST_ARCH_MIPS64)
  break_(0x54321);
#else  // V8_HOST_ARCH_MIPS
  BlockTrampolinePoolFor(3);
  // The Simulator will handle the stop instruction and get the message address.
  // On MIPS stop() is just a special kind of break_().
  break_(code, true);
  emit(reinterpret_cast<uint64_t>(msg));
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
  rt.code_ = (cc & 0x0007) << 2 | 1;
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::movf(Register rd, Register rs, uint16_t cc) {
  Register rt;
  rt.code_ = (cc & 0x0007) << 2 | 0;
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, MOVCI);
}


void Assembler::sel(SecondaryField fmt, FPURegister fd,
    FPURegister ft, FPURegister fs, uint8_t sel) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(fmt == D);
  DCHECK(fmt == S);

  Instr instr = COP1 | fmt << kRsShift | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | SEL;
  emit(instr);
}


// GPR.
void Assembler::seleqz(Register rs, Register rt, Register rd) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELEQZ_S);
}


// FPR.
void Assembler::seleqz(SecondaryField fmt, FPURegister fd,
    FPURegister ft, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(fmt == D);
  DCHECK(fmt == S);

  Instr instr = COP1 | fmt << kRsShift | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | SELEQZ_C;
  emit(instr);
}


// GPR.
void Assembler::selnez(Register rs, Register rt, Register rd) {
  DCHECK(kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SELNEZ_S);
}


// FPR.
void Assembler::selnez(SecondaryField fmt, FPURegister fd,
    FPURegister ft, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK(fmt == D);
  DCHECK(fmt == S);

  Instr instr = COP1 | fmt << kRsShift | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | SELNEZ_C;
  emit(instr);
}


// Bit twiddling.
void Assembler::clz(Register rd, Register rs) {
  if (kArchVariant != kMips64r6) {
    // Clz instr requires same GPR number in 'rd' and 'rt' fields.
    GenInstrRegister(SPECIAL2, rs, rd, rd, 0, CLZ);
  } else {
    GenInstrRegister(SPECIAL, rs, zero_reg, rd, 1, CLZ_R6);
  }
}


void Assembler::ins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ins.
  // Ins instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK((kArchVariant == kMips64r2) || (kArchVariant == kMips64r6));
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, INS);
}


void Assembler::ext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ext.
  // Ext instr has 'rt' field as dest, and two uint5: msb, lsb.
  DCHECK(kArchVariant == kMips64r2 || kArchVariant == kMips64r6);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, EXT);
}


void Assembler::pref(int32_t hint, const MemOperand& rs) {
  DCHECK(is_uint5(hint) && is_uint16(rs.offset_));
  Instr instr = PREF | (rs.rm().code() << kRsShift) | (hint << kRtShift)
      | (rs.offset_);
  emit(instr);
}


// --------Coprocessor-instructions----------------

// Load, store, move.
void Assembler::lwc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(LWC1, src.rm(), fd, src.offset_);
}


void Assembler::ldc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(LDC1, src.rm(), fd, src.offset_);
}


void Assembler::swc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(SWC1, src.rm(), fd, src.offset_);
}


void Assembler::sdc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(SDC1, src.rm(), fd, src.offset_);
}


void Assembler::mtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTC1, rt, fs, f0);
}


void Assembler::mthc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTHC1, rt, fs, f0);
}


void Assembler::dmtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, DMTC1, rt, fs, f0);
}


void Assembler::mfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFC1, rt, fs, f0);
}


void Assembler::mfhc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFHC1, rt, fs, f0);
}


void Assembler::dmfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, DMFC1, rt, fs, f0);
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


// Arithmetic.

void Assembler::add_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, ADD_D);
}


void Assembler::sub_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, SUB_D);
}


void Assembler::mul_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, MUL_D);
}


void Assembler::madd_d(FPURegister fd, FPURegister fr, FPURegister fs,
    FPURegister ft) {
  GenInstrRegister(COP1X, fr, ft, fs, fd, MADD_D);
}


void Assembler::div_d(FPURegister fd, FPURegister fs, FPURegister ft) {
  GenInstrRegister(COP1, D, ft, fs, fd, DIV_D);
}


void Assembler::abs_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ABS_D);
}


void Assembler::mov_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, MOV_D);
}


void Assembler::neg_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, NEG_D);
}


void Assembler::sqrt_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, SQRT_D);
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


void Assembler::cvt_l_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_L_S);
}


void Assembler::cvt_l_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_L_D);
}


void Assembler::trunc_l_s(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_L_S);
}


void Assembler::trunc_l_d(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, D, f0, fs, fd, TRUNC_L_D);
}


void Assembler::round_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, ROUND_L_S);
}


void Assembler::round_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, ROUND_L_D);
}


void Assembler::floor_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, FLOOR_L_S);
}


void Assembler::floor_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, FLOOR_L_D);
}


void Assembler::ceil_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CEIL_L_S);
}


void Assembler::ceil_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CEIL_L_D);
}


void Assembler::min(SecondaryField fmt, FPURegister fd, FPURegister ft,
    FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MIN);
}


void Assembler::mina(SecondaryField fmt, FPURegister fd, FPURegister ft,
    FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MINA);
}


void Assembler::max(SecondaryField fmt, FPURegister fd, FPURegister ft,
    FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAX);
}


void Assembler::maxa(SecondaryField fmt, FPURegister fd, FPURegister ft,
    FPURegister fs) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt == D) || (fmt == S));
  GenInstrRegister(COP1, fmt, ft, fs, fd, MAXA);
}


void Assembler::cvt_s_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_S_W);
}


void Assembler::cvt_s_l(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_S_L);
}


void Assembler::cvt_s_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_S_D);
}


void Assembler::cvt_d_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_D_W);
}


void Assembler::cvt_d_l(FPURegister fd, FPURegister fs) {
  DCHECK(kArchVariant == kMips64r2);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_D_L);
}


void Assembler::cvt_d_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_D_S);
}


// Conditions for >= MIPSr6.
void Assembler::cmp(FPUCondition cond, SecondaryField fmt,
    FPURegister fd, FPURegister fs, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  DCHECK((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift |
      fs.code() << kFsShift | fd.code() << kFdShift | (0 << 5) | cond;
  emit(instr);
}


void Assembler::bc1eqz(int16_t offset, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  Instr instr = COP1 | BC1EQZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1nez(int16_t offset, FPURegister ft) {
  DCHECK(kArchVariant == kMips64r6);
  Instr instr = COP1 | BC1NEZ | ft.code() << kFtShift | (offset & kImm16Mask);
  emit(instr);
}


// Conditions for < MIPSr6.
void Assembler::c(FPUCondition cond, SecondaryField fmt,
    FPURegister fs, FPURegister ft, uint16_t cc) {
  DCHECK(kArchVariant != kMips64r6);
  DCHECK(is_uint3(cc));
  DCHECK((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << kFtShift | fs.code() << kFsShift
      | cc << 8 | 3 << 4 | cond;
  emit(instr);
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


// Debugging.
void Assembler::RecordJSReturn() {
  positions_recorder()->WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::JS_RETURN);
}


void Assembler::RecordDebugBreakSlot() {
  positions_recorder()->WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::DEBUG_BREAK_SLOT);
}


void Assembler::RecordComment(const char* msg) {
  if (FLAG_code_comments) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::COMMENT, reinterpret_cast<intptr_t>(msg));
  }
}


int Assembler::RelocateInternalReference(byte* pc, intptr_t pc_delta) {
  Instr instr = instr_at(pc);
  DCHECK(IsJ(instr) || IsLui(instr));
  if (IsLui(instr)) {
    Instr instr_lui = instr_at(pc + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pc + 1 * Assembler::kInstrSize);
    Instr instr_ori2 = instr_at(pc + 3 * Assembler::kInstrSize);
    DCHECK(IsOri(instr_ori));
    DCHECK(IsOri(instr_ori2));
    // TODO(plind): symbolic names for the shifts.
    int64_t imm = (instr_lui & static_cast<int64_t>(kImm16Mask)) << 48;
    imm |= (instr_ori & static_cast<int64_t>(kImm16Mask)) << 32;
    imm |= (instr_ori2 & static_cast<int64_t>(kImm16Mask)) << 16;
    // Sign extend address.
    imm >>= 16;

    if (imm == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    imm += pc_delta;
    DCHECK((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;
    instr_ori2 &= ~kImm16Mask;

    instr_at_put(pc + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> 32) & kImm16Mask));
    instr_at_put(pc + 1 * Assembler::kInstrSize,
                 instr_ori | (imm >> 16 & kImm16Mask));
    instr_at_put(pc + 3 * Assembler::kInstrSize,
                 instr_ori2 | (imm & kImm16Mask));
    return 4;  // Number of instructions patched.
  } else {
    uint32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    if (static_cast<int32_t>(imm28) == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }

    imm28 += pc_delta;
    imm28 &= kImm28Mask;
    DCHECK((imm28 & 3) == 0);

    instr &= ~kImm26Mask;
    uint32_t imm26 = imm28 >> 2;
    DCHECK(is_uint26(imm26));

    instr_at_put(pc, instr | (imm26 & kImm26Mask));
    return 1;  // Number of instructions patched.
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

  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();

  // Copy the data.
  intptr_t pc_delta = desc.buffer - buffer_;
  intptr_t rc_delta = (desc.buffer + desc.buffer_size) -
      (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(reloc_info_writer.pos() + rc_delta,
              reloc_info_writer.pos(), desc.reloc_size);

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
    if (rmode == RelocInfo::INTERNAL_REFERENCE) {
      byte* p = reinterpret_cast<byte*>(it.rinfo()->pc());
      RelocateInternalReference(p, pc_delta);
    }
  }

  DCHECK(!overflow());
}


void Assembler::db(uint8_t data) {
  CheckBuffer();
  *reinterpret_cast<uint8_t*>(pc_) = data;
  pc_ += sizeof(uint8_t);
}


void Assembler::dd(uint32_t data) {
  CheckBuffer();
  *reinterpret_cast<uint32_t*>(pc_) = data;
  pc_ += sizeof(uint32_t);
}


void Assembler::emit_code_stub_address(Code* stub) {
  CheckBuffer();
  *reinterpret_cast<uint64_t*>(pc_) =
      reinterpret_cast<uint64_t>(stub->instruction_start());
  pc_ += sizeof(uint64_t);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(pc_, rmode, data, NULL);
  if (rmode >= RelocInfo::JS_RETURN && rmode <= RelocInfo::DEBUG_BREAK_SLOT) {
    // Adjust code for new modes.
    DCHECK(RelocInfo::IsDebugBreakSlot(rmode)
           || RelocInfo::IsJSReturn(rmode)
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
      RelocInfo reloc_info_with_ast_id(pc_,
                                       rmode,
                                       RecordedAstId().ToInt(),
                                       NULL);
      ClearRecordedAstId();
      reloc_info_writer.Write(&reloc_info_with_ast_id);
    } else {
      reloc_info_writer.Write(&rinfo);
    }
  }
}


void Assembler::BlockTrampolinePoolFor(int instructions) {
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
      b(&after_pool);
      nop();

      int pool_start = pc_offset();
      for (int i = 0; i < unbound_labels_count_; i++) {
        uint64_t imm64;
        imm64 = jump_address(&after_pool);
        { BlockGrowBufferScope block_buf_growth(this);
          // Buffer growth (and relocation) must be blocked for internal
          // references until associated instructions are emitted and available
          // to be patched.
          RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
          // TODO(plind): Verify this, presume I cannot use macro-assembler
          // here.
          lui(at, (imm64 >> 32) & kImm16Mask);
          ori(at, at, (imm64 >> 16) & kImm16Mask);
          dsll(at, at, 16);
          ori(at, at, imm64 & kImm16Mask);
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
  Instr instr0 = instr_at(pc);
  Instr instr1 = instr_at(pc + 1 * kInstrSize);
  Instr instr3 = instr_at(pc + 3 * kInstrSize);

  // Interpret 4 instructions for address generated by li: See listing in
  // Assembler::set_target_address_at() just below.
  if ((GetOpcodeField(instr0) == LUI) && (GetOpcodeField(instr1) == ORI) &&
      (GetOpcodeField(instr3) == ORI)) {
    // Assemble the 48 bit value.
     int64_t addr  = static_cast<int64_t>(
          ((uint64_t)(GetImmediate16(instr0)) << 32) |
          ((uint64_t)(GetImmediate16(instr1)) << 16) |
          ((uint64_t)(GetImmediate16(instr3))));

    // Sign extend to get canonical address.
    addr = (addr << 16) >> 16;
    return reinterpret_cast<Address>(addr);
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
  HeapNumber::cast(object)->set_value(base::OS::nan_value());
}


// On Mips64, a target address is stored in a 4-instruction sequence:
//    0: lui(rd, (j.imm64_ >> 32) & kImm16Mask);
//    1: ori(rd, rd, (j.imm64_ >> 16) & kImm16Mask);
//    2: dsll(rd, rd, 16);
//    3: ori(rd, rd, j.imm32_ & kImm16Mask);
//
// Patching the address must replace all the lui & ori instructions,
// and flush the i-cache.
//
// There is an optimization below, which emits a nop when the address
// fits in just 16 bits. This is unlikely to help, and should be benchmarked,
// and possibly removed.
void Assembler::set_target_address_at(Address pc,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
// There is an optimization where only 4 instructions are used to load address
// in code on MIP64 because only 48-bits of address is effectively used.
// It relies on fact the upper [63:48] bits are not used for virtual address
// translation and they have to be set according to value of bit 47 in order
// get canonical address.
  Instr instr1 = instr_at(pc + kInstrSize);
  uint32_t rt_code = GetRt(instr1);
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
  uint64_t itarget = reinterpret_cast<uint64_t>(target);

#ifdef DEBUG
  // Check we have the result from a li macro-instruction.
  Instr instr0 = instr_at(pc);
  Instr instr3 = instr_at(pc + kInstrSize * 3);
  CHECK((GetOpcodeField(instr0) == LUI && GetOpcodeField(instr1) == ORI &&
         GetOpcodeField(instr3) == ORI));
#endif

  // Must use 4 instructions to insure patchable code.
  // lui rt, upper-16.
  // ori rt, rt, lower-16.
  // dsll rt, rt, 16.
  // ori rt rt, lower-16.
  *p = LUI | (rt_code << kRtShift) | ((itarget >> 32) & kImm16Mask);
  *(p + 1) = ORI | (rt_code << kRtShift) | (rt_code << kRsShift)
      | ((itarget >> 16) & kImm16Mask);
  *(p + 3) = ORI | (rt_code << kRsShift) | (rt_code << kRtShift)
      | (itarget & kImm16Mask);

  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    CpuFeatures::FlushICache(pc, 4 * Assembler::kInstrSize);
  }
}


void Assembler::JumpLabelToJumpRegister(Address pc) {
  // Address pc points to lui/ori instructions.
  // Jump to label may follow at pc + 2 * kInstrSize.
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
#ifdef DEBUG
  Instr instr1 = instr_at(pc);
#endif
  Instr instr2 = instr_at(pc + 1 * kInstrSize);
  Instr instr3 = instr_at(pc + 6 * kInstrSize);
  bool patched = false;

  if (IsJal(instr3)) {
    DCHECK(GetOpcodeField(instr1) == LUI);
    DCHECK(GetOpcodeField(instr2) == ORI);

    uint32_t rs_field = GetRt(instr2) << kRsShift;
    uint32_t rd_field = ra.code() << kRdShift;  // Return-address (ra) reg.
    *(p+6) = SPECIAL | rs_field | rd_field | JALR;
    patched = true;
  } else if (IsJ(instr3)) {
    DCHECK(GetOpcodeField(instr1) == LUI);
    DCHECK(GetOpcodeField(instr2) == ORI);

    uint32_t rs_field = GetRt(instr2) << kRsShift;
    *(p+6) = SPECIAL | rs_field | JR;
    patched = true;
  }

  if (patched) {
      CpuFeatures::FlushICache(pc+6, sizeof(int32_t));
  }
}


Handle<ConstantPoolArray> Assembler::NewConstantPool(Isolate* isolate) {
  // No out-of-line constant pool support.
  DCHECK(!FLAG_enable_ool_constant_pool);
  return isolate->factory()->empty_constant_pool_array();
}


void Assembler::PopulateConstantPool(ConstantPoolArray* constant_pool) {
  // No out-of-line constant pool support.
  DCHECK(!FLAG_enable_ool_constant_pool);
  return;
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS64
