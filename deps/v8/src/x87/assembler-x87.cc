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

#include "src/x87/assembler-x87.h"

#if V8_TARGET_ARCH_X87

#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/disassembler.h"
#include "src/macro-assembler.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Implementation of CpuFeatures

void CpuFeatures::ProbeImpl(bool cross_compile) {
  base::CPU cpu;

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;
}


void CpuFeatures::PrintTarget() { }
void CpuFeatures::PrintFeatures() { }


// -----------------------------------------------------------------------------
// Implementation of Displacement

void Displacement::init(Label* L, Type type) {
  DCHECK(!L->is_bound());
  int next = 0;
  if (L->is_linked()) {
    next = L->pos();
    DCHECK(next > 0);  // Displacements must be at positions > 0
  }
  // Ensure that we _never_ overflow the next field.
  DCHECK(NextField::is_valid(Assembler::kMaximalBufferSize));
  data_ = NextField::encode(next) | TypeField::encode(type);
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo


const int RelocInfo::kApplyMask =
    RelocInfo::kCodeTargetMask | 1 << RelocInfo::RUNTIME_ENTRY |
    1 << RelocInfo::INTERNAL_REFERENCE | 1 << RelocInfo::CODE_AGE_SEQUENCE |
    RelocInfo::kDebugBreakSlotMask;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on IA32 means that it is a relative address, as used by
  // branch instructions.  These are also the ones that need changing when a
  // code object moves.
  return (1 << rmode_) & kApplyMask;
}


bool RelocInfo::IsInConstantPool() {
  return false;
}

Address RelocInfo::wasm_memory_reference() {
  DCHECK(IsWasmMemoryReference(rmode_));
  return Memory::Address_at(pc_);
}

Address RelocInfo::wasm_global_reference() {
  DCHECK(IsWasmGlobalReference(rmode_));
  return Memory::Address_at(pc_);
}

uint32_t RelocInfo::wasm_memory_size_reference() {
  DCHECK(IsWasmMemorySizeReference(rmode_));
  return Memory::uint32_at(pc_);
}

void RelocInfo::unchecked_update_wasm_memory_reference(
    Address address, ICacheFlushMode flush_mode) {
  Memory::Address_at(pc_) = address;
}

void RelocInfo::unchecked_update_wasm_memory_size(uint32_t size,
                                                  ICacheFlushMode flush_mode) {
  Memory::uint32_at(pc_) = size;
}

// -----------------------------------------------------------------------------
// Implementation of Operand

Operand::Operand(Register base, int32_t disp, RelocInfo::Mode rmode) {
  // [base + disp/r]
  if (disp == 0 && RelocInfo::IsNone(rmode) && !base.is(ebp)) {
    // [base]
    set_modrm(0, base);
    if (base.is(esp)) set_sib(times_1, esp, base);
  } else if (is_int8(disp) && RelocInfo::IsNone(rmode)) {
    // [base + disp8]
    set_modrm(1, base);
    if (base.is(esp)) set_sib(times_1, esp, base);
    set_disp8(disp);
  } else {
    // [base + disp/r]
    set_modrm(2, base);
    if (base.is(esp)) set_sib(times_1, esp, base);
    set_dispr(disp, rmode);
  }
}


Operand::Operand(Register base,
                 Register index,
                 ScaleFactor scale,
                 int32_t disp,
                 RelocInfo::Mode rmode) {
  DCHECK(!index.is(esp));  // illegal addressing mode
  // [base + index*scale + disp/r]
  if (disp == 0 && RelocInfo::IsNone(rmode) && !base.is(ebp)) {
    // [base + index*scale]
    set_modrm(0, esp);
    set_sib(scale, index, base);
  } else if (is_int8(disp) && RelocInfo::IsNone(rmode)) {
    // [base + index*scale + disp8]
    set_modrm(1, esp);
    set_sib(scale, index, base);
    set_disp8(disp);
  } else {
    // [base + index*scale + disp/r]
    set_modrm(2, esp);
    set_sib(scale, index, base);
    set_dispr(disp, rmode);
  }
}


Operand::Operand(Register index,
                 ScaleFactor scale,
                 int32_t disp,
                 RelocInfo::Mode rmode) {
  DCHECK(!index.is(esp));  // illegal addressing mode
  // [index*scale + disp/r]
  set_modrm(0, esp);
  set_sib(scale, index, ebp);
  set_dispr(disp, rmode);
}


bool Operand::is_reg(Register reg) const {
  return ((buf_[0] & 0xF8) == 0xC0)  // addressing mode is register only.
      && ((buf_[0] & 0x07) == reg.code());  // register codes match.
}


bool Operand::is_reg_only() const {
  return (buf_[0] & 0xF8) == 0xC0;  // Addressing mode is register only.
}


Register Operand::reg() const {
  DCHECK(is_reg_only());
  return Register::from_code(buf_[0] & 0x07);
}


// -----------------------------------------------------------------------------
// Implementation of Assembler.

// Emit a single byte. Must always be inlined.
#define EMIT(x)                                 \
  *pc_++ = (x)

