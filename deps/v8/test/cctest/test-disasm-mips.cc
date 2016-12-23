// Copyright 2012 the V8 project authors. All rights reserved.
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
//

#include <stdlib.h>

#include "src/v8.h"

#include "src/debug/debug.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

bool prev_instr_compact_branch = false;

bool DisassembleAndCompare(byte* pc, const char* compare_string) {
  disasm::NameConverter converter;
  disasm::Disassembler disasm(converter);
  EmbeddedVector<char, 128> disasm_buffer;

  if (prev_instr_compact_branch) {
    disasm.InstructionDecode(disasm_buffer, pc);
    pc += 4;
  }

  disasm.InstructionDecode(disasm_buffer, pc);

  if (strcmp(compare_string, disasm_buffer.start()) != 0) {
    fprintf(stderr,
            "expected: \n"
            "%s\n"
            "disassembled: \n"
            "%s\n\n",
            compare_string, disasm_buffer.start());
    return false;
  }
  return true;
}


// Set up V8 to a state where we can at least run the assembler and
// disassembler. Declare the variables and allocate the data structures used
// in the rest of the macros.
#define SET_UP()                                          \
  CcTest::InitializeVM();                                 \
  Isolate* isolate = CcTest::i_isolate();                  \
  HandleScope scope(isolate);                             \
  byte *buffer = reinterpret_cast<byte*>(malloc(4*1024)); \
  Assembler assm(isolate, buffer, 4*1024);                \
  bool failure = false;


// This macro assembles one instruction using the preallocated assembler and
// disassembles the generated instruction, comparing the output to the expected
// value. If the comparison fails an error message is printed, but the test
// continues to run until the end.
#define COMPARE(asm_, compare_string) \
  { \
    int pc_offset = assm.pc_offset(); \
    byte *progcounter = &buffer[pc_offset]; \
    assm.asm_; \
    if (!DisassembleAndCompare(progcounter, compare_string)) failure = true; \
  }


// Verify that all invocations of the COMPARE macro passed successfully.
// Exit with a failure if at least one of the tests failed.
#define VERIFY_RUN() \
if (failure) { \
    V8_Fatal(__FILE__, __LINE__, "MIPS Disassembler tests failed.\n"); \
  }

#define COMPARE_PC_REL_COMPACT(asm_, compare_string, offset)                   \
  {                                                                            \
    int pc_offset = assm.pc_offset();                                          \
    byte *progcounter = &buffer[pc_offset];                                    \
    char str_with_address[100];                                                \
    prev_instr_compact_branch = assm.IsPrevInstrCompactBranch();               \
    if (prev_instr_compact_branch) {                                           \
      snprintf(str_with_address, sizeof(str_with_address), "%s -> %p",         \
               compare_string,                                                 \
               static_cast<void *>(progcounter + 8 + (offset * 4)));           \
    } else {                                                                   \
      snprintf(str_with_address, sizeof(str_with_address), "%s -> %p",         \
               compare_string,                                                 \
               static_cast<void *>(progcounter + 4 + (offset * 4)));           \
    }                                                                          \
    assm.asm_;                                                                 \
    if (!DisassembleAndCompare(progcounter, str_with_address)) failure = true; \
  }

#define COMPARE_PC_REL(asm_, compare_string, offset)                           \
  {                                                                            \
    int pc_offset = assm.pc_offset();                                          \
    byte *progcounter = &buffer[pc_offset];                                    \
    char str_with_address[100];                                                \
    snprintf(str_with_address, sizeof(str_with_address), "%s -> %p",           \
             compare_string, static_cast<void *>(progcounter + (offset * 4))); \
    assm.asm_;                                                                 \
    if (!DisassembleAndCompare(progcounter, str_with_address)) failure = true; \
  }

#define COMPARE_PC_JUMP(asm_, compare_string, target)                          \
  {                                                                            \
    int pc_offset = assm.pc_offset();                                          \
    byte *progcounter = &buffer[pc_offset];                                    \
    char str_with_address[100];                                                \
    int instr_index = (target >> 2) & kImm26Mask;                              \
    snprintf(                                                                  \
        str_with_address, sizeof(str_with_address), "%s %p -> %p",             \
        compare_string, reinterpret_cast<void *>(target),                      \
        reinterpret_cast<void *>(((uint32_t)(progcounter + 4) & ~0xfffffff) |  \
                                 (instr_index << 2)));                         \
    assm.asm_;                                                                 \
    if (!DisassembleAndCompare(progcounter, str_with_address)) failure = true; \
  }

#define GET_PC_REGION(pc_region)                                         \
  {                                                                      \
    int pc_offset = assm.pc_offset();                                    \
    byte *progcounter = &buffer[pc_offset];                              \
    pc_region = reinterpret_cast<int32_t>(progcounter + 4) & ~0xfffffff; \
  }


