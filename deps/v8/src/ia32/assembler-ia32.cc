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

#include "src/ia32/assembler-ia32.h"

#include <cstring>

#if V8_TARGET_ARCH_IA32

#if V8_LIBC_MSVCRT
#include <intrin.h>  // _xgetbv()
#endif
#if V8_OS_MACOSX
#include <sys/sysctl.h>
#endif

#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/base/cpu.h"
#include "src/conversions-inl.h"
#include "src/deoptimizer.h"
#include "src/disassembler.h"
#include "src/macro-assembler.h"
#include "src/string-constants.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

Immediate Immediate::EmbeddedNumber(double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) return Immediate(Smi::FromInt(smi));
  Immediate result(0, RelocInfo::EMBEDDED_OBJECT);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(value);
  return result;
}

Immediate Immediate::EmbeddedStringConstant(const StringConstantBase* str) {
  Immediate result(0, RelocInfo::EMBEDDED_OBJECT);
  result.is_heap_object_request_ = true;
  result.value_.heap_object_request = HeapObjectRequest(str);
  return result;
}

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
  __asm__ volatile(".byte 0x0F, 0x01, 0xD0" : "=a"(eax), "=d"(edx) : "c"(xcr));
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
    FATAL("V8 failed to get kernel version");
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
      "SSE3=%d SSSE3=%d SSE4_1=%d AVX=%d FMA3=%d BMI1=%d BMI2=%d LZCNT=%d "
      "POPCNT=%d ATOM=%d\n",
      CpuFeatures::IsSupported(SSE3), CpuFeatures::IsSupported(SSSE3),
      CpuFeatures::IsSupported(SSE4_1), CpuFeatures::IsSupported(AVX),
      CpuFeatures::IsSupported(FMA3), CpuFeatures::IsSupported(BMI1),
      CpuFeatures::IsSupported(BMI2), CpuFeatures::IsSupported(LZCNT),
      CpuFeatures::IsSupported(POPCNT), CpuFeatures::IsSupported(ATOM));
}


// -----------------------------------------------------------------------------
// Implementation of Displacement

void Displacement::init(Label* L, Type type) {
  DCHECK(!L->is_bound());
  int next = 0;
  if (L->is_linked()) {
    next = L->pos();
    DCHECK_GT(next, 0);  // Displacements must be at positions > 0
  }
  // Ensure that we _never_ overflow the next field.
  DCHECK(NextField::is_valid(Assembler::kMaximalBufferSize));
  data_ = NextField::encode(next) | TypeField::encode(type);
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo

const int RelocInfo::kApplyMask =
    RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
    RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
    RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) |
    RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);

bool RelocInfo::IsCodedSpecially() {
  // The deserializer needs to know whether a pointer is specially coded.  Being
  // specially coded on IA32 means that it is a relative address, as used by
  // branch instructions.  These are also the ones that need changing when a
  // code object moves.
  return RelocInfo::ModeMask(rmode_) & kApplyMask;
}

bool RelocInfo::IsInConstantPool() {
  return false;
}

uint32_t RelocInfo::wasm_call_tag() const {
  DCHECK(rmode_ == WASM_CALL || rmode_ == WASM_STUB_CALL);
  return ReadUnalignedValue<uint32_t>(pc_);
}

// -----------------------------------------------------------------------------
// Implementation of Operand

Operand::Operand(Register base, int32_t disp, RelocInfo::Mode rmode) {
  // [base + disp/r]
  if (disp == 0 && RelocInfo::IsNone(rmode) && base != ebp) {
    // [base]
    set_modrm(0, base);
    if (base == esp) set_sib(times_1, esp, base);
  } else if (is_int8(disp) && RelocInfo::IsNone(rmode)) {
    // [base + disp8]
    set_modrm(1, base);
    if (base == esp) set_sib(times_1, esp, base);
    set_disp8(disp);
  } else {
    // [base + disp/r]
    set_modrm(2, base);
    if (base == esp) set_sib(times_1, esp, base);
    set_dispr(disp, rmode);
  }
}


