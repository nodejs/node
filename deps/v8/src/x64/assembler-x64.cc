// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "macro-assembler.h"
#include "serialize.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Implementation of Register

Register rax = { 0 };
Register rcx = { 1 };
Register rdx = { 2 };
Register rbx = { 3 };
Register rsp = { 4 };
Register rbp = { 5 };
Register rsi = { 6 };
Register rdi = { 7 };
Register r8 = { 8 };
Register r9 = { 9 };
Register r10 = { 10 };
Register r11 = { 11 };
Register r12 = { 12 };
Register r13 = { 13 };
Register r14 = { 14 };
Register r15 = { 15 };

Register no_reg = { -1 };

XMMRegister xmm0 = { 0 };
XMMRegister xmm1 = { 1 };
XMMRegister xmm2 = { 2 };
XMMRegister xmm3 = { 3 };
XMMRegister xmm4 = { 4 };
XMMRegister xmm5 = { 5 };
XMMRegister xmm6 = { 6 };
XMMRegister xmm7 = { 7 };
XMMRegister xmm8 = { 8 };
XMMRegister xmm9 = { 9 };
XMMRegister xmm10 = { 10 };
XMMRegister xmm11 = { 11 };
XMMRegister xmm12 = { 12 };
XMMRegister xmm13 = { 13 };
XMMRegister xmm14 = { 14 };
XMMRegister xmm15 = { 15 };


// -----------------------------------------------------------------------------
// Implementation of CpuFeatures

// The required user mode extensions in X64 are (from AMD64 ABI Table A.1):
//   fpu, tsc, cx8, cmov, mmx, sse, sse2, fxsr, syscall
uint64_t CpuFeatures::supported_ = kDefaultCpuFeatures;
uint64_t CpuFeatures::enabled_ = 0;

