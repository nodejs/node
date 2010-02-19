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
// Copyright 2010 the V8 project authors. All rights reserved.


#include "v8.h"
#include "mips/assembler-mips-inl.h"
#include "serialize.h"


namespace v8 {
namespace internal {



const Register no_reg = { -1 };

const Register zero_reg = { 0 };
const Register at = { 1 };
const Register v0 = { 2 };
const Register v1 = { 3 };
const Register a0 = { 4 };
const Register a1 = { 5 };
const Register a2 = { 6 };
const Register a3 = { 7 };
const Register t0 = { 8 };
const Register t1 = { 9 };
const Register t2 = { 10 };
const Register t3 = { 11 };
const Register t4 = { 12 };
const Register t5 = { 13 };
const Register t6 = { 14 };
const Register t7 = { 15 };
const Register s0 = { 16 };
const Register s1 = { 17 };
const Register s2 = { 18 };
const Register s3 = { 19 };
const Register s4 = { 20 };
const Register s5 = { 21 };
const Register s6 = { 22 };
const Register s7 = { 23 };
const Register t8 = { 24 };
const Register t9 = { 25 };
const Register k0 = { 26 };
const Register k1 = { 27 };
const Register gp = { 28 };
const Register sp = { 29 };
const Register s8_fp = { 30 };
const Register ra = { 31 };


const FPURegister no_creg = { -1 };

const FPURegister f0 = { 0 };
const FPURegister f1 = { 1 };
const FPURegister f2 = { 2 };
const FPURegister f3 = { 3 };
const FPURegister f4 = { 4 };
const FPURegister f5 = { 5 };
const FPURegister f6 = { 6 };
const FPURegister f7 = { 7 };
const FPURegister f8 = { 8 };
const FPURegister f9 = { 9 };
const FPURegister f10 = { 10 };
const FPURegister f11 = { 11 };
const FPURegister f12 = { 12 };
const FPURegister f13 = { 13 };
const FPURegister f14 = { 14 };
const FPURegister f15 = { 15 };
const FPURegister f16 = { 16 };
const FPURegister f17 = { 17 };
const FPURegister f18 = { 18 };
const FPURegister f19 = { 19 };
const FPURegister f20 = { 20 };
const FPURegister f21 = { 21 };
const FPURegister f22 = { 22 };
const FPURegister f23 = { 23 };
const FPURegister f24 = { 24 };
const FPURegister f25 = { 25 };
const FPURegister f26 = { 26 };
const FPURegister f27 = { 27 };
const FPURegister f28 = { 28 };
const FPURegister f29 = { 29 };
const FPURegister f30 = { 30 };
const FPURegister f31 = { 31 };

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
    30,   // s8_fp
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
    s8_fp,
    ra
  };
  return kRegisters[num];
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo.

const int RelocInfo::kApplyMask = 0;

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
  ASSERT(!Heap::InNewSpace(obj));
  if (obj->IsHeapObject()) {
    imm32_ = reinterpret_cast<intptr_t>(handle.location());
    rmode_ = RelocInfo::EMBEDDED_OBJECT;
  } else {
    // No relocation needed.
    imm32_ = reinterpret_cast<intptr_t>(obj);
    rmode_ = RelocInfo::NONE;
  }
}

MemOperand::MemOperand(Register rm, int16_t offset) : Operand(rm) {
  offset_ = offset;
}


// -----------------------------------------------------------------------------
// Implementation of Assembler.

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
  current_statement_position_ = RelocInfo::kNoPosition;
  current_position_ = RelocInfo::kNoPosition;
  written_statement_position_ = current_statement_position_;
  written_position_ = current_position_;
}


Assembler::~Assembler() {
  if (own_buffer_) {
    if (spare_buffer_ == NULL && buffer_size_ == kMinimalBufferSize) {
      spare_buffer_ = buffer_;
    } else {
      DeleteArray(buffer_);
    }
  }
}