Operand::Operand(Register base,
                 Register index,
                 ScaleFactor scale,
                 int32_t disp,
                 RelocInfo::Mode rmode) {
  DCHECK(index != esp);  // illegal addressing mode
  // [base + index*scale + disp/r]
  if (disp == 0 && RelocInfo::IsNone(rmode) && base != ebp) {
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
  DCHECK(index != esp);  // illegal addressing mode
  // [index*scale + disp/r]
  set_modrm(0, esp);
  set_sib(scale, index, ebp);
  set_dispr(disp, rmode);
}


bool Operand::is_reg_only() const {
  return (buf_[0] & 0xF8) == 0xC0;  // Addressing mode is register only.
}


Register Operand::reg() const {
  DCHECK(is_reg_only());
  return Register::from_code(buf_[0] & 0x07);
}

void Assembler::AllocateAndInstallRequestedHeapObjects(Isolate* isolate) {
  DCHECK_IMPLIES(isolate == nullptr, heap_object_requests_.empty());
  for (auto& request : heap_object_requests_) {
    Handle<HeapObject> object;
    switch (request.kind()) {
      case HeapObjectRequest::kHeapNumber:
        object =
            isolate->factory()->NewHeapNumber(request.heap_number(), TENURED);
        break;
      case HeapObjectRequest::kStringConstant: {
        const StringConstantBase* str = request.string();
        CHECK_NOT_NULL(str);
        object = str->AllocateStringConstant(isolate);
        break;
      }
    }
    Address pc = reinterpret_cast<Address>(buffer_start_) + request.offset();
    WriteUnalignedValue(pc, object);
  }
}

// -----------------------------------------------------------------------------
// Implementation of Assembler.

// Emit a single byte. Must always be inlined.
#define EMIT(x)                                 \
  *pc_++ = (x)

Assembler::Assembler(const AssemblerOptions& options,
                     std::unique_ptr<AssemblerBuffer> buffer)
    : AssemblerBase(options, std::move(buffer)) {
  reloc_info_writer.Reposition(buffer_start_ + buffer_->size(), pc_);
}

void Assembler::GetCode(Isolate* isolate, CodeDesc* desc,
                        SafepointTableBuilder* safepoint_table_builder,
                        int handler_table_offset) {
  const int code_comments_size = WriteCodeComments();

  // Finalize code (at this point overflow() may be true, but the gap ensures
  // that we are still not overlapping instructions and relocation info).
  DCHECK(pc_ <= reloc_info_writer.pos());  // No overlap.

  AllocateAndInstallRequestedHeapObjects(isolate);

  // Set up code descriptor.
  // TODO(jgruber): Reconsider how these offsets and sizes are maintained up to
  // this point to make CodeDesc initialization less fiddly.

  static constexpr int kConstantPoolSize = 0;
  const int instruction_size = pc_offset();
  const int code_comments_offset = instruction_size - code_comments_size;
  const int constant_pool_offset = code_comments_offset - kConstantPoolSize;
  const int handler_table_offset2 = (handler_table_offset == kNoHandlerTable)
                                        ? constant_pool_offset
                                        : handler_table_offset;
  const int safepoint_table_offset =
      (safepoint_table_builder == kNoSafepointTable)
          ? handler_table_offset2
          : safepoint_table_builder->GetCodeOffset();
  const int reloc_info_offset =
      static_cast<int>(reloc_info_writer.pos() - buffer_->start());
  CodeDesc::Initialize(desc, this, safepoint_table_offset,
                       handler_table_offset2, constant_pool_offset,
                       code_comments_offset, reloc_info_offset);
}

void Assembler::FinalizeJumpOptimizationInfo() {
  // Collection stage
  auto jump_opt = jump_optimization_info();
  if (jump_opt && jump_opt->is_collecting()) {
    auto& bitmap = jump_opt->farjmp_bitmap();
    int num = static_cast<int>(farjmp_positions_.size());
    if (num && bitmap.empty()) {
      bool can_opt = false;

      bitmap.resize((num + 31) / 32, 0);
      for (int i = 0; i < num; i++) {
        int disp_pos = farjmp_positions_[i];
        int disp = long_at(disp_pos);
        if (is_int8(disp)) {
          bitmap[i / 32] |= 1 << (i & 31);
          can_opt = true;
        }
      }
      if (can_opt) {
        jump_opt->set_optimizable();
      }
    }
  }
}

void Assembler::Align(int m) {
  DCHECK(base::bits::IsPowerOfTwo(m));
  int mask = m - 1;
  int addr = pc_offset();
  Nop((m - (addr & mask)) & mask);
}


bool Assembler::IsNop(Address addr) {
  byte* a = reinterpret_cast<byte*>(addr);
  while (*a == 0x66) a++;
  if (*a == 0x90) return true;
  if (a[0] == 0xF && a[1] == 0x1F) return true;
  return false;
}


void Assembler::Nop(int bytes) {
  EnsureSpace ensure_space(this);
  // Multi byte nops from http://support.amd.com/us/Processor_TechDocs/40546.pdf
  while (bytes > 0) {
    switch (bytes) {
      case 2:
        EMIT(0x66);
        V8_FALLTHROUGH;
      case 1:
        EMIT(0x90);
        return;
      case 3:
        EMIT(0xF);
        EMIT(0x1F);
        EMIT(0);
        return;
      case 4:
        EMIT(0xF);
        EMIT(0x1F);
        EMIT(0x40);
        EMIT(0);
        return;
      case 6:
        EMIT(0x66);
        V8_FALLTHROUGH;
      case 5:
        EMIT(0xF);
        EMIT(0x1F);
        EMIT(0x44);
        EMIT(0);
        EMIT(0);
        return;
      case 7:
        EMIT(0xF);
        EMIT(0x1F);
        EMIT(0x80);
        EMIT(0);
        EMIT(0);
        EMIT(0);
        EMIT(0);
        return;
      default:
      case 11:
        EMIT(0x66);
        bytes--;
        V8_FALLTHROUGH;
      case 10:
        EMIT(0x66);
        bytes--;
        V8_FALLTHROUGH;
      case 9:
        EMIT(0x66);
        bytes--;
        V8_FALLTHROUGH;
      case 8:
        EMIT(0xF);
        EMIT(0x1F);
        EMIT(0x84);
        EMIT(0);
        EMIT(0);
        EMIT(0);
        EMIT(0);
        EMIT(0);
        bytes -= 8;
    }
  }
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
    EMIT(0x6A);
    EMIT(x.immediate());
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

void Assembler::push(Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(esi, src);
}


void Assembler::pop(Register dst) {
  DCHECK_NOT_NULL(reloc_info_writer.last_pc());
  EnsureSpace ensure_space(this);
  EMIT(0x58 | dst.code());
}

void Assembler::pop(Operand dst) {
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

void Assembler::mov_b(Register dst, Operand src) {
  CHECK(dst.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x8A);
  emit_operand(dst, src);
}

void Assembler::mov_b(Operand dst, const Immediate& src) {
  EnsureSpace ensure_space(this);
  EMIT(0xC6);
  emit_operand(eax, dst);
  EMIT(static_cast<int8_t>(src.immediate()));
}

void Assembler::mov_b(Operand dst, Register src) {
  CHECK(src.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x88);
  emit_operand(src, dst);
}

void Assembler::mov_w(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x8B);
  emit_operand(dst, src);
}

void Assembler::mov_w(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x89);
  emit_operand(src, dst);
}

void Assembler::mov_w(Operand dst, const Immediate& src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0xC7);
  emit_operand(eax, dst);
  EMIT(static_cast<int8_t>(src.immediate() & 0xFF));
  EMIT(static_cast<int8_t>(src.immediate() >> 8));
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

void Assembler::mov(Register dst, Handle<HeapObject> handle) {
  EnsureSpace ensure_space(this);
  EMIT(0xB8 | dst.code());
  emit(handle);
}

void Assembler::mov(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x8B);
  emit_operand(dst, src);
}


void Assembler::mov(Register dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x89);
  EMIT(0xC0 | src.code() << 3 | dst.code());
}

void Assembler::mov(Operand dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  EMIT(0xC7);
  emit_operand(eax, dst);
  emit(x);
}

void Assembler::mov(Operand dst, Address src, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  EMIT(0xC7);
  emit_operand(eax, dst);
  emit(src, rmode);
}

void Assembler::mov(Operand dst, Handle<HeapObject> handle) {
  EnsureSpace ensure_space(this);
  EMIT(0xC7);
  emit_operand(eax, dst);
  emit(handle);
}

void Assembler::mov(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x89);
  emit_operand(src, dst);
}

void Assembler::movsx_b(Register dst, Operand src) {
  DCHECK_IMPLIES(src.is_reg_only(), src.reg().is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBE);
  emit_operand(dst, src);
}

void Assembler::movsx_w(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBF);
  emit_operand(dst, src);
}

void Assembler::movzx_b(Register dst, Operand src) {
  DCHECK_IMPLIES(src.is_reg_only(), src.reg().is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB6);
  emit_operand(dst, src);
}

void Assembler::movzx_w(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB7);
  emit_operand(dst, src);
}

void Assembler::movq(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x7E);
  emit_operand(dst, src);
}

void Assembler::cmov(Condition cc, Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  // Opcode: 0f 40 + cc /r.
  EMIT(0x0F);
  EMIT(0x40 + cc);
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
  if (src == eax || dst == eax) {  // Single-byte encoding.
    EMIT(0x90 | (src == eax ? dst.code() : src.code()));
  } else {
    EMIT(0x87);
    EMIT(0xC0 | src.code() << 3 | dst.code());
  }
}

void Assembler::xchg(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x87);
  emit_operand(dst, src);
}

void Assembler::xchg_b(Register reg, Operand op) {
  DCHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x86);
  emit_operand(reg, op);
}

void Assembler::xchg_w(Register reg, Operand op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x87);
  emit_operand(reg, op);
}

