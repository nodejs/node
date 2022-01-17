// Copyright 2011 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "src/base/vector.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/macro-assembler.h"
#include "src/debug/debug.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/execution/frames-inl.h"
#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

#define __ assm.

TEST(DisasmX64) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[8192];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));
  // Some instructions are tested in DisasmX64CheckOutput.

  // Calls

  Label L1, L2;
  __ bind(&L1);
  __ nop();
  __ call(&L1);
  __ call(&L2);
  __ nop();
  __ bind(&L2);
  __ call(rcx);
  __ nop();
  Handle<Code> ic = BUILTIN_CODE(isolate, ArrayFrom);
  __ call(ic, RelocInfo::CODE_TARGET);
  __ nop();

  __ jmp(&L1);
  __ jmp(Operand(rbx, rcx, times_4, 10000));
  __ jmp(ic, RelocInfo::CODE_TARGET);
  __ nop();


  Label Ljcc;
  __ nop();
  // long jumps
  __ j(overflow, &Ljcc);
  __ j(no_overflow, &Ljcc);
  __ j(below, &Ljcc);
  __ j(above_equal, &Ljcc);
  __ j(equal, &Ljcc);
  __ j(not_equal, &Ljcc);
  __ j(below_equal, &Ljcc);
  __ j(above, &Ljcc);
  __ j(sign, &Ljcc);
  __ j(not_sign, &Ljcc);
  __ j(parity_even, &Ljcc);
  __ j(parity_odd, &Ljcc);
  __ j(less, &Ljcc);
  __ j(greater_equal, &Ljcc);
  __ j(less_equal, &Ljcc);
  __ j(greater, &Ljcc);
  __ nop();
  __ bind(&Ljcc);
  // short jumps
  __ j(overflow, &Ljcc);
  __ j(no_overflow, &Ljcc);
  __ j(below, &Ljcc);
  __ j(above_equal, &Ljcc);
  __ j(equal, &Ljcc);
  __ j(not_equal, &Ljcc);
  __ j(below_equal, &Ljcc);
  __ j(above, &Ljcc);
  __ j(sign, &Ljcc);
  __ j(not_sign, &Ljcc);
  __ j(parity_even, &Ljcc);
  __ j(parity_odd, &Ljcc);
  __ j(less, &Ljcc);
  __ j(greater_equal, &Ljcc);
  __ j(less_equal, &Ljcc);
  __ j(greater, &Ljcc);

#define EMIT_SSE34_INSTR(instruction, notUsed1, notUsed2, notUsed3, notUsed4) \
  __ instruction(xmm5, xmm1);                                                 \
  __ instruction(xmm5, Operand(rdx, 4));

