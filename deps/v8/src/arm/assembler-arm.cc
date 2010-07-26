// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2010 the V8 project authors. All rights reserved.

#include "v8.h"

#if defined(V8_TARGET_ARCH_ARM)

#include "arm/assembler-arm-inl.h"
#include "serialize.h"

namespace v8 {
namespace internal {

// Safe default is no features.
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::enabled_ = 0;
unsigned CpuFeatures::found_by_runtime_probing_ = 0;


#ifdef __arm__
static uint64_t CpuFeaturesImpliedByCompiler() {
  uint64_t answer = 0;
#ifdef CAN_USE_ARMV7_INSTRUCTIONS
  answer |= 1u << ARMv7;
#endif  // def CAN_USE_ARMV7_INSTRUCTIONS
  // If the compiler is allowed to use VFP then we can use VFP too in our code
  // generation even when generating snapshots.  This won't work for cross
  // compilation.
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
  answer |= 1u << VFP3;
#endif  // defined(__VFP_FP__) && !defined(__SOFTFP__)
#ifdef CAN_USE_VFP_INSTRUCTIONS
  answer |= 1u << VFP3;
#endif  // def CAN_USE_VFP_INSTRUCTIONS
  return answer;
}
#endif  // def __arm__


void CpuFeatures::Probe() {
#ifndef __arm__
  // For the simulator=arm build, use VFP when FLAG_enable_vfp3 is enabled.
  if (FLAG_enable_vfp3) {
    supported_ |= 1u << VFP3;
  }
  // For the simulator=arm build, use ARMv7 when FLAG_enable_armv7 is enabled
  if (FLAG_enable_armv7) {
    supported_ |= 1u << ARMv7;
  }
#else  // def __arm__
  if (Serializer::enabled()) {
    supported_ |= OS::CpuFeaturesImpliedByPlatform();
    supported_ |= CpuFeaturesImpliedByCompiler();
    return;  // No features if we might serialize.
  }

  if (OS::ArmCpuHasFeature(VFP3)) {
    // This implementation also sets the VFP flags if
    // runtime detection of VFP returns true.
    supported_ |= 1u << VFP3;
    found_by_runtime_probing_ |= 1u << VFP3;
  }

  if (OS::ArmCpuHasFeature(ARMv7)) {
    supported_ |= 1u << ARMv7;
    found_by_runtime_probing_ |= 1u << ARMv7;
  }
#endif
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

const int RelocInfo::kApplyMask = 0;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on ARM means that it is a movw/movt instruction.  We don't
  // generate those yet.
  return false;
}



void RelocInfo::PatchCode(byte* instructions, int instruction_count) {
  // Patch the code at the current address with the supplied instructions.
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
  UNIMPLEMENTED();
}


// -----------------------------------------------------------------------------
// Implementation of Operand and MemOperand
// See assembler-arm-inl.h for inlined constructors

Operand::Operand(Handle<Object> handle) {
  rm_ = no_reg;
  // Verify all Objects referred by code are NOT in new space.
  Object* obj = *handle;
  ASSERT(!Heap::InNewSpace(obj));
  if (obj->IsHeapObject()) {
    imm32_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // no relocation needed
    imm32_ =  reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE;
  }
}


Operand::Operand(Register rm, ShiftOp shift_op, int shift_imm) {
  ASSERT(is_uint5(shift_imm));
  ASSERT(shift_op != ROR || shift_imm != 0);  // use RRX if you mean it
  rm_ = rm;
  rs_ = no_reg;
  shift_op_ = shift_op;
  shift_imm_ = shift_imm & 31;
  if (shift_op == RRX) {
    // encoded as ROR with shift_imm == 0
    ASSERT(shift_imm == 0);
    shift_op_ = ROR;
    shift_imm_ = 0;
  }
}


Operand::Operand(Register rm, ShiftOp shift_op, Register rs) {
  ASSERT(shift_op != RRX);
  rm_ = rm;
  rs_ = no_reg;
  shift_op_ = shift_op;
  rs_ = rs;
}


MemOperand::MemOperand(Register rn, int32_t offset, AddrMode am) {
  rn_ = rn;
  rm_ = no_reg;
  offset_ = offset;
  am_ = am;
}

MemOperand::MemOperand(Register rn, Register rm, AddrMode am) {
  rn_ = rn;
  rm_ = rm;
  shift_op_ = LSL;
  shift_imm_ = 0;
  am_ = am;
}


MemOperand::MemOperand(Register rn, Register rm,
                       ShiftOp shift_op, int shift_imm, AddrMode am) {
  ASSERT(is_uint5(shift_imm));
  rn_ = rn;
  rm_ = rm;
  shift_op_ = shift_op;
  shift_imm_ = shift_imm & 31;
  am_ = am;
}


// -----------------------------------------------------------------------------
// Implementation of Assembler.

// Instruction encoding bits.
enum {
  H   = 1 << 5,   // halfword (or byte)
  S6  = 1 << 6,   // signed (or unsigned)
  L   = 1 << 20,  // load (or store)
  S   = 1 << 20,  // set condition code (or leave unchanged)
  W   = 1 << 21,  // writeback base register (or leave unchanged)
  A   = 1 << 21,  // accumulate in multiply instruction (or not)
  B   = 1 << 22,  // unsigned byte (or word)
  N   = 1 << 22,  // long (or short)
  U   = 1 << 23,  // positive (or negative) offset/index
  P   = 1 << 24,  // offset/pre-indexed addressing (or post-indexed addressing)
  I   = 1 << 25,  // immediate shifter operand (or not)

  B4  = 1 << 4,
  B5  = 1 << 5,
  B6  = 1 << 6,
  B7  = 1 << 7,
  B8  = 1 << 8,
  B9  = 1 << 9,
  B12 = 1 << 12,
  B16 = 1 << 16,
  B18 = 1 << 18,
  B19 = 1 << 19,
  B20 = 1 << 20,
  B21 = 1 << 21,
  B22 = 1 << 22,
  B23 = 1 << 23,
  B24 = 1 << 24,
  B25 = 1 << 25,
  B26 = 1 << 26,
  B27 = 1 << 27,

  // Instruction bit masks.
  RdMask     = 15 << 12,  // in str instruction
  CondMask   = 15 << 28,
  CoprocessorMask = 15 << 8,
  OpCodeMask = 15 << 21,  // in data-processing instructions
  Imm24Mask  = (1 << 24) - 1,
  Off12Mask  = (1 << 12) - 1,
  // Reserved condition.
  nv = 15 << 28
};


// add(sp, sp, 4) instruction (aka Pop())
static const Instr kPopInstruction =
    al | 4 * B21 | 4 | LeaveCC | I | sp.code() * B16 | sp.code() * B12;
// str(r, MemOperand(sp, 4, NegPreIndex), al) instruction (aka push(r))
// register r is not encoded.
static const Instr kPushRegPattern =
    al | B26 | 4 | NegPreIndex | sp.code() * B16;
// ldr(r, MemOperand(sp, 4, PostIndex), al) instruction (aka pop(r))
// register r is not encoded.
static const Instr kPopRegPattern =
    al | B26 | L | 4 | PostIndex | sp.code() * B16;
// mov lr, pc
const Instr kMovLrPc = al | 13*B21 | pc.code() | lr.code() * B12;
// ldr rd, [pc, #offset]
const Instr kLdrPCMask = CondMask | 15 * B24 | 7 * B20 | 15 * B16;
const Instr kLdrPCPattern = al | 5 * B24 | L | pc.code() * B16;
// blxcc rm
const Instr kBlxRegMask =
    15 * B24 | 15 * B20 | 15 * B16 | 15 * B12 | 15 * B8 | 15 * B4;
const Instr kBlxRegPattern =
    B24 | B21 | 15 * B16 | 15 * B12 | 15 * B8 | 3 * B4;
const Instr kMovMvnMask = 0x6d * B21 | 0xf * B16;
const Instr kMovMvnPattern = 0xd * B21;
const Instr kMovMvnFlip = B22;
const Instr kMovLeaveCCMask = 0xdff * B16;
const Instr kMovLeaveCCPattern = 0x1a0 * B16;
const Instr kMovwMask = 0xff * B20;
const Instr kMovwPattern = 0x30 * B20;
const Instr kMovwLeaveCCFlip = 0x5 * B21;
const Instr kCmpCmnMask = 0xdd * B20 | 0xf * B12;
const Instr kCmpCmnPattern = 0x15 * B20;
const Instr kCmpCmnFlip = B21;
const Instr kALUMask = 0x6f * B21;
const Instr kAddPattern = 0x4 * B21;
const Instr kSubPattern = 0x2 * B21;
const Instr kBicPattern = 0xe * B21;
const Instr kAndPattern = 0x0 * B21;
const Instr kAddSubFlip = 0x6 * B21;
const Instr kAndBicFlip = 0xe * B21;

// A mask for the Rd register for push, pop, ldr, str instructions.
const Instr kRdMask = 0x0000f000;
static const int kRdShift = 12;
static const Instr kLdrRegFpOffsetPattern =
    al | B26 | L | Offset | fp.code() * B16;
static const Instr kStrRegFpOffsetPattern =
    al | B26 | Offset | fp.code() * B16;
static const Instr kLdrRegFpNegOffsetPattern =
    al | B26 | L | NegOffset | fp.code() * B16;
static const Instr kStrRegFpNegOffsetPattern =
    al | B26 | NegOffset | fp.code() * B16;
static const Instr kLdrStrInstrTypeMask = 0xffff0000;
static const Instr kLdrStrInstrArgumentMask = 0x0000ffff;
static const Instr kLdrStrOffsetMask = 0x00000fff;

// Spare buffer.
static const int kMinimalBufferSize = 4*KB;
static byte* spare_buffer_ = NULL;

Assembler::Assembler(void* buffer, int buffer_size) {
  if (buffer == NULL) {
    // Do our own buffer management.
    if (buffer_size <= kMinimalBufferSize) {
      buffer_size = kMinimalBufferSize;

      if (spare_buffer_ != NULL) {
        buffer = spare_buffer_;
        spare_buffer_ = NULL;
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

  // Setup buffer pointers.
  ASSERT(buffer_ != NULL);
  pc_ = buffer_;
  reloc_info_writer.Reposition(buffer_ + buffer_size, pc_);
  num_prinfo_ = 0;
  next_buffer_check_ = 0;
  const_pool_blocked_nesting_ = 0;
  no_const_pool_before_ = 0;
  last_const_pool_end_ = 0;
  last_bound_pos_ = 0;
  current_statement_position_ = RelocInfo::kNoPosition;
  current_position_ = RelocInfo::kNoPosition;
  written_statement_position_ = current_statement_position_;
  written_position_ = current_position_;
}


Assembler::~Assembler() {
  ASSERT(const_pool_blocked_nesting_ == 0);
  if (own_buffer_) {
    if (spare_buffer_ == NULL && buffer_size_ == kMinimalBufferSize) {
      spare_buffer_ = buffer_;
    } else {
      DeleteArray(buffer_);
    }
  }
}


void Assembler::GetCode(CodeDesc* desc) {
  // Emit constant pool if necessary.
  CheckConstPool(true, false);
  ASSERT(num_prinfo_ == 0);

  // Setup code descriptor.
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
  // Preferred alignment of jump targets on some ARM chips.
  Align(8);
}


bool Assembler::IsNop(Instr instr, int type) {
  // Check for mov rx, rx.
  ASSERT(0 <= type && type <= 14);  // mov pc, pc is not a nop.
  return instr == (al | 13*B21 | type*B12 | type);
}


bool Assembler::IsBranch(Instr instr) {
  return (instr & (B27 | B25)) == (B27 | B25);
}


int Assembler::GetBranchOffset(Instr instr) {
  ASSERT(IsBranch(instr));
  // Take the jump offset in the lower 24 bits, sign extend it and multiply it
  // with 4 to get the offset in bytes.
  return ((instr & Imm24Mask) << 8) >> 6;
}


bool Assembler::IsLdrRegisterImmediate(Instr instr) {
  return (instr & (B27 | B26 | B25 | B22 | B20)) == (B26 | B20);
}


int Assembler::GetLdrRegisterImmediateOffset(Instr instr) {
  ASSERT(IsLdrRegisterImmediate(instr));
  bool positive = (instr & B23) == B23;
  int offset = instr & Off12Mask;  // Zero extended offset.
  return positive ? offset : -offset;
}


Instr Assembler::SetLdrRegisterImmediateOffset(Instr instr, int offset) {
  ASSERT(IsLdrRegisterImmediate(instr));
  bool positive = offset >= 0;
  if (!positive) offset = -offset;
  ASSERT(is_uint12(offset));
  // Set bit indicating whether the offset should be added.
  instr = (instr & ~B23) | (positive ? B23 : 0);
  // Set the actual offset.
  return (instr & ~Off12Mask) | offset;
}


bool Assembler::IsStrRegisterImmediate(Instr instr) {
  return (instr & (B27 | B26 | B25 | B22 | B20)) == B26;
}


Instr Assembler::SetStrRegisterImmediateOffset(Instr instr, int offset) {
  ASSERT(IsStrRegisterImmediate(instr));
  bool positive = offset >= 0;
  if (!positive) offset = -offset;
  ASSERT(is_uint12(offset));
  // Set bit indicating whether the offset should be added.
  instr = (instr & ~B23) | (positive ? B23 : 0);
  // Set the actual offset.
  return (instr & ~Off12Mask) | offset;
}


bool Assembler::IsAddRegisterImmediate(Instr instr) {
  return (instr & (B27 | B26 | B25 | B24 | B23 | B22 | B21)) == (B25 | B23);
}


Instr Assembler::SetAddRegisterImmediateOffset(Instr instr, int offset) {
  ASSERT(IsAddRegisterImmediate(instr));
  ASSERT(offset >= 0);
  ASSERT(is_uint12(offset));
  // Set the offset.
  return (instr & ~Off12Mask) | offset;
}


Register Assembler::GetRd(Instr instr) {
  Register reg;
  reg.code_ = ((instr & kRdMask) >> kRdShift);
  return reg;
}


bool Assembler::IsPush(Instr instr) {
  return ((instr & ~kRdMask) == kPushRegPattern);
}


bool Assembler::IsPop(Instr instr) {
  return ((instr & ~kRdMask) == kPopRegPattern);
}


bool Assembler::IsStrRegFpOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kStrRegFpOffsetPattern);
}


bool Assembler::IsLdrRegFpOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kLdrRegFpOffsetPattern);
}


bool Assembler::IsStrRegFpNegOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kStrRegFpNegOffsetPattern);
}