void Assembler::lock() {
  EnsureSpace ensure_space(this);
  EMIT(0xF0);
}

void Assembler::cmpxchg(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB1);
  emit_operand(src, dst);
}

void Assembler::cmpxchg_b(Operand dst, Register src) {
  DCHECK(src.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xB0);
  emit_operand(src, dst);
}

void Assembler::cmpxchg_w(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0xB1);
  emit_operand(src, dst);
}

void Assembler::cmpxchg8b(Operand dst) {
  EnsureSpace enure_space(this);
  EMIT(0x0F);
  EMIT(0xC7);
  emit_operand(ecx, dst);
}

void Assembler::lfence() {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAE);
  EMIT(0xE8);
}

void Assembler::pause() {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x90);
}

void Assembler::adc(Register dst, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(2, Operand(dst), Immediate(imm32));
}

void Assembler::adc(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x13);
  emit_operand(dst, src);
}

void Assembler::add(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x03);
  emit_operand(dst, src);
}

void Assembler::add(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x01);
  emit_operand(src, dst);
}

void Assembler::add(Operand dst, const Immediate& x) {
  DCHECK_NOT_NULL(reloc_info_writer.last_pc());
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

void Assembler::and_(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x23);
  emit_operand(dst, src);
}

void Assembler::and_(Operand dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(4, dst, x);
}

void Assembler::and_(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x21);
  emit_operand(src, dst);
}

void Assembler::cmpb(Operand op, Immediate imm8) {
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

void Assembler::cmpb(Operand op, Register reg) {
  CHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x38);
  emit_operand(reg, op);
}

void Assembler::cmpb(Register reg, Operand op) {
  CHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x3A);
  emit_operand(reg, op);
}

void Assembler::cmpw(Operand op, Immediate imm16) {
  DCHECK(imm16.is_int16() || imm16.is_uint16());
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x81);
  emit_operand(edi, op);
  emit_w(imm16);
}

void Assembler::cmpw(Register reg, Operand op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x3B);
  emit_operand(reg, op);
}

void Assembler::cmpw(Operand op, Register reg) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x39);
  emit_operand(reg, op);
}

void Assembler::cmp(Register reg, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(7, Operand(reg), Immediate(imm32));
}

void Assembler::cmp(Register reg, Handle<HeapObject> handle) {
  EnsureSpace ensure_space(this);
  emit_arith(7, Operand(reg), Immediate(handle));
}

void Assembler::cmp(Register reg, Operand op) {
  EnsureSpace ensure_space(this);
  EMIT(0x3B);
  emit_operand(reg, op);
}

void Assembler::cmp(Operand op, Register reg) {
  EnsureSpace ensure_space(this);
  EMIT(0x39);
  emit_operand(reg, op);
}

void Assembler::cmp(Operand op, const Immediate& imm) {
  EnsureSpace ensure_space(this);
  emit_arith(7, op, imm);
}

void Assembler::cmp(Operand op, Handle<HeapObject> handle) {
  EnsureSpace ensure_space(this);
  emit_arith(7, op, Immediate(handle));
}

void Assembler::cmpb_al(Operand op) {
  EnsureSpace ensure_space(this);
  EMIT(0x38);  // CMP r/m8, r8
  emit_operand(eax, op);  // eax has same code as register al.
}

void Assembler::cmpw_ax(Operand op) {
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

void Assembler::dec_b(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xFE);
  emit_operand(ecx, dst);
}


void Assembler::dec(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0x48 | dst.code());
}

void Assembler::dec(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(ecx, dst);
}


void Assembler::cdq() {
  EnsureSpace ensure_space(this);
  EMIT(0x99);
}

void Assembler::idiv(Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(edi, src);
}

void Assembler::div(Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(esi, src);
}


void Assembler::imul(Register reg) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  EMIT(0xE8 | reg.code());
}

void Assembler::imul(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAF);
  emit_operand(dst, src);
}


void Assembler::imul(Register dst, Register src, int32_t imm32) {
  imul(dst, Operand(src), imm32);
}

void Assembler::imul(Register dst, Operand src, int32_t imm32) {
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

void Assembler::inc(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(eax, dst);
}

void Assembler::lea(Register dst, Operand src) {
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

void Assembler::neg(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(ebx, dst);
}


void Assembler::not_(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  EMIT(0xD0 | dst.code());
}

void Assembler::not_(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xF7);
  emit_operand(edx, dst);
}


void Assembler::or_(Register dst, int32_t imm32) {
  EnsureSpace ensure_space(this);
  emit_arith(1, Operand(dst), Immediate(imm32));
}

void Assembler::or_(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0B);
  emit_operand(dst, src);
}

void Assembler::or_(Operand dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(1, dst, x);
}

void Assembler::or_(Operand dst, Register src) {
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

void Assembler::ror(Operand dst, uint8_t imm8) {
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

void Assembler::ror_cl(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(ecx, dst);
}

void Assembler::sar(Operand dst, uint8_t imm8) {
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

void Assembler::sar_cl(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(edi, dst);
}

void Assembler::sbb(Register dst, Operand src) {
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

void Assembler::shl(Operand dst, uint8_t imm8) {
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

void Assembler::shl_cl(Operand dst) {
  EnsureSpace ensure_space(this);
  EMIT(0xD3);
  emit_operand(esp, dst);
}

void Assembler::shr(Operand dst, uint8_t imm8) {
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

void Assembler::shr_cl(Operand dst) {
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

void Assembler::shrd_cl(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAD);
  emit_operand(src, dst);
}

void Assembler::sub(Operand dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(5, dst, x);
}

void Assembler::sub(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x2B);
  emit_operand(dst, src);
}

void Assembler::sub(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x29);
  emit_operand(src, dst);
}

void Assembler::sub_sp_32(uint32_t imm) {
  EnsureSpace ensure_space(this);
  EMIT(0x81);  // using a literal 32-bit immediate.
  static constexpr Register ireg = Register::from_code<5>();
  emit_operand(ireg, Operand(esp));
  emit(imm);
}

void Assembler::test(Register reg, const Immediate& imm) {
  if (imm.is_uint8()) {
    test_b(reg, imm);
    return;
  }

  EnsureSpace ensure_space(this);
  // This is not using emit_arith because test doesn't support
  // sign-extension of 8-bit operands.
  if (reg == eax) {
    EMIT(0xA9);
  } else {
    EMIT(0xF7);
    EMIT(0xC0 | reg.code());
  }
  emit(imm);
}

void Assembler::test(Register reg, Operand op) {
  EnsureSpace ensure_space(this);
  EMIT(0x85);
  emit_operand(reg, op);
}

void Assembler::test_b(Register reg, Operand op) {
  CHECK(reg.is_byte_register());
  EnsureSpace ensure_space(this);
  EMIT(0x84);
  emit_operand(reg, op);
}

void Assembler::test(Operand op, const Immediate& imm) {
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
  if (reg == eax) {
    EMIT(0xA8);
    emit_b(imm8);
  } else if (reg.is_byte_register()) {
    emit_arith_b(0xF6, 0xC0, reg, static_cast<uint8_t>(imm8.immediate()));
  } else {
    EMIT(0x66);
    EMIT(0xF7);
    EMIT(0xC0 | reg.code());
    emit_w(imm8);
  }
}

void Assembler::test_b(Operand op, Immediate imm8) {
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
  if (reg == eax) {
    EMIT(0xA9);
    emit_w(imm16);
  } else {
    EMIT(0x66);
    EMIT(0xF7);
    EMIT(0xC0 | reg.code());
    emit_w(imm16);
  }
}

void Assembler::test_w(Register reg, Operand op) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x85);
  emit_operand(reg, op);
}

void Assembler::test_w(Operand op, Immediate imm16) {
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

void Assembler::xor_(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x33);
  emit_operand(dst, src);
}

void Assembler::xor_(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x31);
  emit_operand(src, dst);
}

void Assembler::xor_(Operand dst, const Immediate& x) {
  EnsureSpace ensure_space(this);
  emit_arith(6, dst, x);
}

void Assembler::bswap(Register dst) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xC8 + dst.code());
}

