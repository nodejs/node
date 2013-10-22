
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


#ifndef V8_MIPS_ASSEMBLER_MIPS_INL_H_
#define V8_MIPS_ASSEMBLER_MIPS_INL_H_

#include "mips/assembler-mips.h"

#include "cpu.h"
#include "debug.h"


namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Operand and MemOperand.

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


Operand::Operand(Smi* value) {
  rm_ = no_reg;
  imm32_ =  reinterpret_cast<intptr_t>(value);
  rmode_ = RelocInfo::NONE32;
}


Operand::Operand(Register rm) {
  rm_ = rm;
}


bool Operand::is_reg() const {
  return rm_.is_valid();
}


int Register::NumAllocatableRegisters() {
    return kMaxNumAllocatableRegisters;
}


int DoubleRegister::NumRegisters() {
    return FPURegister::kMaxNumRegisters;
}


int DoubleRegister::NumAllocatableRegisters() {
    return FPURegister::kMaxNumAllocatableRegisters;
}


int FPURegister::ToAllocationIndex(FPURegister reg) {
  ASSERT(reg.code() % 2 == 0);
  ASSERT(reg.code() / 2 < kMaxNumAllocatableRegisters);
  ASSERT(reg.is_valid());
  ASSERT(!reg.is(kDoubleRegZero));
  ASSERT(!reg.is(kLithiumScratchDouble));
  return (reg.code() / 2);
}


// -----------------------------------------------------------------------------
// RelocInfo.

void RelocInfo::apply(intptr_t delta) {
  if (IsCodeTarget(rmode_)) {
    uint32_t scope1 = (uint32_t) target_address() & ~kImm28Mask;
    uint32_t scope2 = reinterpret_cast<uint32_t>(pc_) & ~kImm28Mask;

    if (scope1 != scope2) {
      Assembler::JumpLabelToJumpRegister(pc_);
    }
  }
  if (IsInternalReference(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    byte* p = reinterpret_cast<byte*>(pc_);
    int count = Assembler::RelocateInternalReference(p, delta);
    CPU::FlushICache(p, count * sizeof(uint32_t));
  }
}


Address RelocInfo::target_address() {
  ASSERT(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_);
}


Address RelocInfo::target_address_address() {
  ASSERT(IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) ||
         rmode_ == EMBEDDED_OBJECT ||
         rmode_ == EXTERNAL_REFERENCE);
  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like LUI/ORI where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written. In this case the target_address_address function should
  // return the end of the instructions to be patched, allowing the
  // deserializer to deserialize the instructions as raw bytes and put them in
  // place, ready to be patched with the target. After jump optimization,
  // that is the address of the instruction that follows J/JAL/JR/JALR
  // instruction.
  return reinterpret_cast<Address>(
    pc_ + Assembler::kInstructionsFor32BitConstant * Assembler::kInstrSize);
}


int RelocInfo::target_address_size() {
  return Assembler::kSpecialTargetSize;
}