bool Assembler::IsLdrRegFpNegOffset(Instr instr) {
  return ((instr & kLdrStrInstrTypeMask) == kLdrRegFpNegOffsetPattern);
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


// The link chain is terminated by a negative code position (must be aligned)
const int kEndOfChain = -4;


int Assembler::target_at(int pos)  {
  Instr instr = instr_at(pos);
  if ((instr & ~Imm24Mask) == 0) {
    // Emitted label constant, not part of a branch.
    return instr - (Code::kHeaderSize - kHeapObjectTag);
  }
  ASSERT((instr & 7*B25) == 5*B25);  // b, bl, or blx imm24
  int imm26 = ((instr & Imm24Mask) << 8) >> 6;
  if ((instr & CondMask) == nv && (instr & B24) != 0) {
    // blx uses bit 24 to encode bit 2 of imm26
    imm26 += 2;
  }
  return pos + kPcLoadDelta + imm26;
}


void Assembler::target_at_put(int pos, int target_pos) {
  Instr instr = instr_at(pos);
  if ((instr & ~Imm24Mask) == 0) {
    ASSERT(target_pos == kEndOfChain || target_pos >= 0);
    // Emitted label constant, not part of a branch.
    // Make label relative to Code* of generated Code object.
    instr_at_put(pos, target_pos + (Code::kHeaderSize - kHeapObjectTag));
    return;
  }
  int imm26 = target_pos - (pos + kPcLoadDelta);
  ASSERT((instr & 7*B25) == 5*B25);  // b, bl, or blx imm24
  if ((instr & CondMask) == nv) {
    // blx uses bit 24 to encode bit 2 of imm26
    ASSERT((imm26 & 1) == 0);
    instr = (instr & ~(B24 | Imm24Mask)) | ((imm26 & 2) >> 1)*B24;
  } else {
    ASSERT((imm26 & 3) == 0);
    instr &= ~Imm24Mask;
  }
  int imm24 = imm26 >> 2;
  ASSERT(is_int24(imm24));
  instr_at_put(pos, instr | (imm24 & Imm24Mask));
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
      if ((instr & ~Imm24Mask) == 0) {
        PrintF("value\n");
      } else {
        ASSERT((instr & 7*B25) == 5*B25);  // b, bl, or blx
        int cond = instr & CondMask;
        const char* b;
        const char* c;
        if (cond == nv) {
          b = "blx";
          c = "";
        } else {
          if ((instr & B24) != 0)
            b = "bl";
          else
            b = "b";

          switch (cond) {
            case eq: c = "eq"; break;
            case ne: c = "ne"; break;
            case hs: c = "hs"; break;
            case lo: c = "lo"; break;
            case mi: c = "mi"; break;
            case pl: c = "pl"; break;
            case vs: c = "vs"; break;
            case vc: c = "vc"; break;
            case hi: c = "hi"; break;
            case ls: c = "ls"; break;
            case ge: c = "ge"; break;
            case lt: c = "lt"; break;
            case gt: c = "gt"; break;
            case le: c = "le"; break;
            case al: c = ""; break;
            default:
              c = "";
              UNREACHABLE();
          }
        }
        PrintF("%s%s\n", b, c);
      }
      next(&l);
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}


void Assembler::bind_to(Label* L, int pos) {
  ASSERT(0 <= pos && pos <= pc_offset());  // must have a valid binding position
  while (L->is_linked()) {
    int fixup_pos = L->pos();
    next(L);  // call next before overwriting link with target at fixup_pos
    target_at_put(fixup_pos, pos);
  }
  L->bind_to(pos);

  // Keep track of the last bound label so we don't eliminate any instructions
  // before a bound label.
  if (pos > last_bound_pos_)
    last_bound_pos_ = pos;
}


void Assembler::link_to(Label* L, Label* appendix) {
  if (appendix->is_linked()) {
    if (L->is_linked()) {
      // Append appendix to L's list.
      int fixup_pos;
      int link = L->pos();
      do {
        fixup_pos = link;
        link = target_at(fixup_pos);
      } while (link > 0);
      ASSERT(link == kEndOfChain);
      target_at_put(fixup_pos, appendix->pos());
    } else {
      // L is empty, simply use appendix.
      *L = *appendix;
    }
  }
  appendix->Unuse();  // appendix should not be used anymore
}


void Assembler::bind(Label* L) {
  ASSERT(!L->is_bound());  // label can only be bound once
  bind_to(L, pc_offset());
}


void Assembler::next(Label* L) {
  ASSERT(L->is_linked());
  int link = target_at(L->pos());
  if (link > 0) {
    L->link_to(link);
  } else {
    ASSERT(link == kEndOfChain);
    L->Unuse();
  }
}


static Instr EncodeMovwImmediate(uint32_t immediate) {
  ASSERT(immediate < 0x10000);
  return ((immediate & 0xf000) << 4) | (immediate & 0xfff);
}


// Low-level code emission routines depending on the addressing mode.
// If this returns true then you have to use the rotate_imm and immed_8
// that it returns, because it may have already changed the instruction
// to match them!
static bool fits_shifter(uint32_t imm32,
                         uint32_t* rotate_imm,
                         uint32_t* immed_8,
                         Instr* instr) {
  // imm32 must be unsigned.
  for (int rot = 0; rot < 16; rot++) {
    uint32_t imm8 = (imm32 << 2*rot) | (imm32 >> (32 - 2*rot));
    if ((imm8 <= 0xff)) {
      *rotate_imm = rot;
      *immed_8 = imm8;
      return true;
    }
  }
  // If the opcode is one with a complementary version and the complementary
  // immediate fits, change the opcode.
  if (instr != NULL) {
    if ((*instr & kMovMvnMask) == kMovMvnPattern) {
      if (fits_shifter(~imm32, rotate_imm, immed_8, NULL)) {
        *instr ^= kMovMvnFlip;
        return true;
      } else if ((*instr & kMovLeaveCCMask) == kMovLeaveCCPattern) {
        if (CpuFeatures::IsSupported(ARMv7)) {
          if (imm32 < 0x10000) {
            *instr ^= kMovwLeaveCCFlip;
            *instr |= EncodeMovwImmediate(imm32);
            *rotate_imm = *immed_8 = 0;  // Not used for movw.
            return true;
          }
        }
      }
    } else if ((*instr & kCmpCmnMask) == kCmpCmnPattern) {
      if (fits_shifter(-imm32, rotate_imm, immed_8, NULL)) {
        *instr ^= kCmpCmnFlip;
        return true;
      }
    } else {
      Instr alu_insn = (*instr & kALUMask);
      if (alu_insn == kAddPattern ||
          alu_insn == kSubPattern) {
        if (fits_shifter(-imm32, rotate_imm, immed_8, NULL)) {
          *instr ^= kAddSubFlip;
          return true;
        }
      } else if (alu_insn == kAndPattern ||
                 alu_insn == kBicPattern) {
        if (fits_shifter(~imm32, rotate_imm, immed_8, NULL)) {
          *instr ^= kAndBicFlip;
          return true;
        }
      }
    }
  }
  return false;
}


// We have to use the temporary register for things that can be relocated even
// if they can be encoded in the ARM's 12 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
static bool MustUseConstantPool(RelocInfo::Mode rmode) {
  if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
#ifdef DEBUG
    if (!Serializer::enabled()) {
      Serializer::TooLateToEnableNow();
    }
#endif  // def DEBUG
    return Serializer::enabled();
  } else if (rmode == RelocInfo::NONE) {
    return false;
  }
  return true;
}


