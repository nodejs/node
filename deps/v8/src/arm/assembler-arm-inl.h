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

// The original source code covered by the above license above has been modified
// significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

#ifndef V8_ARM_ASSEMBLER_ARM_INL_H_
#define V8_ARM_ASSEMBLER_ARM_INL_H_

#include "src/arm/assembler-arm.h"

#include "src/assembler.h"
#include "src/debug.h"


namespace v8 {
namespace internal {


bool CpuFeatures::SupportsCrankshaft() { return IsSupported(VFP3); }


int Register::NumAllocatableRegisters() {
  return kMaxNumAllocatableRegisters;
}


int DwVfpRegister::NumRegisters() {
  return CpuFeatures::IsSupported(VFP32DREGS) ? 32 : 16;
}


int DwVfpRegister::NumReservedRegisters() {
  return kNumReservedRegisters;
}


int DwVfpRegister::NumAllocatableRegisters() {
  return NumRegisters() - kNumReservedRegisters;
}


// static
int DwVfpRegister::NumAllocatableAliasedRegisters() {
  return LowDwVfpRegister::kMaxNumLowRegisters - kNumReservedRegisters;
}


int DwVfpRegister::ToAllocationIndex(DwVfpRegister reg) {
  DCHECK(!reg.is(kDoubleRegZero));
  DCHECK(!reg.is(kScratchDoubleReg));
  if (reg.code() > kDoubleRegZero.code()) {
    return reg.code() - kNumReservedRegisters;
  }
  return reg.code();
}


DwVfpRegister DwVfpRegister::FromAllocationIndex(int index) {
  DCHECK(index >= 0 && index < NumAllocatableRegisters());
  DCHECK(kScratchDoubleReg.code() - kDoubleRegZero.code() ==
         kNumReservedRegisters - 1);
  if (index >= kDoubleRegZero.code()) {
    return from_code(index + kNumReservedRegisters);
  }
  return from_code(index);
}


void RelocInfo::apply(intptr_t delta, ICacheFlushMode icache_flush_mode) {
  if (RelocInfo::IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    int32_t* p = reinterpret_cast<int32_t*>(pc_);
    *p += delta;  // relocate entry
  }
  // We do not use pc relative addressing on ARM, so there is
  // nothing else to do.
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)
                              || rmode_ == EMBEDDED_OBJECT
                              || rmode_ == EXTERNAL_REFERENCE);
  if (FLAG_enable_embedded_constant_pool ||
      Assembler::IsMovW(Memory::int32_at(pc_))) {
    // We return the PC for embedded constant pool since this function is used
    // by the serializer and expects the address to reside within the code
    // object.
    return reinterpret_cast<Address>(pc_);
  } else {
    DCHECK(Assembler::IsLdrPcImmediateOffset(Memory::int32_at(pc_)));
    return constant_pool_entry_address();
  }
}


Address RelocInfo::constant_pool_entry_address() {
  DCHECK(IsInConstantPool());
  return Assembler::constant_pool_entry_address(pc_, host_->constant_pool());
}


int RelocInfo::target_address_size() {
  return kPointerSize;
}


void RelocInfo::set_target_address(Address target,
                                   WriteBarrierMode write_barrier_mode,
                                   ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  Assembler::set_target_address_at(pc_, host_, target, icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER &&
      host() != NULL && IsCodeTarget(rmode_)) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


Object* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_, host_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<Object>(reinterpret_cast<Object**>(
      Assembler::target_address_at(pc_, host_)));
}


void RelocInfo::set_target_object(Object* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(pc_, host_,
                                   reinterpret_cast<Address>(target),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER &&
      host() != NULL &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWrite(
        host(), &Memory::Object_at(pc_), HeapObject::cast(target));
  }
}


Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, host_);
}


Address RelocInfo::target_internal_reference() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return Memory::Address_at(pc_);
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}


void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target)
    set_target_address(target, write_barrier_mode, icache_flush_mode);
}


Handle<Cell> RelocInfo::target_cell_handle() {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = Memory::Address_at(pc_);
  return Handle<Cell>(reinterpret_cast<Cell**>(address));
}