void Assembler::bt(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xA3);
  emit_operand(src, dst);
}

void Assembler::bts(Operand dst, Register src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xAB);
  emit_operand(src, dst);
}

void Assembler::bsr(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xBD);
  emit_operand(dst, src);
}

void Assembler::bsf(Register dst, Operand src) {
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

void Assembler::print(const Label* L) {
  if (L->is_unused()) {
    PrintF("unused label\n");
  } else if (L->is_bound()) {
    PrintF("bound label to %d\n", L->pos());
  } else if (L->is_linked()) {
    Label l;
    l.link_to(L->pos());
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
      long_at_put(fixup_pos, reinterpret_cast<int>(buffer_start_ + pos));
      internal_reference_positions_.push_back(fixup_pos);
    } else if (disp.type() == Displacement::CODE_RELATIVE) {
      // Relative to Code heap object pointer.
      long_at_put(fixup_pos, pos + Code::kHeaderSize - kHeapObjectTag);
    } else {
      if (disp.type() == Displacement::UNCONDITIONAL_JUMP) {
        DCHECK_EQ(byte_at(fixup_pos - 1), 0xE9);  // jmp expected
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
    DCHECK_LE(offset_to_next, 0);
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

  // Optimization stage
  auto jump_opt = jump_optimization_info();
  if (jump_opt && jump_opt->is_optimizing()) {
    auto it = label_farjmp_maps_.find(L);
    if (it != label_farjmp_maps_.end()) {
      auto& pos_vector = it->second;
      for (auto fixup_pos : pos_vector) {
        int disp = pos - (fixup_pos + sizeof(int8_t));
        CHECK(is_int8(disp));
        set_byte_at(fixup_pos, disp);
      }
      label_farjmp_maps_.erase(it);
    }
  }
  L->bind_to(pos);
}


void Assembler::bind(Label* L) {
  EnsureSpace ensure_space(this);
  DCHECK(!L->is_bound());  // label can only be bound once
  bind_to(L, pc_offset());
}

void Assembler::record_farjmp_position(Label* L, int pos) {
  auto& pos_vector = label_farjmp_maps_[L];
  pos_vector.push_back(pos);
}

bool Assembler::is_optimizable_farjmp(int idx) {
  if (predictable_code_size()) return false;

  auto jump_opt = jump_optimization_info();
  CHECK(jump_opt->is_optimizing());

  auto& bitmap = jump_opt->farjmp_bitmap();
  CHECK(idx < static_cast<int>(bitmap.size() * 32));
  return !!(bitmap[idx / 32] & (1 << (idx & 31)));
}

void Assembler::call(Label* L) {
  EnsureSpace ensure_space(this);
  if (L->is_bound()) {
    const int long_size = 5;
    int offs = L->pos() - pc_offset();
    DCHECK_LE(offs, 0);
    // 1110 1000 #32-bit disp.
    EMIT(0xE8);
    emit(offs - long_size);
  } else {
    // 1110 1000 #32-bit disp.
    EMIT(0xE8);
    emit_disp(L, Displacement::OTHER);
  }
}

void Assembler::call(Address entry, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  EMIT(0xE8);
  if (RelocInfo::IsRuntimeEntry(rmode)) {
    emit(entry, rmode);
  } else {
    emit(entry - (reinterpret_cast<Address>(pc_) + sizeof(int32_t)), rmode);
  }
}

void Assembler::wasm_call(Address entry, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  EMIT(0xE8);
  emit(entry, rmode);
}

void Assembler::call(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xFF);
  emit_operand(edx, adr);
}

void Assembler::call(Handle<Code> code, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  EMIT(0xE8);
  emit(code, rmode);
}

void Assembler::jmp_rel(int offset) {
  EnsureSpace ensure_space(this);
  const int short_size = 2;
  const int long_size = 5;
  if (is_int8(offset - short_size)) {
    // 1110 1011 #8-bit disp.
    EMIT(0xEB);
    EMIT((offset - short_size) & 0xFF);
  } else {
    // 1110 1001 #32-bit disp.
    EMIT(0xE9);
    emit(offset - long_size);
  }
}

void Assembler::jmp(Label* L, Label::Distance distance) {
  if (L->is_bound()) {
    int offset = L->pos() - pc_offset();
    DCHECK_LE(offset, 0);  // backward jump.
    jmp_rel(offset);
    return;
  }

  EnsureSpace ensure_space(this);
  if (distance == Label::kNear) {
    EMIT(0xEB);
    emit_near_disp(L);
  } else {
    auto jump_opt = jump_optimization_info();
    if (V8_UNLIKELY(jump_opt)) {
      if (jump_opt->is_optimizing() && is_optimizable_farjmp(farjmp_num_++)) {
        EMIT(0xEB);
        record_farjmp_position(L, pc_offset());
        EMIT(0);
        return;
      }
      if (jump_opt->is_collecting()) {
        farjmp_positions_.push_back(pc_offset() + 1);
      }
    }
    // 1110 1001 #32-bit disp.
    EMIT(0xE9);
    emit_disp(L, Displacement::UNCONDITIONAL_JUMP);
  }
}

void Assembler::jmp(Address entry, RelocInfo::Mode rmode) {
  EnsureSpace ensure_space(this);
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  EMIT(0xE9);
  if (RelocInfo::IsRuntimeEntry(rmode)) {
    emit(entry, rmode);
  } else {
    emit(entry - (reinterpret_cast<Address>(pc_) + sizeof(int32_t)), rmode);
  }
}

void Assembler::jmp(Operand adr) {
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
    DCHECK_LE(offs, 0);
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
    auto jump_opt = jump_optimization_info();
    if (V8_UNLIKELY(jump_opt)) {
      if (jump_opt->is_optimizing() && is_optimizable_farjmp(farjmp_num_++)) {
        // 0111 tttn #8-bit disp
        EMIT(0x70 | cc);
        record_farjmp_position(L, pc_offset());
        EMIT(0);
        return;
      }
      if (jump_opt->is_collecting()) {
        farjmp_positions_.push_back(pc_offset() + 2);
      }
    }
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

void Assembler::fld_s(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(eax, adr);
}

void Assembler::fld_d(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(eax, adr);
}

void Assembler::fstp_s(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(ebx, adr);
}

void Assembler::fst_s(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xD9);
  emit_operand(edx, adr);
}

void Assembler::fstp_d(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(ebx, adr);
}

void Assembler::fst_d(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(edx, adr);
}

void Assembler::fild_s(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(eax, adr);
}

void Assembler::fild_d(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDF);
  emit_operand(ebp, adr);
}

void Assembler::fistp_s(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(ebx, adr);
}

void Assembler::fisttp_s(Operand adr) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(ecx, adr);
}