bool Operand::is_single_instruction() const {
  if (rm_.is_valid()) return true;
  if (MustUseConstantPool(rmode_)) return false;
  uint32_t dummy1, dummy2;
  return fits_shifter(imm32_, &dummy1, &dummy2, NULL);
}


void Assembler::addrmod1(Instr instr,
                         Register rn,
                         Register rd,
                         const Operand& x) {
  CheckBuffer();
  ASSERT((instr & ~(CondMask | OpCodeMask | S)) == 0);
  if (!x.rm_.is_valid()) {
    // Immediate.
    uint32_t rotate_imm;
    uint32_t immed_8;
    if (MustUseConstantPool(x.rmode_) ||
        !fits_shifter(x.imm32_, &rotate_imm, &immed_8, &instr)) {
      // The immediate operand cannot be encoded as a shifter operand, so load
      // it first to register ip and change the original instruction to use ip.
      // However, if the original instruction is a 'mov rd, x' (not setting the
      // condition code), then replace it with a 'ldr rd, [pc]'.
      CHECK(!rn.is(ip));  // rn should never be ip, or will be trashed
      Condition cond = static_cast<Condition>(instr & CondMask);
      if ((instr & ~CondMask) == 13*B21) {  // mov, S not set
        if (MustUseConstantPool(x.rmode_) ||
            !CpuFeatures::IsSupported(ARMv7)) {
          RecordRelocInfo(x.rmode_, x.imm32_);
          ldr(rd, MemOperand(pc, 0), cond);
        } else {
          // Will probably use movw, will certainly not use constant pool.
          mov(rd, Operand(x.imm32_ & 0xffff), LeaveCC, cond);
          movt(rd, static_cast<uint32_t>(x.imm32_) >> 16, cond);
        }
      } else {
        // If this is not a mov or mvn instruction we may still be able to avoid
        // a constant pool entry by using mvn or movw.
        if (!MustUseConstantPool(x.rmode_) &&
            (instr & kMovMvnMask) != kMovMvnPattern) {
          mov(ip, x, LeaveCC, cond);
        } else {
          RecordRelocInfo(x.rmode_, x.imm32_);
          ldr(ip, MemOperand(pc, 0), cond);
        }
        addrmod1(instr, rn, rd, Operand(ip));
      }
      return;
    }
    instr |= I | rotate_imm*B8 | immed_8;
  } else if (!x.rs_.is_valid()) {
    // Immediate shift.
    instr |= x.shift_imm_*B7 | x.shift_op_ | x.rm_.code();
  } else {
    // Register shift.
    ASSERT(!rn.is(pc) && !rd.is(pc) && !x.rm_.is(pc) && !x.rs_.is(pc));
    instr |= x.rs_.code()*B8 | x.shift_op_ | B4 | x.rm_.code();
  }
  emit(instr | rn.code()*B16 | rd.code()*B12);
  if (rn.is(pc) || x.rm_.is(pc)) {
    // Block constant pool emission for one instruction after reading pc.
    BlockConstPoolBefore(pc_offset() + kInstrSize);
  }
}


void Assembler::addrmod2(Instr instr, Register rd, const MemOperand& x) {
  ASSERT((instr & ~(CondMask | B | L)) == B26);
  int am = x.am_;
  if (!x.rm_.is_valid()) {
    // Immediate offset.
    int offset_12 = x.offset_;
    if (offset_12 < 0) {
      offset_12 = -offset_12;
      am ^= U;
    }
    if (!is_uint12(offset_12)) {
      // Immediate offset cannot be encoded, load it first to register ip
      // rn (and rd in a load) should never be ip, or will be trashed.
      ASSERT(!x.rn_.is(ip) && ((instr & L) == L || !rd.is(ip)));
      mov(ip, Operand(x.offset_), LeaveCC,
          static_cast<Condition>(instr & CondMask));
      addrmod2(instr, rd, MemOperand(x.rn_, ip, x.am_));
      return;
    }
    ASSERT(offset_12 >= 0);  // no masking needed
    instr |= offset_12;
  } else {
    // Register offset (shift_imm_ and shift_op_ are 0) or scaled
    // register offset the constructors make sure than both shift_imm_
    // and shift_op_ are initialized.
    ASSERT(!x.rm_.is(pc));
    instr |= B25 | x.shift_imm_*B7 | x.shift_op_ | x.rm_.code();
  }
  ASSERT((am & (P|W)) == P || !x.rn_.is(pc));  // no pc base with writeback
  emit(instr | am | x.rn_.code()*B16 | rd.code()*B12);
}


void Assembler::addrmod3(Instr instr, Register rd, const MemOperand& x) {
  ASSERT((instr & ~(CondMask | L | S6 | H)) == (B4 | B7));
  ASSERT(x.rn_.is_valid());
  int am = x.am_;
  if (!x.rm_.is_valid()) {
    // Immediate offset.
    int offset_8 = x.offset_;
    if (offset_8 < 0) {
      offset_8 = -offset_8;
      am ^= U;
    }
    if (!is_uint8(offset_8)) {
      // Immediate offset cannot be encoded, load it first to register ip
      // rn (and rd in a load) should never be ip, or will be trashed.
      ASSERT(!x.rn_.is(ip) && ((instr & L) == L || !rd.is(ip)));
      mov(ip, Operand(x.offset_), LeaveCC,
          static_cast<Condition>(instr & CondMask));
      addrmod3(instr, rd, MemOperand(x.rn_, ip, x.am_));
      return;
    }
    ASSERT(offset_8 >= 0);  // no masking needed
    instr |= B | (offset_8 >> 4)*B8 | (offset_8 & 0xf);
  } else if (x.shift_imm_ != 0) {
    // Scaled register offset not supported, load index first
    // rn (and rd in a load) should never be ip, or will be trashed.
    ASSERT(!x.rn_.is(ip) && ((instr & L) == L || !rd.is(ip)));
    mov(ip, Operand(x.rm_, x.shift_op_, x.shift_imm_), LeaveCC,
        static_cast<Condition>(instr & CondMask));
    addrmod3(instr, rd, MemOperand(x.rn_, ip, x.am_));
    return;
  } else {
    // Register offset.
    ASSERT((am & (P|W)) == P || !x.rm_.is(pc));  // no pc index with writeback
    instr |= x.rm_.code();
  }
  ASSERT((am & (P|W)) == P || !x.rn_.is(pc));  // no pc base with writeback
  emit(instr | am | x.rn_.code()*B16 | rd.code()*B12);
}


void Assembler::addrmod4(Instr instr, Register rn, RegList rl) {
  ASSERT((instr & ~(CondMask | P | U | W | L)) == B27);
  ASSERT(rl != 0);
  ASSERT(!rn.is(pc));
  emit(instr | rn.code()*B16 | rl);
}


void Assembler::addrmod5(Instr instr, CRegister crd, const MemOperand& x) {
  // Unindexed addressing is not encoded by this function.
  ASSERT_EQ((B27 | B26),
            (instr & ~(CondMask | CoprocessorMask | P | U | N | W | L)));
  ASSERT(x.rn_.is_valid() && !x.rm_.is_valid());
  int am = x.am_;
  int offset_8 = x.offset_;
  ASSERT((offset_8 & 3) == 0);  // offset must be an aligned word offset
  offset_8 >>= 2;
  if (offset_8 < 0) {
    offset_8 = -offset_8;
    am ^= U;
  }
  ASSERT(is_uint8(offset_8));  // unsigned word offset must fit in a byte
  ASSERT((am & (P|W)) == P || !x.rn_.is(pc));  // no pc base with writeback

  // Post-indexed addressing requires W == 1; different than in addrmod2/3.
  if ((am & P) == 0)
    am |= W;

  ASSERT(offset_8 >= 0);  // no masking needed
  emit(instr | am | x.rn_.code()*B16 | crd.code()*B12 | offset_8);
}


int Assembler::branch_offset(Label* L, bool jump_elimination_allowed) {
  int target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link
    } else {
      target_pos = kEndOfChain;
    }
    L->link_to(pc_offset());
  }

  // Block the emission of the constant pool, since the branch instruction must
  // be emitted at the pc offset recorded by the label.
  BlockConstPoolBefore(pc_offset() + kInstrSize);
  return target_pos - (pc_offset() + kPcLoadDelta);
}


void Assembler::label_at_put(Label* L, int at_offset) {
  int target_pos;
  if (L->is_bound()) {
    target_pos = L->pos();
  } else {
    if (L->is_linked()) {
      target_pos = L->pos();  // L's link
    } else {
      target_pos = kEndOfChain;
    }
    L->link_to(at_offset);
    instr_at_put(at_offset, target_pos + (Code::kHeaderSize - kHeapObjectTag));
  }
}


// Branch instructions.
void Assembler::b(int branch_offset, Condition cond) {
  ASSERT((branch_offset & 3) == 0);
  int imm24 = branch_offset >> 2;
  ASSERT(is_int24(imm24));
  emit(cond | B27 | B25 | (imm24 & Imm24Mask));

  if (cond == al) {
    // Dead code is a good location to emit the constant pool.
    CheckConstPool(false, false);
  }
}


void Assembler::bl(int branch_offset, Condition cond) {
  ASSERT((branch_offset & 3) == 0);
  int imm24 = branch_offset >> 2;
  ASSERT(is_int24(imm24));
  emit(cond | B27 | B25 | B24 | (imm24 & Imm24Mask));
}