void CpuFeatures::Probe()  {
  ASSERT(Heap::HasBeenSetup());
  ASSERT(supported_ == kDefaultCpuFeatures);
  if (Serializer::enabled()) return;  // No features if we might serialize.

  Assembler assm(NULL, 0);
  Label cpuid, done;
#define __ assm.
  // Save old rsp, since we are going to modify the stack.
  __ push(rbp);
  __ pushfq();
  __ push(rcx);
  __ push(rbx);
  __ movq(rbp, rsp);

  // If we can modify bit 21 of the EFLAGS register, then CPUID is supported.
  __ pushfq();
  __ pop(rax);
  __ movq(rdx, rax);
  __ xor_(rax, Immediate(0x200000));  // Flip bit 21.
  __ push(rax);
  __ popfq();
  __ pushfq();
  __ pop(rax);
  __ xor_(rax, rdx);  // Different if CPUID is supported.
  __ j(not_zero, &cpuid);

  // CPUID not supported. Clear the supported features in edx:eax.
  __ xor_(rax, rax);
  __ jmp(&done);

  // Invoke CPUID with 1 in eax to get feature information in
  // ecx:edx. Temporarily enable CPUID support because we know it's
  // safe here.
  __ bind(&cpuid);
  __ movq(rax, Immediate(1));
  supported_ = kDefaultCpuFeatures | (1 << CPUID);
  { Scope fscope(CPUID);
    __ cpuid();
    // Move the result from ecx:edx to rdi.
    __ movl(rdi, rdx);  // Zero-extended to 64 bits.
    __ shl(rcx, Immediate(32));
    __ or_(rdi, rcx);

    // Get the sahf supported flag, from CPUID(0x80000001)
    __ movq(rax, 0x80000001, RelocInfo::NONE);
    __ cpuid();
  }
  supported_ = kDefaultCpuFeatures;

  // Put the CPU flags in rax.
  // rax = (rcx & 1) | (rdi & ~1) | (1 << CPUID).
  __ movl(rax, Immediate(1));
  __ and_(rcx, rax);  // Bit 0 is set if SAHF instruction supported.
  __ not_(rax);
  __ and_(rax, rdi);
  __ or_(rax, rcx);
  __ or_(rax, Immediate(1 << CPUID));

  // Done.
  __ bind(&done);
  __ movq(rsp, rbp);
  __ pop(rbx);
  __ pop(rcx);
  __ popfq();
  __ pop(rbp);
  __ ret(0);
#undef __

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code =
      Heap::CreateCode(desc, NULL, Code::ComputeFlags(Code::STUB), NULL);
  if (!code->IsCode()) return;
  LOG(CodeCreateEvent(Logger::BUILTIN_TAG,
                      Code::cast(code), "CpuFeatures::Probe"));
  typedef uint64_t (*F0)();
  F0 probe = FUNCTION_CAST<F0>(Code::cast(code)->entry());
  supported_ = probe();
  // SSE2 and CMOV must be available on an X64 CPU.
  ASSERT(IsSupported(CPUID));
  ASSERT(IsSupported(SSE2));
  ASSERT(IsSupported(CMOV));
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// Patch the code at the current PC with a call to the target address.
// Additional guard int3 instructions can be added if required.
void RelocInfo::PatchCodeWithCall(Address target, int guard_bytes) {
  // Load register with immediate 64 and call through a register instructions
  // takes up 13 bytes and int3 takes up one byte.
  static const int kCallCodeSize = 13;
  int code_size = kCallCodeSize + guard_bytes;

  // Create a code patcher.
  CodePatcher patcher(pc_, code_size);

  // Add a label for checking the size of the code used for returning.
#ifdef DEBUG
  Label check_codesize;
  patcher.masm()->bind(&check_codesize);
#endif

  // Patch the code.
  patcher.masm()->movq(r10, target, RelocInfo::NONE);
  patcher.masm()->call(r10);

  // Check that the size of the code generated is as expected.
  ASSERT_EQ(kCallCodeSize,
            patcher.masm()->SizeOfCodeGeneratedSince(&check_codesize));

  // Add the requested number of int3 instructions after the call.
  for (int i = 0; i < guard_bytes; i++) {
    patcher.masm()->int3();
  }
}


void RelocInfo::PatchCode(byte* instructions, int instruction_count) {
  // Patch the code at the current address with the supplied instructions.
  for (int i = 0; i < instruction_count; i++) {
    *(pc_ + i) = *(instructions + i);
  }

  // Indicate that code has changed.
  CPU::FlushICache(pc_, instruction_count);
}

// -----------------------------------------------------------------------------
// Implementation of Operand

Operand::Operand(Register base, int32_t disp): rex_(0) {
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
                 int32_t disp): rex_(0) {
  ASSERT(!index.is(rsp));
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


// -----------------------------------------------------------------------------
// Implementation of Assembler

#ifdef GENERATED_CODE_COVERAGE
static void InitCoverageLog();
#endif

byte* Assembler::spare_buffer_ = NULL;

Assembler::Assembler(void* buffer, int buffer_size)
    : code_targets_(100) {
  if (buffer == NULL) {
    // do our own buffer management
    if (buffer_size <= kMinimalBufferSize) {
      buffer_size = kMinimalBufferSize;

      if (spare_buffer_ != NULL) {
        buffer = spare_buffer_;
        spare_buffer_ = NULL;
      }
    }
    if (buffer == NULL) {
      buffer_ = NewArray<byte>(buffer_size);
    } else {
      buffer_ = static_cast<byte*>(buffer);
    }
    buffer_size_ = buffer_size;
    own_buffer_ = true;
  } else {
    // use externally provided buffer instead
    ASSERT(buffer_size > 0);
    buffer_ = static_cast<byte*>(buffer);
    buffer_size_ = buffer_size;
    own_buffer_ = false;
  }

  // Clear the buffer in debug mode unless it was provided by the
  // caller in which case we can't be sure it's okay to overwrite
  // existing code in it.
#ifdef DEBUG
  if (own_buffer_) {
    memset(buffer_, 0xCC, buffer_size);  // int3
  }
#endif

  // setup buffer pointers
  ASSERT(buffer_ != NULL);
  pc_ = buffer_;
  reloc_info_writer.Reposition(buffer_ + buffer_size, pc_);

  last_pc_ = NULL;
  current_statement_position_ = RelocInfo::kNoPosition;
  current_position_ = RelocInfo::kNoPosition;
  written_statement_position_ = current_statement_position_;
  written_position_ = current_position_;
#ifdef GENERATED_CODE_COVERAGE
  InitCoverageLog();
#endif
}


Assembler::~Assembler() {
  if (own_buffer_) {
    if (spare_buffer_ == NULL && buffer_size_ == kMinimalBufferSize) {
      spare_buffer_ = buffer_;
    } else {
      DeleteArray(buffer_);
    }
  }
}


void Assembler::GetCode(CodeDesc* desc) {
  // finalize code
  // (at this point overflow() may be true, but the gap ensures that
  // we are still not overlapping instructions and relocation info)
  ASSERT(pc_ <= reloc_info_writer.pos());  // no overlap
  // setup desc
  desc->buffer = buffer_;
  desc->buffer_size = buffer_size_;
  desc->instr_size = pc_offset();
  ASSERT(desc->instr_size > 0);  // Zero-size code objects upset the system.
  desc->reloc_size = (buffer_ + buffer_size_) - reloc_info_writer.pos();
  desc->origin = this;

  Counters::reloc_info_size.Increment(desc->reloc_size);
}


void Assembler::Align(int m) {
  ASSERT(IsPowerOf2(m));
  while ((pc_offset() & (m - 1)) != 0) {
    nop();
  }
}


void Assembler::bind_to(Label* L, int pos) {
  ASSERT(!L->is_bound());  // Label may only be bound once.
  last_pc_ = NULL;
  ASSERT(0 <= pos && pos <= pc_offset());  // Position must be valid.
  if (L->is_linked()) {
    int current = L->pos();
    int next = long_at(current);
    while (next != current) {
      // relative address, relative to point after address
      int imm32 = pos - (current + sizeof(int32_t));
      long_at_put(current, imm32);
      current = next;
      next = long_at(next);
    }
    // Fix up last fixup on linked list.
    int last_imm32 = pos - (current + sizeof(int32_t));
    long_at_put(current, last_imm32);
  }
  L->bind_to(pos);
}


void Assembler::bind(Label* L) {
  bind_to(L, pc_offset());
}


void Assembler::GrowBuffer() {
  ASSERT(buffer_overflow());  // should not call this otherwise
  if (!own_buffer_) FATAL("external code buffer is too small");

  // compute new buffer size
  CodeDesc desc;  // the new buffer
  if (buffer_size_ < 4*KB) {
    desc.buffer_size = 4*KB;
  } else {
    desc.buffer_size = 2*buffer_size_;
  }
  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if ((desc.buffer_size > kMaximalBufferSize) ||
      (desc.buffer_size > Heap::MaxOldGenerationSize())) {
    V8::FatalProcessOutOfMemory("Assembler::GrowBuffer");
  }

  // setup new buffer
  desc.buffer = NewArray<byte>(desc.buffer_size);
  desc.instr_size = pc_offset();
  desc.reloc_size = (buffer_ + buffer_size_) - (reloc_info_writer.pos());

  // Clear the buffer in debug mode. Use 'int3' instructions to make
  // sure to get into problems if we ever run uninitialized code.
#ifdef DEBUG
  memset(desc.buffer, 0xCC, desc.buffer_size);
#endif

  // copy the data
  intptr_t pc_delta = desc.buffer - buffer_;
  intptr_t rc_delta = (desc.buffer + desc.buffer_size) -
      (buffer_ + buffer_size_);
  memmove(desc.buffer, buffer_, desc.instr_size);
  memmove(rc_delta + reloc_info_writer.pos(),
          reloc_info_writer.pos(), desc.reloc_size);

  // switch buffers
  if (spare_buffer_ == NULL && buffer_size_ == kMinimalBufferSize) {
    spare_buffer_ = buffer_;
  } else {
    DeleteArray(buffer_);
  }
  buffer_ = desc.buffer;
  buffer_size_ = desc.buffer_size;
  pc_ += pc_delta;
  if (last_pc_ != NULL) {
    last_pc_ += pc_delta;
  }
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // relocate runtime entries
  for (RelocIterator it(desc); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::INTERNAL_REFERENCE) {
      intptr_t* p = reinterpret_cast<intptr_t*>(it.rinfo()->pc());
      if (*p != 0) {  // 0 means uninitialized.
        *p += pc_delta;
      }
    }
  }

  ASSERT(!buffer_overflow());
}


void Assembler::emit_operand(int code, const Operand& adr) {
  ASSERT(is_uint3(code));
  const unsigned length = adr.len_;
  ASSERT(length > 0);

  // Emit updated ModR/M byte containing the given register.
  ASSERT((adr.buf_[0] & 0x38) == 0);
  pc_[0] = adr.buf_[0] | code << 3;

  // Emit the rest of the encoded operand.
  for (unsigned i = 1; i < length; i++) pc_[i] = adr.buf_[i];
  pc_ += length;
}


// Assembler Instruction implementations

void Assembler::arithmetic_op(byte opcode, Register reg, const Operand& op) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(reg, op);
  emit(opcode);
  emit_operand(reg, op);
}