Assembler::Assembler(Isolate* isolate, void* buffer, int buffer_size)
    : AssemblerBase(isolate, buffer, buffer_size) {
// Clear the buffer in debug mode unless it was provided by the
// caller in which case we can't be sure it's okay to overwrite
// existing code in it; see CodePatcher::CodePatcher(...).
#ifdef DEBUG
  if (own_buffer_) {
    memset(buffer_, 0xCC, buffer_size_);  // int3
  }
#endif

  reloc_info_writer.Reposition(buffer_ + buffer_size_, pc_);
}


void Assembler::GetCode(CodeDesc* desc) {
  // Finalize code (at this point overflow() may be true, but the gap ensures
  // that we are still not overlapping instructions and relocation info).
  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.
  // Set up code descriptor.
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc->origin = this;
  desc->constant_pool_size = 0;
  desc->unwinding_info_size = 0;
  desc->unwinding_info = nullptr;
}


void Assembler::Align(int m) {
  DCHECK(base::bits::IsPowerOfTwo32(m));
  int mask = m - 1;
  int addr = pc_offset();
  Nop((m - (addr & mask)) & mask);
}


bool Assembler::IsNop(Address addr) {
  Address a = addr;
  while (*a == 0x66) a++;
  if (*a == 0x90) return true;
  if (a[0] == 0xf && a[1] == 0x1f) return true;
  return false;
}


void Assembler::Nop(int bytes) {
  EnsureSpace ensure_space(this);

  // Older CPUs that do not support SSE2 may not support multibyte NOP
  // instructions.
  for (; bytes > 0; bytes--) {
    EMIT(0x90);
  }
  return;
}


void Assembler::CodeTargetAlign() {
  Align(16);  // Preferred alignment of jump targets on ia32.
}


void Assembler::cpuid() {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xA2);
}


void Assembler::pushad() {
  EnsureSpace ensure_space(this);
  EMIT(0x60);
}


void Assembler::popad() {
  EnsureSpace ensure_space(this);
  EMIT(0x61);
}


void Assembler::pushfd() {
  EnsureSpace ensure_space(this);
  EMIT(0x9C);
}


void Assembler::popfd() {
  EnsureSpace ensure_space(this);
  EMIT(0x9D);
}


void Assembler::push(const Immediate& x) {
  EnsureSpace ensure_space(this);
  if (x.is_int8()) {
    EMIT(0x6a);
    EMIT(x.x_);
  } else {
    EMIT(0x68);
    emit(x);
  }
}


void Assembler::push_imm32(int32_t imm32) {
  EnsureSpace ensure_space(this);
  EMIT(0x68);
  emit(imm32);
}


void Assembler::push(Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x50 | src.code());
}


void Assembler::push(const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(esi, src);
}


void Assembler::pop(Register dst) {
  DCHECK(reloc_info_writer.last_pc() != NULL);
  EnsureSpace ensure_space(this);
  EMIT(0x58 | dst.code());
}


void Assembler::pop(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0x8F);
  emit_operand(eax, dst);
}


void Assembler::enter(const Immediate& size) {
  EnsureSpace ensure_space(this);
  EMIT(0xC8);
  emit_w(size);
  EMIT(0);
}


void Assembler::leave() {
  EnsureSpace ensure_space(this);
  EMIT(0xC9);
}


void Assembler::mov_b(Register dst, const Operand& src) {
  CHECK(dst.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x8A);
  emit_operand(dst, src);
}


void Assembler::mov_b(const Operand& dst, const Immediate& src) {
  EnsureSpace ensure_space(this);
  EMIT(0xC6);
  emit_operand(eax, dst);
  EMIT(static_cast<int8_t>(src.x_));
}


void Assembler::mov_b(const Operand& dst, int8_t imm8) {
  EnsureSpace ensure_space(this);
  EMIT(0xC6);
  emit_operand(eax, dst);
  EMIT(imm8);
}


void Assembler::mov_b(const Operand& dst, Register src) {
  CHECK(src.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x88);
  emit_operand(src, dst);
}


void Assembler::mov_w(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x8B);
  emit_operand(dst, src);
}


void Assembler::mov_w(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x89);
  emit_operand(src, dst);
}


void Assembler::mov_w(const Operand& dst, int16_t imm16) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0xC7);
  emit_operand(eax, dst);
  EMIT(static_cast<int8_t>(imm16 & 0xff));
  EMIT(static_cast<int8_t>(imm16 >> 8));
}


void Assembler::mov_w(const Operand& dst, const Immediate& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0xC7);
  emit_operand(eax, dst);
  EMIT(static_cast<int8_t>(src.x_ & 0xff));
  EMIT(static_cast<int8_t>(src.x_ >> 8));
}


void Assembler::mov(Register dst, int32_t imm32) {
  EnsureSpace ensure_space(this);
  EMIT(0xB8 | dst.code());
  emit(imm32);
}


void Assembler::mov(Register dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  EMIT(0xB8 | dst.code());
  emit(x);
}


void Assembler::mov(Register dst, Handle<Object> handle) {
  EnsureSpace ensure_space(this);
  EMIT(0xB8 | dst.code());
  emit(handle);
}


void Assembler::mov(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x8B);
  emit_operand(dst, src);
}


