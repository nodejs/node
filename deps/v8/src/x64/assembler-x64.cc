// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/x64/assembler-x64.h"

#include <cstring>

#if V8_TARGET_ARCH_X64

#if V8_LIBC_MSVCRT
#include <intrin.h>  // _xgetbv()
#endif
#if V8_OS_MACOSX
#include <sys/sysctl.h>
#endif

#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/macro-assembler.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Implementation of CpuFeatures

namespace {

#if !V8_LIBC_MSVCRT

V8_INLINE uint64_t _xgetbv(unsigned int xcr) {
  unsigned eax, edx;
  // Check xgetbv; this uses a .byte sequence instead of the instruction
  // directly because older assemblers do not include support for xgetbv and
  // there is no easy way to conditionally compile based on the assembler
  // used.
  __asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return static_cast<uint64_t>(eax) | (static_cast<uint64_t>(edx) << 32);
}

#define _XCR_XFEATURE_ENABLED_MASK 0

#endif  // !V8_LIBC_MSVCRT


bool OSHasAVXSupport() {
#if V8_OS_MACOSX
  // Mac OS X up to 10.9 has a bug where AVX transitions were indeed being
  // caused by ISRs, so we detect that here and disable AVX in that case.
  char buffer[128];
  size_t buffer_size = arraysize(buffer);
  int ctl_name[] = {CTL_KERN, KERN_OSRELEASE};
  if (sysctl(ctl_name, 2, buffer, &buffer_size, nullptr, 0) != 0) {
    V8_Fatal(__FILE__, __LINE__, "V8 failed to get kernel version");
  }
  // The buffer now contains a string of the form XX.YY.ZZ, where
  // XX is the major kernel version component.
  char* period_pos = strchr(buffer, '.');
  DCHECK_NOT_NULL(period_pos);
  *period_pos = '\0';
  long kernel_version_major = strtol(buffer, nullptr, 10);  // NOLINT
  if (kernel_version_major <= 13) return false;
#endif  // V8_OS_MACOSX
  // Check whether OS claims to support AVX.
  uint64_t feature_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
  return (feature_mask & 0x6) == 0x6;
}

}  // namespace


void CpuFeatures::ProbeImpl(bool cross_compile) {
  base::CPU cpu;
  CHECK(cpu.has_sse2());  // SSE2 support is mandatory.
  CHECK(cpu.has_cmov());  // CMOV support is mandatory.

  // Only use statically determined features for cross compile (snapshot).
  if (cross_compile) return;

  if (cpu.has_sse41() && FLAG_enable_sse4_1) supported_ |= 1u << SSE4_1;
  if (cpu.has_ssse3() && FLAG_enable_ssse3) supported_ |= 1u << SSSE3;
  if (cpu.has_sse3() && FLAG_enable_sse3) supported_ |= 1u << SSE3;
  // SAHF is not generally available in long mode.
  if (cpu.has_sahf() && FLAG_enable_sahf) supported_ |= 1u << SAHF;
  if (cpu.has_avx() && FLAG_enable_avx && cpu.has_osxsave() &&
      OSHasAVXSupport()) {
    supported_ |= 1u << AVX;
  }
  if (cpu.has_fma3() && FLAG_enable_fma3 && cpu.has_osxsave() &&
      OSHasAVXSupport()) {
    supported_ |= 1u << FMA3;
  }
  if (cpu.has_bmi1() && FLAG_enable_bmi1) supported_ |= 1u << BMI1;
  if (cpu.has_bmi2() && FLAG_enable_bmi2) supported_ |= 1u << BMI2;
  if (cpu.has_lzcnt() && FLAG_enable_lzcnt) supported_ |= 1u << LZCNT;
  if (cpu.has_popcnt() && FLAG_enable_popcnt) supported_ |= 1u << POPCNT;
  if (strcmp(FLAG_mcpu, "auto") == 0) {
    if (cpu.is_atom()) supported_ |= 1u << ATOM;
  } else if (strcmp(FLAG_mcpu, "atom") == 0) {
    supported_ |= 1u << ATOM;
  }
}


void CpuFeatures::PrintTarget() { }
void CpuFeatures::PrintFeatures() {
  printf(
      "SSE3=%d SSSE3=%d SSE4_1=%d SAHF=%d AVX=%d FMA3=%d BMI1=%d BMI2=%d "
      "LZCNT=%d "
      "POPCNT=%d ATOM=%d\n",
      CpuFeatures::IsSupported(SSE3), CpuFeatures::IsSupported(SSSE3),
      CpuFeatures::IsSupported(SSE4_1), CpuFeatures::IsSupported(SAHF),
      CpuFeatures::IsSupported(AVX), CpuFeatures::IsSupported(FMA3),
      CpuFeatures::IsSupported(BMI1), CpuFeatures::IsSupported(BMI2),
      CpuFeatures::IsSupported(LZCNT), CpuFeatures::IsSupported(POPCNT),
      CpuFeatures::IsSupported(ATOM));
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

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

uint32_t RelocInfo::wasm_function_table_size_reference() {
  DCHECK(IsWasmFunctionTableSizeReference(rmode_));
  return Memory::uint32_at(pc_);
}

void RelocInfo::unchecked_update_wasm_memory_reference(
    Isolate* isolate, Address address, ICacheFlushMode icache_flush_mode) {
  Memory::Address_at(pc_) = address;
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(isolate, pc_, sizeof(Address));
  }
}

void RelocInfo::unchecked_update_wasm_size(Isolate* isolate, uint32_t size,
                                           ICacheFlushMode icache_flush_mode) {
  Memory::uint32_at(pc_) = size;
  if (icache_flush_mode != SKIP_ICACHE_FLUSH) {
    Assembler::FlushICache(isolate, pc_, sizeof(uint32_t));
  }
}

// -----------------------------------------------------------------------------
// Implementation of Operand

Operand::Operand(Register base, int32_t disp) : rex_(0) {
  len_ = 1;
  if (base.is(rsp) || base.is(r12)) {
    // SIB byte is needed to encode (rsp + offset) or (r12 + offset).
    set_sib(times_1, rsp, base);
  }

  if (disp == 0 && !base.is(rbp) && !base.is(r13)) {
    set_modrm(0, base);
  } else if (is_int8(disp)) {
    set_modrm(1, base);
    set_disp8(disp);
  } else {
    set_modrm(2, base);
    set_disp32(disp);
  }
}


Operand::Operand(Register base,
                 Register index,
                 ScaleFactor scale,
                 int32_t disp) : rex_(0) {
  DCHECK(!index.is(rsp));
  len_ = 1;
  set_sib(scale, index, base);
  if (disp == 0 && !base.is(rbp) && !base.is(r13)) {
    // This call to set_modrm doesn't overwrite the REX.B (or REX.X) bits
    // possibly set by set_sib.
    set_modrm(0, rsp);
  } else if (is_int8(disp)) {
    set_modrm(1, rsp);
    set_disp8(disp);
  } else {
    set_modrm(2, rsp);
    set_disp32(disp);
  }
}


Operand::Operand(Register index,
                 ScaleFactor scale,
                 int32_t disp) : rex_(0) {
  DCHECK(!index.is(rsp));
  len_ = 1;
  set_modrm(0, rsp);
  set_sib(scale, index, rbp);
  set_disp32(disp);
}


Operand::Operand(Label* label) : rex_(0), len_(1) {
  DCHECK_NOT_NULL(label);
  set_modrm(0, rbp);
  set_disp64(reinterpret_cast<intptr_t>(label));
}


Operand::Operand(const Operand& operand, int32_t offset) {
  DCHECK(operand.len_ >= 1);
  // Operand encodes REX ModR/M [SIB] [Disp].
  byte modrm = operand.buf_[0];
  DCHECK(modrm < 0xC0);  // Disallow mode 3 (register target).
  bool has_sib = ((modrm & 0x07) == 0x04);
  byte mode = modrm & 0xC0;
  int disp_offset = has_sib ? 2 : 1;
  int base_reg = (has_sib ? operand.buf_[1] : modrm) & 0x07;
  // Mode 0 with rbp/r13 as ModR/M or SIB base register always has a 32-bit
  // displacement.
  bool is_baseless = (mode == 0) && (base_reg == 0x05);  // No base or RIP base.
  int32_t disp_value = 0;
  if (mode == 0x80 || is_baseless) {
    // Mode 2 or mode 0 with rbp/r13 as base: Word displacement.
    disp_value = *bit_cast<const int32_t*>(&operand.buf_[disp_offset]);
  } else if (mode == 0x40) {
    // Mode 1: Byte displacement.
    disp_value = static_cast<signed char>(operand.buf_[disp_offset]);
  }

  // Write new operand with same registers, but with modified displacement.
  DCHECK(offset >= 0 ? disp_value + offset > disp_value
                     : disp_value + offset < disp_value);  // No overflow.
  disp_value += offset;
  rex_ = operand.rex_;
  if (!is_int8(disp_value) || is_baseless) {
    // Need 32 bits of displacement, mode 2 or mode 1 with register rbp/r13.
    buf_[0] = (modrm & 0x3f) | (is_baseless ? 0x00 : 0x80);
    len_ = disp_offset + 4;
    Memory::int32_at(&buf_[disp_offset]) = disp_value;
  } else if (disp_value != 0 || (base_reg == 0x05)) {
    // Need 8 bits of displacement.
    buf_[0] = (modrm & 0x3f) | 0x40;  // Mode 1.
    len_ = disp_offset + 1;
    buf_[disp_offset] = static_cast<byte>(disp_value);
  } else {
    // Need no displacement.
    buf_[0] = (modrm & 0x3f);  // Mode 0.
    len_ = disp_offset;
  }
  if (has_sib) {
    buf_[1] = operand.buf_[1];
  }
}


bool Operand::AddressUsesRegister(Register reg) const {
  int code = reg.code();
  DCHECK((buf_[0] & 0xC0) != 0xC0);  // Always a memory operand.
  // Start with only low three bits of base register. Initial decoding doesn't
  // distinguish on the REX.B bit.
  int base_code = buf_[0] & 0x07;
  if (base_code == rsp.code()) {
    // SIB byte present in buf_[1].
    // Check the index register from the SIB byte + REX.X prefix.
    int index_code = ((buf_[1] >> 3) & 0x07) | ((rex_ & 0x02) << 2);
    // Index code (including REX.X) of 0x04 (rsp) means no index register.
    if (index_code != rsp.code() && index_code == code) return true;
    // Add REX.B to get the full base register code.
    base_code = (buf_[1] & 0x07) | ((rex_ & 0x01) << 3);
    // A base register of 0x05 (rbp) with mod = 0 means no base register.
    if (base_code == rbp.code() && ((buf_[0] & 0xC0) == 0)) return false;
    return code == base_code;
  } else {
    // A base register with low bits of 0x05 (rbp or r13) and mod = 0 means
    // no base register.
    if (base_code == rbp.code() && ((buf_[0] & 0xC0) == 0)) return false;
    base_code |= ((rex_ & 0x01) << 3);
    return code == base_code;
  }
}

