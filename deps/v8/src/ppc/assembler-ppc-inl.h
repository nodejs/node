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
// Copyright 2014 the V8 project authors. All rights reserved.

#ifndef V8_PPC_ASSEMBLER_PPC_INL_H_
#define V8_PPC_ASSEMBLER_PPC_INL_H_

#include "src/ppc/assembler-ppc.h"

#include "src/assembler.h"
#include "src/debug/debug.h"


namespace v8 {
namespace internal {


bool CpuFeatures::SupportsCrankshaft() { return true; }

bool CpuFeatures::SupportsSimd128() { return false; }

void RelocInfo::apply(intptr_t delta) {
  // absolute code pointer inside code object moves with the code object.
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    Address target = Memory::Address_at(pc_);
    Memory::Address_at(pc_) = target + delta;
  } else {
    // mov sequence
    DCHECK(IsInternalReferenceEncoded(rmode_));
    Address target = Assembler::target_address_at(pc_, host_);
    Assembler::set_target_address_at(isolate_, pc_, host_, target + delta,
                                     SKIP_ICACHE_FLUSH);
  }
}


Address RelocInfo::target_internal_reference() {
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    return Memory::Address_at(pc_);
  } else {
    // mov sequence
    DCHECK(IsInternalReferenceEncoded(rmode_));
    return Assembler::target_address_at(pc_, host_);
  }
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) ||
         rmode_ == EMBEDDED_OBJECT || rmode_ == EXTERNAL_REFERENCE);

  if (FLAG_enable_embedded_constant_pool &&
      Assembler::IsConstantPoolLoadStart(pc_)) {
    // We return the PC for embedded constant pool since this function is used
    // by the serializer and expects the address to reside within the code
    // object.
    return reinterpret_cast<Address>(pc_);
  }

  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like LIS/ORI where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written.
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::constant_pool_entry_address() {
  if (FLAG_enable_embedded_constant_pool) {
    Address constant_pool = host_->constant_pool();
    DCHECK(constant_pool);
    ConstantPoolEntry::Access access;
    if (Assembler::IsConstantPoolLoadStart(pc_, &access))
      return Assembler::target_constant_pool_address_at(
          pc_, constant_pool, access, ConstantPoolEntry::INTPTR);
  }
  UNREACHABLE();
  return NULL;
}


int RelocInfo::target_address_size() { return Assembler::kSpecialTargetSize; }


Address Assembler::target_address_from_return_address(Address pc) {
// Returns the address of the call target from the return address that will
// be returned to after a call.
// Call sequence is :
//  mov   ip, @ call address
//  mtlr  ip
//  blrl
//                      @ return address
  int len;
  ConstantPoolEntry::Access access;
  if (FLAG_enable_embedded_constant_pool &&
      IsConstantPoolLoadEnd(pc - 3 * kInstrSize, &access)) {
    len = (access == ConstantPoolEntry::OVERFLOWED) ? 2 : 1;
  } else {
    len = kMovInstructionsNoConstantPool;
  }
  return pc - (len + 2) * kInstrSize;
}


Address Assembler::return_address_from_call_start(Address pc) {
  int len;
  ConstantPoolEntry::Access access;
  if (FLAG_enable_embedded_constant_pool &&
      IsConstantPoolLoadStart(pc, &access)) {
    len = (access == ConstantPoolEntry::OVERFLOWED) ? 2 : 1;
  } else {
    len = kMovInstructionsNoConstantPool;
  }
  return pc + (len + 2) * kInstrSize;
}

Object* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return reinterpret_cast<Object*>(Assembler::target_address_at(pc_, host_));
}


Handle<Object> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<Object>(
      reinterpret_cast<Object**>(Assembler::target_address_at(pc_, host_)));
}


void RelocInfo::set_target_object(Object* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(isolate_, pc_, host_,
                                   reinterpret_cast<Address>(target),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target));
    host()->GetHeap()->RecordWriteIntoCode(host(), this, target);
  }
}


Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, host_);
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


void RelocInfo::set_target_cell(Cell* cell, WriteBarrierMode write_barrier_mode,
                                ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CELL);
  Address address = cell->address() + Cell::kValueOffset;
  Memory::Address_at(pc_) = address;
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  cell);
  }
}


static const int kNoCodeAgeInstructions =
    FLAG_enable_embedded_constant_pool ? 7 : 6;
static const int kCodeAgingInstructions =
    Assembler::kMovInstructionsNoConstantPool + 3;
static const int kNoCodeAgeSequenceInstructions =
    ((kNoCodeAgeInstructions >= kCodeAgingInstructions)
         ? kNoCodeAgeInstructions
         : kCodeAgingInstructions);