void Assembler::arithmetic_op(byte opcode, Register reg, Register rm_reg) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(reg, rm_reg);
  emit(opcode);
  emit_modrm(reg, rm_reg);
}


void Assembler::arithmetic_op_16(byte opcode, Register reg, Register rm_reg) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x66);
  emit_optional_rex_32(reg, rm_reg);
  emit(opcode);
  emit_modrm(reg, rm_reg);
}


void Assembler::arithmetic_op_16(byte opcode,
                                 Register reg,
                                 const Operand& rm_reg) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x66);
  emit_optional_rex_32(reg, rm_reg);
  emit(opcode);
  emit_operand(reg, rm_reg);
}


void Assembler::arithmetic_op_32(byte opcode, Register reg, Register rm_reg) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(reg, rm_reg);
  emit(opcode);
  emit_modrm(reg, rm_reg);
}


void Assembler::arithmetic_op_32(byte opcode,
                                 Register reg,
                                 const Operand& rm_reg) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(reg, rm_reg);
  emit(opcode);
  emit_operand(reg, rm_reg);
}


void Assembler::immediate_arithmetic_op(byte subcode,
                                        Register dst,
                                        Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  if (is_int8(src.value_)) {
    emit(0x83);
    emit_modrm(subcode, dst);
    emit(src.value_);
  } else if (dst.is(rax)) {
    emit(0x05 | (subcode << 3));
    emitl(src.value_);
  } else {
    emit(0x81);
    emit_modrm(subcode, dst);
    emitl(src.value_);
  }
}

void Assembler::immediate_arithmetic_op(byte subcode,
                                        const Operand& dst,
                                        Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  if (is_int8(src.value_)) {
    emit(0x83);
    emit_operand(subcode, dst);
    emit(src.value_);
  } else {
    emit(0x81);
    emit_operand(subcode, dst);
    emitl(src.value_);
  }
}


void Assembler::immediate_arithmetic_op_16(byte subcode,
                                           Register dst,
                                           Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
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
  last_pc_ = pc_;
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


void Assembler::immediate_arithmetic_op_32(byte subcode,
                                           Register dst,
                                           Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  if (is_int8(src.value_)) {
    emit(0x83);
    emit_modrm(subcode, dst);
    emit(src.value_);
  } else if (dst.is(rax)) {
    emit(0x05 | (subcode << 3));
    emitl(src.value_);
  } else {
    emit(0x81);
    emit_modrm(subcode, dst);
    emitl(src.value_);
  }
}


void Assembler::immediate_arithmetic_op_32(byte subcode,
                                           const Operand& dst,
                                           Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  if (is_int8(src.value_)) {
    emit(0x83);
    emit_operand(subcode, dst);
    emit(src.value_);
  } else {
    emit(0x81);
    emit_operand(subcode, dst);
    emitl(src.value_);
  }
}


void Assembler::immediate_arithmetic_op_8(byte subcode,
                                          const Operand& dst,
                                          Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  ASSERT(is_int8(src.value_) || is_uint8(src.value_));
  emit(0x80);
  emit_operand(subcode, dst);
  emit(src.value_);
}


void Assembler::immediate_arithmetic_op_8(byte subcode,
                                          Register dst,
                                          Immediate src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (dst.code() > 3) {
    // Use 64-bit mode byte registers.
    emit_rex_64(dst);
  }
  ASSERT(is_int8(src.value_) || is_uint8(src.value_));
  emit(0x80);
  emit_modrm(subcode, dst);
  emit(src.value_);
}


void Assembler::shift(Register dst, Immediate shift_amount, int subcode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  ASSERT(is_uint6(shift_amount.value_));  // illegal shift count
  if (shift_amount.value_ == 1) {
    emit_rex_64(dst);
    emit(0xD1);
    emit_modrm(subcode, dst);
  } else {
    emit_rex_64(dst);
    emit(0xC1);
    emit_modrm(subcode, dst);
    emit(shift_amount.value_);
  }
}


void Assembler::shift(Register dst, int subcode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xD3);
  emit_modrm(subcode, dst);
}