// -----------------------------------------------------------------------------
// Implementation of Assembler.

Assembler::Assembler(IsolateData isolate_data, void* buffer, int buffer_size)
    : AssemblerBase(isolate_data, buffer, buffer_size), code_targets_(100) {
// Clear the buffer in debug mode unless it was provided by the
// caller in which case we can't be sure it's okay to overwrite
// existing code in it.
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
  DCHECK(desc->instr_size > 0);  // Zero-size code objects upset the system.
  desc->reloc_size =
      static_cast<int>((buffer_ + buffer_size_) - reloc_info_writer.pos());
  desc->origin = this;
  desc->constant_pool_size = 0;
  desc->unwinding_info_size = 0;
  desc->unwinding_info = nullptr;
}


void Assembler::Align(int m) {
  DCHECK(base::bits::IsPowerOfTwo32(m));
  int delta = (m - (pc_offset() & (m - 1))) & (m - 1);
  Nop(delta);
}


void Assembler::CodeTargetAlign() {
  Align(16);  // Preferred alignment of jump targets on x64.
}


bool Assembler::IsNop(Address addr) {
  Address a = addr;
  while (*a == 0x66) a++;
  if (*a == 0x90) return true;
  if (a[0] == 0xf && a[1] == 0x1f) return true;
  return false;
}


void Assembler::bind_to(Label* L, int pos) {
  DCHECK(!L->is_bound());  // Label may only be bound once.
  DCHECK(0 <= pos && pos <= pc_offset());  // Position must be valid.
  if (L->is_linked()) {
    int current = L->pos();
    int next = long_at(current);
    while (next != current) {
      if (current >= 4 && long_at(current - 4) == 0) {
        // Absolute address.
        intptr_t imm64 = reinterpret_cast<intptr_t>(buffer_ + pos);
        *reinterpret_cast<intptr_t*>(addr_at(current - 4)) = imm64;
        internal_reference_positions_.push_back(current - 4);
      } else {
        // Relative address, relative to point after address.
        int imm32 = pos - (current + sizeof(int32_t));
        long_at_put(current, imm32);
      }
      current = next;
      next = long_at(next);
    }
    // Fix up last fixup on linked list.
    if (current >= 4 && long_at(current - 4) == 0) {
      // Absolute address.
      intptr_t imm64 = reinterpret_cast<intptr_t>(buffer_ + pos);
      *reinterpret_cast<intptr_t*>(addr_at(current - 4)) = imm64;
      internal_reference_positions_.push_back(current - 4);
    } else {
      // Relative address, relative to point after address.
      int imm32 = pos - (current + sizeof(int32_t));
      long_at_put(current, imm32);
    }
  }
  while (L->is_near_linked()) {
    int fixup_pos = L->near_link_pos();
    int offset_to_next =
        static_cast<int>(*reinterpret_cast<int8_t*>(addr_at(fixup_pos)));
    DCHECK(offset_to_next <= 0);
    int disp = pos - (fixup_pos + sizeof(int8_t));
    CHECK(is_int8(disp));
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
  bind_to(L, pc_offset());
}


void Assembler::GrowBuffer() {
  DCHECK(buffer_overflow());
  if (!own_buffer_) FATAL("external code buffer is too small");

  // Compute new buffer size.
  CodeDesc desc;  // the new buffer
  desc.buffer_size = 2 * buffer_size_;

  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if (desc.buffer_size > kMaximalBufferSize ||
      static_cast<size_t>(desc.buffer_size) >
          isolate_data().max_old_generation_size_) {
    V8::FatalProcessOutOfMemory("Assembler::GrowBuffer");
  }

  // Set up new buffer.
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.origin = this;
  desc.instr_size = pc_offset();
  desc.reloc_size =
      static_cast<int>((buffer_ + buffer_size_) - (reloc_info_writer.pos()));

  // Clear the buffer in debug mode. Use 'int3' instructions to make
  // sure to get into problems if we ever run uninitialized code.
#ifdef DEBUG
  memset(desc.buffer, 0xCC, desc.buffer_size);
#endif

  // Copy the data.
  intptr_t pc_delta = desc.buffer - buffer_;
  intptr_t rc_delta = (desc.buffer + desc.buffer_size) -
      (buffer_ + buffer_size_);
  MemMove(desc.buffer, buffer_, desc.instr_size);
  MemMove(rc_delta + reloc_info_writer.pos(), reloc_info_writer.pos(),
          desc.reloc_size);

  // Switch buffers.
  DeleteArray(buffer_);
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate internal references.
  for (auto pos : internal_reference_positions_) {
    intptr_t* p = reinterpret_cast<intptr_t*>(buffer_ + pos);
    *p += pc_delta;
  }

  DCHECK(!buffer_overflow());
}


void Assembler::emit_operand(int code, const Operand& adr) {
  DCHECK(is_uint3(code));
  const unsigned length = adr.len_;
  DCHECK(length > 0);

  // Emit updated ModR/M byte containing the given register.
  DCHECK((adr.buf_[0] & 0x38) == 0);
  *pc_++ = adr.buf_[0] | code << 3;

  // Recognize RIP relative addressing.
  if (adr.buf_[0] == 5) {
    DCHECK_EQ(9u, length);
    Label* label = *bit_cast<Label* const*>(&adr.buf_[1]);
    if (label->is_bound()) {
      int offset = label->pos() - pc_offset() - sizeof(int32_t);
      DCHECK_GE(0, offset);
      emitl(offset);
    } else if (label->is_linked()) {
      emitl(label->pos());
      label->link_to(pc_offset() - sizeof(int32_t));
    } else {
      DCHECK(label->is_unused());
      int32_t current = pc_offset();
      emitl(current);
      label->link_to(current);
    }
  } else {
    // Emit the rest of the encoded operand.
    for (unsigned i = 1; i < length; i++) *pc_++ = adr.buf_[i];
  }
}


// Assembler Instruction implementations.

void Assembler::arithmetic_op(byte opcode,
                              Register reg,
                              const Operand& op,
                              int size) {
  EnsureSpace ensure_space(this);
  emit_rex(reg, op, size);
  emit(opcode);
  emit_operand(reg, op);
}


void Assembler::arithmetic_op(byte opcode,
                              Register reg,
                              Register rm_reg,
                              int size) {
  EnsureSpace ensure_space(this);
  DCHECK((opcode & 0xC6) == 2);
  if (rm_reg.low_bits() == 4)  {  // Forces SIB byte.
    // Swap reg and rm_reg and change opcode operand order.
    emit_rex(rm_reg, reg, size);
    emit(opcode ^ 0x02);
    emit_modrm(rm_reg, reg);
  } else {
    emit_rex(reg, rm_reg, size);
    emit(opcode);
    emit_modrm(reg, rm_reg);
  }
}


void Assembler::arithmetic_op_16(byte opcode, Register reg, Register rm_reg) {
  EnsureSpace ensure_space(this);
  DCHECK((opcode & 0xC6) == 2);
  if (rm_reg.low_bits() == 4) {  // Forces SIB byte.
    // Swap reg and rm_reg and change opcode operand order.
    emit(0x66);
    emit_optional_rex_32(rm_reg, reg);
    emit(opcode ^ 0x02);
    emit_modrm(rm_reg, reg);
  } else {
    emit(0x66);
    emit_optional_rex_32(reg, rm_reg);
    emit(opcode);
    emit_modrm(reg, rm_reg);
  }
}


void Assembler::arithmetic_op_16(byte opcode,
                                 Register reg,
                                 const Operand& rm_reg) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg, rm_reg);
  emit(opcode);
  emit_operand(reg, rm_reg);
}


void Assembler::arithmetic_op_8(byte opcode, Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  if (!reg.is_byte_register()) {
    emit_rex_32(reg, op);
  } else {
    emit_optional_rex_32(reg, op);
  }
  emit(opcode);
  emit_operand(reg, op);
}


void Assembler::arithmetic_op_8(byte opcode, Register reg, Register rm_reg) {
  EnsureSpace ensure_space(this);
  DCHECK((opcode & 0xC6) == 2);
  if (rm_reg.low_bits() == 4)  {  // Forces SIB byte.
    // Swap reg and rm_reg and change opcode operand order.
    if (!rm_reg.is_byte_register() || !reg.is_byte_register()) {
      // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
      emit_rex_32(rm_reg, reg);
    }
    emit(opcode ^ 0x02);
    emit_modrm(rm_reg, reg);
  } else {
    if (!reg.is_byte_register() || !rm_reg.is_byte_register()) {
      // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
      emit_rex_32(reg, rm_reg);
    }
    emit(opcode);
    emit_modrm(reg, rm_reg);
  }
}


void Assembler::immediate_arithmetic_op(byte subcode,
                                        Register dst,
                                        Immediate src,
                                        int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  if (is_int8(src.value_) && RelocInfo::IsNone(src.rmode_)) {
    emit(0x83);
    emit_modrm(subcode, dst);
    emit(src.value_);
  } else if (dst.is(rax)) {
    emit(0x05 | (subcode << 3));
    emit(src);
  } else {
    emit(0x81);
    emit_modrm(subcode, dst);
    emit(src);
  }
}

void Assembler::immediate_arithmetic_op(byte subcode,
                                        const Operand& dst,
                                        Immediate src,
                                        int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  if (is_int8(src.value_) && RelocInfo::IsNone(src.rmode_)) {
    emit(0x83);
    emit_operand(subcode, dst);
    emit(src.value_);
  } else {
    emit(0x81);
    emit_operand(subcode, dst);
    emit(src);
  }
}


void Assembler::immediate_arithmetic_op_16(byte subcode,
                                           Register dst,
                                           Immediate src) {
  EnsureSpace ensure_space(this);
  emit(0x66);  // Operand size override prefix.
  emit_optional_rex_32(dst);
  if (is_int8(src.value_)) {
    emit(0x83);
    emit_modrm(subcode, dst);
    emit(src.value_);
  } else if (dst.is(rax)) {
    emit(0x05 | (subcode << 3));
    emitw(src.value_);
  } else {
    emit(0x81);
    emit_modrm(subcode, dst);
    emitw(src.value_);
  }
}


void Assembler::immediate_arithmetic_op_16(byte subcode,
                                           const Operand& dst,
                                           Immediate src) {
  EnsureSpace ensure_space(this);
  emit(0x66);  // Operand size override prefix.
  emit_optional_rex_32(dst);
  if (is_int8(src.value_)) {
    emit(0x83);
    emit_operand(subcode, dst);
    emit(src.value_);
  } else {
    emit(0x81);
    emit_operand(subcode, dst);
    emitw(src.value_);
  }
}


