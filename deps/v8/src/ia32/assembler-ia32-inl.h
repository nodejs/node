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

// A light-weight IA32 Assembler.

#ifndef V8_IA32_ASSEMBLER_IA32_INL_H_
#define V8_IA32_ASSEMBLER_IA32_INL_H_

#include "src/ia32/assembler-ia32.h"

#include "src/assembler.h"
#include "src/debug/debug.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

bool CpuFeatures::SupportsOptimizer() { return true; }

bool CpuFeatures::SupportsWasmSimd128() { return IsSupported(SSE4_1); }


// The modes possibly affected by apply must be in kApplyMask.
void RelocInfo::apply(intptr_t delta) {
  if (IsRuntimeEntry(rmode_) || IsCodeTarget(rmode_) ||
      rmode_ == RelocInfo::JS_TO_WASM_CALL) {
    int32_t* p = reinterpret_cast<int32_t*>(pc_);
    *p -= delta;  // Relocate entry.
  } else if (IsInternalReference(rmode_)) {
    // absolute code pointer inside code object moves with the code object.
    int32_t* p = reinterpret_cast<int32_t*>(pc_);
    *p += delta;  // Relocate entry.
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
  return Assembler::kSpecialTargetSize;
}

HeapObject* RelocInfo::target_object() {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return HeapObject::cast(Memory::Object_at(pc_));
}

Handle<HeapObject> RelocInfo::target_object_handle(Assembler* origin) {
  DCHECK(IsCodeTarget(rmode_) || rmode_ == EMBEDDED_OBJECT);
  return Handle<HeapObject>::cast(Memory::Object_Handle_at(pc_));
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
    host()->GetHeap()->RecordWriteIntoCode(host(), this, target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  target);
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

void RelocInfo::set_wasm_code_table_entry(Address target,
                                          ICacheFlushMode icache_flush_mode) {
  DCHECK(rmode_ == RelocInfo::WASM_CODE_TABLE_ENTRY);
  Memory::Address_at(pc_) = target;
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(pc_, sizeof(Address));
  }
}

Address RelocInfo::target_runtime_entry(Assembler* origin) {
  DCHECK(IsRuntimeEntry(rmode_));
  return reinterpret_cast<Address>(*reinterpret_cast<int32_t*>(pc_));
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
  } else if (IsRuntimeEntry(mode)) {
    visitor->VisitRuntimeEntry(host(), this);
  } else if (RelocInfo::IsOffHeapTarget(mode)) {
    visitor->VisitOffHeapTarget(host(), this);
  }
}

void Assembler::emit(uint32_t x) {
  *reinterpret_cast<uint32_t*>(pc_) = x;
  pc_ += sizeof(uint32_t);
}


void Assembler::emit_q(uint64_t x) {
  *reinterpret_cast<uint64_t*>(pc_) = x;
  pc_ += sizeof(uint64_t);
}

void Assembler::emit(Handle<HeapObject> handle) {
  emit(reinterpret_cast<intptr_t>(handle.address()),
       RelocInfo::EMBEDDED_OBJECT);
}

void Assembler::emit(uint32_t x, RelocInfo::Mode rmode) {
  if (!RelocInfo::IsNone(rmode)) {
    RecordRelocInfo(rmode);
  }
  emit(x);
}

void Assembler::emit(Handle<Code> code, RelocInfo::Mode rmode) {
  emit(reinterpret_cast<intptr_t>(code.address()), rmode);
}


void Assembler::emit(const Immediate& x) {
  if (x.rmode_ == RelocInfo::INTERNAL_REFERENCE) {
    Label* label = reinterpret_cast<Label*>(x.immediate());
    emit_code_relative_offset(label);
    return;
  }
  if (!RelocInfo::IsNone(x.rmode_)) RecordRelocInfo(x.rmode_);
  if (x.is_heap_object_request()) {
    RequestHeapObject(x.heap_object_request());
    emit(0);
  } else {
    emit(x.immediate());
  }
}


void Assembler::emit_code_relative_offset(Label* label) {
  if (label->is_bound()) {
    int32_t pos;
    pos = label->pos() + Code::kHeaderSize - kHeapObjectTag;
    emit(pos);
  } else {
    emit_disp(label, Displacement::CODE_RELATIVE);
  }
}

void Assembler::emit_b(Immediate x) {
  DCHECK(x.is_int8() || x.is_uint8());
  uint8_t value = static_cast<uint8_t>(x.immediate());
  *pc_++ = value;
}

void Assembler::emit_w(const Immediate& x) {
  DCHECK(RelocInfo::IsNone(x.rmode_));
  uint16_t value = static_cast<uint16_t>(x.immediate());
  reinterpret_cast<uint16_t*>(pc_)[0] = value;
  pc_ += sizeof(uint16_t);
}


Address Assembler::target_address_at(Address pc, Address constant_pool) {
  return pc + sizeof(int32_t) + *reinterpret_cast<int32_t*>(pc);
}

void Assembler::set_target_address_at(Address pc, Address constant_pool,
                                      Address target,
                                      ICacheFlushMode icache_flush_mode) {
  int32_t* p = reinterpret_cast<int32_t*>(pc);
  *p = target - (pc + sizeof(int32_t));
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(p, sizeof(int32_t));
  }
}

Address Assembler::target_address_from_return_address(Address pc) {
  return pc - kCallTargetAddressOffset;
}

void Assembler::deserialization_set_special_target_at(
    Address instruction_payload, Code* code, Address target) {
  set_target_address_at(instruction_payload,
                        code ? code->constant_pool() : nullptr, target);
}

Displacement Assembler::disp_at(Label* L) {
  return Displacement(long_at(L->pos()));
}


void Assembler::disp_at_put(Label* L, Displacement disp) {
  long_at_put(L->pos(), disp.data());
}


void Assembler::emit_disp(Label* L, Displacement::Type type) {
  Displacement disp(L, type);
  L->link_to(pc_offset());
  emit(static_cast<int>(disp.data()));
}


void Assembler::emit_near_disp(Label* L) {
  byte disp = 0x00;
  if (L->is_near_linked()) {
    int offset = L->near_link_pos() - pc_offset();
    DCHECK(is_int8(offset));
    disp = static_cast<byte>(offset & 0xFF);
  }
  L->link_to(pc_offset(), Label::kNear);
  *pc_++ = disp;
}

void Assembler::deserialization_set_target_internal_reference_at(
    Address pc, Address target, RelocInfo::Mode mode) {
  Memory::Address_at(pc) = target;
}


void Operand::set_sib(ScaleFactor scale, Register index, Register base) {
  DCHECK_EQ(len_, 1);
  DCHECK_EQ(scale & -4, 0);
  // Use SIB with no index register only for base esp.
  DCHECK(index != esp || base == esp);
  buf_[1] = scale << 6 | index.code() << 3 | base.code();
  len_ = 2;
}


void Operand::set_disp8(int8_t disp) {
  DCHECK(len_ == 1 || len_ == 2);
  *reinterpret_cast<int8_t*>(&buf_[len_++]) = disp;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_ASSEMBLER_IA32_INL_H_