void Assembler::blx(int branch_offset) {  // v5 and above
  WriteRecordedPositions();
  ASSERT((branch_offset & 1) == 0);
  int h = ((branch_offset & 2) >> 1)*B24;
  int imm24 = branch_offset >> 2;
  ASSERT(is_int24(imm24));
  emit(15 << 28 | B27 | B25 | h | (imm24 & Imm24Mask));
}


void Assembler::blx(Register target, Condition cond) {  // v5 and above
  WriteRecordedPositions();
  ASSERT(!target.is(pc));
  emit(cond | B24 | B21 | 15*B16 | 15*B12 | 15*B8 | 3*B4 | target.code());
}


void Assembler::bx(Register target, Condition cond) {  // v5 and above, plus v4t
  WriteRecordedPositions();
  ASSERT(!target.is(pc));  // use of pc is actually allowed, but discouraged
  emit(cond | B24 | B21 | 15*B16 | 15*B12 | 15*B8 | B4 | target.code());
}


// Data-processing instructions.

void Assembler::and_(Register dst, Register src1, const Operand& src2,
                     SBit s, Condition cond) {
  addrmod1(cond | 0*B21 | s, src1, dst, src2);
}


void Assembler::eor(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 1*B21 | s, src1, dst, src2);
}


void Assembler::sub(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 2*B21 | s, src1, dst, src2);
}


void Assembler::rsb(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 3*B21 | s, src1, dst, src2);
}


void Assembler::add(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 4*B21 | s, src1, dst, src2);

  // Eliminate pattern: push(r), pop()
  //   str(src, MemOperand(sp, 4, NegPreIndex), al);
  //   add(sp, sp, Operand(kPointerSize));
  // Both instructions can be eliminated.
  if (can_peephole_optimize(2) &&
      // Pattern.
      instr_at(pc_ - 1 * kInstrSize) == kPopInstruction &&
      (instr_at(pc_ - 2 * kInstrSize) & ~RdMask) == kPushRegPattern) {
    pc_ -= 2 * kInstrSize;
    if (FLAG_print_peephole_optimization) {
      PrintF("%x push(reg)/pop() eliminated\n", pc_offset());
    }
  }
}


void Assembler::adc(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 5*B21 | s, src1, dst, src2);
}


void Assembler::sbc(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 6*B21 | s, src1, dst, src2);
}


void Assembler::rsc(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 7*B21 | s, src1, dst, src2);
}


void Assembler::tst(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | 8*B21 | S, src1, r0, src2);
}


void Assembler::teq(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | 9*B21 | S, src1, r0, src2);
}


void Assembler::cmp(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | 10*B21 | S, src1, r0, src2);
}


void Assembler::cmn(Register src1, const Operand& src2, Condition cond) {
  addrmod1(cond | 11*B21 | S, src1, r0, src2);
}


void Assembler::orr(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 12*B21 | s, src1, dst, src2);
}


void Assembler::mov(Register dst, const Operand& src, SBit s, Condition cond) {
  if (dst.is(pc)) {
    WriteRecordedPositions();
  }
  // Don't allow nop instructions in the form mov rn, rn to be generated using
  // the mov instruction. They must be generated using nop(int)
  // pseudo instructions.
  ASSERT(!(src.is_reg() && src.rm().is(dst) && s == LeaveCC && cond == al));
  addrmod1(cond | 13*B21 | s, r0, dst, src);
}


void Assembler::movw(Register reg, uint32_t immediate, Condition cond) {
  ASSERT(immediate < 0x10000);
  mov(reg, Operand(immediate), LeaveCC, cond);
}


void Assembler::movt(Register reg, uint32_t immediate, Condition cond) {
  emit(cond | 0x34*B20 | reg.code()*B12 | EncodeMovwImmediate(immediate));
}


void Assembler::bic(Register dst, Register src1, const Operand& src2,
                    SBit s, Condition cond) {
  addrmod1(cond | 14*B21 | s, src1, dst, src2);
}


void Assembler::mvn(Register dst, const Operand& src, SBit s, Condition cond) {
  addrmod1(cond | 15*B21 | s, r0, dst, src);
}