static const int kNoCodeAgeSequenceNops =
    (kNoCodeAgeSequenceInstructions - kNoCodeAgeInstructions);
static const int kCodeAgingSequenceNops =
    (kNoCodeAgeSequenceInstructions - kCodeAgingInstructions);
static const int kCodeAgingTargetDelta = 1 * Assembler::kInstrSize;
static const int kNoCodeAgeSequenceLength =
    (kNoCodeAgeSequenceInstructions * Assembler::kInstrSize);


Handle<Object> RelocInfo::code_age_stub_handle(Assembler* origin) {
  UNREACHABLE();  // This should never be reached on PPC.
  return Handle<Object>();
}


Code* RelocInfo::code_age_stub() {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  return Code::GetCodeFromTargetAddress(
      Assembler::target_address_at(pc_ + kCodeAgingTargetDelta, host_));
}


void RelocInfo::set_code_age_stub(Code* stub,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  Assembler::set_target_address_at(isolate_, pc_ + kCodeAgingTargetDelta, host_,
                                   stub->instruction_start(),
                                   icache_flush_mode);
}


Address RelocInfo::debug_call_address() {
  DCHECK(IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence());
  return Assembler::target_address_at(pc_, host_);
}


void RelocInfo::set_debug_call_address(Address target) {
  DCHECK(IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence());
  Assembler::set_target_address_at(isolate_, pc_, host_, target);
  if (host() != NULL) {
    Object* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target_code));
  }
}


void RelocInfo::WipeOut() {
  DCHECK(IsEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    Memory::Address_at(pc_) = NULL;
  } else if (IsInternalReferenceEncoded(rmode_)) {
    // mov sequence
    // Currently used only by deserializer, no need to flush.
    Assembler::set_target_address_at(isolate_, pc_, host_, NULL,
                                     SKIP_ICACHE_FLUSH);
  } else {
    Assembler::set_target_address_at(isolate_, pc_, host_, NULL);
  }
}

template <typename ObjectVisitor>
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
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    visitor->VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    visitor->VisitCodeAgeSequence(this);
  } else if (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence()) {
    visitor->VisitDebugTarget(this);
  } else if (IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(this);
  }
}


template <typename StaticVisitor>
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
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    StaticVisitor::VisitInternalReference(this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    StaticVisitor::VisitCodeAgeSequence(heap, this);
  } else if (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence()) {
    StaticVisitor::VisitDebugTarget(heap, this);
  } else if (IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}

Operand::Operand(intptr_t immediate, RelocInfo::Mode rmode) {
  rm_ = no_reg;
  imm_ = immediate;
  rmode_ = rmode;
}

Operand::Operand(const ExternalReference& f) {
  rm_ = no_reg;
  imm_ = reinterpret_cast<intptr_t>(f.address());
  rmode_ = RelocInfo::EXTERNAL_REFERENCE;
}

Operand::Operand(Smi* value) {
  rm_ = no_reg;
  imm_ = reinterpret_cast<intptr_t>(value);
  rmode_ = kRelocInfo_NONEPTR;
}

Operand::Operand(Register rm) {
  rm_ = rm;
  rmode_ = kRelocInfo_NONEPTR;  // PPC -why doesn't ARM do this?
}

void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}

void Assembler::TrackBranch() {
  DCHECK(!trampoline_emitted_);
  int count = tracked_branch_count_++;
  if (count == 0) {
    // We leave space (kMaxBlockTrampolineSectionSize)
    // for BlockTrampolinePoolScope buffer.
    next_trampoline_check_ =
        pc_offset() + kMaxCondBranchReach - kMaxBlockTrampolineSectionSize;
  } else {
    next_trampoline_check_ -= kTrampolineSlotsSize;
  }
}

void Assembler::UntrackBranch() {
  DCHECK(!trampoline_emitted_);
  DCHECK(tracked_branch_count_ > 0);
  int count = --tracked_branch_count_;
  if (count == 0) {
    // Reset
    next_trampoline_check_ = kMaxInt;
  } else {
    next_trampoline_check_ += kTrampolineSlotsSize;
  }
}

void Assembler::CheckTrampolinePoolQuick() {
  if (pc_offset() >= next_trampoline_check_) {
    CheckTrampolinePool();
  }
}

void Assembler::emit(Instr x) {
  CheckBuffer();
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
  CheckTrampolinePoolQuick();
}

bool Operand::is_reg() const { return rm_.is_valid(); }