void Assembler::GetCode(CodeDesc* desc) {
  ASSERT(pc_ <= reloc_info_writer.pos());  // no overlap
  // Setup code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
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


// The link chain is terminated by a negative code position (must be aligned).
const int kEndOfChain = -4;

bool Assembler::is_branch(Instr instr) {
  uint32_t opcode   = ((instr & kOpcodeMask));
  uint32_t rt_field = ((instr & kRtFieldMask));
  uint32_t rs_field = ((instr & kRsFieldMask));
  // Checks if the instruction is a branch.
  return opcode == BEQ ||
      opcode == BNE ||
      opcode == BLEZ ||
      opcode == BGTZ ||
      opcode == BEQL ||
      opcode == BNEL ||
      opcode == BLEZL ||
      opcode == BGTZL||
      (opcode == REGIMM && (rt_field == BLTZ || rt_field == BGEZ ||
                            rt_field == BLTZAL || rt_field == BGEZAL)) ||
      (opcode == COP1 && rs_field == BC1);  // Coprocessor branch.
}


int Assembler::target_at(int32_t pos) {
  Instr instr = instr_at(pos);
  if ((instr & ~kImm16Mask) == 0) {
    // Emitted label constant, not part of a branch.
    return instr - (Code::kHeaderSize - kHeapObjectTag);
  }
  // Check we have a branch instruction.
  ASSERT(is_branch(instr));
  // Do NOT change this to <<2. We rely on arithmetic shifts here, assuming
  // the compiler uses arithmectic shifts for signed integers.
  int32_t imm18 = ((instr &
                    static_cast<int32_t>(kImm16Mask)) << 16) >> 14;

  return pos + kBranchPCOffset + imm18;
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

  ASSERT(is_branch(instr));
  int32_t imm18 = target_pos - (pos + kBranchPCOffset);
  ASSERT((imm18 & 3) == 0);

  instr &= ~kImm16Mask;
  int32_t imm16 = imm18 >> 2;
  ASSERT(is_int16(imm16));

  instr_at_put(pos, instr | (imm16 & kImm16Mask));
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
  ASSERT(0 <= pos && pos <= pc_offset());  // must have a valid binding position
  while (L->is_linked()) {
    int32_t fixup_pos = L->pos();
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
      // L is empty, simply use appendix
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


// We have to use a temporary register for things that can be relocated even
// if they can be encoded in the MIPS's 16 bits of immediate-offset instruction
// space.  There is no guarantee that the relocated location can be similarly
// encoded.
bool Assembler::MustUseAt(RelocInfo::Mode rmode) {
  if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
    return Serializer::enabled();
  } else if (rmode == RelocInfo::NONE) {
    return false;
  }
  return true;
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
                                 SecondaryField fmt,
                                 FPURegister ft,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  ASSERT(fd.is_valid() && fs.is_valid() && ft.is_valid());
  Instr instr = opcode | fmt | (ft.code() << 16) | (fs.code() << kFsShift)
      | (fd.code() << 6) | func;
  emit(instr);
}


void Assembler::GenInstrRegister(Opcode opcode,
                                 SecondaryField fmt,
                                 Register rt,
                                 FPURegister fs,
                                 FPURegister fd,
                                 SecondaryField func) {
  ASSERT(fd.is_valid() && fs.is_valid() && rt.is_valid());
  Instr instr = opcode | fmt | (rt.code() << kRtShift)
      | (fs.code() << kFsShift) | (fd.code() << 6) | func;
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
  Instr instr = opcode | (rs.code() << kRsShift) | (ft.code() << kFtShift)
      | (j & kImm16Mask);
  emit(instr);
}


// Registers are in the order of the instruction encoding, from left to right.
void Assembler::GenInstrJump(Opcode opcode,
                              uint32_t address) {
  ASSERT(is_uint26(address));
  Instr instr = opcode | address;
  emit(instr);
}


int32_t Assembler::branch_offset(Label* L, bool jump_elimination_allowed) {
  int32_t target_pos;
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

  int32_t offset = target_pos - (pc_offset() + kBranchPCOffset);
  return offset;
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


//------- Branch and jump instructions --------

void Assembler::b(int16_t offset) {
  beq(zero_reg, zero_reg, offset);
}


void Assembler::bal(int16_t offset) {
  bgezal(zero_reg, offset);
}


void Assembler::beq(Register rs, Register rt, int16_t offset) {
  GenInstrImmediate(BEQ, rs, rt, offset);
}


void Assembler::bgez(Register rs, int16_t offset) {
  GenInstrImmediate(REGIMM, rs, BGEZ, offset);
}


void Assembler::bgezal(Register rs, int16_t offset) {
  GenInstrImmediate(REGIMM, rs, BGEZAL, offset);
}


void Assembler::bgtz(Register rs, int16_t offset) {
  GenInstrImmediate(BGTZ, rs, zero_reg, offset);
}


void Assembler::blez(Register rs, int16_t offset) {
  GenInstrImmediate(BLEZ, rs, zero_reg, offset);
}


void Assembler::bltz(Register rs, int16_t offset) {
  GenInstrImmediate(REGIMM, rs, BLTZ, offset);
}


void Assembler::bltzal(Register rs, int16_t offset) {
  GenInstrImmediate(REGIMM, rs, BLTZAL, offset);
}


void Assembler::bne(Register rs, Register rt, int16_t offset) {
  GenInstrImmediate(BNE, rs, rt, offset);
}


void Assembler::j(int32_t target) {
  ASSERT(is_uint28(target) && ((target & 3) == 0));
  GenInstrJump(J, target >> 2);
}


void Assembler::jr(Register rs) {
  GenInstrRegister(SPECIAL, rs, zero_reg, zero_reg, 0, JR);
}


void Assembler::jal(int32_t target) {
  ASSERT(is_uint28(target) && ((target & 3) == 0));
  GenInstrJump(JAL, target >> 2);
}


void Assembler::jalr(Register rs, Register rd) {
  GenInstrRegister(SPECIAL, rs, zero_reg, rd, 0, JALR);
}


//-------Data-processing-instructions---------

// Arithmetic.

void Assembler::add(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, ADD);
}


void Assembler::addu(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, ADDU);
}


void Assembler::addi(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(ADDI, rs, rd, j);
}


void Assembler::addiu(Register rd, Register rs, int32_t j) {
  GenInstrImmediate(ADDIU, rs, rd, j);
}


void Assembler::sub(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, SUB);
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
  GenInstrImmediate(ANDI, rs, rt, j);
}


