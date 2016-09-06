// Copyright 2013 the V8 project authors. All rights reserved.
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

#include <stdio.h>
#include <cstring>

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/macro-assembler.h"

#include "src/arm64/assembler-arm64.h"
#include "src/arm64/decoder-arm64-inl.h"
#include "src/arm64/disasm-arm64.h"
#include "src/arm64/macro-assembler-arm64.h"
#include "src/arm64/utils-arm64.h"

using namespace v8::internal;

#define TEST_(name)  TEST(DISASM_##name)

#define EXP_SIZE   (256)
#define INSTR_SIZE (1024)
#define SET_UP_MASM()                                                    \
  InitializeVM();                                                        \
  Isolate* isolate = CcTest::i_isolate();                                \
  HandleScope scope(isolate);                                            \
  byte* buf = static_cast<byte*>(malloc(INSTR_SIZE));                    \
  uint32_t encoding = 0;                                                 \
  MacroAssembler* assm = new MacroAssembler(                             \
      isolate, buf, INSTR_SIZE, v8::internal::CodeObjectRequired::kYes); \
  Decoder<DispatchingDecoderVisitor>* decoder =                          \
      new Decoder<DispatchingDecoderVisitor>();                          \
  DisassemblingDecoder* disasm = new DisassemblingDecoder();             \
  decoder->AppendVisitor(disasm)

#define SET_UP_ASM()                                         \
  InitializeVM();                                            \
  Isolate* isolate = CcTest::i_isolate();                    \
  HandleScope scope(isolate);                                \
  byte* buf = static_cast<byte*>(malloc(INSTR_SIZE));        \
  uint32_t encoding = 0;                                     \
  Assembler* assm = new Assembler(isolate, buf, INSTR_SIZE); \
  Decoder<DispatchingDecoderVisitor>* decoder =              \
      new Decoder<DispatchingDecoderVisitor>();              \
  DisassemblingDecoder* disasm = new DisassemblingDecoder(); \
  decoder->AppendVisitor(disasm)

#define COMPARE(ASM, EXP)                                                      \
  assm->Reset();                                                               \
  assm->ASM;                                                                   \
  assm->GetCode(NULL);                                                         \
  decoder->Decode(reinterpret_cast<Instruction*>(buf));                        \
  encoding = *reinterpret_cast<uint32_t*>(buf);                                \
  if (strcmp(disasm->GetOutput(), EXP) != 0) {                                 \
    printf("%u : Encoding: %08" PRIx32 "\nExpected: %s\nFound:    %s\n",       \
           __LINE__, encoding, EXP, disasm->GetOutput());                      \
    abort();                                                                   \
  }

#define COMPARE_PREFIX(ASM, EXP)                                               \
  assm->Reset();                                                               \
  assm->ASM;                                                                   \
  assm->GetCode(NULL);                                                         \
  decoder->Decode(reinterpret_cast<Instruction*>(buf));                        \
  encoding = *reinterpret_cast<uint32_t*>(buf);                                \
  if (strncmp(disasm->GetOutput(), EXP, strlen(EXP)) != 0) {                   \
    printf("%u : Encoding: %08" PRIx32 "\nExpected: %s\nFound:    %s\n",       \
           __LINE__, encoding, EXP, disasm->GetOutput());                      \
    abort();                                                                   \
  }

#define CLEANUP() \
  delete disasm;  \
  delete decoder; \
  delete assm;    \
  free(buf)


static bool vm_initialized = false;


static void InitializeVM() {
  if (!vm_initialized) {
    CcTest::InitializeVM();
    vm_initialized = true;
  }
}


TEST_(bootstrap) {
  SET_UP_ASM();

  // Instructions generated by C compiler, disassembled by objdump, and
  // reformatted to suit our disassembly style.
  COMPARE(dci(0xa9ba7bfd), "stp fp, lr, [csp, #-96]!");
  COMPARE(dci(0x910003fd), "mov fp, csp");
  COMPARE(dci(0x9100e3a0), "add x0, fp, #0x38 (56)");
  COMPARE(dci(0xb900001f), "str wzr, [x0]");
  COMPARE(dci(0x528000e1), "movz w1, #0x7");
  COMPARE(dci(0xb9001c01), "str w1, [x0, #28]");
  COMPARE(dci(0x390043a0), "strb w0, [fp, #16]");
  COMPARE(dci(0x790027a0), "strh w0, [fp, #18]");
  COMPARE(dci(0xb9400400), "ldr w0, [x0, #4]");
  COMPARE(dci(0x0b000021), "add w1, w1, w0");
  COMPARE(dci(0x531b6800), "lsl w0, w0, #5");
  COMPARE(dci(0x521e0400), "eor w0, w0, #0xc");
  COMPARE(dci(0x72af0f00), "movk w0, #0x7878, lsl #16");
  COMPARE(dci(0xd360fc00), "lsr x0, x0, #32");
  COMPARE(dci(0x13037c01), "asr w1, w0, #3");
  COMPARE(dci(0x4b000021), "sub w1, w1, w0");
  COMPARE(dci(0x2a0103e0), "mov w0, w1");
  COMPARE(dci(0x93407c00), "sxtw x0, w0");
  COMPARE(dci(0x2a000020), "orr w0, w1, w0");
  COMPARE(dci(0xa8c67bfd), "ldp fp, lr, [csp], #96");

  CLEANUP();
}


TEST_(mov_mvn) {
  SET_UP_MASM();

  COMPARE(Mov(w0, Operand(0x1234)), "movz w0, #0x1234");
  COMPARE(Mov(x1, Operand(0x1234)), "movz x1, #0x1234");
  COMPARE(Mov(w2, Operand(w3)), "mov w2, w3");
  COMPARE(Mov(x4, Operand(x5)), "mov x4, x5");
  COMPARE(Mov(w6, Operand(w7, LSL, 5)), "lsl w6, w7, #5");
  COMPARE(Mov(x8, Operand(x9, ASR, 42)), "asr x8, x9, #42");
  COMPARE(Mov(w10, Operand(w11, UXTB)), "uxtb w10, w11");
  COMPARE(Mov(x12, Operand(x13, UXTB, 1)), "ubfiz x12, x13, #1, #8");
  COMPARE(Mov(w14, Operand(w15, SXTH, 2)), "sbfiz w14, w15, #2, #16");
  COMPARE(Mov(x16, Operand(x20, SXTW, 3)), "sbfiz x16, x20, #3, #32");

  COMPARE(Mov(x0, csp), "mov x0, csp");
  COMPARE(Mov(w0, wcsp), "mov w0, wcsp");
  COMPARE(Mov(x0, xzr), "mov x0, xzr");
  COMPARE(Mov(w0, wzr), "mov w0, wzr");
  COMPARE(mov(x0, csp), "mov x0, csp");
  COMPARE(mov(w0, wcsp), "mov w0, wcsp");
  COMPARE(mov(x0, xzr), "mov x0, xzr");
  COMPARE(mov(w0, wzr), "mov w0, wzr");

  COMPARE(Mvn(w0, Operand(0x1)), "movn w0, #0x1");
  COMPARE(Mvn(x1, Operand(0xfff)), "movn x1, #0xfff");
  COMPARE(Mvn(w2, Operand(w3)), "mvn w2, w3");
  COMPARE(Mvn(x4, Operand(x5)), "mvn x4, x5");
  COMPARE(Mvn(w6, Operand(w7, LSL, 12)), "mvn w6, w7, lsl #12");
  COMPARE(Mvn(x8, Operand(x9, ASR, 63)), "mvn x8, x9, asr #63");

  CLEANUP();
}


TEST_(move_immediate) {
  SET_UP_ASM();

  COMPARE(movz(w0, 0x1234), "movz w0, #0x1234");
  COMPARE(movz(x1, 0xabcd0000), "movz x1, #0xabcd0000");
  COMPARE(movz(x2, 0x555500000000), "movz x2, #0x555500000000");
  COMPARE(movz(x3, 0xaaaa000000000000), "movz x3, #0xaaaa000000000000");
  COMPARE(movz(x4, 0xabcd, 16), "movz x4, #0xabcd0000");
  COMPARE(movz(x5, 0x5555, 32), "movz x5, #0x555500000000");
  COMPARE(movz(x6, 0xaaaa, 48), "movz x6, #0xaaaa000000000000");

  COMPARE(movk(w7, 0x1234), "movk w7, #0x1234");
  COMPARE(movk(x8, 0xabcd0000), "movk x8, #0xabcd, lsl #16");
  COMPARE(movk(x9, 0x555500000000), "movk x9, #0x5555, lsl #32");
  COMPARE(movk(x10, 0xaaaa000000000000), "movk x10, #0xaaaa, lsl #48");
  COMPARE(movk(w11, 0xabcd, 16), "movk w11, #0xabcd, lsl #16");
  COMPARE(movk(x12, 0x5555, 32), "movk x12, #0x5555, lsl #32");
  COMPARE(movk(x13, 0xaaaa, 48), "movk x13, #0xaaaa, lsl #48");

  COMPARE(movn(w14, 0x1234), "movn w14, #0x1234");
  COMPARE(movn(x15, 0xabcd0000), "movn x15, #0xabcd0000");
  COMPARE(movn(x16, 0x555500000000), "movn x16, #0x555500000000");
  COMPARE(movn(x17, 0xaaaa000000000000), "movn x17, #0xaaaa000000000000");
  COMPARE(movn(w18, 0xabcd, 16), "movn w18, #0xabcd0000");
  COMPARE(movn(x19, 0x5555, 32), "movn x19, #0x555500000000");
  COMPARE(movn(x20, 0xaaaa, 48), "movn x20, #0xaaaa000000000000");

  COMPARE(movk(w21, 0), "movk w21, #0x0");
  COMPARE(movk(x22, 0, 0), "movk x22, #0x0");
  COMPARE(movk(w23, 0, 16), "movk w23, #0x0, lsl #16");
  COMPARE(movk(x24, 0, 32), "movk x24, #0x0, lsl #32");
  COMPARE(movk(x25, 0, 48), "movk x25, #0x0, lsl #48");

  CLEANUP();
}


TEST(move_immediate_2) {
  SET_UP_MASM();

  // Move instructions expected for certain immediates. This is really a macro
  // assembler test, to ensure it generates immediates efficiently.
  COMPARE(Mov(w0, 0), "movz w0, #0x0");
  COMPARE(Mov(w0, 0x0000ffff), "movz w0, #0xffff");
  COMPARE(Mov(w0, 0x00010000), "movz w0, #0x10000");
  COMPARE(Mov(w0, 0xffff0000), "movz w0, #0xffff0000");
  COMPARE(Mov(w0, 0x0001ffff), "movn w0, #0xfffe0000");
  COMPARE(Mov(w0, 0xffff8000), "movn w0, #0x7fff");
  COMPARE(Mov(w0, 0xfffffffe), "movn w0, #0x1");
  COMPARE(Mov(w0, 0xffffffff), "movn w0, #0x0");
  COMPARE(Mov(w0, 0x00ffff00), "mov w0, #0xffff00");
  COMPARE(Mov(w0, 0xfffe7fff), "mov w0, #0xfffe7fff");
  COMPARE(Mov(w0, 0xfffeffff), "movn w0, #0x10000");
  COMPARE(Mov(w0, 0xffff7fff), "movn w0, #0x8000");

  COMPARE(Mov(x0, 0), "movz x0, #0x0");
  COMPARE(Mov(x0, 0x0000ffff), "movz x0, #0xffff");
  COMPARE(Mov(x0, 0x00010000), "movz x0, #0x10000");
  COMPARE(Mov(x0, 0xffff0000), "movz x0, #0xffff0000");
  COMPARE(Mov(x0, 0x0001ffff), "mov x0, #0x1ffff");
  COMPARE(Mov(x0, 0xffff8000), "mov x0, #0xffff8000");
  COMPARE(Mov(x0, 0xfffffffe), "mov x0, #0xfffffffe");
  COMPARE(Mov(x0, 0xffffffff), "mov x0, #0xffffffff");
  COMPARE(Mov(x0, 0x00ffff00), "mov x0, #0xffff00");
  COMPARE(Mov(x0, 0xffff000000000000), "movz x0, #0xffff000000000000");
  COMPARE(Mov(x0, 0x0000ffff00000000), "movz x0, #0xffff00000000");
  COMPARE(Mov(x0, 0x00000000ffff0000), "movz x0, #0xffff0000");
  COMPARE(Mov(x0, 0xffffffffffff0000), "movn x0, #0xffff");
  COMPARE(Mov(x0, 0xffffffff0000ffff), "movn x0, #0xffff0000");
  COMPARE(Mov(x0, 0xffff0000ffffffff), "movn x0, #0xffff00000000");
  COMPARE(Mov(x0, 0x0000ffffffffffff), "movn x0, #0xffff000000000000");
  COMPARE(Mov(x0, 0xfffe7fffffffffff), "mov x0, #0xfffe7fffffffffff");
  COMPARE(Mov(x0, 0xfffeffffffffffff), "movn x0, #0x1000000000000");
  COMPARE(Mov(x0, 0xffff7fffffffffff), "movn x0, #0x800000000000");
  COMPARE(Mov(x0, 0xfffffffe7fffffff), "mov x0, #0xfffffffe7fffffff");
  COMPARE(Mov(x0, 0xfffffffeffffffff), "movn x0, #0x100000000");
  COMPARE(Mov(x0, 0xffffffff7fffffff), "movn x0, #0x80000000");
  COMPARE(Mov(x0, 0xfffffffffffe7fff), "mov x0, #0xfffffffffffe7fff");
  COMPARE(Mov(x0, 0xfffffffffffeffff), "movn x0, #0x10000");
  COMPARE(Mov(x0, 0xffffffffffff7fff), "movn x0, #0x8000");
  COMPARE(Mov(x0, 0xffffffffffffffff), "movn x0, #0x0");

  COMPARE(Movk(w0, 0x1234, 0), "movk w0, #0x1234");
  COMPARE(Movk(x1, 0x2345, 0), "movk x1, #0x2345");
  COMPARE(Movk(w2, 0x3456, 16), "movk w2, #0x3456, lsl #16");
  COMPARE(Movk(x3, 0x4567, 16), "movk x3, #0x4567, lsl #16");
  COMPARE(Movk(x4, 0x5678, 32), "movk x4, #0x5678, lsl #32");
  COMPARE(Movk(x5, 0x6789, 48), "movk x5, #0x6789, lsl #48");

  CLEANUP();
}