void Assembler::mov(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x89);
  EMIT(0xC0 | src.code() << 3 | dst.code());
}


void Assembler::mov(const Operand& dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  EMIT(0xC7);
  emit_operand(eax, dst);
  emit(x);
}


void Assembler::mov(const Operand& dst, Handle<Object> handle) {
  EnsureSpace ensure_space(this);
  EMIT(0xC7);
  emit_operand(eax, dst);
  emit(handle);
}


void Assembler::mov(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x89);
  emit_operand(src, dst);
}


void Assembler::movsx_b(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBE);
  emit_operand(dst, src);
}


void Assembler::movsx_w(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBF);
  emit_operand(dst, src);
}


void Assembler::movzx_b(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB6);
  emit_operand(dst, src);
}


void Assembler::movzx_w(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB7);
  emit_operand(dst, src);
}


void Assembler::cld() {
  EnsureSpace ensure_space(this);
  EMIT(0xFC);
}


void Assembler::rep_movs() {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0xA5);
}


void Assembler::rep_stos() {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0xAB);
}


void Assembler::stos() {
  EnsureSpace ensure_space(this);
  EMIT(0xAB);
}


void Assembler::xchg(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  if (src.is(eax) || dst.is(eax)) {  // Single-byte encoding.
    EMIT(0x90 | (src.is(eax) ? dst.code() : src.code()));
  } else {
    EMIT(0x87);
    EMIT(0xC0 | src.code() << 3 | dst.code());
  }
}


void Assembler::xchg(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x87);
  emit_operand(dst, src);
}

void Assembler::xchg_b(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x86);
  emit_operand(reg, op);
}

void Assembler::xchg_w(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x87);
  emit_operand(reg, op);
}

void Assembler::lock() {
  EnsureSpace ensure_space(this);
  EMIT(0xF0);
}

void Assembler::cmpxchg(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB1);
  emit_operand(src, dst);
}

void Assembler::cmpxchg_b(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB0);
  emit_operand(src, dst);
}

void Assembler::cmpxchg_w(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0xB1);
  emit_operand(src, dst);
}

void Assembler::adc(Register dst, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(2, Operand(dst), Immediate(imm32));
}


void Assembler::adc(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x13);
  emit_operand(dst, src);
}


void Assembler::add(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x03);
  emit_operand(dst, src);
}


void Assembler::add(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x01);
  emit_operand(src, dst);
}


void Assembler::add(const Operand& dst, const Immediate& x) {
  DCHECK(reloc_info_writer.last_pc() != NULL);
  EnsureSpace ensure_space(this);
  emit_arith(0, dst, x);
}


void Assembler::and_(Register dst, int32_t imm32) {
  and_(dst, Immediate(imm32));
}


void Assembler::and_(Register dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(4, Operand(dst), x);
}


void Assembler::and_(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x23);
  emit_operand(dst, src);
}


void Assembler::and_(const Operand& dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(4, dst, x);
}


void Assembler::and_(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x21);
  emit_operand(src, dst);
}

void Assembler::cmpb(const Operand& op, Immediate imm8) {
  DCHECK(imm8.is_int8() || imm8.is_uint8());
  EnsureSpace ensure_space(this);
  if (op.is_reg(eax)) {
    EMIT(0x3C);
  } else {
    EMIT(0x80);
    emit_operand(edi, op);  // edi == 7
  }
  emit_b(imm8);
}


void Assembler::cmpb(const Operand& op, Register reg) {
  CHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x38);
  emit_operand(reg, op);
}


void Assembler::cmpb(Register reg, const Operand& op) {
  CHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x3A);
  emit_operand(reg, op);
}


void Assembler::cmpw(const Operand& op, Immediate imm16) {
  DCHECK(imm16.is_int16());
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x81);
  emit_operand(edi, op);
  emit_w(imm16);
}

void Assembler::cmpw(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x3B);
  emit_operand(reg, op);
}

void Assembler::cmpw(const Operand& op, Register reg) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x39);
  emit_operand(reg, op);
}

void Assembler::cmp(Register reg, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(7, Operand(reg), Immediate(imm32));
}


void Assembler::cmp(Register reg, Handle<Object> handle) {
  EnsureSpace ensure_space(this);
  emit_arith(7, Operand(reg), Immediate(handle));
}


void Assembler::cmp(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x3B);
  emit_operand(reg, op);
}

void Assembler::cmp(const Operand& op, Register reg) {
  EnsureSpace ensure_space(this);
  EMIT(0x39);
  emit_operand(reg, op);
}

void Assembler::cmp(const Operand& op, const Immediate& imm) {
  EnsureSpace ensure_space(this);
  emit_arith(7, op, imm);
}


void Assembler::cmp(const Operand& op, Handle<Object> handle) {
  EnsureSpace ensure_space(this);
  emit_arith(7, op, Immediate(handle));
}


void Assembler::cmpb_al(const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x38);  // CMP r/m8, r8
  emit_operand(eax, op);  // eax has same code as register al.
}


void Assembler::cmpw_ax(const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x39);  // CMP r/m16, r16
  emit_operand(eax, op);  // eax has same code as register ax.
}


void Assembler::dec_b(Register dst) {
  CHECK(dst.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0xFE);
  EMIT(0xC8 | dst.code());
}