void Assembler::immediate_arithmetic_op_8(byte subcode,
                                          const Operand& dst,
                                          Immediate src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst);
  DCHECK(is_int8(src.value_) || is_uint8(src.value_));
  emit(0x80);
  emit_operand(subcode, dst);
  emit(src.value_);
}


void Assembler::immediate_arithmetic_op_8(byte subcode,
                                          Register dst,
                                          Immediate src) {
  EnsureSpace ensure_space(this);
  if (!dst.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst);
  }
  DCHECK(is_int8(src.value_) || is_uint8(src.value_));
  emit(0x80);
  emit_modrm(subcode, dst);
  emit(src.value_);
}


void Assembler::shift(Register dst,
                      Immediate shift_amount,
                      int subcode,
                      int size) {
  EnsureSpace ensure_space(this);
  DCHECK(size == kInt64Size ? is_uint6(shift_amount.value_)
                            : is_uint5(shift_amount.value_));
  if (shift_amount.value_ == 1) {
    emit_rex(dst, size);
    emit(0xD1);
    emit_modrm(subcode, dst);
  } else {
    emit_rex(dst, size);
    emit(0xC1);
    emit_modrm(subcode, dst);
    emit(shift_amount.value_);
  }
}


void Assembler::shift(Operand dst, Immediate shift_amount, int subcode,
                      int size) {
  EnsureSpace ensure_space(this);
  DCHECK(size == kInt64Size ? is_uint6(shift_amount.value_)
                            : is_uint5(shift_amount.value_));
  if (shift_amount.value_ == 1) {
    emit_rex(dst, size);
    emit(0xD1);
    emit_operand(subcode, dst);
  } else {
    emit_rex(dst, size);
    emit(0xC1);
    emit_operand(subcode, dst);
    emit(shift_amount.value_);
  }
}


void Assembler::shift(Register dst, int subcode, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xD3);
  emit_modrm(subcode, dst);
}


void Assembler::shift(Operand dst, int subcode, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xD3);
  emit_operand(subcode, dst);
}


void Assembler::bt(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xA3);
  emit_operand(src, dst);
}


void Assembler::bts(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xAB);
  emit_operand(src, dst);
}


void Assembler::bsrl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_modrm(dst, src);
}


void Assembler::bsrl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_operand(dst, src);
}


void Assembler::bsrq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_modrm(dst, src);
}


void Assembler::bsrq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_operand(dst, src);
}


void Assembler::bsfl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_modrm(dst, src);
}


void Assembler::bsfl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_operand(dst, src);
}


void Assembler::bsfq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_modrm(dst, src);
}


void Assembler::bsfq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_operand(dst, src);
}

void Assembler::pshufw(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x70);
  emit(0xC0 | (dst.low_bits() << 3) | src.low_bits());
  emit(shuffle);
}

void Assembler::pshufw(XMMRegister dst, const Operand& src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x70);
  emit_operand(dst.code(), src);
  emit(shuffle);
}

void Assembler::call(Label* L) {
  EnsureSpace ensure_space(this);
  // 1110 1000 #32-bit disp.
  emit(0xE8);
  if (L->is_bound()) {
    int offset = L->pos() - pc_offset() - sizeof(int32_t);
    DCHECK(offset <= 0);
    emitl(offset);
  } else if (L->is_linked()) {
    emitl(L->pos());
    L->link_to(pc_offset() - sizeof(int32_t));
  } else {
    DCHECK(L->is_unused());
    int32_t current = pc_offset();
    emitl(current);
    L->link_to(current);
  }
}


void Assembler::call(Address entry, RelocInfo::Mode rmode) {
  DCHECK(RelocInfo::IsRuntimeEntry(rmode));
  EnsureSpace ensure_space(this);
  // 1110 1000 #32-bit disp.
  emit(0xE8);
  emit_runtime_entry(entry, rmode);
}


void Assembler::call(Handle<Code> target,
                     RelocInfo::Mode rmode,
                     TypeFeedbackId ast_id) {
  EnsureSpace ensure_space(this);
  // 1110 1000 #32-bit disp.
  emit(0xE8);
  emit_code_target(target, rmode, ast_id);
}


void Assembler::call(Register adr) {
  EnsureSpace ensure_space(this);
  // Opcode: FF /2 r64.
  emit_optional_rex_32(adr);
  emit(0xFF);
  emit_modrm(0x2, adr);
}


void Assembler::call(const Operand& op) {
  EnsureSpace ensure_space(this);
  // Opcode: FF /2 m64.
  emit_optional_rex_32(op);
  emit(0xFF);
  emit_operand(0x2, op);
}


// Calls directly to the given address using a relative offset.
// Should only ever be used in Code objects for calls within the
// same Code object. Should not be used when generating new code (use labels),
// but only when patching existing code.
void Assembler::call(Address target) {
  EnsureSpace ensure_space(this);
  // 1110 1000 #32-bit disp.
  emit(0xE8);
  Address source = pc_ + 4;
  intptr_t displacement = target - source;
  DCHECK(is_int32(displacement));
  emitl(static_cast<int32_t>(displacement));
}


void Assembler::clc() {
  EnsureSpace ensure_space(this);
  emit(0xF8);
}


void Assembler::cld() {
  EnsureSpace ensure_space(this);
  emit(0xFC);
}

void Assembler::cdq() {
  EnsureSpace ensure_space(this);
  emit(0x99);
}


void Assembler::cmovq(Condition cc, Register dst, Register src) {
  if (cc == always) {
    movq(dst, src);
  } else if (cc == never) {
    return;
  }
  // No need to check CpuInfo for CMOV support, it's a required part of the
  // 64-bit architecture.
  DCHECK(cc >= 0);  // Use mov for unconditional moves.
  EnsureSpace ensure_space(this);
  // Opcode: REX.W 0f 40 + cc /r.
  emit_rex_64(dst, src);
  emit(0x0f);
  emit(0x40 + cc);
  emit_modrm(dst, src);
}


void Assembler::cmovq(Condition cc, Register dst, const Operand& src) {
  if (cc == always) {
    movq(dst, src);
  } else if (cc == never) {
    return;
  }
  DCHECK(cc >= 0);
  EnsureSpace ensure_space(this);
  // Opcode: REX.W 0f 40 + cc /r.
  emit_rex_64(dst, src);
  emit(0x0f);
  emit(0x40 + cc);
  emit_operand(dst, src);
}


void Assembler::cmovl(Condition cc, Register dst, Register src) {
  if (cc == always) {
    movl(dst, src);
  } else if (cc == never) {
    return;
  }
  DCHECK(cc >= 0);
  EnsureSpace ensure_space(this);
  // Opcode: 0f 40 + cc /r.
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x40 + cc);
  emit_modrm(dst, src);
}


void Assembler::cmovl(Condition cc, Register dst, const Operand& src) {
  if (cc == always) {
    movl(dst, src);
  } else if (cc == never) {
    return;
  }
  DCHECK(cc >= 0);
  EnsureSpace ensure_space(this);
  // Opcode: 0f 40 + cc /r.
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x40 + cc);
  emit_operand(dst, src);
}


void Assembler::cmpb_al(Immediate imm8) {
  DCHECK(is_int8(imm8.value_) || is_uint8(imm8.value_));
  EnsureSpace ensure_space(this);
  emit(0x3c);
  emit(imm8.value_);
}

void Assembler::lock() {
  EnsureSpace ensure_space(this);
  emit(0xf0);
}

void Assembler::cmpxchgb(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  if (!src.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(src, dst);
  } else {
    emit_optional_rex_32(src, dst);
  }
  emit(0x0f);
  emit(0xb0);
  emit_operand(src, dst);
}

void Assembler::cmpxchgw(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0f);
  emit(0xb1);
  emit_operand(src, dst);
}

void Assembler::emit_cmpxchg(const Operand& dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(src, dst, size);
  emit(0x0f);
  emit(0xb1);
  emit_operand(src, dst);
}

void Assembler::cpuid() {
  EnsureSpace ensure_space(this);
  emit(0x0F);
  emit(0xA2);
}


void Assembler::cqo() {
  EnsureSpace ensure_space(this);
  emit_rex_64();
  emit(0x99);
}


void Assembler::emit_dec(Register dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xFF);
  emit_modrm(0x1, dst);
}


void Assembler::emit_dec(const Operand& dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xFF);
  emit_operand(1, dst);
}


void Assembler::decb(Register dst) {
  EnsureSpace ensure_space(this);
  if (!dst.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst);
  }
  emit(0xFE);
  emit_modrm(0x1, dst);
}


void Assembler::decb(const Operand& dst) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst);
  emit(0xFE);
  emit_operand(1, dst);
}


void Assembler::enter(Immediate size) {
  EnsureSpace ensure_space(this);
  emit(0xC8);
  emitw(size.value_);  // 16 bit operand, always.
  emit(0);
}


void Assembler::hlt() {
  EnsureSpace ensure_space(this);
  emit(0xF4);
}


void Assembler::emit_idiv(Register src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(src, size);
  emit(0xF7);
  emit_modrm(0x7, src);
}


void Assembler::emit_div(Register src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(src, size);
  emit(0xF7);
  emit_modrm(0x6, src);
}


void Assembler::emit_imul(Register src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(src, size);
  emit(0xF7);
  emit_modrm(0x5, src);
}


void Assembler::emit_imul(const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(src, size);
  emit(0xF7);
  emit_operand(0x5, src);
}


void Assembler::emit_imul(Register dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  emit(0x0F);
  emit(0xAF);
  emit_modrm(dst, src);
}


void Assembler::emit_imul(Register dst, const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  emit(0x0F);
  emit(0xAF);
  emit_operand(dst, src);
}


void Assembler::emit_imul(Register dst, Register src, Immediate imm, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  if (is_int8(imm.value_)) {
    emit(0x6B);
    emit_modrm(dst, src);
    emit(imm.value_);
  } else {
    emit(0x69);
    emit_modrm(dst, src);
    emitl(imm.value_);
  }
}


void Assembler::emit_imul(Register dst, const Operand& src, Immediate imm,
                          int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  if (is_int8(imm.value_)) {
    emit(0x6B);
    emit_operand(dst, src);
    emit(imm.value_);
  } else {
    emit(0x69);
    emit_operand(dst, src);
    emitl(imm.value_);
  }
}


void Assembler::emit_inc(Register dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xFF);
  emit_modrm(0x0, dst);
}


void Assembler::emit_inc(const Operand& dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xFF);
  emit_operand(0, dst);
}


void Assembler::int3() {
  EnsureSpace ensure_space(this);
  emit(0xCC);
}