void Assembler::or_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, OR);
}


void Assembler::ori(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(ORI, rs, rt, j);
}


void Assembler::xor_(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, XOR);
}


void Assembler::xori(Register rt, Register rs, int32_t j) {
  GenInstrImmediate(XORI, rs, rt, j);
}


void Assembler::nor(Register rd, Register rs, Register rt) {
  GenInstrRegister(SPECIAL, rs, rt, rd, 0, NOR);
}


// Shifts.
void Assembler::sll(Register rd, Register rt, uint16_t sa) {
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


//------------Memory-instructions-------------

void Assembler::lb(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LB, rs.rm(), rd, rs.offset_);
}


void Assembler::lbu(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LBU, rs.rm(), rd, rs.offset_);
}


void Assembler::lw(Register rd, const MemOperand& rs) {
  GenInstrImmediate(LW, rs.rm(), rd, rs.offset_);
}


void Assembler::sb(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SB, rs.rm(), rd, rs.offset_);
}


void Assembler::sw(Register rd, const MemOperand& rs) {
  GenInstrImmediate(SW, rs.rm(), rd, rs.offset_);
}


void Assembler::lui(Register rd, int32_t j) {
  GenInstrImmediate(LUI, zero_reg, rd, j);
}


//-------------Misc-instructions--------------