TEST_(add_immediate) {
  SET_UP_ASM();

  COMPARE(add(w0, w1, Operand(0xff)), "add w0, w1, #0xff (255)");
  COMPARE(add(x2, x3, Operand(0x3ff)), "add x2, x3, #0x3ff (1023)");
  COMPARE(add(w4, w5, Operand(0xfff)), "add w4, w5, #0xfff (4095)");
  COMPARE(add(x6, x7, Operand(0x1000)), "add x6, x7, #0x1000 (4096)");
  COMPARE(add(w8, w9, Operand(0xff000)), "add w8, w9, #0xff000 (1044480)");
  COMPARE(add(x10, x11, Operand(0x3ff000)),
          "add x10, x11, #0x3ff000 (4190208)");
  COMPARE(add(w12, w13, Operand(0xfff000)),
          "add w12, w13, #0xfff000 (16773120)");
  COMPARE(adds(w14, w15, Operand(0xff)), "adds w14, w15, #0xff (255)");
  COMPARE(adds(x16, x17, Operand(0xaa000)),
          "adds x16, x17, #0xaa000 (696320)");
  COMPARE(cmn(w18, Operand(0xff)), "cmn w18, #0xff (255)");
  COMPARE(cmn(x19, Operand(0xff000)), "cmn x19, #0xff000 (1044480)");
  COMPARE(add(w0, wcsp, Operand(0)), "mov w0, wcsp");
  COMPARE(add(csp, x0, Operand(0)), "mov csp, x0");

  COMPARE(add(w1, wcsp, Operand(8)), "add w1, wcsp, #0x8 (8)");
  COMPARE(add(x2, csp, Operand(16)), "add x2, csp, #0x10 (16)");
  COMPARE(add(wcsp, wcsp, Operand(42)), "add wcsp, wcsp, #0x2a (42)");
  COMPARE(cmn(csp, Operand(24)), "cmn csp, #0x18 (24)");
  COMPARE(adds(wzr, wcsp, Operand(9)), "cmn wcsp, #0x9 (9)");

  CLEANUP();
}


TEST_(sub_immediate) {
  SET_UP_ASM();

  COMPARE(sub(w0, w1, Operand(0xff)), "sub w0, w1, #0xff (255)");
  COMPARE(sub(x2, x3, Operand(0x3ff)), "sub x2, x3, #0x3ff (1023)");
  COMPARE(sub(w4, w5, Operand(0xfff)), "sub w4, w5, #0xfff (4095)");
  COMPARE(sub(x6, x7, Operand(0x1000)), "sub x6, x7, #0x1000 (4096)");
  COMPARE(sub(w8, w9, Operand(0xff000)), "sub w8, w9, #0xff000 (1044480)");
  COMPARE(sub(x10, x11, Operand(0x3ff000)),
          "sub x10, x11, #0x3ff000 (4190208)");
  COMPARE(sub(w12, w13, Operand(0xfff000)),
          "sub w12, w13, #0xfff000 (16773120)");
  COMPARE(subs(w14, w15, Operand(0xff)), "subs w14, w15, #0xff (255)");
  COMPARE(subs(x16, x17, Operand(0xaa000)),
          "subs x16, x17, #0xaa000 (696320)");
  COMPARE(cmp(w18, Operand(0xff)), "cmp w18, #0xff (255)");
  COMPARE(cmp(x19, Operand(0xff000)), "cmp x19, #0xff000 (1044480)");

  COMPARE(add(w1, wcsp, Operand(8)), "add w1, wcsp, #0x8 (8)");
  COMPARE(add(x2, csp, Operand(16)), "add x2, csp, #0x10 (16)");
  COMPARE(add(wcsp, wcsp, Operand(42)), "add wcsp, wcsp, #0x2a (42)");
  COMPARE(cmn(csp, Operand(24)), "cmn csp, #0x18 (24)");
  COMPARE(adds(wzr, wcsp, Operand(9)), "cmn wcsp, #0x9 (9)");

  CLEANUP();
}


TEST_(add_shifted) {
  SET_UP_ASM();

  COMPARE(add(w0, w1, Operand(w2)), "add w0, w1, w2");
  COMPARE(add(x3, x4, Operand(x5)), "add x3, x4, x5");
  COMPARE(add(w6, w7, Operand(w8, LSL, 1)), "add w6, w7, w8, lsl #1");
  COMPARE(add(x9, x10, Operand(x11, LSL, 2)), "add x9, x10, x11, lsl #2");
  COMPARE(add(w12, w13, Operand(w14, LSR, 3)), "add w12, w13, w14, lsr #3");
  COMPARE(add(x15, x16, Operand(x17, LSR, 4)), "add x15, x16, x17, lsr #4");
  COMPARE(add(w18, w19, Operand(w20, ASR, 5)), "add w18, w19, w20, asr #5");
  COMPARE(add(x21, x22, Operand(x23, ASR, 6)), "add x21, x22, x23, asr #6");
  COMPARE(cmn(w24, Operand(w25)), "cmn w24, w25");
  COMPARE(cmn(x26, Operand(cp, LSL, 63)), "cmn x26, cp, lsl #63");

  COMPARE(add(x0, csp, Operand(x1)), "add x0, csp, x1");
  COMPARE(add(w2, wcsp, Operand(w3)), "add w2, wcsp, w3");
  COMPARE(add(x4, csp, Operand(x5, LSL, 1)), "add x4, csp, x5, lsl #1");
  COMPARE(add(x4, xzr, Operand(x5, LSL, 1)), "add x4, xzr, x5, lsl #1");
  COMPARE(add(w6, wcsp, Operand(w7, LSL, 3)), "add w6, wcsp, w7, lsl #3");
  COMPARE(adds(xzr, csp, Operand(x8, LSL, 4)), "cmn csp, x8, lsl #4");
  COMPARE(adds(xzr, xzr, Operand(x8, LSL, 5)), "cmn xzr, x8, lsl #5");

  CLEANUP();
}


TEST_(sub_shifted) {
  SET_UP_ASM();

  COMPARE(sub(w0, w1, Operand(w2)), "sub w0, w1, w2");
  COMPARE(sub(x3, x4, Operand(x5)), "sub x3, x4, x5");
  COMPARE(sub(w6, w7, Operand(w8, LSL, 1)), "sub w6, w7, w8, lsl #1");
  COMPARE(sub(x9, x10, Operand(x11, LSL, 2)), "sub x9, x10, x11, lsl #2");
  COMPARE(sub(w12, w13, Operand(w14, LSR, 3)), "sub w12, w13, w14, lsr #3");
  COMPARE(sub(x15, x16, Operand(x17, LSR, 4)), "sub x15, x16, x17, lsr #4");
  COMPARE(sub(w18, w19, Operand(w20, ASR, 5)), "sub w18, w19, w20, asr #5");
  COMPARE(sub(x21, x22, Operand(x23, ASR, 6)), "sub x21, x22, x23, asr #6");
  COMPARE(cmp(w24, Operand(w25)), "cmp w24, w25");
  COMPARE(cmp(x26, Operand(cp, LSL, 63)), "cmp x26, cp, lsl #63");
  COMPARE(neg(w28, Operand(w29)), "neg w28, w29");
  COMPARE(neg(lr, Operand(x0, LSR, 62)), "neg lr, x0, lsr #62");
  COMPARE(negs(w1, Operand(w2)), "negs w1, w2");
  COMPARE(negs(x3, Operand(x4, ASR, 61)), "negs x3, x4, asr #61");

  COMPARE(sub(x0, csp, Operand(x1)), "sub x0, csp, x1");
  COMPARE(sub(w2, wcsp, Operand(w3)), "sub w2, wcsp, w3");
  COMPARE(sub(x4, csp, Operand(x5, LSL, 1)), "sub x4, csp, x5, lsl #1");
  COMPARE(sub(x4, xzr, Operand(x5, LSL, 1)), "neg x4, x5, lsl #1");
  COMPARE(sub(w6, wcsp, Operand(w7, LSL, 3)), "sub w6, wcsp, w7, lsl #3");
  COMPARE(subs(xzr, csp, Operand(x8, LSL, 4)), "cmp csp, x8, lsl #4");
  COMPARE(subs(xzr, xzr, Operand(x8, LSL, 5)), "cmp xzr, x8, lsl #5");

  CLEANUP();
}


TEST_(add_extended) {
  SET_UP_ASM();

  COMPARE(add(w0, w1, Operand(w2, UXTB)), "add w0, w1, w2, uxtb");
  COMPARE(adds(x3, x4, Operand(w5, UXTB, 1)), "adds x3, x4, w5, uxtb #1");
  COMPARE(add(w6, w7, Operand(w8, UXTH, 2)), "add w6, w7, w8, uxth #2");
  COMPARE(adds(x9, x10, Operand(x11, UXTW, 3)), "adds x9, x10, w11, uxtw #3");
  COMPARE(add(x12, x13, Operand(x14, UXTX, 4)), "add x12, x13, x14, uxtx #4");
  COMPARE(adds(w15, w16, Operand(w17, SXTB, 4)), "adds w15, w16, w17, sxtb #4");
  COMPARE(add(x18, x19, Operand(x20, SXTB, 3)), "add x18, x19, w20, sxtb #3");
  COMPARE(adds(w21, w22, Operand(w23, SXTH, 2)), "adds w21, w22, w23, sxth #2");
  COMPARE(add(x24, x25, Operand(x26, SXTW, 1)), "add x24, x25, w26, sxtw #1");
  COMPARE(adds(cp, jssp, Operand(fp, SXTX)), "adds cp, jssp, fp, sxtx");
  COMPARE(cmn(w0, Operand(w1, UXTB, 2)), "cmn w0, w1, uxtb #2");
  COMPARE(cmn(x2, Operand(x3, SXTH, 4)), "cmn x2, w3, sxth #4");

  COMPARE(add(w0, wcsp, Operand(w1, UXTB)), "add w0, wcsp, w1, uxtb");
  COMPARE(add(x2, csp, Operand(x3, UXTH, 1)), "add x2, csp, w3, uxth #1");
  COMPARE(add(wcsp, wcsp, Operand(w4, UXTW, 2)), "add wcsp, wcsp, w4, lsl #2");
  COMPARE(cmn(csp, Operand(xzr, UXTX, 3)), "cmn csp, xzr, lsl #3");
  COMPARE(cmn(csp, Operand(xzr, LSL, 4)), "cmn csp, xzr, lsl #4");

  CLEANUP();
}


TEST_(sub_extended) {
  SET_UP_ASM();

  COMPARE(sub(w0, w1, Operand(w2, UXTB)), "sub w0, w1, w2, uxtb");
  COMPARE(subs(x3, x4, Operand(w5, UXTB, 1)), "subs x3, x4, w5, uxtb #1");
  COMPARE(sub(w6, w7, Operand(w8, UXTH, 2)), "sub w6, w7, w8, uxth #2");
  COMPARE(subs(x9, x10, Operand(x11, UXTW, 3)), "subs x9, x10, w11, uxtw #3");
  COMPARE(sub(x12, x13, Operand(x14, UXTX, 4)), "sub x12, x13, x14, uxtx #4");
  COMPARE(subs(w15, w16, Operand(w17, SXTB, 4)), "subs w15, w16, w17, sxtb #4");
  COMPARE(sub(x18, x19, Operand(x20, SXTB, 3)), "sub x18, x19, w20, sxtb #3");
  COMPARE(subs(w21, w22, Operand(w23, SXTH, 2)), "subs w21, w22, w23, sxth #2");
  COMPARE(sub(x24, x25, Operand(x26, SXTW, 1)), "sub x24, x25, w26, sxtw #1");
  COMPARE(subs(cp, jssp, Operand(fp, SXTX)), "subs cp, jssp, fp, sxtx");
  COMPARE(cmp(w0, Operand(w1, SXTB, 1)), "cmp w0, w1, sxtb #1");
  COMPARE(cmp(x2, Operand(x3, UXTH, 3)), "cmp x2, w3, uxth #3");

  COMPARE(sub(w0, wcsp, Operand(w1, UXTB)), "sub w0, wcsp, w1, uxtb");
  COMPARE(sub(x2, csp, Operand(x3, UXTH, 1)), "sub x2, csp, w3, uxth #1");
  COMPARE(sub(wcsp, wcsp, Operand(w4, UXTW, 2)), "sub wcsp, wcsp, w4, lsl #2");
  COMPARE(cmp(csp, Operand(xzr, UXTX, 3)), "cmp csp, xzr, lsl #3");
  COMPARE(cmp(csp, Operand(xzr, LSL, 4)), "cmp csp, xzr, lsl #4");

  CLEANUP();
}


TEST_(adc_subc_ngc) {
  SET_UP_ASM();

  COMPARE(adc(w0, w1, Operand(w2)), "adc w0, w1, w2");
  COMPARE(adc(x3, x4, Operand(x5)), "adc x3, x4, x5");
  COMPARE(adcs(w6, w7, Operand(w8)), "adcs w6, w7, w8");
  COMPARE(adcs(x9, x10, Operand(x11)), "adcs x9, x10, x11");
  COMPARE(sbc(w12, w13, Operand(w14)), "sbc w12, w13, w14");
  COMPARE(sbc(x15, x16, Operand(x17)), "sbc x15, x16, x17");
  COMPARE(sbcs(w18, w19, Operand(w20)), "sbcs w18, w19, w20");
  COMPARE(sbcs(x21, x22, Operand(x23)), "sbcs x21, x22, x23");
  COMPARE(ngc(w24, Operand(w25)), "ngc w24, w25");
  COMPARE(ngc(x26, Operand(cp)), "ngc x26, cp");
  COMPARE(ngcs(w28, Operand(w29)), "ngcs w28, w29");
  COMPARE(ngcs(lr, Operand(x0)), "ngcs lr, x0");

  CLEANUP();
}


