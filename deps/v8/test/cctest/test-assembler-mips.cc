// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "disassembler.h"
#include "factory.h"
#include "macro-assembler.h"
#include "mips/macro-assembler-mips.h"
#include "mips/simulator-mips.h"

#include "cctest.h"

using namespace v8::internal;


// Define these function prototypes to match JSEntryFunction in execution.cc.
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F2)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F3)(void* p, int p1, int p2, int p3, int p4);


static v8::Persistent<v8::Context> env;


// The test framework does not accept flags on the command line, so we set them.
static void InitializeVM() {
  // Disable compilation of natives.
  FLAG_disable_native_files = true;

  // Enable generation of comments.
  FLAG_debug_code = true;

  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}


#define __ assm.

TEST(MIPS0) {
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);

  // Addition.
  __ addu(v0, a0, a1);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 0xab0, 0xc, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(0xabc, res);
}


TEST(MIPS1) {
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);
  Label L, C;

  __ mov(a1, a0);
  __ li(v0, 0);
  __ b(&C);
  __ nop();

  __ bind(&L);
  __ add(v0, v0, a1);
  __ addiu(a1, a1, -1);

  __ bind(&C);
  __ xori(v1, a1, 0);
  __ Branch(ne, &L, v1, Operand(0, RelocInfo::NONE));
  __ nop();

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 50, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(1275, res);
}


TEST(MIPS2) {
  InitializeVM();
  v8::HandleScope scope;

  MacroAssembler assm(NULL, 0);

  Label exit, error;

  // ----- Test all instructions.

  // Test lui, ori, and addiu, used in the li pseudo-instruction.
  // This way we can then safely load registers with chosen values.

  __ ori(t0, zero_reg, 0);
  __ lui(t0, 0x1234);
  __ ori(t0, t0, 0);
  __ ori(t0, t0, 0x0f0f);
  __ ori(t0, t0, 0xf0f0);
  __ addiu(t1, t0, 1);
  __ addiu(t2, t1, -0x10);

  // Load values in temporary registers.
  __ li(t0, 0x00000004);
  __ li(t1, 0x00001234);
  __ li(t2, 0x12345678);
  __ li(t3, 0x7fffffff);
  __ li(t4, 0xfffffffc);
  __ li(t5, 0xffffedcc);
  __ li(t6, 0xedcba988);
  __ li(t7, 0x80000000);

  // SPECIAL class.
  __ srl(v0, t2, 8);    // 0x00123456
  __ sll(v0, v0, 11);   // 0x91a2b000
  __ sra(v0, v0, 3);    // 0xf2345600
  __ srav(v0, v0, t0);  // 0xff234560
  __ sllv(v0, v0, t0);  // 0xf2345600
  __ srlv(v0, v0, t0);  // 0x0f234560
  __ Branch(ne, &error, v0, Operand(0x0f234560));
  __ nop();

  __ add(v0, t0, t1);   // 0x00001238
  __ sub(v0, v0, t0);   // 0x00001234
  __ Branch(ne, &error, v0, Operand(0x00001234));
  __ nop();
  __ addu(v1, t3, t0);
  __ Branch(ne, &error, v1, Operand(0x80000003));
  __ nop();
  __ subu(v1, t7, t0);  // 0x7ffffffc
  __ Branch(ne, &error, v1, Operand(0x7ffffffc));
  __ nop();

  __ and_(v0, t1, t2);  // 0x00001230
  __ or_(v0, v0, t1);   // 0x00001234
  __ xor_(v0, v0, t2);  // 0x1234444c
  __ nor(v0, v0, t2);   // 0xedcba987
  __ Branch(ne, &error, v0, Operand(0xedcba983));
  __ nop();

  __ slt(v0, t7, t3);
  __ Branch(ne, &error, v0, Operand(0x1));
  __ nop();
  __ sltu(v0, t7, t3);
  __ Branch(ne, &error, v0, Operand(0x0));
  __ nop();
  // End of SPECIAL class.

  __ addi(v0, zero_reg, 0x7421);  // 0x00007421
  __ addi(v0, v0, -0x1);  // 0x00007420
  __ addiu(v0, v0, -0x20);  // 0x00007400
  __ Branch(ne, &error, v0, Operand(0x00007400));
  __ nop();
  __ addiu(v1, t3, 0x1);  // 0x80000000
  __ Branch(ne, &error, v1, Operand(0x80000000));
  __ nop();

  __ slti(v0, t1, 0x00002000);  // 0x1
  __ slti(v0, v0, 0xffff8000);  // 0x0
  __ Branch(ne, &error, v0, Operand(0x0));
  __ nop();
  __ sltiu(v0, t1, 0x00002000);  // 0x1
  __ sltiu(v0, v0, 0x00008000);  // 0x1
  __ Branch(ne, &error, v0, Operand(0x1));
  __ nop();

  __ andi(v0, t1, 0xf0f0);  // 0x00001030
  __ ori(v0, v0, 0x8a00);  // 0x00009a30
  __ xori(v0, v0, 0x83cc);  // 0x000019fc
  __ Branch(ne, &error, v0, Operand(0x000019fc));
  __ nop();
  __ lui(v1, 0x8123);  // 0x81230000
  __ Branch(ne, &error, v1, Operand(0x81230000));
  __ nop();

  // Everything was correctly executed. Load the expected result.
  __ li(v0, 0x31415926);
  __ b(&exit);
  __ nop();

  __ bind(&error);
  // Got an error. Return a wrong result.

  __ bind(&exit);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = Heap::CreateCode(desc,
                                  NULL,
                                  Code::ComputeFlags(Code::STUB),
                                  Handle<Object>(Heap::undefined_value()));
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 0xab0, 0xc, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(0x31415926, res);
}

#undef __
