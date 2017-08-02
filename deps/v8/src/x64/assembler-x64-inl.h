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

bool CpuFeatures::SupportsCrankshaft() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(SSE4_1); }

// -----------------------------------------------------------------------------
// Implementation of Assembler


static const byte kCallOpcode = 0xE8;


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


void Assembler::emit_code_target(Handle<Code> target,
                                 RelocInfo::Mode rmode,
                                 TypeFeedbackId ast_id) {
  DCHECK(RelocInfo::IsCodeTarget(rmode) ||
      rmode == RelocInfo::CODE_AGE_SEQUENCE);
  if (rmode == RelocInfo::CODE_TARGET && !ast_id.IsNone()) {
    RecordRelocInfo(RelocInfo::CODE_TARGET_WITH_ID, ast_id.ToInt());
  } else {
    RecordRelocInfo(rmode);
  }
  int current = code_targets_.length();
  if (current > 0 && code_targets_.last().address() == target.address()) {
    // Optimization if we keep jumping to the same code target.
    emitl(current - 1);
  } else {
    code_targets_.Add(target);
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


void Assembler::emit_rex_64(Register reg, const Operand& op) {
  emit(0x48 | reg.high_bit() << 2 | op.rex_);
}


void Assembler::emit_rex_64(XMMRegister reg, const Operand& op) {
  emit(0x48 | (reg.code() & 0x8) >> 1 | op.rex_);
}


void Assembler::emit_rex_64(Register rm_reg) {
  DCHECK_EQ(rm_reg.code() & 0xf, rm_reg.code());
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

void Assembler::emit_optional_rex_32(XMMRegister rm_reg) {
  if (rm_reg.high_bit()) emit(0x41);
}

void Assembler::emit_optional_rex_32(const Operand& op) {
  if (op.rex_ != 0) emit(0x40 | op.rex_);
}


// byte 1 of 3-byte VEX
void Assembler::emit_vex3_byte1(XMMRegister reg, XMMRegister rm,
                                LeadingOpcode m) {
  byte rxb = ~((reg.high_bit() << 2) | rm.high_bit()) << 5;
  emit(rxb | m);
}


// byte 1 of 3-byte VEX
void Assembler::emit_vex3_byte1(XMMRegister reg, const Operand& rm,
                                LeadingOpcode m) {
  byte rxb = ~((reg.high_bit() << 2) | rm.rex_) << 5;
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
  XMMRegister ireg = {reg.code()};
  XMMRegister ivreg = {vreg.code()};
  XMMRegister irm = {rm.code()};
  emit_vex_prefix(ireg, ivreg, irm, l, pp, mm, w);
}


void Assembler::emit_vex_prefix(XMMRegister reg, XMMRegister vreg,
                                const Operand& rm, VectorLength l,
                                SIMDPrefix pp, LeadingOpcode mm, VexW w) {
  if (rm.rex_ || mm != k0F || w != kW0) {
    emit_vex3_byte0();
    emit_vex3_byte1(reg, rm, mm);
    emit_vex3_byte2(w, vreg, l, pp);
  } else {
    emit_vex2_byte0();
    emit_vex2_byte1(reg, vreg, l, pp);
  }
}


void Assembler::emit_vex_prefix(Register reg, Register vreg, const Operand& rm,
                                VectorLength l, SIMDPrefix pp, LeadingOpcode mm,
                                VexW w) {
  XMMRegister ireg = {reg.code()};
  XMMRegister ivreg = {vreg.code()};
  emit_vex_prefix(ireg, ivreg, rm, l, pp, mm, w);
}


Address Assembler::target_address_at(Address pc, Address constant_pool) {
  return Memory::int32_at(pc) + pc + 4;
}


void Assembler::set_target_address_at(Isolate* isolate, Address pc,
                                      Address constant_pool, Address target,
                                      ICacheFlushMode icache_flush_mode) {
  DCHECK_IMPLIES(isolate == nullptr, icache_flush_mode == SKIP_ICACHE_FLUSH);
  Memory::int32_at(pc) = static_cast<int32_t>(target - pc - 4);
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(isolate, pc, sizeof(int32_t));
  }
}

Address Assembler::target_address_at(Address pc, Code* code) {
  Address constant_pool = code ? code->constant_pool() : NULL;
  return target_address_at(pc, constant_pool);
}

void Assembler::set_target_address_at(Isolate* isolate, Address pc, Code* code,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  Address constant_pool = code ? code->constant_pool() : NULL;
  set_target_address_at(isolate, pc, constant_pool, target, icache_flush_mode);
}

void Assembler::deserialization_set_target_internal_reference_at(
    Isolate* isolate, Address pc, Address target, RelocInfo::Mode mode) {
  Memory::Address_at(pc) = target;
}


Address Assembler::target_address_from_return_address(Address pc) {
  return pc - kCallTargetAddressOffset;
}

void Assembler::deserialization_set_special_target_at(
    Isolate* isolate, Address instruction_payload, Code* code, Address target) {
  set_target_address_at(isolate, instruction_payload, code, target);
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
  } else if (IsCodeAgeSequence(rmode_)) {
    if (*pc_ == kCallOpcode) {
      int32_t* p = reinterpret_cast<int32_t*>(pc_ + 1);
      *p -= static_cast<int32_t>(delta);  // Relocate entry.
    }
  } else if (IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    Memory::Address_at(pc_) += delta;
  }
}


Address RelocInfo::target_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_));
  return Assembler::target_address_at(pc_, host_);
}