void Assembler::fisttp_d(Operand adr) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  EMIT(0xDD);
  emit_operand(ecx, adr);
}

void Assembler::fist_s(Operand adr) {
  EnsureSpace ensure_space(this);
  EMIT(0xDB);
  emit_operand(edx, adr);
}

void Assembler::fistp_d(Operand adr) {
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


void Assembler::fsub(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xE8, i);
}


void Assembler::fsub_i(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xD8, 0xE0, i);
}

void Assembler::fisub_s(Operand adr) {
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


void Assembler::fdiv(int i) {
  EnsureSpace ensure_space(this);
  emit_farith(0xDC, 0xF8, i);
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

void Assembler::cvttss2si(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  // The [src] might contain ebx's register code, but in
  // this case, it refers to xmm3, so it is OK to emit.
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x2C);
  emit_operand(dst, src);
}

void Assembler::cvttsd2si(Register dst, Operand src) {
  EnsureSpace ensure_space(this);
  // The [src] might contain ebx's register code, but in
  // this case, it refers to xmm3, so it is OK to emit.
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x2C);
  emit_operand(dst, src);
}


void Assembler::cvtsd2si(Register dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x2D);
  emit_sse_operand(dst, src);
}

void Assembler::cvtsi2ss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x2A);
  emit_sse_operand(dst, src);
}

void Assembler::cvtsi2sd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x2A);
  emit_sse_operand(dst, src);
}

void Assembler::cvtss2sd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x5A);
  emit_sse_operand(dst, src);
}

void Assembler::cvtsd2ss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x5A);
  emit_sse_operand(dst, src);
}

void Assembler::cvtdq2ps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x5B);
  emit_sse_operand(dst, src);
}

void Assembler::cvttps2dq(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x5B);
  emit_sse_operand(dst, src);
}

void Assembler::addsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x58);
  emit_sse_operand(dst, src);
}

void Assembler::mulsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x59);
  emit_sse_operand(dst, src);
}

void Assembler::subsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x5C);
  emit_sse_operand(dst, src);
}

void Assembler::divsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x5E);
  emit_sse_operand(dst, src);
}

void Assembler::xorpd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x57);
  emit_sse_operand(dst, src);
}

void Assembler::andps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x54);
  emit_sse_operand(dst, src);
}

void Assembler::orps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x56);
  emit_sse_operand(dst, src);
}

void Assembler::xorps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x57);
  emit_sse_operand(dst, src);
}

void Assembler::addps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x58);
  emit_sse_operand(dst, src);
}

void Assembler::subps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x5C);
  emit_sse_operand(dst, src);
}

void Assembler::mulps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x59);
  emit_sse_operand(dst, src);
}

void Assembler::divps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x5E);
  emit_sse_operand(dst, src);
}

void Assembler::rcpps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x53);
  emit_sse_operand(dst, src);
}

void Assembler::rsqrtps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x52);
  emit_sse_operand(dst, src);
}

void Assembler::minps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x5D);
  emit_sse_operand(dst, src);
}

void Assembler::maxps(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x5F);
  emit_sse_operand(dst, src);
}

void Assembler::cmpps(XMMRegister dst, Operand src, uint8_t cmp) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xC2);
  emit_sse_operand(dst, src);
  EMIT(cmp);
}

void Assembler::sqrtsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x51);
  emit_sse_operand(dst, src);
}

void Assembler::haddps(XMMRegister dst, Operand src) {
  DCHECK(IsEnabled(SSE3));
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x7C);
  emit_sse_operand(dst, src);
}

void Assembler::andpd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x54);
  emit_sse_operand(dst, src);
}

void Assembler::orpd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x56);
  emit_sse_operand(dst, src);
}

void Assembler::ucomisd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x2E);
  emit_sse_operand(dst, src);
}


void Assembler::roundss(XMMRegister dst, XMMRegister src, RoundingMode mode) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x0A);
  emit_sse_operand(dst, src);
  // Mask precision exeption.
  EMIT(static_cast<byte>(mode) | 0x8);
}


void Assembler::roundsd(XMMRegister dst, XMMRegister src, RoundingMode mode) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x0B);
  emit_sse_operand(dst, src);
  // Mask precision exeption.
  EMIT(static_cast<byte>(mode) | 0x8);
}


void Assembler::movmskpd(Register dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x50);
  emit_sse_operand(dst, src);
}


void Assembler::movmskps(Register dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x50);
  emit_sse_operand(dst, src);
}

void Assembler::maxsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x5F);
  emit_sse_operand(dst, src);
}

void Assembler::minsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x5D);
  emit_sse_operand(dst, src);
}


void Assembler::cmpltsd(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0xC2);
  emit_sse_operand(dst, src);
  EMIT(1);  // LT == 1
}


void Assembler::movaps(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x28);
  emit_sse_operand(dst, src);
}

void Assembler::movups(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x10);
  emit_sse_operand(dst, src);
}

void Assembler::movups(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x10);
  emit_sse_operand(dst, src);
}

void Assembler::movups(Operand dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x11);
  emit_sse_operand(src, dst);
}

void Assembler::shufps(XMMRegister dst, XMMRegister src, byte imm8) {
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0xC6);
  emit_sse_operand(dst, src);
  EMIT(imm8);
}

void Assembler::movdqa(Operand dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x7F);
  emit_sse_operand(src, dst);
}

void Assembler::movdqa(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x6F);
  emit_sse_operand(dst, src);
}

void Assembler::movdqu(Operand dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x7F);
  emit_sse_operand(src, dst);
}

void Assembler::movdqu(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x6F);
  emit_sse_operand(dst, src);
}

void Assembler::prefetch(Operand src, int level) {
  DCHECK(is_uint2(level));
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x18);
  // Emit hint number in Reg position of RegR/M.
  XMMRegister code = XMMRegister::from_code(level);
  emit_sse_operand(code, src);
}

void Assembler::movsd(Operand dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);  // double
  EMIT(0x0F);
  EMIT(0x11);  // store
  emit_sse_operand(src, dst);
}