void Assembler::dec_b(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xFE);
  emit_operand(ecx, dst);
}


void Assembler::dec(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0x48 | dst.code());
}


void Assembler::dec(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(ecx, dst);
}


void Assembler::cdq() {
  EnsureSpace ensure_space(this);
  EMIT(0x99);
}


void Assembler::idiv(const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(edi, src);
}


void Assembler::div(const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(esi, src);
}


void Assembler::imul(Register reg) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  EMIT(0xE8 | reg.code());
}


void Assembler::imul(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAF);
  emit_operand(dst, src);
}


void Assembler::imul(Register dst, Register src, int32_t imm32) {
  imul(dst, Operand(src), imm32);
}


void Assembler::imul(Register dst, const Operand& src, int32_t imm32) {
  EnsureSpace ensure_space(this);
  if (is_int8(imm32)) {
    EMIT(0x6B);
    emit_operand(dst, src);
    EMIT(imm32);
  } else {
    EMIT(0x69);
    emit_operand(dst, src);
    emit(imm32);
  }
}


void Assembler::inc(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0x40 | dst.code());
}


void Assembler::inc(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(eax, dst);
}


void Assembler::lea(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x8D);
  emit_operand(dst, src);
}


void Assembler::mul(Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  EMIT(0xE0 | src.code());
}


void Assembler::neg(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  EMIT(0xD8 | dst.code());
}


void Assembler::neg(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(ebx, dst);
}


void Assembler::not_(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  EMIT(0xD0 | dst.code());
}


void Assembler::not_(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(edx, dst);
}


void Assembler::or_(Register dst, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(1, Operand(dst), Immediate(imm32));
}


void Assembler::or_(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0B);
  emit_operand(dst, src);
}


void Assembler::or_(const Operand& dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(1, dst, x);
}


void Assembler::or_(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x09);
  emit_operand(src, dst);
}


void Assembler::rcl(Register dst, uint8_t imm8) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint5(imm8));  // illegal shift count
  if (imm8 == 1) {
    EMIT(0xD1);
    EMIT(0xD0 | dst.code());
  } else {
    EMIT(0xC1);
    EMIT(0xD0 | dst.code());
    EMIT(imm8);
  }
}


void Assembler::rcr(Register dst, uint8_t imm8) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint5(imm8));  // illegal shift count
  if (imm8 == 1) {
    EMIT(0xD1);
    EMIT(0xD8 | dst.code());
  } else {
    EMIT(0xC1);
    EMIT(0xD8 | dst.code());
    EMIT(imm8);
  }
}


void Assembler::ror(const Operand& dst, uint8_t imm8) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint5(imm8));  // illegal shift count
  if (imm8 == 1) {
    EMIT(0xD1);
    emit_operand(ecx, dst);
  } else {
    EMIT(0xC1);
    emit_operand(ecx, dst);
    EMIT(imm8);
  }
}


void Assembler::ror_cl(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(ecx, dst);
}


void Assembler::sar(const Operand& dst, uint8_t imm8) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint5(imm8));  // illegal shift count
  if (imm8 == 1) {
    EMIT(0xD1);
    emit_operand(edi, dst);
  } else {
    EMIT(0xC1);
    emit_operand(edi, dst);
    EMIT(imm8);
  }
}


void Assembler::sar_cl(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(edi, dst);
}

void Assembler::sbb(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x1B);
  emit_operand(dst, src);
}

void Assembler::shld(Register dst, Register src, uint8_t shift) {
  DCHECK(is_uint5(shift));
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xA4);
  emit_operand(src, Operand(dst));
  EMIT(shift);
}

void Assembler::shld_cl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xA5);
  emit_operand(src, Operand(dst));
}


void Assembler::shl(const Operand& dst, uint8_t imm8) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint5(imm8));  // illegal shift count
  if (imm8 == 1) {
    EMIT(0xD1);
    emit_operand(esp, dst);
  } else {
    EMIT(0xC1);
    emit_operand(esp, dst);
    EMIT(imm8);
  }
}


void Assembler::shl_cl(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(esp, dst);
}

void Assembler::shr(const Operand& dst, uint8_t imm8) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint5(imm8));  // illegal shift count
  if (imm8 == 1) {
    EMIT(0xD1);
    emit_operand(ebp, dst);
  } else {
    EMIT(0xC1);
    emit_operand(ebp, dst);
    EMIT(imm8);
  }
}


void Assembler::shr_cl(const Operand& dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(ebp, dst);
}

void Assembler::shrd(Register dst, Register src, uint8_t shift) {
  DCHECK(is_uint5(shift));
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAC);
  emit_operand(dst, Operand(src));
  EMIT(shift);
}

void Assembler::shrd_cl(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAD);
  emit_operand(src, dst);
}

void Assembler::sub(const Operand& dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(5, dst, x);
}


void Assembler::sub(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x2B);
  emit_operand(dst, src);
}


void Assembler::sub(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x29);
  emit_operand(src, dst);
}