// Fetch the 32bit value from the FIXED_SEQUENCE lis/ori
Address Assembler::target_address_at(Address pc, Address constant_pool) {
  if (FLAG_enable_embedded_constant_pool && constant_pool) {
    ConstantPoolEntry::Access access;
    if (IsConstantPoolLoadStart(pc, &access))
      return Memory::Address_at(target_constant_pool_address_at(
          pc, constant_pool, access, ConstantPoolEntry::INTPTR));
  }

  Instr instr1 = instr_at(pc);
  Instr instr2 = instr_at(pc + kInstrSize);
  // Interpret 2 instructions generated by lis/ori
  if (IsLis(instr1) && IsOri(instr2)) {
#if V8_TARGET_ARCH_PPC64
    Instr instr4 = instr_at(pc + (3 * kInstrSize));
    Instr instr5 = instr_at(pc + (4 * kInstrSize));
    // Assemble the 64 bit value.
    uint64_t hi = (static_cast<uint32_t>((instr1 & kImm16Mask) << 16) |
                   static_cast<uint32_t>(instr2 & kImm16Mask));
    uint64_t lo = (static_cast<uint32_t>((instr4 & kImm16Mask) << 16) |
                   static_cast<uint32_t>(instr5 & kImm16Mask));
    return reinterpret_cast<Address>((hi << 32) | lo);
#else
    // Assemble the 32 bit value.
    return reinterpret_cast<Address>(((instr1 & kImm16Mask) << 16) |
                                     (instr2 & kImm16Mask));
#endif
  }

  UNREACHABLE();
  return NULL;
}


#if V8_TARGET_ARCH_PPC64
const int kLoadIntptrOpcode = LD;
#else
const int kLoadIntptrOpcode = LWZ;
#endif

// Constant pool load sequence detection:
// 1) REGULAR access:
//    load <dst>, kConstantPoolRegister + <offset>
//
// 2) OVERFLOWED access:
//    addis <scratch>, kConstantPoolRegister, <offset_high>
//    load <dst>, <scratch> + <offset_low>
bool Assembler::IsConstantPoolLoadStart(Address pc,
                                        ConstantPoolEntry::Access* access) {
  Instr instr = instr_at(pc);
  int opcode = instr & kOpcodeMask;
  if (!GetRA(instr).is(kConstantPoolRegister)) return false;
  bool overflowed = (opcode == ADDIS);
#ifdef DEBUG
  if (overflowed) {
    opcode = instr_at(pc + kInstrSize) & kOpcodeMask;
  }
  DCHECK(opcode == kLoadIntptrOpcode || opcode == LFD);
#endif
  if (access) {
    *access = (overflowed ? ConstantPoolEntry::OVERFLOWED
                          : ConstantPoolEntry::REGULAR);
  }
  return true;
}


bool Assembler::IsConstantPoolLoadEnd(Address pc,
                                      ConstantPoolEntry::Access* access) {
  Instr instr = instr_at(pc);
  int opcode = instr & kOpcodeMask;
  bool overflowed = false;
  if (!(opcode == kLoadIntptrOpcode || opcode == LFD)) return false;
  if (!GetRA(instr).is(kConstantPoolRegister)) {
    instr = instr_at(pc - kInstrSize);
    opcode = instr & kOpcodeMask;
    if ((opcode != ADDIS) || !GetRA(instr).is(kConstantPoolRegister)) {
      return false;
    }
    overflowed = true;
  }
  if (access) {
    *access = (overflowed ? ConstantPoolEntry::OVERFLOWED
                          : ConstantPoolEntry::REGULAR);
  }
  return true;
}


int Assembler::GetConstantPoolOffset(Address pc,
                                     ConstantPoolEntry::Access access,
                                     ConstantPoolEntry::Type type) {
  bool overflowed = (access == ConstantPoolEntry::OVERFLOWED);
#ifdef DEBUG
  ConstantPoolEntry::Access access_check =
      static_cast<ConstantPoolEntry::Access>(-1);
  DCHECK(IsConstantPoolLoadStart(pc, &access_check));
  DCHECK(access_check == access);
#endif
  int offset;
  if (overflowed) {
    offset = (instr_at(pc) & kImm16Mask) << 16;
    offset += SIGN_EXT_IMM16(instr_at(pc + kInstrSize) & kImm16Mask);
    DCHECK(!is_int16(offset));
  } else {
    offset = SIGN_EXT_IMM16((instr_at(pc) & kImm16Mask));
  }
  return offset;
}