TEST(Type0) {
  SET_UP();

  COMPARE(addu(a0, a1, a2),
          "00a62021       addu    a0, a1, a2");
  COMPARE(addu(t2, t3, t4),
          "016c5021       addu    t2, t3, t4");
  COMPARE(addu(v0, v1, s0),
          "00701021       addu    v0, v1, s0");

  COMPARE(subu(a0, a1, a2),
          "00a62023       subu    a0, a1, a2");
  COMPARE(subu(t2, t3, t4),
          "016c5023       subu    t2, t3, t4");
  COMPARE(subu(v0, v1, s0),
          "00701023       subu    v0, v1, s0");

  if (!IsMipsArchVariant(kMips32r6)) {
    COMPARE(mult(a0, a1),
            "00850018       mult    a0, a1");
    COMPARE(mult(t2, t3),
            "014b0018       mult    t2, t3");
    COMPARE(mult(v0, v1),
            "00430018       mult    v0, v1");

    COMPARE(multu(a0, a1),
            "00850019       multu   a0, a1");
    COMPARE(multu(t2, t3),
            "014b0019       multu   t2, t3");
    COMPARE(multu(v0, v1),
            "00430019       multu   v0, v1");

    COMPARE(div(a0, a1),
            "0085001a       div     a0, a1");
    COMPARE(div(t2, t3),
            "014b001a       div     t2, t3");
    COMPARE(div(v0, v1),
            "0043001a       div     v0, v1");

    COMPARE(divu(a0, a1),
            "0085001b       divu    a0, a1");
    COMPARE(divu(t2, t3),
            "014b001b       divu    t2, t3");
    COMPARE(divu(v0, v1),
            "0043001b       divu    v0, v1");

    if (!IsMipsArchVariant(kLoongson)) {
      COMPARE(mul(a0, a1, a2),
              "70a62002       mul     a0, a1, a2");
      COMPARE(mul(t2, t3, t4),
              "716c5002       mul     t2, t3, t4");
      COMPARE(mul(v0, v1, s0),
              "70701002       mul     v0, v1, s0");
    }
  } else {  // MIPS32r6.
    COMPARE(mul(a0, a1, a2),
            "00a62098       mul    a0, a1, a2");
    COMPARE(muh(a0, a1, a2),
            "00a620d8       muh    a0, a1, a2");
    COMPARE(mul(t1, t2, t3),
            "014b4898       mul    t1, t2, t3");
    COMPARE(muh(t1, t2, t3),
            "014b48d8       muh    t1, t2, t3");
    COMPARE(mul(v0, v1, a0),
            "00641098       mul    v0, v1, a0");
    COMPARE(muh(v0, v1, a0),
            "006410d8       muh    v0, v1, a0");

    COMPARE(mulu(a0, a1, a2),
            "00a62099       mulu   a0, a1, a2");
    COMPARE(muhu(a0, a1, a2),
            "00a620d9       muhu   a0, a1, a2");
    COMPARE(mulu(t1, t2, t3),
            "014b4899       mulu   t1, t2, t3");
    COMPARE(muhu(t1, t2, t3),
            "014b48d9       muhu   t1, t2, t3");
    COMPARE(mulu(v0, v1, a0),
            "00641099       mulu   v0, v1, a0");
    COMPARE(muhu(v0, v1, a0),
            "006410d9       muhu   v0, v1, a0");

    COMPARE(div(a0, a1, a2),
            "00a6209a       div    a0, a1, a2");
    COMPARE(mod(a0, a1, a2),
            "00a620da       mod    a0, a1, a2");
    COMPARE(div(t1, t2, t3),
            "014b489a       div    t1, t2, t3");
    COMPARE(mod(t1, t2, t3),
            "014b48da       mod    t1, t2, t3");
    COMPARE(div(v0, v1, a0),
            "0064109a       div    v0, v1, a0");
    COMPARE(mod(v0, v1, a0),
            "006410da       mod    v0, v1, a0");

    COMPARE(divu(a0, a1, a2),
            "00a6209b       divu   a0, a1, a2");
    COMPARE(modu(a0, a1, a2),
            "00a620db       modu   a0, a1, a2");
    COMPARE(divu(t1, t2, t3),
            "014b489b       divu   t1, t2, t3");
    COMPARE(modu(t1, t2, t3),
            "014b48db       modu   t1, t2, t3");
    COMPARE(divu(v0, v1, a0),
            "0064109b       divu   v0, v1, a0");
    COMPARE(modu(v0, v1, a0),
            "006410db       modu   v0, v1, a0");

    COMPARE_PC_REL_COMPACT(bovc(a0, a0, static_cast<int16_t>(0)),
                           "20840000       bovc  a0, a0, 0", 0);
    COMPARE_PC_REL_COMPACT(bovc(a1, a0, static_cast<int16_t>(0)),
                           "20a40000       bovc  a1, a0, 0", 0);
    COMPARE_PC_REL_COMPACT(bovc(a1, a0, 32767),
                           "20a47fff       bovc  a1, a0, 32767", 32767);
    COMPARE_PC_REL_COMPACT(bovc(a1, a0, -32768),
                           "20a48000       bovc  a1, a0, -32768", -32768);

    COMPARE_PC_REL_COMPACT(bnvc(a0, a0, static_cast<int16_t>(0)),
                           "60840000       bnvc  a0, a0, 0", 0);
    COMPARE_PC_REL_COMPACT(bnvc(a1, a0, static_cast<int16_t>(0)),
                           "60a40000       bnvc  a1, a0, 0", 0);
    COMPARE_PC_REL_COMPACT(bnvc(a1, a0, 32767),
                           "60a47fff       bnvc  a1, a0, 32767", 32767);
    COMPARE_PC_REL_COMPACT(bnvc(a1, a0, -32768),
                           "60a48000       bnvc  a1, a0, -32768", -32768);

    COMPARE_PC_REL_COMPACT(beqzc(a0, -1048576),
                           "d8900000       beqzc   a0, -1048576", -1048576);
    COMPARE_PC_REL_COMPACT(beqzc(a0, -1), "d89fffff       beqzc   a0, -1", -1);
    COMPARE_PC_REL_COMPACT(beqzc(a0, 0), "d8800000       beqzc   a0, 0", 0);
    COMPARE_PC_REL_COMPACT(beqzc(a0, 1), "d8800001       beqzc   a0, 1", 1);
    COMPARE_PC_REL_COMPACT(beqzc(a0, 1048575),
                           "d88fffff       beqzc   a0, 1048575", 1048575);

    COMPARE_PC_REL_COMPACT(bnezc(a0, 0), "f8800000       bnezc   a0, 0", 0);
    COMPARE_PC_REL_COMPACT(bnezc(a0, 1048575),  // int21 maximal value.
                           "f88fffff       bnezc   a0, 1048575", 1048575);
    COMPARE_PC_REL_COMPACT(bnezc(a0, -1048576),  // int21 minimal value.
                           "f8900000       bnezc   a0, -1048576", -1048576);

    COMPARE_PC_REL_COMPACT(bc(-33554432), "ca000000       bc      -33554432",
                           -33554432);
    COMPARE_PC_REL_COMPACT(bc(-1), "cbffffff       bc      -1", -1);
    COMPARE_PC_REL_COMPACT(bc(0), "c8000000       bc      0", 0);
    COMPARE_PC_REL_COMPACT(bc(1), "c8000001       bc      1", 1);
    COMPARE_PC_REL_COMPACT(bc(33554431), "c9ffffff       bc      33554431",
                           33554431);

    COMPARE_PC_REL_COMPACT(balc(-33554432), "ea000000       balc    -33554432",
                           -33554432);
    COMPARE_PC_REL_COMPACT(balc(-1), "ebffffff       balc    -1", -1);
    COMPARE_PC_REL_COMPACT(balc(0), "e8000000       balc    0", 0);
    COMPARE_PC_REL_COMPACT(balc(1), "e8000001       balc    1", 1);
    COMPARE_PC_REL_COMPACT(balc(33554431), "e9ffffff       balc    33554431",
                           33554431);

    COMPARE_PC_REL_COMPACT(bgeuc(a0, a1, -32768),
                           "18858000       bgeuc   a0, a1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bgeuc(a0, a1, -1),
                           "1885ffff       bgeuc   a0, a1, -1", -1);
    COMPARE_PC_REL_COMPACT(bgeuc(a0, a1, 1), "18850001       bgeuc   a0, a1, 1",
                           1);
    COMPARE_PC_REL_COMPACT(bgeuc(a0, a1, 32767),
                           "18857fff       bgeuc   a0, a1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bgezalc(a0, -32768),
                           "18848000       bgezalc a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bgezalc(a0, -1), "1884ffff       bgezalc a0, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bgezalc(a0, 1), "18840001       bgezalc a0, 1", 1);
    COMPARE_PC_REL_COMPACT(bgezalc(a0, 32767),
                           "18847fff       bgezalc a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(blezalc(a0, -32768),
                           "18048000       blezalc a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(blezalc(a0, -1), "1804ffff       blezalc a0, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(blezalc(a0, 1), "18040001       blezalc a0, 1", 1);
    COMPARE_PC_REL_COMPACT(blezalc(a0, 32767),
                           "18047fff       blezalc a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bltuc(a0, a1, -32768),
                           "1c858000       bltuc   a0, a1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bltuc(a0, a1, -1),
                           "1c85ffff       bltuc   a0, a1, -1", -1);
    COMPARE_PC_REL_COMPACT(bltuc(a0, a1, 1), "1c850001       bltuc   a0, a1, 1",
                           1);
    COMPARE_PC_REL_COMPACT(bltuc(a0, a1, 32767),
                           "1c857fff       bltuc   a0, a1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bltzalc(a0, -32768),
                           "1c848000       bltzalc a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bltzalc(a0, -1), "1c84ffff       bltzalc a0, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bltzalc(a0, 1), "1c840001       bltzalc a0, 1", 1);
    COMPARE_PC_REL_COMPACT(bltzalc(a0, 32767),
                           "1c847fff       bltzalc a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bgtzalc(a0, -32768),
                           "1c048000       bgtzalc a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bgtzalc(a0, -1), "1c04ffff       bgtzalc a0, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bgtzalc(a0, 1), "1c040001       bgtzalc a0, 1", 1);
    COMPARE_PC_REL_COMPACT(bgtzalc(a0, 32767),
                           "1c047fff       bgtzalc a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bgezc(a0, -32768),
                           "58848000       bgezc    a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bgezc(a0, -1), "5884ffff       bgezc    a0, -1", -1);
    COMPARE_PC_REL_COMPACT(bgezc(a0, 1), "58840001       bgezc    a0, 1", 1);
    COMPARE_PC_REL_COMPACT(bgezc(a0, 32767),
                           "58847fff       bgezc    a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bgec(a0, a1, -32768),
                           "58858000       bgec     a0, a1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bgec(a0, a1, -1),
                           "5885ffff       bgec     a0, a1, -1", -1);
    COMPARE_PC_REL_COMPACT(bgec(a0, a1, 1), "58850001       bgec     a0, a1, 1",
                           1);
    COMPARE_PC_REL_COMPACT(bgec(a0, a1, 32767),
                           "58857fff       bgec     a0, a1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(blezc(a0, -32768),
                           "58048000       blezc    a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(blezc(a0, -1), "5804ffff       blezc    a0, -1", -1);
    COMPARE_PC_REL_COMPACT(blezc(a0, 1), "58040001       blezc    a0, 1", 1);
    COMPARE_PC_REL_COMPACT(blezc(a0, 32767),
                           "58047fff       blezc    a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bltzc(a0, -32768),
                           "5c848000       bltzc    a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bltzc(a0, -1), "5c84ffff       bltzc    a0, -1", -1);
    COMPARE_PC_REL_COMPACT(bltzc(a0, 1), "5c840001       bltzc    a0, 1", 1);
    COMPARE_PC_REL_COMPACT(bltzc(a0, 32767),
                           "5c847fff       bltzc    a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bltc(a0, a1, -32768),
                           "5c858000       bltc    a0, a1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bltc(a0, a1, -1),
                           "5c85ffff       bltc    a0, a1, -1", -1);
    COMPARE_PC_REL_COMPACT(bltc(a0, a1, 1), "5c850001       bltc    a0, a1, 1",
                           1);
    COMPARE_PC_REL_COMPACT(bltc(a0, a1, 32767),
                           "5c857fff       bltc    a0, a1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bgtzc(a0, -32768),
                           "5c048000       bgtzc    a0, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bgtzc(a0, -1), "5c04ffff       bgtzc    a0, -1", -1);
    COMPARE_PC_REL_COMPACT(bgtzc(a0, 1), "5c040001       bgtzc    a0, 1", 1);
    COMPARE_PC_REL_COMPACT(bgtzc(a0, 32767),
                           "5c047fff       bgtzc    a0, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bc1eqz(-32768, f1),
                           "45218000       bc1eqz    f1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bc1eqz(-1, f1), "4521ffff       bc1eqz    f1, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bc1eqz(1, f1), "45210001       bc1eqz    f1, 1", 1);
    COMPARE_PC_REL_COMPACT(bc1eqz(32767, f1),
                           "45217fff       bc1eqz    f1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bc1nez(-32768, f1),
                           "45a18000       bc1nez    f1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bc1nez(-1, f1), "45a1ffff       bc1nez    f1, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bc1nez(1, f1), "45a10001       bc1nez    f1, 1", 1);
    COMPARE_PC_REL_COMPACT(bc1nez(32767, f1),
                           "45a17fff       bc1nez    f1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bovc(a1, a0, -1), "20a4ffff       bovc  a1, a0, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bovc(a0, a0, 1), "20840001       bovc  a0, a0, 1",
                           1);

    COMPARE_PC_REL_COMPACT(beqc(a0, a1, -32768),
                           "20858000       beqc    a0, a1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(beqc(a0, a1, -1),
                           "2085ffff       beqc    a0, a1, -1", -1);
    COMPARE_PC_REL_COMPACT(beqc(a0, a1, 1), "20850001       beqc    a0, a1, 1",
                           1);
    COMPARE_PC_REL_COMPACT(beqc(a0, a1, 32767),
                           "20857fff       beqc    a0, a1, 32767", 32767);

    COMPARE_PC_REL_COMPACT(bnec(a0, a1, -32768),
                           "60858000       bnec  a0, a1, -32768", -32768);
    COMPARE_PC_REL_COMPACT(bnec(a0, a1, -1), "6085ffff       bnec  a0, a1, -1",
                           -1);
    COMPARE_PC_REL_COMPACT(bnec(a0, a1, 1), "60850001       bnec  a0, a1, 1",
                           1);
    COMPARE_PC_REL_COMPACT(bnec(a0, a1, 32767),
                           "60857fff       bnec  a0, a1, 32767", 32767);
  }

  COMPARE_PC_REL_COMPACT(bne(a0, a1, -32768),
                         "14858000       bne     a0, a1, -32768", -32768);
  COMPARE_PC_REL_COMPACT(bne(a0, a1, -1), "1485ffff       bne     a0, a1, -1",
                         -1);
  COMPARE_PC_REL_COMPACT(bne(a0, a1, 1), "14850001       bne     a0, a1, 1", 1);
  COMPARE_PC_REL_COMPACT(bne(a0, a1, 32767),
                         "14857fff       bne     a0, a1, 32767", 32767);

  COMPARE_PC_REL_COMPACT(beq(a0, a1, -32768),
                         "10858000       beq     a0, a1, -32768", -32768);
  COMPARE_PC_REL_COMPACT(beq(a0, a1, -1), "1085ffff       beq     a0, a1, -1",
                         -1);
  COMPARE_PC_REL_COMPACT(beq(a0, a1, 1), "10850001       beq     a0, a1, 1", 1);
  COMPARE_PC_REL_COMPACT(beq(a0, a1, 32767),
                         "10857fff       beq     a0, a1, 32767", 32767);

  COMPARE_PC_REL_COMPACT(bltz(a0, -32768), "04808000       bltz    a0, -32768",
                         -32768);
  COMPARE_PC_REL_COMPACT(bltz(a0, -1), "0480ffff       bltz    a0, -1", -1);
  COMPARE_PC_REL_COMPACT(bltz(a0, 1), "04800001       bltz    a0, 1", 1);
  COMPARE_PC_REL_COMPACT(bltz(a0, 32767), "04807fff       bltz    a0, 32767",
                         32767);

  COMPARE_PC_REL_COMPACT(bgez(a0, -32768), "04818000       bgez    a0, -32768",
                         -32768);
  COMPARE_PC_REL_COMPACT(bgez(a0, -1), "0481ffff       bgez    a0, -1", -1);
  COMPARE_PC_REL_COMPACT(bgez(a0, 1), "04810001       bgez    a0, 1", 1);
  COMPARE_PC_REL_COMPACT(bgez(a0, 32767), "04817fff       bgez    a0, 32767",
                         32767);

  COMPARE_PC_REL_COMPACT(blez(a0, -32768), "18808000       blez    a0, -32768",
                         -32768);
  COMPARE_PC_REL_COMPACT(blez(a0, -1), "1880ffff       blez    a0, -1", -1);
  COMPARE_PC_REL_COMPACT(blez(a0, 1), "18800001       blez    a0, 1", 1);
  COMPARE_PC_REL_COMPACT(blez(a0, 32767), "18807fff       blez    a0, 32767",
                         32767);

  COMPARE_PC_REL_COMPACT(bgtz(a0, -32768), "1c808000       bgtz    a0, -32768",
                         -32768);
  COMPARE_PC_REL_COMPACT(bgtz(a0, -1), "1c80ffff       bgtz    a0, -1", -1);
  COMPARE_PC_REL_COMPACT(bgtz(a0, 1), "1c800001       bgtz    a0, 1", 1);
  COMPARE_PC_REL_COMPACT(bgtz(a0, 32767), "1c807fff       bgtz    a0, 32767",
                         32767);

  int32_t pc_region;
  GET_PC_REGION(pc_region);

  int32_t target = pc_region | 0x4;
  COMPARE_PC_JUMP(j(target), "08000001       j      ", target);
  target = pc_region | 0xffffffc;
  COMPARE_PC_JUMP(j(target), "0bffffff       j      ", target);

  target = pc_region | 0x4;
  COMPARE_PC_JUMP(jal(target), "0c000001       jal    ", target);
  target = pc_region | 0xffffffc;
  COMPARE_PC_JUMP(jal(target), "0fffffff       jal    ", target);

  COMPARE(addiu(a0, a1, 0x0),
          "24a40000       addiu   a0, a1, 0");
  COMPARE(addiu(s0, s1, 32767),
          "26307fff       addiu   s0, s1, 32767");
  COMPARE(addiu(t2, t3, -32768),
          "256a8000       addiu   t2, t3, -32768");
  COMPARE(addiu(v0, v1, -1),
          "2462ffff       addiu   v0, v1, -1");

  COMPARE(and_(a0, a1, a2),
          "00a62024       and     a0, a1, a2");
  COMPARE(and_(s0, s1, s2),
          "02328024       and     s0, s1, s2");
  COMPARE(and_(t2, t3, t4),
          "016c5024       and     t2, t3, t4");
  COMPARE(and_(v0, v1, a2),
          "00661024       and     v0, v1, a2");

  COMPARE(or_(a0, a1, a2),
          "00a62025       or      a0, a1, a2");
  COMPARE(or_(s0, s1, s2),
          "02328025       or      s0, s1, s2");
  COMPARE(or_(t2, t3, t4),
          "016c5025       or      t2, t3, t4");
  COMPARE(or_(v0, v1, a2),
          "00661025       or      v0, v1, a2");

  COMPARE(xor_(a0, a1, a2),
          "00a62026       xor     a0, a1, a2");
  COMPARE(xor_(s0, s1, s2),
          "02328026       xor     s0, s1, s2");
  COMPARE(xor_(t2, t3, t4),
          "016c5026       xor     t2, t3, t4");
  COMPARE(xor_(v0, v1, a2),
          "00661026       xor     v0, v1, a2");

  COMPARE(nor(a0, a1, a2),
          "00a62027       nor     a0, a1, a2");
  COMPARE(nor(s0, s1, s2),
          "02328027       nor     s0, s1, s2");
  COMPARE(nor(t2, t3, t4),
          "016c5027       nor     t2, t3, t4");
  COMPARE(nor(v0, v1, a2),
          "00661027       nor     v0, v1, a2");

  COMPARE(andi(a0, a1, 0x1),
          "30a40001       andi    a0, a1, 0x1");
  COMPARE(andi(v0, v1, 0xffff),
          "3062ffff       andi    v0, v1, 0xffff");

  COMPARE(ori(a0, a1, 0x1),
          "34a40001       ori     a0, a1, 0x1");
  COMPARE(ori(v0, v1, 0xffff),
          "3462ffff       ori     v0, v1, 0xffff");

  COMPARE(xori(a0, a1, 0x1),
          "38a40001       xori    a0, a1, 0x1");
  COMPARE(xori(v0, v1, 0xffff),
          "3862ffff       xori    v0, v1, 0xffff");

  COMPARE(lui(a0, 0x1),
          "3c040001       lui     a0, 0x1");
  COMPARE(lui(v0, 0xffff),
          "3c02ffff       lui     v0, 0xffff");

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(aui(a0, a1, 0x1), "3ca40001       aui     a0, a1, 0x1");
    COMPARE(aui(v0, v1, 0xffff), "3c62ffff       aui     v0, v1, 0xffff");
  }

  COMPARE(sll(a0, a1, 0),
          "00052000       sll     a0, a1, 0");
  COMPARE(sll(s0, s1, 8),
          "00118200       sll     s0, s1, 8");
  COMPARE(sll(t2, t3, 24),
          "000b5600       sll     t2, t3, 24");
  COMPARE(sll(v0, v1, 31),
          "000317c0       sll     v0, v1, 31");

  COMPARE(sllv(a0, a1, a2),
          "00c52004       sllv    a0, a1, a2");
  COMPARE(sllv(s0, s1, s2),
          "02518004       sllv    s0, s1, s2");
  COMPARE(sllv(t2, t3, t4),
          "018b5004       sllv    t2, t3, t4");
  COMPARE(sllv(v0, v1, fp),
          "03c31004       sllv    v0, v1, fp");

  COMPARE(srl(a0, a1, 0),
          "00052002       srl     a0, a1, 0");
  COMPARE(srl(s0, s1, 8),
          "00118202       srl     s0, s1, 8");
  COMPARE(srl(t2, t3, 24),
          "000b5602       srl     t2, t3, 24");
  COMPARE(srl(v0, v1, 31),
          "000317c2       srl     v0, v1, 31");

  COMPARE(srlv(a0, a1, a2),
          "00c52006       srlv    a0, a1, a2");
  COMPARE(srlv(s0, s1, s2),
          "02518006       srlv    s0, s1, s2");
  COMPARE(srlv(t2, t3, t4),
          "018b5006       srlv    t2, t3, t4");
  COMPARE(srlv(v0, v1, fp),
          "03c31006       srlv    v0, v1, fp");

  COMPARE(sra(a0, a1, 0),
          "00052003       sra     a0, a1, 0");
  COMPARE(sra(s0, s1, 8),
          "00118203       sra     s0, s1, 8");
  COMPARE(sra(t2, t3, 24),
          "000b5603       sra     t2, t3, 24");
  COMPARE(sra(v0, v1, 31),
          "000317c3       sra     v0, v1, 31");

  COMPARE(srav(a0, a1, a2),
          "00c52007       srav    a0, a1, a2");
  COMPARE(srav(s0, s1, s2),
          "02518007       srav    s0, s1, s2");
  COMPARE(srav(t2, t3, t4),
          "018b5007       srav    t2, t3, t4");
  COMPARE(srav(v0, v1, fp),
          "03c31007       srav    v0, v1, fp");

  if (IsMipsArchVariant(kMips32r2)) {
    COMPARE(rotr(a0, a1, 0),
            "00252002       rotr    a0, a1, 0");
    COMPARE(rotr(s0, s1, 8),
            "00318202       rotr    s0, s1, 8");
    COMPARE(rotr(t2, t3, 24),
            "002b5602       rotr    t2, t3, 24");
    COMPARE(rotr(v0, v1, 31),
            "002317c2       rotr    v0, v1, 31");

    COMPARE(rotrv(a0, a1, a2),
            "00c52046       rotrv   a0, a1, a2");
    COMPARE(rotrv(s0, s1, s2),
            "02518046       rotrv   s0, s1, s2");
    COMPARE(rotrv(t2, t3, t4),
            "018b5046       rotrv   t2, t3, t4");
    COMPARE(rotrv(v0, v1, fp),
            "03c31046       rotrv   v0, v1, fp");
  }

  COMPARE(break_(0),
          "0000000d       break, code: 0x00000 (0)");
  COMPARE(break_(261120),
          "00ff000d       break, code: 0x3fc00 (261120)");
  COMPARE(break_(1047552),
          "03ff000d       break, code: 0xffc00 (1047552)");

  COMPARE(tge(a0, a1, 0),
          "00850030       tge     a0, a1, code: 0x000");
  COMPARE(tge(s0, s1, 1023),
          "0211fff0       tge     s0, s1, code: 0x3ff");
  COMPARE(tgeu(a0, a1, 0),
          "00850031       tgeu    a0, a1, code: 0x000");
  COMPARE(tgeu(s0, s1, 1023),
          "0211fff1       tgeu    s0, s1, code: 0x3ff");
  COMPARE(tlt(a0, a1, 0),
          "00850032       tlt     a0, a1, code: 0x000");
  COMPARE(tlt(s0, s1, 1023),
          "0211fff2       tlt     s0, s1, code: 0x3ff");
  COMPARE(tltu(a0, a1, 0),
          "00850033       tltu    a0, a1, code: 0x000");
  COMPARE(tltu(s0, s1, 1023),
          "0211fff3       tltu    s0, s1, code: 0x3ff");
  COMPARE(teq(a0, a1, 0),
          "00850034       teq     a0, a1, code: 0x000");
  COMPARE(teq(s0, s1, 1023),
          "0211fff4       teq     s0, s1, code: 0x3ff");
  COMPARE(tne(a0, a1, 0),
          "00850036       tne     a0, a1, code: 0x000");
  COMPARE(tne(s0, s1, 1023),
          "0211fff6       tne     s0, s1, code: 0x3ff");

  COMPARE(mfhi(a0),
          "00002010       mfhi    a0");
  COMPARE(mfhi(s2),
          "00009010       mfhi    s2");
  COMPARE(mfhi(t4),
          "00006010       mfhi    t4");
  COMPARE(mfhi(v1),
          "00001810       mfhi    v1");
  COMPARE(mflo(a0),
          "00002012       mflo    a0");
  COMPARE(mflo(s2),
          "00009012       mflo    s2");
  COMPARE(mflo(t4),
          "00006012       mflo    t4");
  COMPARE(mflo(v1),
          "00001812       mflo    v1");

  COMPARE(slt(a0, a1, a2),
          "00a6202a       slt     a0, a1, a2");
  COMPARE(slt(s0, s1, s2),
          "0232802a       slt     s0, s1, s2");
  COMPARE(slt(t2, t3, t4),
          "016c502a       slt     t2, t3, t4");
  COMPARE(slt(v0, v1, a2),
          "0066102a       slt     v0, v1, a2");
  COMPARE(sltu(a0, a1, a2),
          "00a6202b       sltu    a0, a1, a2");
  COMPARE(sltu(s0, s1, s2),
          "0232802b       sltu    s0, s1, s2");
  COMPARE(sltu(t2, t3, t4),
          "016c502b       sltu    t2, t3, t4");
  COMPARE(sltu(v0, v1, a2),
          "0066102b       sltu    v0, v1, a2");

  COMPARE(slti(a0, a1, 0),
          "28a40000       slti    a0, a1, 0");
  COMPARE(slti(s0, s1, 32767),
          "2a307fff       slti    s0, s1, 32767");
  COMPARE(slti(t2, t3, -32768),
          "296a8000       slti    t2, t3, -32768");
  COMPARE(slti(v0, v1, -1),
          "2862ffff       slti    v0, v1, -1");
  COMPARE(sltiu(a0, a1, 0),
          "2ca40000       sltiu   a0, a1, 0");
  COMPARE(sltiu(s0, s1, 32767),
          "2e307fff       sltiu   s0, s1, 32767");
  COMPARE(sltiu(t2, t3, -32768),
          "2d6a8000       sltiu   t2, t3, -32768");
  COMPARE(sltiu(v0, v1, -1),
          "2c62ffff       sltiu   v0, v1, -1");

  if (!IsMipsArchVariant(kLoongson)) {
    COMPARE(movz(a0, a1, a2),
            "00a6200a       movz    a0, a1, a2");
    COMPARE(movz(s0, s1, s2),
            "0232800a       movz    s0, s1, s2");
    COMPARE(movz(t2, t3, t4),
            "016c500a       movz    t2, t3, t4");
    COMPARE(movz(v0, v1, a2),
            "0066100a       movz    v0, v1, a2");
    COMPARE(movn(a0, a1, a2),
            "00a6200b       movn    a0, a1, a2");
    COMPARE(movn(s0, s1, s2),
            "0232800b       movn    s0, s1, s2");
    COMPARE(movn(t2, t3, t4),
            "016c500b       movn    t2, t3, t4");
    COMPARE(movn(v0, v1, a2),
            "0066100b       movn    v0, v1, a2");

    COMPARE(movt(a0, a1, 1),
            "00a52001       movt    a0, a1, 1");
    COMPARE(movt(s0, s1, 2),
            "02298001       movt    s0, s1, 2");
    COMPARE(movt(t2, t3, 3),
            "016d5001       movt    t2, t3, 3");
    COMPARE(movt(v0, v1, 7),
            "007d1001       movt    v0, v1, 7");
    COMPARE(movf(a0, a1, 0),
            "00a02001       movf    a0, a1, 0");
    COMPARE(movf(s0, s1, 4),
            "02308001       movf    s0, s1, 4");
    COMPARE(movf(t2, t3, 5),
            "01745001       movf    t2, t3, 5");
    COMPARE(movf(v0, v1, 6),
            "00781001       movf    v0, v1, 6");

    if (IsMipsArchVariant(kMips32r6)) {
      COMPARE(clz(a0, a1),
              "00a02050       clz     a0, a1");
      COMPARE(clz(s6, s7),
              "02e0b050       clz     s6, s7");
      COMPARE(clz(v0, v1),
              "00601050       clz     v0, v1");
    } else {
      COMPARE(clz(a0, a1),
              "70a42020       clz     a0, a1");
      COMPARE(clz(s6, s7),
              "72f6b020       clz     s6, s7");
      COMPARE(clz(v0, v1),
              "70621020       clz     v0, v1");
    }
  }

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    COMPARE(seb(a0, a1), "7c052420       seb     a0, a1");
    COMPARE(seb(s6, s7), "7c17b420       seb     s6, s7");
    COMPARE(seb(v0, v1), "7c031420       seb     v0, v1");

    COMPARE(seh(a0, a1), "7c052620       seh     a0, a1");
    COMPARE(seh(s6, s7), "7c17b620       seh     s6, s7");
    COMPARE(seh(v0, v1), "7c031620       seh     v0, v1");

    COMPARE(wsbh(a0, a1), "7c0520a0       wsbh    a0, a1");
    COMPARE(wsbh(s6, s7), "7c17b0a0       wsbh    s6, s7");
    COMPARE(wsbh(v0, v1), "7c0310a0       wsbh    v0, v1");
  }

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    COMPARE(ins_(a0, a1, 31, 1),
            "7ca4ffc4       ins     a0, a1, 31, 1");
    COMPARE(ins_(s6, s7, 30, 2),
            "7ef6ff84       ins     s6, s7, 30, 2");
    COMPARE(ins_(v0, v1, 0, 32),
            "7c62f804       ins     v0, v1, 0, 32");
    COMPARE(ext_(a0, a1, 31, 1),
            "7ca407c0       ext     a0, a1, 31, 1");
    COMPARE(ext_(s6, s7, 30, 2),
            "7ef60f80       ext     s6, s7, 30, 2");
    COMPARE(ext_(v0, v1, 0, 32),
            "7c62f800       ext     v0, v1, 0, 32");
  }
  COMPARE(add_s(f4, f6, f8), "46083100       add.s   f4, f6, f8");
  COMPARE(add_d(f12, f14, f16), "46307300       add.d   f12, f14, f16");

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(bitswap(a0, a1), "7c052020       bitswap a0, a1");
    COMPARE(bitswap(t8, s0), "7c10c020       bitswap t8, s0");
  }

  COMPARE(abs_s(f6, f8), "46004185       abs.s   f6, f8");
  COMPARE(abs_d(f10, f12), "46206285       abs.d   f10, f12");

  COMPARE(div_s(f2, f4, f6), "46062083       div.s   f2, f4, f6");
  COMPARE(div_d(f2, f4, f6), "46262083       div.d   f2, f4, f6");

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(align(v0, a0, a1, 0), "7c851220       align  v0, a0, a1, 0");
    COMPARE(align(v0, a0, a1, 1), "7c851260       align  v0, a0, a1, 1");
    COMPARE(align(v0, a0, a1, 2), "7c8512a0       align  v0, a0, a1, 2");
    COMPARE(align(v0, a0, a1, 3), "7c8512e0       align  v0, a0, a1, 3");
  }

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(aluipc(v0, 0), "ec5f0000       aluipc  v0, 0");
    COMPARE(aluipc(v0, 1), "ec5f0001       aluipc  v0, 1");
    COMPARE(aluipc(v0, 32767), "ec5f7fff       aluipc  v0, 32767");
    COMPARE(aluipc(v0, -32768), "ec5f8000       aluipc  v0, -32768");
    COMPARE(aluipc(v0, -1), "ec5fffff       aluipc  v0, -1");
  }

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(auipc(t8, 0), "ef1e0000       auipc   t8, 0");
    COMPARE(auipc(t8, 1), "ef1e0001       auipc   t8, 1");
    COMPARE(auipc(t8, 32767), "ef1e7fff       auipc   t8, 32767");
    COMPARE(auipc(t8, -32768), "ef1e8000       auipc   t8, -32768");
    COMPARE(auipc(t8, -1), "ef1effff       auipc   t8, -1");
  }

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(lwpc(t1, 0), "ed280000       lwpc    t1, 0");
    COMPARE(lwpc(t1, 4), "ed280004       lwpc    t1, 4");
    COMPARE(lwpc(t1, -4), "ed2ffffc       lwpc    t1, -4");
  }

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(jic(t0, -32768), "d8088000       jic     t0, -32768");
    COMPARE(jic(t0, -1), "d808ffff       jic     t0, -1");
    COMPARE(jic(t0, 0), "d8080000       jic     t0, 0");
    COMPARE(jic(t0, 4), "d8080004       jic     t0, 4");
    COMPARE(jic(t0, 32767), "d8087fff       jic     t0, 32767");
  }

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(addiupc(a0, 262143), "ec83ffff       addiupc a0, 262143");
    COMPARE(addiupc(a0, -1), "ec87ffff       addiupc a0, -1");
    COMPARE(addiupc(v0, 0), "ec400000       addiupc v0, 0");
    COMPARE(addiupc(s1, 1), "ee200001       addiupc s1, 1");
    COMPARE(addiupc(a0, -262144), "ec840000       addiupc a0, -262144");
  }

  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(jialc(a0, -32768), "f8048000       jialc   a0, -32768");
    COMPARE(jialc(a0, -1), "f804ffff       jialc   a0, -1");
    COMPARE(jialc(v0, 0), "f8020000       jialc   v0, 0");
    COMPARE(jialc(s1, 1), "f8110001       jialc   s1, 1");
    COMPARE(jialc(a0, 32767), "f8047fff       jialc   a0, 32767");
  }

  VERIFY_RUN();
}