void Assembler::test(Register reg, const Immediate& imm) {
  if (imm.is_uint8()) {
    test_b(reg, imm);
    return;
  }

  EnsureSpace ensure_space(this);
  // This is not using emit_arith because test doesn't support
  // sign-extension of 8-bit operands.
  if (reg.is(eax)) {
    EMIT(0xA9);
  } else {
    EMIT(0xF7);
    EMIT(0xC0 | reg.code());
  }
  emit(imm);
}


void Assembler::test(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x85);
  emit_operand(reg, op);
}


void Assembler::test_b(Register reg, const Operand& op) {
  CHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x84);
  emit_operand(reg, op);
}


void Assembler::test(const Operand& op, const Immediate& imm) {
  if (op.is_reg_only()) {
    test(op.reg(), imm);
    return;
  }
  if (imm.is_uint8()) {
    return test_b(op, imm);
  }
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(eax, op);
  emit(imm);
}

void Assembler::test_b(Register reg, Immediate imm8) {
  DCHECK(imm8.is_uint8());
  EnsureSpace ensure_space(this);
  // Only use test against byte for registers that have a byte
  // variant: eax, ebx, ecx, and edx.
  if (reg.is(eax)) {
    EMIT(0xA8);
    emit_b(imm8);
  } else if (reg.is_byte_register()) {
    emit_arith_b(0xF6, 0xC0, reg, static_cast<uint8_t>(imm8.x_));
  } else {
    EMIT(0x66);
    EMIT(0xF7);
    EMIT(0xC0 | reg.code());
    emit_w(imm8);
  }
}

void Assembler::test_b(const Operand& op, Immediate imm8) {
  if (op.is_reg_only()) {
    test_b(op.reg(), imm8);
    return;
  }
  EnsureSpace ensure_space(this);
  EMIT(0xF6);
  emit_operand(eax, op);
  emit_b(imm8);
}

void Assembler::test_w(Register reg, Immediate imm16) {
  DCHECK(imm16.is_int16() || imm16.is_uint16());
  EnsureSpace ensure_space(this);
  if (reg.is(eax)) {
    EMIT(0xA9);
    emit_w(imm16);
  } else {
    EMIT(0x66);
    EMIT(0xF7);
    EMIT(0xc0 | reg.code());
    emit_w(imm16);
  }
}

void Assembler::test_w(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x85);
  emit_operand(reg, op);
}

void Assembler::test_w(const Operand& op, Immediate imm16) {
  DCHECK(imm16.is_int16() || imm16.is_uint16());
  if (op.is_reg_only()) {
    test_w(op.reg(), imm16);
    return;
  }
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0xF7);
  emit_operand(eax, op);
  emit_w(imm16);
}

void Assembler::xor_(Register dst, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(6, Operand(dst), Immediate(imm32));
}


void Assembler::xor_(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x33);
  emit_operand(dst, src);
}


void Assembler::xor_(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x31);
  emit_operand(src, dst);
}


void Assembler::xor_(const Operand& dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(6, dst, x);
}


void Assembler::bt(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xA3);
  emit_operand(src, dst);
}


void Assembler::bts(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAB);
  emit_operand(src, dst);
}


void Assembler::bsr(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBD);
  emit_operand(dst, src);
}


void Assembler::bsf(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBC);
  emit_operand(dst, src);
}


void Assembler::hlt() {
  EnsureSpace ensure_space(this);
  EMIT(0xF4);
}


void Assembler::int3() {
  EnsureSpace ensure_space(this);
  EMIT(0xCC);
}


void Assembler::nop() {
  EnsureSpace ensure_space(this);
  EMIT(0x90);
}


void Assembler::ret(int imm16) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint16(imm16));
  if (imm16 == 0) {
    EMIT(0xC3);
  } else {
    EMIT(0xC2);
    EMIT(imm16 & 0xFF);
    EMIT((imm16 >> 8) & 0xFF);
  }
}


void Assembler::ud2() {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x0B);
}


// Labels refer to positions in the (to be) generated code.
// There are bound, linked, and unused labels.
//
// Bound labels refer to known positions in the already
// generated code. pos() is the position the label refers to.
//
// Linked labels refer to unknown positions in the code
// to be generated; pos() is the position of the 32bit
// Displacement of the last instruction using the label.


void Assembler::print(Label* L) {
  if (L->is_unused()) {
    PrintF("unused label\n");
  } else if (L->is_bound()) {
    PrintF("bound label to %d\n", L->pos());
  } else if (L->is_linked()) {
    Label l = *L;
    PrintF("unbound label");
    while (l.is_linked()) {
      Displacement disp = disp_at(&l);
      PrintF("@ %d ", l.pos());
      disp.print();
      PrintF("\n");
      disp.next(&l);
    }
  } else {
    PrintF("label in inconsistent state (pos = %d)\n", L->pos_);
  }
}


