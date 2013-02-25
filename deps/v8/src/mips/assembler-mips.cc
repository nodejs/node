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


#include "v8.h"

#if defined(V8_TARGET_ARCH_MIPS)

#include "mips/assembler-mips-inl.h"
#include "serialize.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool CpuFeatures::initialized_ = false;
#endif
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::found_by_runtime_probing_ = 0;


// Get the CPU features enabled by the build. For cross compilation the
// preprocessor symbols CAN_USE_FPU_INSTRUCTIONS
// can be defined to enable FPU instructions when building the
// snapshot.
static uint64_t CpuFeaturesImpliedByCompiler() {
  uint64_t answer = 0;
#ifdef CAN_USE_FPU_INSTRUCTIONS
  answer |= 1u << FPU;
#endif  // def CAN_USE_FPU_INSTRUCTIONS

#ifdef __mips__
  // If the compiler is allowed to use FPU then we can use FPU too in our code
  // generation even when generating snapshots.  This won't work for cross
  // compilation.
#if(defined(__mips_hard_float) && __mips_hard_float != 0)
  answer |= 1u << FPU;
#endif  // defined(__mips_hard_float) && __mips_hard_float != 0
#endif  // def __mips__

  return answer;
}


void CpuFeatures::Probe() {
  unsigned standard_features = (OS::CpuFeaturesImpliedByPlatform() |
                                CpuFeaturesImpliedByCompiler());
  ASSERT(supported_ == 0 || supported_ == standard_features);
#ifdef DEBUG
  initialized_ = true;
#endif

  // Get the features implied by the OS and the compiler settings. This is the
  // minimal set of features which is also allowed for generated code in the
  // snapshot.
  supported_ |= standard_features;

  if (Serializer::enabled()) {
    // No probing for features if we might serialize (generate snapshot).
    return;
  }

  // If the compiler is allowed to use fpu then we can use fpu too in our
  // code generation.
#if !defined(__mips__)
  // For the simulator=mips build, use FPU when FLAG_enable_fpu is enabled.
  if (FLAG_enable_fpu) {
      supported_ |= 1u << FPU;
  }
#else
  // Probe for additional features not already known to be available.
  if (OS::MipsCpuHasFeature(FPU)) {
    // This implementation also sets the FPU flags if
    // runtime detection of FPU returns true.
    supported_ |= 1u << FPU;
    found_by_runtime_probing_ |= 1u << FPU;
  }
#endif
}


int ToNumber(Register reg) {
  ASSERT(reg.is_valid());
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
  ASSERT(num >= 0 && num < kNumRegisters);
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
                                  1 << RelocInfo::INTERNAL_REFERENCE;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on MIPS means that it is a lui/ori instruction, and that is
  // always the case inside code objects.
  return true;
}


// Patch the code at the current address with the supplied instructions.
void RelocInfo::PatchCode(byte* instructions, int instruction_count) {
  Instr* pc = reinterpret_cast<Instr*>(pc_);
  Instr* instr = reinterpret_cast<Instr*>(instructions);
  for (int i = 0; i < instruction_count; i++) {
    *(pc + i) = *(instr + i);
  }

  // Indicate that code has changed.
  CPU::FlushICache(pc_, instruction_count * Assembler::kInstrSize);
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
  rm_ = no_reg;
  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  ASSERT(!HEAP->InNewSpace(obj));
  if (obj->IsHeapObject()) {
    imm32_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // No relocation needed.
    imm32_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE;
  }
}


MemOperand::MemOperand(Register rm, int32_t offset) : Operand(rm) {
  offset_ = offset;
}


// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.

static const int kNegOffset = 0x00008000;
// addiu(sp, sp, 4) aka Pop() operation or part of Pop(r)
// operations as post-increment of sp.
const Instr kPopInstruction = ADDIU | (kRegister_sp_Code << kRsShift)
      | (kRegister_sp_Code << kRtShift) | (kPointerSize & kImm16Mask);
// addiu(sp, sp, -4) part of Push(r) operation as pre-decrement of sp.
const Instr kPushInstruction = ADDIU | (kRegister_sp_Code << kRsShift)
      | (kRegister_sp_Code << kRtShift) | (-kPointerSize & kImm16Mask);
// sw(r, MemOperand(sp, 0))
const Instr kPushRegPattern = SW | (kRegister_sp_Code << kRsShift)
      |  (0 & kImm16Mask);