TEST(Type1) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(seleqz(a0, a1, a2), "00a62035       seleqz    a0, a1, a2");
    COMPARE(selnez(a0, a1, a2), "00a62037       selnez    a0, a1, a2");


    COMPARE(seleqz_d(f3, f4, f5), "462520d4       seleqz.d    f3, f4, f5");
    COMPARE(selnez_d(f3, f4, f5), "462520d7       selnez.d    f3, f4, f5");
    COMPARE(seleqz_s(f3, f4, f5), "460520d4       seleqz.s    f3, f4, f5");
    COMPARE(selnez_s(f3, f4, f5), "460520d7       selnez.s    f3, f4, f5");

    COMPARE(min_d(f3, f4, f5), "462520dc       min.d    f3, f4, f5");
    COMPARE(max_d(f3, f4, f5), "462520de       max.d    f3, f4, f5");

    COMPARE(sel_s(f3, f4, f5), "460520d0       sel.s      f3, f4, f5");
    COMPARE(sel_d(f3, f4, f5), "462520d0       sel.d      f3, f4, f5");

    COMPARE(rint_d(f8, f6), "4620321a       rint.d    f8, f6");
    COMPARE(rint_s(f8, f6), "4600321a       rint.s    f8, f6");

    COMPARE(min_s(f3, f4, f5), "460520dc       min.s    f3, f4, f5");
    COMPARE(max_s(f3, f4, f5), "460520de       max.s    f3, f4, f5");

    COMPARE(mina_d(f3, f4, f5), "462520dd       mina.d   f3, f4, f5");
    COMPARE(mina_s(f3, f4, f5), "460520dd       mina.s   f3, f4, f5");

    COMPARE(maxa_d(f3, f4, f5), "462520df       maxa.d   f3, f4, f5");
    COMPARE(maxa_s(f3, f4, f5), "460520df       maxa.s   f3, f4, f5");
  }

  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    COMPARE(trunc_l_d(f8, f6), "46203209       trunc.l.d f8, f6");
    COMPARE(trunc_l_s(f8, f6), "46003209       trunc.l.s f8, f6");

    COMPARE(round_l_s(f8, f6), "46003208       round.l.s f8, f6");
    COMPARE(round_l_d(f8, f6), "46203208       round.l.d f8, f6");

    COMPARE(floor_l_s(f8, f6), "4600320b       floor.l.s f8, f6");
    COMPARE(floor_l_d(f8, f6), "4620320b       floor.l.d f8, f6");

    COMPARE(ceil_l_s(f8, f6), "4600320a       ceil.l.s f8, f6");
    COMPARE(ceil_l_d(f8, f6), "4620320a       ceil.l.d f8, f6");
  }

  COMPARE(trunc_w_d(f8, f6), "4620320d       trunc.w.d f8, f6");
  COMPARE(trunc_w_s(f8, f6), "4600320d       trunc.w.s f8, f6");

  COMPARE(round_w_s(f8, f6), "4600320c       round.w.s f8, f6");
  COMPARE(round_w_d(f8, f6), "4620320c       round.w.d f8, f6");

  COMPARE(floor_w_s(f8, f6), "4600320f       floor.w.s f8, f6");
  COMPARE(floor_w_d(f8, f6), "4620320f       floor.w.d f8, f6");

  COMPARE(ceil_w_s(f8, f6), "4600320e       ceil.w.s f8, f6");
  COMPARE(ceil_w_d(f8, f6), "4620320e       ceil.w.d f8, f6");

  COMPARE(sub_s(f10, f8, f6), "46064281       sub.s   f10, f8, f6");
  COMPARE(sub_d(f10, f8, f6), "46264281       sub.d   f10, f8, f6");

  COMPARE(sqrt_s(f8, f6), "46003204       sqrt.s  f8, f6");
  COMPARE(sqrt_d(f8, f6), "46203204       sqrt.d  f8, f6");

  COMPARE(neg_s(f8, f6), "46003207       neg.s   f8, f6");
  COMPARE(neg_d(f8, f6), "46203207       neg.d   f8, f6");

  COMPARE(mul_s(f8, f6, f4), "46043202       mul.s   f8, f6, f4");
  COMPARE(mul_d(f8, f6, f4), "46243202       mul.d   f8, f6, f4");

  if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
    COMPARE(rsqrt_s(f8, f6), "46003216       rsqrt.s  f8, f6");
    COMPARE(rsqrt_d(f8, f6), "46203216       rsqrt.d  f8, f6");

    COMPARE(recip_s(f8, f6), "46003215       recip.s  f8, f6");
    COMPARE(recip_d(f8, f6), "46203215       recip.d  f8, f6");
  }

  COMPARE(mov_s(f6, f4), "46002186       mov.s   f6, f4");
  COMPARE(mov_d(f6, f4), "46202186       mov.d   f6, f4");

  if (IsMipsArchVariant(kMips32r2)) {
    COMPARE(movz_s(f6, f4, t0), "46082192       movz.s    f6, f4, t0");
    COMPARE(movz_d(f6, f4, t0), "46282192       movz.d    f6, f4, t0");

    COMPARE(movt_s(f6, f4, 4), "46112191       movt.s    f6, f4, cc(1)");
    COMPARE(movt_d(f6, f4, 4), "46312191       movt.d    f6, f4, cc(1)");

    COMPARE(movf_s(f6, f4, 4), "46102191       movf.s    f6, f4, cc(1)");
    COMPARE(movf_d(f6, f4, 4), "46302191       movf.d    f6, f4, cc(1)");

    COMPARE(movn_s(f6, f4, t0), "46082193       movn.s    f6, f4, t0");
    COMPARE(movn_d(f6, f4, t0), "46282193       movn.d    f6, f4, t0");
  }
  VERIFY_RUN();
}


