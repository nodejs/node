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
#include "debug.h"
#include "memory.h"

namespace v8 {
namespace internal {


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


void Assembler::emit_code_target(Handle<Code> target, RelocInfo::Mode rmode) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  RecordRelocInfo(rmode);
  int current = code_targets_.length();
  if (current > 0 && code_targets_.last().is_identical_to(target)) {
    // Optimization if we keep jumping to the same code target.
    emitl(current - 1);
  } else {
    code_targets_.Add(target);
    emitl(current);
  }
}


void Assembler::emit_rex_64(Register reg, Register rm_reg) {
  emit(0x48 | reg.high_bit() << 2 | rm_reg.high_bit());
}


void Assembler::emit_rex_64(XMMRegister reg, Register rm_reg) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | rm_reg.code() >> 3);
}


void Assembler::emit_rex_64(Register reg, XMMRegister rm_reg) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | rm_reg.code() >> 3);
}


void Assembler::emit_rex_64(Register reg, const Operand& op) {
  emit(0x48 | reg.high_bit() << 2 | op.rex_);
}


void Assembler::emit_rex_64(XMMRegister reg, const Operand& op) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | op.rex_);
}


void Assembler::emit_rex_64(Register rm_reg) {
  ASSERT_EQ(rm_reg.code() & 0xf, rm_reg.code());
  emit(0x48 | rm_reg.high_bit());
}


void Assembler::emit_rex_64(const Operand& op) {
  emit(0x48 | op.rex_);
}


void Assembler::emit_rex_32(Register reg, Register rm_reg) {
  emit(0x40 | reg.high_bit() << 2 | rm_reg.high_bit());
}


void Assembler::emit_rex_32(Register reg, const Operand& op) {
  emit(0x40 | reg.high_bit() << 2  | op.rex_);
}


void Assembler::emit_rex_32(Register rm_reg) {
  emit(0x40 | rm_reg.high_bit());
}


void Assembler::emit_rex_32(const Operand& op) {
  emit(0x40 | op.rex_);
}


