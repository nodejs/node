
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

#include "src/mips64/assembler-mips64.h"

#include "src/assembler.h"
#include "src/debug/debug.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


bool CpuFeatures::SupportsCrankshaft() { return IsSupported(FPU); }

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(MIPS_SIMD); }

// -----------------------------------------------------------------------------
// Operand and MemOperand.

bool Operand::is_reg() const {
  return rm_.is_valid();
}

int64_t Operand::immediate() const {
  DCHECK(!is_reg());
  DCHECK(!IsHeapObjectRequest());
  return value_.immediate;
}

// -----------------------------------------------------------------------------
// RelocInfo.

void RelocInfo::apply(intptr_t delta) {
  if (IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    byte* p = reinterpret_cast<byte*>(pc_);
    Assembler::RelocateInternalReference(rmode_, p, delta);
  }
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) ||
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
  // return reinterpret_cast<Address>(
  //  pc_ + Assembler::kInstructionsFor32BitConstant * Assembler::kInstrSize);
  return reinterpret_cast<Address>(
    pc_ + Assembler::kInstructionsFor64BitConstant * Assembler::kInstrSize);
}


Address RelocInfo::constant_pool_entry_address() {
  UNREACHABLE();
}


int RelocInfo::target_address_size() {
  return Assembler::kSpecialTargetSize;
}

Address Assembler::target_address_from_return_address(Address pc) {
  return pc - kCallTargetAddressOffset;
}

void Assembler::deserialization_set_special_target_at(
    Isolate* isolate, Address instruction_payload, Code* code, Address target) {
  set_target_address_at(
      isolate, instruction_payload - kInstructionsFor64BitConstant * kInstrSize,
      code ? code->constant_pool() : nullptr, target);
}

void Assembler::set_target_internal_reference_encoded_at(Address pc,
                                                         Address target) {
  // Encoded internal references are j/jal instructions.
  Instr instr = Assembler::instr_at(pc + 0 * Assembler::kInstrSize);

  uint64_t imm28 =
      (reinterpret_cast<uint64_t>(target) & static_cast<uint64_t>(kImm28Mask));

  instr &= ~kImm26Mask;
  uint64_t imm26 = imm28 >> 2;
  DCHECK(is_uint26(imm26));

  instr_at_put(pc, instr | (imm26 & kImm26Mask));
  // Currently used only by deserializer, and all code will be flushed
  // after complete deserialization, no need to flush on each reference.
}


void Assembler::deserialization_set_target_internal_reference_at(
    Isolate* isolate, Address pc, Address target, RelocInfo::Mode mode) {
  if (mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    DCHECK(IsJ(instr_at(pc)));
    set_target_internal_reference_encoded_at(pc, target);
  } else {
    DCHECK(mode == RelocInfo::INTERNAL_REFERENCE);
    Memory::Address_at(pc) = target;
  }
}

HeapObject* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return HeapObject::cast(reinterpret_cast<Object*>(
      Assembler::target_address_at(pc_, constant_pool_)));
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<HeapObject>(reinterpret_cast<HeapObject**>(
      Assembler::target_address_at(pc_, constant_pool_)));
}

void RelocInfo::set_target_object(HeapObject* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Assembler::set_target_address_at(target->GetIsolate(), pc_, constant_pool_,
                                   reinterpret_cast<Address>(target),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != nullptr &&
      target->IsHeapObject()) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(
        host(), this, HeapObject::cast(target));
    host()->GetHeap()->RecordWriteIntoCode(host(), this, target);
  }
}


Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, constant_pool_);
}


Address RelocInfo::target_internal_reference() {
  if (rmode_ == INTERNAL_REFERENCE) {
    return Memory::Address_at(pc_);
  } else {
    // Encoded internal references are j/jal instructions.
    DCHECK(rmode_ == INTERNAL_REFERENCE_ENCODED);
    Instr instr = Assembler::instr_at(pc_ + 0 * Assembler::kInstrSize);
    instr &= kImm26Mask;
    uint64_t imm28 = instr << 2;
    uint64_t segment =
        (reinterpret_cast<uint64_t>(pc_) & ~static_cast<uint64_t>(kImm28Mask));
    return reinterpret_cast<Address>(segment | imm28);
  }
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE || rmode_ == INTERNAL_REFERENCE_ENCODED);
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}

void RelocInfo::set_target_runtime_entry(Isolate* isolate, Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target)
    set_target_address(isolate, target, write_barrier_mode, icache_flush_mode);
}

void RelocInfo::WipeOut(Isolate* isolate) {
  DCHECK(IsEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  if (IsInternalReference(rmode_)) {
    Memory::Address_at(pc_) = nullptr;
  } else if (IsInternalReferenceEncoded(rmode_)) {
    Assembler::set_target_internal_reference_encoded_at(pc_, nullptr);
  } else {
    Assembler::set_target_address_at(isolate, pc_, constant_pool_, nullptr);
  }
}

template <typename ObjectVisitor>
void RelocInfo::Visit(Isolate* isolate, ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(host(), this);
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(host(), this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(host(), this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE ||
             mode == RelocInfo::INTERNAL_REFERENCE_ENCODED) {
    visitor->VisitInternalReference(host(), this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(host(), this);
  }
}

// -----------------------------------------------------------------------------
// Assembler.


void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}


void Assembler::CheckTrampolinePoolQuick(int extra_instructions) {
  if (pc_offset() >= next_buffer_check_ - extra_instructions * kInstrSize) {
    CheckTrampolinePool();
  }
}


void Assembler::CheckForEmitInForbiddenSlot() {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  if (IsPrevInstrCompactBranch()) {
    // Nop instruction to precede a CTI in forbidden slot:
    Instr nop = SPECIAL | SLL;
    *reinterpret_cast<Instr*>(pc_) = nop;
    pc_ += kInstrSize;

    ClearCompactBranchState();
  }
}


void Assembler::EmitHelper(Instr x, CompactBranchType is_compact_branch) {
  if (IsPrevInstrCompactBranch()) {
    if (Instruction::IsForbiddenAfterBranchInstr(x)) {
      // Nop instruction to precede a CTI in forbidden slot:
      Instr nop = SPECIAL | SLL;
      *reinterpret_cast<Instr*>(pc_) = nop;
      pc_ += kInstrSize;
    }
    ClearCompactBranchState();
  }
  *reinterpret_cast<Instr*>(pc_) = x;
  pc_ += kInstrSize;
  if (is_compact_branch == CompactBranchType::COMPACT_BRANCH) {
    EmittedCompactBranchInstruction();
  }
  CheckTrampolinePoolQuick();
}

template <>
inline void Assembler::EmitHelper(uint8_t x);

template <typename T>
void Assembler::EmitHelper(T x) {
  *reinterpret_cast<T*>(pc_) = x;
  pc_ += sizeof(x);
  CheckTrampolinePoolQuick();
}

template <>
void Assembler::EmitHelper(uint8_t x) {
  *reinterpret_cast<uint8_t*>(pc_) = x;
  pc_ += sizeof(x);
  if (reinterpret_cast<intptr_t>(pc_) % kInstrSize == 0) {
    CheckTrampolinePoolQuick();
  }
}

void Assembler::emit(Instr x, CompactBranchType is_compact_branch) {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  EmitHelper(x, is_compact_branch);
}


void Assembler::emit(uint64_t data) {
  CheckForEmitInForbiddenSlot();
  EmitHelper(data);
}

EnsureSpace::EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_MIPS_ASSEMBLER_MIPS_INL_H_