Cell* RelocInfo::target_cell() {
  DCHECK(rmode_ == RelocInfo::CELL);
  return Cell::FromValueAddress(Memory::Address_at(pc_));
}


void RelocInfo::set_target_cell(Cell* cell,
                                WriteBarrierMode write_barrier_mode,
                                ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = cell->address() + Cell::kValueOffset;
  Memory::Address_at(pc_) = address;
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL) {
    // TODO(1550) We are passing NULL as a slot because cell can never be on
    // evacuation candidate.
    host()->GetHeap()->incremental_marking()->RecordWrite(
        host(), NULL, cell);
  }
}


static const int kNoCodeAgeSequenceLength = 3 * Assembler::kInstrSize;


Handle<Object> RelocInfo::code_age_stub_handle(Assembler* origin) {
  UNREACHABLE();  // This should never be reached on Arm.
  return Handle<Object>();
}


Code* RelocInfo::code_age_stub() {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  return Code::GetCodeFromTargetAddress(
      Memory::Address_at(pc_ +
                         (kNoCodeAgeSequenceLength - Assembler::kInstrSize)));
}


void RelocInfo::set_code_age_stub(Code* stub,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  Memory::Address_at(pc_ +
                     (kNoCodeAgeSequenceLength - Assembler::kInstrSize)) =
      stub->instruction_start();
}


Address RelocInfo::call_address() {
  // The 2 instructions offset assumes patched debug break slot or return
  // sequence.
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  return Memory::Address_at(pc_ + 2 * Assembler::kInstrSize);
}


void RelocInfo::set_call_address(Address target) {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  Memory::Address_at(pc_ + 2 * Assembler::kInstrSize) = target;
  if (host() != NULL) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


Object* RelocInfo::call_object() {
  return *call_object_address();
}


void RelocInfo::set_call_object(Object* target) {
  *call_object_address() = target;
}


Object** RelocInfo::call_object_address() {
  DCHECK((IsJSReturn(rmode()) && IsPatchedReturnSequence()) ||
         (IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence()));
  return reinterpret_cast<Object**>(pc_ + 2 * Assembler::kInstrSize);
}


void RelocInfo::WipeOut() {
  DCHECK(IsEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_));
  if (IsInternalReference(rmode_)) {
    Memory::Address_at(pc_) = NULL;
  } else {
    Assembler::set_target_address_at(pc_, host_, NULL);
  }
}


bool RelocInfo::IsPatchedReturnSequence() {
  Instr current_instr = Assembler::instr_at(pc_);
  Instr next_instr = Assembler::instr_at(pc_ + Assembler::kInstrSize);
  // A patched return sequence is:
  //  ldr ip, [pc, #0]
  //  blx ip
  return Assembler::IsLdrPcImmediateOffset(current_instr) &&
         Assembler::IsBlxReg(next_instr);
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
  } else if (mode == RelocInfo::INTERNAL_REFERENCE) {
    visitor->VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    visitor->VisitCodeAgeSequence(this);
  } else if (((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
              IsPatchedDebugBreakSlotSequence())) &&
             isolate->debug()->has_break_points()) {
    visitor->VisitDebugTarget(this);
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
  } else if (mode == RelocInfo::INTERNAL_REFERENCE) {
    StaticVisitor::VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    StaticVisitor::VisitCodeAgeSequence(heap, this);
  } else if (heap->isolate()->debug()->has_break_points() &&
             ((RelocInfo::IsJSReturn(mode) &&
              IsPatchedReturnSequence()) ||
             (RelocInfo::IsDebugBreakSlot(mode) &&
              IsPatchedDebugBreakSlotSequence()))) {
    StaticVisitor::VisitDebugTarget(heap, this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}


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
  rs_ = no_reg;
  shift_op_ = LSL;
  shift_imm_ = 0;
}


bool Operand::is_reg() const {
  return rm_.is_valid() &&
         rs_.is(no_reg) &&
         shift_op_ == LSL &&
         shift_imm_ == 0;
}


void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
  if (pc_offset() >= next_buffer_check_) {
    CheckConstPool(false, true);
  }
}


void Assembler::emit(Instr x) {
  CheckBuffer();
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
}