void Assembler::bind_to(Label* L, int pos) {
  EnsureSpace ensure_space(this);
  DCHECK(0 <= pos && pos <= pc_offset());  // must have a valid binding position
  while (L->is_linked()) {
    Displacement disp = disp_at(L);
    int fixup_pos = L->pos();
    if (disp.type() == Displacement::CODE_ABSOLUTE) {
      long_at_put(fixup_pos, reinterpret_cast<int>(buffer_ + pos));
      internal_reference_positions_.push_back(fixup_pos);
    } else if (disp.type() == Displacement::CODE_RELATIVE) {
      // Relative to Code* heap object pointer.
      long_at_put(fixup_pos, pos + Code::kHeaderSize - kHeapObjectTag);
    } else {
      if (disp.type() == Displacement::UNCONDITIONAL_JUMP) {
        DCHECK(byte_at(fixup_pos - 1) == 0xE9);  // jmp expected
      }
      // Relative address, relative to point after address.
      int imm32 = pos - (fixup_pos + sizeof(int32_t));
      long_at_put(fixup_pos, imm32);
    }
    disp.next(L);
  }
  while (L->is_near_linked()) {
    int fixup_pos = L->near_link_pos();
    int offset_to_next =
        static_cast<int>(*reinterpret_cast<int8_t*>(addr_at(fixup_pos)));
    DCHECK(offset_to_next <= 0);
    // Relative address, relative to point after address.
    int disp = pos - fixup_pos - sizeof(int8_t);
    CHECK(0 <= disp && disp <= 127);
    set_byte_at(fixup_pos, disp);
    if (offset_to_next < 0) {
      L->link_to(fixup_pos + offset_to_next, Label::kNear);
    } else {
      L->UnuseNear();
    }
  }
  L->bind_to(pos);
}


void Assembler::bind(Label* L) {
  EnsureSpace ensure_space(this);
  DCHECK(!L->is_bound());  // label can only be bound once
  bind_to(L, pc_offset());
}


void Assembler::call(Label* L) {
  EnsureSpace ensure_space(this);
  if (L->is_bound()) {
    const int long_size = 5;
    int offs = L->pos() - pc_offset();
    DCHECK(offs <= 0);
    // 1110 1000 #32-bit disp.
    EMIT(0xE8);
    emit(offs - long_size);
  } else {
    // 1110 1000 #32-bit disp.
    EMIT(0xE8);
    emit_disp(L, Displacement::OTHER);
  }
}


void Assembler::call(byte* entry, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  EMIT(0xE8);
  if (RelocInfo::IsRuntimeEntry(rmode)) {
    emit(reinterpret_cast<uint32_t>(entry), rmode);
  } else {
    emit(entry - (pc_ + sizeof(int32_t)), rmode);
  }
}


int Assembler::CallSize(const Operand& adr) {
  // Call size is 1 (opcode) + adr.len_ (operand).
  return 1 + adr.len_;
}


void Assembler::call(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(edx, adr);
}


int Assembler::CallSize(Handle<Code> code, RelocInfo::Mode rmode) {
  return 1 /* EMIT */ + sizeof(uint32_t) /* emit */;
}


void Assembler::call(Handle<Code> code,
                     RelocInfo::Mode rmode,
                     TypeFeedbackId ast_id) {
  EnsureSpace ensure_space(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode)
      || rmode == RelocInfo::CODE_AGE_SEQUENCE);
  EMIT(0xE8);
  emit(code, rmode, ast_id);
}


void Assembler::jmp(Label* L, Label::Distance distance) {
  EnsureSpace ensure_space(this);
  if (L->is_bound()) {
    const int short_size = 2;
    const int long_size  = 5;
    int offs = L->pos() - pc_offset();
    DCHECK(offs <= 0);
    if (is_int8(offs - short_size)) {
      // 1110 1011 #8-bit disp.
      EMIT(0xEB);
      EMIT((offs - short_size) & 0xFF);
    } else {
      // 1110 1001 #32-bit disp.
      EMIT(0xE9);
      emit(offs - long_size);
    }
  } else if (distance == Label::kNear) {
    EMIT(0xEB);
    emit_near_disp(L);
  } else {
    // 1110 1001 #32-bit disp.
    EMIT(0xE9);
    emit_disp(L, Displacement::UNCONDITIONAL_JUMP);
  }
}


void Assembler::jmp(byte* entry, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  EMIT(0xE9);
  if (RelocInfo::IsRuntimeEntry(rmode)) {
    emit(reinterpret_cast<uint32_t>(entry), rmode);
  } else {
    emit(entry - (pc_ + sizeof(int32_t)), rmode);
  }
}


void Assembler::jmp(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(esp, adr);
}


void Assembler::jmp(Handle<Code> code, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  EMIT(0xE9);
  emit(code, rmode);
}


void Assembler::j(Condition cc, Label* L, Label::Distance distance) {
  EnsureSpace ensure_space(this);
  DCHECK(0 <= cc && static_cast<int>(cc) < 16);
  if (L->is_bound()) {
    const int short_size = 2;
    const int long_size  = 6;
    int offs = L->pos() - pc_offset();
    DCHECK(offs <= 0);
    if (is_int8(offs - short_size)) {
      // 0111 tttn #8-bit disp
      EMIT(0x70 | cc);
      EMIT((offs - short_size) & 0xFF);
    } else {
      // 0000 1111 1000 tttn #32-bit disp
      EMIT(0x0F);
      EMIT(0x80 | cc);
      emit(offs - long_size);
    }
  } else if (distance == Label::kNear) {
    EMIT(0x70 | cc);
    emit_near_disp(L);
  } else {
    // 0000 1111 1000 tttn #32-bit disp
    // Note: could eliminate cond. jumps to this jump if condition
    //       is the same however, seems to be rather unlikely case.
    EMIT(0x0F);
    EMIT(0x80 | cc);
    emit_disp(L, Displacement::OTHER);
  }
}