void Assembler::j(Condition cc, Label* L, Label::Distance distance) {
  if (cc == always) {
    jmp(L);
    return;
  } else if (cc == never) {
    return;
  }
  EnsureSpace ensure_space(this);
  DCHECK(is_uint4(cc));
  if (L->is_bound()) {
    const int short_size = 2;
    const int long_size  = 6;
    int offs = L->pos() - pc_offset();
    DCHECK(offs <= 0);
    // Determine whether we can use 1-byte offsets for backwards branches,
    // which have a max range of 128 bytes.

    // We also need to check predictable_code_size() flag here, because on x64,
    // when the full code generator recompiles code for debugging, some places
    // need to be padded out to a certain size. The debugger is keeping track of
    // how often it did this so that it can adjust return addresses on the
    // stack, but if the size of jump instructions can also change, that's not
    // enough and the calculated offsets would be incorrect.
    if (is_int8(offs - short_size) && !predictable_code_size()) {
      // 0111 tttn #8-bit disp.
      emit(0x70 | cc);
      emit((offs - short_size) & 0xFF);
    } else {
      // 0000 1111 1000 tttn #32-bit disp.
      emit(0x0F);
      emit(0x80 | cc);
      emitl(offs - long_size);
    }
  } else if (distance == Label::kNear) {
    // 0111 tttn #8-bit disp
    emit(0x70 | cc);
    byte disp = 0x00;
    if (L->is_near_linked()) {
      int offset = L->near_link_pos() - pc_offset();
      DCHECK(is_int8(offset));
      disp = static_cast<byte>(offset & 0xFF);
    }
    L->link_to(pc_offset(), Label::kNear);
    emit(disp);
  } else if (L->is_linked()) {
    // 0000 1111 1000 tttn #32-bit disp.
    emit(0x0F);
    emit(0x80 | cc);
    emitl(L->pos());
    L->link_to(pc_offset() - sizeof(int32_t));
  } else {
    DCHECK(L->is_unused());
    emit(0x0F);
    emit(0x80 | cc);
    int32_t current = pc_offset();
    emitl(current);
    L->link_to(current);
  }
}


void Assembler::j(Condition cc, Address entry, RelocInfo::Mode rmode) {
  DCHECK(RelocInfo::IsRuntimeEntry(rmode));
  EnsureSpace ensure_space(this);
  DCHECK(is_uint4(cc));
  emit(0x0F);
  emit(0x80 | cc);
  emit_runtime_entry(entry, rmode);
}


void Assembler::j(Condition cc,
                  Handle<Code> target,
                  RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint4(cc));
  // 0000 1111 1000 tttn #32-bit disp.
  emit(0x0F);
  emit(0x80 | cc);
  emit_code_target(target, rmode);
}


void Assembler::jmp(Label* L, Label::Distance distance) {
  EnsureSpace ensure_space(this);
  const int short_size = sizeof(int8_t);
  const int long_size = sizeof(int32_t);
  if (L->is_bound()) {
    int offs = L->pos() - pc_offset() - 1;
    DCHECK(offs <= 0);
    if (is_int8(offs - short_size) && !predictable_code_size()) {
      // 1110 1011 #8-bit disp.
      emit(0xEB);
      emit((offs - short_size) & 0xFF);
    } else {
      // 1110 1001 #32-bit disp.
      emit(0xE9);
      emitl(offs - long_size);
    }
  } else if (distance == Label::kNear) {
    emit(0xEB);
    byte disp = 0x00;
    if (L->is_near_linked()) {
      int offset = L->near_link_pos() - pc_offset();
      DCHECK(is_int8(offset));
      disp = static_cast<byte>(offset & 0xFF);
    }
    L->link_to(pc_offset(), Label::kNear);
    emit(disp);
  } else if (L->is_linked()) {
    // 1110 1001 #32-bit disp.
    emit(0xE9);
    emitl(L->pos());
    L->link_to(pc_offset() - long_size);
  } else {
    // 1110 1001 #32-bit disp.
    DCHECK(L->is_unused());
    emit(0xE9);
    int32_t current = pc_offset();
    emitl(current);
    L->link_to(current);
  }
}


void Assembler::jmp(Handle<Code> target, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  // 1110 1001 #32-bit disp.
  emit(0xE9);
  emit_code_target(target, rmode);
}


void Assembler::jmp(Register target) {
  EnsureSpace ensure_space(this);
  // Opcode FF/4 r64.
  emit_optional_rex_32(target);
  emit(0xFF);
  emit_modrm(0x4, target);
}


void Assembler::jmp(const Operand& src) {
  EnsureSpace ensure_space(this);
  // Opcode FF/4 m64.
  emit_optional_rex_32(src);
  emit(0xFF);
  emit_operand(0x4, src);
}


void Assembler::emit_lea(Register dst, const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  emit(0x8D);
  emit_operand(dst, src);
}


void Assembler::load_rax(void* value, RelocInfo::Mode mode) {
  EnsureSpace ensure_space(this);
  if (kPointerSize == kInt64Size) {
    emit(0x48);  // REX.W
    emit(0xA1);
    emitp(value, mode);
  } else {
    DCHECK(kPointerSize == kInt32Size);
    emit(0xA1);
    emitp(value, mode);
    // In 64-bit mode, need to zero extend the operand to 8 bytes.
    // See 2.2.1.4 in Intel64 and IA32 Architectures Software
    // Developer's Manual Volume 2.
    emitl(0);
  }
}


void Assembler::load_rax(ExternalReference ref) {
  load_rax(ref.address(), RelocInfo::EXTERNAL_REFERENCE);
}


void Assembler::leave() {
  EnsureSpace ensure_space(this);
  emit(0xC9);
}


void Assembler::movb(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  if (!dst.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst, src);
  } else {
    emit_optional_rex_32(dst, src);
  }
  emit(0x8A);
  emit_operand(dst, src);
}


void Assembler::movb(Register dst, Immediate imm) {
  EnsureSpace ensure_space(this);
  if (!dst.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst);
  }
  emit(0xB0 + dst.low_bits());
  emit(imm.value_);
}


void Assembler::movb(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  if (!src.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(src, dst);
  } else {
    emit_optional_rex_32(src, dst);
  }
  emit(0x88);
  emit_operand(src, dst);
}


void Assembler::movb(const Operand& dst, Immediate imm) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst);
  emit(0xC6);
  emit_operand(0x0, dst);
  emit(static_cast<byte>(imm.value_));
}


void Assembler::movw(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x8B);
  emit_operand(dst, src);
}


void Assembler::movw(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x89);
  emit_operand(src, dst);
}


void Assembler::movw(const Operand& dst, Immediate imm) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst);
  emit(0xC7);
  emit_operand(0x0, dst);
  emit(static_cast<byte>(imm.value_ & 0xff));
  emit(static_cast<byte>(imm.value_ >> 8));
}


void Assembler::emit_mov(Register dst, const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  emit(0x8B);
  emit_operand(dst, src);
}


void Assembler::emit_mov(Register dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  if (src.low_bits() == 4) {
    emit_rex(src, dst, size);
    emit(0x89);
    emit_modrm(src, dst);
  } else {
    emit_rex(dst, src, size);
    emit(0x8B);
    emit_modrm(dst, src);
  }
}


void Assembler::emit_mov(const Operand& dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(src, dst, size);
  emit(0x89);
  emit_operand(src, dst);
}


void Assembler::emit_mov(Register dst, Immediate value, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  if (size == kInt64Size) {
    emit(0xC7);
    emit_modrm(0x0, dst);
  } else {
    DCHECK(size == kInt32Size);
    emit(0xB8 + dst.low_bits());
  }
  emit(value);
}


void Assembler::emit_mov(const Operand& dst, Immediate value, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xC7);
  emit_operand(0x0, dst);
  emit(value);
}


void Assembler::movp(Register dst, void* value, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, kPointerSize);
  emit(0xB8 | dst.low_bits());
  emitp(value, rmode);
}

void Assembler::movq(Register dst, int64_t value, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst);
  emit(0xB8 | dst.low_bits());
  if (!RelocInfo::IsNone(rmode)) {
    RecordRelocInfo(rmode, value);
  }
  emitq(value);
}

void Assembler::movq(Register dst, uint64_t value, RelocInfo::Mode rmode) {
  movq(dst, static_cast<int64_t>(value), rmode);
}

// Loads the ip-relative location of the src label into the target location
// (as a 32-bit offset sign extended to 64-bit).
void Assembler::movl(const Operand& dst, Label* src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst);
  emit(0xC7);
  emit_operand(0, dst);
  if (src->is_bound()) {
    int offset = src->pos() - pc_offset() - sizeof(int32_t);
    DCHECK(offset <= 0);
    emitl(offset);
  } else if (src->is_linked()) {
    emitl(src->pos());
    src->link_to(pc_offset() - sizeof(int32_t));
  } else {
    DCHECK(src->is_unused());
    int32_t current = pc_offset();
    emitl(current);
    src->link_to(current);
  }
}


void Assembler::movsxbl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  if (!src.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst, src);
  } else {
    emit_optional_rex_32(dst, src);
  }
  emit(0x0F);
  emit(0xBE);
  emit_modrm(dst, src);
}


void Assembler::movsxbl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBE);
  emit_operand(dst, src);
}


void Assembler::movsxbq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBE);
  emit_operand(dst, src);
}

void Assembler::movsxbq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBE);
  emit_modrm(dst, src);
}

void Assembler::movsxwl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBF);
  emit_modrm(dst, src);
}


void Assembler::movsxwl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBF);
  emit_operand(dst, src);
}


void Assembler::movsxwq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBF);
  emit_operand(dst, src);
}

void Assembler::movsxwq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBF);
  emit_modrm(dst, src);
}

void Assembler::movsxlq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x63);
  emit_modrm(dst, src);
}


void Assembler::movsxlq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst, src);
  emit(0x63);
  emit_operand(dst, src);
}


void Assembler::emit_movzxb(Register dst, const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  // 32 bit operations zero the top 32 bits of 64 bit registers.  Therefore
  // there is no need to make this a 64 bit operation.
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB6);
  emit_operand(dst, src);
}


void Assembler::emit_movzxb(Register dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  // 32 bit operations zero the top 32 bits of 64 bit registers.  Therefore
  // there is no need to make this a 64 bit operation.
  if (!src.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst, src);
  } else {
    emit_optional_rex_32(dst, src);
  }
  emit(0x0F);
  emit(0xB6);
  emit_modrm(dst, src);
}


void Assembler::emit_movzxw(Register dst, const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  // 32 bit operations zero the top 32 bits of 64 bit registers.  Therefore
  // there is no need to make this a 64 bit operation.
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB7);
  emit_operand(dst, src);
}


void Assembler::emit_movzxw(Register dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  // 32 bit operations zero the top 32 bits of 64 bit registers.  Therefore
  // there is no need to make this a 64 bit operation.
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB7);
  emit_modrm(dst, src);
}


void Assembler::repmovsb() {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit(0xA4);
}


void Assembler::repmovsw() {
  EnsureSpace ensure_space(this);
  emit(0x66);  // Operand size override.
  emit(0xF3);
  emit(0xA4);
}


void Assembler::emit_repmovs(int size) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex(size);
  emit(0xA5);
}