TEST(Type2) {
  if (IsMipsArchVariant(kMips32r6)) {
    SET_UP();

    COMPARE(class_s(f3, f4), "460020db       class.s f3, f4");
    COMPARE(class_d(f2, f3), "4620189b       class.d f2, f3");

    VERIFY_RUN();
  }
}


TEST(C_FMT_DISASM) {
  if (IsMipsArchVariant(kMips32r1) || IsMipsArchVariant(kMips32r2)) {
    SET_UP();

    COMPARE(c_s(F, f8, f10, 0), "460a4030       c.f.s   f8, f10, cc(0)");
    COMPARE(c_d(F, f8, f10, 0), "462a4030       c.f.d   f8, f10, cc(0)");

    COMPARE(c_s(UN, f8, f10, 2), "460a4231       c.un.s  f8, f10, cc(2)");
    COMPARE(c_d(UN, f8, f10, 2), "462a4231       c.un.d  f8, f10, cc(2)");

    COMPARE(c_s(EQ, f8, f10, 4), "460a4432       c.eq.s  f8, f10, cc(4)");
    COMPARE(c_d(EQ, f8, f10, 4), "462a4432       c.eq.d  f8, f10, cc(4)");

    COMPARE(c_s(UEQ, f8, f10, 6), "460a4633       c.ueq.s f8, f10, cc(6)");
    COMPARE(c_d(UEQ, f8, f10, 6), "462a4633       c.ueq.d f8, f10, cc(6)");

    COMPARE(c_s(OLT, f8, f10, 0), "460a4034       c.olt.s f8, f10, cc(0)");
    COMPARE(c_d(OLT, f8, f10, 0), "462a4034       c.olt.d f8, f10, cc(0)");

    COMPARE(c_s(ULT, f8, f10, 2), "460a4235       c.ult.s f8, f10, cc(2)");
    COMPARE(c_d(ULT, f8, f10, 2), "462a4235       c.ult.d f8, f10, cc(2)");

    COMPARE(c_s(OLE, f8, f10, 4), "460a4436       c.ole.s f8, f10, cc(4)");
    COMPARE(c_d(OLE, f8, f10, 4), "462a4436       c.ole.d f8, f10, cc(4)");

    COMPARE(c_s(ULE, f8, f10, 6), "460a4637       c.ule.s f8, f10, cc(6)");
    COMPARE(c_d(ULE, f8, f10, 6), "462a4637       c.ule.d f8, f10, cc(6)");

    VERIFY_RUN();
  }
}