TEST_(mul_and_div) {
  SET_UP_ASM();

  COMPARE(mul(w0, w1, w2), "mul w0, w1, w2");
  COMPARE(mul(x3, x4, x5), "mul x3, x4, x5");
  COMPARE(mul(w30, w0, w1), "mul w30, w0, w1");
  COMPARE(mul(lr, x0, x1), "mul lr, x0, x1");
  COMPARE(mneg(w0, w1, w2), "mneg w0, w1, w2");
  COMPARE(mneg(x3, x4, x5), "mneg x3, x4, x5");
  COMPARE(mneg(w30, w0, w1), "mneg w30, w0, w1");
  COMPARE(mneg(lr, x0, x1), "mneg lr, x0, x1");
  COMPARE(smull(x0, w0, w1), "smull x0, w0, w1");
  COMPARE(smull(lr, w30, w0), "smull lr, w30, w0");
  COMPARE(smulh(x0, x1, x2), "smulh x0, x1, x2");

  COMPARE(madd(w0, w1, w2, w3), "madd w0, w1, w2, w3");
  COMPARE(madd(x4, x5, x6, x7), "madd x4, x5, x6, x7");
  COMPARE(madd(w8, w9, w10, wzr), "mul w8, w9, w10");
  COMPARE(madd(x11, x12, x13, xzr), "mul x11, x12, x13");
  COMPARE(msub(w14, w15, w16, w17), "msub w14, w15, w16, w17");
  COMPARE(msub(x18, x19, x20, x21), "msub x18, x19, x20, x21");
  COMPARE(msub(w22, w23, w24, wzr), "mneg w22, w23, w24");
  COMPARE(msub(x25, x26, x0, xzr), "mneg x25, x26, x0");

  COMPARE(sdiv(w0, w1, w2), "sdiv w0, w1, w2");
  COMPARE(sdiv(x3, x4, x5), "sdiv x3, x4, x5");
  COMPARE(udiv(w6, w7, w8), "udiv w6, w7, w8");
  COMPARE(udiv(x9, x10, x11), "udiv x9, x10, x11");

  CLEANUP();
}


TEST(maddl_msubl) {
  SET_UP_ASM();

  COMPARE(smaddl(x0, w1, w2, x3), "smaddl x0, w1, w2, x3");
  COMPARE(smaddl(x25, w21, w22, x16), "smaddl x25, w21, w22, x16");
  COMPARE(umaddl(x0, w1, w2, x3), "umaddl x0, w1, w2, x3");
  COMPARE(umaddl(x25, w21, w22, x16), "umaddl x25, w21, w22, x16");

  COMPARE(smsubl(x0, w1, w2, x3), "smsubl x0, w1, w2, x3");
  COMPARE(smsubl(x25, w21, w22, x16), "smsubl x25, w21, w22, x16");
  COMPARE(umsubl(x0, w1, w2, x3), "umsubl x0, w1, w2, x3");
  COMPARE(umsubl(x25, w21, w22, x16), "umsubl x25, w21, w22, x16");

  CLEANUP();
}


TEST_(dp_1_source) {
  SET_UP_ASM();

  COMPARE(rbit(w0, w1), "rbit w0, w1");
  COMPARE(rbit(x2, x3), "rbit x2, x3");
  COMPARE(rev16(w4, w5), "rev16 w4, w5");
  COMPARE(rev16(x6, x7), "rev16 x6, x7");
  COMPARE(rev32(x8, x9), "rev32 x8, x9");
  COMPARE(rev(w10, w11), "rev w10, w11");
  COMPARE(rev(x12, x13), "rev x12, x13");
  COMPARE(clz(w14, w15), "clz w14, w15");
  COMPARE(clz(x16, x17), "clz x16, x17");
  COMPARE(cls(w18, w19), "cls w18, w19");
  COMPARE(cls(x20, x21), "cls x20, x21");

  CLEANUP();
}


TEST_(bitfield) {
  SET_UP_ASM();

  COMPARE(sxtb(w0, w1), "sxtb w0, w1");
  COMPARE(sxtb(x2, x3), "sxtb x2, w3");
  COMPARE(sxth(w4, w5), "sxth w4, w5");
  COMPARE(sxth(x6, x7), "sxth x6, w7");
  COMPARE(sxtw(x8, x9), "sxtw x8, w9");
  COMPARE(sxtb(x0, w1), "sxtb x0, w1");
  COMPARE(sxth(x2, w3), "sxth x2, w3");
  COMPARE(sxtw(x4, w5), "sxtw x4, w5");

  COMPARE(uxtb(w10, w11), "uxtb w10, w11");
  COMPARE(uxtb(x12, x13), "uxtb x12, w13");
  COMPARE(uxth(w14, w15), "uxth w14, w15");
  COMPARE(uxth(x16, x17), "uxth x16, w17");
  COMPARE(uxtw(x18, x19), "ubfx x18, x19, #0, #32");

  COMPARE(asr(w20, w21, 10), "asr w20, w21, #10");
  COMPARE(asr(x22, x23, 20), "asr x22, x23, #20");
  COMPARE(lsr(w24, w25, 10), "lsr w24, w25, #10");
  COMPARE(lsr(x26, cp, 20), "lsr x26, cp, #20");
  COMPARE(lsl(w28, w29, 10), "lsl w28, w29, #10");
  COMPARE(lsl(lr, x0, 20), "lsl lr, x0, #20");

  COMPARE(sbfiz(w1, w2, 1, 20), "sbfiz w1, w2, #1, #20");
  COMPARE(sbfiz(x3, x4, 2, 19), "sbfiz x3, x4, #2, #19");
  COMPARE(sbfx(w5, w6, 3, 18), "sbfx w5, w6, #3, #18");
  COMPARE(sbfx(x7, x8, 4, 17), "sbfx x7, x8, #4, #17");
  COMPARE(bfi(w9, w10, 5, 16), "bfi w9, w10, #5, #16");
  COMPARE(bfi(x11, x12, 6, 15), "bfi x11, x12, #6, #15");
  COMPARE(bfxil(w13, w14, 7, 14), "bfxil w13, w14, #7, #14");
  COMPARE(bfxil(x15, x16, 8, 13), "bfxil x15, x16, #8, #13");
  COMPARE(ubfiz(w17, w18, 9, 12), "ubfiz w17, w18, #9, #12");
  COMPARE(ubfiz(x19, x20, 10, 11), "ubfiz x19, x20, #10, #11");
  COMPARE(ubfx(w21, w22, 11, 10), "ubfx w21, w22, #11, #10");
  COMPARE(ubfx(x23, x24, 12, 9), "ubfx x23, x24, #12, #9");

  CLEANUP();
}


TEST_(extract) {
  SET_UP_ASM();

  COMPARE(extr(w0, w1, w2, 0), "extr w0, w1, w2, #0");
  COMPARE(extr(x3, x4, x5, 1), "extr x3, x4, x5, #1");
  COMPARE(extr(w6, w7, w8, 31), "extr w6, w7, w8, #31");
  COMPARE(extr(x9, x10, x11, 63), "extr x9, x10, x11, #63");
  COMPARE(extr(w12, w13, w13, 10), "ror w12, w13, #10");
  COMPARE(extr(x14, x15, x15, 42), "ror x14, x15, #42");

  CLEANUP();
}


TEST_(logical_immediate) {
  SET_UP_ASM();
  #define RESULT_SIZE (256)

  char result[RESULT_SIZE];

  // Test immediate encoding - 64-bit destination.
  // 64-bit patterns.
  uint64_t value = 0x7fffffff;
  for (int i = 0; i < 64; i++) {
    snprintf(result, RESULT_SIZE, "and x0, x0, #0x%" PRIx64, value);
    COMPARE(and_(x0, x0, Operand(value)), result);
    value = ((value & 1) << 63) | (value >> 1);  // Rotate right 1 bit.
  }

  // 32-bit patterns.
  value = 0x00003fff00003fffL;
  for (int i = 0; i < 32; i++) {
    snprintf(result, RESULT_SIZE, "and x0, x0, #0x%" PRIx64, value);
    COMPARE(and_(x0, x0, Operand(value)), result);
    value = ((value & 1) << 63) | (value >> 1);  // Rotate right 1 bit.
  }

  // 16-bit patterns.
  value = 0x001f001f001f001fL;
  for (int i = 0; i < 16; i++) {
    snprintf(result, RESULT_SIZE, "and x0, x0, #0x%" PRIx64, value);
    COMPARE(and_(x0, x0, Operand(value)), result);
    value = ((value & 1) << 63) | (value >> 1);  // Rotate right 1 bit.
  }

  // 8-bit patterns.
  value = 0x0e0e0e0e0e0e0e0eL;
  for (int i = 0; i < 8; i++) {
    snprintf(result, RESULT_SIZE, "and x0, x0, #0x%" PRIx64, value);
    COMPARE(and_(x0, x0, Operand(value)), result);
    value = ((value & 1) << 63) | (value >> 1);  // Rotate right 1 bit.
  }

  // 4-bit patterns.
  value = 0x6666666666666666L;
  for (int i = 0; i < 4; i++) {
    snprintf(result, RESULT_SIZE, "and x0, x0, #0x%" PRIx64, value);
    COMPARE(and_(x0, x0, Operand(value)), result);
    value = ((value & 1) << 63) | (value >> 1);  // Rotate right 1 bit.
  }

  // 2-bit patterns.
  COMPARE(and_(x0, x0, Operand(0x5555555555555555L)),
          "and x0, x0, #0x5555555555555555");
  COMPARE(and_(x0, x0, Operand(0xaaaaaaaaaaaaaaaaL)),
          "and x0, x0, #0xaaaaaaaaaaaaaaaa");

  // Test immediate encoding - 32-bit destination.
  COMPARE(and_(w0, w0, Operand(0xff8007ff)),
          "and w0, w0, #0xff8007ff");  // 32-bit pattern.
  COMPARE(and_(w0, w0, Operand(0xf87ff87f)),
          "and w0, w0, #0xf87ff87f");  // 16-bit pattern.
  COMPARE(and_(w0, w0, Operand(0x87878787)),
          "and w0, w0, #0x87878787");  // 8-bit pattern.
  COMPARE(and_(w0, w0, Operand(0x66666666)),
          "and w0, w0, #0x66666666");  // 4-bit pattern.
  COMPARE(and_(w0, w0, Operand(0x55555555)),
          "and w0, w0, #0x55555555");  // 2-bit pattern.

  // Test other instructions.
  COMPARE(tst(w1, Operand(0x11111111)),
          "tst w1, #0x11111111");
  COMPARE(tst(x2, Operand(0x8888888888888888L)),
          "tst x2, #0x8888888888888888");
  COMPARE(orr(w7, w8, Operand(0xaaaaaaaa)),
          "orr w7, w8, #0xaaaaaaaa");
  COMPARE(orr(x9, x10, Operand(0x5555555555555555L)),
          "orr x9, x10, #0x5555555555555555");
  COMPARE(eor(w15, w16, Operand(0x00000001)),
          "eor w15, w16, #0x1");
  COMPARE(eor(x17, x18, Operand(0x0000000000000003L)),
          "eor x17, x18, #0x3");
  COMPARE(ands(w23, w24, Operand(0x0000000f)), "ands w23, w24, #0xf");
  COMPARE(ands(x25, x26, Operand(0x800000000000000fL)),
          "ands x25, x26, #0x800000000000000f");

  // Test inverse.
  COMPARE(bic(w3, w4, Operand(0x20202020)),
          "and w3, w4, #0xdfdfdfdf");
  COMPARE(bic(x5, x6, Operand(0x4040404040404040L)),
          "and x5, x6, #0xbfbfbfbfbfbfbfbf");
  COMPARE(orn(w11, w12, Operand(0x40004000)),
          "orr w11, w12, #0xbfffbfff");
  COMPARE(orn(x13, x14, Operand(0x8181818181818181L)),
          "orr x13, x14, #0x7e7e7e7e7e7e7e7e");
  COMPARE(eon(w19, w20, Operand(0x80000001)),
          "eor w19, w20, #0x7ffffffe");
  COMPARE(eon(x21, x22, Operand(0xc000000000000003L)),
          "eor x21, x22, #0x3ffffffffffffffc");
  COMPARE(bics(w27, w28, Operand(0xfffffff7)), "ands w27, w28, #0x8");
  COMPARE(bics(fp, x0, Operand(0xfffffffeffffffffL)),
          "ands fp, x0, #0x100000000");

  // Test stack pointer.
  COMPARE(and_(wcsp, wzr, Operand(7)), "and wcsp, wzr, #0x7");
  COMPARE(ands(xzr, xzr, Operand(7)), "tst xzr, #0x7");
  COMPARE(orr(csp, xzr, Operand(15)), "orr csp, xzr, #0xf");
  COMPARE(eor(wcsp, w0, Operand(31)), "eor wcsp, w0, #0x1f");

  // Test move aliases.
  COMPARE(orr(w0, wzr, Operand(0x00000780)), "orr w0, wzr, #0x780");
  COMPARE(orr(w1, wzr, Operand(0x00007800)), "orr w1, wzr, #0x7800");
  COMPARE(orr(w2, wzr, Operand(0x00078000)), "mov w2, #0x78000");
  COMPARE(orr(w3, wzr, Operand(0x00780000)), "orr w3, wzr, #0x780000");
  COMPARE(orr(w4, wzr, Operand(0x07800000)), "orr w4, wzr, #0x7800000");
  COMPARE(orr(x5, xzr, Operand(0xffffffffffffc001UL)),
          "orr x5, xzr, #0xffffffffffffc001");
  COMPARE(orr(x6, xzr, Operand(0xfffffffffffc001fUL)),
          "mov x6, #0xfffffffffffc001f");
  COMPARE(orr(x7, xzr, Operand(0xffffffffffc001ffUL)),
          "mov x7, #0xffffffffffc001ff");
  COMPARE(orr(x8, xzr, Operand(0xfffffffffc001fffUL)),
          "mov x8, #0xfffffffffc001fff");
  COMPARE(orr(x9, xzr, Operand(0xffffffffc001ffffUL)),
          "orr x9, xzr, #0xffffffffc001ffff");

  CLEANUP();
}