void Assembler::shift_32(Register dst, int subcode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xD3);
  emit_modrm(subcode, dst);
}


void Assembler::shift_32(Register dst, Immediate shift_amount, int subcode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  ASSERT(is_uint5(shift_amount.value_));  // illegal shift count
  if (shift_amount.value_ == 1) {
    emit_optional_rex_32(dst);
    emit(0xD1);
    emit_modrm(subcode, dst);
  } else {
    emit_optional_rex_32(dst);
    emit(0xC1);
    emit_modrm(subcode, dst);
    emit(shift_amount.value_);
  }
}


void Assembler::bt(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xA3);
  emit_operand(src, dst);
}


void Assembler::bts(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xAB);
  emit_operand(src, dst);
}


void Assembler::call(Label* L) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // 1110 1000 #32-bit disp
  emit(0xE8);
  if (L->is_bound()) {
    int offset = L->pos() - pc_offset() - sizeof(int32_t);
    ASSERT(offset <= 0);
    emitl(offset);
  } else if (L->is_linked()) {
    emitl(L->pos());
    L->link_to(pc_offset() - sizeof(int32_t));
  } else {
    ASSERT(L->is_unused());
    int32_t current = pc_offset();
    emitl(current);
    L->link_to(current);
  }
}


void Assembler::call(Handle<Code> target, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // 1110 1000 #32-bit disp
  emit(0xE8);
  emit_code_target(target, rmode);
}


void Assembler::call(Register adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode: FF /2 r64
  if (adr.high_bit()) {
    emit_rex_64(adr);
  }
  emit(0xFF);
  emit_modrm(0x2, adr);
}


void Assembler::call(const Operand& op) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode: FF /2 m64
  emit_rex_64(op);
  emit(0xFF);
  emit_operand(2, op);
}


void Assembler::clc() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF8);
}

void Assembler::cdq() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
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
  ASSERT(cc >= 0);  // Use mov for unconditional moves.
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode: REX.W 0f 40 + cc /r
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
  ASSERT(cc >= 0);
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode: REX.W 0f 40 + cc /r
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
  ASSERT(cc >= 0);
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode: 0f 40 + cc /r
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
  ASSERT(cc >= 0);
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode: 0f 40 + cc /r
  emit_optional_rex_32(dst, src);
  emit(0x0f);
  emit(0x40 + cc);
  emit_operand(dst, src);
}


void Assembler::cmpb_al(Immediate imm8) {
  ASSERT(is_int8(imm8.value_) || is_uint8(imm8.value_));
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x3c);
  emit(imm8.value_);
}


void Assembler::cpuid() {
  ASSERT(CpuFeatures::IsEnabled(CpuFeatures::CPUID));
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x0F);
  emit(0xA2);
}


void Assembler::cqo() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64();
  emit(0x99);
}


void Assembler::decq(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xFF);
  emit_modrm(0x1, dst);
}


void Assembler::decq(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xFF);
  emit_operand(1, dst);
}


void Assembler::decl(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xFF);
  emit_modrm(0x1, dst);
}


void Assembler::decl(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xFF);
  emit_operand(1, dst);
}


void Assembler::decb(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (dst.code() > 3) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst);
  }
  emit(0xFE);
  emit_modrm(0x1, dst);
}


void Assembler::decb(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xFE);
  emit_operand(1, dst);
}


void Assembler::enter(Immediate size) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xC8);
  emitw(size.value_);  // 16 bit operand, always.
  emit(0);
}


void Assembler::hlt() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF4);
}


void Assembler::idivq(Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src);
  emit(0xF7);
  emit_modrm(0x7, src);
}


void Assembler::idivl(Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(src);
  emit(0xF7);
  emit_modrm(0x7, src);
}


void Assembler::imul(Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src);
  emit(0xF7);
  emit_modrm(0x5, src);
}


void Assembler::imul(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xAF);
  emit_modrm(dst, src);
}


void Assembler::imul(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xAF);
  emit_operand(dst, src);
}


void Assembler::imul(Register dst, Register src, Immediate imm) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
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


void Assembler::imull(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xAF);
  emit_modrm(dst, src);
}


void Assembler::incq(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xFF);
  emit_modrm(0x0, dst);
}


void Assembler::incq(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xFF);
  emit_operand(0, dst);
}


void Assembler::incl(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xFF);
  emit_operand(0, dst);
}


void Assembler::int3() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xCC);
}