// Multiply instructions.
void Assembler::mla(Register dst, Register src1, Register src2, Register srcA,
                    SBit s, Condition cond) {
  ASSERT(!dst.is(pc) && !src1.is(pc) && !src2.is(pc) && !srcA.is(pc));
  emit(cond | A | s | dst.code()*B16 | srcA.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::mul(Register dst, Register src1, Register src2,
                    SBit s, Condition cond) {
  ASSERT(!dst.is(pc) && !src1.is(pc) && !src2.is(pc));
  // dst goes in bits 16-19 for this instruction!
  emit(cond | s | dst.code()*B16 | src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::smlal(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  ASSERT(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  ASSERT(!dstL.is(dstH));
  emit(cond | B23 | B22 | A | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::smull(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  ASSERT(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  ASSERT(!dstL.is(dstH));
  emit(cond | B23 | B22 | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::umlal(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  ASSERT(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  ASSERT(!dstL.is(dstH));
  emit(cond | B23 | A | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


void Assembler::umull(Register dstL,
                      Register dstH,
                      Register src1,
                      Register src2,
                      SBit s,
                      Condition cond) {
  ASSERT(!dstL.is(pc) && !dstH.is(pc) && !src1.is(pc) && !src2.is(pc));
  ASSERT(!dstL.is(dstH));
  emit(cond | B23 | s | dstH.code()*B16 | dstL.code()*B12 |
       src2.code()*B8 | B7 | B4 | src1.code());
}


// Miscellaneous arithmetic instructions.
void Assembler::clz(Register dst, Register src, Condition cond) {
  // v5 and above.
  ASSERT(!dst.is(pc) && !src.is(pc));
  emit(cond | B24 | B22 | B21 | 15*B16 | dst.code()*B12 |
       15*B8 | B4 | src.code());
}


// Saturating instructions.

// Unsigned saturate.
void Assembler::usat(Register dst,
                     int satpos,
                     const Operand& src,
                     Condition cond) {
  // v6 and above.
  ASSERT(CpuFeatures::IsSupported(ARMv7));
  ASSERT(!dst.is(pc) && !src.rm_.is(pc));
  ASSERT((satpos >= 0) && (satpos <= 31));
  ASSERT((src.shift_op_ == ASR) || (src.shift_op_ == LSL));
  ASSERT(src.rs_.is(no_reg));

  int sh = 0;
  if (src.shift_op_ == ASR) {
      sh = 1;
  }

  emit(cond | 0x6*B24 | 0xe*B20 | satpos*B16 | dst.code()*B12 |
       src.shift_imm_*B7 | sh*B6 | 0x1*B4 | src.rm_.code());
}


// Bitfield manipulation instructions.

// Unsigned bit field extract.
// Extracts #width adjacent bits from position #lsb in a register, and
// writes them to the low bits of a destination register.
//   ubfx dst, src, #lsb, #width
void Assembler::ubfx(Register dst,
                     Register src,
                     int lsb,
                     int width,
                     Condition cond) {
  // v7 and above.
  ASSERT(CpuFeatures::IsSupported(ARMv7));
  ASSERT(!dst.is(pc) && !src.is(pc));
  ASSERT((lsb >= 0) && (lsb <= 31));
  ASSERT((width >= 1) && (width <= (32 - lsb)));
  emit(cond | 0xf*B23 | B22 | B21 | (width - 1)*B16 | dst.code()*B12 |
       lsb*B7 | B6 | B4 | src.code());
}


// Signed bit field extract.
// Extracts #width adjacent bits from position #lsb in a register, and
// writes them to the low bits of a destination register. The extracted
// value is sign extended to fill the destination register.
//   sbfx dst, src, #lsb, #width
void Assembler::sbfx(Register dst,
                     Register src,
                     int lsb,
                     int width,
                     Condition cond) {
  // v7 and above.
  ASSERT(CpuFeatures::IsSupported(ARMv7));
  ASSERT(!dst.is(pc) && !src.is(pc));
  ASSERT((lsb >= 0) && (lsb <= 31));
  ASSERT((width >= 1) && (width <= (32 - lsb)));
  emit(cond | 0xf*B23 | B21 | (width - 1)*B16 | dst.code()*B12 |
       lsb*B7 | B6 | B4 | src.code());
}


// Bit field clear.
// Sets #width adjacent bits at position #lsb in the destination register
// to zero, preserving the value of the other bits.
//   bfc dst, #lsb, #width
void Assembler::bfc(Register dst, int lsb, int width, Condition cond) {
  // v7 and above.
  ASSERT(CpuFeatures::IsSupported(ARMv7));
  ASSERT(!dst.is(pc));
  ASSERT((lsb >= 0) && (lsb <= 31));
  ASSERT((width >= 1) && (width <= (32 - lsb)));
  int msb = lsb + width - 1;
  emit(cond | 0x1f*B22 | msb*B16 | dst.code()*B12 | lsb*B7 | B4 | 0xf);
}


// Bit field insert.
// Inserts #width adjacent bits from the low bits of the source register
// into position #lsb of the destination register.
//   bfi dst, src, #lsb, #width
void Assembler::bfi(Register dst,
                    Register src,
                    int lsb,
                    int width,
                    Condition cond) {
  // v7 and above.
  ASSERT(CpuFeatures::IsSupported(ARMv7));
  ASSERT(!dst.is(pc) && !src.is(pc));
  ASSERT((lsb >= 0) && (lsb <= 31));
  ASSERT((width >= 1) && (width <= (32 - lsb)));
  int msb = lsb + width - 1;
  emit(cond | 0x1f*B22 | msb*B16 | dst.code()*B12 | lsb*B7 | B4 |
       src.code());
}


// Status register access instructions.
void Assembler::mrs(Register dst, SRegister s, Condition cond) {
  ASSERT(!dst.is(pc));
  emit(cond | B24 | s | 15*B16 | dst.code()*B12);
}


void Assembler::msr(SRegisterFieldMask fields, const Operand& src,
                    Condition cond) {
  ASSERT(fields >= B16 && fields < B20);  // at least one field set
  Instr instr;
  if (!src.rm_.is_valid()) {
    // Immediate.
    uint32_t rotate_imm;
    uint32_t immed_8;
    if (MustUseConstantPool(src.rmode_) ||
        !fits_shifter(src.imm32_, &rotate_imm, &immed_8, NULL)) {
      // Immediate operand cannot be encoded, load it first to register ip.
      RecordRelocInfo(src.rmode_, src.imm32_);
      ldr(ip, MemOperand(pc, 0), cond);
      msr(fields, Operand(ip), cond);
      return;
    }
    instr = I | rotate_imm*B8 | immed_8;
  } else {
    ASSERT(!src.rs_.is_valid() && src.shift_imm_ == 0);  // only rm allowed
    instr = src.rm_.code();
  }
  emit(cond | instr | B24 | B21 | fields | 15*B12);
}


// Load/Store instructions.
void Assembler::ldr(Register dst, const MemOperand& src, Condition cond) {
  if (dst.is(pc)) {
    WriteRecordedPositions();
  }
  addrmod2(cond | B26 | L, dst, src);

  // Eliminate pattern: push(ry), pop(rx)
  //   str(ry, MemOperand(sp, 4, NegPreIndex), al)
  //   ldr(rx, MemOperand(sp, 4, PostIndex), al)
  // Both instructions can be eliminated if ry = rx.
  // If ry != rx, a register copy from ry to rx is inserted
  // after eliminating the push and the pop instructions.
  if (can_peephole_optimize(2)) {
    Instr push_instr = instr_at(pc_ - 2 * kInstrSize);
    Instr pop_instr = instr_at(pc_ - 1 * kInstrSize);

    if (IsPush(push_instr) && IsPop(pop_instr)) {
      if ((pop_instr & kRdMask) != (push_instr & kRdMask)) {
        // For consecutive push and pop on different registers,
        // we delete both the push & pop and insert a register move.
        // push ry, pop rx --> mov rx, ry
        Register reg_pushed, reg_popped;
        reg_pushed = GetRd(push_instr);
        reg_popped = GetRd(pop_instr);
        pc_ -= 2 * kInstrSize;
        // Insert a mov instruction, which is better than a pair of push & pop
        mov(reg_popped, reg_pushed);
        if (FLAG_print_peephole_optimization) {
          PrintF("%x push/pop (diff reg) replaced by a reg move\n",
                 pc_offset());
        }
      } else {
        // For consecutive push and pop on the same register,
        // both the push and the pop can be deleted.
        pc_ -= 2 * kInstrSize;
        if (FLAG_print_peephole_optimization) {
          PrintF("%x push/pop (same reg) eliminated\n", pc_offset());
        }
      }
    }
  }

  if (can_peephole_optimize(2)) {
    Instr str_instr = instr_at(pc_ - 2 * kInstrSize);
    Instr ldr_instr = instr_at(pc_ - 1 * kInstrSize);

    if ((IsStrRegFpOffset(str_instr) &&
         IsLdrRegFpOffset(ldr_instr)) ||
       (IsStrRegFpNegOffset(str_instr) &&
         IsLdrRegFpNegOffset(ldr_instr))) {
      if ((ldr_instr & kLdrStrInstrArgumentMask) ==
            (str_instr & kLdrStrInstrArgumentMask)) {
        // Pattern: Ldr/str same fp+offset, same register.
        //
        // The following:
        // str rx, [fp, #-12]
        // ldr rx, [fp, #-12]
        //
        // Becomes:
        // str rx, [fp, #-12]

        pc_ -= 1 * kInstrSize;
        if (FLAG_print_peephole_optimization) {
          PrintF("%x str/ldr (fp + same offset), same reg\n", pc_offset());
        }
      } else if ((ldr_instr & kLdrStrOffsetMask) ==
                 (str_instr & kLdrStrOffsetMask)) {
        // Pattern: Ldr/str same fp+offset, different register.
        //
        // The following:
        // str rx, [fp, #-12]
        // ldr ry, [fp, #-12]
        //
        // Becomes:
        // str rx, [fp, #-12]
        // mov ry, rx

        Register reg_stored, reg_loaded;
        reg_stored = GetRd(str_instr);
        reg_loaded = GetRd(ldr_instr);
        pc_ -= 1 * kInstrSize;
        // Insert a mov instruction, which is better than ldr.
        mov(reg_loaded, reg_stored);
        if (FLAG_print_peephole_optimization) {
          PrintF("%x str/ldr (fp + same offset), diff reg \n", pc_offset());
        }
      }
    }
  }

  if (can_peephole_optimize(3)) {
    Instr mem_write_instr = instr_at(pc_ - 3 * kInstrSize);
    Instr ldr_instr = instr_at(pc_ - 2 * kInstrSize);
    Instr mem_read_instr = instr_at(pc_ - 1 * kInstrSize);
    if (IsPush(mem_write_instr) &&
        IsPop(mem_read_instr)) {
      if ((IsLdrRegFpOffset(ldr_instr) ||
        IsLdrRegFpNegOffset(ldr_instr))) {
        if ((mem_write_instr & kRdMask) ==
              (mem_read_instr & kRdMask)) {
          // Pattern: push & pop from/to same register,
          // with a fp+offset ldr in between
          //
          // The following:
          // str rx, [sp, #-4]!
          // ldr rz, [fp, #-24]
          // ldr rx, [sp], #+4
          //
          // Becomes:
          // if(rx == rz)
          //   delete all
          // else
          //   ldr rz, [fp, #-24]

          if ((mem_write_instr & kRdMask) == (ldr_instr & kRdMask)) {
            pc_ -= 3 * kInstrSize;
          } else {
            pc_ -= 3 * kInstrSize;
            // Reinsert back the ldr rz.
            emit(ldr_instr);
          }
          if (FLAG_print_peephole_optimization) {
            PrintF("%x push/pop -dead ldr fp+offset in middle\n", pc_offset());
          }
        } else {
          // Pattern: push & pop from/to different registers
          // with a fp+offset ldr in between
          //
          // The following:
          // str rx, [sp, #-4]!
          // ldr rz, [fp, #-24]
          // ldr ry, [sp], #+4
          //
          // Becomes:
          // if(ry == rz)
          //   mov ry, rx;
          // else if(rx != rz)
          //   ldr rz, [fp, #-24]
          //   mov ry, rx
          // else if((ry != rz) || (rx == rz)) becomes:
          //   mov ry, rx
          //   ldr rz, [fp, #-24]

          Register reg_pushed, reg_popped;
          if ((mem_read_instr & kRdMask) == (ldr_instr & kRdMask)) {
            reg_pushed = GetRd(mem_write_instr);
            reg_popped = GetRd(mem_read_instr);
            pc_ -= 3 * kInstrSize;
            mov(reg_popped, reg_pushed);
          } else if ((mem_write_instr & kRdMask)
                                != (ldr_instr & kRdMask)) {
            reg_pushed = GetRd(mem_write_instr);
            reg_popped = GetRd(mem_read_instr);
            pc_ -= 3 * kInstrSize;
            emit(ldr_instr);
            mov(reg_popped, reg_pushed);
          } else if (((mem_read_instr & kRdMask)
                                     != (ldr_instr & kRdMask)) ||
                    ((mem_write_instr & kRdMask)
                                     == (ldr_instr & kRdMask)) ) {
            reg_pushed = GetRd(mem_write_instr);
            reg_popped = GetRd(mem_read_instr);
            pc_ -= 3 * kInstrSize;
            mov(reg_popped, reg_pushed);
            emit(ldr_instr);
          }
          if (FLAG_print_peephole_optimization) {
            PrintF("%x push/pop (ldr fp+off in middle)\n", pc_offset());
          }
        }
      }
    }
  }
}


void Assembler::str(Register src, const MemOperand& dst, Condition cond) {
  addrmod2(cond | B26, src, dst);

  // Eliminate pattern: pop(), push(r)
  //     add sp, sp, #4 LeaveCC, al; str r, [sp, #-4], al
  // ->  str r, [sp, 0], al
  if (can_peephole_optimize(2) &&
     // Pattern.
     instr_at(pc_ - 1 * kInstrSize) == (kPushRegPattern | src.code() * B12) &&
     instr_at(pc_ - 2 * kInstrSize) == kPopInstruction) {
    pc_ -= 2 * kInstrSize;
    emit(al | B26 | 0 | Offset | sp.code() * B16 | src.code() * B12);
    if (FLAG_print_peephole_optimization) {
      PrintF("%x pop()/push(reg) eliminated\n", pc_offset());
    }
  }
}


void Assembler::ldrb(Register dst, const MemOperand& src, Condition cond) {
  addrmod2(cond | B26 | B | L, dst, src);
}


void Assembler::strb(Register src, const MemOperand& dst, Condition cond) {
  addrmod2(cond | B26 | B, src, dst);
}


void Assembler::ldrh(Register dst, const MemOperand& src, Condition cond) {
  addrmod3(cond | L | B7 | H | B4, dst, src);
}


void Assembler::strh(Register src, const MemOperand& dst, Condition cond) {
  addrmod3(cond | B7 | H | B4, src, dst);
}


void Assembler::ldrsb(Register dst, const MemOperand& src, Condition cond) {
  addrmod3(cond | L | B7 | S6 | B4, dst, src);
}


void Assembler::ldrsh(Register dst, const MemOperand& src, Condition cond) {
  addrmod3(cond | L | B7 | S6 | H | B4, dst, src);
}


void Assembler::ldrd(Register dst1, Register dst2,
                     const MemOperand& src, Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(ARMv7));
  ASSERT(src.rm().is(no_reg));
  ASSERT(!dst1.is(lr));  // r14.
  ASSERT_EQ(0, dst1.code() % 2);
  ASSERT_EQ(dst1.code() + 1, dst2.code());
  addrmod3(cond | B7 | B6 | B4, dst1, src);
}


void Assembler::strd(Register src1, Register src2,
                     const MemOperand& dst, Condition cond) {
  ASSERT(dst.rm().is(no_reg));
  ASSERT(!src1.is(lr));  // r14.
  ASSERT_EQ(0, src1.code() % 2);
  ASSERT_EQ(src1.code() + 1, src2.code());
  ASSERT(CpuFeatures::IsEnabled(ARMv7));
  addrmod3(cond | B7 | B6 | B5 | B4, src1, dst);
}

// Load/Store multiple instructions.
void Assembler::ldm(BlockAddrMode am,
                    Register base,
                    RegList dst,
                    Condition cond) {
  // ABI stack constraint: ldmxx base, {..sp..}  base != sp  is not restartable.
  ASSERT(base.is(sp) || (dst & sp.bit()) == 0);

  addrmod4(cond | B27 | am | L, base, dst);

  // Emit the constant pool after a function return implemented by ldm ..{..pc}.
  if (cond == al && (dst & pc.bit()) != 0) {
    // There is a slight chance that the ldm instruction was actually a call,
    // in which case it would be wrong to return into the constant pool; we
    // recognize this case by checking if the emission of the pool was blocked
    // at the pc of the ldm instruction by a mov lr, pc instruction; if this is
    // the case, we emit a jump over the pool.
    CheckConstPool(true, no_const_pool_before_ == pc_offset() - kInstrSize);
  }
}


void Assembler::stm(BlockAddrMode am,
                    Register base,
                    RegList src,
                    Condition cond) {
  addrmod4(cond | B27 | am, base, src);
}


// Exception-generating instructions and debugging support.
void Assembler::stop(const char* msg) {
#ifndef __arm__
  // The simulator handles these special instructions and stops execution.
  emit(15 << 28 | ((intptr_t) msg));
#else  // def __arm__
#ifdef CAN_USE_ARMV5_INSTRUCTIONS
  bkpt(0);
#else  // ndef CAN_USE_ARMV5_INSTRUCTIONS
  swi(0x9f0001);
#endif  // ndef CAN_USE_ARMV5_INSTRUCTIONS
#endif  // def __arm__
}


void Assembler::bkpt(uint32_t imm16) {  // v5 and above
  ASSERT(is_uint16(imm16));
  emit(al | B24 | B21 | (imm16 >> 4)*B8 | 7*B4 | (imm16 & 0xf));
}


void Assembler::swi(uint32_t imm24, Condition cond) {
  ASSERT(is_uint24(imm24));
  emit(cond | 15*B24 | imm24);
}


// Coprocessor instructions.
void Assembler::cdp(Coprocessor coproc,
                    int opcode_1,
                    CRegister crd,
                    CRegister crn,
                    CRegister crm,
                    int opcode_2,
                    Condition cond) {
  ASSERT(is_uint4(opcode_1) && is_uint3(opcode_2));
  emit(cond | B27 | B26 | B25 | (opcode_1 & 15)*B20 | crn.code()*B16 |
       crd.code()*B12 | coproc*B8 | (opcode_2 & 7)*B5 | crm.code());
}


void Assembler::cdp2(Coprocessor coproc,
                     int opcode_1,
                     CRegister crd,
                     CRegister crn,
                     CRegister crm,
                     int opcode_2) {  // v5 and above
  cdp(coproc, opcode_1, crd, crn, crm, opcode_2, static_cast<Condition>(nv));
}


void Assembler::mcr(Coprocessor coproc,
                    int opcode_1,
                    Register rd,
                    CRegister crn,
                    CRegister crm,
                    int opcode_2,
                    Condition cond) {
  ASSERT(is_uint3(opcode_1) && is_uint3(opcode_2));
  emit(cond | B27 | B26 | B25 | (opcode_1 & 7)*B21 | crn.code()*B16 |
       rd.code()*B12 | coproc*B8 | (opcode_2 & 7)*B5 | B4 | crm.code());
}


void Assembler::mcr2(Coprocessor coproc,
                     int opcode_1,
                     Register rd,
                     CRegister crn,
                     CRegister crm,
                     int opcode_2) {  // v5 and above
  mcr(coproc, opcode_1, rd, crn, crm, opcode_2, static_cast<Condition>(nv));
}


void Assembler::mrc(Coprocessor coproc,
                    int opcode_1,
                    Register rd,
                    CRegister crn,
                    CRegister crm,
                    int opcode_2,
                    Condition cond) {
  ASSERT(is_uint3(opcode_1) && is_uint3(opcode_2));
  emit(cond | B27 | B26 | B25 | (opcode_1 & 7)*B21 | L | crn.code()*B16 |
       rd.code()*B12 | coproc*B8 | (opcode_2 & 7)*B5 | B4 | crm.code());
}


void Assembler::mrc2(Coprocessor coproc,
                     int opcode_1,
                     Register rd,
                     CRegister crn,
                     CRegister crm,
                     int opcode_2) {  // v5 and above
  mrc(coproc, opcode_1, rd, crn, crm, opcode_2, static_cast<Condition>(nv));
}


void Assembler::ldc(Coprocessor coproc,
                    CRegister crd,
                    const MemOperand& src,
                    LFlag l,
                    Condition cond) {
  addrmod5(cond | B27 | B26 | l | L | coproc*B8, crd, src);
}


void Assembler::ldc(Coprocessor coproc,
                    CRegister crd,
                    Register rn,
                    int option,
                    LFlag l,
                    Condition cond) {
  // Unindexed addressing.
  ASSERT(is_uint8(option));
  emit(cond | B27 | B26 | U | l | L | rn.code()*B16 | crd.code()*B12 |
       coproc*B8 | (option & 255));
}


void Assembler::ldc2(Coprocessor coproc,
                     CRegister crd,
                     const MemOperand& src,
                     LFlag l) {  // v5 and above
  ldc(coproc, crd, src, l, static_cast<Condition>(nv));
}


void Assembler::ldc2(Coprocessor coproc,
                     CRegister crd,
                     Register rn,
                     int option,
                     LFlag l) {  // v5 and above
  ldc(coproc, crd, rn, option, l, static_cast<Condition>(nv));
}


void Assembler::stc(Coprocessor coproc,
                    CRegister crd,
                    const MemOperand& dst,
                    LFlag l,
                    Condition cond) {
  addrmod5(cond | B27 | B26 | l | coproc*B8, crd, dst);
}


void Assembler::stc(Coprocessor coproc,
                    CRegister crd,
                    Register rn,
                    int option,
                    LFlag l,
                    Condition cond) {
  // Unindexed addressing.
  ASSERT(is_uint8(option));
  emit(cond | B27 | B26 | U | l | rn.code()*B16 | crd.code()*B12 |
       coproc*B8 | (option & 255));
}


void Assembler::stc2(Coprocessor
                     coproc, CRegister crd,
                     const MemOperand& dst,
                     LFlag l) {  // v5 and above
  stc(coproc, crd, dst, l, static_cast<Condition>(nv));
}


void Assembler::stc2(Coprocessor coproc,
                     CRegister crd,
                     Register rn,
                     int option,
                     LFlag l) {  // v5 and above
  stc(coproc, crd, rn, option, l, static_cast<Condition>(nv));
}


// Support for VFP.
void Assembler::vldr(const DwVfpRegister dst,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // Ddst = MEM(Rbase + offset).
  // Instruction details available in ARM DDI 0406A, A8-628.
  // cond(31-28) | 1101(27-24)| 1001(23-20) | Rbase(19-16) |
  // Vdst(15-12) | 1011(11-8) | offset
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(offset % 4 == 0);
  ASSERT((offset / 4) < 256);
  emit(cond | 0xD9*B20 | base.code()*B16 | dst.code()*B12 |
       0xB*B8 | ((offset / 4) & 255));
}


void Assembler::vldr(const SwVfpRegister dst,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // Sdst = MEM(Rbase + offset).
  // Instruction details available in ARM DDI 0406A, A8-628.
  // cond(31-28) | 1101(27-24)| 1001(23-20) | Rbase(19-16) |
  // Vdst(15-12) | 1010(11-8) | offset
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(offset % 4 == 0);
  ASSERT((offset / 4) < 256);
  emit(cond | 0xD9*B20 | base.code()*B16 | dst.code()*B12 |
       0xA*B8 | ((offset / 4) & 255));
}


void Assembler::vstr(const DwVfpRegister src,
                     const Register base,
                     int offset,
                     const Condition cond) {
  // MEM(Rbase + offset) = Dsrc.
  // Instruction details available in ARM DDI 0406A, A8-786.
  // cond(31-28) | 1101(27-24)| 1000(23-20) | | Rbase(19-16) |
  // Vsrc(15-12) | 1011(11-8) | (offset/4)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(offset % 4 == 0);
  ASSERT((offset / 4) < 256);
  emit(cond | 0xD8*B20 | base.code()*B16 | src.code()*B12 |
       0xB*B8 | ((offset / 4) & 255));
}


static void DoubleAsTwoUInt32(double d, uint32_t* lo, uint32_t* hi) {
  uint64_t i;
  memcpy(&i, &d, 8);

  *lo = i & 0xffffffff;
  *hi = i >> 32;
}

// Only works for little endian floating point formats.
// We don't support VFP on the mixed endian floating point platform.
static bool FitsVMOVDoubleImmediate(double d, uint32_t *encoding) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));

  // VMOV can accept an immediate of the form:
  //
  //  +/- m * 2^(-n) where 16 <= m <= 31 and 0 <= n <= 7
  //
  // The immediate is encoded using an 8-bit quantity, comprised of two
  // 4-bit fields. For an 8-bit immediate of the form:
  //
  //  [abcdefgh]
  //
  // where a is the MSB and h is the LSB, an immediate 64-bit double can be
  // created of the form:
  //
  //  [aBbbbbbb,bbcdefgh,00000000,00000000,
  //      00000000,00000000,00000000,00000000]
  //
  // where B = ~b.
  //

  uint32_t lo, hi;
  DoubleAsTwoUInt32(d, &lo, &hi);

  // The most obvious constraint is the long block of zeroes.
  if ((lo != 0) || ((hi & 0xffff) != 0)) {
    return false;
  }

  // Bits 62:55 must be all clear or all set.
  if (((hi & 0x3fc00000) != 0) && ((hi & 0x3fc00000) != 0x3fc00000)) {
    return false;
  }

  // Bit 63 must be NOT bit 62.
  if (((hi ^ (hi << 1)) & (0x40000000)) == 0) {
    return false;
  }

  // Create the encoded immediate in the form:
  //  [00000000,0000abcd,00000000,0000efgh]
  *encoding  = (hi >> 16) & 0xf;      // Low nybble.
  *encoding |= (hi >> 4) & 0x70000;   // Low three bits of the high nybble.
  *encoding |= (hi >> 12) & 0x80000;  // Top bit of the high nybble.

  return true;
}


void Assembler::vmov(const DwVfpRegister dst,
                     double imm,
                     const Condition cond) {
  // Dd = immediate
  // Instruction details available in ARM DDI 0406B, A8-640.
  ASSERT(CpuFeatures::IsEnabled(VFP3));

  uint32_t enc;
  if (FitsVMOVDoubleImmediate(imm, &enc)) {
    // The double can be encoded in the instruction.
    emit(cond | 0xE*B24 | 0xB*B20 | dst.code()*B12 | 0xB*B8 | enc);
  } else {
    // Synthesise the double from ARM immediates. This could be implemented
    // using vldr from a constant pool.
    uint32_t lo, hi;
    DoubleAsTwoUInt32(imm, &lo, &hi);

    if (lo == hi) {
      // If the lo and hi parts of the double are equal, the literal is easier
      // to create. This is the case with 0.0.
      mov(ip, Operand(lo));
      vmov(dst, ip, ip);
    } else {
      // Move the low part of the double into the lower of the corresponsing S
      // registers of D register dst.
      mov(ip, Operand(lo));
      vmov(dst.low(), ip, cond);

      // Move the high part of the double into the higher of the corresponsing S
      // registers of D register dst.
      mov(ip, Operand(hi));
      vmov(dst.high(), ip, cond);
    }
  }
}


void Assembler::vmov(const SwVfpRegister dst,
                     const SwVfpRegister src,
                     const Condition cond) {
  // Sd = Sm
  // Instruction details available in ARM DDI 0406B, A8-642.
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | 0xB*B20 |
       dst.code()*B12 | 0x5*B9 | B6 | src.code());
}


void Assembler::vmov(const DwVfpRegister dst,
                     const DwVfpRegister src,
                     const Condition cond) {
  // Dd = Dm
  // Instruction details available in ARM DDI 0406B, A8-642.
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | 0xB*B20 |
       dst.code()*B12 | 0x5*B9 | B8 | B6 | src.code());
}


void Assembler::vmov(const DwVfpRegister dst,
                     const Register src1,
                     const Register src2,
                     const Condition cond) {
  // Dm = <Rt,Rt2>.
  // Instruction details available in ARM DDI 0406A, A8-646.
  // cond(31-28) | 1100(27-24)| 010(23-21) | op=0(20) | Rt2(19-16) |
  // Rt(15-12) | 1011(11-8) | 00(7-6) | M(5) | 1(4) | Vm
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(!src1.is(pc) && !src2.is(pc));
  emit(cond | 0xC*B24 | B22 | src2.code()*B16 |
       src1.code()*B12 | 0xB*B8 | B4 | dst.code());
}


void Assembler::vmov(const Register dst1,
                     const Register dst2,
                     const DwVfpRegister src,
                     const Condition cond) {
  // <Rt,Rt2> = Dm.
  // Instruction details available in ARM DDI 0406A, A8-646.
  // cond(31-28) | 1100(27-24)| 010(23-21) | op=1(20) | Rt2(19-16) |
  // Rt(15-12) | 1011(11-8) | 00(7-6) | M(5) | 1(4) | Vm
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(!dst1.is(pc) && !dst2.is(pc));
  emit(cond | 0xC*B24 | B22 | B20 | dst2.code()*B16 |
       dst1.code()*B12 | 0xB*B8 | B4 | src.code());
}


void Assembler::vmov(const SwVfpRegister dst,
                     const Register src,
                     const Condition cond) {
  // Sn = Rt.
  // Instruction details available in ARM DDI 0406A, A8-642.
  // cond(31-28) | 1110(27-24)| 000(23-21) | op=0(20) | Vn(19-16) |
  // Rt(15-12) | 1010(11-8) | N(7)=0 | 00(6-5) | 1(4) | 0000(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(!src.is(pc));
  emit(cond | 0xE*B24 | (dst.code() >> 1)*B16 |
       src.code()*B12 | 0xA*B8 | (0x1 & dst.code())*B7 | B4);
}


void Assembler::vmov(const Register dst,
                     const SwVfpRegister src,
                     const Condition cond) {
  // Rt = Sn.
  // Instruction details available in ARM DDI 0406A, A8-642.
  // cond(31-28) | 1110(27-24)| 000(23-21) | op=1(20) | Vn(19-16) |
  // Rt(15-12) | 1010(11-8) | N(7)=0 | 00(6-5) | 1(4) | 0000(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  ASSERT(!dst.is(pc));
  emit(cond | 0xE*B24 | B20 | (src.code() >> 1)*B16 |
       dst.code()*B12 | 0xA*B8 | (0x1 & src.code())*B7 | B4);
}


// Type of data to read from or write to VFP register.
// Used as specifier in generic vcvt instruction.
enum VFPType { S32, U32, F32, F64 };


static bool IsSignedVFPType(VFPType type) {
  switch (type) {
    case S32:
      return true;
    case U32:
      return false;
    default:
      UNREACHABLE();
      return false;
  }
}


static bool IsIntegerVFPType(VFPType type) {
  switch (type) {
    case S32:
    case U32:
      return true;
    case F32:
    case F64:
      return false;
    default:
      UNREACHABLE();
      return false;
  }
}


static bool IsDoubleVFPType(VFPType type) {
  switch (type) {
    case F32:
      return false;
    case F64:
      return true;
    default:
      UNREACHABLE();
      return false;
  }
}


// Depending on split_last_bit split binary representation of reg_code into Vm:M
// or M:Vm form (where M is single bit).
static void SplitRegCode(bool split_last_bit,
                         int reg_code,
                         int* vm,
                         int* m) {
  if (split_last_bit) {
    *m  = reg_code & 0x1;
    *vm = reg_code >> 1;
  } else {
    *m  = (reg_code & 0x10) >> 4;
    *vm = reg_code & 0x0F;
  }
}


// Encode vcvt.src_type.dst_type instruction.
static Instr EncodeVCVT(const VFPType dst_type,
                        const int dst_code,
                        const VFPType src_type,
                        const int src_code,
                        const Condition cond) {
  if (IsIntegerVFPType(dst_type) || IsIntegerVFPType(src_type)) {
    // Conversion between IEEE floating point and 32-bit integer.
    // Instruction details available in ARM DDI 0406B, A8.6.295.
    // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 1(19) | opc2(18-16) |
    // Vd(15-12) | 101(11-9) | sz(8) | op(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
    ASSERT(!IsIntegerVFPType(dst_type) || !IsIntegerVFPType(src_type));

    int sz, opc2, D, Vd, M, Vm, op;

    if (IsIntegerVFPType(dst_type)) {
      opc2 = IsSignedVFPType(dst_type) ? 0x5 : 0x4;
      sz = IsDoubleVFPType(src_type) ? 0x1 : 0x0;
      op = 1;  // round towards zero
      SplitRegCode(!IsDoubleVFPType(src_type), src_code, &Vm, &M);
      SplitRegCode(true, dst_code, &Vd, &D);
    } else {
      ASSERT(IsIntegerVFPType(src_type));

      opc2 = 0x0;
      sz = IsDoubleVFPType(dst_type) ? 0x1 : 0x0;
      op = IsSignedVFPType(src_type) ? 0x1 : 0x0;
      SplitRegCode(true, src_code, &Vm, &M);
      SplitRegCode(!IsDoubleVFPType(dst_type), dst_code, &Vd, &D);
    }

    return (cond | 0xE*B24 | B23 | D*B22 | 0x3*B20 | B19 | opc2*B16 |
            Vd*B12 | 0x5*B9 | sz*B8 | op*B7 | B6 | M*B5 | Vm);
  } else {
    // Conversion between IEEE double and single precision.
    // Instruction details available in ARM DDI 0406B, A8.6.298.
    // cond(31-28) | 11101(27-23)| D(22) | 11(21-20) | 0111(19-16) |
    // Vd(15-12) | 101(11-9) | sz(8) | 1(7) | 1(6) | M(5) | 0(4) | Vm(3-0)
    int sz, D, Vd, M, Vm;

    ASSERT(IsDoubleVFPType(dst_type) != IsDoubleVFPType(src_type));
    sz = IsDoubleVFPType(src_type) ? 0x1 : 0x0;
    SplitRegCode(IsDoubleVFPType(src_type), dst_code, &Vd, &D);
    SplitRegCode(!IsDoubleVFPType(src_type), src_code, &Vm, &M);

    return (cond | 0xE*B24 | B23 | D*B22 | 0x3*B20 | 0x7*B16 |
            Vd*B12 | 0x5*B9 | sz*B8 | B7 | B6 | M*B5 | Vm);
  }
}


void Assembler::vcvt_f64_s32(const DwVfpRegister dst,
                             const SwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(F64, dst.code(), S32, src.code(), cond));
}


void Assembler::vcvt_f32_s32(const SwVfpRegister dst,
                             const SwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(F32, dst.code(), S32, src.code(), cond));
}


void Assembler::vcvt_f64_u32(const DwVfpRegister dst,
                             const SwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(F64, dst.code(), U32, src.code(), cond));
}


void Assembler::vcvt_s32_f64(const SwVfpRegister dst,
                             const DwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(S32, dst.code(), F64, src.code(), cond));
}


void Assembler::vcvt_u32_f64(const SwVfpRegister dst,
                             const DwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(U32, dst.code(), F64, src.code(), cond));
}


void Assembler::vcvt_f64_f32(const DwVfpRegister dst,
                             const SwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(F64, dst.code(), F32, src.code(), cond));
}


void Assembler::vcvt_f32_f64(const SwVfpRegister dst,
                             const DwVfpRegister src,
                             const Condition cond) {
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(EncodeVCVT(F32, dst.code(), F64, src.code(), cond));
}


void Assembler::vadd(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vadd(Dn, Dm) double precision floating point addition.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406A, A8-536.
  // cond(31-28) | 11100(27-23)| D=?(22) | 11(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz(8)=1 | N(7)=0 | 0(6) | M=?(5) | 0(4) | Vm(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | 0x3*B20 | src1.code()*B16 |
       dst.code()*B12 | 0x5*B9 | B8 | src2.code());
}