void Assembler::movsd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);  // double
  EMIT(0x0F);
  EMIT(0x10);  // load
  emit_sse_operand(dst, src);
}

void Assembler::movss(Operand dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);  // float
  EMIT(0x0F);
  EMIT(0x11);  // store
  emit_sse_operand(src, dst);
}

void Assembler::movss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);  // float
  EMIT(0x0F);
  EMIT(0x10);  // load
  emit_sse_operand(dst, src);
}

void Assembler::movd(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x6E);
  emit_sse_operand(dst, src);
}

void Assembler::movd(Operand dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x7E);
  emit_sse_operand(src, dst);
}


void Assembler::extractps(Register dst, XMMRegister src, byte imm8) {
  DCHECK(IsEnabled(SSE4_1));
  DCHECK(is_uint8(imm8));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x17);
  emit_sse_operand(src, dst);
  EMIT(imm8);
}

void Assembler::psllw(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x71);
  emit_sse_operand(esi, reg);  // esi == 6
  EMIT(shift);
}

void Assembler::pslld(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x72);
  emit_sse_operand(esi, reg);  // esi == 6
  EMIT(shift);
}

void Assembler::psrlw(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x71);
  emit_sse_operand(edx, reg);  // edx == 2
  EMIT(shift);
}

void Assembler::psrld(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x72);
  emit_sse_operand(edx, reg);  // edx == 2
  EMIT(shift);
}

void Assembler::psraw(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x71);
  emit_sse_operand(esp, reg);  // esp == 4
  EMIT(shift);
}

void Assembler::psrad(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x72);
  emit_sse_operand(esp, reg);  // esp == 4
  EMIT(shift);
}

void Assembler::psllq(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x73);
  emit_sse_operand(esi, reg);  // esi == 6
  EMIT(shift);
}


void Assembler::psllq(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0xF3);
  emit_sse_operand(dst, src);
}

void Assembler::psrlq(XMMRegister reg, uint8_t shift) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x73);
  emit_sse_operand(edx, reg);  // edx == 2
  EMIT(shift);
}


void Assembler::psrlq(XMMRegister dst, XMMRegister src) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0xD3);
  emit_sse_operand(dst, src);
}

void Assembler::pshufhw(XMMRegister dst, Operand src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x70);
  emit_sse_operand(dst, src);
  EMIT(shuffle);
}

void Assembler::pshuflw(XMMRegister dst, Operand src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  EMIT(0xF2);
  EMIT(0x0F);
  EMIT(0x70);
  emit_sse_operand(dst, src);
  EMIT(shuffle);
}

void Assembler::pshufd(XMMRegister dst, Operand src, uint8_t shuffle) {
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x70);
  emit_sse_operand(dst, src);
  EMIT(shuffle);
}

void Assembler::pblendw(XMMRegister dst, Operand src, uint8_t mask) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x0E);
  emit_sse_operand(dst, src);
  EMIT(mask);
}

void Assembler::palignr(XMMRegister dst, Operand src, uint8_t mask) {
  DCHECK(IsEnabled(SSSE3));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x0F);
  emit_sse_operand(dst, src);
  EMIT(mask);
}

void Assembler::pextrb(Operand dst, XMMRegister src, uint8_t offset) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x14);
  emit_sse_operand(src, dst);
  EMIT(offset);
}

void Assembler::pextrw(Operand dst, XMMRegister src, uint8_t offset) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x15);
  emit_sse_operand(src, dst);
  EMIT(offset);
}

void Assembler::pextrd(Operand dst, XMMRegister src, uint8_t offset) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x16);
  emit_sse_operand(src, dst);
  EMIT(offset);
}

void Assembler::insertps(XMMRegister dst, Operand src, uint8_t offset) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x21);
  emit_sse_operand(dst, src);
  EMIT(offset);
}

void Assembler::pinsrb(XMMRegister dst, Operand src, uint8_t offset) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x20);
  emit_sse_operand(dst, src);
  EMIT(offset);
}

void Assembler::pinsrw(XMMRegister dst, Operand src, uint8_t offset) {
  DCHECK(is_uint8(offset));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0xC4);
  emit_sse_operand(dst, src);
  EMIT(offset);
}

void Assembler::pinsrd(XMMRegister dst, Operand src, uint8_t offset) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(0x66);
  EMIT(0x0F);
  EMIT(0x3A);
  EMIT(0x22);
  emit_sse_operand(dst, src);
  EMIT(offset);
}

void Assembler::addss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x58);
  emit_sse_operand(dst, src);
}

void Assembler::subss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x5C);
  emit_sse_operand(dst, src);
}

void Assembler::mulss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x59);
  emit_sse_operand(dst, src);
}

void Assembler::divss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x5E);
  emit_sse_operand(dst, src);
}

void Assembler::sqrtss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x51);
  emit_sse_operand(dst, src);
}

void Assembler::ucomiss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0x0F);
  EMIT(0x2E);
  emit_sse_operand(dst, src);
}

void Assembler::maxss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x5F);
  emit_sse_operand(dst, src);
}

void Assembler::minss(XMMRegister dst, Operand src) {
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0x5D);
  emit_sse_operand(dst, src);
}


// AVX instructions
void Assembler::vfmasd(byte op, XMMRegister dst, XMMRegister src1,
                       Operand src2) {
  DCHECK(IsEnabled(FMA3));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(src1, kLIG, k66, k0F38, kW1);
  EMIT(op);
  emit_sse_operand(dst, src2);
}

void Assembler::vfmass(byte op, XMMRegister dst, XMMRegister src1,
                       Operand src2) {
  DCHECK(IsEnabled(FMA3));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(src1, kLIG, k66, k0F38, kW0);
  EMIT(op);
  emit_sse_operand(dst, src2);
}

void Assembler::vsd(byte op, XMMRegister dst, XMMRegister src1, Operand src2) {
  vinstr(op, dst, src1, src2, kF2, k0F, kWIG);
}

void Assembler::vss(byte op, XMMRegister dst, XMMRegister src1, Operand src2) {
  vinstr(op, dst, src1, src2, kF3, k0F, kWIG);
}

void Assembler::vps(byte op, XMMRegister dst, XMMRegister src1, Operand src2) {
  vinstr(op, dst, src1, src2, kNone, k0F, kWIG);
}

void Assembler::vpd(byte op, XMMRegister dst, XMMRegister src1, Operand src2) {
  vinstr(op, dst, src1, src2, k66, k0F, kWIG);
}

void Assembler::vcmpps(XMMRegister dst, XMMRegister src1, Operand src2,
                       uint8_t cmp) {
  vps(0xC2, dst, src1, src2);
  EMIT(cmp);
}

void Assembler::vshufps(XMMRegister dst, XMMRegister src1, Operand src2,
                        byte imm8) {
  DCHECK(is_uint8(imm8));
  vps(0xC6, dst, src1, src2);
  EMIT(imm8);
}

void Assembler::vpsllw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
  XMMRegister iop = XMMRegister::from_code(6);
  vinstr(0x71, iop, dst, Operand(src), k66, k0F, kWIG);
  EMIT(imm8);
}