TEST(COND_FMT_DISASM) {
  if (IsMipsArchVariant(kMips32r6)) {
    SET_UP();

    COMPARE(cmp_s(F, f6, f8, f10), "468a4180       cmp.af.s    f6, f8, f10");
    COMPARE(cmp_d(F, f6, f8, f10), "46aa4180       cmp.af.d  f6,  f8, f10");

    COMPARE(cmp_s(UN, f6, f8, f10), "468a4181       cmp.un.s    f6, f8, f10");
    COMPARE(cmp_d(UN, f6, f8, f10), "46aa4181       cmp.un.d  f6,  f8, f10");

    COMPARE(cmp_s(EQ, f6, f8, f10), "468a4182       cmp.eq.s    f6, f8, f10");
    COMPARE(cmp_d(EQ, f6, f8, f10), "46aa4182       cmp.eq.d  f6,  f8, f10");

    COMPARE(cmp_s(UEQ, f6, f8, f10), "468a4183       cmp.ueq.s   f6, f8, f10");
    COMPARE(cmp_d(UEQ, f6, f8, f10), "46aa4183       cmp.ueq.d  f6,  f8, f10");

    COMPARE(cmp_s(LT, f6, f8, f10), "468a4184       cmp.lt.s    f6, f8, f10");
    COMPARE(cmp_d(LT, f6, f8, f10), "46aa4184       cmp.lt.d  f6,  f8, f10");

    COMPARE(cmp_s(ULT, f6, f8, f10), "468a4185       cmp.ult.s   f6, f8, f10");
    COMPARE(cmp_d(ULT, f6, f8, f10), "46aa4185       cmp.ult.d  f6,  f8, f10");

    COMPARE(cmp_s(LE, f6, f8, f10), "468a4186       cmp.le.s    f6, f8, f10");
    COMPARE(cmp_d(LE, f6, f8, f10), "46aa4186       cmp.le.d  f6,  f8, f10");

    COMPARE(cmp_s(ULE, f6, f8, f10), "468a4187       cmp.ule.s   f6, f8, f10");
    COMPARE(cmp_d(ULE, f6, f8, f10), "46aa4187       cmp.ule.d  f6,  f8, f10");

    COMPARE(cmp_s(ORD, f6, f8, f10), "468a4191       cmp.or.s    f6, f8, f10");
    COMPARE(cmp_d(ORD, f6, f8, f10), "46aa4191       cmp.or.d  f6,  f8, f10");

    COMPARE(cmp_s(UNE, f6, f8, f10), "468a4192       cmp.une.s   f6, f8, f10");
    COMPARE(cmp_d(UNE, f6, f8, f10), "46aa4192       cmp.une.d  f6,  f8, f10");

    COMPARE(cmp_s(NE, f6, f8, f10), "468a4193       cmp.ne.s    f6, f8, f10");
    COMPARE(cmp_d(NE, f6, f8, f10), "46aa4193       cmp.ne.d  f6,  f8, f10");

    VERIFY_RUN();
  }
}