void RelocInfo::set_target_address(Address target, WriteBarrierMode mode) {
  ASSERT(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  Assembler::set_target_address_at(pc_, target);
  if (mode == UPDATE_WRITE_BARRIER && host() != NULL && IsCodeTarget(rmode_)) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


Address Assembler::target_address_from_return_address(Address pc) {
  return pc - kCallTargetAddressOffset;
}


Object* RelocInfo::target_object() {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler* origin) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<Object>(reinterpret_cast<Object**>(
      Assembler::target_address_at(pc_)));
}


Object** RelocInfo::target_object_address() {
  // Provide a "natural pointer" to the embedded object,
  // which can be de-referenced during heap iteration.
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  reconstructed_obj_ptr_ =
      reinterpret_cast<Object*>(Assembler::target_address_at(pc_));
  return &reconstructed_obj_ptr_;
}


void RelocInfo::set_target_object(Object* target, WriteBarrierMode mode) {
  ASSERT(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  ASSERT(!target->IsConsString());
  Assembler::set_target_address_at(pc_, reinterpret_cast<Address>(target));
  if (mode == UPDATE_WRITE_BARRIER &&
      host() != NULL &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWrite(
        host(), &Memory::Object_at(pc_), HeapObject::cast(target));
  }
}


Address* RelocInfo::target_reference_address() {
  ASSERT(rmode_ == EXTERNAL_REFERENCE);
  reconstructed_adr_ptr_ = Assembler::target_address_at(pc_);
  return &reconstructed_adr_ptr_;
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  ASSERT(IsRuntimeEntry(rmode_));
  return target_address();
}


void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode mode) {
  ASSERT(IsRuntimeEntry(rmode_));
  if (target_address() != target) set_target_address(target, mode);
}


Handle<Cell> RelocInfo::target_cell_handle() {
  ASSERT(rmode_ == RelocInfo::CELL);
  Address address = Memory::Address_at(pc_);
  return Handle<Cell>(reinterpret_cast<Cell**>(address));
}


Cell* RelocInfo::target_cell() {
  ASSERT(rmode_ == RelocInfo::CELL);
  return Cell::FromValueAddress(Memory::Address_at(pc_));
}


void RelocInfo::set_target_cell(Cell* cell, WriteBarrierMode mode) {
  ASSERT(rmode_ == RelocInfo::CELL);
  Address address = cell->address() + Cell::kValueOffset;
  Memory::Address_at(pc_) = address;
  if (mode == UPDATE_WRITE_BARRIER && host() != NULL) {
    // TODO(1550) We are passing NULL as a slot because cell can never be on
    // evacuation candidate.
    host()->GetHeap()->incremental_marking()->RecordWrite(
        host(), NULL, cell);
  }
}


static const int kNoCodeAgeSequenceLength = 7;

Code* RelocInfo::code_age_stub() {
  ASSERT(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  return Code::GetCodeFromTargetAddress(
      Memory::Address_at(pc_ + Assembler::kInstrSize *
                         (kNoCodeAgeSequenceLength - 1)));
}


void RelocInfo::set_code_age_stub(Code* stub) {
  ASSERT(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  Memory::Address_at(pc_ + Assembler::kInstrSize *
                     (kNoCodeAgeSequenceLength - 1)) =
      stub->instruction_start();
}


Address RelocInfo::call_address() {
  ASSERT((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  // The pc_ offset of 0 assumes mips patched return sequence per
  // debug-mips.cc BreakLocationIterator::SetDebugBreakAtReturn(), or
  // debug break slot per BreakLocationIterator::SetDebugBreakAtSlot().
  return Assembler::target_address_at(pc_);
}


void RelocInfo::set_call_address(Address target) {
  ASSERT((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  // The pc_ offset of 0 assumes mips patched return sequence per
  // debug-mips.cc BreakLocationIterator::SetDebugBreakAtReturn(), or
  // debug break slot per BreakLocationIterator::SetDebugBreakAtSlot().
  Assembler::set_target_address_at(pc_, target);
  if (host() != NULL) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


Object* RelocInfo::call_object() {
  return *call_object_address();
}


Object** RelocInfo::call_object_address() {
  ASSERT((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  return reinterpret_cast<Object**>(pc_ + 2 * Assembler::kInstrSize);
}


void RelocInfo::set_call_object(Object* target) {
  *call_object_address() = target;
}


bool RelocInfo::IsPatchedReturnSequence() {
  Instr instr0 = Assembler::instr_at(pc_);
  Instr instr1 = Assembler::instr_at(pc_ + 1 * Assembler::kInstrSize);
  Instr instr2 = Assembler::instr_at(pc_ + 2 * Assembler::kInstrSize);
  bool patched_return = ((instr0 & kOpcodeMask) == LUI &&
                         (instr1 & kOpcodeMask) == ORI &&
                         ((instr2 & kOpcodeMask) == JAL ||
                          ((instr2 & kOpcodeMask) == SPECIAL &&
                           (instr2 & kFunctionFieldMask) == JALR)));
  return patched_return;
}


bool RelocInfo::IsPatchedDebugBreakSlotSequence() {
  Instr current_instr = Assembler::instr_at(pc_);
  return !Assembler::IsNop(current_instr, Assembler::DEBUG_BREAK_NOP);
}


void RelocInfo::Visit(Isolate* isolate, ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(this);
  } else if (mode == RelocInfo::CELL) {
    visitor->VisitCell(this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    visitor->VisitCodeAgeSequence(this);
#ifdef ENABLE_DEBUGGER_SUPPORT
  } else if (((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence())) &&
             isolate->debug()->has_break_points()) {
    visitor->VisitDebugTarget(this);
#endif
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(this);
  }
}


template<typename StaticVisitor>
void RelocInfo::Visit(Heap* heap) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    StaticVisitor::VisitEmbeddedPointer(heap, this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    StaticVisitor::VisitCodeTarget(heap, this);
  } else if (mode == RelocInfo::CELL) {
    StaticVisitor::VisitCell(heap, this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    StaticVisitor::VisitExternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    StaticVisitor::VisitCodeAgeSequence(heap, this);
#ifdef ENABLE_DEBUGGER_SUPPORT
  } else if (heap->isolate()->debug()->has_break_points() &&
             ((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
              IsPatchedDebugBreakSlotSequence()))) {
    StaticVisitor::VisitDebugTarget(heap, this);
#endif
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}


// -----------------------------------------------------------------------------
// Assembler.


void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}


void Assembler::CheckTrampolinePoolQuick() {
  if (pc_offset() >= next_buffer_check_) {
    CheckTrampolinePool();
  }
}


void Assembler::emit(Instr x) {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
  CheckTrampolinePoolQuick();
}


} }  // namespace v8::internal

#endif  // V8_MIPS_ASSEMBLER_MIPS_INL_H_
