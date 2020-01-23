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

#ifndef V8_CODEGEN_S390_ASSEMBLER_S390_INL_H_
#define V8_CODEGEN_S390_ASSEMBLER_S390_INL_H_

#include "src/codegen/s390/assembler-s390.h"

#include "src/codegen/assembler.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return false; }

void RelocInfo::apply(intptr_t delta) {
  // Absolute code pointer inside code object moves with the code object.
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    Address target = Memory<Address>(pc_);
    Memory<Address>(pc_) = target + delta;
  } else if (IsCodeTarget(rmode_)) {
    SixByteInstr instr =
        Instruction::InstructionBits(reinterpret_cast<const byte*>(pc_));
    int32_t dis = static_cast<int32_t>(instr & 0xFFFFFFFF) * 2  // halfwords
                  - static_cast<int32_t>(delta);
    instr >>= 32;  // Clear the 4-byte displacement field.
    instr <<= 32;
    instr |= static_cast<uint32_t>(dis / 2);
    Instruction::SetInstructionBits<SixByteInstr>(reinterpret_cast<byte*>(pc_),
                                                  instr);
  } else {
    // mov sequence
    DCHECK(IsInternalReferenceEncoded(rmode_));
    Address target = Assembler::target_address_at(pc_, constant_pool_);
    Assembler::set_target_address_at(pc_, constant_pool_, target + delta,
                                     SKIP_ICACHE_FLUSH);
  }
}

Address RelocInfo::target_internal_reference() {
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    return Memory<Address>(pc_);
  } else {
    // mov sequence
    DCHECK(IsInternalReferenceEncoded(rmode_));
    return Assembler::target_address_at(pc_, constant_pool_);
  }
}

Address RelocInfo::target_internal_reference_address() {
  DCHECK(IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_));
  return pc_;
}

Address RelocInfo::target_address() {
  DCHECK(IsRelativeCodeTarget(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(HasTargetAddressAddress());

  // Read the address of the word containing the target_address in an
  // instruction stream.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.
  // For an instruction like LIS/ORI where the target bits are mixed into the
  // instruction bits, the size of the target will be zero, indicating that the
  // serializer should not step forward in memory after a target is resolved
  // and written.
  return pc_;
}

Address RelocInfo::constant_pool_entry_address() { UNREACHABLE(); }

int RelocInfo::target_address_size() { return Assembler::kSpecialTargetSize; }

Handle<Object> Assembler::code_target_object_handle_at(Address pc) {
  SixByteInstr instr =
      Instruction::InstructionBits(reinterpret_cast<const byte*>(pc));
  int index = instr & 0xFFFFFFFF;
  return GetCodeTarget(index);
}

HeapObject RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == FULL_EMBEDDED_OBJECT);
  return HeapObject::cast(
      Object(Assembler::target_address_at(pc_, constant_pool_)));
}

HeapObject RelocInfo::target_object_no_host(Isolate* isolate) {
  return target_object();
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsRelativeCodeTarget(rmode_) || IsCodeTarget(rmode_) ||
         rmode_ == FULL_EMBEDDED_OBJECT);
  if (rmode_ == FULL_EMBEDDED_OBJECT) {
    return Handle<HeapObject>(reinterpret_cast<Address*>(
        Assembler::target_address_at(pc_, constant_pool_)));
  } else {
    return Handle<HeapObject>::cast(origin->code_target_object_handle_at(pc_));
  }
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == FULL_EMBEDDED_OBJECT);
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

Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return target_address();
}

Address RelocInfo::target_off_heap_target() {
  DCHECK(IsOffHeapTarget(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target)
    set_target_address(target, write_barrier_mode, icache_flush_mode);
}

void RelocInfo::WipeOut() {
  DCHECK(IsFullEmbeddedObject(rmode_) || IsCodeTarget(rmode_) ||
         IsRuntimeEntry(rmode_) || IsExternalReference(rmode_) ||
         IsInternalReference(rmode_) || IsInternalReferenceEncoded(rmode_) ||
         IsOffHeapTarget(rmode_));
  if (IsInternalReference(rmode_)) {
    // Jump table entry
    Memory<Address>(pc_) = kNullAddress;
  } else if (IsInternalReferenceEncoded(rmode_) || IsOffHeapTarget(rmode_)) {
    // mov sequence
    // Currently used only by deserializer, no need to flush.
    Assembler::set_target_address_at(pc_, constant_pool_, kNullAddress,
                                     SKIP_ICACHE_FLUSH);
  } else {
    Assembler::set_target_address_at(pc_, constant_pool_, kNullAddress);
  }
}

// Operand constructors
Operand::Operand(Register rm) : rm_(rm), rmode_(RelocInfo::NONE) {}

// Fetch the 32bit value from the FIXED_SEQUENCE IIHF / IILF
Address Assembler::target_address_at(Address pc, Address constant_pool) {
  // S390 Instruction!
  // We want to check for instructions generated by Asm::mov()
  Opcode op1 = Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc));
  SixByteInstr instr_1 =
      Instruction::InstructionBits(reinterpret_cast<const byte*>(pc));

  if (BRASL == op1 || BRCL == op1) {
    int32_t dis = static_cast<int32_t>(instr_1 & 0xFFFFFFFF) * 2;
    return pc + dis;
  }