void Assembler::j(Condition cc, Label* L) {
  if (cc == always) {
    jmp(L);
    return;
  } else if (cc == never) {
    return;
  }
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  ASSERT(is_uint4(cc));
  if (L->is_bound()) {
    const int short_size = 2;
    const int long_size  = 6;
    int offs = L->pos() - pc_offset();
    ASSERT(offs <= 0);
    if (is_int8(offs - short_size)) {
      // 0111 tttn #8-bit disp
      emit(0x70 | cc);
      emit((offs - short_size) & 0xFF);
    } else {
      // 0000 1111 1000 tttn #32-bit disp
      emit(0x0F);
      emit(0x80 | cc);
      emitl(offs - long_size);
    }
  } else if (L->is_linked()) {
    // 0000 1111 1000 tttn #32-bit disp
    emit(0x0F);
    emit(0x80 | cc);
    emitl(L->pos());
    L->link_to(pc_offset() - sizeof(int32_t));
  } else {
    ASSERT(L->is_unused());
    emit(0x0F);
    emit(0x80 | cc);
    int32_t current = pc_offset();
    emitl(current);
    L->link_to(current);
  }
}


void Assembler::j(Condition cc,
                  Handle<Code> target,
                  RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  ASSERT(is_uint4(cc));
  // 0000 1111 1000 tttn #32-bit disp
  emit(0x0F);
  emit(0x80 | cc);
  emit_code_target(target, rmode);
}


void Assembler::jmp(Label* L) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (L->is_bound()) {
    int offs = L->pos() - pc_offset() - 1;
    ASSERT(offs <= 0);
    if (is_int8(offs - sizeof(int8_t))) {
      // 1110 1011 #8-bit disp
      emit(0xEB);
      emit((offs - sizeof(int8_t)) & 0xFF);
    } else {
      // 1110 1001 #32-bit disp
      emit(0xE9);
      emitl(offs - sizeof(int32_t));
    }
  } else  if (L->is_linked()) {
    // 1110 1001 #32-bit disp
    emit(0xE9);
    emitl(L->pos());
    L->link_to(pc_offset() - sizeof(int32_t));
  } else {
    // 1110 1001 #32-bit disp
    ASSERT(L->is_unused());
    emit(0xE9);
    int32_t current = pc_offset();
    emitl(current);
    L->link_to(current);
  }
}


void Assembler::jmp(Handle<Code> target, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // 1110 1001 #32-bit disp
  emit(0xE9);
  emit_code_target(target, rmode);
}


void Assembler::jmp(Register target) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode FF/4 r64
  if (target.high_bit()) {
    emit_rex_64(target);
  }
  emit(0xFF);
  emit_modrm(0x4, target);
}


void Assembler::jmp(const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  // Opcode FF/4 m64
  emit_optional_rex_32(src);
  emit(0xFF);
  emit_operand(0x4, src);
}


void Assembler::lea(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x8D);
  emit_operand(dst, src);
}


void Assembler::load_rax(void* value, RelocInfo::Mode mode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x48);  // REX.W
  emit(0xA1);
  emitq(reinterpret_cast<uintptr_t>(value), mode);
}


void Assembler::load_rax(ExternalReference ref) {
  load_rax(ref.address(), RelocInfo::EXTERNAL_REFERENCE);
}


void Assembler::leave() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xC9);
}


void Assembler::movb(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_32(dst, src);
  emit(0x8A);
  emit_operand(dst, src);
}

void Assembler::movb(Register dst, Immediate imm) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_32(dst);
  emit(0xC6);
  emit_modrm(0x0, dst);
  emit(imm.value_);
}

void Assembler::movb(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_32(src, dst);
  emit(0x88);
  emit_operand(src, dst);
}

void Assembler::movw(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x66);
  emit_optional_rex_32(src, dst);
  emit(0x89);
  emit_operand(src, dst);
}

void Assembler::movl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst, src);
  emit(0x8B);
  emit_operand(dst, src);
}


void Assembler::movl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst, src);
  emit(0x8B);
  emit_modrm(dst, src);
}


void Assembler::movl(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(src, dst);
  emit(0x89);
  emit_operand(src, dst);
}


void Assembler::movl(const Operand& dst, Immediate value) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xC7);
  emit_operand(0x0, dst);
  emit(value);  // Only 32-bit immediates are possible, not 8-bit immediates.
}


void Assembler::movl(Register dst, Immediate value) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xC7);
  emit_modrm(0x0, dst);
  emit(value);  // Only 32-bit immediates are possible, not 8-bit immediates.
}


void Assembler::movq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x8B);
  emit_operand(dst, src);
}


void Assembler::movq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x8B);
  emit_modrm(dst, src);
}


void Assembler::movq(Register dst, Immediate value) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xC7);
  emit_modrm(0x0, dst);
  emit(value);  // Only 32-bit immediates are possible, not 8-bit immediates.
}


void Assembler::movq(const Operand& dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src, dst);
  emit(0x89);
  emit_operand(src, dst);
}


void Assembler::movq(Register dst, void* value, RelocInfo::Mode rmode) {
  // This method must not be used with heap object references. The stored
  // address is not GC safe. Use the handle version instead.
  ASSERT(rmode > RelocInfo::LAST_GCED_ENUM);
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xB8 | dst.low_bits());
  emitq(reinterpret_cast<uintptr_t>(value), rmode);
}


void Assembler::movq(Register dst, int64_t value, RelocInfo::Mode rmode) {
  // Non-relocatable values might not need a 64-bit representation.
  if (rmode == RelocInfo::NONE) {
    // Sadly, there is no zero or sign extending move for 8-bit immediates.
    if (is_int32(value)) {
      movq(dst, Immediate(static_cast<int32_t>(value)));
      return;
    } else if (is_uint32(value)) {
      movl(dst, Immediate(static_cast<int32_t>(value)));
      return;
    }
    // Value cannot be represented by 32 bits, so do a full 64 bit immediate
    // value.
  }
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xB8 | dst.low_bits());
  emitq(value, rmode);
}


