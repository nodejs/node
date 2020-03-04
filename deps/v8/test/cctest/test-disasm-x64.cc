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

#include "src/init/v8.h"

#include "src/codegen/code-factory.h"
#include "src/codegen/macro-assembler.h"
#include "src/debug/debug.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/execution/frames-inl.h"
#include "src/utils/ostreams.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

#define __ assm.

TEST(DisasmX64) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  v8::internal::byte buffer[8192];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  // Short immediate instructions
  __ addq(rax, Immediate(12345678));
  __ orq(rax, Immediate(12345678));
  __ subq(rax, Immediate(12345678));
  __ xorq(rax, Immediate(12345678));
  __ andq(rax, Immediate(12345678));

  // ---- This one caused crash
  __ movq(rbx,  Operand(rsp, rcx, times_2, 0));  // [rsp+rcx*4]

  // ---- All instructions that I can think of
  __ addq(rdx, rbx);
  __ addq(rdx, Operand(rbx, 0));
  __ addq(rdx, Operand(rbx, 16));
  __ addq(rdx, Operand(rbx, 1999));
  __ addq(rdx, Operand(rbx, -4));
  __ addq(rdx, Operand(rbx, -1999));
  __ addq(rdx, Operand(rsp, 0));
  __ addq(rdx, Operand(rsp, 16));
  __ addq(rdx, Operand(rsp, 1999));
  __ addq(rdx, Operand(rsp, -4));
  __ addq(rdx, Operand(rsp, -1999));
  __ nop();
  __ addq(rsi, Operand(rcx, times_4, 0));
  __ addq(rsi, Operand(rcx, times_4, 24));
  __ addq(rsi, Operand(rcx, times_4, -4));
  __ addq(rsi, Operand(rcx, times_4, -1999));
  __ nop();
  __ addq(rdi, Operand(rbp, rcx, times_4, 0));
  __ addq(rdi, Operand(rbp, rcx, times_4, 12));
  __ addq(rdi, Operand(rbp, rcx, times_4, -8));
  __ addq(rdi, Operand(rbp, rcx, times_4, -3999));
  __ addq(Operand(rbp, rcx, times_4, 12), Immediate(12));

  __ bswapl(rax);
  __ bswapq(rdi);
  __ bsrl(rax, r15);
  __ bsrl(r9, Operand(rcx, times_8, 91919));

  __ nop();
  __ addq(rbx, Immediate(12));
  __ nop();
  __ nop();
  __ andq(rdx, Immediate(3));
  __ andq(rdx, Operand(rsp, 4));
  __ cmpq(rdx, Immediate(3));
  __ cmpq(rdx, Operand(rsp, 4));
  __ cmpq(Operand(rbp, rcx, times_4, 0), Immediate(1000));
  __ cmpb(rbx, Operand(rbp, rcx, times_2, 0));
  __ cmpb(Operand(rbp, rcx, times_2, 0), rbx);
  __ orq(rdx, Immediate(3));
  __ xorq(rdx, Immediate(3));
  __ nop();
  __ cpuid();
  __ movsxbl(rdx, Operand(rcx, 0));
  __ movsxbq(rdx, Operand(rcx, 0));
  __ movsxwl(rdx, Operand(rcx, 0));
  __ movsxwq(rdx, Operand(rcx, 0));
  __ movzxbl(rdx, Operand(rcx, 0));
  __ movzxwl(rdx, Operand(rcx, 0));
  __ movzxbq(rdx, Operand(rcx, 0));
  __ movzxwq(rdx, Operand(rcx, 0));

  __ nop();
  __ imulq(rdx, rcx);
  __ shld(rdx, rcx);
  __ shrd(rdx, rcx);
  __ shlq(Operand(rdi, rax, times_4, 100), Immediate(1));
  __ shlq(Operand(rdi, rax, times_4, 100), Immediate(6));
  __ shlq(Operand(r15, 0), Immediate(1));
  __ shlq(Operand(r15, 0), Immediate(6));
  __ shlq_cl(Operand(r15, 0));
  __ shlq_cl(Operand(r15, 0));
  __ shlq_cl(Operand(rdi, rax, times_4, 100));
  __ shlq_cl(Operand(rdi, rax, times_4, 100));
  __ shlq(rdx, Immediate(1));
  __ shlq(rdx, Immediate(6));
  __ shll(Operand(rdi, rax, times_4, 100), Immediate(1));
  __ shll(Operand(rdi, rax, times_4, 100), Immediate(6));
  __ shll(Operand(r15, 0), Immediate(1));
  __ shll(Operand(r15, 0), Immediate(6));
  __ shll_cl(Operand(r15, 0));
  __ shll_cl(Operand(r15, 0));
  __ shll_cl(Operand(rdi, rax, times_4, 100));
  __ shll_cl(Operand(rdi, rax, times_4, 100));
  __ shll(rdx, Immediate(1));
  __ shll(rdx, Immediate(6));
  __ btq(Operand(rdx, 0), rcx);
  __ btsq(Operand(rdx, 0), rcx);
  __ btsq(Operand(rbx, rcx, times_4, 0), rcx);
  __ btsq(rcx, Immediate(13));
  __ btrq(rcx, Immediate(13));
  __ nop();
  __ pushq(Immediate(12));
  __ pushq(Immediate(23456));
  __ pushq(rcx);
  __ pushq(rsi);
  __ pushq(Operand(rbp, StandardFrameConstants::kFunctionOffset));
  __ pushq(Operand(rbx, rcx, times_4, 0));
  __ pushq(Operand(rbx, rcx, times_4, 0));
  __ pushq(Operand(rbx, rcx, times_4, 10000));
  __ popq(rdx);
  __ popq(rax);
  __ popq(Operand(rbx, rcx, times_4, 0));
  __ nop();

  __ addq(rdx, Operand(rsp, 16));
  __ addq(rdx, rcx);
  __ movb(rdx, Operand(rcx, 0));
  __ movb(rcx, Immediate(6));
  __ movb(Operand(rsp, 16), rdx);
  __ movw(Operand(rsp, 16), rdx);
  __ nop();
  __ movsxwq(rdx, Operand(rsp, 12));
  __ movsxbq(rdx, Operand(rsp, 12));
  __ movsxlq(rdx, Operand(rsp, 12));
  __ movzxwq(rdx, Operand(rsp, 12));
  __ movzxbq(rdx, Operand(rsp, 12));
  __ nop();
  __ movq(rdx, Immediate(1234567));
  __ movq(rdx, Operand(rsp, 12));
  __ movq(Operand(rbx, rcx, times_4, 10000), Immediate(12345));
  __ movq(Operand(rbx, rcx, times_4, 10000), rdx);
  __ nop();
  __ decb(rdx);
  __ decb(Operand(rax, 10));
  __ decb(Operand(rbx, rcx, times_4, 10000));
  __ decq(rdx);
  __ cdq();

  __ repstosq();

  __ nop();
  __ idivq(rdx);
  __ mull(rdx);
  __ mulq(rdx);
  __ negq(rdx);
  __ notq(rdx);
  __ testq(Operand(rbx, rcx, times_4, 10000), rdx);

  __ imulq(rdx, rcx, Immediate(12));
  __ imulq(rdx, rcx, Immediate(1000));
  __ imulq(rdx, Operand(rbx, rcx, times_4, 10000));
  __ imulq(rdx, Operand(rbx, rcx, times_4, 10000), Immediate(12));
  __ imulq(rdx, Operand(rbx, rcx, times_4, 10000), Immediate(1000));
  __ imull(r15, rcx, Immediate(12));
  __ imull(r15, rcx, Immediate(1000));
  __ imull(r15, Operand(rbx, rcx, times_4, 10000));
  __ imull(r15, Operand(rbx, rcx, times_4, 10000), Immediate(12));
  __ imull(r15, Operand(rbx, rcx, times_4, 10000), Immediate(1000));

  __ incq(rdx);
  __ incq(Operand(rbx, rcx, times_4, 10000));
  __ pushq(Operand(rbx, rcx, times_4, 10000));
  __ popq(Operand(rbx, rcx, times_4, 10000));
  __ jmp(Operand(rbx, rcx, times_4, 10000));

  __ leaq(rdx, Operand(rbx, rcx, times_4, 10000));
  __ orq(rdx, Immediate(12345));
  __ orq(rdx, Operand(rbx, rcx, times_4, 10000));

  __ nop();

  __ rclq(rdx, Immediate(1));
  __ rclq(rdx, Immediate(7));
  __ rcrq(rdx, Immediate(1));
  __ rcrq(rdx, Immediate(7));
  __ sarq(rdx, Immediate(1));
  __ sarq(rdx, Immediate(6));
  __ sarq_cl(rdx);
  __ sbbq(rdx, rbx);
  __ shld(rdx, rbx);
  __ shlq(rdx, Immediate(1));
  __ shlq(rdx, Immediate(6));
  __ shlq_cl(rdx);
  __ shrd(rdx, rbx);
  __ shrq(rdx, Immediate(1));
  __ shrq(rdx, Immediate(7));
  __ shrq_cl(rdx);


  // Immediates

  __ addq(rbx, Immediate(12));
  __ addq(Operand(rdx, rcx, times_4, 10000), Immediate(12));

  __ andq(rbx, Immediate(12345));

  __ cmpq(rbx, Immediate(12345));
  __ cmpq(rbx, Immediate(12));
  __ cmpq(Operand(rdx, rcx, times_4, 10000), Immediate(12));
  __ cmpb(rax, Immediate(100));

  __ orq(rbx, Immediate(12345));

  __ subq(rbx, Immediate(12));
  __ subq(Operand(rdx, rcx, times_4, 10000), Immediate(12));

  __ xorq(rbx, Immediate(12345));

  __ imulq(rdx, rcx, Immediate(12));
  __ imulq(rdx, rcx, Immediate(1000));

  __ cld();

  __ subq(rdx, Operand(rbx, rcx, times_4, 10000));
  __ subq(rdx, rbx);

  __ testq(rdx, Immediate(12345));
  __ testq(Operand(rbx, rcx, times_8, 10000), rdx);
  __ testb(Operand(rcx, rbx, times_2, 1000), rdx);
  __ testb(Operand(rax, -20), Immediate(0x9A));
  __ nop();

  __ xorq(rdx, Immediate(12345));
  __ xorq(rdx, Operand(rbx, rcx, times_8, 10000));
  __ hlt();
  __ int3();
  __ ret(0);
  __ ret(8);

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

  // 0xD9 instructions
  __ nop();

  __ fld(1);
  __ fld1();
  __ fldz();
  __ fldpi();
  __ fabs();
  __ fchs();
  __ fprem();
  __ fprem1();
  __ fincstp();
  __ ftst();
  __ fxch(3);
  __ fld_s(Operand(rbx, rcx, times_4, 10000));
  __ fstp_s(Operand(rbx, rcx, times_4, 10000));
  __ ffree(3);
  __ fld_d(Operand(rbx, rcx, times_4, 10000));
  __ fstp_d(Operand(rbx, rcx, times_4, 10000));
  __ nop();

  __ fild_s(Operand(rbx, rcx, times_4, 10000));
  __ fistp_s(Operand(rbx, rcx, times_4, 10000));
  __ fild_d(Operand(rbx, rcx, times_4, 10000));
  __ fistp_d(Operand(rbx, rcx, times_4, 10000));
  __ fnstsw_ax();
  __ nop();
  __ fadd(3);
  __ fsub(3);
  __ fmul(3);
  __ fdiv(3);

  __ faddp(3);
  __ fsubp(3);
  __ fmulp(3);
  __ fdivp(3);
  __ fcompp();
  __ fwait();
  __ frndint();
  __ fninit();
  __ nop();

  // SSE instruction
  {
    // Move operation
    __ cvttss2si(rdx, Operand(rbx, rcx, times_4, 10000));
    __ cvttss2si(rdx, xmm1);
    __ cvtsd2ss(xmm0, xmm1);
    __ cvtsd2ss(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ cvttps2dq(xmm0, xmm1);
    __ cvttps2dq(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ movaps(xmm0, xmm1);
    __ movdqa(xmm0, Operand(rsp, 12));
    __ movdqa(Operand(rsp, 12), xmm0);
    __ movdqu(xmm0, Operand(rsp, 12));
    __ movdqu(Operand(rsp, 12), xmm0);
    __ shufps(xmm0, xmm9, 0x0);

    __ ucomiss(xmm0, xmm1);
    __ ucomiss(xmm0, Operand(rbx, rcx, times_4, 10000));

#define EMIT_SSE_INSTR(instruction, notUsed1, notUsed2) \
  __ instruction(xmm1, xmm0);                           \
  __ instruction(xmm1, Operand(rbx, rcx, times_4, 10000));
    SSE_BINOP_INSTRUCTION_LIST(EMIT_SSE_INSTR)
    SSE_UNOP_INSTRUCTION_LIST(EMIT_SSE_INSTR)
#undef EMIT_SSE_INSTR

#define EMIT_SSE_INSTR(instruction, notUsed1, notUsed2, notUse3) \
  __ instruction(xmm1, xmm0);                                    \
  __ instruction(xmm1, Operand(rbx, rcx, times_4, 10000));
    SSE_INSTRUCTION_LIST_SS(EMIT_SSE_INSTR)
#undef EMIT_SSE_INSTR
  }

  // SSE2 instructions
  {
    __ cvttsd2si(rdx, Operand(rbx, rcx, times_4, 10000));
    __ cvttsd2si(rdx, xmm1);
    __ cvttsd2siq(rdx, xmm1);
    __ cvttsd2siq(rdx, Operand(rbx, rcx, times_4, 10000));
    __ cvtqsi2sd(xmm1, Operand(rbx, rcx, times_4, 10000));
    __ cvtqsi2sd(xmm1, rdx);
    __ movsd(xmm1, Operand(rbx, rcx, times_4, 10000));
    __ movsd(Operand(rbx, rcx, times_4, 10000), xmm1);
    // 128 bit move instructions.
    __ movupd(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ movupd(Operand(rbx, rcx, times_4, 10000), xmm0);
    __ movdqa(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ movdqa(Operand(rbx, rcx, times_4, 10000), xmm0);

    __ ucomisd(xmm0, xmm1);

    __ andpd(xmm0, xmm1);
    __ andpd(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ orpd(xmm0, xmm1);
    __ orpd(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ xorpd(xmm0, xmm1);
    __ xorpd(xmm0, Operand(rbx, rcx, times_4, 10000));

    __ pslld(xmm0, 6);
    __ psrld(xmm0, 6);
    __ psllq(xmm0, 6);
    __ psrlq(xmm0, 6);

    __ pcmpeqd(xmm1, xmm0);

    __ punpckldq(xmm1, xmm11);
    __ punpckldq(xmm5, Operand(rdx, 4));
    __ punpckhdq(xmm8, xmm15);

    __ pshuflw(xmm2, xmm4, 3);
    __ pshufhw(xmm1, xmm9, 6);

#define EMIT_SSE2_INSTR(instruction, notUsed1, notUsed2, notUsed3) \
  __ instruction(xmm5, xmm1);                                      \
  __ instruction(xmm5, Operand(rdx, 4));

    SSE2_INSTRUCTION_LIST(EMIT_SSE2_INSTR)
    SSE2_INSTRUCTION_LIST_SD(EMIT_SSE2_INSTR)
#undef EMIT_SSE2_INSTR
  }

  // cmov.
  {
    __ cmovq(overflow, rax, Operand(rax, 0));
    __ cmovq(no_overflow, rax, Operand(rax, 1));
    __ cmovq(below, rax, Operand(rax, 2));
    __ cmovq(above_equal, rax, Operand(rax, 3));
    __ cmovq(equal, rax, Operand(rbx, 0));
    __ cmovq(not_equal, rax, Operand(rbx, 1));
    __ cmovq(below_equal, rax, Operand(rbx, 2));
    __ cmovq(above, rax, Operand(rbx, 3));
    __ cmovq(sign, rax, Operand(rcx, 0));
    __ cmovq(not_sign, rax, Operand(rcx, 1));
    __ cmovq(parity_even, rax, Operand(rcx, 2));
    __ cmovq(parity_odd, rax, Operand(rcx, 3));
    __ cmovq(less, rax, Operand(rdx, 0));
    __ cmovq(greater_equal, rax, Operand(rdx, 1));
    __ cmovq(less_equal, rax, Operand(rdx, 2));
    __ cmovq(greater, rax, Operand(rdx, 3));
  }

  {
    if (CpuFeatures::IsSupported(SSE3)) {
      CpuFeatureScope scope(&assm, SSE3);
      __ haddps(xmm1, xmm0);
      __ haddps(xmm1, Operand(rbx, rcx, times_4, 10000));
      __ lddqu(xmm1, Operand(rdx, 4));
      __ movddup(xmm1, Operand(rax, 5));
      __ movddup(xmm1, xmm2);
    }
  }

#define EMIT_SSE34_INSTR(instruction, notUsed1, notUsed2, notUsed3, notUsed4) \
  __ instruction(xmm5, xmm1);                                                 \
  __ instruction(xmm5, Operand(rdx, 4));

#define EMIT_SSE34_IMM_INSTR(instruction, notUsed1, notUsed2, notUsed3, \
                             notUsed4)                                  \
  __ instruction(rbx, xmm15, 0);                                        \
  __ instruction(Operand(rax, 10), xmm0, 1);

  {
    if (CpuFeatures::IsSupported(SSSE3)) {
      CpuFeatureScope scope(&assm, SSSE3);
      __ palignr(xmm5, xmm1, 5);
      __ palignr(xmm5, Operand(rdx, 4), 5);
      SSSE3_INSTRUCTION_LIST(EMIT_SSE34_INSTR)
    }
  }

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
      __ cmpneqpd(xmm5, xmm1);
      __ cmpneqpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpnltpd(xmm5, xmm1);
      __ cmpnltpd(xmm5, Operand(rbx, rcx, times_4, 10000));
      __ cmpnlepd(xmm5, xmm1);
      __ cmpnlepd(xmm5, Operand(rbx, rcx, times_4, 10000));

      __ movups(xmm5, xmm1);
      __ movups(xmm5, Operand(rdx, 4));
      __ movups(Operand(rdx, 4), xmm5);
      __ movlhps(xmm5, xmm1);
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

      SSE4_INSTRUCTION_LIST(EMIT_SSE34_INSTR)
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

      __ vmovdqu(xmm9, Operand(rbx, rcx, times_4, 10000));
      __ vmovdqu(Operand(rbx, rcx, times_4, 10000), xmm0);

      __ vaddsd(xmm0, xmm1, xmm2);
      __ vaddsd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vmulsd(xmm0, xmm1, xmm2);
      __ vmulsd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vsubsd(xmm0, xmm1, xmm2);
      __ vsubsd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vdivsd(xmm0, xmm1, xmm2);
      __ vdivsd(xmm0, xmm1, Operand(rbx, rcx, times_2, 10000));
      __ vminsd(xmm8, xmm1, xmm2);
      __ vminsd(xmm9, xmm1, Operand(rbx, rcx, times_8, 10000));
      __ vmaxsd(xmm8, xmm1, xmm2);
      __ vmaxsd(xmm9, xmm1, Operand(rbx, rcx, times_1, 10000));
      __ vroundsd(xmm8, xmm3, xmm0, kRoundDown);
      __ vsqrtsd(xmm8, xmm1, xmm2);
      __ vsqrtsd(xmm9, xmm1, Operand(rbx, rcx, times_1, 10000));
      __ vucomisd(xmm9, xmm1);
      __ vucomisd(xmm8, Operand(rbx, rdx, times_2, 10981));

      __ vcvtss2sd(xmm4, xmm9, xmm11);
      __ vcvtsd2ss(xmm9, xmm3, xmm2);
      __ vcvtss2sd(xmm4, xmm9, Operand(rbx, rcx, times_1, 10000));
      __ vcvtsd2ss(xmm9, xmm3, Operand(rbx, rcx, times_1, 10000));
      __ vcvtlsi2sd(xmm5, xmm9, rcx);
      __ vcvtlsi2sd(xmm9, xmm3, Operand(rbx, r9, times_4, 10000));
      __ vcvtqsi2sd(xmm5, xmm9, r11);
      __ vcvttsd2si(r9, xmm6);
      __ vcvttsd2si(rax, Operand(rbx, r9, times_4, 10000));
      __ vcvttsd2siq(rdi, xmm9);
      __ vcvttsd2siq(r8, Operand(r9, rbx, times_4, 10000));
      __ vcvtsd2si(rdi, xmm9);

      __ vmovaps(xmm10, xmm11);
      __ vmovapd(xmm7, xmm0);
      __ vmovupd(xmm0, Operand(rbx, rcx, times_4, 10000));
      __ vmovupd(Operand(rbx, rcx, times_4, 10000), xmm0);
      __ vmovmskpd(r9, xmm4);

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

      __ vandpd(xmm0, xmm9, xmm2);
      __ vandpd(xmm9, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vorpd(xmm0, xmm1, xmm9);
      __ vorpd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vxorpd(xmm0, xmm1, xmm9);
      __ vxorpd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vpcmpeqd(xmm0, xmm15, xmm5);
      __ vpcmpeqd(xmm15, xmm0, Operand(rbx, rcx, times_4, 10000));
      __ vpsllq(xmm0, xmm15, 21);
      __ vpsrlq(xmm15, xmm0, 21);

      __ vcmpps(xmm5, xmm4, xmm1, 1);
      __ vcmpps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000), 1);
      __ vcmpeqps(xmm5, xmm4, xmm1);
      __ vcmpeqps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpltps(xmm5, xmm4, xmm1);
      __ vcmpltps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpleps(xmm5, xmm4, xmm1);
      __ vcmpleps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpneqps(xmm5, xmm4, xmm1);
      __ vcmpneqps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnltps(xmm5, xmm4, xmm1);
      __ vcmpnltps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnleps(xmm5, xmm4, xmm1);
      __ vcmpnleps(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmppd(xmm5, xmm4, xmm1, 1);
      __ vcmppd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000), 1);
      __ vcmpeqpd(xmm5, xmm4, xmm1);
      __ vcmpeqpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpltpd(xmm5, xmm4, xmm1);
      __ vcmpltpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmplepd(xmm5, xmm4, xmm1);
      __ vcmplepd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpneqpd(xmm5, xmm4, xmm1);
      __ vcmpneqpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnltpd(xmm5, xmm4, xmm1);
      __ vcmpnltpd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));
      __ vcmpnlepd(xmm5, xmm4, xmm1);
      __ vcmpnlepd(xmm5, xmm4, Operand(rbx, rcx, times_4, 10000));

#define EMIT_SSE_UNOP_AVXINSTR(instruction, notUsed1, notUsed2) \
  __ v##instruction(xmm10, xmm1);                               \
  __ v##instruction(xmm10, Operand(rbx, rcx, times_4, 10000));

      SSE_UNOP_INSTRUCTION_LIST(EMIT_SSE_UNOP_AVXINSTR)
#undef EMIT_SSE_UNOP_AVXINSTR

#define EMIT_SSE_BINOP_AVXINSTR(instruction, notUsed1, notUsed2) \
  __ v##instruction(xmm10, xmm5, xmm1);                          \
  __ v##instruction(xmm10, xmm5, Operand(rbx, rcx, times_4, 10000));

      SSE_BINOP_INSTRUCTION_LIST(EMIT_SSE_BINOP_AVXINSTR)
#undef EMIT_SSE_BINOP_AVXINSTR

#define EMIT_SSE2_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3) \
  __ v##instruction(xmm10, xmm5, xmm1);                               \
  __ v##instruction(xmm10, xmm5, Operand(rdx, 4));

#define EMIT_SSE34_AVXINSTR(instruction, notUsed1, notUsed2, notUsed3, \
                            notUsed4)                                  \
  __ v##instruction(xmm10, xmm5, xmm1);                                \
  __ v##instruction(xmm10, xmm5, Operand(rdx, 4));

      SSE2_INSTRUCTION_LIST(EMIT_SSE2_AVXINSTR)
      SSSE3_INSTRUCTION_LIST(EMIT_SSE34_AVXINSTR)
      SSE4_INSTRUCTION_LIST(EMIT_SSE34_AVXINSTR)
