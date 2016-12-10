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

#include "src/v8.h"

#include "src/code-stubs.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/objects.h"
#include "test/cctest/cctest.h"

#ifdef USE_SIMULATOR
#include "src/simulator.h"
#endif

using namespace v8::internal;


typedef uint32_t (*HASH_FUNCTION)();

#define __ masm->


void generate(MacroAssembler* masm, uint32_t key) {
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87
  __ push(ebx);
  __ mov(eax, Immediate(key));
  __ GetNumberHash(eax, ebx);
  __ pop(ebx);
  __ Ret();
#elif V8_TARGET_ARCH_X64
  __ pushq(kRootRegister);
  __ InitializeRootRegister();
  __ pushq(rbx);
  __ movp(rax, Immediate(key));
  __ GetNumberHash(rax, rbx);
  __ popq(rbx);
  __ popq(kRootRegister);
  __ Ret();
#elif V8_TARGET_ARCH_ARM
  __ push(kRootRegister);
  __ InitializeRootRegister();
  __ mov(r0, Operand(key));
  __ GetNumberHash(r0, ip);
  __ pop(kRootRegister);
  __ mov(pc, Operand(lr));
#elif V8_TARGET_ARCH_ARM64
  // The ARM64 assembler usually uses jssp (x28) as a stack pointer, but only
  // csp is initialized by the calling (C++) code.
  Register old_stack_pointer = __ StackPointer();
  __ SetStackPointer(csp);
  __ Push(root, xzr);
  __ InitializeRootRegister();
  __ Mov(x0, key);
  __ GetNumberHash(x0, x10);
  __ Pop(xzr, root);
  __ Ret();
  __ SetStackPointer(old_stack_pointer);
#elif V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
  __ push(kRootRegister);
  __ InitializeRootRegister();
  __ li(v0, Operand(key));
  __ GetNumberHash(v0, t1);
  __ pop(kRootRegister);
  __ jr(ra);
  __ nop();
#elif V8_TARGET_ARCH_S390
  __ push(kRootRegister);
  __ push(ip);
  __ InitializeRootRegister();
  __ lhi(r2, Operand(key));
  __ GetNumberHash(r2, ip);
  __ pop(ip);
  __ pop(kRootRegister);
  __ Ret();
#elif V8_TARGET_ARCH_PPC
  __ function_descriptor();
  __ push(kRootRegister);
  __ InitializeRootRegister();
  __ li(r3, Operand(key));
  __ GetNumberHash(r3, ip);
  __ pop(kRootRegister);
  __ blr();
#else
#error Unsupported architecture.
#endif
}


void check(uint32_t key) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  v8::internal::byte buffer[2048];
  MacroAssembler masm(CcTest::i_isolate(), buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);

  generate(&masm, key);

  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Object> undefined(isolate->heap()->undefined_value(), isolate);
  Handle<Code> code = factory->NewCode(desc,
                                       Code::ComputeFlags(Code::STUB),
                                       undefined);
  CHECK(code->IsCode());

  HASH_FUNCTION hash = FUNCTION_CAST<HASH_FUNCTION>(code->entry());
#ifdef USE_SIMULATOR
  uint32_t codegen_hash = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
      CALL_GENERATED_CODE(isolate, hash, 0, 0, 0, 0, 0)));
#else
  uint32_t codegen_hash = hash();
#endif

  uint32_t runtime_hash = ComputeIntegerHash(key, isolate->heap()->HashSeed());
  CHECK_EQ(runtime_hash, codegen_hash);
}


static uint32_t PseudoRandom(uint32_t i, uint32_t j) {
  return ~(~((i * 781) ^ (j * 329)));
}


TEST(NumberHash) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(v8::Context::New(isolate));

  // Some specific numbers
  for (uint32_t key = 0; key < 42; key += 7) {
    check(key);
  }

  // Some pseudo-random numbers
  static const uint32_t kLimit = 1000;
  for (uint32_t i = 0; i < 5; i++) {
    for (uint32_t j = 0; j < 5; j++) {
      check(PseudoRandom(i, j) % kLimit);
    }
  }
}

#undef __