TEST_(logical_shifted) {
  SET_UP_ASM();

  COMPARE(and_(w0, w1, Operand(w2)), "and w0, w1, w2");
  COMPARE(and_(x3, x4, Operand(x5, LSL, 1)), "and x3, x4, x5, lsl #1");
  COMPARE(and_(w6, w7, Operand(w8, LSR, 2)), "and w6, w7, w8, lsr #2");
  COMPARE(and_(x9, x10, Operand(x11, ASR, 3)), "and x9, x10, x11, asr #3");
  COMPARE(and_(w12, w13, Operand(w14, ROR, 4)), "and w12, w13, w14, ror #4");

  COMPARE(bic(w15, w16, Operand(w17)), "bic w15, w16, w17");
  COMPARE(bic(x18, x19, Operand(x20, LSL, 5)), "bic x18, x19, x20, lsl #5");
  COMPARE(bic(w21, w22, Operand(w23, LSR, 6)), "bic w21, w22, w23, lsr #6");
  COMPARE(bic(x24, x25, Operand(x26, ASR, 7)), "bic x24, x25, x26, asr #7");
  COMPARE(bic(w27, w28, Operand(w29, ROR, 8)), "bic w27, w28, w29, ror #8");

  COMPARE(orr(w0, w1, Operand(w2)), "orr w0, w1, w2");
  COMPARE(orr(x3, x4, Operand(x5, LSL, 9)), "orr x3, x4, x5, lsl #9");
  COMPARE(orr(w6, w7, Operand(w8, LSR, 10)), "orr w6, w7, w8, lsr #10");
  COMPARE(orr(x9, x10, Operand(x11, ASR, 11)), "orr x9, x10, x11, asr #11");
  COMPARE(orr(w12, w13, Operand(w14, ROR, 12)), "orr w12, w13, w14, ror #12");

  COMPARE(orn(w15, w16, Operand(w17)), "orn w15, w16, w17");
  COMPARE(orn(x18, x19, Operand(x20, LSL, 13)), "orn x18, x19, x20, lsl #13");
  COMPARE(orn(w21, w22, Operand(w23, LSR, 14)), "orn w21, w22, w23, lsr #14");
  COMPARE(orn(x24, x25, Operand(x26, ASR, 15)), "orn x24, x25, x26, asr #15");
  COMPARE(orn(w27, w28, Operand(w29, ROR, 16)), "orn w27, w28, w29, ror #16");

  COMPARE(eor(w0, w1, Operand(w2)), "eor w0, w1, w2");
  COMPARE(eor(x3, x4, Operand(x5, LSL, 17)), "eor x3, x4, x5, lsl #17");
  COMPARE(eor(w6, w7, Operand(w8, LSR, 18)), "eor w6, w7, w8, lsr #18");
  COMPARE(eor(x9, x10, Operand(x11, ASR, 19)), "eor x9, x10, x11, asr #19");
  COMPARE(eor(w12, w13, Operand(w14, ROR, 20)), "eor w12, w13, w14, ror #20");

  COMPARE(eon(w15, w16, Operand(w17)), "eon w15, w16, w17");
  COMPARE(eon(x18, x19, Operand(x20, LSL, 21)), "eon x18, x19, x20, lsl #21");
  COMPARE(eon(w21, w22, Operand(w23, LSR, 22)), "eon w21, w22, w23, lsr #22");
  COMPARE(eon(x24, x25, Operand(x26, ASR, 23)), "eon x24, x25, x26, asr #23");
  COMPARE(eon(w27, w28, Operand(w29, ROR, 24)), "eon w27, w28, w29, ror #24");

  COMPARE(ands(w0, w1, Operand(w2)), "ands w0, w1, w2");
  COMPARE(ands(x3, x4, Operand(x5, LSL, 1)), "ands x3, x4, x5, lsl #1");
  COMPARE(ands(w6, w7, Operand(w8, LSR, 2)), "ands w6, w7, w8, lsr #2");
  COMPARE(ands(x9, x10, Operand(x11, ASR, 3)), "ands x9, x10, x11, asr #3");
  COMPARE(ands(w12, w13, Operand(w14, ROR, 4)), "ands w12, w13, w14, ror #4");

  COMPARE(bics(w15, w16, Operand(w17)), "bics w15, w16, w17");
  COMPARE(bics(x18, x19, Operand(x20, LSL, 5)), "bics x18, x19, x20, lsl #5");
  COMPARE(bics(w21, w22, Operand(w23, LSR, 6)), "bics w21, w22, w23, lsr #6");
  COMPARE(bics(x24, x25, Operand(x26, ASR, 7)), "bics x24, x25, x26, asr #7");
  COMPARE(bics(w27, w28, Operand(w29, ROR, 8)), "bics w27, w28, w29, ror #8");

  COMPARE(tst(w0, Operand(w1)), "tst w0, w1");
  COMPARE(tst(w2, Operand(w3, ROR, 10)), "tst w2, w3, ror #10");
  COMPARE(tst(x0, Operand(x1)), "tst x0, x1");
  COMPARE(tst(x2, Operand(x3, ROR, 42)), "tst x2, x3, ror #42");

  COMPARE(orn(w0, wzr, Operand(w1)), "mvn w0, w1");
  COMPARE(orn(w2, wzr, Operand(w3, ASR, 5)), "mvn w2, w3, asr #5");
  COMPARE(orn(x0, xzr, Operand(x1)), "mvn x0, x1");
  COMPARE(orn(x2, xzr, Operand(x3, ASR, 42)), "mvn x2, x3, asr #42");

  COMPARE(orr(w0, wzr, Operand(w1)), "mov w0, w1");
  COMPARE(orr(x0, xzr, Operand(x1)), "mov x0, x1");
  COMPARE(orr(w16, wzr, Operand(w17, LSL, 1)), "orr w16, wzr, w17, lsl #1");
  COMPARE(orr(x16, xzr, Operand(x17, ASR, 2)), "orr x16, xzr, x17, asr #2");

  CLEANUP();
}


TEST_(dp_2_source) {
  SET_UP_ASM();

  COMPARE(lslv(w0, w1, w2), "lsl w0, w1, w2");
  COMPARE(lslv(x3, x4, x5), "lsl x3, x4, x5");
  COMPARE(lsrv(w6, w7, w8), "lsr w6, w7, w8");
  COMPARE(lsrv(x9, x10, x11), "lsr x9, x10, x11");
  COMPARE(asrv(w12, w13, w14), "asr w12, w13, w14");
  COMPARE(asrv(x15, x16, x17), "asr x15, x16, x17");
  COMPARE(rorv(w18, w19, w20), "ror w18, w19, w20");
  COMPARE(rorv(x21, x22, x23), "ror x21, x22, x23");

  CLEANUP();
}


TEST_(adr) {
  SET_UP_ASM();

  COMPARE_PREFIX(adr(x0, 0), "adr x0, #+0x0");
  COMPARE_PREFIX(adr(x1, 1), "adr x1, #+0x1");
  COMPARE_PREFIX(adr(x2, -1), "adr x2, #-0x1");
  COMPARE_PREFIX(adr(x3, 4), "adr x3, #+0x4");
  COMPARE_PREFIX(adr(x4, -4), "adr x4, #-0x4");
  COMPARE_PREFIX(adr(x5, 0x000fffff), "adr x5, #+0xfffff");
  COMPARE_PREFIX(adr(x6, -0x00100000), "adr x6, #-0x100000");
  COMPARE_PREFIX(adr(xzr, 0), "adr xzr, #+0x0");

  CLEANUP();
}


TEST_(branch) {
  SET_UP_ASM();

  #define INST_OFF(x) ((x) >> kInstructionSizeLog2)
  COMPARE_PREFIX(b(INST_OFF(0x4)), "b #+0x4");
  COMPARE_PREFIX(b(INST_OFF(-0x4)), "b #-0x4");
  COMPARE_PREFIX(b(INST_OFF(0x7fffffc)), "b #+0x7fffffc");
  COMPARE_PREFIX(b(INST_OFF(-0x8000000)), "b #-0x8000000");
  COMPARE_PREFIX(b(INST_OFF(0xffffc), eq), "b.eq #+0xffffc");
  COMPARE_PREFIX(b(INST_OFF(-0x100000), mi), "b.mi #-0x100000");
  COMPARE_PREFIX(bl(INST_OFF(0x4)), "bl #+0x4");
  COMPARE_PREFIX(bl(INST_OFF(-0x4)), "bl #-0x4");
  COMPARE_PREFIX(bl(INST_OFF(0xffffc)), "bl #+0xffffc");
  COMPARE_PREFIX(bl(INST_OFF(-0x100000)), "bl #-0x100000");
  COMPARE_PREFIX(cbz(w0, INST_OFF(0xffffc)), "cbz w0, #+0xffffc");
  COMPARE_PREFIX(cbz(x1, INST_OFF(-0x100000)), "cbz x1, #-0x100000");
  COMPARE_PREFIX(cbnz(w2, INST_OFF(0xffffc)), "cbnz w2, #+0xffffc");
  COMPARE_PREFIX(cbnz(x3, INST_OFF(-0x100000)), "cbnz x3, #-0x100000");
  COMPARE_PREFIX(tbz(w4, 0, INST_OFF(0x7ffc)), "tbz w4, #0, #+0x7ffc");
  COMPARE_PREFIX(tbz(x5, 63, INST_OFF(-0x8000)), "tbz x5, #63, #-0x8000");
  COMPARE_PREFIX(tbz(w6, 31, INST_OFF(0)), "tbz w6, #31, #+0x0");
  COMPARE_PREFIX(tbz(x7, 31, INST_OFF(0x4)), "tbz w7, #31, #+0x4");
  COMPARE_PREFIX(tbz(x8, 32, INST_OFF(0x8)), "tbz x8, #32, #+0x8");
  COMPARE_PREFIX(tbnz(w8, 0, INST_OFF(0x7ffc)), "tbnz w8, #0, #+0x7ffc");
  COMPARE_PREFIX(tbnz(x9, 63, INST_OFF(-0x8000)), "tbnz x9, #63, #-0x8000");
  COMPARE_PREFIX(tbnz(w10, 31, INST_OFF(0)), "tbnz w10, #31, #+0x0");
  COMPARE_PREFIX(tbnz(x11, 31, INST_OFF(0x4)), "tbnz w11, #31, #+0x4");
  COMPARE_PREFIX(tbnz(x12, 32, INST_OFF(0x8)), "tbnz x12, #32, #+0x8");
  COMPARE(br(x0), "br x0");
  COMPARE(blr(x1), "blr x1");
  COMPARE(ret(x2), "ret x2");
  COMPARE(ret(lr), "ret")

  CLEANUP();
}


TEST_(load_store) {
  SET_UP_ASM();

  COMPARE(ldr(w0, MemOperand(x1)), "ldr w0, [x1]");
  COMPARE(ldr(w2, MemOperand(x3, 4)), "ldr w2, [x3, #4]");
  COMPARE(ldr(w4, MemOperand(x5, 16380)), "ldr w4, [x5, #16380]");
  COMPARE(ldr(x6, MemOperand(x7)), "ldr x6, [x7]");
  COMPARE(ldr(x8, MemOperand(x9, 8)), "ldr x8, [x9, #8]");
  COMPARE(ldr(x10, MemOperand(x11, 32760)), "ldr x10, [x11, #32760]");
  COMPARE(str(w12, MemOperand(x13)), "str w12, [x13]");
  COMPARE(str(w14, MemOperand(x15, 4)), "str w14, [x15, #4]");
  COMPARE(str(w16, MemOperand(x17, 16380)), "str w16, [x17, #16380]");
  COMPARE(str(x18, MemOperand(x19)), "str x18, [x19]");
  COMPARE(str(x20, MemOperand(x21, 8)), "str x20, [x21, #8]");
  COMPARE(str(x22, MemOperand(x23, 32760)), "str x22, [x23, #32760]");

  COMPARE(ldr(w0, MemOperand(x1, 4, PreIndex)), "ldr w0, [x1, #4]!");
  COMPARE(ldr(w2, MemOperand(x3, 255, PreIndex)), "ldr w2, [x3, #255]!");
  COMPARE(ldr(w4, MemOperand(x5, -256, PreIndex)), "ldr w4, [x5, #-256]!");
  COMPARE(ldr(x6, MemOperand(x7, 8, PreIndex)), "ldr x6, [x7, #8]!");
  COMPARE(ldr(x8, MemOperand(x9, 255, PreIndex)), "ldr x8, [x9, #255]!");
  COMPARE(ldr(x10, MemOperand(x11, -256, PreIndex)), "ldr x10, [x11, #-256]!");
  COMPARE(str(w12, MemOperand(x13, 4, PreIndex)), "str w12, [x13, #4]!");
  COMPARE(str(w14, MemOperand(x15, 255, PreIndex)), "str w14, [x15, #255]!");
  COMPARE(str(w16, MemOperand(x17, -256, PreIndex)), "str w16, [x17, #-256]!");
  COMPARE(str(x18, MemOperand(x19, 8, PreIndex)), "str x18, [x19, #8]!");
  COMPARE(str(x20, MemOperand(x21, 255, PreIndex)), "str x20, [x21, #255]!");
  COMPARE(str(x22, MemOperand(x23, -256, PreIndex)), "str x22, [x23, #-256]!");

  COMPARE(ldr(w0, MemOperand(x1, 4, PostIndex)), "ldr w0, [x1], #4");
  COMPARE(ldr(w2, MemOperand(x3, 255, PostIndex)), "ldr w2, [x3], #255");
  COMPARE(ldr(w4, MemOperand(x5, -256, PostIndex)), "ldr w4, [x5], #-256");
  COMPARE(ldr(x6, MemOperand(x7, 8, PostIndex)), "ldr x6, [x7], #8");
  COMPARE(ldr(x8, MemOperand(x9, 255, PostIndex)), "ldr x8, [x9], #255");
  COMPARE(ldr(x10, MemOperand(x11, -256, PostIndex)), "ldr x10, [x11], #-256");
  COMPARE(str(w12, MemOperand(x13, 4, PostIndex)), "str w12, [x13], #4");
  COMPARE(str(w14, MemOperand(x15, 255, PostIndex)), "str w14, [x15], #255");
  COMPARE(str(w16, MemOperand(x17, -256, PostIndex)), "str w16, [x17], #-256");
  COMPARE(str(x18, MemOperand(x19, 8, PostIndex)), "str x18, [x19], #8");
  COMPARE(str(x20, MemOperand(x21, 255, PostIndex)), "str x20, [x21], #255");
  COMPARE(str(x22, MemOperand(x23, -256, PostIndex)), "str x22, [x23], #-256");

  // TODO(all): Fix this for jssp.
  COMPARE(ldr(w24, MemOperand(jssp)), "ldr w24, [jssp]");
  COMPARE(ldr(x25, MemOperand(jssp, 8)), "ldr x25, [jssp, #8]");
  COMPARE(str(w26, MemOperand(jssp, 4, PreIndex)), "str w26, [jssp, #4]!");
  COMPARE(str(cp, MemOperand(jssp, -8, PostIndex)), "str cp, [jssp], #-8");

  COMPARE(ldrsw(x0, MemOperand(x1)), "ldrsw x0, [x1]");
  COMPARE(ldrsw(x2, MemOperand(x3, 8)), "ldrsw x2, [x3, #8]");
  COMPARE(ldrsw(x4, MemOperand(x5, 42, PreIndex)), "ldrsw x4, [x5, #42]!");
  COMPARE(ldrsw(x6, MemOperand(x7, -11, PostIndex)), "ldrsw x6, [x7], #-11");

  CLEANUP();
}