void Assembler::vpslld(XMMRegister dst, XMMRegister src, uint8_t imm8) {
  XMMRegister iop = XMMRegister::from_code(6);
  vinstr(0x72, iop, dst, Operand(src), k66, k0F, kWIG);
  EMIT(imm8);
}

void Assembler::vpsrlw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
  XMMRegister iop = XMMRegister::from_code(2);
  vinstr(0x71, iop, dst, Operand(src), k66, k0F, kWIG);
  EMIT(imm8);
}

void Assembler::vpsrld(XMMRegister dst, XMMRegister src, uint8_t imm8) {
  XMMRegister iop = XMMRegister::from_code(2);
  vinstr(0x72, iop, dst, Operand(src), k66, k0F, kWIG);
  EMIT(imm8);
}

void Assembler::vpsraw(XMMRegister dst, XMMRegister src, uint8_t imm8) {
  XMMRegister iop = XMMRegister::from_code(4);
  vinstr(0x71, iop, dst, Operand(src), k66, k0F, kWIG);
  EMIT(imm8);
}

void Assembler::vpsrad(XMMRegister dst, XMMRegister src, uint8_t imm8) {
  XMMRegister iop = XMMRegister::from_code(4);
  vinstr(0x72, iop, dst, Operand(src), k66, k0F, kWIG);
  EMIT(imm8);
}

void Assembler::vpshufhw(XMMRegister dst, Operand src, uint8_t shuffle) {
  vinstr(0x70, dst, xmm0, src, kF3, k0F, kWIG);
  EMIT(shuffle);
}

void Assembler::vpshuflw(XMMRegister dst, Operand src, uint8_t shuffle) {
  vinstr(0x70, dst, xmm0, src, kF2, k0F, kWIG);
  EMIT(shuffle);
}

void Assembler::vpshufd(XMMRegister dst, Operand src, uint8_t shuffle) {
  vinstr(0x70, dst, xmm0, src, k66, k0F, kWIG);
  EMIT(shuffle);
}

void Assembler::vpblendw(XMMRegister dst, XMMRegister src1, Operand src2,
                         uint8_t mask) {
  vinstr(0x0E, dst, src1, src2, k66, k0F3A, kWIG);
  EMIT(mask);
}

void Assembler::vpalignr(XMMRegister dst, XMMRegister src1, Operand src2,
                         uint8_t mask) {
  vinstr(0x0F, dst, src1, src2, k66, k0F3A, kWIG);
  EMIT(mask);
}

void Assembler::vpextrb(Operand dst, XMMRegister src, uint8_t offset) {
  vinstr(0x14, src, xmm0, dst, k66, k0F3A, kWIG);
  EMIT(offset);
}

void Assembler::vpextrw(Operand dst, XMMRegister src, uint8_t offset) {
  vinstr(0x15, src, xmm0, dst, k66, k0F3A, kWIG);
  EMIT(offset);
}

void Assembler::vpextrd(Operand dst, XMMRegister src, uint8_t offset) {
  vinstr(0x16, src, xmm0, dst, k66, k0F3A, kWIG);
  EMIT(offset);
}

void Assembler::vinsertps(XMMRegister dst, XMMRegister src1, Operand src2,
                          uint8_t offset) {
  vinstr(0x21, dst, src1, src2, k66, k0F3A, kWIG);
  EMIT(offset);
}

void Assembler::vpinsrb(XMMRegister dst, XMMRegister src1, Operand src2,
                        uint8_t offset) {
  vinstr(0x20, dst, src1, src2, k66, k0F3A, kWIG);
  EMIT(offset);
}

void Assembler::vpinsrw(XMMRegister dst, XMMRegister src1, Operand src2,
                        uint8_t offset) {
  vinstr(0xC4, dst, src1, src2, k66, k0F, kWIG);
  EMIT(offset);
}

void Assembler::vpinsrd(XMMRegister dst, XMMRegister src1, Operand src2,
                        uint8_t offset) {
  vinstr(0x22, dst, src1, src2, k66, k0F3A, kWIG);
  EMIT(offset);
}

void Assembler::bmi1(byte op, Register reg, Register vreg, Operand rm) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(vreg, kLZ, kNone, k0F38, kW0);
  EMIT(op);
  emit_operand(reg, rm);
}

void Assembler::tzcnt(Register dst, Operand src) {
  DCHECK(IsEnabled(BMI1));
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0xBC);
  emit_operand(dst, src);
}

void Assembler::lzcnt(Register dst, Operand src) {
  DCHECK(IsEnabled(LZCNT));
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0xBD);
  emit_operand(dst, src);
}

void Assembler::popcnt(Register dst, Operand src) {
  DCHECK(IsEnabled(POPCNT));
  EnsureSpace ensure_space(this);
  EMIT(0xF3);
  EMIT(0x0F);
  EMIT(0xB8);
  emit_operand(dst, src);
}

void Assembler::bmi2(SIMDPrefix pp, byte op, Register reg, Register vreg,
                     Operand rm) {
  DCHECK(IsEnabled(BMI2));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(vreg, kLZ, pp, k0F38, kW0);
  EMIT(op);
  emit_operand(reg, rm);
}

void Assembler::rorx(Register dst, Operand src, byte imm8) {
  DCHECK(IsEnabled(BMI2));
  DCHECK(is_uint8(imm8));
  Register vreg = Register::from_code<0>();  // VEX.vvvv unused
  EnsureSpace ensure_space(this);
  emit_vex_prefix(vreg, kLZ, kF2, k0F3A, kW0);
  EMIT(0xF0);
  emit_operand(dst, src);
  EMIT(imm8);
}