void Assembler::j(Condition cc, byte* entry, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK((0 <= cc) && (static_cast<int>(cc) < 16));
  // 0000 1111 1000 tttn #32-bit disp.
  EMIT(0x0F);
  EMIT(0x80 | cc);
  if (RelocInfo::IsRuntimeEntry(rmode)) {
    emit(reinterpret_cast<uint32_t>(entry), rmode);
  } else {
    emit(entry - (pc_ + sizeof(int32_t)), rmode);
  }
}


void Assembler::j(Condition cc, Handle<Code> code, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  // 0000 1111 1000 tttn #32-bit disp
  EMIT(0x0F);
  EMIT(0x80 | cc);
  emit(code, rmode);
}


// FPU instructions.

void Assembler::fld(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD9, 0xC0, i);
}


void Assembler::fstp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDD, 0xD8, i);
}


void Assembler::fld1() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xE8);
}


void Assembler::fldpi() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xEB);
}


void Assembler::fldz() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xEE);
}


void Assembler::fldln2() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xED);
}


void Assembler::fld_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(eax, adr);
}


void Assembler::fld_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(eax, adr);
}


void Assembler::fstp_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(ebx, adr);
}


void Assembler::fst_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(edx, adr);
}


void Assembler::fldcw(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(ebp, adr);
}


void Assembler::fnstcw(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(edi, adr);
}


void Assembler::fstp_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(ebx, adr);
}


void Assembler::fst_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(edx, adr);
}


void Assembler::fild_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(eax, adr);
}


void Assembler::fild_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDF);
  emit_operand(ebp, adr);
}


void Assembler::fistp_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(ebx, adr);
}


void Assembler::fisttp_s(const Operand& adr) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(ecx, adr);
}


void Assembler::fisttp_d(const Operand& adr) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(ecx, adr);
}


void Assembler::fist_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(edx, adr);
}


void Assembler::fistp_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDF);
  emit_operand(edi, adr);
}


void Assembler::fabs() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xE1);
}


void Assembler::fchs() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xE0);
}


void Assembler::fsqrt() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xFA);
}


void Assembler::fcos() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xFF);
}


void Assembler::fsin() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xFE);
}


void Assembler::fptan() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xF2);
}


void Assembler::fyl2x() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xF1);
}


void Assembler::f2xm1() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xF0);
}


void Assembler::fscale() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xFD);
}


void Assembler::fninit() {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  EMIT(0xE3);
}


void Assembler::fadd(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xC0, i);
}


void Assembler::fadd_i(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD8, 0xC0, i);
}


void Assembler::fadd_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDC);
  emit_operand(eax, adr);
}


void Assembler::fsub(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xE8, i);
}


void Assembler::fsub_i(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD8, 0xE0, i);
}


void Assembler::fsubr_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDC);
  emit_operand(ebp, adr);
}


void Assembler::fsub_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDC);
  emit_operand(esp, adr);
}


void Assembler::fisub_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDA);
  emit_operand(esp, adr);
}


void Assembler::fmul_i(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD8, 0xC8, i);
}


void Assembler::fmul(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xC8, i);
}


void Assembler::fmul_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDC);
  emit_operand(ecx, adr);
}


void Assembler::fdiv(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xF8, i);
}


void Assembler::fdiv_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDC);
  emit_operand(esi, adr);
}


void Assembler::fdivr_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDC);
  emit_operand(edi, adr);
}


void Assembler::fdiv_i(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD8, 0xF0, i);
}


void Assembler::faddp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDE, 0xC0, i);
}


void Assembler::fsubp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDE, 0xE8, i);
}


void Assembler::fsubrp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDE, 0xE0, i);
}


void Assembler::fmulp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDE, 0xC8, i);
}


void Assembler::fdivp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDE, 0xF8, i);
}


void Assembler::fprem() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xF8);
}


void Assembler::fprem1() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xF5);
}


void Assembler::fxch(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD9, 0xC8, i);
}


void Assembler::fincstp() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xF7);
}


void Assembler::ffree(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDD, 0xC0, i);
}


void Assembler::ftst() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xE4);
}


void Assembler::fxam() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xE5);
}


void Assembler::fucomp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDD, 0xE8, i);
}


void Assembler::fucompp() {
  EnsureSpace ensure_space(this);
  EMIT(0xDA);
  EMIT(0xE9);
}


void Assembler::fucomi(int i) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  EMIT(0xE8 + i);
}


void Assembler::fucomip() {
  EnsureSpace ensure_space(this);
  EMIT(0xDF);
  EMIT(0xE9);
}


void Assembler::fcompp() {
  EnsureSpace ensure_space(this);
  EMIT(0xDE);
  EMIT(0xD9);
}


void Assembler::fnstsw_ax() {
  EnsureSpace ensure_space(this);
  EMIT(0xDF);
  EMIT(0xE0);
}