void Assembler::mull(Register src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(src);
  emit(0xF7);
  emit_modrm(0x4, src);
}


void Assembler::mull(const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(src);
  emit(0xF7);
  emit_operand(0x4, src);
}


void Assembler::mulq(Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(src);
  emit(0xF7);
  emit_modrm(0x4, src);
}


void Assembler::emit_neg(Register dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xF7);
  emit_modrm(0x3, dst);
}


void Assembler::emit_neg(const Operand& dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex_64(dst);
  emit(0xF7);
  emit_operand(3, dst);
}


void Assembler::nop() {
  EnsureSpace ensure_space(this);
  emit(0x90);
}


void Assembler::emit_not(Register dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xF7);
  emit_modrm(0x2, dst);
}


void Assembler::emit_not(const Operand& dst, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, size);
  emit(0xF7);
  emit_operand(2, dst);
}


void Assembler::Nop(int n) {
  // The recommended muti-byte sequences of NOP instructions from the Intel 64
  // and IA-32 Architectures Software Developer's Manual.
  //
  // Length   Assembly                                Byte Sequence
  // 2 bytes  66 NOP                                  66 90H
  // 3 bytes  NOP DWORD ptr [EAX]                     0F 1F 00H
  // 4 bytes  NOP DWORD ptr [EAX + 00H]               0F 1F 40 00H
  // 5 bytes  NOP DWORD ptr [EAX + EAX*1 + 00H]       0F 1F 44 00 00H
  // 6 bytes  66 NOP DWORD ptr [EAX + EAX*1 + 00H]    66 0F 1F 44 00 00H
  // 7 bytes  NOP DWORD ptr [EAX + 00000000H]         0F 1F 80 00 00 00 00H
  // 8 bytes  NOP DWORD ptr [EAX + EAX*1 + 00000000H] 0F 1F 84 00 00 00 00 00H
  // 9 bytes  66 NOP DWORD ptr [EAX + EAX*1 +         66 0F 1F 84 00 00 00 00
  //          00000000H]                              00H

  EnsureSpace ensure_space(this);
  while (n > 0) {
    switch (n) {
      case 2:
        emit(0x66);
      case 1:
        emit(0x90);
        return;
      case 3:
        emit(0x0f);
        emit(0x1f);
        emit(0x00);
        return;
      case 4:
        emit(0x0f);
        emit(0x1f);
        emit(0x40);
        emit(0x00);
        return;
      case 6:
        emit(0x66);
      case 5:
        emit(0x0f);
        emit(0x1f);
        emit(0x44);
        emit(0x00);
        emit(0x00);
        return;
      case 7:
        emit(0x0f);
        emit(0x1f);
        emit(0x80);
        emit(0x00);
        emit(0x00);
        emit(0x00);
        emit(0x00);
        return;
      default:
      case 11:
        emit(0x66);
        n--;
      case 10:
        emit(0x66);
        n--;
      case 9:
        emit(0x66);
        n--;
      case 8:
        emit(0x0f);
        emit(0x1f);
        emit(0x84);
        emit(0x00);
        emit(0x00);
        emit(0x00);
        emit(0x00);
        emit(0x00);
        n -= 8;
    }
  }
}


void Assembler::popq(Register dst) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst);
  emit(0x58 | dst.low_bits());
}


void Assembler::popq(const Operand& dst) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst);
  emit(0x8F);
  emit_operand(0, dst);
}


void Assembler::popfq() {
  EnsureSpace ensure_space(this);
  emit(0x9D);
}


void Assembler::pushq(Register src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(src);
  emit(0x50 | src.low_bits());
}


void Assembler::pushq(const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(src);
  emit(0xFF);
  emit_operand(6, src);
}


void Assembler::pushq(Immediate value) {
  EnsureSpace ensure_space(this);
  if (is_int8(value.value_)) {
    emit(0x6A);
    emit(value.value_);  // Emit low byte of value.
  } else {
    emit(0x68);
    emitl(value.value_);
  }
}


void Assembler::pushq_imm32(int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit(0x68);
  emitl(imm32);
}


void Assembler::pushfq() {
  EnsureSpace ensure_space(this);
  emit(0x9C);
}


void Assembler::ret(int imm16) {
  EnsureSpace ensure_space(this);
  DCHECK(is_uint16(imm16));
  if (imm16 == 0) {
    emit(0xC3);
  } else {
    emit(0xC2);
    emit(imm16 & 0xFF);
    emit((imm16 >> 8) & 0xFF);
  }
}


void Assembler::ud2() {
  EnsureSpace ensure_space(this);
  emit(0x0F);
  emit(0x0B);
}


void Assembler::setcc(Condition cc, Register reg) {
  if (cc > last_condition) {
    movb(reg, Immediate(cc == always ? 1 : 0));
    return;
  }
  EnsureSpace ensure_space(this);
  DCHECK(is_uint4(cc));
  if (!reg.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(reg);
  }
  emit(0x0F);
  emit(0x90 | cc);
  emit_modrm(0x0, reg);
}


void Assembler::shld(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xA5);
  emit_modrm(src, dst);
}


void Assembler::shrd(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xAD);
  emit_modrm(src, dst);
}

void Assembler::xchgb(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  if (!reg.is_byte_register()) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(reg, op);
  } else {
    emit_optional_rex_32(reg, op);
  }
  emit(0x86);
  emit_operand(reg, op);
}

void Assembler::xchgw(Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg, op);
  emit(0x87);
  emit_operand(reg, op);
}

void Assembler::emit_xchg(Register dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  if (src.is(rax) || dst.is(rax)) {  // Single-byte encoding
    Register other = src.is(rax) ? dst : src;
    emit_rex(other, size);
    emit(0x90 | other.low_bits());
  } else if (dst.low_bits() == 4) {
    emit_rex(dst, src, size);
    emit(0x87);
    emit_modrm(dst, src);
  } else {
    emit_rex(src, dst, size);
    emit(0x87);
    emit_modrm(src, dst);
  }
}


void Assembler::emit_xchg(Register dst, const Operand& src, int size) {
  EnsureSpace ensure_space(this);
  emit_rex(dst, src, size);
  emit(0x87);
  emit_operand(dst, src);
}


void Assembler::store_rax(void* dst, RelocInfo::Mode mode) {
  EnsureSpace ensure_space(this);
  if (kPointerSize == kInt64Size) {
    emit(0x48);  // REX.W
    emit(0xA3);
    emitp(dst, mode);
  } else {
    DCHECK(kPointerSize == kInt32Size);
    emit(0xA3);
    emitp(dst, mode);
    // In 64-bit mode, need to zero extend the operand to 8 bytes.
    // See 2.2.1.4 in Intel64 and IA32 Architectures Software
    // Developer's Manual Volume 2.
    emitl(0);
  }
}


void Assembler::store_rax(ExternalReference ref) {
  store_rax(ref.address(), RelocInfo::EXTERNAL_REFERENCE);
}


void Assembler::testb(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  emit_test(dst, src, sizeof(int8_t));
}

void Assembler::testb(Register reg, Immediate mask) {
  DCHECK(is_int8(mask.value_) || is_uint8(mask.value_));
  emit_test(reg, mask, sizeof(int8_t));
}

void Assembler::testb(const Operand& op, Immediate mask) {
  DCHECK(is_int8(mask.value_) || is_uint8(mask.value_));
  emit_test(op, mask, sizeof(int8_t));
}


void Assembler::testb(const Operand& op, Register reg) {
  emit_test(op, reg, sizeof(int8_t));
}

void Assembler::testw(Register dst, Register src) {
  emit_test(dst, src, sizeof(uint16_t));
}

void Assembler::testw(Register reg, Immediate mask) {
  emit_test(reg, mask, sizeof(int16_t));
}

void Assembler::testw(const Operand& op, Immediate mask) {
  emit_test(op, mask, sizeof(int16_t));
}

void Assembler::testw(const Operand& op, Register reg) {
  emit_test(op, reg, sizeof(int16_t));
}

void Assembler::emit_test(Register dst, Register src, int size) {
  EnsureSpace ensure_space(this);
  if (src.low_bits() == 4) std::swap(dst, src);
  if (size == sizeof(int16_t)) {
    emit(0x66);
    size = sizeof(int32_t);
  }
  bool byte_operand = size == sizeof(int8_t);
  if (byte_operand) {
    size = sizeof(int32_t);
    if (!src.is_byte_register() || !dst.is_byte_register()) {
      emit_rex_32(dst, src);
    }
  } else {
    emit_rex(dst, src, size);
  }
  emit(byte_operand ? 0x84 : 0x85);
  emit_modrm(dst, src);
}


void Assembler::emit_test(Register reg, Immediate mask, int size) {
  if (is_uint8(mask.value_)) {
    size = sizeof(int8_t);
  } else if (is_uint16(mask.value_)) {
    size = sizeof(int16_t);
  }
  EnsureSpace ensure_space(this);
  bool half_word = size == sizeof(int16_t);
  if (half_word) {
    emit(0x66);
    size = sizeof(int32_t);
  }
  bool byte_operand = size == sizeof(int8_t);
  if (byte_operand) {
    size = sizeof(int32_t);
    if (!reg.is_byte_register()) emit_rex_32(reg);
  } else {
    emit_rex(reg, size);
  }
  if (reg.is(rax)) {
    emit(byte_operand ? 0xA8 : 0xA9);
  } else {
    emit(byte_operand ? 0xF6 : 0xF7);
    emit_modrm(0x0, reg);
  }
  if (byte_operand) {
    emit(mask.value_);
  } else if (half_word) {
    emitw(mask.value_);
  } else {
    emit(mask);
  }
}

void Assembler::emit_test(const Operand& op, Immediate mask, int size) {
  if (is_uint8(mask.value_)) {
    size = sizeof(int8_t);
  } else if (is_uint16(mask.value_)) {
    size = sizeof(int16_t);
  }
  EnsureSpace ensure_space(this);
  bool half_word = size == sizeof(int16_t);
  if (half_word) {
    emit(0x66);
    size = sizeof(int32_t);
  }
  bool byte_operand = size == sizeof(int8_t);
  if (byte_operand) {
    size = sizeof(int32_t);
  }
  emit_rex(rax, op, size);
  emit(byte_operand ? 0xF6 : 0xF7);
  emit_operand(rax, op);  // Operation code 0
  if (byte_operand) {
    emit(mask.value_);
  } else if (half_word) {
    emitw(mask.value_);
  } else {
    emit(mask);
  }
}

void Assembler::emit_test(const Operand& op, Register reg, int size) {
  EnsureSpace ensure_space(this);
  if (size == sizeof(int16_t)) {
    emit(0x66);
    size = sizeof(int32_t);
  }
  bool byte_operand = size == sizeof(int8_t);
  if (byte_operand) {
    size = sizeof(int32_t);
    if (!reg.is_byte_register()) {
      // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
      emit_rex_32(reg, op);
    } else {
      emit_optional_rex_32(reg, op);
    }
  } else {
    emit_rex(reg, op, size);
  }
  emit(byte_operand ? 0x84 : 0x85);
  emit_operand(reg, op);
}