void Assembler::movq(Register dst, ExternalReference ref) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xB8 | dst.low_bits());
  emitq(reinterpret_cast<uintptr_t>(ref.address()),
        RelocInfo::EXTERNAL_REFERENCE);
}


void Assembler::movq(const Operand& dst, Immediate value) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xC7);
  emit_operand(0, dst);
  emit(value);
}


/*
 * Loads the ip-relative location of the src label into the target
 * location (as a 32-bit offset sign extended to 64-bit).
 */
void Assembler::movl(const Operand& dst, Label* src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xC7);
  emit_operand(0, dst);
  if (src->is_bound()) {
    int offset = src->pos() - pc_offset() - sizeof(int32_t);
    ASSERT(offset <= 0);
    emitl(offset);
  } else if (src->is_linked()) {
    emitl(src->pos());
    src->link_to(pc_offset() - sizeof(int32_t));
  } else {
    ASSERT(src->is_unused());
    int32_t current = pc_offset();
    emitl(current);
    src->link_to(current);
  }
}


void Assembler::movq(Register dst, Handle<Object> value, RelocInfo::Mode mode) {
  // If there is no relocation info, emit the value of the handle efficiently
  // (possibly using less that 8 bytes for the value).
  if (mode == RelocInfo::NONE) {
    // There is no possible reason to store a heap pointer without relocation
    // info, so it must be a smi.
    ASSERT(value->IsSmi());
    movq(dst, reinterpret_cast<int64_t>(*value), RelocInfo::NONE);
  } else {
    EnsureSpace ensure_space(this);
    last_pc_ = pc_;
    ASSERT(value->IsHeapObject());
    ASSERT(!Heap::InNewSpace(*value));
    emit_rex_64(dst);
    emit(0xB8 | dst.low_bits());
    emitq(reinterpret_cast<uintptr_t>(value.location()), mode);
  }
}


void Assembler::movsxbq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_32(dst, src);
  emit(0x0F);
  emit(0xBE);
  emit_operand(dst, src);
}


void Assembler::movsxwq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xBF);
  emit_operand(dst, src);
}


void Assembler::movsxlq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x63);
  emit_modrm(dst, src);
}


void Assembler::movsxlq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x63);
  emit_operand(dst, src);
}


void Assembler::movzxbq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xB6);
  emit_operand(dst, src);
}


void Assembler::movzxbl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB6);
  emit_operand(dst, src);
}


void Assembler::movzxwq(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0xB7);
  emit_operand(dst, src);
}


void Assembler::movzxwl(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0xB7);
  emit_operand(dst, src);
}


void Assembler::mul(Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src);
  emit(0xF7);
  emit_modrm(0x4, src);
}


void Assembler::neg(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xF7);
  emit_modrm(0x3, dst);
}


void Assembler::negl(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst);
  emit(0xF7);
  emit_modrm(0x3, dst);
}


void Assembler::neg(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xF7);
  emit_operand(3, dst);
}


void Assembler::nop() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x90);
}


void Assembler::not_(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xF7);
  emit_modrm(0x2, dst);
}


void Assembler::not_(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);
  emit(0xF7);
  emit_operand(2, dst);
}


void Assembler::nop(int n) {
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

  ASSERT(1 <= n);
  ASSERT(n <= 9);
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  switch (n) {
  case 1:
    emit(0x90);
    return;
  case 2:
    emit(0x66);
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
  case 5:
    emit(0x0f);
    emit(0x1f);
    emit(0x44);
    emit(0x00);
    emit(0x00);
    return;
  case 6:
    emit(0x66);
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
  case 8:
    emit(0x0f);
    emit(0x1f);
    emit(0x84);
    emit(0x00);
    emit(0x00);
    emit(0x00);
    emit(0x00);
    emit(0x00);
    return;
  case 9:
    emit(0x66);
    emit(0x0f);
    emit(0x1f);
    emit(0x84);
    emit(0x00);
    emit(0x00);
    emit(0x00);
    emit(0x00);
    emit(0x00);
    return;
  }
}


void Assembler::pop(Register dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (dst.high_bit()) {
    emit_rex_64(dst);
  }
  emit(0x58 | dst.low_bits());
}


void Assembler::pop(const Operand& dst) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst);  // Could be omitted in some cases.
  emit(0x8F);
  emit_operand(0, dst);
}


void Assembler::popfq() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x9D);
}


void Assembler::push(Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (src.high_bit()) {
    emit_rex_64(src);
  }
  emit(0x50 | src.low_bits());
}


void Assembler::push(const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src);  // Could be omitted in some cases.
  emit(0xFF);
  emit_operand(6, src);
}


void Assembler::push(Immediate value) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (is_int8(value.value_)) {
    emit(0x6A);
    emit(value.value_);  // Emit low byte of value.
  } else {
    emit(0x68);
    emitl(value.value_);
  }
}


void Assembler::pushfq() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x9C);
}


void Assembler::rdtsc() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x0F);
  emit(0x31);
}


void Assembler::ret(int imm16) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  ASSERT(is_uint16(imm16));
  if (imm16 == 0) {
    emit(0xC3);
  } else {
    emit(0xC2);
    emit(imm16 & 0xFF);
    emit((imm16 >> 8) & 0xFF);
  }
}