void Assembler::vsub(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vsub(Dn, Dm) double precision floating point subtraction.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406A, A8-784.
  // cond(31-28) | 11100(27-23)| D=?(22) | 11(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz(8)=1 | N(7)=0 | 1(6) | M=?(5) | 0(4) | Vm(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | 0x3*B20 | src1.code()*B16 |
       dst.code()*B12 | 0x5*B9 | B8 | B6 | src2.code());
}


void Assembler::vmul(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vmul(Dn, Dm) double precision floating point multiplication.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406A, A8-784.
  // cond(31-28) | 11100(27-23)| D=?(22) | 10(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz(8)=1 | N(7)=0 | 0(6) | M=?(5) | 0(4) | Vm(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | 0x2*B20 | src1.code()*B16 |
       dst.code()*B12 | 0x5*B9 | B8 | src2.code());
}


void Assembler::vdiv(const DwVfpRegister dst,
                     const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const Condition cond) {
  // Dd = vdiv(Dn, Dm) double precision floating point division.
  // Dd = D:Vd; Dm=M:Vm; Dn=N:Vm.
  // Instruction details available in ARM DDI 0406A, A8-584.
  // cond(31-28) | 11101(27-23)| D=?(22) | 00(21-20) | Vn(19-16) |
  // Vd(15-12) | 101(11-9) | sz(8)=1 | N(7)=? | 0(6) | M=?(5) | 0(4) | Vm(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | B23 | src1.code()*B16 |
       dst.code()*B12 | 0x5*B9 | B8 | src2.code());
}