void Assembler::emit_optional_rex_32(Register reg, Register rm_reg) {
  byte rex_bits = reg.high_bit() << 2 | rm_reg.high_bit();
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(Register reg, const Operand& op) {
  byte rex_bits =  reg.high_bit() << 2 | op.rex_;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(XMMRegister reg, const Operand& op) {
  byte rex_bits =  (reg.code() & 0x8) >> 1 | op.rex_;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(XMMRegister reg, XMMRegister base) {
  byte rex_bits =  (reg.code() & 0x8) >> 1 | (base.code() & 0x8) >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(XMMRegister reg, Register base) {
  byte rex_bits =  (reg.code() & 0x8) >> 1 | (base.code() & 0x8) >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(Register reg, XMMRegister base) {
  byte rex_bits =  (reg.code() & 0x8) >> 1 | (base.code() & 0x8) >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}


void Assembler::emit_optional_rex_32(Register rm_reg) {
  if (rm_reg.high_bit()) emit(0x41);
}


void Assembler::emit_optional_rex_32(const Operand& op) {
  if (op.rex_ != 0) emit(0x40 | op.rex_);
}


Address Assembler::target_address_at(Address pc) {
  return Memory::int32_at(pc) + pc + 4;
}


void Assembler::set_target_address_at(Address pc, Address target) {
  Memory::int32_at(pc) = static_cast<int32_t>(target - pc - 4);
  CPU::FlushICache(pc, sizeof(int32_t));
}

Handle<Object> Assembler::code_target_object_handle_at(Address pc) {
  return code_targets_[Memory::int32_at(pc)];
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(intptr_t delta) {
  if (IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    Memory::Address_at(pc_) += static_cast<int32_t>(delta);
  } else if (IsCodeTarget(rmode_)) {
    Memory::int32_at(pc_) -= static_cast<int32_t>(delta);
  } else if (rmode_ == JS_RETURN && IsPatchedReturnSequence()) {
    // Special handling of js_return when a break point is set (call
    // instruction has been inserted).
    Memory::int32_at(pc_ + 1) -= static_cast<int32_t>(delta);  // relocate entry
  } else if (rmode_ == DEBUG_BREAK_SLOT && IsPatchedDebugBreakSlotSequence()) {
    // Special handling of debug break slot when a break point is set (call
    // instruction has been inserted).
    Memory::int32_at(pc_ + 1) -= static_cast<int32_t>(delta);  // relocate entry
  }
}


Address RelocInfo::target_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  if (IsCodeTarget(rmode_)) {
    return Assembler::target_address_at(pc_);
  } else {
    return Memory::Address_at(pc_);
  }
}


Address RelocInfo::target_address_address() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  return reinterpret_cast<Address>(pc_);
}


int RelocInfo::target_address_size() {
  if (IsCodedSpecially()) {
    return Assembler::kCallTargetSize;
  } else {
    return Assembler::kExternalTargetSize;
  }
}


void RelocInfo::set_target_address(Address target) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == RUNTIME_ENTRY);
  if (IsCodeTarget(rmode_)) {
    Assembler::set_target_address_at(pc_, target);
  } else {
    Memory::Address_at(pc_) = target;
  }
}


Object* RelocInfo::target_object() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Memory::Object_at(pc_);
}


Handle<Object> RelocInfo::target_object_handle(Assembler *origin) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  if (rmode_ == EMBEDDED_OBJECT) {
    return Memory::Object_Handle_at(pc_);
  } else {
    return origin->code_target_object_handle_at(pc_);
  }
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


bool RelocInfo::IsPatchedReturnSequence() {
  // The recognized call sequence is:
  //  movq(kScratchRegister, immediate64); call(kScratchRegister);
  // It only needs to be distinguished from a return sequence
  //  movq(rsp, rbp); pop(rbp); ret(n); int3 *6
  // The 11th byte is int3 (0xCC) in the return sequence and
  // REX.WB (0x48+register bit) for the call sequence.
#ifdef ENABLE_DEBUGGER_SUPPORT
  return pc_[10] != 0xCC;
#else
  return false;
#endif
}


bool RelocInfo::IsPatchedDebugBreakSlotSequence() {
  return !Assembler::IsNop(pc());
}


Address RelocInfo::call_address() {
  ASSERT(IsPatchedReturnSequence());
  return Memory::Address_at(
      pc_ + Assembler::kRealPatchReturnSequenceAddressOffset);
}


void RelocInfo::set_call_address(Address target) {
  ASSERT(IsPatchedReturnSequence());
  Memory::Address_at(pc_ + Assembler::kRealPatchReturnSequenceAddressOffset) =
      target;
}


Object* RelocInfo::call_object() {
  ASSERT(IsPatchedReturnSequence());
  return *call_object_address();
}


void RelocInfo::set_call_object(Object* target) {
  ASSERT(IsPatchedReturnSequence());
  *call_object_address() = target;
}


Object** RelocInfo::call_object_address() {
  ASSERT(IsPatchedReturnSequence());
  return reinterpret_cast<Object**>(
      pc_ + Assembler::kPatchReturnSequenceAddressOffset);
}


void RelocInfo::Visit(ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitPointer(target_object_address());
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(target_reference_address());
#ifdef ENABLE_DEBUGGER_SUPPORT
  } else if (Debug::has_break_points() &&
             ((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
              IsPatchedDebugBreakSlotSequence()))) {
    visitor->VisitDebugTarget(this);
#endif
  } else if (mode == RelocInfo::RUNTIME_ENTRY) {
    visitor->VisitRuntimeEntry(this);
  }
}


// -----------------------------------------------------------------------------
// Implementation of Operand

void Operand::set_modrm(int mod, Register rm_reg) {
  ASSERT(is_uint2(mod));
  buf_[0] = mod << 6 | rm_reg.low_bits();
  // Set REX.B to the high bit of rm.code().
  rex_ |= rm_reg.high_bit();
}


void Operand::set_sib(ScaleFactor scale, Register index, Register base) {
  ASSERT(len_ == 1);
  ASSERT(is_uint2(scale));
  // Use SIB with no index register only for base rsp or r12. Otherwise we
  // would skip the SIB byte entirely.
  ASSERT(!index.is(rsp) || base.is(rsp) || base.is(r12));
  buf_[1] = scale << 6 | index.low_bits() << 3 | base.low_bits();
  rex_ |= index.high_bit() << 1 | base.high_bit();
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