void Assembler::setcc(Condition cc, Register reg) {
  if (cc > last_condition) {
    movb(reg, Immediate(cc == always ? 1 : 0));
    return;
  }
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  ASSERT(is_uint4(cc));
  if (reg.code() > 3) {  // Use x64 byte registers, where different.
    emit_rex_32(reg);
  }
  emit(0x0F);
  emit(0x90 | cc);
  emit_modrm(0x0, reg);
}


void Assembler::shld(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xA5);
  emit_modrm(src, dst);
}


void Assembler::shrd(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(src, dst);
  emit(0x0F);
  emit(0xAD);
  emit_modrm(src, dst);
}


void Assembler::xchg(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (src.is(rax) || dst.is(rax)) {  // Single-byte encoding
    Register other = src.is(rax) ? dst : src;
    emit_rex_64(other);
    emit(0x90 | other.low_bits());
  } else {
    emit_rex_64(src, dst);
    emit(0x87);
    emit_modrm(src, dst);
  }
}


void Assembler::store_rax(void* dst, RelocInfo::Mode mode) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x48);  // REX.W
  emit(0xA3);
  emitq(reinterpret_cast<uintptr_t>(dst), mode);
}


void Assembler::store_rax(ExternalReference ref) {
  store_rax(ref.address(), RelocInfo::EXTERNAL_REFERENCE);
}


void Assembler::testb(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (dst.code() > 3 || src.code() > 3) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_32(dst, src);
  }
  emit(0x84);
  emit_modrm(dst, src);
}


void Assembler::testb(Register reg, Immediate mask) {
  ASSERT(is_int8(mask.value_) || is_uint8(mask.value_));
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (reg.is(rax)) {
    emit(0xA8);
    emit(mask.value_);  // Low byte emitted.
  } else {
    if (reg.code() > 3) {
      // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
      emit_rex_32(reg);
    }
    emit(0xF6);
    emit_modrm(0x0, reg);
    emit(mask.value_);  // Low byte emitted.
  }
}


void Assembler::testb(const Operand& op, Immediate mask) {
  ASSERT(is_int8(mask.value_) || is_uint8(mask.value_));
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(rax, op);
  emit(0xF6);
  emit_operand(rax, op);  // Operation code 0
  emit(mask.value_);  // Low byte emitted.
}


void Assembler::testl(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(dst, src);
  emit(0x85);
  emit_modrm(dst, src);
}


void Assembler::testl(Register reg, Immediate mask) {
  // testl with a mask that fits in the low byte is exactly testb.
  if (is_uint8(mask.value_)) {
    testb(reg, mask);
    return;
  }
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (reg.is(rax)) {
    emit(0xA9);
    emit(mask);
  } else {
    emit_optional_rex_32(rax, reg);
    emit(0xF7);
    emit_modrm(0x0, reg);
    emit(mask);
  }
}


void Assembler::testl(const Operand& op, Immediate mask) {
  // testl with a mask that fits in the low byte is exactly testb.
  if (is_uint8(mask.value_)) {
    testb(op, mask);
    return;
  }
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(rax, op);
  emit(0xF7);
  emit_operand(rax, op);  // Operation code 0
  emit(mask);
}


void Assembler::testq(const Operand& op, Register reg) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(reg, op);
  emit(0x85);
  emit_operand(reg, op);
}


void Assembler::testq(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_rex_64(dst, src);
  emit(0x85);
  emit_modrm(dst, src);
}


void Assembler::testq(Register dst, Immediate mask) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  if (dst.is(rax)) {
    emit_rex_64();
    emit(0xA9);
    emit(mask);
  } else {
    emit_rex_64(dst);
    emit(0xF7);
    emit_modrm(0, dst);
    emit(mask);
  }
}


// FPU instructions


void Assembler::fld(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xD9, 0xC0, i);
}


void Assembler::fld1() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xE8);
}


void Assembler::fldz() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xEE);
}


void Assembler::fld_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xD9);
  emit_operand(0, adr);
}


void Assembler::fld_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDD);
  emit_operand(0, adr);
}


void Assembler::fstp_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xD9);
  emit_operand(3, adr);
}


void Assembler::fstp_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDD);
  emit_operand(3, adr);
}


void Assembler::fstp(int index) {
  ASSERT(is_uint3(index));
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDD, 0xD8, index);
}


void Assembler::fild_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(0, adr);
}


void Assembler::fild_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDF);
  emit_operand(5, adr);
}


void Assembler::fistp_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(3, adr);
}


void Assembler::fisttp_s(const Operand& adr) {
  ASSERT(CpuFeatures::IsEnabled(CpuFeatures::SSE3));
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(1, adr);
}


void Assembler::fist_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDB);
  emit_operand(2, adr);
}


void Assembler::fistp_d(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDF);
  emit_operand(7, adr);
}


void Assembler::fabs() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xE1);
}


void Assembler::fchs() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xE0);
}


void Assembler::fcos() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xFF);
}


void Assembler::fsin() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xFE);
}


void Assembler::fadd(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDC, 0xC0, i);
}


void Assembler::fsub(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDC, 0xE8, i);
}


void Assembler::fisub_s(const Operand& adr) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_optional_rex_32(adr);
  emit(0xDA);
  emit_operand(4, adr);
}


void Assembler::fmul(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDC, 0xC8, i);
}


void Assembler::fdiv(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDC, 0xF8, i);
}


void Assembler::faddp(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDE, 0xC0, i);
}


void Assembler::fsubp(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDE, 0xE8, i);
}


void Assembler::fsubrp(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDE, 0xE0, i);
}


void Assembler::fmulp(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDE, 0xC8, i);
}