// Break / Trap instructions.
void Assembler::break_(uint32_t code) {
  ASSERT((code & ~0xfffff) == 0);
  Instr break_instr = SPECIAL | BREAK | (code << 6);
  emit(break_instr);
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
  Instr instr = SPECIAL | TLTU | rs.code() << kRsShift
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


//--------Coprocessor-instructions----------------

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


void Assembler::mtc1(FPURegister fs, Register rt) {
  GenInstrRegister(COP1, MTC1, rt, fs, f0);
}


void Assembler::mthc1(FPURegister fs, Register rt) {
  GenInstrRegister(COP1, MTHC1, rt, fs, f0);
}


void Assembler::mfc1(FPURegister fs, Register rt) {
  GenInstrRegister(COP1, MFC1, rt, fs, f0);
}


void Assembler::mfhc1(FPURegister fs, Register rt) {
  GenInstrRegister(COP1, MFHC1, rt, fs, f0);
}


// Conversions.

void Assembler::cvt_w_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_W_S);
}


void Assembler::cvt_w_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_W_D);
}


void Assembler::cvt_l_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_L_S);
}


void Assembler::cvt_l_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_L_D);
}


void Assembler::cvt_s_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_S_W);
}


void Assembler::cvt_s_l(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_S_L);
}


void Assembler::cvt_s_d(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, D, f0, fs, fd, CVT_S_D);
}


void Assembler::cvt_d_w(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, W, f0, fs, fd, CVT_D_W);
}


void Assembler::cvt_d_l(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, L, f0, fs, fd, CVT_D_L);
}


void Assembler::cvt_d_s(FPURegister fd, FPURegister fs) {
  GenInstrRegister(COP1, S, f0, fs, fd, CVT_D_S);
}


// Conditions.
void Assembler::c(FPUCondition cond, SecondaryField fmt,
    FPURegister ft, FPURegister fs, uint16_t cc) {
  ASSERT(is_uint3(cc));
  ASSERT((fmt & ~(31 << kRsShift)) == 0);
  Instr instr = COP1 | fmt | ft.code() << 16 | fs.code() << kFsShift
      | cc << 8 | 3 << 4 | cond;
  emit(instr);
}


void Assembler::bc1f(int16_t offset, uint16_t cc) {
  ASSERT(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 0 << 16 | (offset & kImm16Mask);
  emit(instr);
}


void Assembler::bc1t(int16_t offset, uint16_t cc) {
  ASSERT(is_uint3(cc));
  Instr instr = COP1 | BC1 | cc << 18 | 1 << 16 | (offset & kImm16Mask);
  emit(instr);
}


// Debugging.
void Assembler::RecordJSReturn() {
  WriteRecordedPositions();
  CheckBuffer();
  RecordRelocInfo(RelocInfo::JS_RETURN);
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


void Assembler::WriteRecordedPositions() {
  // Write the statement position if it is different from what was written last
  // time.
  if (current_statement_position_ != written_statement_position_) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::STATEMENT_POSITION, current_statement_position_);
    written_statement_position_ = current_statement_position_;
  }

  // Write the position if it is different from what was written last time and
  // also different from the written statement position.
  if (current_position_ != written_position_ &&
      current_position_ != written_statement_position_) {
    CheckBuffer();
    RecordRelocInfo(RelocInfo::POSITION, current_position_);
    written_position_ = current_position_;
  }
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


  // On ia32 and ARM pc relative addressing is used, and we thus need to apply a
  // shift by pc_delta. But on MIPS the target address it directly loaded, so
  // we do not need to relocate here.

  ASSERT(!overflow());
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  RelocInfo rinfo(pc_, rmode, data);  // we do not try to reuse pool constants
  if (rmode >= RelocInfo::JS_RETURN && rmode <= RelocInfo::STATEMENT_POSITION) {
    // Adjust code for new modes.
    ASSERT(RelocInfo::IsJSReturn(rmode)
           || RelocInfo::IsComment(rmode)
           || RelocInfo::IsPosition(rmode));
    // These modes do not need an entry in the constant pool.
  }
  if (rinfo.rmode() != RelocInfo::NONE) {
    // Don't record external references unless the heap will be serialized.
    if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
        !Serializer::enabled() &&
        !FLAG_debug_code) {
      return;
    }
    ASSERT(buffer_space() >= kMaxRelocSize);  // too late to grow buffer here
    reloc_info_writer.Write(&rinfo);
  }
}


