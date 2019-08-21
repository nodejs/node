// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_ASSEMBLER_X64_INL_H_
#define V8_CODEGEN_X64_ASSEMBLER_X64_INL_H_

#include "src/codegen/x64/assembler-x64.h"

#include "src/base/cpu.h"
#include "src/base/memory.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(SSE4_1); }

// -----------------------------------------------------------------------------
// Implementation of Assembler

void Assembler::emitl(uint32_t x) {
  WriteUnalignedValue(reinterpret_cast<Address>(pc_), x);
  pc_ += sizeof(uint32_t);
}

void Assembler::emitq(uint64_t x) {
  WriteUnalignedValue(reinterpret_cast<Address>(pc_), x);
  pc_ += sizeof(uint64_t);
}

void Assembler::emitw(uint16_t x) {
  WriteUnalignedValue(reinterpret_cast<Address>(pc_), x);
  pc_ += sizeof(uint16_t);
}

void Assembler::emit_runtime_entry(Address entry, RelocInfo::Mode rmode) {
  DCHECK(RelocInfo::IsRuntimeEntry(rmode));
  RecordRelocInfo(rmode);
  emitl(static_cast<uint32_t>(entry - options().code_range_start));
}

void Assembler::emit(Immediate x) {
  if (!RelocInfo::IsNone(x.rmode_)) {
    RecordRelocInfo(x.rmode_);
  }
  emitl(x.value_);
}

void Assembler::emit(Immediate64 x) {
  if (!RelocInfo::IsNone(x.rmode_)) {
    RecordRelocInfo(x.rmode_);
  }
  emitq(static_cast<uint64_t>(x.value_));
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

void Assembler::emit_rex_64(XMMRegister reg, XMMRegister rm_reg) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | rm_reg.code() >> 3);
}

void Assembler::emit_rex_64(Register reg, Operand op) {
  emit(0x48 | reg.high_bit() << 2 | op.data().rex);
}

void Assembler::emit_rex_64(XMMRegister reg, Operand op) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | op.data().rex);
}

void Assembler::emit_rex_64(Register rm_reg) {
  DCHECK_EQ(rm_reg.code() & 0xf, rm_reg.code());
  emit(0x48 | rm_reg.high_bit());
}

void Assembler::emit_rex_64(Operand op) { emit(0x48 | op.data().rex); }

void Assembler::emit_rex_32(Register reg, Register rm_reg) {
  emit(0x40 | reg.high_bit() << 2 | rm_reg.high_bit());
}

void Assembler::emit_rex_32(Register reg, Operand op) {
  emit(0x40 | reg.high_bit() << 2 | op.data().rex);
}

void Assembler::emit_rex_32(Register rm_reg) { emit(0x40 | rm_reg.high_bit()); }

void Assembler::emit_rex_32(Operand op) { emit(0x40 | op.data().rex); }

void Assembler::emit_optional_rex_32(Register reg, Register rm_reg) {
  byte rex_bits = reg.high_bit() << 2 | rm_reg.high_bit();
  if (rex_bits != 0) emit(0x40 | rex_bits);
}

void Assembler::emit_optional_rex_32(Register reg, Operand op) {
  byte rex_bits = reg.high_bit() << 2 | op.data().rex;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}

void Assembler::emit_optional_rex_32(XMMRegister reg, Operand op) {
  byte rex_bits = (reg.code() & 0x8) >> 1 | op.data().rex;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}

void Assembler::emit_optional_rex_32(XMMRegister reg, XMMRegister base) {
  byte rex_bits = (reg.code() & 0x8) >> 1 | (base.code() & 0x8) >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}

void Assembler::emit_optional_rex_32(XMMRegister reg, Register base) {
  byte rex_bits = (reg.code() & 0x8) >> 1 | (base.code() & 0x8) >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}

void Assembler::emit_optional_rex_32(Register reg, XMMRegister base) {
  byte rex_bits = (reg.code() & 0x8) >> 1 | (base.code() & 0x8) >> 3;
  if (rex_bits != 0) emit(0x40 | rex_bits);
}

void Assembler::emit_optional_rex_32(Register rm_reg) {
  if (rm_reg.high_bit()) emit(0x41);
}

void Assembler::emit_optional_rex_32(XMMRegister rm_reg) {
  if (rm_reg.high_bit()) emit(0x41);
}