TEST_(load_store_regoffset) {
  SET_UP_ASM();

  COMPARE(ldr(w0, MemOperand(x1, w2, UXTW)), "ldr w0, [x1, w2, uxtw]");
  COMPARE(ldr(w3, MemOperand(x4, w5, UXTW, 2)), "ldr w3, [x4, w5, uxtw #2]");
  COMPARE(ldr(w6, MemOperand(x7, x8)), "ldr w6, [x7, x8]");
  COMPARE(ldr(w9, MemOperand(x10, x11, LSL, 2)), "ldr w9, [x10, x11, lsl #2]");
  COMPARE(ldr(w12, MemOperand(x13, w14, SXTW)), "ldr w12, [x13, w14, sxtw]");
  COMPARE(ldr(w15, MemOperand(x16, w17, SXTW, 2)),
          "ldr w15, [x16, w17, sxtw #2]");
  COMPARE(ldr(w18, MemOperand(x19, x20, SXTX)), "ldr w18, [x19, x20, sxtx]");
  COMPARE(ldr(w21, MemOperand(x22, x23, SXTX, 2)),
          "ldr w21, [x22, x23, sxtx #2]");
  COMPARE(ldr(x0, MemOperand(x1, w2, UXTW)), "ldr x0, [x1, w2, uxtw]");
  COMPARE(ldr(x3, MemOperand(x4, w5, UXTW, 3)), "ldr x3, [x4, w5, uxtw #3]");
  COMPARE(ldr(x6, MemOperand(x7, x8)), "ldr x6, [x7, x8]");
  COMPARE(ldr(x9, MemOperand(x10, x11, LSL, 3)), "ldr x9, [x10, x11, lsl #3]");
  COMPARE(ldr(x12, MemOperand(x13, w14, SXTW)), "ldr x12, [x13, w14, sxtw]");
  COMPARE(ldr(x15, MemOperand(x16, w17, SXTW, 3)),
          "ldr x15, [x16, w17, sxtw #3]");
  COMPARE(ldr(x18, MemOperand(x19, x20, SXTX)), "ldr x18, [x19, x20, sxtx]");
  COMPARE(ldr(x21, MemOperand(x22, x23, SXTX, 3)),
          "ldr x21, [x22, x23, sxtx #3]");

  COMPARE(str(w0, MemOperand(x1, w2, UXTW)), "str w0, [x1, w2, uxtw]");
  COMPARE(str(w3, MemOperand(x4, w5, UXTW, 2)), "str w3, [x4, w5, uxtw #2]");
  COMPARE(str(w6, MemOperand(x7, x8)), "str w6, [x7, x8]");
  COMPARE(str(w9, MemOperand(x10, x11, LSL, 2)), "str w9, [x10, x11, lsl #2]");
  COMPARE(str(w12, MemOperand(x13, w14, SXTW)), "str w12, [x13, w14, sxtw]");
  COMPARE(str(w15, MemOperand(x16, w17, SXTW, 2)),
          "str w15, [x16, w17, sxtw #2]");
  COMPARE(str(w18, MemOperand(x19, x20, SXTX)), "str w18, [x19, x20, sxtx]");
  COMPARE(str(w21, MemOperand(x22, x23, SXTX, 2)),
          "str w21, [x22, x23, sxtx #2]");
  COMPARE(str(x0, MemOperand(x1, w2, UXTW)), "str x0, [x1, w2, uxtw]");
  COMPARE(str(x3, MemOperand(x4, w5, UXTW, 3)), "str x3, [x4, w5, uxtw #3]");
  COMPARE(str(x6, MemOperand(x7, x8)), "str x6, [x7, x8]");
  COMPARE(str(x9, MemOperand(x10, x11, LSL, 3)), "str x9, [x10, x11, lsl #3]");
  COMPARE(str(x12, MemOperand(x13, w14, SXTW)), "str x12, [x13, w14, sxtw]");
  COMPARE(str(x15, MemOperand(x16, w17, SXTW, 3)),
          "str x15, [x16, w17, sxtw #3]");
  COMPARE(str(x18, MemOperand(x19, x20, SXTX)), "str x18, [x19, x20, sxtx]");
  COMPARE(str(x21, MemOperand(x22, x23, SXTX, 3)),
          "str x21, [x22, x23, sxtx #3]");

  COMPARE(ldrb(w0, MemOperand(x1, w2, UXTW)), "ldrb w0, [x1, w2, uxtw]");
  COMPARE(ldrb(w6, MemOperand(x7, x8)), "ldrb w6, [x7, x8]");
  COMPARE(ldrb(w12, MemOperand(x13, w14, SXTW)), "ldrb w12, [x13, w14, sxtw]");
  COMPARE(ldrb(w18, MemOperand(x19, x20, SXTX)), "ldrb w18, [x19, x20, sxtx]");
  COMPARE(strb(w0, MemOperand(x1, w2, UXTW)), "strb w0, [x1, w2, uxtw]");
  COMPARE(strb(w6, MemOperand(x7, x8)), "strb w6, [x7, x8]");
  COMPARE(strb(w12, MemOperand(x13, w14, SXTW)), "strb w12, [x13, w14, sxtw]");
  COMPARE(strb(w18, MemOperand(x19, x20, SXTX)), "strb w18, [x19, x20, sxtx]");

  COMPARE(ldrh(w0, MemOperand(x1, w2, UXTW)), "ldrh w0, [x1, w2, uxtw]");
  COMPARE(ldrh(w3, MemOperand(x4, w5, UXTW, 1)), "ldrh w3, [x4, w5, uxtw #1]");
  COMPARE(ldrh(w6, MemOperand(x7, x8)), "ldrh w6, [x7, x8]");
  COMPARE(ldrh(w9, MemOperand(x10, x11, LSL, 1)),
          "ldrh w9, [x10, x11, lsl #1]");
  COMPARE(ldrh(w12, MemOperand(x13, w14, SXTW)), "ldrh w12, [x13, w14, sxtw]");
  COMPARE(ldrh(w15, MemOperand(x16, w17, SXTW, 1)),
          "ldrh w15, [x16, w17, sxtw #1]");
  COMPARE(ldrh(w18, MemOperand(x19, x20, SXTX)), "ldrh w18, [x19, x20, sxtx]");
  COMPARE(ldrh(w21, MemOperand(x22, x23, SXTX, 1)),
          "ldrh w21, [x22, x23, sxtx #1]");
  COMPARE(strh(w0, MemOperand(x1, w2, UXTW)), "strh w0, [x1, w2, uxtw]");
  COMPARE(strh(w3, MemOperand(x4, w5, UXTW, 1)), "strh w3, [x4, w5, uxtw #1]");
  COMPARE(strh(w6, MemOperand(x7, x8)), "strh w6, [x7, x8]");
  COMPARE(strh(w9, MemOperand(x10, x11, LSL, 1)),
          "strh w9, [x10, x11, lsl #1]");
  COMPARE(strh(w12, MemOperand(x13, w14, SXTW)), "strh w12, [x13, w14, sxtw]");
  COMPARE(strh(w15, MemOperand(x16, w17, SXTW, 1)),
          "strh w15, [x16, w17, sxtw #1]");
  COMPARE(strh(w18, MemOperand(x19, x20, SXTX)), "strh w18, [x19, x20, sxtx]");
  COMPARE(strh(w21, MemOperand(x22, x23, SXTX, 1)),
          "strh w21, [x22, x23, sxtx #1]");

  // TODO(all): Fix this for jssp.
  COMPARE(ldr(x0, MemOperand(jssp, wzr, SXTW)), "ldr x0, [jssp, wzr, sxtw]");
  COMPARE(str(x1, MemOperand(jssp, xzr)), "str x1, [jssp, xzr]");

  CLEANUP();
}


TEST_(load_store_byte) {
  SET_UP_ASM();

  COMPARE(ldrb(w0, MemOperand(x1)), "ldrb w0, [x1]");
  COMPARE(ldrb(x2, MemOperand(x3)), "ldrb w2, [x3]");
  COMPARE(ldrb(w4, MemOperand(x5, 4095)), "ldrb w4, [x5, #4095]");
  COMPARE(ldrb(w6, MemOperand(x7, 255, PreIndex)), "ldrb w6, [x7, #255]!");
  COMPARE(ldrb(w8, MemOperand(x9, -256, PreIndex)), "ldrb w8, [x9, #-256]!");
  COMPARE(ldrb(w10, MemOperand(x11, 255, PostIndex)), "ldrb w10, [x11], #255");
  COMPARE(ldrb(w12, MemOperand(x13, -256, PostIndex)),
          "ldrb w12, [x13], #-256");
  COMPARE(strb(w14, MemOperand(x15)), "strb w14, [x15]");
  COMPARE(strb(x16, MemOperand(x17)), "strb w16, [x17]");
  COMPARE(strb(w18, MemOperand(x19, 4095)), "strb w18, [x19, #4095]");
  COMPARE(strb(w20, MemOperand(x21, 255, PreIndex)), "strb w20, [x21, #255]!");
  COMPARE(strb(w22, MemOperand(x23, -256, PreIndex)),
          "strb w22, [x23, #-256]!");
  COMPARE(strb(w24, MemOperand(x25, 255, PostIndex)), "strb w24, [x25], #255");
  COMPARE(strb(w26, MemOperand(cp, -256, PostIndex)),
          "strb w26, [cp], #-256");
  // TODO(all): Fix this for jssp.
  COMPARE(ldrb(w28, MemOperand(jssp, 3, PostIndex)), "ldrb w28, [jssp], #3");
  COMPARE(strb(fp, MemOperand(jssp, -42, PreIndex)), "strb w29, [jssp, #-42]!");
  COMPARE(ldrsb(w0, MemOperand(x1)), "ldrsb w0, [x1]");
  COMPARE(ldrsb(x2, MemOperand(x3, 8)), "ldrsb x2, [x3, #8]");
  COMPARE(ldrsb(w4, MemOperand(x5, 42, PreIndex)), "ldrsb w4, [x5, #42]!");
  COMPARE(ldrsb(x6, MemOperand(x7, -11, PostIndex)), "ldrsb x6, [x7], #-11");

  CLEANUP();
}


TEST_(load_store_half) {
  SET_UP_ASM();

  COMPARE(ldrh(w0, MemOperand(x1)), "ldrh w0, [x1]");
  COMPARE(ldrh(x2, MemOperand(x3)), "ldrh w2, [x3]");
  COMPARE(ldrh(w4, MemOperand(x5, 8190)), "ldrh w4, [x5, #8190]");
  COMPARE(ldrh(w6, MemOperand(x7, 255, PreIndex)), "ldrh w6, [x7, #255]!");
  COMPARE(ldrh(w8, MemOperand(x9, -256, PreIndex)), "ldrh w8, [x9, #-256]!");
  COMPARE(ldrh(w10, MemOperand(x11, 255, PostIndex)), "ldrh w10, [x11], #255");
  COMPARE(ldrh(w12, MemOperand(x13, -256, PostIndex)),
          "ldrh w12, [x13], #-256");
  COMPARE(strh(w14, MemOperand(x15)), "strh w14, [x15]");
  COMPARE(strh(x16, MemOperand(x17)), "strh w16, [x17]");
  COMPARE(strh(w18, MemOperand(x19, 8190)), "strh w18, [x19, #8190]");
  COMPARE(strh(w20, MemOperand(x21, 255, PreIndex)), "strh w20, [x21, #255]!");
  COMPARE(strh(w22, MemOperand(x23, -256, PreIndex)),
          "strh w22, [x23, #-256]!");
  COMPARE(strh(w24, MemOperand(x25, 255, PostIndex)), "strh w24, [x25], #255");
  COMPARE(strh(w26, MemOperand(cp, -256, PostIndex)),
          "strh w26, [cp], #-256");
  // TODO(all): Fix this for jssp.
  COMPARE(ldrh(w28, MemOperand(jssp, 3, PostIndex)), "ldrh w28, [jssp], #3");
  COMPARE(strh(fp, MemOperand(jssp, -42, PreIndex)), "strh w29, [jssp, #-42]!");
  COMPARE(ldrh(w30, MemOperand(x0, 255)), "ldurh w30, [x0, #255]");
  COMPARE(ldrh(x1, MemOperand(x2, -256)), "ldurh w1, [x2, #-256]");
  COMPARE(strh(w3, MemOperand(x4, 255)), "sturh w3, [x4, #255]");
  COMPARE(strh(x5, MemOperand(x6, -256)), "sturh w5, [x6, #-256]");
  COMPARE(ldrsh(w0, MemOperand(x1)), "ldrsh w0, [x1]");
  COMPARE(ldrsh(w2, MemOperand(x3, 8)), "ldrsh w2, [x3, #8]");
  COMPARE(ldrsh(w4, MemOperand(x5, 42, PreIndex)), "ldrsh w4, [x5, #42]!");
  COMPARE(ldrsh(x6, MemOperand(x7, -11, PostIndex)), "ldrsh x6, [x7], #-11");

  CLEANUP();
}