//  lw(r, MemOperand(sp, 0))
const Instr kPopRegPattern = LW | (kRegister_sp_Code << kRsShift)
      |  (0 & kImm16Mask);

const Instr kLwRegFpOffsetPattern = LW | (kRegister_fp_Code << kRsShift)
      |  (0 & kImm16Mask);

const Instr kSwRegFpOffsetPattern = SW | (kRegister_fp_Code << kRsShift)
      |  (0 & kImm16Mask);

const Instr kLwRegFpNegOffsetPattern = LW | (kRegister_fp_Code << kRsShift)
      |  (kNegOffset & kImm16Mask);

const Instr kSwRegFpNegOffsetPattern = SW | (kRegister_fp_Code << kRsShift)
      |  (kNegOffset & kImm16Mask);
// A mask for the Rt register for push, pop, lw, sw instructions.
const Instr kRtMask = kRtFieldMask;
const Instr kLwSwInstrTypeMask = 0xffe00000;
const Instr kLwSwInstrArgumentMask  = ~kLwSwInstrTypeMask;
const Instr kLwSwOffsetMask = kImm16Mask;


// Spare buffer.
static const int kMinimalBufferSize = 4 * KB;


Assembler::Assembler(Isolate* arg_isolate, void* buffer, int buffer_size)
    : AssemblerBase(arg_isolate),
      recorded_ast_id_(TypeFeedbackId::None()),
      positions_recorder_(this),
      emit_debug_code_(FLAG_debug_code) {
  if (buffer == NULL) {
    // Do our own buffer management.
    if (buffer_size <= kMinimalBufferSize) {
      buffer_size = kMinimalBufferSize;

      if (isolate()->assembler_spare_buffer() != NULL) {
        buffer = isolate()->assembler_spare_buffer();
        isolate()->set_assembler_spare_buffer(NULL);
      }
    }
    if (buffer == NULL) {
      buffer_ = NewArray<byte>(buffer_size);
    } else {
      buffer_ = static_cast<byte*>(buffer);
    }
    buffer_size_ = buffer_size;
    own_buffer_ = true;

  } else {
    // Use externally provided buffer instead.
    ASSERT(buffer_size > 0);
    buffer_ = static_cast<byte*>(buffer);
    buffer_size_ = buffer_size;
    own_buffer_ = false;
  }

  // Set up buffer pointers.
  ASSERT(buffer_ != NULL);
  pc_ = buffer_;
  reloc_info_writer.Reposition(buffer_ + buffer_size, pc_);

  last_trampoline_pool_end_ = 0;
  no_trampoline_pool_before_ = 0;
  trampoline_pool_blocked_nesting_ = 0;
  // We leave space (16 * kTrampolineSlotsSize)
  // for BlockTrampolinePoolScope buffer.
  next_buffer_check_ = kMaxBranchOffset - kTrampolineSlotsSize * 16;
  internal_trampoline_exception_ = false;
  last_bound_pos_ = 0;

  trampoline_emitted_ = false;
  unbound_labels_count_ = 0;
  block_buffer_growth_ = false;

  ClearRecordedAstId();
}


Assembler::~Assembler() {
  if (own_buffer_) {
    if (isolate()->assembler_spare_buffer() == NULL &&
        buffer_size_ == kMinimalBufferSize) {
      isolate()->set_assembler_spare_buffer(buffer_);
    } else {
      DeleteArray(buffer_);
    }
  }
}


void Assembler::GetCode(CodeDesc* desc) {
  ASSERT(pc_ <= reloc_info_writer.pos());  // No overlap.
  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
}


void Assembler::Align(int m) {
  ASSERT(m >= 4 && IsPowerOf2(m));
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
  uint32_t label_constant = GetLabelConst(instr);
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
      label_constant == 0;  // Emitted label const in reg-exp engine.
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
  ASSERT(type < 32);
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
  ASSERT(IsBranch(instr));
  return ((int16_t)(instr & kImm16Mask)) << 2;
}


bool Assembler::IsLw(Instr instr) {
  return ((instr & kOpcodeMask) == LW);
}


int16_t Assembler::GetLwOffset(Instr instr) {
  ASSERT(IsLw(instr));
  return ((instr & kImm16Mask));
}


Instr Assembler::SetLwOffset(Instr instr, int16_t offset) {
  ASSERT(IsLw(instr));

  // We actually create a new lw instruction based on the original one.
  Instr temp_instr = LW | (instr & kRsFieldMask) | (instr & kRtFieldMask)
      | (offset & kImm16Mask);

  return temp_instr;
}


