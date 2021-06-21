
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

#ifndef V8_CODEGEN_MIPS_ASSEMBLER_MIPS_INL_H_
#define V8_CODEGEN_MIPS_ASSEMBLER_MIPS_INL_H_

#include "src/codegen/mips/assembler-mips.h"

#include "src/codegen/assembler.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return IsSupported(FPU); }

// -----------------------------------------------------------------------------
// Operand and MemOperand.

bool Operand::is_reg() const { return rm_.is_valid(); }

int32_t Operand::immediate() const {
  DCHECK(!is_reg());
  DCHECK(!IsHeapObjectRequest());
  return value_.immediate;
}

// -----------------------------------------------------------------------------
// RelocInfo.

void RelocInfo::apply(intptr_t delta) {
  if (IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    Assembler::RelocateInternalReference(rmode_, pc_, delta);
  } else if (IsRelativeCodeTarget(rmode_)) {
    Assembler::RelocateRelativeReference(rmode_, pc_, delta);
  }
}

Address RelocInfo::target_address() {
  DCHECK(IsCodeTargetMode(rmode_) || IsRuntimeEntry(rmode_) ||
         IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(HasTargetAddressAddress());
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
  if (IsMipsArchVariant(kMips32r6)) {
    // On R6 we don't move to the end of the instructions to be patched, but one
    // instruction before, because if these instructions are at the end of the
    // code object it can cause errors in the deserializer.
    return pc_ + (Assembler::kInstructionsFor32BitConstant - 1) * kInstrSize;
  } else {
    return pc_ + Assembler::kInstructionsFor32BitConstant * kInstrSize;
  }
}

Address RelocInfo::constant_pool_entry_address() { UNREACHABLE(); }

int RelocInfo::target_address_size() { return Assembler::kSpecialTargetSize; }

void Assembler::deserialization_set_special_target_at(
    Address instruction_payload, Code code, Address target) {
  set_target_address_at(instruction_payload,
                        !code.is_null() ? code.constant_pool() : kNullAddress,
                        target);
}

int Assembler::deserialization_special_target_size(
    Address instruction_payload) {
  return kSpecialTargetSize;
}

void Assembler::set_target_internal_reference_encoded_at(Address pc,
                                                         Address target) {
  Instr instr1 = Assembler::instr_at(pc + 0 * kInstrSize);
  Instr instr2 = Assembler::instr_at(pc + 1 * kInstrSize);
  DCHECK(Assembler::IsLui(instr1));
  DCHECK(Assembler::IsOri(instr2) || Assembler::IsJicOrJialc(instr2));
  instr1 &= ~kImm16Mask;
  instr2 &= ~kImm16Mask;
  int32_t imm = static_cast<int32_t>(target);
  DCHECK_EQ(imm & 3, 0);
  if (Assembler::IsJicOrJialc(instr2)) {
    // Encoded internal references are lui/jic load of 32-bit absolute address.
    uint32_t lui_offset_u, jic_offset_u;
    Assembler::UnpackTargetAddressUnsigned(imm, &lui_offset_u, &jic_offset_u);

    Assembler::instr_at_put(pc + 0 * kInstrSize, instr1 | lui_offset_u);
    Assembler::instr_at_put(pc + 1 * kInstrSize, instr2 | jic_offset_u);
  } else {
    // Encoded internal references are lui/ori load of 32-bit absolute address.
    PatchLuiOriImmediate(pc, imm, instr1, 0 * kInstrSize, instr2,
                         1 * kInstrSize);
  }

  // Currently used only by deserializer, and all code will be flushed
  // after complete deserialization, no need to flush on each reference.
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  if (RelocInfo::IsInternalReferenceEncoded(mode)) {
    DCHECK(IsLui(instr_at(pc)));
    set_target_internal_reference_encoded_at(pc, target);
  } else {
    DCHECK(RelocInfo::IsInternalReference(mode));
    Memory<Address>(pc) = target;
  }
}

HeapObject RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_) ||
         IsDataEmbeddedObject(rmode_));
  if (IsDataEmbeddedObject(rmode_)) {
    return HeapObject::cast(Object(ReadUnalignedValue<Address>(pc_)));
  }
  return HeapObject::cast(
      Object(Assembler::target_address_at(pc_, constant_pool_)));
}