TEST_(load_store_fp) {
  SET_UP_ASM();

  COMPARE(ldr(s0, MemOperand(x1)), "ldr s0, [x1]");
  COMPARE(ldr(s2, MemOperand(x3, 4)), "ldr s2, [x3, #4]");
  COMPARE(ldr(s4, MemOperand(x5, 16380)), "ldr s4, [x5, #16380]");
  COMPARE(ldr(d6, MemOperand(x7)), "ldr d6, [x7]");
  COMPARE(ldr(d8, MemOperand(x9, 8)), "ldr d8, [x9, #8]");
  COMPARE(ldr(d10, MemOperand(x11, 32760)), "ldr d10, [x11, #32760]");
  COMPARE(str(s12, MemOperand(x13)), "str s12, [x13]");
  COMPARE(str(s14, MemOperand(x15, 4)), "str s14, [x15, #4]");
  COMPARE(str(s16, MemOperand(x17, 16380)), "str s16, [x17, #16380]");
  COMPARE(str(d18, MemOperand(x19)), "str d18, [x19]");
  COMPARE(str(d20, MemOperand(x21, 8)), "str d20, [x21, #8]");
  COMPARE(str(d22, MemOperand(x23, 32760)), "str d22, [x23, #32760]");

  COMPARE(ldr(s0, MemOperand(x1, 4, PreIndex)), "ldr s0, [x1, #4]!");
  COMPARE(ldr(s2, MemOperand(x3, 255, PreIndex)), "ldr s2, [x3, #255]!");
  COMPARE(ldr(s4, MemOperand(x5, -256, PreIndex)), "ldr s4, [x5, #-256]!");
  COMPARE(ldr(d6, MemOperand(x7, 8, PreIndex)), "ldr d6, [x7, #8]!");
  COMPARE(ldr(d8, MemOperand(x9, 255, PreIndex)), "ldr d8, [x9, #255]!");
  COMPARE(ldr(d10, MemOperand(x11, -256, PreIndex)), "ldr d10, [x11, #-256]!");
  COMPARE(str(s12, MemOperand(x13, 4, PreIndex)), "str s12, [x13, #4]!");
  COMPARE(str(s14, MemOperand(x15, 255, PreIndex)), "str s14, [x15, #255]!");
  COMPARE(str(s16, MemOperand(x17, -256, PreIndex)), "str s16, [x17, #-256]!");
  COMPARE(str(d18, MemOperand(x19, 8, PreIndex)), "str d18, [x19, #8]!");
  COMPARE(str(d20, MemOperand(x21, 255, PreIndex)), "str d20, [x21, #255]!");
  COMPARE(str(d22, MemOperand(x23, -256, PreIndex)), "str d22, [x23, #-256]!");

  COMPARE(ldr(s0, MemOperand(x1, 4, PostIndex)), "ldr s0, [x1], #4");
  COMPARE(ldr(s2, MemOperand(x3, 255, PostIndex)), "ldr s2, [x3], #255");
  COMPARE(ldr(s4, MemOperand(x5, -256, PostIndex)), "ldr s4, [x5], #-256");
  COMPARE(ldr(d6, MemOperand(x7, 8, PostIndex)), "ldr d6, [x7], #8");
  COMPARE(ldr(d8, MemOperand(x9, 255, PostIndex)), "ldr d8, [x9], #255");
  COMPARE(ldr(d10, MemOperand(x11, -256, PostIndex)), "ldr d10, [x11], #-256");
  COMPARE(str(s12, MemOperand(x13, 4, PostIndex)), "str s12, [x13], #4");
  COMPARE(str(s14, MemOperand(x15, 255, PostIndex)), "str s14, [x15], #255");
  COMPARE(str(s16, MemOperand(x17, -256, PostIndex)), "str s16, [x17], #-256");
  COMPARE(str(d18, MemOperand(x19, 8, PostIndex)), "str d18, [x19], #8");
  COMPARE(str(d20, MemOperand(x21, 255, PostIndex)), "str d20, [x21], #255");
  COMPARE(str(d22, MemOperand(x23, -256, PostIndex)), "str d22, [x23], #-256");

  // TODO(all): Fix this for jssp.
  COMPARE(ldr(s24, MemOperand(jssp)), "ldr s24, [jssp]");
  COMPARE(ldr(d25, MemOperand(jssp, 8)), "ldr d25, [jssp, #8]");
  COMPARE(str(s26, MemOperand(jssp, 4, PreIndex)), "str s26, [jssp, #4]!");
  COMPARE(str(d27, MemOperand(jssp, -8, PostIndex)), "str d27, [jssp], #-8");

  CLEANUP();
}


TEST_(load_store_unscaled) {
  SET_UP_ASM();

  COMPARE(ldr(w0, MemOperand(x1, 1)), "ldur w0, [x1, #1]");
  COMPARE(ldr(w2, MemOperand(x3, -1)), "ldur w2, [x3, #-1]");
  COMPARE(ldr(w4, MemOperand(x5, 255)), "ldur w4, [x5, #255]");
  COMPARE(ldr(w6, MemOperand(x7, -256)), "ldur w6, [x7, #-256]");
  COMPARE(ldr(x8, MemOperand(x9, 1)), "ldur x8, [x9, #1]");
  COMPARE(ldr(x10, MemOperand(x11, -1)), "ldur x10, [x11, #-1]");
  COMPARE(ldr(x12, MemOperand(x13, 255)), "ldur x12, [x13, #255]");
  COMPARE(ldr(x14, MemOperand(x15, -256)), "ldur x14, [x15, #-256]");
  COMPARE(str(w16, MemOperand(x17, 1)), "stur w16, [x17, #1]");
  COMPARE(str(w18, MemOperand(x19, -1)), "stur w18, [x19, #-1]");
  COMPARE(str(w20, MemOperand(x21, 255)), "stur w20, [x21, #255]");
  COMPARE(str(w22, MemOperand(x23, -256)), "stur w22, [x23, #-256]");
  COMPARE(str(x24, MemOperand(x25, 1)), "stur x24, [x25, #1]");
  COMPARE(str(x26, MemOperand(cp, -1)), "stur x26, [cp, #-1]");
  COMPARE(str(jssp, MemOperand(fp, 255)), "stur jssp, [fp, #255]");
  COMPARE(str(lr, MemOperand(x0, -256)), "stur lr, [x0, #-256]");
  COMPARE(ldr(w0, MemOperand(csp, 1)), "ldur w0, [csp, #1]");
  COMPARE(str(x1, MemOperand(csp, -1)), "stur x1, [csp, #-1]");
  COMPARE(ldrb(w2, MemOperand(x3, -2)), "ldurb w2, [x3, #-2]");
  COMPARE(ldrsb(w4, MemOperand(x5, -3)), "ldursb w4, [x5, #-3]");
  COMPARE(ldrsb(x6, MemOperand(x7, -4)), "ldursb x6, [x7, #-4]");
  COMPARE(ldrh(w8, MemOperand(x9, -5)), "ldurh w8, [x9, #-5]");
  COMPARE(ldrsh(w10, MemOperand(x11, -6)), "ldursh w10, [x11, #-6]");
  COMPARE(ldrsh(x12, MemOperand(x13, -7)), "ldursh x12, [x13, #-7]");
  COMPARE(ldrsw(x14, MemOperand(x15, -8)), "ldursw x14, [x15, #-8]");

  CLEANUP();
}


TEST_(load_store_pair) {
  SET_UP_ASM();

  COMPARE(ldp(w0, w1, MemOperand(x2)), "ldp w0, w1, [x2]");
  COMPARE(ldp(x3, x4, MemOperand(x5)), "ldp x3, x4, [x5]");
  COMPARE(ldp(w6, w7, MemOperand(x8, 4)), "ldp w6, w7, [x8, #4]");
  COMPARE(ldp(x9, x10, MemOperand(x11, 8)), "ldp x9, x10, [x11, #8]");
  COMPARE(ldp(w12, w13, MemOperand(x14, 252)), "ldp w12, w13, [x14, #252]");
  COMPARE(ldp(x15, x16, MemOperand(x17, 504)), "ldp x15, x16, [x17, #504]");
  COMPARE(ldp(w18, w19, MemOperand(x20, -256)), "ldp w18, w19, [x20, #-256]");
  COMPARE(ldp(x21, x22, MemOperand(x23, -512)), "ldp x21, x22, [x23, #-512]");
  COMPARE(ldp(w24, w25, MemOperand(x26, 252, PreIndex)),
          "ldp w24, w25, [x26, #252]!");
  COMPARE(ldp(cp, jssp, MemOperand(fp, 504, PreIndex)),
          "ldp cp, jssp, [fp, #504]!");
  COMPARE(ldp(w30, w0, MemOperand(x1, -256, PreIndex)),
          "ldp w30, w0, [x1, #-256]!");
  COMPARE(ldp(x2, x3, MemOperand(x4, -512, PreIndex)),
          "ldp x2, x3, [x4, #-512]!");
  COMPARE(ldp(w5, w6, MemOperand(x7, 252, PostIndex)),
          "ldp w5, w6, [x7], #252");
  COMPARE(ldp(x8, x9, MemOperand(x10, 504, PostIndex)),
          "ldp x8, x9, [x10], #504");
  COMPARE(ldp(w11, w12, MemOperand(x13, -256, PostIndex)),
          "ldp w11, w12, [x13], #-256");
  COMPARE(ldp(x14, x15, MemOperand(x16, -512, PostIndex)),
          "ldp x14, x15, [x16], #-512");

  COMPARE(ldp(s17, s18, MemOperand(x19)), "ldp s17, s18, [x19]");
  COMPARE(ldp(s20, s21, MemOperand(x22, 252)), "ldp s20, s21, [x22, #252]");
  COMPARE(ldp(s23, s24, MemOperand(x25, -256)), "ldp s23, s24, [x25, #-256]");
  COMPARE(ldp(s26, s27, MemOperand(jssp, 252, PreIndex)),
          "ldp s26, s27, [jssp, #252]!");
  COMPARE(ldp(s29, s30, MemOperand(fp, -256, PreIndex)),
          "ldp s29, s30, [fp, #-256]!");
  COMPARE(ldp(s31, s0, MemOperand(x1, 252, PostIndex)),
          "ldp s31, s0, [x1], #252");
  COMPARE(ldp(s2, s3, MemOperand(x4, -256, PostIndex)),
          "ldp s2, s3, [x4], #-256");
  COMPARE(ldp(d17, d18, MemOperand(x19)), "ldp d17, d18, [x19]");
  COMPARE(ldp(d20, d21, MemOperand(x22, 504)), "ldp d20, d21, [x22, #504]");
  COMPARE(ldp(d23, d24, MemOperand(x25, -512)), "ldp d23, d24, [x25, #-512]");
  COMPARE(ldp(d26, d27, MemOperand(jssp, 504, PreIndex)),
          "ldp d26, d27, [jssp, #504]!");
  COMPARE(ldp(d29, d30, MemOperand(fp, -512, PreIndex)),
          "ldp d29, d30, [fp, #-512]!");
  COMPARE(ldp(d31, d0, MemOperand(x1, 504, PostIndex)),
          "ldp d31, d0, [x1], #504");
  COMPARE(ldp(d2, d3, MemOperand(x4, -512, PostIndex)),
          "ldp d2, d3, [x4], #-512");

  COMPARE(stp(w0, w1, MemOperand(x2)), "stp w0, w1, [x2]");
  COMPARE(stp(x3, x4, MemOperand(x5)), "stp x3, x4, [x5]");
  COMPARE(stp(w6, w7, MemOperand(x8, 4)), "stp w6, w7, [x8, #4]");
  COMPARE(stp(x9, x10, MemOperand(x11, 8)), "stp x9, x10, [x11, #8]");
  COMPARE(stp(w12, w13, MemOperand(x14, 252)), "stp w12, w13, [x14, #252]");
  COMPARE(stp(x15, x16, MemOperand(x17, 504)), "stp x15, x16, [x17, #504]");
  COMPARE(stp(w18, w19, MemOperand(x20, -256)), "stp w18, w19, [x20, #-256]");
  COMPARE(stp(x21, x22, MemOperand(x23, -512)), "stp x21, x22, [x23, #-512]");
  COMPARE(stp(w24, w25, MemOperand(x26, 252, PreIndex)),
          "stp w24, w25, [x26, #252]!");
  COMPARE(stp(cp, jssp, MemOperand(fp, 504, PreIndex)),
          "stp cp, jssp, [fp, #504]!");
  COMPARE(stp(w30, w0, MemOperand(x1, -256, PreIndex)),
          "stp w30, w0, [x1, #-256]!");
  COMPARE(stp(x2, x3, MemOperand(x4, -512, PreIndex)),
          "stp x2, x3, [x4, #-512]!");
  COMPARE(stp(w5, w6, MemOperand(x7, 252, PostIndex)),
          "stp w5, w6, [x7], #252");
  COMPARE(stp(x8, x9, MemOperand(x10, 504, PostIndex)),
          "stp x8, x9, [x10], #504");
  COMPARE(stp(w11, w12, MemOperand(x13, -256, PostIndex)),
          "stp w11, w12, [x13], #-256");
  COMPARE(stp(x14, x15, MemOperand(x16, -512, PostIndex)),
          "stp x14, x15, [x16], #-512");

  COMPARE(stp(s17, s18, MemOperand(x19)), "stp s17, s18, [x19]");
  COMPARE(stp(s20, s21, MemOperand(x22, 252)), "stp s20, s21, [x22, #252]");
  COMPARE(stp(s23, s24, MemOperand(x25, -256)), "stp s23, s24, [x25, #-256]");
  COMPARE(stp(s26, s27, MemOperand(jssp, 252, PreIndex)),
          "stp s26, s27, [jssp, #252]!");
  COMPARE(stp(s29, s30, MemOperand(fp, -256, PreIndex)),
          "stp s29, s30, [fp, #-256]!");
  COMPARE(stp(s31, s0, MemOperand(x1, 252, PostIndex)),
          "stp s31, s0, [x1], #252");
  COMPARE(stp(s2, s3, MemOperand(x4, -256, PostIndex)),
          "stp s2, s3, [x4], #-256");
  COMPARE(stp(d17, d18, MemOperand(x19)), "stp d17, d18, [x19]");
  COMPARE(stp(d20, d21, MemOperand(x22, 504)), "stp d20, d21, [x22, #504]");
  COMPARE(stp(d23, d24, MemOperand(x25, -512)), "stp d23, d24, [x25, #-512]");
  COMPARE(stp(d26, d27, MemOperand(jssp, 504, PreIndex)),
          "stp d26, d27, [jssp, #504]!");
  COMPARE(stp(d29, d30, MemOperand(fp, -512, PreIndex)),
          "stp d29, d30, [fp, #-512]!");
  COMPARE(stp(d31, d0, MemOperand(x1, 504, PostIndex)),
          "stp d31, d0, [x1], #504");
  COMPARE(stp(d2, d3, MemOperand(x4, -512, PostIndex)),
          "stp d2, d3, [x4], #-512");

  // TODO(all): Update / Restore this test.
  COMPARE(ldp(w16, w17, MemOperand(jssp, 4, PostIndex)),
          "ldp w16, w17, [jssp], #4");
  COMPARE(stp(x18, x19, MemOperand(jssp, -8, PreIndex)),
          "stp x18, x19, [jssp, #-8]!");
  COMPARE(ldp(s30, s31, MemOperand(jssp, 12, PostIndex)),
          "ldp s30, s31, [jssp], #12");
  COMPARE(stp(d30, d31, MemOperand(jssp, -16)),
          "stp d30, d31, [jssp, #-16]");

  COMPARE(ldpsw(x0, x1, MemOperand(x2)), "ldpsw x0, x1, [x2]");
  COMPARE(ldpsw(x3, x4, MemOperand(x5, 16)), "ldpsw x3, x4, [x5, #16]");
  COMPARE(ldpsw(x6, x7, MemOperand(x8, -32, PreIndex)),
          "ldpsw x6, x7, [x8, #-32]!");
  COMPARE(ldpsw(x9, x10, MemOperand(x11, 128, PostIndex)),
          "ldpsw x9, x10, [x11], #128");

  CLEANUP();
}