// FPU instructions.


void Assembler::fld(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD9, 0xC0, i);
}


void Assembler::fld1() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xE8);
}


void Assembler::fldz() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xEE);
}


void Assembler::fldpi() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xEB);
}


void Assembler::fldln2() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xED);
}


void Assembler::fld_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xD9);
  emit_operand(0, adr);
}


void Assembler::fld_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDD);
  emit_operand(0, adr);
}


void Assembler::fstp_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xD9);
  emit_operand(3, adr);
}


void Assembler::fstp_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDD);
  emit_operand(3, adr);
}


void Assembler::fstp(int index) {
  DCHECK(is_uint3(index));
  EnsureSpace ensure_space(this);
  emit_farith(0xDD, 0xD8, index);
}


void Assembler::fild_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(0, adr);
}


void Assembler::fild_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDF);
  emit_operand(5, adr);
}


void Assembler::fistp_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(3, adr);
}


void Assembler::fisttp_s(const Operand& adr) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(1, adr);
}


void Assembler::fisttp_d(const Operand& adr) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDD);
  emit_operand(1, adr);
}


void Assembler::fist_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(2, adr);
}


void Assembler::fistp_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDF);
  emit_operand(7, adr);
}


void Assembler::fabs() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xE1);
}


void Assembler::fchs() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xE0);
}


void Assembler::fcos() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xFF);
}


void Assembler::fsin() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xFE);
}


void Assembler::fptan() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xF2);
}


void Assembler::fyl2x() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xF1);
}


void Assembler::f2xm1() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xF0);
}


void Assembler::fscale() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xFD);
}


void Assembler::fninit() {
  EnsureSpace ensure_space(this);
  emit(0xDB);
  emit(0xE3);
}


void Assembler::fadd(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xC0, i);
}


void Assembler::fsub(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xE8, i);
}


void Assembler::fisub_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(adr);
  emit(0xDA);
  emit_operand(4, adr);
}


void Assembler::fmul(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xC8, i);
}


void Assembler::fdiv(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xF8, i);
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
  emit(0xD9);
  emit(0xF8);
}


void Assembler::fprem1() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xF5);
}


void Assembler::fxch(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD9, 0xC8, i);
}


void Assembler::fincstp() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xF7);
}


void Assembler::ffree(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDD, 0xC0, i);
}


void Assembler::ftst() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xE4);
}


void Assembler::fucomp(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDD, 0xE8, i);
}


void Assembler::fucompp() {
  EnsureSpace ensure_space(this);
  emit(0xDA);
  emit(0xE9);
}


void Assembler::fucomi(int i) {
  EnsureSpace ensure_space(this);
  emit(0xDB);
  emit(0xE8 + i);
}


void Assembler::fucomip() {
  EnsureSpace ensure_space(this);
  emit(0xDF);
  emit(0xE9);
}


void Assembler::fcompp() {
  EnsureSpace ensure_space(this);
  emit(0xDE);
  emit(0xD9);
}


void Assembler::fnstsw_ax() {
  EnsureSpace ensure_space(this);
  emit(0xDF);
  emit(0xE0);
}


void Assembler::fwait() {
  EnsureSpace ensure_space(this);
  emit(0x9B);
}


void Assembler::frndint() {
  EnsureSpace ensure_space(this);
  emit(0xD9);
  emit(0xFC);
}


void Assembler::fnclex() {
  EnsureSpace ensure_space(this);
  emit(0xDB);
  emit(0xE2);
}


void Assembler::sahf() {
  // TODO(X64): Test for presence. Not all 64-bit intel CPU's have sahf
  // in 64-bit mode. Test CpuID.
  DCHECK(IsEnabled(SAHF));
  EnsureSpace ensure_space(this);
  emit(0x9E);
}


void Assembler::emit_farith(int b1, int b2, int i) {
  DCHECK(is_uint8(b1) && is_uint8(b2));  // wrong opcode
  DCHECK(is_uint3(i));  // illegal stack offset
  emit(b1);
  emit(b2 + i);
}


// SSE operations.

void Assembler::andps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x54);
  emit_sse_operand(dst, src);
}


void Assembler::andps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x54);
  emit_sse_operand(dst, src);
}


void Assembler::orps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x56);
  emit_sse_operand(dst, src);
}


void Assembler::orps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x56);
  emit_sse_operand(dst, src);
}


void Assembler::xorps(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x57);
  emit_sse_operand(dst, src);
}


void Assembler::xorps(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x57);
  emit_sse_operand(dst, src);
}


void Assembler::addps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::addps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::subps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::subps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::mulps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::mulps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::divps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}


void Assembler::divps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}


// SSE 2 operations.

void Assembler::movd(XMMRegister dst, Register src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x6E);
  emit_sse_operand(dst, src);
}


void Assembler::movd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x6E);
  emit_sse_operand(dst, src);
}


void Assembler::movd(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x7E);
  emit_sse_operand(src, dst);
}


void Assembler::movq(XMMRegister dst, Register src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x6E);
  emit_sse_operand(dst, src);
}


void Assembler::movq(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0x7E);
  emit_sse_operand(src, dst);
}


void Assembler::movq(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  if (dst.low_bits() == 4) {
    // Avoid unnecessary SIB byte.
    emit(0xf3);
    emit_optional_rex_32(dst, src);
    emit(0x0F);
    emit(0x7e);
    emit_sse_operand(dst, src);
  } else {
    emit(0x66);
    emit_optional_rex_32(src, dst);
    emit(0x0F);
    emit(0xD6);
    emit_sse_operand(src, dst);
  }
}


void Assembler::movdqa(const Operand& dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0x7F);
  emit_sse_operand(src, dst);
}


void Assembler::movdqa(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x6F);
  emit_sse_operand(dst, src);
}


void Assembler::movdqu(const Operand& dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0x7F);
  emit_sse_operand(src, dst);
}


void Assembler::movdqu(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x6F);
  emit_sse_operand(dst, src);
}


void Assembler::extractps(Register dst, XMMRegister src, byte imm8) {
  DCHECK(IsEnabled(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x17);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pextrb(Register dst, XMMRegister src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x14);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pextrb(const Operand& dst, XMMRegister src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x14);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pinsrw(XMMRegister dst, Register src, int8_t imm8) {
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xC4);
  emit_sse_operand(dst, src);
  emit(imm8);
}

void Assembler::pinsrw(XMMRegister dst, const Operand& src, int8_t imm8) {
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xC4);
  emit_sse_operand(dst, src);
  emit(imm8);
}

void Assembler::pextrw(Register dst, XMMRegister src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x15);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pextrw(const Operand& dst, XMMRegister src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x15);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pextrd(Register dst, XMMRegister src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x16);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pextrd(const Operand& dst, XMMRegister src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x3A);
  emit(0x16);
  emit_sse_operand(src, dst);
  emit(imm8);
}

void Assembler::pinsrd(XMMRegister dst, Register src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x3A);
  emit(0x22);
  emit_sse_operand(dst, src);
  emit(imm8);
}


void Assembler::pinsrd(XMMRegister dst, const Operand& src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x3A);
  emit(0x22);
  emit_sse_operand(dst, src);
  emit(imm8);
}

void Assembler::pinsrb(XMMRegister dst, Register src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x3A);
  emit(0x20);
  emit_sse_operand(dst, src);
  emit(imm8);
}

void Assembler::pinsrb(XMMRegister dst, const Operand& src, int8_t imm8) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x3A);
  emit(0x20);
  emit_sse_operand(dst, src);
  emit(imm8);
}

void Assembler::insertps(XMMRegister dst, XMMRegister src, byte imm8) {
  DCHECK(CpuFeatures::IsSupported(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x3A);
  emit(0x21);
  emit_sse_operand(dst, src);
  emit(imm8);
}

void Assembler::movsd(const Operand& dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);  // double
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x11);  // store
  emit_sse_operand(src, dst);
}


void Assembler::movsd(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);  // double
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);  // load
  emit_sse_operand(dst, src);
}


void Assembler::movsd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);  // double
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);  // load
  emit_sse_operand(dst, src);
}


void Assembler::movaps(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  if (src.low_bits() == 4) {
    // Try to avoid an unnecessary SIB byte.
    emit_optional_rex_32(src, dst);
    emit(0x0F);
    emit(0x29);
    emit_sse_operand(src, dst);
  } else {
    emit_optional_rex_32(dst, src);
    emit(0x0F);
    emit(0x28);
    emit_sse_operand(dst, src);
  }
}


void Assembler::shufps(XMMRegister dst, XMMRegister src, byte imm8) {
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xC6);
  emit_sse_operand(dst, src);
  emit(imm8);
}


void Assembler::movapd(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  if (src.low_bits() == 4) {
    // Try to avoid an unnecessary SIB byte.
    emit(0x66);
    emit_optional_rex_32(src, dst);
    emit(0x0F);
    emit(0x29);
    emit_sse_operand(src, dst);
  } else {
    emit(0x66);
    emit_optional_rex_32(dst, src);
    emit(0x0F);
    emit(0x28);
    emit_sse_operand(dst, src);
  }
}


void Assembler::movupd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);
  emit_sse_operand(dst, src);
}

void Assembler::movupd(const Operand& dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x11);
  emit_sse_operand(src, dst);
}

void Assembler::addss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::addss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::subss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::subss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::mulss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::mulss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::divss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}


void Assembler::divss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}


void Assembler::maxss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5F);
  emit_sse_operand(dst, src);
}


void Assembler::maxss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5F);
  emit_sse_operand(dst, src);
}


void Assembler::minss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5D);
  emit_sse_operand(dst, src);
}


void Assembler::minss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5D);
  emit_sse_operand(dst, src);
}


void Assembler::sqrtss(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x51);
  emit_sse_operand(dst, src);
}


void Assembler::sqrtss(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x51);
  emit_sse_operand(dst, src);
}


void Assembler::ucomiss(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x2e);
  emit_sse_operand(dst, src);
}


void Assembler::ucomiss(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x2e);
  emit_sse_operand(dst, src);
}


void Assembler::movss(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);  // single
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);  // load
  emit_sse_operand(dst, src);
}


void Assembler::movss(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);  // single
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);  // load
  emit_sse_operand(dst, src);
}


void Assembler::movss(const Operand& src, XMMRegister dst) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);  // single
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x11);  // store
  emit_sse_operand(dst, src);
}


void Assembler::psllq(XMMRegister reg, byte imm8) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x73);
  emit_sse_operand(rsi, reg);  // rsi == 6
  emit(imm8);
}


void Assembler::psrlq(XMMRegister reg, byte imm8) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x73);
  emit_sse_operand(rdx, reg);  // rdx == 2
  emit(imm8);
}