void Assembler::sse2_instr(XMMRegister dst, Operand src, byte prefix,
                           byte escape, byte opcode) {
  EnsureSpace ensure_space(this);
  EMIT(prefix);
  EMIT(escape);
  EMIT(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::ssse3_instr(XMMRegister dst, Operand src, byte prefix,
                            byte escape1, byte escape2, byte opcode) {
  DCHECK(IsEnabled(SSSE3));
  EnsureSpace ensure_space(this);
  EMIT(prefix);
  EMIT(escape1);
  EMIT(escape2);
  EMIT(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::sse4_instr(XMMRegister dst, Operand src, byte prefix,
                           byte escape1, byte escape2, byte opcode) {
  DCHECK(IsEnabled(SSE4_1));
  EnsureSpace ensure_space(this);
  EMIT(prefix);
  EMIT(escape1);
  EMIT(escape2);
  EMIT(opcode);
  emit_sse_operand(dst, src);
}

void Assembler::vinstr(byte op, XMMRegister dst, XMMRegister src1, Operand src2,
                       SIMDPrefix pp, LeadingOpcode m, VexW w) {
  DCHECK(IsEnabled(AVX));
  EnsureSpace ensure_space(this);
  emit_vex_prefix(src1, kL128, pp, m, w);
  EMIT(op);
  emit_sse_operand(dst, src2);
}

void Assembler::emit_sse_operand(XMMRegister reg, Operand adr) {
  Register ireg = Register::from_code(reg.code());
  emit_operand(ireg, adr);
}


void Assembler::emit_sse_operand(XMMRegister dst, XMMRegister src) {
  EMIT(0xC0 | dst.code() << 3 | src.code());
}


void Assembler::emit_sse_operand(Register dst, XMMRegister src) {
  EMIT(0xC0 | dst.code() << 3 | src.code());
}


void Assembler::emit_sse_operand(XMMRegister dst, Register src) {
  EMIT(0xC0 | (dst.code() << 3) | src.code());
}


void Assembler::emit_vex_prefix(XMMRegister vreg, VectorLength l, SIMDPrefix pp,
                                LeadingOpcode mm, VexW w) {
  if (mm != k0F || w != kW0) {
    EMIT(0xC4);
    // Change RXB from "110" to "111" to align with gdb disassembler.
    EMIT(0xE0 | mm);
    EMIT(w | ((~vreg.code() & 0xF) << 3) | l | pp);
  } else {
    EMIT(0xC5);
    EMIT(((~vreg.code()) << 3) | l | pp);
  }
}


void Assembler::emit_vex_prefix(Register vreg, VectorLength l, SIMDPrefix pp,
                                LeadingOpcode mm, VexW w) {
  XMMRegister ivreg = XMMRegister::from_code(vreg.code());
  emit_vex_prefix(ivreg, l, pp, mm, w);
}


void Assembler::GrowBuffer() {
  DCHECK(buffer_overflow());
  DCHECK_EQ(buffer_start_, buffer_->start());

  // Compute new buffer size.
  int old_size = buffer_->size();
  int new_size = 2 * old_size;

  // Some internal data structures overflow for very large buffers,
  // they must ensure that kMaximalBufferSize is not too large.
  if (new_size > kMaximalBufferSize) {
    V8::FatalProcessOutOfMemory(nullptr, "Assembler::GrowBuffer");
  }

  // Set up new buffer.
  std::unique_ptr<AssemblerBuffer> new_buffer = buffer_->Grow(new_size);
  DCHECK_EQ(new_size, new_buffer->size());
  byte* new_start = new_buffer->start();

  // Copy the data.
  intptr_t pc_delta = new_start - buffer_start_;
  intptr_t rc_delta = (new_start + new_size) - (buffer_start_ + old_size);
  size_t reloc_size = (buffer_start_ + old_size) - reloc_info_writer.pos();
  MemMove(new_start, buffer_start_, pc_offset());
  MemMove(rc_delta + reloc_info_writer.pos(), reloc_info_writer.pos(),
          reloc_size);

  // Switch buffers.
  buffer_ = std::move(new_buffer);
  buffer_start_ = new_start;
  pc_ += pc_delta;
  reloc_info_writer.Reposition(reloc_info_writer.pos() + rc_delta,
                               reloc_info_writer.last_pc() + pc_delta);

  // Relocate internal references.
  for (auto pos : internal_reference_positions_) {
    Address p = reinterpret_cast<Address>(buffer_start_ + pos);
    WriteUnalignedValue(p, ReadUnalignedValue<int>(p) + pc_delta);
  }

  // Relocate pc-relative references.
  int mode_mask = RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET);
  DCHECK_EQ(mode_mask, RelocInfo::kApplyMask & mode_mask);
  Vector<byte> instructions{buffer_start_, static_cast<size_t>(pc_offset())};
  Vector<const byte> reloc_info{reloc_info_writer.pos(), reloc_size};
  for (RelocIterator it(instructions, reloc_info, 0, mode_mask); !it.done();
       it.next()) {
    it.rinfo()->apply(pc_delta);
  }

  DCHECK(!buffer_overflow());
}


void Assembler::emit_arith_b(int op1, int op2, Register dst, int imm8) {
  DCHECK(is_uint8(op1) && is_uint8(op2));  // wrong opcode
  DCHECK(is_uint8(imm8));
  DCHECK_EQ(op1 & 0x01, 0);  // should be 8bit operation
  EMIT(op1);
  EMIT(op2 | dst.code());
  EMIT(imm8);
}


void Assembler::emit_arith(int sel, Operand dst, const Immediate& x) {
  DCHECK((0 <= sel) && (sel <= 7));
  Register ireg = Register::from_code(sel);
  if (x.is_int8()) {
    EMIT(0x83);  // using a sign-extended 8-bit immediate.
    emit_operand(ireg, dst);
    EMIT(x.immediate() & 0xFF);
  } else if (dst.is_reg(eax)) {
    EMIT((sel << 3) | 0x05);  // short form if the destination is eax.
    emit(x);
  } else {
    EMIT(0x81);  // using a literal 32-bit immediate.
    emit_operand(ireg, dst);
    emit(x);
  }
}

void Assembler::emit_operand(Register reg, Operand adr) {
  emit_operand(reg.code(), adr);
}

void Assembler::emit_operand(XMMRegister reg, Operand adr) {
  Register ireg = Register::from_code(reg.code());
  emit_operand(ireg, adr);
}

void Assembler::emit_operand(int code, Operand adr) {
  // Isolate-independent code may not embed relocatable addresses.
  DCHECK(!options().isolate_independent_code ||
         adr.rmode_ != RelocInfo::CODE_TARGET);
  DCHECK(!options().isolate_independent_code ||
         adr.rmode_ != RelocInfo::EMBEDDED_OBJECT);
  DCHECK(!options().isolate_independent_code ||
         adr.rmode_ != RelocInfo::EXTERNAL_REFERENCE);

  const unsigned length = adr.len_;
  DCHECK_GT(length, 0);

  // Emit updated ModRM byte containing the given register.
  pc_[0] = (adr.buf_[0] & ~0x38) | (code << 3);

  // Emit the rest of the encoded operand.
  for (unsigned i = 1; i < length; i++) pc_[i] = adr.buf_[i];
  pc_ += length;

  // Emit relocation information if necessary.
  if (length >= sizeof(int32_t) && !RelocInfo::IsNone(adr.rmode_)) {
    pc_ -= sizeof(int32_t);  // pc_ must be *at* disp32
    RecordRelocInfo(adr.rmode_);
    if (adr.rmode_ == RelocInfo::INTERNAL_REFERENCE) {  // Fixup for labels
      emit_label(ReadUnalignedValue<Label*>(reinterpret_cast<Address>(pc_)));
    } else {
      pc_ += sizeof(int32_t);
    }
  }
}


void Assembler::emit_label(Label* label) {
  if (label->is_bound()) {
    internal_reference_positions_.push_back(pc_offset());
    emit(reinterpret_cast<uint32_t>(buffer_start_ + label->pos()));
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
  if (!ShouldRecordRelocInfo(rmode)) return;
  RelocInfo rinfo(reinterpret_cast<Address>(pc_), rmode, data, Code());
  reloc_info_writer.Write(&rinfo);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