Address Assembler::target_address_from_return_address(Address pc) {
  // Returns the address of the call target from the return address that will
  // be returned to after a call.
  // Call sequence on V7 or later is:
  //  movw  ip, #... @ call address low 16
  //  movt  ip, #... @ call address high 16
  //  blx   ip
  //                      @ return address
  // For V6 when the constant pool is unavailable, it is:
  //  mov  ip, #...     @ call address low 8
  //  orr  ip, ip, #... @ call address 2nd 8
  //  orr  ip, ip, #... @ call address 3rd 8
  //  orr  ip, ip, #... @ call address high 8
  //  blx   ip
  //                      @ return address
  // In cases that need frequent patching, the address is in the
  // constant pool.  It could be a small constant pool load:
  //  ldr   ip, [pc / pp, #...] @ call address
  //  blx   ip
  //                      @ return address
  // Or an extended constant pool load (ARMv7):
  //  movw  ip, #...
  //  movt  ip, #...
  //  ldr   ip, [pc, ip]  @ call address
  //  blx   ip
  //                      @ return address
  // Or an extended constant pool load (ARMv6):
  //  mov  ip, #...
  //  orr  ip, ip, #...
  //  orr  ip, ip, #...
  //  orr  ip, ip, #...
  //  ldr   ip, [pc, ip]  @ call address
  //  blx   ip
  //                      @ return address
  Address candidate = pc - 2 * Assembler::kInstrSize;
  Instr candidate_instr(Memory::int32_at(candidate));
  if (IsLdrPcImmediateOffset(candidate_instr) |
      IsLdrPpImmediateOffset(candidate_instr)) {
    return candidate;
  } else {
    if (IsLdrPpRegOffset(candidate_instr)) {
      candidate -= Assembler::kInstrSize;
    }
    if (CpuFeatures::IsSupported(ARMv7)) {
      candidate -= 1 * Assembler::kInstrSize;
      DCHECK(IsMovW(Memory::int32_at(candidate)) &&
             IsMovT(Memory::int32_at(candidate + Assembler::kInstrSize)));
    } else {
      candidate -= 3 * Assembler::kInstrSize;
      DCHECK(
          IsMovImmed(Memory::int32_at(candidate)) &&
          IsOrrImmed(Memory::int32_at(candidate + Assembler::kInstrSize)) &&
          IsOrrImmed(Memory::int32_at(candidate + 2 * Assembler::kInstrSize)) &&
          IsOrrImmed(Memory::int32_at(candidate + 3 * Assembler::kInstrSize)));
    }
    return candidate;
  }
}


Address Assembler::break_address_from_return_address(Address pc) {
  return pc - Assembler::kPatchDebugBreakSlotReturnOffset;
}


Address Assembler::return_address_from_call_start(Address pc) {
  if (IsLdrPcImmediateOffset(Memory::int32_at(pc)) |
      IsLdrPpImmediateOffset(Memory::int32_at(pc))) {
    // Load from constant pool, small section.
    return pc + kInstrSize * 2;
  } else {
    if (CpuFeatures::IsSupported(ARMv7)) {
      DCHECK(IsMovW(Memory::int32_at(pc)));
      DCHECK(IsMovT(Memory::int32_at(pc + kInstrSize)));
      if (IsLdrPpRegOffset(Memory::int32_at(pc + 2 * kInstrSize))) {
        // Load from constant pool, extended section.
        return pc + kInstrSize * 4;
      } else {
        // A movw / movt load immediate.
        return pc + kInstrSize * 3;
      }
    } else {
      DCHECK(IsMovImmed(Memory::int32_at(pc)));
      DCHECK(IsOrrImmed(Memory::int32_at(pc + kInstrSize)));
      DCHECK(IsOrrImmed(Memory::int32_at(pc + 2 * kInstrSize)));
      DCHECK(IsOrrImmed(Memory::int32_at(pc + 3 * kInstrSize)));
      if (IsLdrPpRegOffset(Memory::int32_at(pc + 4 * kInstrSize))) {
        // Load from constant pool, extended section.
        return pc + kInstrSize * 6;
      } else {
        // A mov / orr load immediate.
        return pc + kInstrSize * 5;
      }
    }
  }
}