void Assembler::vcmp(const DwVfpRegister src1,
                     const DwVfpRegister src2,
                     const SBit s,
                     const Condition cond) {
  // vcmp(Dd, Dm) double precision floating point comparison.
  // Instruction details available in ARM DDI 0406A, A8-570.
  // cond(31-28) | 11101 (27-23)| D=?(22) | 11 (21-20) | 0100 (19-16) |
  // Vd(15-12) | 101(11-9) | sz(8)=1 | E(7)=? | 1(6) | M(5)=? | 0(4) | Vm(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 |B23 | 0x3*B20 | B18 |
       src1.code()*B12 | 0x5*B9 | B8 | B6 | src2.code());
}


void Assembler::vmrs(Register dst, Condition cond) {
  // Instruction details available in ARM DDI 0406A, A8-652.
  // cond(31-28) | 1110 (27-24) | 1111(23-20)| 0001 (19-16) |
  // Rt(15-12) | 1010 (11-8) | 0(7) | 00 (6-5) | 1(4) | 0000(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | 0xF*B20 |  B16 |
       dst.code()*B12 | 0xA*B8 | B4);
}



void Assembler::vsqrt(const DwVfpRegister dst,
                      const DwVfpRegister src,
                      const Condition cond) {
  // cond(31-28) | 11101 (27-23)| D=?(22) | 11 (21-20) | 0001 (19-16) |
  // Vd(15-12) | 101(11-9) | sz(8)=1 | 11 (7-6) | M(5)=? | 0(4) | Vm(3-0)
  ASSERT(CpuFeatures::IsEnabled(VFP3));
  emit(cond | 0xE*B24 | B23 | 0x3*B20 | B16 |
       dst.code()*B12 | 0x5*B9 | B8 | 3*B6 | src.code());
}


