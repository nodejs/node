// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}


#define __ assm.


static void DummyStaticFunction(Object* result) {
}


TEST(DisasmIa320) {
  InitializeVM();
  v8::HandleScope scope;
  v8::internal::byte buffer[2048];
  Assembler assm(buffer, sizeof buffer);
  DummyStaticFunction(NULL);  // just bloody use it (DELETE; debugging)

  // Short immediate instructions
  __ adc(eax, 12345678);
  __ add(Operand(eax), Immediate(12345678));
  __ or_(eax, 12345678);
  __ sub(Operand(eax), Immediate(12345678));
  __ xor_(eax, 12345678);
  __ and_(eax, 12345678);
  Handle<FixedArray> foo = Factory::NewFixedArray(10, TENURED);
  __ cmp(eax, foo);

  // ---- This one caused crash
  __ mov(ebx,  Operand(esp, ecx, times_2, 0));  // [esp+ecx*4]

  // ---- All instructions that I can think of
  __ add(edx, Operand(ebx));
  __ add(edx, Operand(12, RelocInfo::NONE));
  __ add(edx, Operand(ebx, 0));
  __ add(edx, Operand(ebx, 16));
  __ add(edx, Operand(ebx, 1999));
  __ add(edx, Operand(esp, 0));
  __ add(edx, Operand(esp, 16));
  __ add(edx, Operand(esp, 1999));
  __ nop();
  __ add(edi, Operand(ebp, ecx, times_4, 0));
  __ add(edi, Operand(ebp, ecx, times_4, 12));
  __ add(Operand(ebp, ecx, times_4, 12), Immediate(12));

  __ nop();
  __ add(Operand(ebx), Immediate(12));
  __ nop();
  __ adc(ecx, 12);
  __ adc(ecx, 1000);
  __ nop();
  __ and_(edx, 3);
  __ and_(edx, Operand(esp, 4));
  __ cmp(edx, 3);
  __ cmp(edx, Operand(esp, 4));
  __ cmp(Operand(ebp, ecx, times_4, 0), Immediate(1000));
  Handle<FixedArray> foo2 = Factory::NewFixedArray(10, TENURED);
  __ cmp(ebx, foo2);
  __ cmpb(ebx, Operand(ebp, ecx, times_2, 0));
  __ cmpb(Operand(ebp, ecx, times_2, 0), ebx);
  __ or_(edx, 3);
  __ xor_(edx, 3);
  __ nop();
  {
    CHECK(CpuFeatures::IsSupported(CPUID));
    CpuFeatures::Scope fscope(CPUID);
    __ cpuid();
  }
  {
    CHECK(CpuFeatures::IsSupported(RDTSC));
    CpuFeatures::Scope fscope(RDTSC);
    __ rdtsc();
  }
  __ movsx_b(edx, Operand(ecx));
  __ movsx_w(edx, Operand(ecx));
  __ movzx_b(edx, Operand(ecx));
  __ movzx_w(edx, Operand(ecx));

  __ nop();
  __ imul(edx, Operand(ecx));
  __ shld(edx, Operand(ecx));
  __ shrd(edx, Operand(ecx));
  __ bts(Operand(edx), ecx);
  __ bts(Operand(ebx, ecx, times_4, 0), ecx);
  __ nop();
  __ pushad();
  __ popad();
  __ pushfd();
  __ popfd();
  __ push(Immediate(12));
  __ push(Immediate(23456));
  __ push(ecx);
  __ push(esi);
  __ push(Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(Operand(ebx, ecx, times_4, 0));
  __ push(Operand(ebx, ecx, times_4, 0));
  __ push(Operand(ebx, ecx, times_4, 10000));
  __ pop(edx);
  __ pop(eax);
  __ pop(Operand(ebx, ecx, times_4, 0));
  __ nop();

  __ add(edx, Operand(esp, 16));
  __ add(edx, Operand(ecx));
  __ mov_b(edx, Operand(ecx));
  __ mov_b(Operand(ecx), 6);
  __ mov_b(Operand(ebx, ecx, times_4, 10000), 6);
  __ mov_b(Operand(esp, 16), edx);
  __ mov_w(edx, Operand(esp, 16));
  __ mov_w(Operand(esp, 16), edx);
  __ nop();
  __ movsx_w(edx, Operand(esp, 12));
  __ movsx_b(edx, Operand(esp, 12));
  __ movzx_w(edx, Operand(esp, 12));
  __ movzx_b(edx, Operand(esp, 12));
  __ nop();
  __ mov(edx, 1234567);
  __ mov(edx, Operand(esp, 12));
  __ mov(Operand(ebx, ecx, times_4, 10000), Immediate(12345));
  __ mov(Operand(ebx, ecx, times_4, 10000), edx);
  __ nop();
  __ dec_b(edx);
  __ dec(edx);
  __ cdq();

  __ nop();
  __ idiv(edx);
  __ mul(edx);
  __ neg(edx);
  __ not_(edx);
  __ test(Operand(ebx, ecx, times_4, 10000), Immediate(123456));

  __ imul(edx, Operand(ebx, ecx, times_4, 10000));
  __ imul(edx, ecx, 12);
  __ imul(edx, ecx, 1000);

  __ inc(edx);
  __ inc(Operand(ebx, ecx, times_4, 10000));
  __ push(Operand(ebx, ecx, times_4, 10000));
  __ pop(Operand(ebx, ecx, times_4, 10000));
  __ call(Operand(ebx, ecx, times_4, 10000));
  __ jmp(Operand(ebx, ecx, times_4, 10000));

  __ lea(edx, Operand(ebx, ecx, times_4, 10000));
  __ or_(edx, 12345);
  __ or_(edx, Operand(ebx, ecx, times_4, 10000));

  __ nop();

  __ rcl(edx, 1);
  __ rcl(edx, 7);
  __ sar(edx, 1);
  __ sar(edx, 6);
  __ sar_cl(edx);
  __ sbb(edx, Operand(ebx, ecx, times_4, 10000));
  __ shld(edx, Operand(ebx, ecx, times_4, 10000));
  __ shl(edx, 1);
  __ shl(edx, 6);
  __ shl_cl(edx);
  __ shrd(edx, Operand(ebx, ecx, times_4, 10000));
  __ shr(edx, 1);
  __ shr(edx, 7);
  __ shr_cl(edx);


  // Immediates

  __ adc(edx, 12345);

  __ add(Operand(ebx), Immediate(12));
  __ add(Operand(edx, ecx, times_4, 10000), Immediate(12));

  __ and_(ebx, 12345);

  __ cmp(ebx, 12345);
  __ cmp(Operand(ebx), Immediate(12));
  __ cmp(Operand(edx, ecx, times_4, 10000), Immediate(12));

  __ or_(ebx, 12345);

  __ sub(Operand(ebx), Immediate(12));
  __ sub(Operand(edx, ecx, times_4, 10000), Immediate(12));
  __ subb(Operand(edx, ecx, times_4, 10000), 100);
  __ subb(Operand(eax), 100);
  __ subb(eax, Operand(edx, ecx, times_4, 10000));

  __ xor_(ebx, 12345);

  __ imul(edx, ecx, 12);
  __ imul(edx, ecx, 1000);

  __ cld();
  __ rep_movs();
  __ rep_stos();
  __ stos();

  __ sub(edx, Operand(ebx, ecx, times_4, 10000));
  __ sub(edx, Operand(ebx));

  __ test(edx, Immediate(12345));
  __ test(edx, Operand(ebx, ecx, times_8, 10000));
  __ nop();

  __ xor_(edx, 12345);
  __ xor_(edx, Operand(ebx, ecx, times_8, 10000));
  __ bts(Operand(ebx, ecx, times_8, 10000), edx);
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
  __ call(Operand(ebx, ecx, times_4, 10000));
  __ nop();
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  __ call(ic, RelocInfo::CODE_TARGET);
  __ nop();
  __ call(FUNCTION_ADDR(DummyStaticFunction), RelocInfo::RUNTIME_ENTRY);
  __ nop();

  __ jmp(&L1);
  __ jmp(Operand(ebx, ecx, times_4, 10000));
  ExternalReference after_break_target =
      ExternalReference(Debug_Address::AfterBreakTarget());
  __ jmp(Operand::StaticVariable(after_break_target));
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

  // checking hints
  __ j(zero, &Ljcc, taken);
  __ j(zero, &Ljcc, not_taken);

  // __ mov(Operand::StaticVariable(Top::handler_address()), eax);
  // 0xD9 instructions
  __ nop();

  __ fld1();
  __ fldz();
  __ fabs();
  __ fchs();
  __ fprem();
  __ fprem1();
  __ fincstp();
  __ ftst();
  __ fxch(3);
  __ fld_s(Operand(ebx, ecx, times_4, 10000));
  __ fstp_s(Operand(ebx, ecx, times_4, 10000));
  __ ffree(3);
  __ fld_d(Operand(ebx, ecx, times_4, 10000));
  __ fstp_d(Operand(ebx, ecx, times_4, 10000));
  __ nop();

  __ fild_s(Operand(ebx, ecx, times_4, 10000));
  __ fistp_s(Operand(ebx, ecx, times_4, 10000));
  __ fild_d(Operand(ebx, ecx, times_4, 10000));
  __ fistp_d(Operand(ebx, ecx, times_4, 10000));
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
  __ nop();
  {
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatures::Scope fscope(SSE2);
      __ cvttss2si(edx, Operand(ebx, ecx, times_4, 10000));
      __ cvtsi2sd(xmm1, Operand(ebx, ecx, times_4, 10000));
      __ addsd(xmm1, xmm0);
      __ mulsd(xmm1, xmm0);
      __ subsd(xmm1, xmm0);
      __ divsd(xmm1, xmm0);
      __ movdbl(xmm1, Operand(ebx, ecx, times_4, 10000));
      __ movdbl(Operand(ebx, ecx, times_4, 10000), xmm1);
      __ comisd(xmm0, xmm1);

      // 128 bit move instructions.
      __ movdqa(xmm0, Operand(ebx, ecx, times_4, 10000));
      __ movdqa(Operand(ebx, ecx, times_4, 10000), xmm0);
      __ movdqu(xmm0, Operand(ebx, ecx, times_4, 10000));
      __ movdqu(Operand(ebx, ecx, times_4, 10000), xmm0);
    }
  }

  // cmov.
  {
    if (CpuFeatures::IsSupported(CMOV)) {
      CpuFeatures::Scope use_cmov(CMOV);
      __ cmov(overflow, eax, Operand(eax, 0));
      __ cmov(no_overflow, eax, Operand(eax, 1));
      __ cmov(below, eax, Operand(eax, 2));
      __ cmov(above_equal, eax, Operand(eax, 3));
      __ cmov(equal, eax, Operand(ebx, 0));
      __ cmov(not_equal, eax, Operand(ebx, 1));
      __ cmov(below_equal, eax, Operand(ebx, 2));
      __ cmov(above, eax, Operand(ebx, 3));
      __ cmov(sign, eax, Operand(ecx, 0));
      __ cmov(not_sign, eax, Operand(ecx, 1));
      __ cmov(parity_even, eax, Operand(ecx, 2));
      __ cmov(parity_odd, eax, Operand(ecx, 3));
      __ cmov(less, eax, Operand(edx, 0));
      __ cmov(greater_equal, eax, Operand(edx, 1));
      __ cmov(less_equal, eax, Operand(edx, 2));
      __ cmov(greater, eax, Operand(edx, 3));
    }
  }

  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
  byte* begin = Code::cast(code)->instruction_start();
  byte* end = begin + Code::cast(code)->instruction_size();
  disasm::Disassembler::Disassemble(stdout, begin, end);
#endif
}

#undef __