void Assembler::deserialization_set_special_target_at(
    Address constant_pool_entry, Code* code, Address target) {
  if (FLAG_enable_embedded_constant_pool) {
    set_target_address_at(constant_pool_entry, code, target);
  } else {
    Memory::Address_at(constant_pool_entry) = target;
  }
}


void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  Memory::Address_at(pc) = target;
}


bool Assembler::is_constant_pool_load(Address pc) {
  if (CpuFeatures::IsSupported(ARMv7)) {
    return !Assembler::IsMovW(Memory::int32_at(pc)) ||
           (FLAG_enable_embedded_constant_pool &&
            Assembler::IsLdrPpRegOffset(
                Memory::int32_at(pc + 2 * Assembler::kInstrSize)));
  } else {
    return !Assembler::IsMovImmed(Memory::int32_at(pc)) ||
           (FLAG_enable_embedded_constant_pool &&
            Assembler::IsLdrPpRegOffset(
                Memory::int32_at(pc + 4 * Assembler::kInstrSize)));
  }
}


Address Assembler::constant_pool_entry_address(Address pc,
                                               Address constant_pool) {
  if (FLAG_enable_embedded_constant_pool) {
    DCHECK(constant_pool != NULL);
    int cp_offset;
    if (!CpuFeatures::IsSupported(ARMv7) && IsMovImmed(Memory::int32_at(pc))) {
      DCHECK(IsOrrImmed(Memory::int32_at(pc + kInstrSize)) &&
             IsOrrImmed(Memory::int32_at(pc + 2 * kInstrSize)) &&
             IsOrrImmed(Memory::int32_at(pc + 3 * kInstrSize)) &&
             IsLdrPpRegOffset(Memory::int32_at(pc + 4 * kInstrSize)));
      // This is an extended constant pool lookup (ARMv6).
      Instr mov_instr = instr_at(pc);
      Instr orr_instr_1 = instr_at(pc + kInstrSize);
      Instr orr_instr_2 = instr_at(pc + 2 * kInstrSize);
      Instr orr_instr_3 = instr_at(pc + 3 * kInstrSize);
      cp_offset = DecodeShiftImm(mov_instr) | DecodeShiftImm(orr_instr_1) |
                  DecodeShiftImm(orr_instr_2) | DecodeShiftImm(orr_instr_3);
    } else if (IsMovW(Memory::int32_at(pc))) {
      DCHECK(IsMovT(Memory::int32_at(pc + kInstrSize)) &&
             IsLdrPpRegOffset(Memory::int32_at(pc + 2 * kInstrSize)));
      // This is an extended constant pool lookup (ARMv7).
      Instruction* movw_instr = Instruction::At(pc);
      Instruction* movt_instr = Instruction::At(pc + kInstrSize);
      cp_offset = (movt_instr->ImmedMovwMovtValue() << 16) |
                  movw_instr->ImmedMovwMovtValue();
    } else {
      // This is a small constant pool lookup.
      DCHECK(Assembler::IsLdrPpImmediateOffset(Memory::int32_at(pc)));
      cp_offset = GetLdrRegisterImmediateOffset(Memory::int32_at(pc));
    }
    return constant_pool + cp_offset;
  } else {
    DCHECK(Assembler::IsLdrPcImmediateOffset(Memory::int32_at(pc)));
    Instr instr = Memory::int32_at(pc);
    return pc + GetLdrRegisterImmediateOffset(instr) + kPcLoadDelta;
  }
}


