// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_ASSEMBLER_X64_INL_H_
#define V8_X64_ASSEMBLER_X64_INL_H_

#include "src/x64/assembler-x64.h"

#include "src/base/cpu.h"
#include "src/debug/debug.h"
#include "src/objects-inl.h"
#include "src/v8memory.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(SSE4_1); }

// -----------------------------------------------------------------------------
// Implementation of Assembler


void Assembler::emitl(uint32_t x) {
  Memory::uint32_at(pc_) = x;
  pc_ += sizeof(uint32_t);
}


void Assembler::emitp(void* x, RelocInfo::Mode rmode) {
  uintptr_t value = reinterpret_cast<uintptr_t>(x);
  Memory::uintptr_at(pc_) = value;
  if (!RelocInfo::IsNone(rmode)) {
    RecordRelocInfo(rmode, value);
  }
  pc_ += sizeof(uintptr_t);
}


void Assembler::emitq(uint64_t x) {
  Memory::uint64_at(pc_) = x;
  pc_ += sizeof(uint64_t);
}


void Assembler::emitw(uint16_t x) {
  Memory::uint16_at(pc_) = x;
  pc_ += sizeof(uint16_t);
}

void Assembler::emit_code_target(Handle<Code> target, RelocInfo::Mode rmode) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  RecordRelocInfo(rmode);
  int current = static_cast<int>(code_targets_.size());
  if (current > 0 && !target.is_null() &&
      code_targets_.back().address() == target.address()) {
    // Optimization if we keep jumping to the same code target.
    emitl(current - 1);
  } else {
    code_targets_.push_back(target);
    emitl(current);
  }
}


void Assembler::emit_runtime_entry(Address entry, RelocInfo::Mode rmode) {
  DCHECK(RelocInfo::IsRuntimeEntry(rmode));
  RecordRelocInfo(rmode);
  emitl(static_cast<uint32_t>(entry - isolate_data().code_range_start_));
}

void Assembler::emit(Immediate x) {
  if (!RelocInfo::IsNone(x.rmode_)) {
    RecordRelocInfo(x.rmode_);
  }
  emitl(x.value_);
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


void Assembler::emit_rex_32(Register rm_reg) {
  emit(0x40 | rm_reg.high_bit());
}

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

void Assembler::emit_optional_rex_32(XMMRegister rm_reg) {
  if (rm_reg.high_bit()) emit(0x41);
}

void Assembler::emit_optional_rex_32(Operand op) {
  if (op.data().rex != 0) emit(0x40 | op.data().rex);
}


// byte 1 of 3-byte VEX
void Assembler::emit_vex3_byte1(XMMRegister reg, XMMRegister rm,
                                LeadingOpcode m) {
  byte rxb = ~((reg.high_bit() << 2) | rm.high_bit()) << 5;
  emit(rxb | m);
}


// byte 1 of 3-byte VEX
void Assembler::emit_vex3_byte1(XMMRegister reg, Operand rm, LeadingOpcode m) {
  byte rxb = ~((reg.high_bit() << 2) | rm.data().rex) << 5;
  emit(rxb | m);
}


// byte 1 of 2-byte VEX
void Assembler::emit_vex2_byte1(XMMRegister reg, XMMRegister v, VectorLength l,
                                SIMDPrefix pp) {
  byte rv = ~((reg.high_bit() << 4) | v.code()) << 3;
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
  return Memory::int32_at(pc) + pc + 4;
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  Memory::int32_at(pc) = static_cast<int32_t>(target - pc - 4);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(pc, sizeof(int32_t));
  }
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  Memory::Address_at(pc) = target;
}


Address Assembler::target_address_from_return_address(Address pc) {
  return pc - kCallTargetAddressOffset;
}

void Assembler::deserialization_set_special_target_at(
    Address instruction_payload, Code* code, Address target) {
  set_target_address_at(instruction_payload,
                        code ? code->constant_pool() : nullptr, target);
}

Handle<Code> Assembler::code_target_object_handle_at(Address pc) {
  return code_targets_[Memory::int32_at(pc)];
}

Address Assembler::runtime_entry_at(Address pc) {
  return Memory::int32_at(pc) + isolate_data().code_range_start_;
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(intptr_t delta) {
  if (IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)) {
    Memory::int32_at(pc_) -= static_cast<int32_t>(delta);
  } else if (IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    Memory::Address_at(pc_) += delta;
  }
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  return Assembler::target_address_at(pc_, constant_pool_);
}

Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_) ||
         IsEmbeddedObject(rmode_) || IsExternalReference(rmode_) ||
         IsOffHeapTarget(rmode_));
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::constant_pool_entry_address() {
  UNREACHABLE();
}


int RelocInfo::target_address_size() {
  if (IsCodedSpecially()) {
    return Assembler::kSpecialTargetSize;
  } else {
    return kPointerSize;
  }
}

HeapObject* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return HeapObject::cast(Memory::Object_at(pc_));
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  if (rmode_ == EMBEDDED_OBJECT) {
    return Handle<HeapObject>::cast(Memory::Object_Handle_at(pc_));
  } else {
    return origin->code_target_object_handle_at(pc_);
  }
}

void RelocInfo::set_wasm_code_table_entry(Address target,
                                          ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::WASM_CODE_TABLE_ENTRY);
  Memory::Address_at(pc_) = target;
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(pc_, sizeof(Address));
  }
}

Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  return Memory::Address_at(pc_);
}

void RelocInfo::set_target_external_reference(
    Address target, ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  Memory::Address_at(pc_) = target;
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(pc_, sizeof(Address));
  }
}

Address RelocInfo::target_internal_reference() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return Memory::Address_at(pc_);
}


Address RelocInfo::target_internal_reference_address() {
  DCHECK(rmode_ == INTERNAL_REFERENCE);
  return reinterpret_cast<Address>(pc_);
}

void RelocInfo::set_target_object(HeapObject* target,
                                  WriteBarrierMode write_barrier_mode,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  Memory::Object_at(pc_) = target;
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(pc_, sizeof(Address));
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != nullptr) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  target);
    host()->GetHeap()->RecordWriteIntoCode(host(), this, target);
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
  return Memory::Address_at(pc_);
}

void RelocInfo::WipeOut() {
  if (IsEmbeddedObject(rmode_) || IsExternalReference(rmode_) ||
      IsInternalReference(rmode_)) {
    Memory::Address_at(pc_) = nullptr;
  } else if (IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)) {
    // Effectively write zero into the relocation.
    Assembler::set_target_address_at(pc_, constant_pool_,
                                     pc_ + sizeof(int32_t));
  } else {
    UNREACHABLE();
  }
}

template <typename ObjectVisitor>
void RelocInfo::Visit(ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(host(), this);
    Assembler::FlushICache(pc_, sizeof(Address));
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(host(), this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(host(), this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE) {
    visitor->VisitInternalReference(host(), this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(host(), this);
  } else if (RelocInfo::IsOffHeapTarget(mode)) {
    visitor->VisitOffHeapTarget(host(), this);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_X64_ASSEMBLER_X64_INL_H_
