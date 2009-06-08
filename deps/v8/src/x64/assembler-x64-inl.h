// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_X64_ASSEMBLER_X64_INL_H_
#define V8_X64_ASSEMBLER_X64_INL_H_

#include "cpu.h"

namespace v8 {
namespace internal {

Condition NegateCondition(Condition cc) {
  return static_cast<Condition>(cc ^ 1);
}

// -----------------------------------------------------------------------------

Immediate::Immediate(Smi* value) {
  value_ = static_cast<int32_t>(reinterpret_cast<intptr_t>(value));
}

// -----------------------------------------------------------------------------
// Implementation of Assembler



void Assembler::emitl(uint32_t x) {
  Memory::uint32_at(pc_) = x;
  pc_ += sizeof(uint32_t);
}


void Assembler::emitq(uint64_t x, RelocInfo::Mode rmode) {
  Memory::uint64_at(pc_) = x;
  if (rmode != RelocInfo::NONE) {
    RecordRelocInfo(rmode, x);
  }
  pc_ += sizeof(uint64_t);
}


void Assembler::emitw(uint16_t x) {
  Memory::uint16_at(pc_) = x;
  pc_ += sizeof(uint16_t);
}


void Assembler::emit_rex_64(Register reg, Register rm_reg) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | rm_reg.code() >> 3);
}


void Assembler::emit_rex_64(Register reg, const Operand& op) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | op.rex_);
}


void Assembler::emit_rex_64(Register rm_reg) {
  ASSERT_EQ(rm_reg.code() & 0xf, rm_reg.code());
  emit(0x48 | (rm_reg.code() >> 3));
}


void Assembler::emit_rex_64(const Operand& op) {
  emit(0x48 | op.rex_);
}


void Assembler::emit_rex_32(Register reg, Register rm_reg) {
  emit(0x40 | (reg.code() & 0x8) >> 1 | rm_reg.code() >> 3);
}


void Assembler::emit_rex_32(Register reg, const Operand& op) {
  emit(0x40 | (reg.code() & 0x8) >> 1 | op.rex_);
}


void Assembler::emit_rex_32(Register rm_reg) {
  emit(0x40 | (rm_reg.code() & 0x8) >> 3);
}


void Assembler::emit_rex_32(const Operand& op) {
  emit(0x40 | op.rex_);
}


void Assembler::emit_optional_rex_32(Register reg, Register rm_reg) {
  byte rex_bits = (reg.code() & 0x8) >> 1 | rm_reg.code() >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(Register reg, const Operand& op) {
  byte rex_bits =  (reg.code() & 0x8) >> 1 | op.rex_;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(Register rm_reg) {
  if (rm_reg.code() & 0x8 != 0) emit(0x41);
}


void Assembler::emit_optional_rex_32(const Operand& op) {
  if (op.rex_ != 0) emit(0x40 | op.rex_);
}


Address Assembler::target_address_at(Address pc) {
  return Memory::Address_at(pc);
}


void Assembler::set_target_address_at(Address pc, Address target) {
  Memory::Address_at(pc) = target;
  CPU::FlushICache(pc, sizeof(intptr_t));
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(int delta) {
  if (rmode_ == RUNTIME_ENTRY || IsCodeTarget(rmode_)) {
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_);
    *p -= delta;  // relocate entry
  } else if (rmode_ == JS_RETURN && IsCallInstruction()) {
    // Special handling of js_return when a break point is set (call
    // instruction has been inserted).
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_ + 1);
    *p -= delta;  // relocate entry
  } else if (IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    intptr_t* p = reinterpret_cast<intptr_t*>(pc_);
    *p += delta;  // relocate entry
  }
}


Address RelocInfo::target_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return Assembler::target_address_at(pc_);
}


Address RelocInfo::target_address_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return reinterpret_cast<Address>(pc_);
}


void RelocInfo::set_target_address(Address target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  Assembler::set_target_address_at(pc_, target);
}


Object* RelocInfo::target_object() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return *reinterpret_cast<Object**>(pc_);
}


Object** RelocInfo::target_object_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object**>(pc_);
}


Address* RelocInfo::target_reference_address() {
  ASSERT(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  return reinterpret_cast<Address*>(pc_);
}


void RelocInfo::set_target_object(Object* target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  *reinterpret_cast<Object**>(pc_) = target;
}


bool RelocInfo::IsCallInstruction() {
  UNIMPLEMENTED();  // IA32 code below.
  return *pc_ == 0xE8;
}


Address RelocInfo::call_address() {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  return Assembler::target_address_at(pc_ + 1);
}


void RelocInfo::set_call_address(Address target) {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  Assembler::set_target_address_at(pc_ + 1, target);
}


Object* RelocInfo::call_object() {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  return *call_object_address();
}


void RelocInfo::set_call_object(Object* target) {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  *call_object_address() = target;
}


Object** RelocInfo::call_object_address() {
  UNIMPLEMENTED();  // IA32 code below.
  ASSERT(IsCallInstruction());
  return reinterpret_cast<Object**>(pc_ + 1);
}

// -----------------------------------------------------------------------------
// Implementation of Operand

Operand::Operand(Register base, int32_t disp) {
  len_ = 1;
  if (base.is(rsp) || base.is(r12)) {
    // SIB byte is needed to encode (rsp + offset) or (r12 + offset).
    set_sib(kTimes1, rsp, base);
  }

  if (disp == 0 && !base.is(rbp) && !base.is(r13)) {
    set_modrm(0, rsp);
  } else if (is_int8(disp)) {
    set_modrm(1, base);
    set_disp8(disp);
  } else {
    set_modrm(2, base);
    set_disp32(disp);
  }
}

void Operand::set_modrm(int mod, Register rm) {
  ASSERT((mod & -4) == 0);
  buf_[0] = mod << 6 | (rm.code() & 0x7);
  // Set REX.B to the high bit of rm.code().
  rex_ |= (rm.code() >> 3);
}


void Operand::set_sib(ScaleFactor scale, Register index, Register base) {
  ASSERT(len_ == 1);
  ASSERT(is_uint2(scale));
  // Use SIB with no index register only for base rsp or r12.
  ASSERT(!index.is(rsp) || base.is(rsp) || base.is(r12));
  buf_[1] = scale << 6 | (index.code() & 0x7) << 3 | (base.code() & 0x7);
  rex_ |= (index.code() >> 3) << 1 | base.code() >> 3;
  len_ = 2;
}

void Operand::set_disp8(int disp) {
  ASSERT(is_int8(disp));
  ASSERT(len_ == 1 || len_ == 2);
  int8_t* p = reinterpret_cast<int8_t*>(&buf_[len_]);
  *p = disp;
  len_ += sizeof(int8_t);
}

void Operand::set_disp32(int disp) {
  ASSERT(len_ == 1 || len_ == 2);
  int32_t* p = reinterpret_cast<int32_t*>(&buf_[len_]);
  *p = disp;
  len_ += sizeof(int32_t);
}


} }  // namespace v8::internal

#endif  // V8_X64_ASSEMBLER_X64_INL_H_
