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


#ifndef V8_MIPS_ASSEMBLER_MIPS_INL_H_
#define V8_MIPS_ASSEMBLER_MIPS_INL_H_

#include "mips/assembler-mips.h"
#include "cpu.h"


namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Condition

Condition NegateCondition(Condition cc) {
  ASSERT(cc != cc_always);
  return static_cast<Condition>(cc ^ 1);
}


// -----------------------------------------------------------------------------
// Operand and MemOperand

Operand::Operand(int32_t immediate, RelocInfo::Mode rmode)  {
  rm_ = no_reg;
  imm32_ = immediate;
  rmode_ = rmode;
}

Operand::Operand(const ExternalReference& f)  {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(f.address());
  rmode_ = RelocInfo::EXTERNAL_REFERENCE;
}

Operand::Operand(const char* s) {
  rm_ = no_reg;
  imm32_ = reinterpret_cast<int32_t>(s);
  rmode_ = RelocInfo::EMBEDDED_STRING;
}

Operand::Operand(Smi* value) {
  rm_ = no_reg;
  imm32_ =  reinterpret_cast<intptr_t>(value);
  rmode_ = RelocInfo::NONE;
}

Operand::Operand(Register rm) {
  rm_ = rm;
}

bool Operand::is_reg() const {
  return rm_.is_valid();
}



// -----------------------------------------------------------------------------
// RelocInfo

void RelocInfo::apply(intptr_t delta) {
  // On MIPS we do not use pc relative addressing, so we don't need to patch the
  // code here.
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
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler *origin) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<Object>(reinterpret_cast<Object**>(
      Assembler::target_address_at(pc_)));
}


Object** RelocInfo::target_object_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object**>(pc_);
}


void RelocInfo::set_target_object(Object* target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(pc_, reinterpret_cast<Address>(target));
}


Address* RelocInfo::target_reference_address() {
  ASSERT(rmode_ == EXTERNAL_REFERENCE);
  return reinterpret_cast<Address*>(pc_);
}


Address RelocInfo::call_address() {
  ASSERT(IsPatchedReturnSequence());
  // The 2 instructions offset assumes patched return sequence.
  ASSERT(IsJSReturn(rmode()));
  return Memory::Address_at(pc_ + 2 * Assembler::kInstrSize);
}


void RelocInfo::set_call_address(Address target) {
  ASSERT(IsPatchedReturnSequence());
  // The 2 instructions offset assumes patched return sequence.
  ASSERT(IsJSReturn(rmode()));
  Memory::Address_at(pc_ + 2 * Assembler::kInstrSize) = target;
}


Object* RelocInfo::call_object() {
  return *call_object_address();
}


Object** RelocInfo::call_object_address() {
  ASSERT(IsPatchedReturnSequence());
  // The 2 instructions offset assumes patched return sequence.
  ASSERT(IsJSReturn(rmode()));
  return reinterpret_cast<Object**>(pc_ + 2 * Assembler::kInstrSize);
}


void RelocInfo::set_call_object(Object* target) {
  *call_object_address() = target;
}


bool RelocInfo::IsPatchedReturnSequence() {
#ifdef DEBUG
  PrintF("%s - %d - %s : Checking for jal(r)",
      __FILE__, __LINE__, __func__);
#endif
  return ((Assembler::instr_at(pc_) & kOpcodeMask) == SPECIAL) &&
         (((Assembler::instr_at(pc_) & kFunctionFieldMask) == JAL) ||
          ((Assembler::instr_at(pc_) & kFunctionFieldMask) == JALR));
}


// -----------------------------------------------------------------------------
// Assembler


void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}


void Assembler::emit(Instr x) {
  CheckBuffer();
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
}


} }  // namespace v8::internal

#endif  // V8_MIPS_ASSEMBLER_MIPS_INL_H_