void Assembler::psllw(XMMRegister reg, byte imm8) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x71);
  emit_sse_operand(rsi, reg);  // rsi == 6
  emit(imm8);
}

void Assembler::pslld(XMMRegister reg, byte imm8) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x72);
  emit_sse_operand(rsi, reg);  // rsi == 6
  emit(imm8);
}

void Assembler::psrlw(XMMRegister reg, byte imm8) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x71);
  emit_sse_operand(rdx, reg);  // rdx == 2
  emit(imm8);
}

void Assembler::psrld(XMMRegister reg, byte imm8) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x72);
  emit_sse_operand(rdx, reg);  // rdx == 2
  emit(imm8);
}

void Assembler::psraw(XMMRegister reg, byte imm8) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x71);
  emit_sse_operand(rsp, reg);  // rsp == 4
  emit(imm8);
}

void Assembler::psrad(XMMRegister reg, byte imm8) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(reg);
  emit(0x0F);
  emit(0x72);
  emit_sse_operand(rsp, reg);  // rsp == 4
  emit(imm8);
}

void Assembler::cmpps(XMMRegister dst, XMMRegister src, int8_t cmp) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xC2);
  emit_sse_operand(dst, src);
  emit(cmp);
}

void Assembler::cmpps(XMMRegister dst, const Operand& src, int8_t cmp) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xC2);
  emit_sse_operand(dst, src);
  emit(cmp);
}

void Assembler::cmppd(XMMRegister dst, XMMRegister src, int8_t cmp) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x66);
  emit(0x0F);
  emit(0xC2);
  emit_sse_operand(dst, src);
  emit(cmp);
}

void Assembler::cmppd(XMMRegister dst, const Operand& src, int8_t cmp) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x66);
  emit(0x0F);
  emit(0xC2);
  emit_sse_operand(dst, src);
  emit(cmp);
}

void Assembler::cvttss2si(Register dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_operand(dst, src);
}


void Assembler::cvttss2si(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_sse_operand(dst, src);
}


void Assembler::cvttsd2si(Register dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_operand(dst, src);
}


void Assembler::cvttsd2si(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_sse_operand(dst, src);
}


void Assembler::cvttss2siq(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_sse_operand(dst, src);
}


void Assembler::cvttss2siq(Register dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_sse_operand(dst, src);
}


void Assembler::cvttsd2siq(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_sse_operand(dst, src);
}


void Assembler::cvttsd2siq(Register dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_sse_operand(dst, src);
}


void Assembler::cvtlsi2sd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtlsi2sd(XMMRegister dst, Register src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtlsi2ss(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtlsi2ss(XMMRegister dst, Register src) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtqsi2ss(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtqsi2ss(XMMRegister dst, Register src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtqsi2sd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtqsi2sd(XMMRegister dst, Register src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtss2sd(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtss2sd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtsd2ss(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtsd2ss(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtsd2si(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2D);
  emit_sse_operand(dst, src);
}


void Assembler::cvtsd2siq(Register dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2D);
  emit_sse_operand(dst, src);
}


void Assembler::addsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::addsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::mulsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::mulsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::subsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::subsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::divsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}


void Assembler::divsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}


void Assembler::maxsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5F);
  emit_sse_operand(dst, src);
}


void Assembler::maxsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5F);
  emit_sse_operand(dst, src);
}


void Assembler::minsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5D);
  emit_sse_operand(dst, src);
}


void Assembler::minsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5D);
  emit_sse_operand(dst, src);
}


void Assembler::andpd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x54);
  emit_sse_operand(dst, src);
}


void Assembler::andpd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x54);
  emit_sse_operand(dst, src);
}


void Assembler::orpd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x56);
  emit_sse_operand(dst, src);
}


void Assembler::orpd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x56);
  emit_sse_operand(dst, src);
}


void Assembler::xorpd(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x57);
  emit_sse_operand(dst, src);
}


void Assembler::xorpd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x57);
  emit_sse_operand(dst, src);
}


void Assembler::sqrtsd(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x51);
  emit_sse_operand(dst, src);
}


void Assembler::sqrtsd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x51);
  emit_sse_operand(dst, src);
}


void Assembler::ucomisd(XMMRegister dst, XMMRegister src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x2e);
  emit_sse_operand(dst, src);
}


void Assembler::ucomisd(XMMRegister dst, const Operand& src) {
  DCHECK(!IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x2e);
  emit_sse_operand(dst, src);
}


void Assembler::cmpltsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xC2);
  emit_sse_operand(dst, src);
  emit(0x01);  // LT == 1
}


void Assembler::roundss(XMMRegister dst, XMMRegister src, RoundingMode mode) {
  DCHECK(!IsEnabled(AVX));
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x3a);
  emit(0x0a);
  emit_sse_operand(dst, src);
  // Mask precision exception.
  emit(static_cast<byte>(mode) | 0x8);
}


void Assembler::roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode) {
  DCHECK(!IsEnabled(AVX));
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x3a);
  emit(0x0b);
  emit_sse_operand(dst, src);
  // Mask precision exception.
  emit(static_cast<byte>(mode) | 0x8);
}


void Assembler::movmskpd(Register dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x50);
  emit_sse_operand(dst, src);
}


void Assembler::movmskps(Register dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x50);
  emit_sse_operand(dst, src);
}


void Assembler::punpckldq(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x62);
  emit_sse_operand(dst, src);
}

void Assembler::punpckldq(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x62);
  emit_sse_operand(dst, src);
}

void Assembler::punpckhdq(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x6A);
  emit_sse_operand(dst, src);
}


// AVX instructions
void Assembler::vfmasd(byte op, XMMRegister dst, XMMRegister src1,
                       XMMRegister src2) {
  DCHECK(IsEnabled(FMA3));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, k66, k0F38, kW1);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vfmasd(byte op, XMMRegister dst, XMMRegister src1,
                       const Operand& src2) {
  DCHECK(IsEnabled(FMA3));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, k66, k0F38, kW1);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vfmass(byte op, XMMRegister dst, XMMRegister src1,
                       XMMRegister src2) {
  DCHECK(IsEnabled(FMA3));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, k66, k0F38, kW0);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vfmass(byte op, XMMRegister dst, XMMRegister src1,
                       const Operand& src2) {
  DCHECK(IsEnabled(FMA3));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, k66, k0F38, kW0);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vmovd(XMMRegister dst, Register src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  XMMRegister isrc = {src.code()};
  emit_vex_prefix(dst, xmm0, isrc, kL128, k66, k0F, kW0);
  emit(0x6e);
  emit_sse_operand(dst, src);
}


void Assembler::vmovd(XMMRegister dst, const Operand& src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, xmm0, src, kL128, k66, k0F, kW0);
  emit(0x6e);
  emit_sse_operand(dst, src);
}


void Assembler::vmovd(Register dst, XMMRegister src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  XMMRegister idst = {dst.code()};
  emit_vex_prefix(src, xmm0, idst, kL128, k66, k0F, kW0);
  emit(0x7e);
  emit_sse_operand(src, dst);
}


void Assembler::vmovq(XMMRegister dst, Register src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  XMMRegister isrc = {src.code()};
  emit_vex_prefix(dst, xmm0, isrc, kL128, k66, k0F, kW1);
  emit(0x6e);
  emit_sse_operand(dst, src);
}


void Assembler::vmovq(XMMRegister dst, const Operand& src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, xmm0, src, kL128, k66, k0F, kW1);
  emit(0x6e);
  emit_sse_operand(dst, src);
}


void Assembler::vmovq(Register dst, XMMRegister src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  XMMRegister idst = {dst.code()};
  emit_vex_prefix(src, xmm0, idst, kL128, k66, k0F, kW1);
  emit(0x7e);
  emit_sse_operand(src, dst);
}

void Assembler::vinstr(byte op, XMMRegister dst, XMMRegister src1,
                       XMMRegister src2, SIMDPrefix pp, LeadingOpcode m,
                       VexW w) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, pp, m, w);
  emit(op);
  emit_sse_operand(dst, src2);
}

void Assembler::vinstr(byte op, XMMRegister dst, XMMRegister src1,
                       const Operand& src2, SIMDPrefix pp, LeadingOpcode m,
                       VexW w) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, pp, m, w);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vps(byte op, XMMRegister dst, XMMRegister src1,
                    XMMRegister src2) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kL128, kNone, k0F, kWIG);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vps(byte op, XMMRegister dst, XMMRegister src1,
                    const Operand& src2) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kL128, kNone, k0F, kWIG);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vpd(byte op, XMMRegister dst, XMMRegister src1,
                    XMMRegister src2) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kL128, k66, k0F, kWIG);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vpd(byte op, XMMRegister dst, XMMRegister src1,
                    const Operand& src2) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kL128, k66, k0F, kWIG);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vucomiss(XMMRegister dst, XMMRegister src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, xmm0, src, kLIG, kNone, k0F, kWIG);
  emit(0x2e);
  emit_sse_operand(dst, src);
}


void Assembler::vucomiss(XMMRegister dst, const Operand& src) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, xmm0, src, kLIG, kNone, k0F, kWIG);
  emit(0x2e);
  emit_sse_operand(dst, src);
}


void Assembler::vss(byte op, XMMRegister dst, XMMRegister src1,
                    XMMRegister src2) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, kF3, k0F, kWIG);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::vss(byte op, XMMRegister dst, XMMRegister src1,
                    const Operand& src2) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, src1, src2, kLIG, kF3, k0F, kWIG);
  emit(op);
  emit_sse_operand(dst, src2);
}


void Assembler::bmi1q(byte op, Register reg, Register vreg, Register rm) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, kNone, k0F38, kW1);
  emit(op);
  emit_modrm(reg, rm);
}


void Assembler::bmi1q(byte op, Register reg, Register vreg, const Operand& rm) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, kNone, k0F38, kW1);
  emit(op);
  emit_operand(reg, rm);
}


void Assembler::bmi1l(byte op, Register reg, Register vreg, Register rm) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, kNone, k0F38, kW0);
  emit(op);
  emit_modrm(reg, rm);
}


void Assembler::bmi1l(byte op, Register reg, Register vreg, const Operand& rm) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, kNone, k0F38, kW0);
  emit(op);
  emit_operand(reg, rm);
}


void Assembler::tzcntq(Register dst, Register src) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_modrm(dst, src);
}


void Assembler::tzcntq(Register dst, const Operand& src) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_operand(dst, src);
}


void Assembler::tzcntl(Register dst, Register src) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_modrm(dst, src);
}


void Assembler::tzcntl(Register dst, const Operand& src) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBC);
  emit_operand(dst, src);
}