void Assembler::fwait() {
  EnsureSpace ensure_space(this);
  EMIT(0x9B);
}


void Assembler::frndint() {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  EMIT(0xFC);
}


void Assembler::fnclex() {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  EMIT(0xE2);
}


void Assembler::fnsave(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(esi, adr);
}


void Assembler::frstor(const Operand& adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(esp, adr);
}


void Assembler::sahf() {
  EnsureSpace ensure_space(this);
  EMIT(0x9E);
}


void Assembler::setcc(Condition cc, Register reg) {
  DCHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x90 | cc);
  EMIT(0xC0 | reg.code());
}


void Assembler::GrowBuffer() {
  DCHECK(buffer_overflow());
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // the new buffer
  desc.buffer_size = 2 * buffer_size_;

  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if ((desc.buffer_size > kMaximalBufferSize) ||
      (desc.buffer_size > isolate()->heap()->MaxOldGenerationSize())) {
    V8::FatalProcessOutOfMemory("Assembler::GrowBuffer");
  }

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;
  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer_ + buffer_size_) - (reloc_info_writer.pos());

  // Clear the buffer in debug mode. Use 'int3' instructions to make
  // sure to get into problems if we ever run uninitialized code.
#ifdef DEBUG
  memset(desc.buffer, 0xCC, desc.buffer_size);
#endif

  // Copy the data.
  int pc_delta = desc.buffer - buffer_;
  int rc_delta = (desc.buffer + desc.buffer_size) - (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(rc_delta + reloc_info_writer.pos(), reloc_info_writer.pos(),
          desc.reloc_size);

  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate internal references.
  for (auto pos : internal_reference_positions_) {
    int32_t* p = reinterpret_cast<int32_t*>(buffer_ + pos);
    *p += pc_delta;
  }

  DCHECK(!buffer_overflow());
}


void Assembler::emit_arith_b(int op1, int op2, Register dst, int imm8) {
  DCHECK(is_uint8(op1) && is_uint8(op2));  // wrong opcode
  DCHECK(is_uint8(imm8));
  DCHECK((op1 & 0x01) == 0);  // should be 8bit operation
  EMIT(op1);
  EMIT(op2 | dst.code());
  EMIT(imm8);
}


void Assembler::emit_arith(int sel, Operand dst, const Immediate& x) {
  DCHECK((0 <= sel) && (sel <= 7));
  Register ireg = { sel };
  if (x.is_int8()) {
    EMIT(0x83);  // using a sign-extended 8-bit immediate.
    emit_operand(ireg, dst);
    EMIT(x.x_ & 0xFF);
  } else if (dst.is_reg(eax)) {
    EMIT((sel << 3) | 0x05);  // short form if the destination is eax.
    emit(x);
  } else {
    EMIT(0x81);  // using a literal 32-bit immediate.
    emit_operand(ireg, dst);
    emit(x);
  }
}


void Assembler::emit_operand(Register reg, const Operand& adr) {
  const unsigned length = adr.len_;
  DCHECK(length > 0);

  // Emit updated ModRM byte containing the given register.
  pc_[0] = (adr.buf_[0] & ~0x38) | (reg.code() << 3);

  // Emit the rest of the encoded operand.
  for (unsigned i = 1; i < length; i++) pc_[i] = adr.buf_[i];
  pc_ += length;

  // Emit relocation information if necessary.
  if (length >= sizeof(int32_t) && !RelocInfo::IsNone(adr.rmode_)) {
    pc_ -= sizeof(int32_t);  // pc_ must be *at* disp32
    RecordRelocInfo(adr.rmode_);
    if (adr.rmode_ == RelocInfo::INTERNAL_REFERENCE) {  // Fixup for labels
      emit_label(*reinterpret_cast<Label**>(pc_));
    } else {
      pc_ += sizeof(int32_t);
    }
  }
}


void Assembler::emit_label(Label* label) {
  if (label->is_bound()) {
    internal_reference_positions_.push_back(pc_offset());
    emit(reinterpret_cast<uint32_t>(buffer_ + label->pos()));
  } else {
    emit_disp(label, Displacement::CODE_ABSOLUTE);
  }
}


void Assembler::emit_farith(int b1, int b2, int i) {
  DCHECK(is_uint8(b1) && is_uint8(b2));  // wrong opcode
  DCHECK(0 <= i &&  i < 8);  // illegal stack offset
  EMIT(b1);
  EMIT(b2 + i);
}


void Assembler::db(uint8_t data) {
  EnsureSpace ensure_space(this);
  EMIT(data);
}


void Assembler::dd(uint32_t data) {
  EnsureSpace ensure_space(this);
  emit(data);
}


void Assembler::dq(uint64_t data) {
  EnsureSpace ensure_space(this);
  emit_q(data);
}


void Assembler::dd(Label* label) {
  EnsureSpace ensure_space(this);
  RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
  emit_label(label);
}


void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  DCHECK(!RelocInfo::IsNone(rmode));
  // Don't record external references unless the heap will be serialized.
  if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
      !serializer_enabled() && !emit_debug_code()) {
      return;
  }
  RelocInfo rinfo(isolate(), pc_, rmode, data, NULL);
  reloc_info_writer.Write(&rinfo);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87