#if V8_TARGET_ARCH_S390X
  int instr1_length =
      Instruction::InstructionLength(reinterpret_cast<const byte*>(pc));
  Opcode op2 = Instruction::S390OpcodeValue(
      reinterpret_cast<const byte*>(pc + instr1_length));
  SixByteInstr instr_2 = Instruction::InstructionBits(
      reinterpret_cast<const byte*>(pc + instr1_length));
  // IIHF for hi_32, IILF for lo_32
  if (IIHF == op1 && IILF == op2) {
    return static_cast<Address>(((instr_1 & 0xFFFFFFFF) << 32) |
                                ((instr_2 & 0xFFFFFFFF)));
  }
#else
  // IILF loads 32-bits
  if (IILF == op1 || CFI == op1) {
    return static_cast<Address>((instr_1 & 0xFFFFFFFF));
  }
#endif

  UNIMPLEMENTED();
  return 0;
}

// This sets the branch destination (which gets loaded at the call address).
// This is for calls and branches within generated code.  The serializer
// has already deserialized the mov instructions etc.
// There is a FIXED_SEQUENCE assumption here
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

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  if (RelocInfo::IsInternalReferenceEncoded(mode)) {
    set_target_address_at(pc, kNullAddress, target, SKIP_ICACHE_FLUSH);
  } else {
    Memory<Address>(pc) = target;
  }
}

// This code assumes the FIXED_SEQUENCE of IIHF/IILF
void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  // Check for instructions generated by Asm::mov()
  Opcode op1 = Instruction::S390OpcodeValue(reinterpret_cast<const byte*>(pc));
  SixByteInstr instr_1 =
      Instruction::InstructionBits(reinterpret_cast<const byte*>(pc));
  bool patched = false;

  if (BRASL == op1 || BRCL == op1) {
    instr_1 >>= 32;  // Zero out the lower 32-bits
    instr_1 <<= 32;
    int32_t halfwords = (target - pc) / 2;  // number of halfwords
    instr_1 |= static_cast<uint32_t>(halfwords);
    Instruction::SetInstructionBits<SixByteInstr>(reinterpret_cast<byte*>(pc),
                                                  instr_1);
    if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
      FlushInstructionCache(pc, 6);
    }
    patched = true;
  } else {
#if V8_TARGET_ARCH_S390X
    int instr1_length =
        Instruction::InstructionLength(reinterpret_cast<const byte*>(pc));
    Opcode op2 = Instruction::S390OpcodeValue(
        reinterpret_cast<const byte*>(pc + instr1_length));
    SixByteInstr instr_2 = Instruction::InstructionBits(
        reinterpret_cast<const byte*>(pc + instr1_length));
    // IIHF for hi_32, IILF for lo_32
    if (IIHF == op1 && IILF == op2) {
      // IIHF
      instr_1 >>= 32;  // Zero out the lower 32-bits
      instr_1 <<= 32;
      instr_1 |= reinterpret_cast<uint64_t>(target) >> 32;

      Instruction::SetInstructionBits<SixByteInstr>(reinterpret_cast<byte*>(pc),
                                                    instr_1);

      // IILF
      instr_2 >>= 32;
      instr_2 <<= 32;
      instr_2 |= reinterpret_cast<uint64_t>(target) & 0xFFFFFFFF;

      Instruction::SetInstructionBits<SixByteInstr>(
          reinterpret_cast<byte*>(pc + instr1_length), instr_2);
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        FlushInstructionCache(pc, 12);
      }
      patched = true;
    }
#else
    // IILF loads 32-bits
    if (IILF == op1 || CFI == op1) {
      instr_1 >>= 32;  // Zero out the lower 32-bits
      instr_1 <<= 32;
      instr_1 |= reinterpret_cast<uint32_t>(target);

      Instruction::SetInstructionBits<SixByteInstr>(reinterpret_cast<byte*>(pc),
                                                    instr_1);
      if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
        FlushInstructionCache(pc, 6);
      }
      patched = true;
    }
#endif
  }
  if (!patched) UNREACHABLE();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_S390_ASSEMBLER_S390_INL_H_