TEST(CVT_DISSASM) {
  SET_UP();
  COMPARE(cvt_d_s(f22, f24), "4600c5a1       cvt.d.s f22, f24");
  COMPARE(cvt_d_w(f22, f24), "4680c5a1       cvt.d.w f22, f24");

  COMPARE(cvt_s_d(f22, f24), "4620c5a0       cvt.s.d f22, f24");
  COMPARE(cvt_s_w(f22, f24), "4680c5a0       cvt.s.w f22, f24");

  if ((IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) &&
      IsFp64Mode()) {
    COMPARE(cvt_d_l(f22, f24), "46a0c5a1       cvt.d.l f22, f24");
    COMPARE(cvt_l_d(f22, f24), "4620c5a5       cvt.l.d f22, f24");

    COMPARE(cvt_s_l(f22, f24), "46a0c5a0       cvt.s.l f22, f24");
    COMPARE(cvt_l_s(f22, f24), "4600c5a5       cvt.l.s f22, f24");
  }

  VERIFY_RUN();
}


TEST(ctc1_cfc1_disasm) {
  SET_UP();
  COMPARE(abs_d(f10, f31), "4620fa85       abs.d   f10, f31");
  COMPARE(ceil_w_s(f8, f31), "4600fa0e       ceil.w.s f8, f31");
  COMPARE(ctc1(a0, FCSR), "44c4f800       ctc1    a0, FCSR");
  COMPARE(cfc1(a0, FCSR), "4444f800       cfc1    a0, FCSR");
  VERIFY_RUN();
}

TEST(madd_msub_maddf_msubf) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r2)) {
    COMPARE(madd_s(f4, f6, f8, f10), "4cca4120       madd.s  f4, f6, f8, f10");
    COMPARE(madd_d(f4, f6, f8, f10), "4cca4121       madd.d  f4, f6, f8, f10");
    COMPARE(msub_s(f4, f6, f8, f10), "4cca4128       msub.s  f4, f6, f8, f10");
    COMPARE(msub_d(f4, f6, f8, f10), "4cca4129       msub.d  f4, f6, f8, f10");
  }
  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(maddf_s(f4, f8, f10), "460a4118       maddf.s  f4, f8, f10");
    COMPARE(maddf_d(f4, f8, f10), "462a4118       maddf.d  f4, f8, f10");
    COMPARE(msubf_s(f4, f8, f10), "460a4119       msubf.s  f4, f8, f10");
    COMPARE(msubf_d(f4, f8, f10), "462a4119       msubf.d  f4, f8, f10");
  }
  VERIFY_RUN();
}