#undef EMIT_SSE2_AVXINSTR
#undef EMIT_SSE34_AVXINSTR

      __ vinsertps(xmm1, xmm2, xmm3, 1);
      __ vinsertps(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 1);
      __ vextractps(rax, xmm1, 1);

      __ vlddqu(xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vpsllw(xmm0, xmm15, 21);
      __ vpsrlw(xmm0, xmm15, 21);
      __ vpsraw(xmm0, xmm15, 21);
      __ vpsrad(xmm0, xmm15, 21);
      __ vpextrb(rax, xmm2, 12);
      __ vpextrb(Operand(rbx, rcx, times_4, 10000), xmm2, 12);
      __ vpextrw(rax, xmm2, 5);
      __ vpextrw(Operand(rbx, rcx, times_4, 10000), xmm2, 5);
      __ vpextrd(rax, xmm2, 2);
      __ vpextrd(Operand(rbx, rcx, times_4, 10000), xmm2, 2);

      __ vpinsrb(xmm1, xmm2, rax, 12);
      __ vpinsrb(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 12);
      __ vpinsrw(xmm1, xmm2, rax, 5);
      __ vpinsrw(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 5);
      __ vpinsrd(xmm1, xmm2, rax, 2);
      __ vpinsrd(xmm1, xmm2, Operand(rbx, rcx, times_4, 10000), 2);
      __ vpshufd(xmm1, xmm2, 85);
      __ vpshuflw(xmm1, xmm2, 85);
      __ vpshuflw(xmm1, Operand(rbx, rcx, times_4, 10000), 85);
      __ vshufps(xmm3, xmm2, xmm3, 3);

      __ vmovddup(xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vbroadcastss(xmm1, Operand(rbx, rcx, times_4, 10000));
    }
  }

  // FMA3 instruction
  {
    if (CpuFeatures::IsSupported(FMA3)) {
      CpuFeatureScope scope(&assm, FMA3);
      __ vfmadd132sd(xmm0, xmm1, xmm2);
      __ vfmadd132sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmadd213sd(xmm0, xmm1, xmm2);
      __ vfmadd213sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmadd231sd(xmm0, xmm1, xmm2);
      __ vfmadd231sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfmadd132sd(xmm9, xmm10, xmm11);
      __ vfmadd132sd(xmm9, xmm10, Operand(r9, r11, times_4, 10000));
      __ vfmadd213sd(xmm9, xmm10, xmm11);
      __ vfmadd213sd(xmm9, xmm10, Operand(r9, r11, times_4, 10000));
      __ vfmadd231sd(xmm9, xmm10, xmm11);
      __ vfmadd231sd(xmm9, xmm10, Operand(r9, r11, times_4, 10000));

      __ vfmsub132sd(xmm0, xmm1, xmm2);
      __ vfmsub132sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmsub213sd(xmm0, xmm1, xmm2);
      __ vfmsub213sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmsub231sd(xmm0, xmm1, xmm2);
      __ vfmsub231sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfnmadd132sd(xmm0, xmm1, xmm2);
      __ vfnmadd132sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmadd213sd(xmm0, xmm1, xmm2);
      __ vfnmadd213sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmadd231sd(xmm0, xmm1, xmm2);
      __ vfnmadd231sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfnmsub132sd(xmm0, xmm1, xmm2);
      __ vfnmsub132sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmsub213sd(xmm0, xmm1, xmm2);
      __ vfnmsub213sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmsub231sd(xmm0, xmm1, xmm2);
      __ vfnmsub231sd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfmadd132ss(xmm0, xmm1, xmm2);
      __ vfmadd132ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmadd213ss(xmm0, xmm1, xmm2);
      __ vfmadd213ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmadd231ss(xmm0, xmm1, xmm2);
      __ vfmadd231ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfmsub132ss(xmm0, xmm1, xmm2);
      __ vfmsub132ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmsub213ss(xmm0, xmm1, xmm2);
      __ vfmsub213ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmsub231ss(xmm0, xmm1, xmm2);
      __ vfmsub231ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfnmadd132ss(xmm0, xmm1, xmm2);
      __ vfnmadd132ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmadd213ss(xmm0, xmm1, xmm2);
      __ vfnmadd213ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmadd231ss(xmm0, xmm1, xmm2);
      __ vfnmadd231ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfnmsub132ss(xmm0, xmm1, xmm2);
      __ vfnmsub132ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmsub213ss(xmm0, xmm1, xmm2);
      __ vfnmsub213ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmsub231ss(xmm0, xmm1, xmm2);
      __ vfnmsub231ss(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));

      __ vfmadd231ps(xmm0, xmm1, xmm2);
      __ vfmadd231ps(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmadd231ps(xmm0, xmm1, xmm2);
      __ vfnmadd231ps(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfmadd231pd(xmm0, xmm1, xmm2);
      __ vfmadd231pd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
      __ vfnmadd231pd(xmm0, xmm1, xmm2);
      __ vfnmadd231pd(xmm0, xmm1, Operand(rbx, rcx, times_4, 10000));
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
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();
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

#undef __

}  // namespace internal
}  // namespace v8