void Assembler::fdivp(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDE, 0xF8, i);
}


void Assembler::fprem() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xF8);
}


void Assembler::fprem1() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xF5);
}


void Assembler::fxch(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xD9, 0xC8, i);
}


void Assembler::fincstp() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xF7);
}


void Assembler::ffree(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDD, 0xC0, i);
}


void Assembler::ftst() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xE4);
}


void Assembler::fucomp(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit_farith(0xDD, 0xE8, i);
}


void Assembler::fucompp() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xDA);
  emit(0xE9);
}


void Assembler::fucomi(int i) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xDB);
  emit(0xE8 + i);
}


void Assembler::fucomip() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xDF);
  emit(0xE9);
}


void Assembler::fcompp() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xDE);
  emit(0xD9);
}


void Assembler::fnstsw_ax() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xDF);
  emit(0xE0);
}


void Assembler::fwait() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x9B);
}


void Assembler::frndint() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xD9);
  emit(0xFC);
}


void Assembler::fnclex() {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xDB);
  emit(0xE2);
}


void Assembler::sahf() {
  // TODO(X64): Test for presence. Not all 64-bit intel CPU's have sahf
  // in 64-bit mode. Test CpuID.
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0x9E);
}


void Assembler::emit_farith(int b1, int b2, int i) {
  ASSERT(is_uint8(b1) && is_uint8(b2));  // wrong opcode
  ASSERT(is_uint3(i));  // illegal stack offset
  emit(b1);
  emit(b2 + i);
}

// SSE 2 operations

void Assembler::movsd(const Operand& dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);  // double
  emit_optional_rex_32(src, dst);
  emit(0x0F);
  emit(0x11);  // store
  emit_sse_operand(src, dst);
}


void Assembler::movsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);  // double
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);  // load
  emit_sse_operand(dst, src);
}


void Assembler::movsd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);  // double
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x10);  // load
  emit_sse_operand(dst, src);
}


void Assembler::cvttss2si(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF3);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_operand(dst, src);
}


void Assembler::cvttsd2si(Register dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2C);
  emit_operand(dst, src);
}


void Assembler::cvtlsi2sd(XMMRegister dst, const Operand& src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtlsi2sd(XMMRegister dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::cvtqsi2sd(XMMRegister dst, Register src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_rex_64(dst, src);
  emit(0x0F);
  emit(0x2A);
  emit_sse_operand(dst, src);
}


void Assembler::addsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x58);
  emit_sse_operand(dst, src);
}


void Assembler::mulsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x59);
  emit_sse_operand(dst, src);
}


void Assembler::subsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5C);
  emit_sse_operand(dst, src);
}


void Assembler::divsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  last_pc_ = pc_;
  emit(0xF2);
  emit_optional_rex_32(dst, src);
  emit(0x0F);
  emit(0x5E);
  emit_sse_operand(dst, src);
}



void Assembler::emit_sse_operand(XMMRegister reg, const Operand& adr) {
  Register ireg = { reg.code() };
  emit_operand(ireg, adr);
}


void Assembler::emit_sse_operand(XMMRegister dst, XMMRegister src) {
  emit(0xC0 | (dst.low_bits() << 3) | src.low_bits());
}

void Assembler::emit_sse_operand(XMMRegister dst, Register src) {
  emit(0xC0 | (dst.low_bits() << 3) | src.low_bits());
}


// Relocation information implementations

void Assembler::RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data) {
  ASSERT(rmode != RelocInfo::NONE);
  // Don't record external references unless the heap will be serialized.
  if (rmode == RelocInfo::EXTERNAL_REFERENCE &&
      !Serializer::enabled() &&
      !FLAG_debug_code) {
    return;
  }
  RelocInfo rinfo(pc_, rmode, data);
  reloc_info_writer.Write(&rinfo);
}

void Assembler::RecordJSReturn() {
  WriteRecordedPositions();
  EnsureSpace ensure_space(this);
  RecordRelocInfo(RelocInfo::JS_RETURN);
}


void Assembler::RecordComment(const char* msg) {
  if (FLAG_debug_code) {
    EnsureSpace ensure_space(this);
    RecordRelocInfo(RelocInfo::COMMENT, reinterpret_cast<intptr_t>(msg));
  }
}


void Assembler::RecordPosition(int pos) {
  ASSERT(pos != RelocInfo::kNoPosition);
  ASSERT(pos >= 0);
  current_position_ = pos;
}


void Assembler::RecordStatementPosition(int pos) {
  ASSERT(pos != RelocInfo::kNoPosition);
  ASSERT(pos >= 0);
  current_statement_position_ = pos;
}


void Assembler::WriteRecordedPositions() {
  // Write the statement position if it is different from what was written last
  // time.
  if (current_statement_position_ != written_statement_position_) {
    EnsureSpace ensure_space(this);
    RecordRelocInfo(RelocInfo::STATEMENT_POSITION, current_statement_position_);
    written_statement_position_ = current_statement_position_;
  }

  // Write the position if it is different from what was written last time and
  // also different from the written statement position.
  if (current_position_ != written_position_ &&
      current_position_ != written_statement_position_) {
    EnsureSpace ensure_space(this);
    RecordRelocInfo(RelocInfo::POSITION, current_position_);
    written_position_ = current_position_;
  }
}


const int RelocInfo::kApplyMask = RelocInfo::kCodeTargetMask |
                                  1 << RelocInfo::INTERNAL_REFERENCE |
                                  1 << RelocInfo::JS_RETURN;

} }  // namespace v8::internal
