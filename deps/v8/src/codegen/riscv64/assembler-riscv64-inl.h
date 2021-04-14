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
// Copyright 2021 the V8 project authors. All rights reserved.

#ifndef V8_CODEGEN_RISCV64_ASSEMBLER_RISCV64_INL_H_
#define V8_CODEGEN_RISCV64_ASSEMBLER_RISCV64_INL_H_

#include "src/codegen/assembler.h"
#include "src/codegen/riscv64/assembler-riscv64.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return IsSupported(FPU); }

// -----------------------------------------------------------------------------
// Operand and MemOperand.

bool Operand::is_reg() const { return rm_.is_valid(); }

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
    Assembler::RelocateInternalReference(rmode_, pc_, delta);
  } else {
    DCHECK(IsRelativeCodeTarget(rmode_));
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
  return pc_ + Assembler::kInstructionsFor64BitConstant * kInstrSize;
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
  set_target_value_at(pc, static_cast<uint64_t>(target));
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
  DCHECK(IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_));
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
  } else {
    DCHECK(IsRelativeCodeTarget(rmode_));
    return origin->relative_code_target_object_handle_at(pc_);
  }
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsFullEmbeddedObject(rmode_));
  Assembler::set_target_address_at(pc_, constant_pool_, target.ptr(),
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && !host().is_null() &&
      !FLAG_disable_write_barriers) {
    WriteBarrierForCode(host(), this, target);
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == EXTERNAL_REFERENCE);
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
}

Address RelocInfo::target_internal_reference() {
  if (IsInternalReference(rmode_)) {
    return Memory<Address>(pc_);
  } else {
    // Encoded internal references are j/jal instructions.
    DCHECK(IsInternalReferenceEncoded(rmode_));
    DCHECK(Assembler::IsLui(Assembler::instr_at(pc_ + 0 * kInstrSize)));
    Address address = Assembler::target_address_at(pc_);
    return address;
  }
}

Address RelocInfo::target_internal_reference_address() {
  DCHECK(IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  return pc_;
}

Handle<Code> Assembler::relative_code_target_object_handle_at(
    Address pc) const {
  Instr instr1 = Assembler::instr_at(pc);
  Instr instr2 = Assembler::instr_at(pc + kInstrSize);
  DCHECK(IsAuipc(instr1));
  DCHECK(IsJalr(instr2));
  int32_t code_target_index = BrachlongOffset(instr1, instr2);
  return GetCodeTarget(code_target_index);
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

// -----------------------------------------------------------------------------
// Assembler.

void Assembler::CheckBuffer() {
  if (buffer_space() <= kGap) {
    GrowBuffer();
  }
}

template <typename T>
void Assembler::EmitHelper(T x) {
  *reinterpret_cast<T*>(pc_) = x;
  pc_ += sizeof(x);
}

void Assembler::emit(Instr x) {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  DEBUG_PRINTF("%p: ", pc_);
  disassembleInstr(x);
  EmitHelper(x);
  CheckTrampolinePoolQuick();
}

void Assembler::emit(ShortInstr x) {
  if (!is_buffer_growth_blocked()) {
    CheckBuffer();
  }
  DEBUG_PRINTF("%p: ", pc_);
  disassembleInstr(x);
  EmitHelper(x);
  CheckTrampolinePoolQuick();
}

void Assembler::emit(uint64_t data) {
  if (!is_buffer_growth_blocked()) CheckBuffer();
  EmitHelper(data);
}

EnsureSpace::EnsureSpace(Assembler* assembler) { assembler->CheckBuffer(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV64_ASSEMBLER_RISCV64_INL_H_