Address RelocInfo::target_address_address() {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)
                              || rmode_ == EMBEDDED_OBJECT
                              || rmode_ == EXTERNAL_REFERENCE);
  return reinterpret_cast<Address>(pc_);
}


Address RelocInfo::constant_pool_entry_address() {
  UNREACHABLE();
  return NULL;
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


Address RelocInfo::target_external_reference() {
  DCHECK(rmode_ == RelocInfo::EXTERNAL_REFERENCE);
  return Memory::Address_at(pc_);
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
    Assembler::FlushICache(target->GetIsolate(), pc_, sizeof(Address));
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != NULL) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  target);
    host()->GetHeap()->RecordWriteIntoCode(host(), this, target);
  }
}


Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return origin->runtime_entry_at(pc_);
}

void RelocInfo::set_target_runtime_entry(Isolate* isolate, Address target,
                                         WriteBarrierMode write_barrier_mode,
                                         ICacheFlushMode icache_flush_mode) {
  DCHECK(IsRuntimeEntry(rmode_));
  if (target_address() != target) {
    set_target_address(isolate, target, write_barrier_mode, icache_flush_mode);
  }
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
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(cell->GetIsolate(), pc_, sizeof(Address));
  }
  if (write_barrier_mode == UPDATE_WRITE_BARRIER &&
      host() != NULL) {
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  cell);
  }
}

void RelocInfo::WipeOut(Isolate* isolate) {
  if (IsEmbeddedObject(rmode_) || IsExternalReference(rmode_) ||
      IsInternalReference(rmode_)) {
    Memory::Address_at(pc_) = NULL;
  } else if (IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)) {
    // Effectively write zero into the relocation.
    Assembler::set_target_address_at(isolate, pc_, host_,
                                     pc_ + sizeof(int32_t));
  } else {
    UNREACHABLE();
  }
}

Handle<Code> RelocInfo::code_age_stub_handle(Assembler* origin) {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  DCHECK(*pc_ == kCallOpcode);
  return origin->code_target_object_handle_at(pc_ + 1);
}


Code* RelocInfo::code_age_stub() {
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  DCHECK(*pc_ == kCallOpcode);
  return Code::GetCodeFromTargetAddress(
      Assembler::target_address_at(pc_ + 1, host_));
}


void RelocInfo::set_code_age_stub(Code* stub,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK(*pc_ == kCallOpcode);
  DCHECK(rmode_ == RelocInfo::CODE_AGE_SEQUENCE);
  Assembler::set_target_address_at(stub->GetIsolate(), pc_ + 1, host_,
                                   stub->instruction_start(),
                                   icache_flush_mode);
}