#define EMIT_SSE34_IMM_INSTR(instruction, notUsed1, notUsed2, notUsed3, \
                             notUsed4)                                  \
  __ instruction(rbx, xmm15, 0);                                        \
  __ instruction(Operand(rax, 10), xmm0, 1);

  {
    if (CpuFeatures::IsSupported(SSE4_1)) {
      CpuFeatureScope scope(&assm, SSE4_1);
      __ insertps(xmm5, xmm1, 123);
      __ pinsrw(xmm2, rcx, 1);
      __ pextrq(r12, xmm0, 1);
      __ pinsrd(xmm9, r9, 0);
      __ pinsrd(xmm5, Operand(rax, 4), 1);
      __ pinsrq(xmm9, r9, 0);
      __ pinsrq(xmm5, Operand(rax, 4), 1);
      __ pblendw(xmm5, xmm1, 1);
      __ pblendw(xmm9, Operand(rax, 4), 1);

      __ cmpps(xmm5, xmm1, 1);
      __ cmpps(xmm5, Operand(rbx, rcx, times_4, 10000), 1);
      __ cmpeqps(xmm5, xmm1);
      __ cmpeqps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpltps(xmm5, xmm1);
      __ cmpltps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpleps(xmm5, xmm1);
      __ cmpleps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpunordps(xmm5, xmm1);
      __ cmpunordps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpneqps(xmm5, xmm1);
      __ cmpneqps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpnltps(xmm5, xmm1);
      __ cmpnltps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpnleps(xmm5, xmm1);
      __ cmpnleps(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmppd(xmm5, xmm1, 1);
      __ cmppd(xmm5, Operand(rbx, rcx, times_4, 10000), 1);
      __ cmpeqpd(xmm5, xmm1);
      __ cmpeqpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpltpd(xmm5, xmm1);
      __ cmpltpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmplepd(xmm5, xmm1);
      __ cmplepd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpunordpd(xmm5, xmm1);
      __ cmpunordpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpneqpd(xmm5, xmm1);
      __ cmpneqpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpnltpd(xmm5, xmm1);
      __ cmpnltpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpnlepd(xmm5, xmm1);
      __ cmpnlepd(xmm5, Operand(rbx, rcx, times_4, 10000));

      __ movups(xmm5, xmm1);
      __ movups(xmm5, Operand(rdx, 4));
      __ movups(Operand(rdx, 4), xmm5);
      __ pmulld(xmm5, xmm1);
      __ pmulld(xmm5, Operand(rdx, 4));
      __ pmullw(xmm5, xmm1);
      __ pmullw(xmm5, Operand(rdx, 4));
      __ pmuludq(xmm5, xmm1);
      __ pmuludq(xmm5, Operand(rdx, 4));
      __ psrldq(xmm5, 123);
      __ pshufd(xmm5, xmm1, 3);
      __ cvtps2dq(xmm5, xmm1);
      __ cvtps2dq(xmm5, Operand(rdx, 4));
      __ cvtdq2ps(xmm5, xmm1);
      __ cvtdq2ps(xmm5, Operand(rdx, 4));

      __ pblendvb(xmm5, xmm1);
      __ blendvps(xmm5, xmm1);
      __ blendvps(xmm5, Operand(rdx, 4));
      __ blendvpd(xmm5, xmm1);
      __ blendvpd(xmm5, Operand(rdx, 4));

      __ roundps(xmm8, xmm3, kRoundUp);
      __ roundpd(xmm8, xmm3, kRoundToNearest);
      __ roundss(xmm8, xmm3, kRoundDown);
      __ roundsd(xmm8, xmm3, kRoundDown);

      SSE4_INSTRUCTION_LIST(EMIT_SSE34_INSTR)
      SSE4_UNOP_INSTRUCTION_LIST(EMIT_SSE34_INSTR)
      SSE4_EXTRACT_INSTRUCTION_LIST(EMIT_SSE34_IMM_INSTR)
    }
  }

  {
    if (CpuFeatures::IsSupported(SSE4_2)) {
      CpuFeatureScope scope(&assm, SSE4_2);

      SSE4_2_INSTRUCTION_LIST(EMIT_SSE34_INSTR)
    }
  }
#undef EMIT_SSE34_INSTR
#undef EMIT_SSE34_IMM_INSTR

  // AVX instruction
  {
    if (CpuFeatures::IsSupported(AVX)) {
      CpuFeatureScope scope(&assm, AVX);
      __ vmovss(xmm6, xmm14, xmm2);
      __ vmovss(xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovss(Operand(rbx, rcx, times_4, 10000), xmm0);

      __ vaddss(xmm0, xmm1, xmm2);
      __ vaddss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vmulss(xmm0, xmm1, xmm2);
      __ vmulss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vsubss(xmm0, xmm1, xmm2);
      __ vsubss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vdivss(xmm0, xmm1, xmm2);
      __ vdivss(xmm0, xmm1, Operand(rbx, rcx, times_2, 10000));
      __ vminss(xmm8, xmm1, xmm2);
      __ vminss(xmm9, xmm1, Operand(rbx, rcx, times_8, 10000));
      __ vmaxss(xmm8, xmm1, xmm2);
      __ vmaxss(xmm9, xmm1, Operand(rbx, rcx, times_1, 10000));
      __ vsqrtss(xmm8, xmm1, xmm2);
      __ vsqrtss(xmm9, xmm1, Operand(rbx, rcx, times_1, 10000));
      __ vmovss(xmm9, Operand(r11, rcx, times_8, -10000));
      __ vmovss(Operand(rbx, r9, times_4, 10000), xmm1);
      __ vucomiss(xmm9, xmm1);
      __ vucomiss(xmm8, Operand(rbx, rdx, times_2, 10981));

      __ vmovd(xmm5, rdi);
      __ vmovd(xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovd(r9, xmm6);
      __ vmovq(xmm5, rdi);
      __ vmovq(xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovq(r9, xmm6);

      __ vmovsd(xmm6, xmm14, xmm2);
      __ vmovsd(xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovsd(Operand(rbx, rcx, times_4, 10000), xmm0);

      __ vmovdqa(xmm4, xmm5);
      __ vmovdqa(xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vmovdqa(ymm4, ymm5);
      __ vmovdqa(xmm4, Operand(rbx, rcx, times_4, 10000));

      __ vmovdqu(xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovdqu(Operand(rbx, rcx, times_4, 10000), xmm0);
      __ vmovdqu(xmm4, xmm5);
      __ vmovdqu(ymm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovdqu(Operand(rbx, rcx, times_4, 10000), ymm0);
      __ vmovdqu(ymm4, ymm5);

      __ vmovhlps(xmm1, xmm3, xmm5);
      __ vmovlps(xmm8, xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovlps(Operand(rbx, rcx, times_4, 10000), xmm9);
      __ vmovlhps(xmm1, xmm3, xmm5);
      __ vmovhps(xmm8, xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovhps(Operand(rbx, rcx, times_4, 10000), xmm12);

      __ vroundps(xmm9, xmm2, kRoundUp);
      __ vroundpd(xmm9, xmm2, kRoundToNearest);
      __ vroundss(xmm9, xmm1, xmm2, kRoundDown);
      __ vroundsd(xmm8, xmm3, xmm0, kRoundDown);
      __ vucomisd(xmm9, xmm1);
      __ vucomisd(xmm8, Operand(rbx, rdx, times_2, 10981));

      __ vcvtdq2pd(xmm9, xmm11);
      __ vcvtss2sd(xmm4, xmm9, xmm11);
      __ vcvtss2sd(xmm4, xmm9, Operand(rbx, rcx, times_1, 10000));
      __ vcvttps2dq(xmm4, xmm11);
      __ vcvtlsi2sd(xmm5, xmm9, rcx);
      __ vcvtlsi2sd(xmm9, xmm3, Operand(rbx, r9, times_4, 10000));
      __ vcvtqsi2sd(xmm5, xmm9, r11);
      __ vcvttsd2si(r9, xmm6);
      __ vcvttsd2si(rax, Operand(rbx, r9, times_4, 10000));
      __ vcvttsd2siq(rdi, xmm9);
      __ vcvttsd2siq(r8, Operand(r9, rbx, times_4, 10000));
      __ vcvtsd2si(rdi, xmm9);

      __ vmovaps(xmm10, xmm11);
      __ vmovaps(xmm0, Operand(rbx, rcx, times_4, 10000));
      __ vmovapd(xmm7, xmm0);
      __ vmovupd(xmm0, Operand(rbx, rcx, times_4, 10000));
      __ vmovupd(Operand(rbx, rcx, times_4, 10000), xmm0);
      __ vmovmskpd(r9, xmm4);
      __ vpmovmskb(r10, xmm9);

      __ vmovups(xmm5, xmm1);
      __ vmovups(xmm5, Operand(rdx, 4));
      __ vmovups(Operand(rdx, 4), xmm5);

      __ vandps(xmm0, xmm9, xmm2);
      __ vandps(xmm9, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vandnps(xmm0, xmm9, xmm2);
      __ vandnps(xmm9, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vxorps(xmm0, xmm1, xmm9);
      __ vxorps(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vhaddps(xmm0, xmm1, xmm9);
      __ vhaddps(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vhaddps(ymm0, ymm1, ymm2);
      __ vhaddps(ymm0, ymm1, Operand(rbx, rcx, times_4, 10000));

      __ vpcmpeqd(xmm0, xmm15, xmm5);
      __ vpcmpeqd(xmm15, xmm0, Operand(rbx, rcx, times_4, 10000));

      __ vcmpps(xmm5, xmm4, xmm1, 1);
      __ vcmpps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000), 1);
      __ vcmpeqps(xmm5, xmm4, xmm1);
      __ vcmpeqps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpltps(xmm5, xmm4, xmm1);
      __ vcmpltps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpleps(xmm5, xmm4, xmm1);
      __ vcmpleps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpunordps(xmm5, xmm4, xmm1);
      __ vcmpunordps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpneqps(xmm5, xmm4, xmm1);
      __ vcmpneqps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnltps(xmm5, xmm4, xmm1);
      __ vcmpnltps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnleps(xmm5, xmm4, xmm1);
      __ vcmpnleps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpgeps(xmm5, xmm4, xmm1);
      __ vcmpgeps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmppd(xmm5, xmm4, xmm1, 1);
      __ vcmppd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000), 1);
      __ vcmpeqpd(xmm5, xmm4, xmm1);
      __ vcmpeqpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpltpd(xmm5, xmm4, xmm1);
      __ vcmpltpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmplepd(xmm5, xmm4, xmm1);
      __ vcmplepd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpunordpd(xmm5, xmm4, xmm1);
      __ vcmpunordpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpneqpd(xmm5, xmm4, xmm1);
      __ vcmpneqpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnltpd(xmm5, xmm4, xmm1);
      __ vcmpnltpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnlepd(xmm5, xmm4, xmm1);
      __ vcmpnlepd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));

#define EMIT_SSE_UNOP_AVXINSTR(instruction, notUsed1, notUsed2) \
  __ v##instruction(xmm10, xmm1);                               \
  __ v##instruction(xmm10, Operand(rbx, rcx, times_4, 10000));  \
  __ v##instruction(ymm10, ymm1);                               \
  __ v##instruction(ymm10, Operand(rbx, rcx, times_4, 10000));

      SSE_UNOP_INSTRUCTION_LIST(EMIT_SSE_UNOP_AVXINSTR)
#undef EMIT_SSE_UNOP_AVXINSTR

#define EMIT_SSE_BINOP_AVXINSTR(instruction, notUsed1, notUsed2)     \
  __ v##instruction(xmm10, xmm5, xmm1);                              \
  __ v##instruction(xmm10, xmm5, Operand(rbx, rcx, times_4, 10000)); \
  __ v##instruction(ymm10, ymm5, ymm1);                              \
  __ v##instruction(ymm10, ymm5, Operand(rbx, rcx, times_4, 10000));

      SSE_BINOP_INSTRUCTION_LIST(EMIT_SSE_BINOP_AVXINSTR)
#undef EMIT_SSE_BINOP_AVXINSTR

#define EMIT_SSE2_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3) \
  __ v##instruction(xmm10, xmm5, xmm1);                               \
  __ v##instruction(xmm10, xmm5, Operand(rdx, 4));

      SSE2_INSTRUCTION_LIST(EMIT_SSE2_AVXINSTR)
#undef EMIT_SSE2_AVXINSTR

#define EMIT_SSE2_UNOP_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3) \
  __ v##instruction(xmm10, xmm1);                                          \
  __ v##instruction(xmm10, Operand(rdx, 4));

      SSE2_UNOP_INSTRUCTION_LIST(EMIT_SSE2_UNOP_AVXINSTR)
#undef EMIT_SSE2_AVXINSTR

#define EMIT_SSE2_SD_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3) \
  __ v##instruction(xmm10, xmm5, xmm1);                                  \
  __ v##instruction(xmm10, xmm5, Operand(rbx, rcx, times_4, 10000));
      SSE2_INSTRUCTION_LIST_SD(EMIT_SSE2_SD_AVXINSTR)
#undef EMIT_SSE2_SD_AVXINSTR

#define EMIT_SSE34_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3, \
                            notUsed4)                                  \
  __ v##instruction(xmm10, xmm5, xmm1);                                \
  __ v##instruction(xmm10, xmm5, Operand(rdx, 4));

      SSSE3_INSTRUCTION_LIST(EMIT_SSE34_AVXINSTR)
      SSE4_INSTRUCTION_LIST(EMIT_SSE34_AVXINSTR)
      SSE4_2_INSTRUCTION_LIST(EMIT_SSE34_AVXINSTR)
#undef EMIT_SSE34_AVXINSTR

#define EMIT_SSSE3_UNOP_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3, \
                                 notUsed4)                                  \
  __ v##instruction(xmm9, xmm3);                                            \
  __ v##instruction(xmm9, Operand(rdx, 5));
      SSSE3_UNOP_INSTRUCTION_LIST(EMIT_SSSE3_UNOP_AVXINSTR)
#undef EMIT_SSSE3_UNOP_AVXINSTR

#define EMIT_SSE4_PMOV_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3, \
                                notUsed4)                                  \
  __ v##instruction(xmm10, xmm1);                                          \
  __ v##instruction(xmm10, Operand(rdx, 4));
      SSE4_UNOP_INSTRUCTION_LIST(EMIT_SSE4_PMOV_AVXINSTR)
#undef EMIT_SSE4_PMOV_AVXINSTR

#define EMIT_SSE2_SHIFT_IMM_AVX(instruction, notUsed1, notUsed2, notUsed3, \
                                notUsed4)                                  \
  __ v##instruction(xmm0, xmm15, 21);
      SSE2_INSTRUCTION_LIST_SHIFT_IMM(EMIT_SSE2_SHIFT_IMM_AVX)
#undef EMIT_SSE2_SHIFT_IMM_AVX

      __ vinsertps(xmm1, xmm2, xmm3, 1);
      __ vinsertps(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 1);
      __ vextractps(rax, xmm1, 1);

      __ vlddqu(xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vpextrb(rax, xmm2, 12);
      __ vpextrb(Operand(rbx, rcx, times_4, 10000), xmm2, 12);
      __ vpextrw(rax, xmm2, 5);
      __ vpextrw(Operand(rbx, rcx, times_4, 10000), xmm2, 5);
      __ vpextrd(rax, xmm2, 2);
      __ vpextrd(Operand(rbx, rcx, times_4, 10000), xmm2, 2);
      __ vpextrq(rax, xmm2, 2);

      __ vpinsrb(xmm1, xmm2, rax, 12);
      __ vpinsrb(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 12);
      __ vpinsrw(xmm1, xmm2, rax, 5);
      __ vpinsrw(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 5);
      __ vpinsrd(xmm1, xmm2, rax, 2);
      __ vpinsrd(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 2);
      __ vpinsrq(xmm1, xmm2, rax, 9);
      __ vpinsrq(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 9);
      __ vpshufd(xmm1, xmm2, 85);
      __ vpshufd(xmm1, Operand(rbx, rcx, times_4, 10000), 85);
      __ vpshuflw(xmm1, xmm2, 85);
      __ vpshuflw(xmm1, Operand(rbx, rcx, times_4, 10000), 85);
      __ vpshufhw(xmm1, xmm2, 85);
      __ vpshufhw(xmm1, Operand(rbx, rcx, times_4, 10000), 85);
      __ vshufps(xmm3, xmm2, xmm3, 3);
      __ vpblendw(xmm1, xmm2, xmm3, 23);
      __ vpblendw(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 23);
      __ vpalignr(xmm1, xmm2, xmm3, 4);
      __ vpalignr(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 4);

      __ vpblendvb(xmm1, xmm2, xmm3, xmm4);
      __ vblendvps(xmm1, xmm2, xmm3, xmm4);
      __ vblendvpd(xmm1, xmm2, xmm3, xmm4);

      __ vmovddup(xmm1, xmm2);
      __ vmovddup(xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vmovshdup(xmm1, xmm2);
      __ vbroadcastss(xmm1, Operand(rbx, rcx, times_4, 10000));
    }
  }

  // AVX2 instruction
  {
    if (CpuFeatures::IsSupported(AVX2)) {
      CpuFeatureScope scope(&assm, AVX2);
      __ vbroadcastss(xmm1, xmm2);
#define EMIT_AVX2_BROADCAST(instruction, notUsed1, notUsed2, notUsed3, \
                            notUsed4)                                  \
  __ instruction(xmm0, xmm1);                                          \
  __ instruction(xmm0, Operand(rbx, rcx, times_4, 10000));
      AVX2_BROADCAST_LIST(EMIT_AVX2_BROADCAST)
    }
  }

  // FMA3 instruction
  {
    if (CpuFeatures::IsSupported(FMA3)) {
      CpuFeatureScope scope(&assm, FMA3);
#define EMIT_FMA(instr, notUsed1, notUsed2, notUsed3, notUsed4, notUsed5, \
                 notUsed6)                                                \
  __ instr(xmm9, xmm10, xmm11);                                           \
  __ instr(xmm9, xmm10, Operand(rbx, rcx, times_4, 10000));
      FMA_INSTRUCTION_LIST(EMIT_FMA)
#undef EMIT_FMA
    }
  }

  // BMI1 instructions
  {
    if (CpuFeatures::IsSupported(BMI1)) {
      CpuFeatureScope scope(&assm, BMI1);
      __ andnq(rax, rbx, rcx);
      __ andnq(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ andnl(rax, rbx, rcx);
      __ andnl(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ bextrq(rax, rbx, rcx);
      __ bextrq(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ bextrl(rax, rbx, rcx);
      __ bextrl(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ blsiq(rax, rbx);
      __ blsiq(rax, Operand(rbx, rcx, times_4, 10000));
      __ blsil(rax, rbx);
      __ blsil(rax, Operand(rbx, rcx, times_4, 10000));
      __ blsmskq(rax, rbx);
      __ blsmskq(rax, Operand(rbx, rcx, times_4, 10000));
      __ blsmskl(rax, rbx);
      __ blsmskl(rax, Operand(rbx, rcx, times_4, 10000));
      __ blsrq(rax, rbx);
      __ blsrq(rax, Operand(rbx, rcx, times_4, 10000));
      __ blsrl(rax, rbx);
      __ blsrl(rax, Operand(rbx, rcx, times_4, 10000));
      __ tzcntq(rax, rbx);
      __ tzcntq(rax, Operand(rbx, rcx, times_4, 10000));
      __ tzcntl(rax, rbx);
      __ tzcntl(rax, Operand(rbx, rcx, times_4, 10000));
    }
  }

  // LZCNT instructions
  {
    if (CpuFeatures::IsSupported(LZCNT)) {
      CpuFeatureScope scope(&assm, LZCNT);
      __ lzcntq(rax, rbx);
      __ lzcntq(rax, Operand(rbx, rcx, times_4, 10000));
      __ lzcntl(rax, rbx);
      __ lzcntl(rax, Operand(rbx, rcx, times_4, 10000));
    }
  }

  // POPCNT instructions
  {
    if (CpuFeatures::IsSupported(POPCNT)) {
      CpuFeatureScope scope(&assm, POPCNT);
      __ popcntq(rax, rbx);
      __ popcntq(rax, Operand(rbx, rcx, times_4, 10000));
      __ popcntl(rax, rbx);
      __ popcntl(rax, Operand(rbx, rcx, times_4, 10000));
    }
  }

  // BMI2 instructions
  {
    if (CpuFeatures::IsSupported(BMI2)) {
      CpuFeatureScope scope(&assm, BMI2);
      __ bzhiq(rax, rbx, rcx);
      __ bzhiq(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ bzhil(rax, rbx, rcx);
      __ bzhil(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ mulxq(rax, rbx, rcx);
      __ mulxq(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ mulxl(rax, rbx, rcx);
      __ mulxl(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ pdepq(rax, rbx, rcx);
      __ pdepq(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ pdepl(rax, rbx, rcx);
      __ pdepl(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ pextq(rax, rbx, rcx);
      __ pextq(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ pextl(rax, rbx, rcx);
      __ pextl(rax, rbx, Operand(rbx, rcx, times_4, 10000));
      __ sarxq(rax, rbx, rcx);
      __ sarxq(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ sarxl(rax, rbx, rcx);
      __ sarxl(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ shlxq(rax, rbx, rcx);
      __ shlxq(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ shlxl(rax, rbx, rcx);
      __ shlxl(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ shrxq(rax, rbx, rcx);
      __ shrxq(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ shrxl(rax, rbx, rcx);
      __ shrxl(rax, Operand(rbx, rcx, times_4, 10000), rbx);
      __ rorxq(rax, rbx, 63);
      __ rorxq(rax, Operand(rbx, rcx, times_4, 10000), 63);
      __ rorxl(rax, rbx, 31);
      __ rorxl(rax, Operand(rbx, rcx, times_4, 10000), 31);
    }
  }

  // xchg.
  {
    __ xchgb(rax, Operand(rax, 8));
    __ xchgw(rax, Operand(rbx, 8));
    __ xchgq(rax, rax);
    __ xchgq(rax, rbx);
    __ xchgq(rbx, rbx);
    __ xchgq(rbx, Operand(rsp, 12));
  }

  // cmpxchg.
  {
    __ cmpxchgb(Operand(rsp, 12), rax);
    __ cmpxchgw(Operand(rbx, rcx, times_4, 10000), rax);
    __ cmpxchgl(Operand(rbx, rcx, times_4, 10000), rax);
    __ cmpxchgq(Operand(rbx, rcx, times_4, 10000), rax);
  }

  // xadd.
  {
    __ xaddb(Operand(rsp, 12), rax);
    __ xaddw(Operand(rsp, 12), rax);
    __ xaddl(Operand(rsp, 12), rax);
    __ xaddq(Operand(rsp, 12), rax);
    __ xaddb(Operand(rbx, rcx, times_4, 10000), rax);
    __ xaddw(Operand(rbx, rcx, times_4, 10000), rax);
    __ xaddl(Operand(rbx, rcx, times_4, 10000), rax);
    __ xaddq(Operand(rbx, rcx, times_4, 10000), rax);
  }

  // lock prefix.
  {
    __ lock();
    __ cmpxchgl(Operand(rsp, 12), rbx);

    __ lock();
    __ xchgw(rax, Operand(rcx, 8));
  }

  // Nop instructions
  for (int i = 0; i < 16; i++) {
    __ Nop(i);
  }

  __ mfence();
  __ lfence();
  __ pause();
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  USE(code);
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
  Address begin = code->raw_instruction_start();
  Address end = code->raw_instruction_end();
  disasm::Disassembler::Disassemble(stdout, reinterpret_cast<byte*>(begin),
                                    reinterpret_cast<byte*>(end));
#endif
}

constexpr int kAssemblerBufferSize = 8192;

// Helper to package up all the required classes for disassembling into a
// buffer using |InstructionDecode|.
struct DisassemblerTester {
  DisassemblerTester()
      : assm_(AssemblerOptions{},
              ExternalAssemblerBuffer(buffer_, sizeof(buffer_))),
        disasm(converter_) {}

  std::string InstructionDecode() {
    disasm.InstructionDecode(disasm_buffer, buffer_ + prev_offset);
    return std::string{disasm_buffer.begin()};
  }

  int pc_offset() { return assm_.pc_offset(); }

  Assembler* assm() { return &assm_; }

  v8::internal::byte buffer_[kAssemblerBufferSize];
  Assembler assm_;
  disasm::NameConverter converter_;
  disasm::Disassembler disasm;
  base::EmbeddedVector<char, 128> disasm_buffer;
  int prev_offset = 0;
};

// Helper macro to compare the disassembly of an assembler function call with
// the expected disassembly output. We reuse |Assembler|, so we need to keep
// track of the offset into |buffer| which the Assembler has used, and
// disassemble the instruction at that offset.
// Requires a DisassemblerTester named t.
#define COMPARE(str, ASM)        \
  t.prev_offset = t.pc_offset(); \
  t.assm_.ASM;                   \
  CHECK_EQ(str, t.InstructionDecode());

// Tests that compares the checks the disassembly output with an expected
// string.
UNINITIALIZED_TEST(DisasmX64CheckOutput) {
  DisassemblerTester t;

  // Short immediate instructions
  COMPARE("48054e61bc00         REX.W add rax,0xbc614e",
          addq(rax, Immediate(12345678)));
  COMPARE("480d4e61bc00         REX.W or rax,0xbc614e",
          orq(rax, Immediate(12345678)));
  COMPARE("482d4e61bc00         REX.W sub rax,0xbc614e",
          subq(rax, Immediate(12345678)));
  COMPARE("48354e61bc00         REX.W xor rax,0xbc614e",
          xorq(rax, Immediate(12345678)));
  COMPARE("48254e61bc00         REX.W and rax,0xbc614e",
          andq(rax, Immediate(12345678)));
  COMPARE("488b1c4c             REX.W movq rbx,[rsp+rcx*2]",
          movq(rbx, Operand(rsp, rcx, times_2, 0)));  // [rsp+rcx*2);
  COMPARE("4803d3               REX.W addq rdx,rbx", addq(rdx, rbx));
  COMPARE("480313               REX.W addq rdx,[rbx]",
          addq(rdx, Operand(rbx, 0)));
  COMPARE("48035310             REX.W addq rdx,[rbx+0x10]",
          addq(rdx, Operand(rbx, 16)));
  COMPARE("480393cf070000       REX.W addq rdx,[rbx+0x7cf]",
          addq(rdx, Operand(rbx, 1999)));
  COMPARE("480353fc             REX.W addq rdx,[rbx-0x4]",
          addq(rdx, Operand(rbx, -4)));
  COMPARE("48039331f8ffff       REX.W addq rdx,[rbx-0x7cf]",
          addq(rdx, Operand(rbx, -1999)));
  COMPARE("48031424             REX.W addq rdx,[rsp]",
          addq(rdx, Operand(rsp, 0)));
  COMPARE("4803542410           REX.W addq rdx,[rsp+0x10]",
          addq(rdx, Operand(rsp, 16)));
  COMPARE("48039424cf070000     REX.W addq rdx,[rsp+0x7cf]",
          addq(rdx, Operand(rsp, 1999)));
  COMPARE("48035424fc           REX.W addq rdx,[rsp-0x4]",
          addq(rdx, Operand(rsp, -4)));
  COMPARE("4803942431f8ffff     REX.W addq rdx,[rsp-0x7cf]",
          addq(rdx, Operand(rsp, -1999)));
  COMPARE("4803348d00000000     REX.W addq rsi,[rcx*4+0x0]",
          addq(rsi, Operand(rcx, times_4, 0)));
  COMPARE("4803348d18000000     REX.W addq rsi,[rcx*4+0x18]",
          addq(rsi, Operand(rcx, times_4, 24)));
  COMPARE("4803348dfcffffff     REX.W addq rsi,[rcx*4-0x4]",
          addq(rsi, Operand(rcx, times_4, -4)));
  COMPARE("4803348d31f8ffff     REX.W addq rsi,[rcx*4-0x7cf]",
          addq(rsi, Operand(rcx, times_4, -1999)));
  COMPARE("48037c8d00           REX.W addq rdi,[rbp+rcx*4+0x0]",
          addq(rdi, Operand(rbp, rcx, times_4, 0)));
  COMPARE("48037c8d0c           REX.W addq rdi,[rbp+rcx*4+0xc]",
          addq(rdi, Operand(rbp, rcx, times_4, 12)));
  COMPARE("48037c8df8           REX.W addq rdi,[rbp+rcx*4-0x8]",
          addq(rdi, Operand(rbp, rcx, times_4, -8)));
  COMPARE("4803bc8d61f0ffff     REX.W addq rdi,[rbp+rcx*4-0xf9f]",
          addq(rdi, Operand(rbp, rcx, times_4, -3999)));
  COMPARE("4883448d0c0c         REX.W addq [rbp+rcx*4+0xc],0xc",
          addq(Operand(rbp, rcx, times_4, 12), Immediate(12)));

  COMPARE("400fc8               bswapl rax", bswapl(rax));
  COMPARE("480fcf               REX.W bswapq rdi", bswapq(rdi));
  COMPARE("410fbdc7             bsrl rax,r15", bsrl(rax, r15));
  COMPARE("440fbd0ccd0f670100   bsrl r9,[rcx*8+0x1670f]",
          bsrl(r9, Operand(rcx, times_8, 91919)));

  COMPARE("90                   nop", nop());
  COMPARE("4883c30c             REX.W addq rbx,0xc", addq(rbx, Immediate(12)));
  COMPARE("4883e203             REX.W andq rdx,0x3", andq(rdx, Immediate(3)));
  COMPARE("4823542404           REX.W andq rdx,[rsp+0x4]",
          andq(rdx, Operand(rsp, 4)));
  COMPARE("4883fa03             REX.W cmpq rdx,0x3", cmpq(rdx, Immediate(3)));
  COMPARE("483b542404           REX.W cmpq rdx,[rsp+0x4]",
          cmpq(rdx, Operand(rsp, 4)));
  COMPARE("48817c8d00e8030000   REX.W cmpq [rbp+rcx*4+0x0],0x3e8",
          cmpq(Operand(rbp, rcx, times_4, 0), Immediate(1000)));
  COMPARE("3a5c4d00             cmpb bl,[rbp+rcx*2+0x0]",
          cmpb(rbx, Operand(rbp, rcx, times_2, 0)));
  COMPARE("385c4d00             cmpb [rbp+rcx*2+0x0],bl",
          cmpb(Operand(rbp, rcx, times_2, 0), rbx));
  COMPARE("4883ca03             REX.W orq rdx,0x3", orq(rdx, Immediate(3)));
  COMPARE("4883f203             REX.W xorq rdx,0x3", xorq(rdx, Immediate(3)));
  COMPARE("90                   nop", nop());
  COMPARE("0fa2                 cpuid", cpuid());
  COMPARE("0fbe11               movsxbl rdx,[rcx]",
          movsxbl(rdx, Operand(rcx, 0)));
  COMPARE("480fbe11             REX.W movsxbq rdx,[rcx]",
          movsxbq(rdx, Operand(rcx, 0)));
  COMPARE("0fbf11               movsxwl rdx,[rcx]",
          movsxwl(rdx, Operand(rcx, 0)));
  COMPARE("480fbf11             REX.W movsxwq rdx,[rcx]",
          movsxwq(rdx, Operand(rcx, 0)));
  COMPARE("0fb611               movzxbl rdx,[rcx]",
          movzxbl(rdx, Operand(rcx, 0)));
  COMPARE("0fb711               movzxwl rdx,[rcx]",
          movzxwl(rdx, Operand(rcx, 0)));
  COMPARE("0fb611               movzxbl rdx,[rcx]",
          movzxbq(rdx, Operand(rcx, 0)));
  COMPARE("0fb711               movzxwl rdx,[rcx]",
          movzxwq(rdx, Operand(rcx, 0)));

  COMPARE("480fafd1             REX.W imulq rdx,rcx", imulq(rdx, rcx));
  COMPARE("480fa5ca             REX.W shld rdx,rcx,cl", shld(rdx, rcx));
  COMPARE("480fadca             REX.W shrd rdx,rcx,cl", shrd(rdx, rcx));
  COMPARE("48d1648764           REX.W shlq [rdi+rax*4+0x64], 1",
          shlq(Operand(rdi, rax, times_4, 100), Immediate(1)));
  COMPARE("48c164876406         REX.W shlq [rdi+rax*4+0x64], 6",
          shlq(Operand(rdi, rax, times_4, 100), Immediate(6)));
  COMPARE("49d127               REX.W shlq [r15], 1",
          shlq(Operand(r15, 0), Immediate(1)));
  COMPARE("49c12706             REX.W shlq [r15], 6",
          shlq(Operand(r15, 0), Immediate(6)));
  COMPARE("49d327               REX.W shlq [r15], cl",
          shlq_cl(Operand(r15, 0)));
  COMPARE("49d327               REX.W shlq [r15], cl",
          shlq_cl(Operand(r15, 0)));
  COMPARE("48d3648764           REX.W shlq [rdi+rax*4+0x64], cl",
          shlq_cl(Operand(rdi, rax, times_4, 100)));
  COMPARE("48d3648764           REX.W shlq [rdi+rax*4+0x64], cl",
          shlq_cl(Operand(rdi, rax, times_4, 100)));
  COMPARE("48d1e2               REX.W shlq rdx, 1", shlq(rdx, Immediate(1)));
  COMPARE("48c1e206             REX.W shlq rdx, 6", shlq(rdx, Immediate(6)));
  COMPARE("d1648764             shll [rdi+rax*4+0x64], 1",
          shll(Operand(rdi, rax, times_4, 100), Immediate(1)));
  COMPARE("c164876406           shll [rdi+rax*4+0x64], 6",
          shll(Operand(rdi, rax, times_4, 100), Immediate(6)));
  COMPARE("41d127               shll [r15], 1",
          shll(Operand(r15, 0), Immediate(1)));
  COMPARE("41c12706             shll [r15], 6",
          shll(Operand(r15, 0), Immediate(6)));
  COMPARE("41d327               shll [r15], cl", shll_cl(Operand(r15, 0)));
  COMPARE("41d327               shll [r15], cl", shll_cl(Operand(r15, 0)));
  COMPARE("d3648764             shll [rdi+rax*4+0x64], cl",
          shll_cl(Operand(rdi, rax, times_4, 100)));
  COMPARE("d3648764             shll [rdi+rax*4+0x64], cl",
          shll_cl(Operand(rdi, rax, times_4, 100)));
  COMPARE("d1e2                 shll rdx, 1", shll(rdx, Immediate(1)));
  COMPARE("c1e206               shll rdx, 6", shll(rdx, Immediate(6)));
  COMPARE("480fa30a             REX.W bt [rdx],rcx,cl",
          btq(Operand(rdx, 0), rcx));
  COMPARE("480fab0a             REX.W bts [rdx],rcx",
          btsq(Operand(rdx, 0), rcx));
  COMPARE("480fab0c8b           REX.W bts [rbx+rcx*4],rcx",
          btsq(Operand(rbx, rcx, times_4, 0), rcx));
  COMPARE("480fbae90d           REX.W bts rcx,13", btsq(rcx, Immediate(13)));
  COMPARE("480fbaf10d           REX.W btr rcx,13", btrq(rcx, Immediate(13)));
  COMPARE("6a0c                 push 0xc", pushq(Immediate(12)));
  COMPARE("68a05b0000           push 0x5ba0", pushq(Immediate(23456)));
  COMPARE("51                   push rcx", pushq(rcx));
  COMPARE("56                   push rsi", pushq(rsi));
  COMPARE("ff75f0               push [rbp-0x10]",
          pushq(Operand(rbp, StandardFrameConstants::kFunctionOffset)));
  COMPARE("ff348b               push [rbx+rcx*4]",
          pushq(Operand(rbx, rcx, times_4, 0)));
  COMPARE("ff348b               push [rbx+rcx*4]",
          pushq(Operand(rbx, rcx, times_4, 0)));
  COMPARE("ffb48b10270000       push [rbx+rcx*4+0x2710]",
          pushq(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("5a                   pop rdx", popq(rdx));
  COMPARE("58                   pop rax", popq(rax));
  COMPARE("8f048b               pop [rbx+rcx*4]",
          popq(Operand(rbx, rcx, times_4, 0)));

  COMPARE("4803542410           REX.W addq rdx,[rsp+0x10]",
          addq(rdx, Operand(rsp, 16)));
  COMPARE("4803d1               REX.W addq rdx,rcx", addq(rdx, rcx));
  COMPARE("8a11                 movb dl,[rcx]", movb(rdx, Operand(rcx, 0)));
  COMPARE("b106                 movb cl,6", movb(rcx, Immediate(6)));
  COMPARE("88542410             movb [rsp+0x10],dl",
          movb(Operand(rsp, 16), rdx));
  COMPARE("6689542410           movw [rsp+0x10],rdx",
          movw(Operand(rsp, 16), rdx));
  COMPARE("90                   nop", nop());
  COMPARE("480fbf54240c         REX.W movsxwq rdx,[rsp+0xc]",
          movsxwq(rdx, Operand(rsp, 12)));
  COMPARE("480fbe54240c         REX.W movsxbq rdx,[rsp+0xc]",
          movsxbq(rdx, Operand(rsp, 12)));
  COMPARE("486354240c           REX.W movsxlq rdx,[rsp+0xc]",
          movsxlq(rdx, Operand(rsp, 12)));
  COMPARE("0fb754240c           movzxwl rdx,[rsp+0xc]",
          movzxwq(rdx, Operand(rsp, 12)));
  COMPARE("0fb654240c           movzxbl rdx,[rsp+0xc]",
          movzxbq(rdx, Operand(rsp, 12)));
  COMPARE("90                   nop", nop());
  COMPARE("48c7c287d61200       REX.W movq rdx,0x12d687",
          movq(rdx, Immediate(1234567)));
  COMPARE("488b54240c           REX.W movq rdx,[rsp+0xc]",
          movq(rdx, Operand(rsp, 12)));
  COMPARE("48c7848b1027000039300000 REX.W movq [rbx+rcx*4+0x2710],0x3039",
          movq(Operand(rbx, rcx, times_4, 10000), Immediate(12345)));
  COMPARE("4889948b10270000     REX.W movq [rbx+rcx*4+0x2710],rdx",
          movq(Operand(rbx, rcx, times_4, 10000), rdx));
  COMPARE("90                   nop", nop());
  COMPARE("feca                 decb dl", decb(rdx));
  COMPARE("fe480a               decb [rax+0xa]", decb(Operand(rax, 10)));
  COMPARE("fe8c8b10270000       decb [rbx+rcx*4+0x2710]",
          decb(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("48ffca               REX.W decq rdx", decq(rdx));
  COMPARE("99                   cdql", cdq());

  COMPARE("f3ab                 rep stosl", repstosl());
  COMPARE("f348ab               REX.W rep stosq", repstosq());

  COMPARE("48f7fa               REX.W idivq rdx", idivq(rdx));
  COMPARE("f7e2                 mull rdx", mull(rdx));
  COMPARE("48f7e2               REX.W mulq rdx", mulq(rdx));

  COMPARE("f6da                 negb rdx", negb(rdx));
  COMPARE("41f6da               negb r10", negb(r10));
  COMPARE("66f7da               negw rdx", negw(rdx));
  COMPARE("f7da                 negl rdx", negl(rdx));
  COMPARE("48f7da               REX.W negq rdx", negq(rdx));
  COMPARE("f65c240c             negb [rsp+0xc]", negb(Operand(rsp, 12)));
  COMPARE("66f75c240c           negw [rsp+0xc]", negw(Operand(rsp, 12)));
  COMPARE("f75c240c             negl [rsp+0xc]", negl(Operand(rsp, 12)));
  COMPARE("f65c240c             negb [rsp+0xc]", negb(Operand(rsp, 12)));

  COMPARE("48f7d2               REX.W notq rdx", notq(rdx));
  COMPARE("4885948b10270000     REX.W testq rdx,[rbx+rcx*4+0x2710]",
          testq(Operand(rbx, rcx, times_4, 10000), rdx));

  COMPARE("486bd10c             REX.W imulq rdx,rcx,0xc",
          imulq(rdx, rcx, Immediate(12)));
  COMPARE("4869d1e8030000       REX.W imulq rdx,rcx,0x3e8",
          imulq(rdx, rcx, Immediate(1000)));
  COMPARE("480faf948b10270000   REX.W imulq rdx,[rbx+rcx*4+0x2710]",
          imulq(rdx, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("486b948b102700000c   REX.W imulq rdx,[rbx+rcx*4+0x2710],0xc",
          imulq(rdx, Operand(rbx, rcx, times_4, 10000), Immediate(12)));
  COMPARE("4869948b10270000e8030000 REX.W imulq rdx,[rbx+rcx*4+0x2710],0x3e8",
          imulq(rdx, Operand(rbx, rcx, times_4, 10000), Immediate(1000)));
  COMPARE("446bf90c             imull r15,rcx,0xc",
          imull(r15, rcx, Immediate(12)));
  COMPARE("4469f9e8030000       imull r15,rcx,0x3e8",
          imull(r15, rcx, Immediate(1000)));
  COMPARE("440fafbc8b10270000   imull r15,[rbx+rcx*4+0x2710]",
          imull(r15, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("446bbc8b102700000c   imull r15,[rbx+rcx*4+0x2710],0xc",
          imull(r15, Operand(rbx, rcx, times_4, 10000), Immediate(12)));
  COMPARE("4469bc8b10270000e8030000 imull r15,[rbx+rcx*4+0x2710],0x3e8",
          imull(r15, Operand(rbx, rcx, times_4, 10000), Immediate(1000)));

  COMPARE("48ffc2               REX.W incq rdx", incq(rdx));
  COMPARE("48ff848b10270000     REX.W incq [rbx+rcx*4+0x2710]",
          incq(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("ffb48b10270000       push [rbx+rcx*4+0x2710]",
          pushq(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("8f848b10270000       pop [rbx+rcx*4+0x2710]",
          popq(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("ffa48b10270000       jmp [rbx+rcx*4+0x2710]",
          jmp(Operand(rbx, rcx, times_4, 10000)));

  COMPARE("488d948b10270000     REX.W leaq rdx,[rbx+rcx*4+0x2710]",
          leaq(rdx, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("4881ca39300000       REX.W orq rdx,0x3039",
          orq(rdx, Immediate(12345)));
  COMPARE("480b948b10270000     REX.W orq rdx,[rbx+rcx*4+0x2710]",
          orq(rdx, Operand(rbx, rcx, times_4, 10000)));

  COMPARE("48d1d2               REX.W rclq rdx, 1", rclq(rdx, Immediate(1)));
  COMPARE("48c1d207             REX.W rclq rdx, 7", rclq(rdx, Immediate(7)));
  COMPARE("48d1da               REX.W rcrq rdx, 1", rcrq(rdx, Immediate(1)));
  COMPARE("48c1da07             REX.W rcrq rdx, 7", rcrq(rdx, Immediate(7)));
  COMPARE("48d1fa               REX.W sarq rdx, 1", sarq(rdx, Immediate(1)));
  COMPARE("48c1fa06             REX.W sarq rdx, 6", sarq(rdx, Immediate(6)));
  COMPARE("48d3fa               REX.W sarq rdx, cl", sarq_cl(rdx));
  COMPARE("481bd3               REX.W sbbq rdx,rbx", sbbq(rdx, rbx));
  COMPARE("480fa5da             REX.W shld rdx,rbx,cl", shld(rdx, rbx));
  COMPARE("48d1e2               REX.W shlq rdx, 1", shlq(rdx, Immediate(1)));
  COMPARE("48c1e206             REX.W shlq rdx, 6", shlq(rdx, Immediate(6)));
  COMPARE("48d3e2               REX.W shlq rdx, cl", shlq_cl(rdx));
  COMPARE("480fadda             REX.W shrd rdx,rbx,cl", shrd(rdx, rbx));
  COMPARE("48d1ea               REX.W shrq rdx, 1", shrq(rdx, Immediate(1)));
  COMPARE("48c1ea07             REX.W shrq rdx, 7", shrq(rdx, Immediate(7)));
  COMPARE("48d3ea               REX.W shrq rdx, cl", shrq_cl(rdx));

  COMPARE("4883c30c             REX.W addq rbx,0xc", addq(rbx, Immediate(12)));
  COMPARE("4883848a102700000c   REX.W addq [rdx+rcx*4+0x2710],0xc",
          addq(Operand(rdx, rcx, times_4, 10000), Immediate(12)));
  COMPARE("4881e339300000       REX.W andq rbx,0x3039",
          andq(rbx, Immediate(12345)));

  COMPARE("4881fb39300000       REX.W cmpq rbx,0x3039",
          cmpq(rbx, Immediate(12345)));
  COMPARE("4883fb0c             REX.W cmpq rbx,0xc", cmpq(rbx, Immediate(12)));
  COMPARE("4883bc8a102700000c   REX.W cmpq [rdx+rcx*4+0x2710],0xc",
          cmpq(Operand(rdx, rcx, times_4, 10000), Immediate(12)));
  COMPARE("80f864               cmpb al,0x64", cmpb(rax, Immediate(100)));

  COMPARE("4881cb39300000       REX.W orq rbx,0x3039",
          orq(rbx, Immediate(12345)));
  COMPARE("4883eb0c             REX.W subq rbx,0xc", subq(rbx, Immediate(12)));
  COMPARE("4883ac8a102700000c   REX.W subq [rdx+rcx*4+0x2710],0xc",
          subq(Operand(rdx, rcx, times_4, 10000), Immediate(12)));
  COMPARE("4881f339300000       REX.W xorq rbx,0x3039",
          xorq(rbx, Immediate(12345)));
  COMPARE("486bd10c             REX.W imulq rdx,rcx,0xc",
          imulq(rdx, rcx, Immediate(12)));
  COMPARE("4869d1e8030000       REX.W imulq rdx,rcx,0x3e8",
          imulq(rdx, rcx, Immediate(1000)));

  COMPARE("fc                   cldl", cld());

  COMPARE("482b948b10270000     REX.W subq rdx,[rbx+rcx*4+0x2710]",
          subq(rdx, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("482bd3               REX.W subq rdx,rbx", subq(rdx, rbx));

  COMPARE("66f7c23930           testw rdx,0x3039",
          testq(rdx, Immediate(12345)));
  COMPARE("488594cb10270000     REX.W testq rdx,[rbx+rcx*8+0x2710]",
          testq(Operand(rbx, rcx, times_8, 10000), rdx));
  COMPARE("849459e8030000       testb dl,[rcx+rbx*2+0x3e8]",
          testb(Operand(rcx, rbx, times_2, 1000), rdx));
  COMPARE("f640ec9a             testb [rax-0x14],0x9a",
          testb(Operand(rax, -20), Immediate(0x9A)));

  COMPARE("4881f239300000       REX.W xorq rdx,0x3039",
          xorq(rdx, Immediate(12345)));
  COMPARE("483394cb10270000     REX.W xorq rdx,[rbx+rcx*8+0x2710]",
          xorq(rdx, Operand(rbx, rcx, times_8, 10000)));
  COMPARE("f4                   hltl", hlt());
  COMPARE("cc                   int3l", int3());
  COMPARE("c3                   retl", ret(0));
  COMPARE("c20800               ret 0x8", ret(8));

  // 0xD9 instructions
  COMPARE("d9c1                 fld st1", fld(1));
  COMPARE("d9e8                 fld1", fld1());
  COMPARE("d9ee                 fldz", fldz());
  COMPARE("d9eb                 fldpi", fldpi());
  COMPARE("d9e1                 fabs", fabs());
  COMPARE("d9e0                 fchs", fchs());
  COMPARE("d9f8                 fprem", fprem());
  COMPARE("d9f5                 fprem1", fprem1());
  COMPARE("d9f7                 fincstp", fincstp());
  COMPARE("d9e4                 ftst", ftst());
  COMPARE("d9cb                 fxch st3", fxch(3));
  COMPARE("d9848b10270000       fld_s [rbx+rcx*4+0x2710]",
          fld_s(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("d99c8b10270000       fstp_s [rbx+rcx*4+0x2710]",
          fstp_s(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("ddc3                 ffree st3", ffree(3));
  COMPARE("dd848b10270000       fld_d [rbx+rcx*4+0x2710]",
          fld_d(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("dd9c8b10270000       fstp_d [rbx+rcx*4+0x2710]",
          fstp_d(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("db848b10270000       fild_s [rbx+rcx*4+0x2710]",
          fild_s(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("db9c8b10270000       fistp_s [rbx+rcx*4+0x2710]",
          fistp_s(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("dfac8b10270000       fild_d [rbx+rcx*4+0x2710]",
          fild_d(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("dfbc8b10270000       fistp_d [rbx+rcx*4+0x2710]",
          fistp_d(Operand(rbx, rcx, times_4, 10000)));
  COMPARE("dfe0                 fnstsw_ax", fnstsw_ax());
  COMPARE("dcc3                 fadd st3", fadd(3));
  COMPARE("dceb                 fsub st3", fsub(3));
  COMPARE("dccb                 fmul st3", fmul(3));
  COMPARE("dcfb                 fdiv st3", fdiv(3));
  COMPARE("dec3                 faddp st3", faddp(3));
  COMPARE("deeb                 fsubp st3", fsubp(3));
  COMPARE("decb                 fmulp st3", fmulp(3));
  COMPARE("defb                 fdivp st3", fdivp(3));
  COMPARE("ded9                 fcompp", fcompp());
  COMPARE("9b                   fwaitl", fwait());
  COMPARE("d9fc                 frndint", frndint());
  COMPARE("dbe3                 fninit", fninit());

  COMPARE("480f4000             REX.W cmovoq rax,[rax]",
          cmovq(overflow, rax, Operand(rax, 0)));
  COMPARE("480f414001           REX.W cmovnoq rax,[rax+0x1]",
          cmovq(no_overflow, rax, Operand(rax, 1)));
  COMPARE("480f424002           REX.W cmovcq rax,[rax+0x2]",
          cmovq(below, rax, Operand(rax, 2)));
  COMPARE("480f434003           REX.W cmovncq rax,[rax+0x3]",
          cmovq(above_equal, rax, Operand(rax, 3)));
  COMPARE("480f4403             REX.W cmovzq rax,[rbx]",
          cmovq(equal, rax, Operand(rbx, 0)));
  COMPARE("480f454301           REX.W cmovnzq rax,[rbx+0x1]",
          cmovq(not_equal, rax, Operand(rbx, 1)));
  COMPARE("480f464302           REX.W cmovnaq rax,[rbx+0x2]",
          cmovq(below_equal, rax, Operand(rbx, 2)));
  COMPARE("480f474303           REX.W cmovaq rax,[rbx+0x3]",
          cmovq(above, rax, Operand(rbx, 3)));
  COMPARE("480f4801             REX.W cmovsq rax,[rcx]",
          cmovq(sign, rax, Operand(rcx, 0)));
  COMPARE("480f494101           REX.W cmovnsq rax,[rcx+0x1]",
          cmovq(not_sign, rax, Operand(rcx, 1)));
  COMPARE("480f4a4102           REX.W cmovpeq rax,[rcx+0x2]",
          cmovq(parity_even, rax, Operand(rcx, 2)));
  COMPARE("480f4b4103           REX.W cmovpoq rax,[rcx+0x3]",
          cmovq(parity_odd, rax, Operand(rcx, 3)));
  COMPARE("480f4c02             REX.W cmovlq rax,[rdx]",
          cmovq(less, rax, Operand(rdx, 0)));
  COMPARE("480f4d4201           REX.W cmovgeq rax,[rdx+0x1]",
          cmovq(greater_equal, rax, Operand(rdx, 1)));
  COMPARE("480f4e4202           REX.W cmovleq rax,[rdx+0x2]",
          cmovq(less_equal, rax, Operand(rdx, 2)));
  COMPARE("480f4f4203           REX.W cmovgq rax,[rdx+0x3]",
          cmovq(greater, rax, Operand(rdx, 3)));
}

// This compares just the disassemble instruction (without the hex).
// Requires a |std::string actual| to be in scope.
// Hard coded offset of 21, the hex part is 20 bytes, plus a space. If and when
// the padding changes, this should be adjusted.
constexpr int kHexOffset = 21;
#define COMPARE_INSTR(str, ASM)                                         \
  t.prev_offset = t.pc_offset();                                        \
  t.assm_.ASM;                                                          \
  actual = t.InstructionDecode();                                       \
  actual = std::string(actual, kHexOffset, actual.size() - kHexOffset); \
  CHECK_EQ(str, actual);

UNINITIALIZED_TEST(DisasmX64CheckOutputSSE) {
  DisassemblerTester t;
  std::string actual;

  COMPARE("f30f2c948b10270000   cvttss2sil rdx,[rbx+rcx*4+0x2710]",
          cvttss2si(rdx, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f30f2cd1             cvttss2sil rdx,xmm1", cvttss2si(rdx, xmm1));
  COMPARE("f3480f2a8c8b10270000 REX.W cvtsi2ss xmm1,[rbx+rcx*4+0x2710]",
          cvtqsi2ss(xmm1, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f3480f2aca           REX.W cvtsi2ss xmm1,rdx", cvtqsi2ss(xmm1, rdx));
  COMPARE("f3480f5bc1           REX.W cvttps2dq xmm0,xmm1",
          cvttps2dq(xmm0, xmm1));
  COMPARE("f3480f5b848b10270000 REX.W cvttps2dq xmm0,[rbx+rcx*4+0x2710]",
          cvttps2dq(xmm0, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("0f28c1               movaps xmm0,xmm1", movaps(xmm0, xmm1));
  COMPARE("0f28848b10270000     movaps xmm0,[rbx+rcx*4+0x2710]",
          movaps(xmm0, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("66480f6f44240c       REX.W movdqa xmm0,[rsp+0xc]",
          movdqa(xmm0, Operand(rsp, 12)));
  COMPARE("66480f7f44240c       REX.W movdqa [rsp+0xc],xmm0",
          movdqa(Operand(rsp, 12), xmm0));
  COMPARE("f3480f6f44240c       REX.W movdqu xmm0,[rsp+0xc]",
          movdqu(xmm0, Operand(rsp, 12)));
  COMPARE("f3480f7f44240c       REX.W movdqu [rsp+0xc],xmm0",
          movdqu(Operand(rsp, 12), xmm0));
  COMPARE("f3480f6fc8           REX.W movdqu xmm1,xmm0", movdqu(xmm1, xmm0));
  COMPARE("0f12e9               movhlps xmm5,xmm1", movhlps(xmm5, xmm1));
  COMPARE("440f12848b10270000   movlps xmm8,[rbx+rcx*4+0x2710]",
          movlps(xmm8, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("440f138c8b10270000   movlps [rbx+rcx*4+0x2710],xmm9",
          movlps(Operand(rbx, rcx, times_4, 10000), xmm9));
  COMPARE("0f16e9               movlhps xmm5,xmm1", movlhps(xmm5, xmm1));
  COMPARE("440f16848b10270000   movhps xmm8,[rbx+rcx*4+0x2710]",
          movhps(xmm8, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("440f178c8b10270000   movhps [rbx+rcx*4+0x2710],xmm9",
          movhps(Operand(rbx, rcx, times_4, 10000), xmm9));
  COMPARE("410fc6c100           shufps xmm0, xmm9, 0", shufps(xmm0, xmm9, 0x0));
  COMPARE("0f2ec1               ucomiss xmm0,xmm1", ucomiss(xmm0, xmm1));
  COMPARE("0f2e848b10270000     ucomiss xmm0,[rbx+rcx*4+0x2710]",
          ucomiss(xmm0, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("410f50d1             movmskps rdx,xmm9", movmskps(rdx, xmm9));

  std::string exp;

#define COMPARE_SSE_INSTR(instruction, _, __)    \
  exp = #instruction " xmm1,xmm0";               \
  COMPARE_INSTR(exp, instruction(xmm1, xmm0));   \
  exp = #instruction " xmm1,[rbx+rcx*4+0x2710]"; \
  COMPARE_INSTR(exp, instruction(xmm1, Operand(rbx, rcx, times_4, 10000)));
  SSE_BINOP_INSTRUCTION_LIST(COMPARE_SSE_INSTR)
  SSE_UNOP_INSTRUCTION_LIST(COMPARE_SSE_INSTR)
#undef COMPARE_SSE_INSTR

#define COMPARE_SSE_INSTR(instruction, _, __, ___) \
  exp = #instruction " xmm1,xmm0";                 \
  COMPARE_INSTR(exp, instruction(xmm1, xmm0));     \
  exp = #instruction " xmm1,[rbx+rcx*4+0x2710]";   \
  COMPARE_INSTR(exp, instruction(xmm1, Operand(rbx, rcx, times_4, 10000)));
  SSE_INSTRUCTION_LIST_SS(COMPARE_SSE_INSTR)
#undef COMPARE_SSE_INSTR
}

UNINITIALIZED_TEST(DisasmX64CheckOutputSSE2) {
  DisassemblerTester t;
  std::string actual, exp;

  COMPARE("f30fe6dc             cvtdq2pd xmm3,xmm4", cvtdq2pd(xmm3, xmm4));
  COMPARE("f20f2c948b10270000   cvttsd2sil rdx,[rbx+rcx*4+0x2710]",
          cvttsd2si(rdx, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f20f2cd1             cvttsd2sil rdx,xmm1", cvttsd2si(rdx, xmm1));
  COMPARE("f2480f2cd1           REX.W cvttsd2siq rdx,xmm1",
          cvttsd2siq(rdx, xmm1));
  COMPARE("f2480f2c948b10270000 REX.W cvttsd2siq rdx,[rbx+rcx*4+0x2710]",
          cvttsd2siq(rdx, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f20f2a8c8b10270000   cvtsi2sd xmm1,[rbx+rcx*4+0x2710]",
          cvtlsi2sd(xmm1, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f20f2aca             cvtsi2sd xmm1,rdx", cvtlsi2sd(xmm1, rdx));
  COMPARE("f2480f2a8c8b10270000 REX.W cvtsi2sd xmm1,[rbx+rcx*4+0x2710]",
          cvtqsi2sd(xmm1, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f2480f2aca           REX.W cvtsi2sd xmm1,rdx", cvtqsi2sd(xmm1, rdx));
  COMPARE("f3410f5ac9           cvtss2sd xmm1,xmm9", cvtss2sd(xmm1, xmm9));
  COMPARE("f30f5a8c8b10270000   cvtss2sd xmm1,[rbx+rcx*4+0x2710]",
          cvtss2sd(xmm1, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f2410f2dd1           cvtsd2sil rdx,xmm9", cvtsd2si(rdx, xmm9));
  COMPARE("f2490f2dd1           REX.W cvtsd2siq rdx,xmm9",
          cvtsd2siq(rdx, xmm9););

  COMPARE("f20f108c8b10270000   movsd xmm1,[rbx+rcx*4+0x2710]",
          movsd(xmm1, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f20f118c8b10270000   movsd [rbx+rcx*4+0x2710],xmm1",
          movsd(Operand(rbx, rcx, times_4, 10000), xmm1));
  COMPARE("660f10848b10270000   movupd xmm0,[rbx+rcx*4+0x2710]",
          movupd(xmm0, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("660f11848b10270000   movupd [rbx+rcx*4+0x2710],xmm0",
          movupd(Operand(rbx, rcx, times_4, 10000), xmm0));
  COMPARE("66480f6f848b10270000 REX.W movdqa xmm0,[rbx+rcx*4+0x2710]",
          movdqa(xmm0, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("66480f7f848b10270000 REX.W movdqa [rbx+rcx*4+0x2710],xmm0",
          movdqa(Operand(rbx, rcx, times_4, 10000), xmm0));
  COMPARE("66480f7fc8           REX.W movdqa xmm0,xmm1", movdqa(xmm0, xmm1));
  COMPARE("660f2ec1             ucomisd xmm0,xmm1", ucomisd(xmm0, xmm1));
  COMPARE("66440f2e849310270000 ucomisd xmm8,[rbx+rdx*4+0x2710]",
          ucomisd(xmm8, Operand(rbx, rdx, times_4, 10000)));
  COMPARE("f2410fc2db01         cmpltsd xmm3,xmm11", cmpltsd(xmm3, xmm11));
  COMPARE("66410f50d1           movmskpd rdx,xmm9", movmskpd(rdx, xmm9));
  COMPARE("66410fd7d1           pmovmskb r9,xmm2", pmovmskb(rdx, xmm9));
  COMPARE("660f76c8             pcmpeqd xmm1,xmm0", pcmpeqd(xmm1, xmm0));
  COMPARE("66410f62cb           punpckldq xmm1,xmm11", punpckldq(xmm1, xmm11));
  COMPARE("660f626a04           punpckldq xmm5,[rdx+0x4]",
          punpckldq(xmm5, Operand(rdx, 4)));
  COMPARE("66450f6ac7           punpckhdq xmm8,xmm15", punpckhdq(xmm8, xmm15));
  COMPARE("f20f70d403           pshuflw xmm2,xmm4,3", pshuflw(xmm2, xmm4, 3));
  COMPARE("f3410f70c906         pshufhw xmm1,xmm9, 6", pshufhw(xmm1, xmm9, 6));

#define COMPARE_SSE2_INSTR(instruction, _, __, ___) \
  exp = #instruction " xmm1,xmm0";                  \
  COMPARE_INSTR(exp, instruction(xmm1, xmm0));      \
  exp = #instruction " xmm1,[rbx+rcx*4+0x2710]";    \
  COMPARE_INSTR(exp, instruction(xmm1, Operand(rbx, rcx, times_4, 10000)));
  SSE2_INSTRUCTION_LIST(COMPARE_SSE2_INSTR)
  SSE2_UNOP_INSTRUCTION_LIST(COMPARE_SSE2_INSTR)
  SSE2_INSTRUCTION_LIST_SD(COMPARE_SSE2_INSTR)
#undef COMPARE_SSE2_INSTR

#define COMPARE_SSE2_SHIFT_IMM(instruction, _, __, ___, ____) \
  exp = #instruction " xmm3,35";                              \
  COMPARE_INSTR(exp, instruction(xmm3, 0xA3));
  SSE2_INSTRUCTION_LIST_SHIFT_IMM(COMPARE_SSE2_SHIFT_IMM)
#undef COMPARE_SSE2_SHIFT_IMM
}

UNINITIALIZED_TEST(DisasmX64CheckOutputSSE3) {
  if (!CpuFeatures::IsSupported(SSE3)) {
    return;
  }

  DisassemblerTester t;
  CpuFeatureScope scope(&t.assm_, SSE3);

  COMPARE("f20f7cc8             haddps xmm1,xmm0", haddps(xmm1, xmm0));
  COMPARE("f20f7c8c8b10270000   haddps xmm1,[rbx+rcx*4+0x2710]",
          haddps(xmm1, Operand(rbx, rcx, times_4, 10000)));
  COMPARE("f20ff04a04           lddqu xmm1,[rdx+0x4]",
          lddqu(xmm1, Operand(rdx, 4)));
  COMPARE("f20f124805           movddup xmm1,[rax+0x5]",
          movddup(xmm1, Operand(rax, 5)));
  COMPARE("f20f12ca             movddup xmm1,xmm2", movddup(xmm1, xmm2));
  COMPARE("f30f16ca             movshdup xmm1,xmm2", movshdup(xmm1, xmm2));
}

UNINITIALIZED_TEST(DisasmX64CheckOutputSSSE3) {
  if (!CpuFeatures::IsSupported(SSSE3)) {
    return;
  }

  DisassemblerTester t;
  std::string actual, exp;
  CpuFeatureScope scope(&t.assm_, SSSE3);

  COMPARE("660f3a0fe905         palignr xmm5,xmm1,0x5", palignr(xmm5, xmm1, 5));
  COMPARE("660f3a0f6a0405       palignr xmm5,[rdx+0x4],0x5",
          palignr(xmm5, Operand(rdx, 4), 5));

#define COMPARE_SSSE3_INSTR(instruction, _, __, ___, ____) \
  exp = #instruction " xmm5,xmm1";                         \
  COMPARE_INSTR(exp, instruction(xmm5, xmm1));             \
  exp = #instruction " xmm5,[rbx+rcx*4+0x2710]";           \
  COMPARE_INSTR(exp, instruction(xmm5, Operand(rbx, rcx, times_4, 10000)));
  SSSE3_INSTRUCTION_LIST(COMPARE_SSSE3_INSTR)
  SSSE3_UNOP_INSTRUCTION_LIST(COMPARE_SSSE3_INSTR)
#undef COMPARE_SSSE3_INSTR
}

UNINITIALIZED_TEST(DisasmX64YMMRegister) {
  if (!CpuFeatures::IsSupported(AVX)) return;
  DisassemblerTester t;
  CpuFeatureScope fscope(t.assm(), AVX);

  // Short immediate instructions
  COMPARE("c5fd6fc1             vmovdqa ymm0,ymm1", vmovdqa(ymm0, ymm1));
  COMPARE("c5f77cc2             vhaddps ymm0,ymm1,ymm2",
          vhaddps(ymm0, ymm1, ymm2));
  COMPARE("c5f77c848b10270000   vhaddps ymm0,ymm1,[rbx+rcx*4+0x2710]",
          vhaddps(ymm0, ymm1, Operand(rbx, rcx, times_4, 10000)));
}

#undef __

}  // namespace internal
}  // namespace v8