Address Assembler::target_address_at(Address pc, Address constant_pool) {
  if (is_constant_pool_load(pc)) {
    // This is a constant pool lookup. Return the value in the constant pool.
    return Memory::Address_at(constant_pool_entry_address(pc, constant_pool));
  } else if (CpuFeatures::IsSupported(ARMv7)) {
    // This is an movw / movt immediate load. Return the immediate.
    DCHECK(IsMovW(Memory::int32_at(pc)) &&
           IsMovT(Memory::int32_at(pc + kInstrSize)));
    Instruction* movw_instr = Instruction::At(pc);
    Instruction* movt_instr = Instruction::At(pc + kInstrSize);
    return reinterpret_cast<Address>(
        (movt_instr->ImmedMovwMovtValue() << 16) |
         movw_instr->ImmedMovwMovtValue());
  } else {
    // This is an mov / orr immediate load. Return the immediate.
    DCHECK(IsMovImmed(Memory::int32_at(pc)) &&
           IsOrrImmed(Memory::int32_at(pc + kInstrSize)) &&
           IsOrrImmed(Memory::int32_at(pc + 2 * kInstrSize)) &&
           IsOrrImmed(Memory::int32_at(pc + 3 * kInstrSize)));
    Instr mov_instr = instr_at(pc);
    Instr orr_instr_1 = instr_at(pc + kInstrSize);
    Instr orr_instr_2 = instr_at(pc + 2 * kInstrSize);
    Instr orr_instr_3 = instr_at(pc + 3 * kInstrSize);
    Address ret = reinterpret_cast<Address>(
        DecodeShiftImm(mov_instr) | DecodeShiftImm(orr_instr_1) |
        DecodeShiftImm(orr_instr_2) | DecodeShiftImm(orr_instr_3));
    return ret;
  }
}


void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  if (is_constant_pool_load(pc)) {
    // This is a constant pool lookup. Update the entry in the constant pool.
    Memory::Address_at(constant_pool_entry_address(pc, constant_pool)) = target;
    // Intuitively, we would think it is necessary to always flush the
    // instruction cache after patching a target address in the code as follows:
    //   CpuFeatures::FlushICache(pc, sizeof(target));
    // However, on ARM, no instruction is actually patched in the case
    // of embedded constants of the form:
    // ldr   ip, [pp, #...]
    // since the instruction accessing this address in the constant pool remains
    // unchanged.
  } else if (CpuFeatures::IsSupported(ARMv7)) {
    // This is an movw / movt immediate load. Patch the immediate embedded in
    // the instructions.
    DCHECK(IsMovW(Memory::int32_at(pc)));
    DCHECK(IsMovT(Memory::int32_at(pc + kInstrSize)));
    uint32_t* instr_ptr = reinterpret_cast<uint32_t*>(pc);
    uint32_t immediate = reinterpret_cast<uint32_t>(target);
    instr_ptr[0] = PatchMovwImmediate(instr_ptr[0], immediate & 0xFFFF);
    instr_ptr[1] = PatchMovwImmediate(instr_ptr[1], immediate >> 16);
    DCHECK(IsMovW(Memory::int32_at(pc)));
    DCHECK(IsMovT(Memory::int32_at(pc + kInstrSize)));
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      CpuFeatures::FlushICache(pc, 2 * kInstrSize);
    }
  } else {
    // This is an mov / orr immediate load. Patch the immediate embedded in
    // the instructions.
    DCHECK(IsMovImmed(Memory::int32_at(pc)) &&
           IsOrrImmed(Memory::int32_at(pc + kInstrSize)) &&
           IsOrrImmed(Memory::int32_at(pc + 2 * kInstrSize)) &&
           IsOrrImmed(Memory::int32_at(pc + 3 * kInstrSize)));
    uint32_t* instr_ptr = reinterpret_cast<uint32_t*>(pc);
    uint32_t immediate = reinterpret_cast<uint32_t>(target);
    instr_ptr[0] = PatchShiftImm(instr_ptr[0], immediate & kImm8Mask);
    instr_ptr[1] = PatchShiftImm(instr_ptr[1], immediate & (kImm8Mask << 8));
    instr_ptr[2] = PatchShiftImm(instr_ptr[2], immediate & (kImm8Mask << 16));
    instr_ptr[3] = PatchShiftImm(instr_ptr[3], immediate & (kImm8Mask << 24));
    DCHECK(IsMovImmed(Memory::int32_at(pc)) &&
           IsOrrImmed(Memory::int32_at(pc + kInstrSize)) &&
           IsOrrImmed(Memory::int32_at(pc + 2 * kInstrSize)) &&
           IsOrrImmed(Memory::int32_at(pc + 3 * kInstrSize)));
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      CpuFeatures::FlushICache(pc, 4 * kInstrSize);
    }
  }
}


} }  // namespace v8::internal

#endif  // V8_ARM_ASSEMBLER_ARM_INL_H_