void Assembler::emit_optional_rex_32(Operand op) {
  if (op.data().rex != 0) emit(0x40 | op.data().rex);
}

// byte 1 of 3-byte VEX
void Assembler::emit_vex3_byte1(XMMRegister reg, XMMRegister rm,
                                LeadingOpcode m) {
  byte rxb = static_cast<byte>(~((reg.high_bit() << 2) | rm.high_bit())) << 5;
  emit(rxb | m);
}

// byte 1 of 3-byte VEX
void Assembler::emit_vex3_byte1(XMMRegister reg, Operand rm, LeadingOpcode m) {
  byte rxb = static_cast<byte>(~((reg.high_bit() << 2) | rm.data().rex)) << 5;
  emit(rxb | m);
}

// byte 1 of 2-byte VEX
void Assembler::emit_vex2_byte1(XMMRegister reg, XMMRegister v, VectorLength l,
                                SIMDPrefix pp) {
  byte rv = static_cast<byte>(~((reg.high_bit() << 4) | v.code())) << 3;
  emit(rv | l | pp);
}

// byte 2 of 3-byte VEX
void Assembler::emit_vex3_byte2(VexW w, XMMRegister v, VectorLength l,
                                SIMDPrefix pp) {
  emit(w | ((~v.code() & 0xf) << 3) | l | pp);
}

void Assembler::emit_vex_prefix(XMMRegister reg, XMMRegister vreg,
                                XMMRegister rm, VectorLength l, SIMDPrefix pp,
                                LeadingOpcode mm, VexW w) {
  if (rm.high_bit() || mm != k0F || w != kW0) {
    emit_vex3_byte0();
    emit_vex3_byte1(reg, rm, mm);
    emit_vex3_byte2(w, vreg, l, pp);
  } else {
    emit_vex2_byte0();
    emit_vex2_byte1(reg, vreg, l, pp);
  }
}

void Assembler::emit_vex_prefix(Register reg, Register vreg, Register rm,
                                VectorLength l, SIMDPrefix pp, LeadingOpcode mm,
                                VexW w) {
  XMMRegister ireg = XMMRegister::from_code(reg.code());
  XMMRegister ivreg = XMMRegister::from_code(vreg.code());
  XMMRegister irm = XMMRegister::from_code(rm.code());
  emit_vex_prefix(ireg, ivreg, irm, l, pp, mm, w);
}

void Assembler::emit_vex_prefix(XMMRegister reg, XMMRegister vreg, Operand rm,
                                VectorLength l, SIMDPrefix pp, LeadingOpcode mm,
                                VexW w) {
  if (rm.data().rex || mm != k0F || w != kW0) {
    emit_vex3_byte0();
    emit_vex3_byte1(reg, rm, mm);
    emit_vex3_byte2(w, vreg, l, pp);
  } else {
    emit_vex2_byte0();
    emit_vex2_byte1(reg, vreg, l, pp);
  }
}

void Assembler::emit_vex_prefix(Register reg, Register vreg, Operand rm,
                                VectorLength l, SIMDPrefix pp, LeadingOpcode mm,
                                VexW w) {
  XMMRegister ireg = XMMRegister::from_code(reg.code());
  XMMRegister ivreg = XMMRegister::from_code(vreg.code());
  emit_vex_prefix(ireg, ivreg, rm, l, pp, mm, w);
}

Address Assembler::target_address_at(Address pc, Address constant_pool) {
  return ReadUnalignedValue<int32_t>(pc) + pc + 4;
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  WriteUnalignedValue(pc, static_cast<int32_t>(target - pc - 4));
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc, sizeof(int32_t));
  }
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  WriteUnalignedValue(pc, target);
}

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

Handle<Code> Assembler::code_target_object_handle_at(Address pc) {
  return GetCodeTarget(ReadUnalignedValue<int32_t>(pc));
}

Handle<HeapObject> Assembler::compressed_embedded_object_handle_at(Address pc) {
  return GetEmbeddedObject(ReadUnalignedValue<uint32_t>(pc));
}