HeapObject RelocInfo::target_object_no_host(Isolate* isolate) {
  return target_object();
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  if (IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_)) {
    return Handle<HeapObject>(reinterpret_cast<Address*>(
        Assembler::target_address_at(pc_, constant_pool_)));
  } else if (IsDataEmbeddedObject(rmode_)) {
    return Handle<HeapObject>::cast(ReadUnalignedValue<Handle<Object>>(pc_));
  }
  DCHECK(IsRelativeCodeTarget(rmode_));
  return origin->relative_code_target_object_handle_at(pc_);
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_) ||
         IsDataEmbeddedObject(rmode_));
  if (IsDataEmbeddedObject(rmode_)) {
    WriteUnalignedValue(pc_, target.ptr());
    // No need to flush icache since no instructions were changed.
  } else {
    Assembler::set_target_address_at(pc_, constant_pool_, target.ptr(),
                                     icache_flush_mode);
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && !host().is_null() &&
      !FLAG_disable_write_barriers) {
    WriteBarrierForCode(host(), this, target);
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(IsExternalReference(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(IsExternalReference(rmode_));
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
}

Address RelocInfo::target_internal_reference() {
  if (IsInternalReference(rmode_)) {
    return Memory<Address>(pc_);
  } else {
    // Encoded internal references are lui/ori or lui/jic load of 32-bit
    // absolute address.
    DCHECK(IsInternalReferenceEncoded(rmode_));
    Instr instr1 = Assembler::instr_at(pc_ + 0 * kInstrSize);
    Instr instr2 = Assembler::instr_at(pc_ + 1 * kInstrSize);
    DCHECK(Assembler::IsLui(instr1));
    DCHECK(Assembler::IsOri(instr2) || Assembler::IsJicOrJialc(instr2));
    if (Assembler::IsJicOrJialc(instr2)) {
      return static_cast<Address>(
          Assembler::CreateTargetAddress(instr1, instr2));
    }
    return static_cast<Address>(Assembler::GetLuiOriImmediate(instr1, instr2));
  }
}

Address RelocInfo::target_internal_reference_address() {
  DCHECK(IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  return pc_;
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

Address RelocInfo::target_off_heap_target() {
  DCHECK(IsOffHeapTarget(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::WipeOut() {
  DCHECK(IsFullEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_) ||
         IsOffHeapTarget(rmode_));
  if (IsInternalReference(rmode_)) {
    Memory<Address>(pc_) = kNullAddress;
  } else if (IsInternalReferenceEncoded(rmode_)) {
    Assembler::set_target_internal_reference_encoded_at(pc_, kNullAddress);
  } else {
    Assembler::set_target_address_at(pc_, constant_pool_, kNullAddress);
  }
}

Handle<Code> Assembler::relative_code_target_object_handle_at(
    Address pc) const {
  Instr instr1 = instr_at(pc);
  Instr instr2 = instr_at(pc + kInstrSize);
  DCHECK(IsLui(instr1));
  DCHECK(IsOri(instr2) || IsNal(instr2));
  DCHECK(IsNal(instr2) || IsNal(instr_at(pc - kInstrSize)));
  if (IsNal(instr2)) {
    instr2 = instr_at(pc + 2 * kInstrSize);
  }
  // Interpret 2 instructions generated by li (lui/ori).
  int code_target_index = GetLuiOriImmediate(instr1, instr2);
  return GetCodeTarget(code_target_index);
}

// -----------------------------------------------------------------------------
// Assembler.

void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
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

EnsureSpace::EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_MIPS_ASSEMBLER_MIPS_INL_H_
