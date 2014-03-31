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

#include "factory.h"
#include "macro-assembler.h"
#include "cctest.h"
#include "code-stubs.h"
#include "objects.h"

#ifdef USE_SIMULATOR
#include "simulator.h"
#endif

using namespace v8::internal;


typedef uint32_t (*HASH_FUNCTION)();

#define __ masm->


void generate(MacroAssembler* masm, i::Vector<const uint8_t> string) {
  // GenerateHashInit takes the first character as an argument so it can't
  // handle the zero length string.
  ASSERT(string.length() > 0);
#if V8_TARGET_ARCH_IA32
  __ push(ebx);
  __ push(ecx);
  __ mov(eax, Immediate(0));
  __ mov(ebx, Immediate(string.at(0)));
  StringHelper::GenerateHashInit(masm, eax, ebx, ecx);
  for (int i = 1; i < string.length(); i++) {
    __ mov(ebx, Immediate(string.at(i)));
    StringHelper::GenerateHashAddCharacter(masm, eax, ebx, ecx);
  }
  StringHelper::GenerateHashGetHash(masm, eax, ecx);
  __ pop(ecx);
  __ pop(ebx);
  __ Ret();
#elif V8_TARGET_ARCH_X64
  __ pushq(kRootRegister);
  __ InitializeRootRegister();
  __ pushq(rbx);
  __ pushq(rcx);
  __ movp(rax, Immediate(0));
  __ movp(rbx, Immediate(string.at(0)));
  StringHelper::GenerateHashInit(masm, rax, rbx, rcx);
  for (int i = 1; i < string.length(); i++) {
    __ movp(rbx, Immediate(string.at(i)));
    StringHelper::GenerateHashAddCharacter(masm, rax, rbx, rcx);
  }
  StringHelper::GenerateHashGetHash(masm, rax, rcx);
  __ popq(rcx);
  __ popq(rbx);
  __ popq(kRootRegister);
  __ Ret();
#elif V8_TARGET_ARCH_ARM
  __ push(kRootRegister);
  __ InitializeRootRegister();

  __ mov(r0, Operand(0));
  __ mov(ip, Operand(string.at(0)));
  StringHelper::GenerateHashInit(masm, r0, ip);
  for (int i = 1; i < string.length(); i++) {
    __ mov(ip, Operand(string.at(i)));
    StringHelper::GenerateHashAddCharacter(masm, r0, ip);
  }
  StringHelper::GenerateHashGetHash(masm, r0);
  __ pop(kRootRegister);
  __ mov(pc, Operand(lr));
#elif V8_TARGET_ARCH_ARM64
  // The ARM64 assembler usually uses jssp (x28) as a stack pointer, but only
  // csp is initialized by the calling (C++) code.
  Register old_stack_pointer = __ StackPointer();
  __ SetStackPointer(csp);
  __ Push(root, xzr);
  __ InitializeRootRegister();
  __ Mov(x0, 0);
  __ Mov(x10, Operand(string.at(0)));
  StringHelper::GenerateHashInit(masm, x0, x10);
  for (int i = 1; i < string.length(); i++) {
    __ Mov(x10, Operand(string.at(i)));
    StringHelper::GenerateHashAddCharacter(masm, x0, x10);
  }
  StringHelper::GenerateHashGetHash(masm, x0, x10);
  __ Pop(xzr, root);
  __ Ret();
  __ SetStackPointer(old_stack_pointer);
#elif V8_TARGET_ARCH_MIPS
  __ push(kRootRegister);
  __ InitializeRootRegister();

  __ li(v0, Operand(0));
  __ li(t1, Operand(string.at(0)));
  StringHelper::GenerateHashInit(masm, v0, t1);
  for (int i = 1; i < string.length(); i++) {
    __ li(t1, Operand(string.at(i)));
    StringHelper::GenerateHashAddCharacter(masm, v0, t1);
  }
  StringHelper::GenerateHashGetHash(masm, v0);
  __ pop(kRootRegister);
  __ jr(ra);
  __ nop();
#else
#error Unsupported architecture.
#endif
}


void generate(MacroAssembler* masm, uint32_t key) {
#if V8_TARGET_ARCH_IA32
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
#elif V8_TARGET_ARCH_MIPS
  __ push(kRootRegister);
  __ InitializeRootRegister();
  __ li(v0, Operand(key));
  __ GetNumberHash(v0, t1);
  __ pop(kRootRegister);
  __ jr(ra);
  __ nop();
#else
#error Unsupported architecture.
#endif
}


void check(i::Vector<const uint8_t> string) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  v8::internal::byte buffer[2048];
  MacroAssembler masm(isolate, buffer, sizeof buffer);

  generate(&masm, string);

  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Object> undefined(isolate->heap()->undefined_value(), isolate);
  Handle<Code> code = factory->NewCode(desc,
                                       Code::ComputeFlags(Code::STUB),
                                       undefined);
  CHECK(code->IsCode());

  HASH_FUNCTION hash = FUNCTION_CAST<HASH_FUNCTION>(code->entry());
  Handle<String> v8_string = factory->NewStringFromOneByte(string);
  v8_string->set_hash_field(String::kEmptyHashField);
#ifdef USE_SIMULATOR
  uint32_t codegen_hash = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(CALL_GENERATED_CODE(hash, 0, 0, 0, 0, 0)));
#else
  uint32_t codegen_hash = hash();
#endif
  uint32_t runtime_hash = v8_string->Hash();
  CHECK(runtime_hash == codegen_hash);
}


void check(i::Vector<const char> s) {
  check(i::Vector<const uint8_t>::cast(s));
}


void check(uint32_t key) {
  Isolate* isolate = CcTest::i_isolate();
  Factory* factory = isolate->factory();
  HandleScope scope(isolate);

  v8::internal::byte buffer[2048];
  MacroAssembler masm(CcTest::i_isolate(), buffer, sizeof buffer);

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
  uint32_t codegen_hash = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(CALL_GENERATED_CODE(hash, 0, 0, 0, 0, 0)));
#else
  uint32_t codegen_hash = hash();
#endif

  uint32_t runtime_hash = ComputeIntegerHash(key, isolate->heap()->HashSeed());
  CHECK(runtime_hash == codegen_hash);
}


void check_twochars(uint8_t a, uint8_t b) {
  uint8_t ab[2] = {a, b};
  check(i::Vector<const uint8_t>(ab, 2));
}


static uint32_t PseudoRandom(uint32_t i, uint32_t j) {
  return ~(~((i * 781) ^ (j * 329)));
}


TEST(StringHash) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(v8::Context::New(isolate));

  for (uint8_t a = 0; a < String::kMaxOneByteCharCode; a++) {
    // Numbers are hashed differently.
    if (a >= '0' && a <= '9') continue;
    for (uint8_t b = 0; b < String::kMaxOneByteCharCode; b++) {
      if (b >= '0' && b <= '9') continue;
      check_twochars(a, b);
    }
  }
  check(i::Vector<const char>("*",       1));
  check(i::Vector<const char>(".zZ",     3));
  check(i::Vector<const char>("muc",     3));
  check(i::Vector<const char>("(>'_')>", 7));
  check(i::Vector<const char>("-=[ vee eight ftw ]=-", 21));
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