Address Assembler::target_address_at(Address pc) {
  Instr instr1 = instr_at(pc);
  Instr instr2 = instr_at(pc + kInstrSize);
  // Check we have 2 instructions generated by li.
  ASSERT(((instr1 & kOpcodeMask) == LUI && (instr2 & kOpcodeMask) == ORI) ||
         ((instr1 == nopInstr) && ((instr2 & kOpcodeMask) == ADDI ||
                            (instr2 & kOpcodeMask) == ORI ||
                            (instr2 & kOpcodeMask) == LUI)));
  // Interpret these 2 instructions.
  if (instr1 == nopInstr) {
    if ((instr2 & kOpcodeMask) == ADDI) {
      return reinterpret_cast<Address>(((instr2 & kImm16Mask) << 16) >> 16);
    } else if ((instr2 & kOpcodeMask) == ORI) {
      return reinterpret_cast<Address>(instr2 & kImm16Mask);
    } else if ((instr2 & kOpcodeMask) == LUI) {
      return reinterpret_cast<Address>((instr2 & kImm16Mask) << 16);
    }
  } else if ((instr1 & kOpcodeMask) == LUI && (instr2 & kOpcodeMask) == ORI) {
    // 32 bits value.
    return reinterpret_cast<Address>(
        (instr1 & kImm16Mask) << 16 | (instr2 & kImm16Mask));
  }

  // We should never get here.
  UNREACHABLE();
  return (Address)0x0;
}


void Assembler::set_target_address_at(Address pc, Address target) {
  // On MIPS we need to patch the code to generate.

  // First check we have a li.
  Instr instr2 = instr_at(pc + kInstrSize);
#ifdef DEBUG
  Instr instr1 = instr_at(pc);

  // Check we have indeed the result from a li with MustUseAt true.
  CHECK(((instr1 & kOpcodeMask) == LUI && (instr2 & kOpcodeMask) == ORI) ||
        ((instr1 == 0) && ((instr2 & kOpcodeMask)== ADDIU ||
                           (instr2 & kOpcodeMask)== ORI ||
                           (instr2 & kOpcodeMask)== LUI)));
#endif


  uint32_t rt_code = (instr2 & kRtFieldMask);
  uint32_t* p = reinterpret_cast<uint32_t*>(pc);
  uint32_t itarget = reinterpret_cast<uint32_t>(target);

  if (is_int16(itarget)) {
    // nop
    // addiu rt zero_reg j
    *p = nopInstr;
    *(p+1) = ADDIU | rt_code | (itarget & LOMask);
  } else if (!(itarget & HIMask)) {
    // nop
    // ori rt zero_reg j
    *p = nopInstr;
    *(p+1) = ORI | rt_code | (itarget & LOMask);
  } else if (!(itarget & LOMask)) {
    // nop
    // lui rt (HIMask & itarget)>>16
    *p = nopInstr;
    *(p+1) = LUI | rt_code | ((itarget & HIMask)>>16);
  } else {
    // lui rt (HIMask & itarget)>>16
    // ori rt rt, (LOMask & itarget)
    *p = LUI | rt_code | ((itarget & HIMask)>>16);
    *(p+1) = ORI | rt_code | (rt_code << 5) | (itarget & LOMask);
  }

  CPU::FlushICache(pc, 2 * sizeof(int32_t));
}


} }  // namespace v8::internal