void Assembler::PatchConstantPoolAccessInstruction(
    int pc_offset, int offset, ConstantPoolEntry::Access access,
    ConstantPoolEntry::Type type) {
  Address pc = buffer_ + pc_offset;
  bool overflowed = (access == ConstantPoolEntry::OVERFLOWED);
  CHECK(overflowed != is_int16(offset));
#ifdef DEBUG
  ConstantPoolEntry::Access access_check =
      static_cast<ConstantPoolEntry::Access>(-1);
  DCHECK(IsConstantPoolLoadStart(pc, &access_check));
  DCHECK(access_check == access);
#endif
  if (overflowed) {
    int hi_word = static_cast<int>(offset >> 16);
    int lo_word = static_cast<int>(offset & 0xffff);
    if (lo_word & 0x8000) hi_word++;

    Instr instr1 = instr_at(pc);
    Instr instr2 = instr_at(pc + kInstrSize);
    instr1 &= ~kImm16Mask;
    instr1 |= (hi_word & kImm16Mask);
    instr2 &= ~kImm16Mask;
    instr2 |= (lo_word & kImm16Mask);
    instr_at_put(pc, instr1);
    instr_at_put(pc + kInstrSize, instr2);
  } else {
    Instr instr = instr_at(pc);
    instr &= ~kImm16Mask;
    instr |= (offset & kImm16Mask);
    instr_at_put(pc, instr);
  }
}


Address Assembler::target_constant_pool_address_at(
    Address pc, Address constant_pool, ConstantPoolEntry::Access access,
    ConstantPoolEntry::Type type) {
  Address addr = constant_pool;
  DCHECK(addr);
  addr += GetConstantPoolOffset(pc, access, type);
  return addr;
}


// This sets the branch destination (which gets loaded at the call address).
// This is for calls and branches within generated code.  The serializer
// has already deserialized the mov instructions etc.
// There is a FIXED_SEQUENCE assumption here
void Assembler::deserialization_set_special_target_at(
    Isolate* isolate, Address instruction_payload, Code* code, Address target) {
  set_target_address_at(isolate, instruction_payload, code, target);
}


void Assembler::deserialization_set_target_internal_reference_at(
    Isolate* isolate, Address pc, Address target, RelocInfo::Mode mode) {
  if (RelocInfo::IsInternalReferenceEncoded(mode)) {
    Code* code = NULL;
    set_target_address_at(isolate, pc, code, target, SKIP_ICACHE_FLUSH);
  } else {
    Memory::Address_at(pc) = target;
  }
}


// This code assumes the FIXED_SEQUENCE of lis/ori
void Assembler::set_target_address_at(Isolate* isolate, Address pc,
                                      Address constant_pool, Address target,
                                      ICacheFlushMode icache_flush_mode) {
  if (FLAG_enable_embedded_constant_pool && constant_pool) {
    ConstantPoolEntry::Access access;
    if (IsConstantPoolLoadStart(pc, &access)) {
      Memory::Address_at(target_constant_pool_address_at(
          pc, constant_pool, access, ConstantPoolEntry::INTPTR)) = target;
      return;
    }
  }

  Instr instr1 = instr_at(pc);
  Instr instr2 = instr_at(pc + kInstrSize);
  // Interpret 2 instructions generated by lis/ori
  if (IsLis(instr1) && IsOri(instr2)) {
#if V8_TARGET_ARCH_PPC64
    Instr instr4 = instr_at(pc + (3 * kInstrSize));
    Instr instr5 = instr_at(pc + (4 * kInstrSize));
    // Needs to be fixed up when mov changes to handle 64-bit values.
    uint32_t* p = reinterpret_cast<uint32_t*>(pc);
    uintptr_t itarget = reinterpret_cast<uintptr_t>(target);

    instr5 &= ~kImm16Mask;
    instr5 |= itarget & kImm16Mask;
    itarget = itarget >> 16;

    instr4 &= ~kImm16Mask;
    instr4 |= itarget & kImm16Mask;
    itarget = itarget >> 16;

    instr2 &= ~kImm16Mask;
    instr2 |= itarget & kImm16Mask;
    itarget = itarget >> 16;

    instr1 &= ~kImm16Mask;
    instr1 |= itarget & kImm16Mask;
    itarget = itarget >> 16;

    *p = instr1;
    *(p + 1) = instr2;
    *(p + 3) = instr4;
    *(p + 4) = instr5;
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      Assembler::FlushICache(isolate, p, 5 * kInstrSize);
    }
#else
    uint32_t* p = reinterpret_cast<uint32_t*>(pc);
    uint32_t itarget = reinterpret_cast<uint32_t>(target);
    int lo_word = itarget & kImm16Mask;
    int hi_word = itarget >> 16;
    instr1 &= ~kImm16Mask;
    instr1 |= hi_word;
    instr2 &= ~kImm16Mask;
    instr2 |= lo_word;

    *p = instr1;
    *(p + 1) = instr2;
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      Assembler::FlushICache(isolate, p, 2 * kInstrSize);
    }
#endif
    return;
  }
  UNREACHABLE();
}
}  // namespace internal
}  // namespace v8

#endif  // V8_PPC_ASSEMBLER_PPC_INL_H_