// Pseudo instructions.
void Assembler::nop(int type) {
  // This is mov rx, rx.
  ASSERT(0 <= type && type <= 14);  // mov pc, pc is not a nop.
  emit(al | 13*B21 | type*B12 | type);
}


bool Assembler::ImmediateFitsAddrMode1Instruction(int32_t imm32) {
  uint32_t dummy1;
  uint32_t dummy2;
  return fits_shifter(imm32, &dummy1, &dummy2, NULL);
}


void Assembler::BlockConstPoolFor(int instructions) {
  BlockConstPoolBefore(pc_offset() + instructions * kInstrSize);
}


// Debugging.
void Assembler::RecordJSReturn() {
  WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::JS_RETURN);
}


void Assembler::RecordDebugBreakSlot() {
  WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::DEBUG_BREAK_SLOT);
}


void Assembler::RecordComment(const char* msg) {
  if (FLAG_debug_code) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::COMMENT, reinterpret_cast<intptr_t>(msg));
  }
}


void Assembler::RecordPosition(int pos) {
  if (pos == RelocInfo::kNoPosition) return;
  ASSERT(pos >= 0);
  current_position_ = pos;
}


void Assembler::RecordStatementPosition(int pos) {
  if (pos == RelocInfo::kNoPosition) return;
  ASSERT(pos >= 0);
  current_statement_position_ = pos;
}


bool Assembler::WriteRecordedPositions() {
  bool written = false;

  // Write the statement position if it is different from what was written last
  // time.
  if (current_statement_position_ != written_statement_position_) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::STATEMENT_POSITION, current_statement_position_);
    written_statement_position_ = current_statement_position_;
    written = true;
  }

  // Write the position if it is different from what was written last time and
  // also different from the written statement position.
  if (current_position_ != written_position_ &&
      current_position_ != written_statement_position_) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::POSITION, current_position_);
    written_position_ = current_position_;
    written = true;
  }

  // Return whether something was written.
  return written;
}


void Assembler::GrowBuffer() {
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // the new buffer
  if (buffer_size_ < 4*KB) {
    desc.buffer_size = 4*KB;
  } else if (buffer_size_ < 1*MB) {
    desc.buffer_size = 2*buffer_size_;
  } else {
    desc.buffer_size = buffer_size_ + 1*MB;
  }
  CHECK_GT(desc.buffer_size, 0);  // no overflow

  // Setup new buffer.
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

  // None of our relocation types are pc relative pointing outside the code
  // buffer nor pc absolute pointing inside the code buffer, so there is no need
  // to relocate any emitted relocation entries.

  // Relocate pending relocation entries.
  for (int i = 0; i < num_prinfo_; i++) {
    RelocInfo& rinfo = prinfo_[i];
    ASSERT(rinfo.rmode() != RelocInfo::COMMENT &&
           rinfo.rmode() != RelocInfo::POSITION);
    if (rinfo.rmode() != RelocInfo::JS_RETURN) {
      rinfo.set_pc(rinfo.pc() + pc_delta);
    }
  }
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  RelocInfo rinfo(pc_, rmode, data);  // we do not try to reuse pool constants
  if (rmode >= RelocInfo::JS_RETURN && rmode <= RelocInfo::DEBUG_BREAK_SLOT) {
    // Adjust code for new modes.
    ASSERT(RelocInfo::IsDebugBreakSlot(rmode)
           || RelocInfo::IsJSReturn(rmode)
           || RelocInfo::IsComment(rmode)
           || RelocInfo::IsPosition(rmode));
    // These modes do not need an entry in the constant pool.
  } else {
    ASSERT(num_prinfo_ < kMaxNumPRInfo);
    prinfo_[num_prinfo_++] = rinfo;
    // Make sure the constant pool is not emitted in place of the next
    // instruction for which we just recorded relocation info.
    BlockConstPoolBefore(pc_offset() + kInstrSize);
  }
  if (rinfo.rmode() != RelocInfo::NONE) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
#ifdef DEBUG
      if (!Serializer::enabled()) {
        Serializer::TooLateToEnableNow();
      }
#endif
      if (!Serializer::enabled() && !FLAG_debug_code) {
        return;
      }
    }
    ASSERT(buffer_space() >= kMaxRelocSize);  // too late to grow buffer here
    reloc_info_writer.Write(&rinfo);
  }
}


void Assembler::CheckConstPool(bool force_emit, bool require_jump) {
  // Calculate the offset of the next check. It will be overwritten
  // when a const pool is generated or when const pools are being
  // blocked for a specific range.
  next_buffer_check_ = pc_offset() + kCheckConstInterval;

  // There is nothing to do if there are no pending relocation info entries.
  if (num_prinfo_ == 0) return;

  // We emit a constant pool at regular intervals of about kDistBetweenPools
  // or when requested by parameter force_emit (e.g. after each function).
  // We prefer not to emit a jump unless the max distance is reached or if we
  // are running low on slots, which can happen if a lot of constants are being
  // emitted (e.g. --debug-code and many static references).
  int dist = pc_offset() - last_const_pool_end_;
  if (!force_emit && dist < kMaxDistBetweenPools &&
      (require_jump || dist < kDistBetweenPools) &&
      // TODO(1236125): Cleanup the "magic" number below. We know that
      // the code generation will test every kCheckConstIntervalInst.
      // Thus we are safe as long as we generate less than 7 constant
      // entries per instruction.
      (num_prinfo_ < (kMaxNumPRInfo - (7 * kCheckConstIntervalInst)))) {
    return;
  }

  // If we did not return by now, we need to emit the constant pool soon.

  // However, some small sequences of instructions must not be broken up by the
  // insertion of a constant pool; such sequences are protected by setting
  // either const_pool_blocked_nesting_ or no_const_pool_before_, which are
  // both checked here. Also, recursive calls to CheckConstPool are blocked by
  // no_const_pool_before_.
  if (const_pool_blocked_nesting_ > 0 || pc_offset() < no_const_pool_before_) {
    // Emission is currently blocked; make sure we try again as soon as
    // possible.
    if (const_pool_blocked_nesting_ > 0) {
      next_buffer_check_ = pc_offset() + kInstrSize;
    } else {
      next_buffer_check_ = no_const_pool_before_;
    }

    // Something is wrong if emission is forced and blocked at the same time.
    ASSERT(!force_emit);
    return;
  }

  int jump_instr = require_jump ? kInstrSize : 0;

  // Check that the code buffer is large enough before emitting the constant
  // pool and relocation information (include the jump over the pool and the
  // constant pool marker).
  int max_needed_space =
      jump_instr + kInstrSize + num_prinfo_*(kInstrSize + kMaxRelocSize);
  while (buffer_space() <= (max_needed_space + kGap)) GrowBuffer();

  // Block recursive calls to CheckConstPool.
  BlockConstPoolBefore(pc_offset() + jump_instr + kInstrSize +
                       num_prinfo_*kInstrSize);
  // Don't bother to check for the emit calls below.
  next_buffer_check_ = no_const_pool_before_;

  // Emit jump over constant pool if necessary.
  Label after_pool;
  if (require_jump) b(&after_pool);

  RecordComment("[ Constant Pool");

  // Put down constant pool marker "Undefined instruction" as specified by
  // A3.1 Instruction set encoding.
  emit(0x03000000 | num_prinfo_);

  // Emit constant pool entries.
  for (int i = 0; i < num_prinfo_; i++) {
    RelocInfo& rinfo = prinfo_[i];
    ASSERT(rinfo.rmode() != RelocInfo::COMMENT &&
           rinfo.rmode() != RelocInfo::POSITION &&
           rinfo.rmode() != RelocInfo::STATEMENT_POSITION);
    Instr instr = instr_at(rinfo.pc());

    // Instruction to patch must be a ldr/str [pc, #offset].
    // P and U set, B and W clear, Rn == pc, offset12 still 0.
    ASSERT((instr & (7*B25 | P | U | B | W | 15*B16 | Off12Mask)) ==
           (2*B25 | P | U | pc.code()*B16));
    int delta = pc_ - rinfo.pc() - 8;
    ASSERT(delta >= -4);  // instr could be ldr pc, [pc, #-4] followed by targ32
    if (delta < 0) {
      instr &= ~U;
      delta = -delta;
    }
    ASSERT(is_uint12(delta));
    instr_at_put(rinfo.pc(), instr + delta);
    emit(rinfo.data());
  }
  num_prinfo_ = 0;
  last_const_pool_end_ = pc_offset();

  RecordComment("]");

  if (after_pool.is_linked()) {
    bind(&after_pool);
  }

  // Since a constant pool was just emitted, move the check offset forward by
  // the standard interval.
  next_buffer_check_ = pc_offset() + kCheckConstInterval;
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