void Assembler::lzcntq(Register dst, Register src) {
  DCHECK(IsEnabled(LZCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_modrm(dst, src);
}


void Assembler::lzcntq(Register dst, const Operand& src) {
  DCHECK(IsEnabled(LZCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_operand(dst, src);
}


void Assembler::lzcntl(Register dst, Register src) {
  DCHECK(IsEnabled(LZCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_modrm(dst, src);
}


void Assembler::lzcntl(Register dst, const Operand& src) {
  DCHECK(IsEnabled(LZCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xBD);
  emit_operand(dst, src);
}


void Assembler::popcntq(Register dst, Register src) {
  DCHECK(IsEnabled(POPCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xB8);
  emit_modrm(dst, src);
}


void Assembler::popcntq(Register dst, const Operand& src) {
  DCHECK(IsEnabled(POPCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xB8);
  emit_operand(dst, src);
}


void Assembler::popcntl(Register dst, Register src) {
  DCHECK(IsEnabled(POPCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB8);
  emit_modrm(dst, src);
}


void Assembler::popcntl(Register dst, const Operand& src) {
  DCHECK(IsEnabled(POPCNT));
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB8);
  emit_operand(dst, src);
}


void Assembler::bmi2q(SIMDPrefix pp, byte op, Register reg, Register vreg,
                      Register rm) {
  DCHECK(IsEnabled(BMI2));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, pp, k0F38, kW1);
  emit(op);
  emit_modrm(reg, rm);
}


void Assembler::bmi2q(SIMDPrefix pp, byte op, Register reg, Register vreg,
                      const Operand& rm) {
  DCHECK(IsEnabled(BMI2));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, pp, k0F38, kW1);
  emit(op);
  emit_operand(reg, rm);
}


void Assembler::bmi2l(SIMDPrefix pp, byte op, Register reg, Register vreg,
                      Register rm) {
  DCHECK(IsEnabled(BMI2));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, pp, k0F38, kW0);
  emit(op);
  emit_modrm(reg, rm);
}


void Assembler::bmi2l(SIMDPrefix pp, byte op, Register reg, Register vreg,
                      const Operand& rm) {
  DCHECK(IsEnabled(BMI2));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(reg, vreg, rm, kLZ, pp, k0F38, kW0);
  emit(op);
  emit_operand(reg, rm);
}


void Assembler::rorxq(Register dst, Register src, byte imm8) {
  DCHECK(IsEnabled(BMI2));
  DCHECK(is_uint8(imm8));
  Register vreg = {0};  // VEX.vvvv unused
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, vreg, src, kLZ, kF2, k0F3A, kW1);
  emit(0xF0);
  emit_modrm(dst, src);
  emit(imm8);
}


void Assembler::rorxq(Register dst, const Operand& src, byte imm8) {
  DCHECK(IsEnabled(BMI2));
  DCHECK(is_uint8(imm8));
  Register vreg = {0};  // VEX.vvvv unused
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, vreg, src, kLZ, kF2, k0F3A, kW1);
  emit(0xF0);
  emit_operand(dst, src);
  emit(imm8);
}


void Assembler::rorxl(Register dst, Register src, byte imm8) {
  DCHECK(IsEnabled(BMI2));
  DCHECK(is_uint8(imm8));
  Register vreg = {0};  // VEX.vvvv unused
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, vreg, src, kLZ, kF2, k0F3A, kW0);
  emit(0xF0);
  emit_modrm(dst, src);
  emit(imm8);
}


void Assembler::rorxl(Register dst, const Operand& src, byte imm8) {
  DCHECK(IsEnabled(BMI2));
  DCHECK(is_uint8(imm8));
  Register vreg = {0};  // VEX.vvvv unused
  EnsureSpace ensure_space(this);
  emit_vex_prefix(dst, vreg, src, kLZ, kF2, k0F3A, kW0);
  emit(0xF0);
  emit_operand(dst, src);
  emit(imm8);
}

void Assembler::minps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5D);
  emit_sse_operand(dst, src);
}

void Assembler::minps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5D);
  emit_sse_operand(dst, src);
}

void Assembler::maxps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5F);
  emit_sse_operand(dst, src);
}

void Assembler::maxps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5F);
  emit_sse_operand(dst, src);
}

void Assembler::rcpps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x53);
  emit_sse_operand(dst, src);
}

void Assembler::rcpps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x53);
  emit_sse_operand(dst, src);
}

void Assembler::rsqrtps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x52);
  emit_sse_operand(dst, src);
}

void Assembler::rsqrtps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x52);
  emit_sse_operand(dst, src);
}

void Assembler::sqrtps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x51);
  emit_sse_operand(dst, src);
}

void Assembler::sqrtps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x51);
  emit_sse_operand(dst, src);
}

void Assembler::cvtdq2ps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5B);
  emit_sse_operand(dst, src);
}

void Assembler::cvtdq2ps(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5B);
  emit_sse_operand(dst, src);
}

void Assembler::movups(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  if (src.low_bits() == 4) {
    // Try to avoid an unnecessary SIB byte.
    emit_optional_rex_32(src, dst);
    emit(0x0F);
    emit(0x11);
    emit_sse_operand(src, dst);
  } else {
    emit_optional_rex_32(dst, src);
    emit(0x0F);
    emit(0x10);
    emit_sse_operand(dst, src);
  }
}

void Assembler::movups(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);
  emit_sse_operand(dst, src);
}

void Assembler::movups(const Operand& dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x11);
  emit_sse_operand(src, dst);
}

void Assembler::sse2_instr(XMMRegister dst, XMMRegister src, byte prefix,
                           byte escape, byte opcode) {
  EnsureSpace ensure_space(this);
  emit(prefix);
  emit_optional_rex_32(dst, src);
  emit(escape);
  emit(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::sse2_instr(XMMRegister dst, const Operand& src, byte prefix,
                           byte escape, byte opcode) {
  EnsureSpace ensure_space(this);
  emit(prefix);
  emit_optional_rex_32(dst, src);
  emit(escape);
  emit(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::ssse3_instr(XMMRegister dst, XMMRegister src, byte prefix,
                            byte escape1, byte escape2, byte opcode) {
  DCHECK(IsEnabled(SSSE3));
  EnsureSpace ensure_space(this);
  emit(prefix);
  emit_optional_rex_32(dst, src);
  emit(escape1);
  emit(escape2);
  emit(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::ssse3_instr(XMMRegister dst, const Operand& src, byte prefix,
                            byte escape1, byte escape2, byte opcode) {
  DCHECK(IsEnabled(SSSE3));
  EnsureSpace ensure_space(this);
  emit(prefix);
  emit_optional_rex_32(dst, src);
  emit(escape1);
  emit(escape2);
  emit(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::sse4_instr(XMMRegister dst, XMMRegister src, byte prefix,
                           byte escape1, byte escape2, byte opcode) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(prefix);
  emit_optional_rex_32(dst, src);
  emit(escape1);
  emit(escape2);
  emit(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::sse4_instr(XMMRegister dst, const Operand& src, byte prefix,
                           byte escape1, byte escape2, byte opcode) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  emit(prefix);
  emit_optional_rex_32(dst, src);
  emit(escape1);
  emit(escape2);
  emit(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::lddqu(XMMRegister dst, const Operand& src) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xF0);
  emit_sse_operand(dst, src);
}

void Assembler::psrldq(XMMRegister dst, uint8_t shift) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst);
  emit(0x0F);
  emit(0x73);
  emit_sse_operand(dst);
  emit(shift);
}

void Assembler::pshufhw(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x70);
  emit_sse_operand(dst, src);
  emit(shuffle);
}

void Assembler::pshuflw(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x70);
  emit_sse_operand(dst, src);
  emit(shuffle);
}

void Assembler::pshufd(XMMRegister dst, XMMRegister src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x70);
  emit_sse_operand(dst, src);
  emit(shuffle);
}

void Assembler::pshufd(XMMRegister dst, const Operand& src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  emit(0x66);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x70);
  emit_sse_operand(dst, src);
  emit(shuffle);
}

void Assembler::emit_sse_operand(XMMRegister reg, const Operand& adr) {
  Register ireg = { reg.code() };
  emit_operand(ireg, adr);
}


void Assembler::emit_sse_operand(Register reg, const Operand& adr) {
  Register ireg = {reg.code()};
  emit_operand(ireg, adr);
}


void Assembler::emit_sse_operand(XMMRegister dst, XMMRegister src) {
  emit(0xC0 | (dst.low_bits() << 3) | src.low_bits());
}


void Assembler::emit_sse_operand(XMMRegister dst, Register src) {
  emit(0xC0 | (dst.low_bits() << 3) | src.low_bits());
}


void Assembler::emit_sse_operand(Register dst, XMMRegister src) {
  emit(0xC0 | (dst.low_bits() << 3) | src.low_bits());
}

void Assembler::emit_sse_operand(XMMRegister dst) {
  emit(0xD8 | dst.low_bits());
}

void Assembler::RecordProtectedInstructionLanding(int pc_offset) {
  EnsureSpace ensure_space(this);
  RelocInfo rinfo(pc(), RelocInfo::WASM_PROTECTED_INSTRUCTION_LANDING,
                  pc_offset, nullptr);
  reloc_info_writer.Write(&rinfo);
}


void Assembler::db(uint8_t data) {
  EnsureSpace ensure_space(this);
  emit(data);
}


void Assembler::dd(uint32_t data) {
  EnsureSpace ensure_space(this);
  emitl(data);
}


void Assembler::dq(uint64_t data) {
  EnsureSpace ensure_space(this);
  emitq(data);
}


void Assembler::dq(Label* label) {
  EnsureSpace ensure_space(this);
  if (label->is_bound()) {
    internal_reference_positions_.push_back(pc_offset());
    emitp(buffer_ + label->pos(), RelocInfo::INTERNAL_REFERENCE);
  } else {
    RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
    emitl(0);  // Zero for the first 32bit marks it as 64bit absolute address.
    if (label->is_linked()) {
      emitl(label->pos());
      label->link_to(pc_offset() - sizeof(int32_t));
    } else {
      DCHECK(label->is_unused());
      int32_t current = pc_offset();
      emitl(current);
      label->link_to(current);
    }
  }
}


// Relocation information implementations.

void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  DCHECK(!RelocInfo::IsNone(rmode));
  // Don't record external references unless the heap will be serialized.
  if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
      !serializer_enabled() && !emit_debug_code()) {
    return;
  } else if (rmode == RelocInfo::CODE_AGE_SEQUENCE) {
    // Don't record psuedo relocation info for code age sequence mode.
    return;
  }
  RelocInfo rinfo(pc_, rmode, data, NULL);
  reloc_info_writer.Write(&rinfo);
}


const int RelocInfo::kApplyMask = RelocInfo::kCodeTargetMask |
    1 << RelocInfo::RUNTIME_ENTRY |
    1 << RelocInfo::INTERNAL_REFERENCE |
    1 << RelocInfo::CODE_AGE_SEQUENCE;


bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on x64 means that it is a relative 32 bit address, as used
  // by branch instructions.
  return (1 << rmode_) & kApplyMask;
}


bool RelocInfo::IsInConstantPool() {
  return false;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