bool Assembler::IsSw(Instr instr) {
  return ((instr & kOpcodeMask) == SW);
}


Instr Assembler::SetSwOffset(Instr instr, int16_t offset) {
  ASSERT(IsSw(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAddImmediate(Instr instr) {
  return ((instr & kOpcodeMask) == ADDIU);
}


Instr Assembler::SetAddImmediateOffset(Instr instr, int16_t offset) {
  ASSERT(IsAddImmediate(instr));
  return ((instr & ~kImm16Mask) | (offset & kImm16Mask));
}


bool Assembler::IsAndImmediate(Instr instr) {
  return GetOpcodeField(instr) == ANDI;
}


int Assembler::target_at(int32_t pos) {
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
  ASSERT(IsBranch(instr) || IsJ(instr) || IsLui(instr));
  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmectic shifts for signed integers.
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
    ASSERT(IsOri(instr_ori));
    int32_t imm = (instr_lui & static_cast<int32_t>(kImm16Mask)) << kLuiShift;
    imm |= (instr_ori & static_cast<int32_t>(kImm16Mask));

    if (imm == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint32_t instr_address = reinterpret_cast<int32_t>(buffer_ + pos);
      int32_t delta = instr_address - imm;
      ASSERT(pos > delta);
      return pos - delta;
    }
  } else {
    int32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    if (imm28 == kEndOfJumpChain) {
      // EndOfChain sentinel is returned directly, not relative to pc or pos.
      return kEndOfChain;
    } else {
      uint32_t instr_address = reinterpret_cast<int32_t>(buffer_ + pos);
      instr_address &= kImm28Mask;
      int32_t delta = instr_address - imm28;
      ASSERT(pos > delta);
      return pos - delta;
    }
  }
}


void Assembler::target_at_put(int32_t pos, int32_t target_pos) {
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    ASSERT(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    return;
  }

  ASSERT(IsBranch(instr) || IsJ(instr) || IsLui(instr));
  if (IsBranch(instr)) {
    int32_t imm18 = target_pos - (pos + kBranchPCOffset);
    ASSERT((imm18 & 3) == 0);

    instr &= ~kImm16Mask;
    int32_t imm16 = imm18 >> 2;
    ASSERT(is_int16(imm16));

    instr_at_put(pos, instr | (imm16 & kImm16Mask));
  } else if (IsLui(instr)) {
    Instr instr_lui = instr_at(pos + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pos + 1 * Assembler::kInstrSize);
    ASSERT(IsOri(instr_ori));
    uint32_t imm = (uint32_t)buffer_ + target_pos;
    ASSERT((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;

    instr_at_put(pos + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm & kHiMask) >> kLuiShift));
    instr_at_put(pos + 1 * Assembler::kInstrSize,
                 instr_ori | (imm & kImm16Mask));
  } else {
    uint32_t imm28 = (uint32_t)buffer_ + target_pos;
    imm28 &= kImm28Mask;
    ASSERT((imm28 & 3) == 0);

    instr &= ~kImm26Mask;
    uint32_t imm26 = imm28 >> 2;
    ASSERT(is_uint26(imm26));

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
  ASSERT(0 <= pos && pos <= pc_offset());  // Must have valid binding position.
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
        ASSERT((trampoline_pos - fixup_pos) <= kMaxBranchOffset);
        target_at_put(fixup_pos, trampoline_pos);
        fixup_pos = trampoline_pos;
        dist = pos - fixup_pos;
      }
      target_at_put(fixup_pos, pos);
    } else {
      ASSERT(IsJ(instr) || IsLui(instr));
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
  ASSERT(!L->is_bound());  // Label can only be bound once.
  bind_to(L, pc_offset());
}


void Assembler::next(Label* L) {
  ASSERT(L->is_linked());
  int link = target_at(L->pos());
  if (link == kEndOfChain) {
    L->Unuse();
  } else {
    ASSERT(link >= 0);
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
  return rmode != RelocInfo::NONE;
}

void Assembler::GenInstrRegister(Opcode opcode,
                                 Register rs,
                                 Register rt,
                                 Register rd,
                                 uint16_t sa,
                                 SecondaryField func) {
  ASSERT(rd.is_valid() && rs.is_valid() && rt.is_valid() && is_uint5(sa));
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
  ASSERT(rs.is_valid() && rt.is_valid() && is_uint5(msb) && is_uint5(lsb));
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
  ASSERT(fd.is_valid() && fs.is_valid() && ft.is_valid());
  ASSERT(CpuFeatures::IsEnabled(FPU));
  Instr instr = opcode | fmt | (ft.code() << kFtShift) | (fs.code() << kFsShift)
      | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  ASSERT(fd.is_valid() && fs.is_valid() && rt.is_valid());
  ASSERT(CpuFeatures::IsEnabled(FPU));
  Instr instr = opcode | fmt | (rt.code() << kRtShift)
      | (fs.code() << kFsShift) | (fd.code() << kFdShift) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPUControlRegister fs,
                                 SecondaryField func) {
  ASSERT(fs.is_valid() && rt.is_valid());
  ASSERT(CpuFeatures::IsEnabled(FPU));
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
  ASSERT(rs.is_valid() && rt.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | (rt.code() << kRtShift)
      | (j & kImm16Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode,
                                  Register rs,
                                  SecondaryField SF,
                                  int32_t j) {
  ASSERT(rs.is_valid() && (is_int16(j) || is_uint16(j)));
  Instr instr = opcode | (rs.code() << kRsShift) | SF | (j & kImm16Mask);
  emit(instr);
}


void Assembler::GenInstrImmediate(Opcode opcode,
                                  Register rs,
                                  FPURegister ft,
                                  int32_t j) {
  ASSERT(rs.is_valid() && ft.is_valid() && (is_int16(j) || is_uint16(j)));
  ASSERT(CpuFeatures::IsEnabled(FPU));
  Instr instr = opcode | (rs.code() << kRsShift) | (ft.code() << kFtShift)
      | (j & kImm16Mask);
  emit(instr);
}


void Assembler::GenInstrJump(Opcode opcode,
                             uint32_t address) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  ASSERT(is_uint26(address));
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

  uint32_t imm = (uint32_t)buffer_ + target_pos;
  ASSERT((imm & 3) == 0);

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
  ASSERT((offset & 3) == 0);
  ASSERT(is_int16(offset >> 2));

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
      ASSERT((imm18 & 3) == 0);
      int32_t imm16 = imm18 >> 2;
      ASSERT(is_int16(imm16));
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


void Assembler::bgezal(Register rs, int16_t offset) {
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


void Assembler::blez(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(BLEZ, rs, zero_reg, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltz(Register rs, int16_t offset) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  GenInstrImmediate(REGIMM, rs, BLTZ, offset);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::bltzal(Register rs, int16_t offset) {
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


void Assembler::j(int32_t target) {
#if DEBUG
  // Get pc of delay slot.
  uint32_t ipc = reinterpret_cast<uint32_t>(pc_ + 1 * kInstrSize);
  bool in_range = ((uint32_t)(ipc^target) >> (kImm26Bits+kImmFieldShift)) == 0;
  ASSERT(in_range && ((target & 3) == 0));
#endif
  GenInstrJump(J, target >> 2);
}


void Assembler::jr(Register rs) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (rs.is(ra)) {
    positions_recorder()->WriteRecordedPositions();
  }
  GenInstrRegister(SPECIAL, rs, zero_reg, zero_reg, 0, JR);
  BlockTrampolinePoolFor(1);  // For associated delay slot.
}


void Assembler::jal(int32_t target) {
#ifdef DEBUG
  // Get pc of delay slot.
  uint32_t ipc = reinterpret_cast<uint32_t>(pc_ + 1 * kInstrSize);
  bool in_range = ((uint32_t)(ipc^target) >> (kImm26Bits+kImmFieldShift)) == 0;
  ASSERT(in_range && ((target & 3) == 0));
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


void Assembler::j_or_jr(int32_t target, Register rs) {
  // Get pc of delay slot.
  uint32_t ipc = reinterpret_cast<uint32_t>(pc_ + 1 * kInstrSize);
  bool in_range = ((uint32_t)(ipc^target) >> (kImm26Bits+kImmFieldShift)) == 0;

  if (in_range) {
      j(target);
  } else {
      jr(t9);
  }
}


void Assembler::jal_or_jalr(int32_t target, Register rs) {
  // Get pc of delay slot.
  uint32_t ipc = reinterpret_cast<uint32_t>(pc_ + 1 * kInstrSize);
  bool in_range = ((uint32_t)(ipc^target) >> (kImm26Bits+kImmFieldShift)) == 0;

  if (in_range) {
      jal(target);
  } else {
      jalr(t9);
  }
}


//-------Data-processing-instructions---------

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
  GenInstrRegister(SPECIAL2, rs, rt, rd, 0, MUL);
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


void Assembler::divu(Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, zero_reg, 0, DIVU);
}


// Logical.

void Assembler::and_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, AND);
}


void Assembler::andi(Register rt, Register rs, int32_t j) {
  ASSERT(is_uint16(j));
  GenInstrImmediate(ANDI, rs, rt, j);
}


void Assembler::or_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, OR);
}


void Assembler::ori(Register rt, Register rs, int32_t j) {
  ASSERT(is_uint16(j));
  GenInstrImmediate(ORI, rs, rt, j);
}


void Assembler::xor_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, XOR);
}


void Assembler::xori(Register rt, Register rs, int32_t j) {
  ASSERT(is_uint16(j));
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
  ASSERT(coming_from_nop || !(rd.is(zero_reg) && rt.is(zero_reg)));
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
  ASSERT(rd.is_valid() && rt.is_valid() && is_uint5(sa));
  ASSERT(kArchVariant == kMips32r2);
  Instr instr = SPECIAL | (1 << kRsShift) | (rt.code() << kRtShift)
      | (rd.code() << kRdShift) | (sa << kSaShift) | SRL;
  emit(instr);
}


void Assembler::rotrv(Register rd, Register rt, Register rs) {
  // Should be called via MacroAssembler::Ror.
  ASSERT(rd.is_valid() && rt.is_valid() && rs.is_valid() );
  ASSERT(kArchVariant == kMips32r2);
  Instr instr = SPECIAL | (rs.code() << kRsShift) | (rt.code() << kRtShift)
     | (rd.code() << kRdShift) | (1 << kSaShift) | SRLV;
  emit(instr);
}


//------------Memory-instructions-------------

// Helper for base-reg + offset, when offset is larger than int16.
void Assembler::LoadRegPlusOffsetToAt(const MemOperand& src) {
  ASSERT(!src.rm().is(at));
  lui(at, src.offset_ >> kLuiShift);
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
  ASSERT(is_uint16(j));
  GenInstrImmediate(LUI, zero_reg, rd, j);
}


//-------------Misc-instructions--------------

// Break / Trap instructions.
void Assembler::break_(uint32_t code, bool break_as_stop) {
  ASSERT((code & ~0xfffff) == 0);
  // We need to invalidate breaks that could be stops as well because the
  // simulator expects a char pointer after the stop instruction.
  // See constants-mips.h for explanation.
  ASSERT((break_as_stop &&
          code <= kMaxStopCode &&
          code > kMaxWatchpointCode) ||
         (!break_as_stop &&
          (code > kMaxStopCode ||
           code <= kMaxWatchpointCode)));
  Instr break_instr = SPECIAL | BREAK | (code << 6);
  emit(break_instr);
}


void Assembler::stop(const char* msg, uint32_t code) {
  ASSERT(code > kMaxWatchpointCode);
  ASSERT(code <= kMaxStopCode);
#if defined(V8_HOST_ARCH_MIPS)
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
  ASSERT(is_uint10(code));
  Instr instr = SPECIAL | TGE | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tgeu(Register rs, Register rt, uint16_t code) {
  ASSERT(is_uint10(code));
  Instr instr = SPECIAL | TGEU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tlt(Register rs, Register rt, uint16_t code) {
  ASSERT(is_uint10(code));
  Instr instr =
      SPECIAL | TLT | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tltu(Register rs, Register rt, uint16_t code) {
  ASSERT(is_uint10(code));
  Instr instr =
      SPECIAL | TLTU | rs.code() << kRsShift
      | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::teq(Register rs, Register rt, uint16_t code) {
  ASSERT(is_uint10(code));
  Instr instr =
      SPECIAL | TEQ | rs.code() << kRsShift | rt.code() << kRtShift | code << 6;
  emit(instr);
}


void Assembler::tne(Register rs, Register rt, uint16_t code) {
  ASSERT(is_uint10(code));
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


// Bit twiddling.
void Assembler::clz(Register rd, Register rs) {
  // Clz instr requires same GPR number in 'rd' and 'rt' fields.
  GenInstrRegister(SPECIAL2, rs, rd, rd, 0, CLZ);
}


void Assembler::ins_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ins.
  // Ins instr has 'rt' field as dest, and two uint5: msb, lsb.
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(SPECIAL3, rs, rt, pos + size - 1, pos, INS);
}


void Assembler::ext_(Register rt, Register rs, uint16_t pos, uint16_t size) {
  // Should be called via MacroAssembler::Ext.
  // Ext instr has 'rt' field as dest, and two uint5: msb, lsb.
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(SPECIAL3, rs, rt, size - 1, pos, EXT);
}


//--------Coprocessor-instructions----------------

// Load, store, move.
void Assembler::lwc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(LWC1, src.rm(), fd, src.offset_);
}


void Assembler::ldc1(FPURegister fd, const MemOperand& src) {
  // Workaround for non-8-byte alignment of HeapNumber, convert 64-bit
  // load to two 32-bit loads.
  GenInstrImmediate(LWC1, src.rm(), fd, src.offset_);
  FPURegister nextfpreg;
  nextfpreg.setcode(fd.code() + 1);
  GenInstrImmediate(LWC1, src.rm(), nextfpreg, src.offset_ + 4);
}


void Assembler::swc1(FPURegister fd, const MemOperand& src) {
  GenInstrImmediate(SWC1, src.rm(), fd, src.offset_);
}


void Assembler::sdc1(FPURegister fd, const MemOperand& src) {
  // Workaround for non-8-byte alignment of HeapNumber, convert 64-bit
  // store to two 32-bit stores.
  GenInstrImmediate(SWC1, src.rm(), fd, src.offset_);
  FPURegister nextfpreg;
  nextfpreg.setcode(fd.code() + 1);
  GenInstrImmediate(SWC1, src.rm(), nextfpreg, src.offset_ + 4);
}


void Assembler::mtc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MTC1, rt, fs, f0);
}


void Assembler::mfc1(Register rt, FPURegister fs) {
  GenInstrRegister(COP1, MFC1, rt, fs, f0);
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
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_L_S);
}


void Assembler::cvt_l_d(FPURegister fd, FPURegister fs) {
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_L_D);
}


void Assembler::trunc_l_s(FPURegister fd, FPURegister fs) {
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(COP1, S, f0, fs, fd, TRUNC_L_S);
}


void Assembler::trunc_l_d(FPURegister fd, FPURegister fs) {
  ASSERT(kArchVariant == kMips32r2);
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


void Assembler::cvt_s_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_S_W);
}