Address RelocInfo::debug_call_address() {
  DCHECK(IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence());
  return Memory::Address_at(pc_ + Assembler::kPatchDebugBreakSlotAddressOffset);
}

void RelocInfo::set_debug_call_address(Isolate* isolate, Address target) {
  DCHECK(IsDebugBreakSlot(rmode()) && IsPatchedDebugBreakSlotSequence());
  Memory::Address_at(pc_ + Assembler::kPatchDebugBreakSlotAddressOffset) =
      target;
  Assembler::FlushICache(isolate,
                         pc_ + Assembler::kPatchDebugBreakSlotAddressOffset,
                         sizeof(Address));
  if (host() != NULL) {
    Code* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  target_code);
  }
}

template <typename ObjectVisitor>
void RelocInfo::Visit(Isolate* isolate, ObjectVisitor* visitor) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    visitor->VisitEmbeddedPointer(host(), this);
    Assembler::FlushICache(isolate, pc_, sizeof(Address));
  } else if (RelocInfo::IsCodeTarget(mode)) {
    visitor->VisitCodeTarget(host(), this);
  } else if (mode == RelocInfo::CELL) {
    visitor->VisitCellPointer(host(), this);
  } else if (mode == RelocInfo::EXTERNAL_REFERENCE) {
    visitor->VisitExternalReference(host(), this);
  } else if (mode == RelocInfo::INTERNAL_REFERENCE) {
    visitor->VisitInternalReference(host(), this);
  } else if (RelocInfo::IsCodeAgeSequence(mode)) {
    visitor->VisitCodeAgeSequence(host(), this);
  } else if (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence()) {
    visitor->VisitDebugTarget(host(), this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(host(), this);
  }
}


template<typename StaticVisitor>
void RelocInfo::Visit(Heap* heap) {
  RelocInfo::Mode mode = rmode();
  if (mode == RelocInfo::EMBEDDED_OBJECT) {
    StaticVisitor::VisitEmbeddedPointer(heap, this);
    Assembler::FlushICache(heap->isolate(), pc_, sizeof(Address));
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
  } else if (RelocInfo::IsDebugBreakSlot(mode) &&
             IsPatchedDebugBreakSlotSequence()) {
    StaticVisitor::VisitDebugTarget(heap, this);
  } else if (RelocInfo::IsRuntimeEntry(mode)) {
    StaticVisitor::VisitRuntimeEntry(this);
  }
}


// -----------------------------------------------------------------------------
// Implementation of Operand

void Operand::set_modrm(int mod, Register rm_reg) {
  DCHECK(is_uint2(mod));
  buf_[0] = mod << 6 | rm_reg.low_bits();
  // Set REX.B to the high bit of rm.code().
  rex_ |= rm_reg.high_bit();
}


void Operand::set_sib(ScaleFactor scale, Register index, Register base) {
  DCHECK(len_ == 1);
  DCHECK(is_uint2(scale));
  // Use SIB with no index register only for base rsp or r12. Otherwise we
  // would skip the SIB byte entirely.
  DCHECK(!index.is(rsp) || base.is(rsp) || base.is(r12));
  buf_[1] = (scale << 6) | (index.low_bits() << 3) | base.low_bits();
  rex_ |= index.high_bit() << 1 | base.high_bit();
  len_ = 2;
}

void Operand::set_disp8(int disp) {
  DCHECK(is_int8(disp));
  DCHECK(len_ == 1 || len_ == 2);
  int8_t* p = reinterpret_cast<int8_t*>(&buf_[len_]);
  *p = disp;
  len_ += sizeof(int8_t);
}

void Operand::set_disp32(int disp) {
  DCHECK(len_ == 1 || len_ == 2);
  int32_t* p = reinterpret_cast<int32_t*>(&buf_[len_]);
  *p = disp;
  len_ += sizeof(int32_t);
}

void Operand::set_disp64(int64_t disp) {
  DCHECK_EQ(1, len_);
  int64_t* p = reinterpret_cast<int64_t*>(&buf_[len_]);
  *p = disp;
  len_ += sizeof(disp);
}
}  // namespace internal
}  // namespace v8

#endif  // V8_X64_ASSEMBLER_X64_INL_H_