TEST_(load_store_acquire_release) {
  SET_UP_MASM();

  COMPARE(ldar(w0, x1), "ldar w0, [x1]");
  COMPARE(ldarb(w2, x3), "ldarb w2, [x3]");
  COMPARE(ldarh(w4, x5), "ldarh w4, [x5]");
  COMPARE(ldaxr(w6, x7), "ldaxr w6, [x7]");
  COMPARE(ldaxrb(w8, x9), "ldaxrb w8, [x9]");
  COMPARE(ldaxrh(w10, x11), "ldaxrh w10, [x11]");
  COMPARE(stlr(w12, x13), "stlr w12, [x13]");
  COMPARE(stlrb(w14, x15), "stlrb w14, [x15]");
  COMPARE(stlrh(w16, x17), "stlrh w16, [x17]");
  COMPARE(stlxr(w18, w19, x20), "stlxr w18, w19, [x20]");
  COMPARE(stlxrb(w21, w22, x23), "stlxrb w21, w22, [x23]");
  COMPARE(stlxrh(w24, w25, x26), "stlxrh w24, w25, [x26]");

  CLEANUP();
}

#if 0  // TODO(all): enable.
TEST_(load_literal) {
  SET_UP_ASM();

  COMPARE_PREFIX(ldr(x10, 0x1234567890abcdefUL),  "ldr x10, pc+8");
  COMPARE_PREFIX(ldr(w20, 0xfedcba09),  "ldr w20, pc+8");
  COMPARE_PREFIX(ldr(d11, 1.234),  "ldr d11, pc+8");
  COMPARE_PREFIX(ldr(s22, 2.5f),  "ldr s22, pc+8");

  CLEANUP();
}
#endif

TEST_(cond_select) {
  SET_UP_ASM();

  COMPARE(csel(w0, w1, w2, eq), "csel w0, w1, w2, eq");
  COMPARE(csel(x3, x4, x5, ne), "csel x3, x4, x5, ne");
  COMPARE(csinc(w6, w7, w8, hs), "csinc w6, w7, w8, hs");
  COMPARE(csinc(x9, x10, x11, lo), "csinc x9, x10, x11, lo");
  COMPARE(csinv(w12, w13, w14, mi), "csinv w12, w13, w14, mi");
  COMPARE(csinv(x15, x16, x17, pl), "csinv x15, x16, x17, pl");
  COMPARE(csneg(w18, w19, w20, vs), "csneg w18, w19, w20, vs");
  COMPARE(csneg(x21, x22, x23, vc), "csneg x21, x22, x23, vc");
  COMPARE(cset(w24, hi), "cset w24, hi");
  COMPARE(cset(x25, ls), "cset x25, ls");
  COMPARE(csetm(w26, ge), "csetm w26, ge");
  COMPARE(csetm(cp, lt), "csetm cp, lt");
  COMPARE(cinc(w28, w29, gt), "cinc w28, w29, gt");
  COMPARE(cinc(lr, x0, le), "cinc lr, x0, le");
  COMPARE(cinv(w1, w2, eq), "cinv w1, w2, eq");
  COMPARE(cinv(x3, x4, ne), "cinv x3, x4, ne");
  COMPARE(cneg(w5, w6, hs), "cneg w5, w6, hs");
  COMPARE(cneg(x7, x8, lo), "cneg x7, x8, lo");

  COMPARE(csel(x0, x1, x2, al), "csel x0, x1, x2, al");
  COMPARE(csel(x1, x2, x3, nv), "csel x1, x2, x3, nv");
  COMPARE(csinc(x2, x3, x4, al), "csinc x2, x3, x4, al");
  COMPARE(csinc(x3, x4, x5, nv), "csinc x3, x4, x5, nv");
  COMPARE(csinv(x4, x5, x6, al), "csinv x4, x5, x6, al");
  COMPARE(csinv(x5, x6, x7, nv), "csinv x5, x6, x7, nv");
  COMPARE(csneg(x6, x7, x8, al), "csneg x6, x7, x8, al");
  COMPARE(csneg(x7, x8, x9, nv), "csneg x7, x8, x9, nv");

  CLEANUP();
}


TEST(cond_select_macro) {
  SET_UP_MASM();

  COMPARE(Csel(w0, w1, -1, eq), "csinv w0, w1, wzr, eq");
  COMPARE(Csel(w2, w3, 0, ne), "csel w2, w3, wzr, ne");
  COMPARE(Csel(w4, w5, 1, hs), "csinc w4, w5, wzr, hs");
  COMPARE(Csel(x6, x7, -1, lo), "csinv x6, x7, xzr, lo");
  COMPARE(Csel(x8, x9, 0, mi), "csel x8, x9, xzr, mi");
  COMPARE(Csel(x10, x11, 1, pl), "csinc x10, x11, xzr, pl");

  CLEANUP();
}


TEST_(cond_cmp) {
  SET_UP_ASM();

  COMPARE(ccmn(w0, w1, NZCVFlag, eq), "ccmn w0, w1, #NZCV, eq");
  COMPARE(ccmn(x2, x3, NZCFlag, ne), "ccmn x2, x3, #NZCv, ne");
  COMPARE(ccmp(w4, w5, NZVFlag, hs), "ccmp w4, w5, #NZcV, hs");
  COMPARE(ccmp(x6, x7, NZFlag, lo), "ccmp x6, x7, #NZcv, lo");
  COMPARE(ccmn(w8, 31, NFlag, mi), "ccmn w8, #31, #Nzcv, mi");
  COMPARE(ccmn(x9, 30, NCFlag, pl), "ccmn x9, #30, #NzCv, pl");
  COMPARE(ccmp(w10, 29, NVFlag, vs), "ccmp w10, #29, #NzcV, vs");
  COMPARE(ccmp(x11, 28, NFlag, vc), "ccmp x11, #28, #Nzcv, vc");
  COMPARE(ccmn(w12, w13, NoFlag, al), "ccmn w12, w13, #nzcv, al");
  COMPARE(ccmp(x14, 27, ZVFlag, nv), "ccmp x14, #27, #nZcV, nv");

  CLEANUP();
}


TEST_(cond_cmp_macro) {
  SET_UP_MASM();

  COMPARE(Ccmp(w0, -1, VFlag, hi), "ccmn w0, #1, #nzcV, hi");
  COMPARE(Ccmp(x1, -31, CFlag, ge), "ccmn x1, #31, #nzCv, ge");
  COMPARE(Ccmn(w2, -1, CVFlag, gt), "ccmp w2, #1, #nzCV, gt");
  COMPARE(Ccmn(x3, -31, ZCVFlag, ls), "ccmp x3, #31, #nZCV, ls");

  CLEANUP();
}


TEST_(fmov_imm) {
  SET_UP_ASM();

  COMPARE(fmov(s0, 1.0f), "fmov s0, #0x70 (1.0000)");
  COMPARE(fmov(s31, -13.0f), "fmov s31, #0xaa (-13.0000)");
  COMPARE(fmov(d1, 1.0), "fmov d1, #0x70 (1.0000)");
  COMPARE(fmov(d29, -13.0), "fmov d29, #0xaa (-13.0000)");

  CLEANUP();
}


TEST_(fmov_reg) {
  SET_UP_ASM();

  COMPARE(fmov(w3, s13), "fmov w3, s13");
  COMPARE(fmov(x6, d26), "fmov x6, d26");
  COMPARE(fmov(s11, w30), "fmov s11, w30");
  COMPARE(fmov(d31, x2), "fmov d31, x2");
  COMPARE(fmov(s12, s13), "fmov s12, s13");
  COMPARE(fmov(d22, d23), "fmov d22, d23");

  CLEANUP();
}


TEST_(fp_dp1) {
  SET_UP_ASM();

  COMPARE(fabs(s0, s1), "fabs s0, s1");
  COMPARE(fabs(s31, s30), "fabs s31, s30");
  COMPARE(fabs(d2, d3), "fabs d2, d3");
  COMPARE(fabs(d31, d30), "fabs d31, d30");
  COMPARE(fneg(s4, s5), "fneg s4, s5");
  COMPARE(fneg(s31, s30), "fneg s31, s30");
  COMPARE(fneg(d6, d7), "fneg d6, d7");
  COMPARE(fneg(d31, d30), "fneg d31, d30");
  COMPARE(fsqrt(s8, s9), "fsqrt s8, s9");
  COMPARE(fsqrt(s31, s30), "fsqrt s31, s30");
  COMPARE(fsqrt(d10, d11), "fsqrt d10, d11");
  COMPARE(fsqrt(d31, d30), "fsqrt d31, d30");
  COMPARE(frinta(s10, s11), "frinta s10, s11");
  COMPARE(frinta(s31, s30), "frinta s31, s30");
  COMPARE(frinta(d12, d13), "frinta d12, d13");
  COMPARE(frinta(d31, d30), "frinta d31, d30");
  COMPARE(frintn(s10, s11), "frintn s10, s11");
  COMPARE(frintn(s31, s30), "frintn s31, s30");
  COMPARE(frintn(d12, d13), "frintn d12, d13");
  COMPARE(frintn(d31, d30), "frintn d31, d30");
  COMPARE(frintp(s10, s11), "frintp s10, s11");
  COMPARE(frintp(s31, s30), "frintp s31, s30");
  COMPARE(frintp(d12, d13), "frintp d12, d13");
  COMPARE(frintp(d31, d30), "frintp d31, d30");
  COMPARE(frintz(s10, s11), "frintz s10, s11");
  COMPARE(frintz(s31, s30), "frintz s31, s30");
  COMPARE(frintz(d12, d13), "frintz d12, d13");
  COMPARE(frintz(d31, d30), "frintz d31, d30");
  COMPARE(fcvt(d14, s15), "fcvt d14, s15");
  COMPARE(fcvt(d31, s31), "fcvt d31, s31");

  CLEANUP();
}


TEST_(fp_dp2) {
  SET_UP_ASM();

  COMPARE(fadd(s0, s1, s2), "fadd s0, s1, s2");
  COMPARE(fadd(d3, d4, d5), "fadd d3, d4, d5");
  COMPARE(fsub(s31, s30, s29), "fsub s31, s30, s29");
  COMPARE(fsub(d31, d30, d29), "fsub d31, d30, d29");
  COMPARE(fmul(s7, s8, s9), "fmul s7, s8, s9");
  COMPARE(fmul(d10, d11, d12), "fmul d10, d11, d12");
  COMPARE(fdiv(s13, s14, s15), "fdiv s13, s14, s15");
  COMPARE(fdiv(d16, d17, d18), "fdiv d16, d17, d18");
  COMPARE(fmax(s19, s20, s21), "fmax s19, s20, s21");
  COMPARE(fmax(d22, d23, d24), "fmax d22, d23, d24");
  COMPARE(fmin(s25, s26, s27), "fmin s25, s26, s27");
  COMPARE(fmin(d28, d29, d30), "fmin d28, d29, d30");
  COMPARE(fmaxnm(s31, s0, s1), "fmaxnm s31, s0, s1");
  COMPARE(fmaxnm(d2, d3, d4), "fmaxnm d2, d3, d4");
  COMPARE(fminnm(s5, s6, s7), "fminnm s5, s6, s7");
  COMPARE(fminnm(d8, d9, d10), "fminnm d8, d9, d10");

  CLEANUP();
}


TEST(fp_dp3) {
  SET_UP_ASM();

  COMPARE(fmadd(s7, s8, s9, s10), "fmadd s7, s8, s9, s10");
  COMPARE(fmadd(d10, d11, d12, d10), "fmadd d10, d11, d12, d10");
  COMPARE(fmsub(s7, s8, s9, s10), "fmsub s7, s8, s9, s10");
  COMPARE(fmsub(d10, d11, d12, d10), "fmsub d10, d11, d12, d10");

  COMPARE(fnmadd(s7, s8, s9, s10), "fnmadd s7, s8, s9, s10");
  COMPARE(fnmadd(d10, d11, d12, d10), "fnmadd d10, d11, d12, d10");
  COMPARE(fnmsub(s7, s8, s9, s10), "fnmsub s7, s8, s9, s10");
  COMPARE(fnmsub(d10, d11, d12, d10), "fnmsub d10, d11, d12, d10");

  CLEANUP();
}


TEST_(fp_compare) {
  SET_UP_ASM();

  COMPARE(fcmp(s0, s1), "fcmp s0, s1");
  COMPARE(fcmp(s31, s30), "fcmp s31, s30");
  COMPARE(fcmp(d0, d1), "fcmp d0, d1");
  COMPARE(fcmp(d31, d30), "fcmp d31, d30");
  COMPARE(fcmp(s12, 0), "fcmp s12, #0.0");
  COMPARE(fcmp(d12, 0), "fcmp d12, #0.0");

  CLEANUP();
}


TEST_(fp_cond_compare) {
  SET_UP_ASM();

  COMPARE(fccmp(s0, s1, NoFlag, eq), "fccmp s0, s1, #nzcv, eq");
  COMPARE(fccmp(s2, s3, ZVFlag, ne), "fccmp s2, s3, #nZcV, ne");
  COMPARE(fccmp(s30, s16, NCFlag, pl), "fccmp s30, s16, #NzCv, pl");
  COMPARE(fccmp(s31, s31, NZCVFlag, le), "fccmp s31, s31, #NZCV, le");
  COMPARE(fccmp(d4, d5, VFlag, gt), "fccmp d4, d5, #nzcV, gt");
  COMPARE(fccmp(d6, d7, NFlag, vs), "fccmp d6, d7, #Nzcv, vs");
  COMPARE(fccmp(d30, d0, NZFlag, vc), "fccmp d30, d0, #NZcv, vc");
  COMPARE(fccmp(d31, d31, ZFlag, hs), "fccmp d31, d31, #nZcv, hs");
  COMPARE(fccmp(s14, s15, CVFlag, al), "fccmp s14, s15, #nzCV, al");
  COMPARE(fccmp(d16, d17, CFlag, nv), "fccmp d16, d17, #nzCv, nv");

  CLEANUP();
}


TEST_(fp_select) {
  SET_UP_ASM();

  COMPARE(fcsel(s0, s1, s2, eq), "fcsel s0, s1, s2, eq")
  COMPARE(fcsel(s31, s31, s30, ne), "fcsel s31, s31, s30, ne");
  COMPARE(fcsel(d0, d1, d2, mi), "fcsel d0, d1, d2, mi");
  COMPARE(fcsel(d31, d30, d31, pl), "fcsel d31, d30, d31, pl");
  COMPARE(fcsel(s14, s15, s16, al), "fcsel s14, s15, s16, al");
  COMPARE(fcsel(d17, d18, d19, nv), "fcsel d17, d18, d19, nv");

  CLEANUP();
}