void Assembler::cvt_s_l(FPURegister fd, FPURegister fs) {
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_S_L);
}


void Assembler::cvt_s_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_S_D);
}


void Assembler::cvt_d_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_D_W);
}


void Assembler::cvt_d_l(FPURegister fd, FPURegister fs) {
  ASSERT(kArchVariant == kMips32r2);
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_D_L);
}


void Assembler::cvt_d_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_D_S);
}


// Conditions.
void Assembler::c(FPUCondition cond, SecondaryField fmt,
    FPURegister fs, FPURegister ft, uint16_t cc) {
  ASSERT(CpuFeatures::IsEnabled(FPU));
  ASSERT(is_uint3(cc));
  ASSERT((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << 16 | fs.code() << kFsShift
      | cc << 8 | 3 << 4 | cond;
  emit(instr);
}


void Assembler::fcmp(FPURegister src1, const double src2,
      FPUCondition cond) {
  ASSERT(CpuFeatures::IsEnabled(FPU));
  ASSERT(src2 == 0.0);
  mtc1(zero_reg, f14);
  cvt_d_w(f14, f14);
  c(cond, D, src1, f14, 0);
}


void Assembler::bc1f(int16_t offset, uint16_t cc) {
  ASSERT(CpuFeatures::IsEnabled(FPU));
  ASSERT(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 0 << 16 | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1t(int16_t offset, uint16_t cc) {
  ASSERT(CpuFeatures::IsEnabled(FPU));
  ASSERT(is_uint3(cc));
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
  ASSERT(IsJ(instr) || IsLui(instr));
  if (IsLui(instr)) {
    Instr instr_lui = instr_at(pc + 0 * Assembler::kInstrSize);
    Instr instr_ori = instr_at(pc + 1 * Assembler::kInstrSize);
    ASSERT(IsOri(instr_ori));
    int32_t imm = (instr_lui & static_cast<int32_t>(kImm16Mask)) << kLuiShift;
    imm |= (instr_ori & static_cast<int32_t>(kImm16Mask));
    if (imm == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    imm += pc_delta;
    ASSERT((imm & 3) == 0);

    instr_lui &= ~kImm16Mask;
    instr_ori &= ~kImm16Mask;

    instr_at_put(pc + 0 * Assembler::kInstrSize,
                 instr_lui | ((imm >> kLuiShift) & kImm16Mask));
    instr_at_put(pc + 1 * Assembler::kInstrSize,
                 instr_ori | (imm & kImm16Mask));
    return 2;  // Number of instructions patched.
  } else {
    uint32_t imm28 = (instr & static_cast<int32_t>(kImm26Mask)) << 2;
    if ((int32_t)imm28 == kEndOfJumpChain) {
      return 0;  // Number of instructions patched.
    }
    imm28 += pc_delta;
    imm28 &= kImm28Mask;
    ASSERT((imm28 & 3) == 0);

    instr &= ~kImm26Mask;
    uint32_t imm26 = imm28 >> 2;
    ASSERT(is_uint26(imm26));

    instr_at_put(pc, instr | (imm26 & kImm26Mask));
    return 1;  // Number of instructions patched.
  }
}


void Assembler::GrowBuffer() {
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // The new buffer.
  if (buffer_size_ < 4*KB) {
    desc.buffer_size = 4*KB;
  } else if (buffer_size_ < 1*MB) {
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
  int pc_delta = desc.buffer - buffer_;
  int rc_delta = (desc.buffer + desc.buffer_size) - (buffer_ + buffer_size_);
  memmove(desc.buffer, buffer_, desc.instr_size);
  memmove(reloc_info_writer.pos() + rc_delta,
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

  ASSERT(!overflow());
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


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  // We do not try to reuse pool constants.
  RelocInfo rinfo(pc_, rmode, data, NULL);
  if (rmode >= RelocInfo::JS_RETURN && rmode <= RelocInfo::DEBUG_BREAK_SLOT) {
    // Adjust code for new modes.
    ASSERT(RelocInfo::IsDebugBreakSlot(rmode)
           || RelocInfo::IsJSReturn(rmode)
           || RelocInfo::IsComment(rmode)
           || RelocInfo::IsPosition(rmode));
    // These modes do not need an entry in the constant pool.
  }
  if (rinfo.rmode() != RelocInfo::NONE) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
#ifdef DEBUG
      if (!Serializer::enabled()) {
        Serializer::TooLateToEnableNow();
      }
#endif
      if (!Serializer::enabled() && !emit_debug_code()) {
        return;
      }
    }
    ASSERT(buffer_space() >= kMaxRelocSize);  // Too late to grow buffer here.
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

  ASSERT(!trampoline_emitted_);
  ASSERT(unbound_labels_count_ >= 0);
  if (unbound_labels_count_ > 0) {
    // First we emit jump (2 instructions), then we emit trampoline pool.
    { BlockTrampolinePoolScope block_trampoline_pool(this);
      Label after_pool;
      b(&after_pool);
      nop();

      int pool_start = pc_offset();
      for (int i = 0; i < unbound_labels_count_; i++) {
        uint32_t imm32;
        imm32 = jump_address(&after_pool);
        { BlockGrowBufferScope block_buf_growth(this);
          // Buffer growth (and relocation) must be blocked for internal
          // references until associated instructions are emitted and available
          // to be patched.
          RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
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
  HeapNumber::cast(object)->set_value(OS::nan_value());
}


// On Mips, a target address is stored in a lui/ori instruction pair, each
// of which load 16 bits of the 32-bit address to a register.
// Patching the address must replace both instr, and flush the i-cache.
//
// There is an optimization below, which emits a nop when the address
// fits in just 16 bits. This is unlikely to help, and should be benchmarked,
// and possibly removed.
void Assembler::set_target_address_at(Address pc, Address target) {
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
  *(p+1) = ORI | rt_code | (rt_code << 5) | (itarget & kImm16Mask);

  // The following code is an optimization for the common case of Call()
  // or Jump() which is load to register, and jump through register:
  //     li(t9, address); jalr(t9)    (or jr(t9)).
  // If the destination address is in the same 256 MB page as the call, it
  // is faster to do a direct jal, or j, rather than jump thru register, since
  // that lets the cpu pipeline prefetch the target address. However each
  // time the address above is patched, we have to patch the direct jal/j
  // instruction, as well as possibly revert to jalr/jr if we now cross a
  // 256 MB page. Note that with the jal/j instructions, we do not need to
  // load the register, but that code is left, since it makes it easy to
  // revert this process. A further optimization could try replacing the
  // li sequence with nops.
  // This optimization can only be applied if the rt-code from instr2 is the
  // register used for the jalr/jr. Finally, we have to skip 'jr ra', which is
  // mips return. Occasionally this lands after an li().

  Instr instr3 = instr_at(pc + 2 * kInstrSize);
  uint32_t ipc = reinterpret_cast<uint32_t>(pc + 3 * kInstrSize);
  bool in_range =
             ((uint32_t)(ipc ^ itarget) >> (kImm26Bits + kImmFieldShift)) == 0;
  uint32_t target_field = (uint32_t)(itarget & kJumpAddrMask) >> kImmFieldShift;
  bool patched_jump = false;

#ifndef ALLOW_JAL_IN_BOUNDARY_REGION
  // This is a workaround to the 24k core E156 bug (affect some 34k cores also).
  // Since the excluded space is only 64KB out of 256MB (0.02 %), we will just
  // apply this workaround for all cores so we don't have to identify the core.
  if (in_range) {
    // The 24k core E156 bug has some very specific requirements, we only check
    // the most simple one: if the address of the delay slot instruction is in
    // the first or last 32 KB of the 256 MB segment.
    uint32_t segment_mask = ((256 * MB) - 1) ^ ((32 * KB) - 1);
    uint32_t ipc_segment_addr = ipc & segment_mask;
    if (ipc_segment_addr == 0 || ipc_segment_addr == segment_mask)
      in_range = false;
  }
#endif

  if (IsJalr(instr3)) {
    // Try to convert JALR to JAL.
    if (in_range && GetRt(instr2) == GetRs(instr3)) {
      *(p+2) = JAL | target_field;
      patched_jump = true;
    }
  } else if (IsJr(instr3)) {
    // Try to convert JR to J, skip returns (jr ra).
    bool is_ret = static_cast<int>(GetRs(instr3)) == ra.code();
    if (in_range && !is_ret && GetRt(instr2) == GetRs(instr3)) {
      *(p+2) = J | target_field;
      patched_jump = true;
    }
  } else if (IsJal(instr3)) {
    if (in_range) {
      // We are patching an already converted JAL.
      *(p+2) = JAL | target_field;
    } else {
      // Patch JAL, but out of range, revert to JALR.
      // JALR rs reg is the rt reg specified in the ORI instruction.
      uint32_t rs_field = GetRt(instr2) << kRsShift;
      uint32_t rd_field = ra.code() << kRdShift;  // Return-address (ra) reg.
      *(p+2) = SPECIAL | rs_field | rd_field | JALR;
    }
    patched_jump = true;
  } else if (IsJ(instr3)) {
    if (in_range) {
      // We are patching an already converted J (jump).
      *(p+2) = J | target_field;
    } else {
      // Trying patch J, but out of range, just go back to JR.
      // JR 'rs' reg is the 'rt' reg specified in the ORI instruction (instr2).
      uint32_t rs_field = GetRt(instr2) << kRsShift;
      *(p+2) = SPECIAL | rs_field | JR;
    }
    patched_jump = true;
  }

  CPU::FlushICache(pc, (patched_jump ? 3 : 2) * sizeof(int32_t));
}

void Assembler::JumpLabelToJumpRegister(Address pc) {
  // Address pc points to lui/ori instructions.
  // Jump to label may follow at pc + 2 * kInstrSize.
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
#ifdef DEBUG
  Instr instr1 = instr_at(pc);
#endif
  Instr instr2 = instr_at(pc + 1 * kInstrSize);
  Instr instr3 = instr_at(pc + 2 * kInstrSize);
  bool patched = false;

  if (IsJal(instr3)) {
    ASSERT(GetOpcodeField(instr1) == LUI);
    ASSERT(GetOpcodeField(instr2) == ORI);

    uint32_t rs_field = GetRt(instr2) << kRsShift;
    uint32_t rd_field = ra.code() << kRdShift;  // Return-address (ra) reg.
    *(p+2) = SPECIAL | rs_field | rd_field | JALR;
    patched = true;
  } else if (IsJ(instr3)) {
    ASSERT(GetOpcodeField(instr1) == LUI);
    ASSERT(GetOpcodeField(instr2) == ORI);

    uint32_t rs_field = GetRt(instr2) << kRsShift;
    *(p+2) = SPECIAL | rs_field | JR;
    patched = true;
  }

  if (patched) {
      CPU::FlushICache(pc+2, sizeof(Address));
  }
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