Address Assembler::runtime_entry_at(Address pc) {
  return ReadUnalignedValue<int32_t>(pc) + options().code_range_start;
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(intptr_t delta) {
  if (IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)) {
    WriteUnalignedValue(
        pc_, ReadUnalignedValue<int32_t>(pc_) - static_cast<int32_t>(delta));
  } else if (IsInternalReference(rmode_)) {
    // Absolute code pointer inside code object moves with the code object.
    WriteUnalignedValue(pc_, ReadUnalignedValue<Address>(pc_) + delta);
  }
}

Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_) ||
         IsWasmStubCall(rmode_) || IsFullEmbeddedObject(rmode_) ||
         IsCompressedEmbeddedObject(rmode_) || IsExternalReference(rmode_) ||
         IsOffHeapTarget(rmode_));
  return pc_;
}

Address RelocInfo::constant_pool_entry_address() { UNREACHABLE(); }

int RelocInfo::target_address_size() {
  if (IsCodedSpecially()) {
    return Assembler::kSpecialTargetSize;
  } else {
    return IsCompressedEmbeddedObject(rmode_) ? kTaggedSize
                                              : kSystemPointerSize;
  }
}

HeapObject RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || IsEmbeddedObjectMode(rmode_));
  if (IsCompressedEmbeddedObject(rmode_)) {
    CHECK(!host_.is_null());
    Object o = static_cast<Object>(DecompressTaggedPointer(
        host_.ptr(), ReadUnalignedValue<Tagged_t>(pc_)));
    return HeapObject::cast(o);
  }
  return HeapObject::cast(Object(ReadUnalignedValue<Address>(pc_)));
}

HeapObject RelocInfo::target_object_no_host(Isolate* isolate) {
  DCHECK(IsCodeTarget(rmode_) || IsEmbeddedObjectMode(rmode_));
  if (IsCompressedEmbeddedObject(rmode_)) {
    Tagged_t compressed = ReadUnalignedValue<Tagged_t>(pc_);
    DCHECK(!HAS_SMI_TAG(compressed));
    Object obj(DecompressTaggedPointer(isolate, compressed));
    return HeapObject::cast(obj);
  }
  return HeapObject::cast(Object(ReadUnalignedValue<Address>(pc_)));
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || IsEmbeddedObjectMode(rmode_));
  if (IsCodeTarget(rmode_)) {
    return origin->code_target_object_handle_at(pc_);
  } else {
    if (IsCompressedEmbeddedObject(rmode_)) {
      return origin->compressed_embedded_object_handle_at(pc_);
    }
    return Handle<HeapObject>::cast(ReadUnalignedValue<Handle<Object>>(pc_));
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  return ReadUnalignedValue<Address>(pc_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  WriteUnalignedValue(pc_, target);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc_, sizeof(Address));
  }
}

Address RelocInfo::target_internal_reference() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return ReadUnalignedValue<Address>(pc_);
}

Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return pc_;
}

void RelocInfo::set_target_object(Heap* heap, HeapObject target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsEmbeddedObjectMode(rmode_));
  if (IsCompressedEmbeddedObject(rmode_)) {
    DCHECK(COMPRESS_POINTERS_BOOL);
    Tagged_t tagged = CompressTagged(target.ptr());
    WriteUnalignedValue(pc_, tagged);
  } else {
    WriteUnalignedValue(pc_, target.ptr());
  }
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    FlushInstructionCache(pc_, sizeof(Address));
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && !host().is_null()) {
    WriteBarrierForCode(host(), this, target);
  }
}

Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return origin->runtime_entry_at(pc_);
}

void RelocInfo::set_target_runtime_entry(Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target) {
    set_target_address(target, write_barrier_mode, icache_flush_mode);
  }
}

Address RelocInfo::target_off_heap_target() {
  DCHECK(IsOffHeapTarget(rmode_));
  return ReadUnalignedValue<Address>(pc_);
}

void RelocInfo::WipeOut() {
  if (IsFullEmbeddedObject(rmode_) || IsExternalReference(rmode_) ||
      IsInternalReference(rmode_) || IsOffHeapTarget(rmode_)) {
    WriteUnalignedValue(pc_, kNullAddress);
  } else if (IsCompressedEmbeddedObject(rmode_)) {
    Address smi_address = Smi::FromInt(0).ptr();
    WriteUnalignedValue(pc_, CompressTagged(smi_address));
  } else if (IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)) {
    // Effectively write zero into the relocation.
    Assembler::set_target_address_at(pc_, constant_pool_,
                                     pc_ + sizeof(int32_t));
  } else {
    UNREACHABLE();
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_X64_ASSEMBLER_X64_INL_H_
