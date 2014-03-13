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

#include "v8.h"

#include "debug.h"
#include "disasm.h"
#include "disassembler.h"
#include "macro-assembler.h"
#include "serialize.h"
#include "cctest.h"

using namespace v8::internal;


#define __ assm.


static void DummyStaticFunction(Object* result) {
}


TEST(DisasmX64) {
  CcTest::InitializeVM();
  v8::HandleScope scope;
  v8::internal::byte buffer[2048];
  Assembler assm(CcTest::i_isolate(), buffer, sizeof buffer);
  DummyStaticFunction(NULL);  // just bloody use it (DELETE; debugging)

  // Short immediate instructions
  __ addq(rax, Immediate(12345678));
  __ or_(rax, Immediate(12345678));
  __ subq(rax, Immediate(12345678));
  __ xor_(rax, Immediate(12345678));
  __ and_(rax, Immediate(12345678));

  // ---- This one caused crash
  __ movq(rbx,  Operand(rsp, rcx, times_2, 0));  // [rsp+rcx*4]

  // ---- All instructions that I can think of
  __ addq(rdx, rbx);
  __ addq(rdx, Operand(rbx, 0));
  __ addq(rdx, Operand(rbx, 16));
  __ addq(rdx, Operand(rbx, 1999));
  __ addq(rdx, Operand(rsp, 0));
  __ addq(rdx, Operand(rsp, 16));
  __ addq(rdx, Operand(rsp, 1999));
  __ nop();
  __ addq(rdi, Operand(rbp, rcx, times_4, 0));
  __ addq(rdi, Operand(rbp, rcx, times_4, 12));
  __ addq(Operand(rbp, rcx, times_4, 12), Immediate(12));

  __ nop();
  __ addq(rbx, Immediate(12));
  __ nop();
  __ nop();
  __ and_(rdx, Immediate(3));
  __ and_(rdx, Operand(rsp, 4));
  __ cmpq(rdx, Immediate(3));
  __ cmpq(rdx, Operand(rsp, 4));
  __ cmpq(Operand(rbp, rcx, times_4, 0), Immediate(1000));
  __ cmpb(rbx, Operand(rbp, rcx, times_2, 0));
  __ cmpb(Operand(rbp, rcx, times_2, 0), rbx);
  __ or_(rdx, Immediate(3));
  __ xor_(rdx, Immediate(3));
  __ nop();
  __ cpuid();
  __ movsxbq(rdx, Operand(rcx, 0));
  __ movsxwq(rdx, Operand(rcx, 0));
  __ movzxbl(rdx, Operand(rcx, 0));
  __ movzxwl(rdx, Operand(rcx, 0));
  __ movzxbq(rdx, Operand(rcx, 0));
  __ movzxwq(rdx, Operand(rcx, 0));

  __ nop();
  __ imul(rdx, rcx);
  __ shld(rdx, rcx);
  __ shrd(rdx, rcx);
  __ bts(Operand(rdx, 0), rcx);
  __ bts(Operand(rbx, rcx, times_4, 0), rcx);
  __ nop();
  __ push(Immediate(12));
  __ push(Immediate(23456));
  __ push(rcx);
  __ push(rsi);
  __ push(Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(Operand(rbx, rcx, times_4, 0));
  __ push(Operand(rbx, rcx, times_4, 0));
  __ push(Operand(rbx, rcx, times_4, 10000));
  __ pop(rdx);
  __ pop(rax);
  __ pop(Operand(rbx, rcx, times_4, 0));
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

  __ nop();
  __ idivq(rdx);
  __ mul(rdx);
  __ neg(rdx);
  __ not_(rdx);
  __ testq(Operand(rbx, rcx, times_4, 10000), rdx);

  __ imul(rdx, Operand(rbx, rcx, times_4, 10000));
  __ imul(rdx, rcx, Immediate(12));
  __ imul(rdx, rcx, Immediate(1000));

  __ incq(rdx);
  __ incq(Operand(rbx, rcx, times_4, 10000));
  __ push(Operand(rbx, rcx, times_4, 10000));
  __ pop(Operand(rbx, rcx, times_4, 10000));
  __ jmp(Operand(rbx, rcx, times_4, 10000));

  __ lea(rdx, Operand(rbx, rcx, times_4, 10000));
  __ or_(rdx, Immediate(12345));
  __ or_(rdx, Operand(rbx, rcx, times_4, 10000));

  __ nop();

  __ rcl(rdx, Immediate(1));
  __ rcl(rdx, Immediate(7));
  __ rcr(rdx, Immediate(1));
  __ rcr(rdx, Immediate(7));
  __ sar(rdx, Immediate(1));
  __ sar(rdx, Immediate(6));
  __ sar_cl(rdx);
  __ sbbq(rdx, rbx);
  __ shld(rdx, rbx);
  __ shl(rdx, Immediate(1));
  __ shl(rdx, Immediate(6));
  __ shl_cl(rdx);
  __ shrd(rdx, rbx);
  __ shr(rdx, Immediate(1));
  __ shr(rdx, Immediate(7));
  __ shr_cl(rdx);


  // Immediates

  __ addq(rbx, Immediate(12));
  __ addq(Operand(rdx, rcx, times_4, 10000), Immediate(12));

  __ and_(rbx, Immediate(12345));

  __ cmpq(rbx, Immediate(12345));
  __ cmpq(rbx, Immediate(12));
  __ cmpq(Operand(rdx, rcx, times_4, 10000), Immediate(12));
  __ cmpb(rax, Immediate(100));

  __ or_(rbx, Immediate(12345));

  __ subq(rbx, Immediate(12));
  __ subq(Operand(rdx, rcx, times_4, 10000), Immediate(12));

  __ xor_(rbx, Immediate(12345));

  __ imul(rdx, rcx, Immediate(12));
  __ imul(rdx, rcx, Immediate(1000));

  __ cld();

  __ subq(rdx, Operand(rbx, rcx, times_4, 10000));
  __ subq(rdx, rbx);

  __ testq(rdx, Immediate(12345));
  __ testq(Operand(rbx, rcx, times_8, 10000), rdx);
  __ testb(Operand(rcx, rbx, times_2, 1000), rdx);
  __ testb(Operand(rax, -20), Immediate(0x9A));
  __ nop();

  __ xor_(rdx, Immediate(12345));
  __ xor_(rdx, Operand(rbx, rcx, times_8, 10000));
  __ bts(Operand(rbx, rcx, times_8, 10000), rdx);
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
  __ call(Operand(rbx, rcx, times_4, 10000));
  __ nop();
  Handle<Code> ic(CcTest::i_isolate()->builtins()->builtin(
      Builtins::kLoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  __ nop();
  __ nop();

  __ jmp(&L1);
  __ jmp(Operand(rbx, rcx, times_4, 10000));
#ifdef ENABLE_DEBUGGER_SUPPORT
  ExternalReference after_break_target =
      ExternalReference(Debug_Address::AfterBreakTarget(),
                        assm.isolate());
  USE(after_break_target);
#endif  // ENABLE_DEBUGGER_SUPPORT
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
    __ movaps(xmm0, xmm1);

    // logic operation
    __ andps(xmm0, xmm1);
    __ andps(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ orps(xmm0, xmm1);
    __ ordps(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ xorps(xmm0, xmm1);
    __ xordps(xmm0, Operand(rbx, rcx, times_4, 10000));

    // Arithmetic operation
    __ addps(xmm1, xmm0);
    __ addps(xmm1, Operand(rbx, rcx, times_4, 10000));
    __ subps(xmm1, xmm0);
    __ subps(xmm1, Operand(rbx, rcx, times_4, 10000));
    __ mulps(xmm1, xmm0);
    __ mulps(xmm1, Operand(rbx, ecx, times_4, 10000));
    __ divps(xmm1, xmm0);
    __ divps(xmm1, Operand(rbx, rcx, times_4, 10000));
  }
  // SSE 2 instructions
  {
    __ cvttsd2si(rdx, Operand(rbx, rcx, times_4, 10000));
    __ cvttsd2si(rdx, xmm1);
    __ cvttsd2siq(rdx, xmm1);
    __ movsd(xmm1, Operand(rbx, rcx, times_4, 10000));
    __ movsd(Operand(rbx, rcx, times_4, 10000), xmm1);
    // 128 bit move instructions.
    __ movdqa(xmm0, Operand(rbx, rcx, times_4, 10000));
    __ movdqa(Operand(rbx, rcx, times_4, 10000), xmm0);

    __ addsd(xmm1, xmm0);
    __ mulsd(xmm1, xmm0);
    __ subsd(xmm1, xmm0);
    __ divsd(xmm1, xmm0);
    __ ucomisd(xmm0, xmm1);

    __ andpd(xmm0, xmm1);
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
    if (CpuFeatures::IsSupported(SSE4_1)) {
      CpuFeatureScope scope(&assm, SSE4_1);
      __ extractps(rax, xmm1, 0);
    }
  }

  // Nop instructions
  for (int i = 0; i < 16; i++) {
    __ Nop(i);
  }

  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = CcTest::heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef OBJECT_PRINT
  Code::cast(code)->Print();
  byte* begin = Code::cast(code)->instruction_start();
  byte* end = begin + Code::cast(code)->instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif
}

#undef __