TEST_(fcvt_scvtf_ucvtf) {
  SET_UP_ASM();

  COMPARE(fcvtas(w0, s1), "fcvtas w0, s1");
  COMPARE(fcvtas(x2, s3), "fcvtas x2, s3");
  COMPARE(fcvtas(w4, d5), "fcvtas w4, d5");
  COMPARE(fcvtas(x6, d7), "fcvtas x6, d7");
  COMPARE(fcvtau(w8, s9), "fcvtau w8, s9");
  COMPARE(fcvtau(x10, s11), "fcvtau x10, s11");
  COMPARE(fcvtau(w12, d13), "fcvtau w12, d13");
  COMPARE(fcvtau(x14, d15), "fcvtau x14, d15");
  COMPARE(fcvtns(w0, s1), "fcvtns w0, s1");
  COMPARE(fcvtns(x2, s3), "fcvtns x2, s3");
  COMPARE(fcvtns(w4, d5), "fcvtns w4, d5");
  COMPARE(fcvtns(x6, d7), "fcvtns x6, d7");
  COMPARE(fcvtnu(w8, s9), "fcvtnu w8, s9");
  COMPARE(fcvtnu(x10, s11), "fcvtnu x10, s11");
  COMPARE(fcvtnu(w12, d13), "fcvtnu w12, d13");
  COMPARE(fcvtnu(x14, d15), "fcvtnu x14, d15");
  COMPARE(fcvtzu(x16, d17), "fcvtzu x16, d17");
  COMPARE(fcvtzu(w18, d19), "fcvtzu w18, d19");
  COMPARE(fcvtzs(x20, d21), "fcvtzs x20, d21");
  COMPARE(fcvtzs(w22, d23), "fcvtzs w22, d23");
  COMPARE(fcvtzu(x16, s17), "fcvtzu x16, s17");
  COMPARE(fcvtzu(w18, s19), "fcvtzu w18, s19");
  COMPARE(fcvtzs(x20, s21), "fcvtzs x20, s21");
  COMPARE(fcvtzs(w22, s23), "fcvtzs w22, s23");
  COMPARE(scvtf(d24, w25), "scvtf d24, w25");
  COMPARE(scvtf(s24, w25), "scvtf s24, w25");
  COMPARE(scvtf(d26, x0), "scvtf d26, x0");
  COMPARE(scvtf(s26, x0), "scvtf s26, x0");
  COMPARE(ucvtf(d28, w29), "ucvtf d28, w29");
  COMPARE(ucvtf(s28, w29), "ucvtf s28, w29");
  COMPARE(ucvtf(d0, x1), "ucvtf d0, x1");
  COMPARE(ucvtf(s0, x1), "ucvtf s0, x1");
  COMPARE(ucvtf(d0, x1, 0), "ucvtf d0, x1");
  COMPARE(ucvtf(s0, x1, 0), "ucvtf s0, x1");
  COMPARE(scvtf(d1, x2, 1), "scvtf d1, x2, #1");
  COMPARE(scvtf(s1, x2, 1), "scvtf s1, x2, #1");
  COMPARE(scvtf(d3, x4, 15), "scvtf d3, x4, #15");
  COMPARE(scvtf(s3, x4, 15), "scvtf s3, x4, #15");
  COMPARE(scvtf(d5, x6, 32), "scvtf d5, x6, #32");
  COMPARE(scvtf(s5, x6, 32), "scvtf s5, x6, #32");
  COMPARE(ucvtf(d7, x8, 2), "ucvtf d7, x8, #2");
  COMPARE(ucvtf(s7, x8, 2), "ucvtf s7, x8, #2");
  COMPARE(ucvtf(d9, x10, 16), "ucvtf d9, x10, #16");
  COMPARE(ucvtf(s9, x10, 16), "ucvtf s9, x10, #16");
  COMPARE(ucvtf(d11, x12, 33), "ucvtf d11, x12, #33");
  COMPARE(ucvtf(s11, x12, 33), "ucvtf s11, x12, #33");
  COMPARE(fcvtms(w0, s1), "fcvtms w0, s1");
  COMPARE(fcvtms(x2, s3), "fcvtms x2, s3");
  COMPARE(fcvtms(w4, d5), "fcvtms w4, d5");
  COMPARE(fcvtms(x6, d7), "fcvtms x6, d7");
  COMPARE(fcvtmu(w8, s9), "fcvtmu w8, s9");
  COMPARE(fcvtmu(x10, s11), "fcvtmu x10, s11");
  COMPARE(fcvtmu(w12, d13), "fcvtmu w12, d13");
  COMPARE(fcvtmu(x14, d15), "fcvtmu x14, d15");

  CLEANUP();
}


TEST_(system_mrs) {
  SET_UP_ASM();

  COMPARE(mrs(x0, NZCV), "mrs x0, nzcv");
  COMPARE(mrs(lr, NZCV), "mrs lr, nzcv");
  COMPARE(mrs(x15, FPCR), "mrs x15, fpcr");

  CLEANUP();
}


TEST_(system_msr) {
  SET_UP_ASM();

  COMPARE(msr(NZCV, x0), "msr nzcv, x0");
  COMPARE(msr(NZCV, x30), "msr nzcv, lr");
  COMPARE(msr(FPCR, x15), "msr fpcr, x15");

  CLEANUP();
}


TEST_(system_nop) {
  SET_UP_ASM();

  COMPARE(nop(), "nop");

  CLEANUP();
}


TEST_(debug) {
  SET_UP_ASM();

  CHECK(kImmExceptionIsDebug == 0xdeb0);

  // All debug codes should produce the same instruction, and the debug code
  // can be any uint32_t.
  COMPARE(debug("message", 0, BREAK), "hlt #0xdeb0");
  COMPARE(debug("message", 1, BREAK), "hlt #0xdeb0");
  COMPARE(debug("message", 0xffff, BREAK), "hlt #0xdeb0");
  COMPARE(debug("message", 0x10000, BREAK), "hlt #0xdeb0");
  COMPARE(debug("message", 0x7fffffff, BREAK), "hlt #0xdeb0");
  COMPARE(debug("message", 0x80000000u, BREAK), "hlt #0xdeb0");
  COMPARE(debug("message", 0xffffffffu, BREAK), "hlt #0xdeb0");

  CLEANUP();
}


TEST_(hlt) {
  SET_UP_ASM();

  COMPARE(hlt(0), "hlt #0x0");
  COMPARE(hlt(1), "hlt #0x1");
  COMPARE(hlt(65535), "hlt #0xffff");

  CLEANUP();
}


TEST_(brk) {
  SET_UP_ASM();

  COMPARE(brk(0), "brk #0x0");
  COMPARE(brk(1), "brk #0x1");
  COMPARE(brk(65535), "brk #0xffff");

  CLEANUP();
}


TEST_(add_sub_negative) {
  SET_UP_MASM();

  COMPARE(Add(x10, x0, -42), "sub x10, x0, #0x2a (42)");
  COMPARE(Add(x11, x1, -687), "sub x11, x1, #0x2af (687)");
  COMPARE(Add(x12, x2, -0x88), "sub x12, x2, #0x88 (136)");

  COMPARE(Sub(x13, x0, -600), "add x13, x0, #0x258 (600)");
  COMPARE(Sub(x14, x1, -313), "add x14, x1, #0x139 (313)");
  COMPARE(Sub(x15, x2, -0x555), "add x15, x2, #0x555 (1365)");

  COMPARE(Add(w19, w3, -0x344), "sub w19, w3, #0x344 (836)");
  COMPARE(Add(w20, w4, -2000), "sub w20, w4, #0x7d0 (2000)");

  COMPARE(Sub(w21, w3, -0xbc), "add w21, w3, #0xbc (188)");
  COMPARE(Sub(w22, w4, -2000), "add w22, w4, #0x7d0 (2000)");

  COMPARE(Cmp(w0, -1), "cmn w0, #0x1 (1)");
  COMPARE(Cmp(x1, -1), "cmn x1, #0x1 (1)");
  COMPARE(Cmp(w2, -4095), "cmn w2, #0xfff (4095)");
  COMPARE(Cmp(x3, -4095), "cmn x3, #0xfff (4095)");

  COMPARE(Cmn(w0, -1), "cmp w0, #0x1 (1)");
  COMPARE(Cmn(x1, -1), "cmp x1, #0x1 (1)");
  COMPARE(Cmn(w2, -4095), "cmp w2, #0xfff (4095)");
  COMPARE(Cmn(x3, -4095), "cmp x3, #0xfff (4095)");

  CLEANUP();
}


TEST_(logical_immediate_move) {
  SET_UP_MASM();

  COMPARE(And(w0, w1, 0), "movz w0, #0x0");
  COMPARE(And(x0, x1, 0), "movz x0, #0x0");
  COMPARE(Orr(w2, w3, 0), "mov w2, w3");
  COMPARE(Orr(x2, x3, 0), "mov x2, x3");
  COMPARE(Eor(w4, w5, 0), "mov w4, w5");
  COMPARE(Eor(x4, x5, 0), "mov x4, x5");
  COMPARE(Bic(w6, w7, 0), "mov w6, w7");
  COMPARE(Bic(x6, x7, 0), "mov x6, x7");
  COMPARE(Orn(w8, w9, 0), "movn w8, #0x0");
  COMPARE(Orn(x8, x9, 0), "movn x8, #0x0");
  COMPARE(Eon(w10, w11, 0), "mvn w10, w11");
  COMPARE(Eon(x10, x11, 0), "mvn x10, x11");

  COMPARE(And(w12, w13, 0xffffffff), "mov w12, w13");
  COMPARE(And(x12, x13, 0xffffffff), "and x12, x13, #0xffffffff");
  COMPARE(And(x12, x13, 0xffffffffffffffff), "mov x12, x13");
  COMPARE(Orr(w14, w15, 0xffffffff), "movn w14, #0x0");
  COMPARE(Orr(x14, x15, 0xffffffff), "orr x14, x15, #0xffffffff");
  COMPARE(Orr(x14, x15, 0xffffffffffffffff), "movn x14, #0x0");
  COMPARE(Eor(w16, w17, 0xffffffff), "mvn w16, w17");
  COMPARE(Eor(x16, x17, 0xffffffff), "eor x16, x17, #0xffffffff");
  COMPARE(Eor(x16, x17, 0xffffffffffffffff), "mvn x16, x17");
  COMPARE(Bic(w18, w19, 0xffffffff), "movz w18, #0x0");
  COMPARE(Bic(x18, x19, 0xffffffff), "and x18, x19, #0xffffffff00000000");
  COMPARE(Bic(x18, x19, 0xffffffffffffffff), "movz x18, #0x0");
  COMPARE(Orn(w20, w21, 0xffffffff), "mov w20, w21");
  COMPARE(Orn(x20, x21, 0xffffffff), "orr x20, x21, #0xffffffff00000000");
  COMPARE(Orn(x20, x21, 0xffffffffffffffff), "mov x20, x21");
  COMPARE(Eon(w22, w23, 0xffffffff), "mov w22, w23");
  COMPARE(Eon(x22, x23, 0xffffffff), "eor x22, x23, #0xffffffff00000000");
  COMPARE(Eon(x22, x23, 0xffffffffffffffff), "mov x22, x23");

  CLEANUP();
}


TEST_(barriers) {
  SET_UP_MASM();

  // DMB
  COMPARE(Dmb(FullSystem, BarrierAll), "dmb sy");
  COMPARE(Dmb(FullSystem, BarrierReads), "dmb ld");
  COMPARE(Dmb(FullSystem, BarrierWrites), "dmb st");

  COMPARE(Dmb(InnerShareable, BarrierAll), "dmb ish");
  COMPARE(Dmb(InnerShareable, BarrierReads), "dmb ishld");
  COMPARE(Dmb(InnerShareable, BarrierWrites), "dmb ishst");

  COMPARE(Dmb(NonShareable, BarrierAll), "dmb nsh");
  COMPARE(Dmb(NonShareable, BarrierReads), "dmb nshld");
  COMPARE(Dmb(NonShareable, BarrierWrites), "dmb nshst");

  COMPARE(Dmb(OuterShareable, BarrierAll), "dmb osh");
  COMPARE(Dmb(OuterShareable, BarrierReads), "dmb oshld");
  COMPARE(Dmb(OuterShareable, BarrierWrites), "dmb oshst");

  COMPARE(Dmb(FullSystem, BarrierOther), "dmb sy (0b1100)");
  COMPARE(Dmb(InnerShareable, BarrierOther), "dmb sy (0b1000)");
  COMPARE(Dmb(NonShareable, BarrierOther), "dmb sy (0b0100)");
  COMPARE(Dmb(OuterShareable, BarrierOther), "dmb sy (0b0000)");

  // DSB
  COMPARE(Dsb(FullSystem, BarrierAll), "dsb sy");
  COMPARE(Dsb(FullSystem, BarrierReads), "dsb ld");
  COMPARE(Dsb(FullSystem, BarrierWrites), "dsb st");

  COMPARE(Dsb(InnerShareable, BarrierAll), "dsb ish");
  COMPARE(Dsb(InnerShareable, BarrierReads), "dsb ishld");
  COMPARE(Dsb(InnerShareable, BarrierWrites), "dsb ishst");

  COMPARE(Dsb(NonShareable, BarrierAll), "dsb nsh");
  COMPARE(Dsb(NonShareable, BarrierReads), "dsb nshld");
  COMPARE(Dsb(NonShareable, BarrierWrites), "dsb nshst");

  COMPARE(Dsb(OuterShareable, BarrierAll), "dsb osh");
  COMPARE(Dsb(OuterShareable, BarrierReads), "dsb oshld");
  COMPARE(Dsb(OuterShareable, BarrierWrites), "dsb oshst");

  COMPARE(Dsb(FullSystem, BarrierOther), "dsb sy (0b1100)");
  COMPARE(Dsb(InnerShareable, BarrierOther), "dsb sy (0b1000)");
  COMPARE(Dsb(NonShareable, BarrierOther), "dsb sy (0b0100)");
  COMPARE(Dsb(OuterShareable, BarrierOther), "dsb sy (0b0000)");

  // ISB
  COMPARE(Isb(), "isb");

  CLEANUP();
}
