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
#include "src/frames-inl.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

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

#define COMPARE_MSA_BRANCH(asm_, compare_string, offset)                       \
  {                                                                            \
    int pc_offset = assm.pc_offset();                                          \
    byte* progcounter = &buffer[pc_offset];                                    \
    char str_with_address[100];                                                \
    snprintf(str_with_address, sizeof(str_with_address), "%s -> %p",           \
             compare_string,                                                   \
             static_cast<void*>(progcounter + 4 + (offset * 4)));              \
    assm.asm_;                                                                 \
    if (!DisassembleAndCompare(progcounter, str_with_address)) failure = true; \
  }

#define COMPARE_PC_JUMP(asm_, compare_string, target)                          \
  {                                                                            \
    int pc_offset = assm.pc_offset();                                          \
    byte* progcounter = &buffer[pc_offset];                                    \
    char str_with_address[100];                                                \
    int instr_index = (target >> 2) & kImm26Mask;                              \
    snprintf(                                                                  \
        str_with_address, sizeof(str_with_address), "%s %p -> %p",             \
        compare_string, reinterpret_cast<void*>(target),                       \
        reinterpret_cast<void*>(((uint32_t)(progcounter + 4) & ~0xFFFFFFF) |   \
                                (instr_index << 2)));                          \
    assm.asm_;                                                                 \
    if (!DisassembleAndCompare(progcounter, str_with_address)) failure = true; \
  }

#define GET_PC_REGION(pc_region)                                         \
  {                                                                      \
    int pc_offset = assm.pc_offset();                                    \
    byte* progcounter = &buffer[pc_offset];                              \
    pc_region = reinterpret_cast<int32_t>(progcounter + 4) & ~0xFFFFFFF; \
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
  target = pc_region | 0xFFFFFFC;
  COMPARE_PC_JUMP(j(target), "0bffffff       j      ", target);

  target = pc_region | 0x4;
  COMPARE_PC_JUMP(jal(target), "0c000001       jal    ", target);
  target = pc_region | 0xFFFFFFC;
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

TEST(atomic_load_store) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6)) {
    COMPARE(ll(v0, MemOperand(v1, -1)), "7c62ffb6       ll     v0, -1(v1)");
    COMPARE(sc(v0, MemOperand(v1, 1)), "7c6200a6       sc     v0, 1(v1)");
  } else {
    COMPARE(ll(v0, MemOperand(v1, -1)), "c062ffff       ll     v0, -1(v1)");
    COMPARE(sc(v0, MemOperand(v1, 1)), "e0620001       sc     v0, 1(v1)");
  }
  VERIFY_RUN();
}

TEST(MSA_BRANCH) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE_MSA_BRANCH(bnz_b(w0, 1), "47800001       bnz.b  w0, 1", 1);
    COMPARE_MSA_BRANCH(bnz_h(w1, -1), "47a1ffff       bnz.h  w1, -1", -1);
    COMPARE_MSA_BRANCH(bnz_w(w2, 32767), "47c27fff       bnz.w  w2, 32767",
                       32767);
    COMPARE_MSA_BRANCH(bnz_d(w3, -32768), "47e38000       bnz.d  w3, -32768",
                       -32768);
    COMPARE_MSA_BRANCH(bnz_v(w0, static_cast<int16_t>(0)),
                       "45e00000       bnz.v  w0, 0", 0);
    COMPARE_MSA_BRANCH(bz_b(w0, 1), "47000001       bz.b  w0, 1", 1);
    COMPARE_MSA_BRANCH(bz_h(w1, -1), "4721ffff       bz.h  w1, -1", -1);
    COMPARE_MSA_BRANCH(bz_w(w2, 32767), "47427fff       bz.w  w2, 32767",
                       32767);
    COMPARE_MSA_BRANCH(bz_d(w3, -32768), "47638000       bz.d  w3, -32768",
                       -32768);
    COMPARE_MSA_BRANCH(bz_v(w0, static_cast<int16_t>(0)),
                       "45600000       bz.v  w0, 0", 0);
  }
  VERIFY_RUN();
}

TEST(MSA_MI10) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(ld_b(w0, MemOperand(at, -512)),
            "7a000820       ld.b  w0, -512(at)");
    COMPARE(ld_b(w1, MemOperand(v0, 0)), "78001060       ld.b  w1, 0(v0)");
    COMPARE(ld_b(w2, MemOperand(v1, 511)), "79ff18a0       ld.b  w2, 511(v1)");
    COMPARE(ld_h(w4, MemOperand(a1, -512)),
            "7a002921       ld.h  w4, -512(a1)");
    COMPARE(ld_h(w5, MemOperand(a2, 64)), "78403161       ld.h  w5, 64(a2)");
    COMPARE(ld_h(w6, MemOperand(a3, 511)), "79ff39a1       ld.h  w6, 511(a3)");
    COMPARE(ld_w(w10, MemOperand(t3, -512)),
            "7a005aa2       ld.w  w10, -512(t3)");
    COMPARE(ld_w(w11, MemOperand(t4, 511)),
            "79ff62e2       ld.w  w11, 511(t4)");
    COMPARE(ld_w(w12, MemOperand(t5, -128)),
            "7b806b22       ld.w  w12, -128(t5)");
    COMPARE(ld_d(w17, MemOperand(s2, -512)),
            "7a009463       ld.d  w17, -512(s2)");
    COMPARE(ld_d(w18, MemOperand(s3, 128)),
            "78809ca3       ld.d  w18, 128(s3)");
    COMPARE(ld_d(w19, MemOperand(s4, 511)),
            "79ffa4e3       ld.d  w19, 511(s4)");
    COMPARE(st_b(w0, MemOperand(at, -512)),
            "7a000824       st.b  w0, -512(at)");
    COMPARE(st_b(w1, MemOperand(v0, 0)), "78001064       st.b  w1, 0(v0)");
    COMPARE(st_b(w2, MemOperand(v1, 511)), "79ff18a4       st.b  w2, 511(v1)");
    COMPARE(st_h(w4, MemOperand(a1, -512)),
            "7a002925       st.h  w4, -512(a1)");
    COMPARE(st_h(w5, MemOperand(a2, 64)), "78403165       st.h  w5, 64(a2)");
    COMPARE(st_h(w6, MemOperand(a3, 511)), "79ff39a5       st.h  w6, 511(a3)");
    COMPARE(st_w(w10, MemOperand(t3, -512)),
            "7a005aa6       st.w  w10, -512(t3)");
    COMPARE(st_w(w11, MemOperand(t4, 511)),
            "79ff62e6       st.w  w11, 511(t4)");
    COMPARE(st_w(w12, MemOperand(t5, -128)),
            "7b806b26       st.w  w12, -128(t5)");
    COMPARE(st_d(w17, MemOperand(s2, -512)),
            "7a009467       st.d  w17, -512(s2)");
    COMPARE(st_d(w18, MemOperand(s3, 128)),
            "78809ca7       st.d  w18, 128(s3)");
    COMPARE(st_d(w19, MemOperand(s4, 511)),
            "79ffa4e7       st.d  w19, 511(s4)");
  }
  VERIFY_RUN();
}

TEST(MSA_I5) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(addvi_b(w3, w31, 30), "781ef8c6       addvi.b  w3, w31, 30");
    COMPARE(addvi_h(w24, w13, 26), "783a6e06       addvi.h  w24, w13, 26");
    COMPARE(addvi_w(w26, w20, 26), "785aa686       addvi.w  w26, w20, 26");
    COMPARE(addvi_d(w16, w1, 21), "78750c06       addvi.d  w16, w1, 21");
    COMPARE(ceqi_b(w24, w21, -8), "7818ae07       ceqi.b  w24, w21, -8");
    COMPARE(ceqi_h(w31, w15, 2), "78227fc7       ceqi.h  w31, w15, 2");
    COMPARE(ceqi_w(w12, w1, -1), "785f0b07       ceqi.w  w12, w1, -1");
    COMPARE(ceqi_d(w24, w22, 7), "7867b607       ceqi.d  w24, w22, 7");
    COMPARE(clei_s_b(w12, w16, 1), "7a018307       clei_s.b  w12, w16, 1");
    COMPARE(clei_s_h(w2, w10, -9), "7a375087       clei_s.h  w2, w10, -9");
    COMPARE(clei_s_w(w4, w11, -10), "7a565907       clei_s.w  w4, w11, -10");
    COMPARE(clei_s_d(w0, w29, -10), "7a76e807       clei_s.d  w0, w29, -10");
    COMPARE(clei_u_b(w21, w17, 3), "7a838d47       clei_u.b  w21, w17, 3");
    COMPARE(clei_u_h(w29, w7, 17), "7ab13f47       clei_u.h  w29, w7, 17");
    COMPARE(clei_u_w(w1, w1, 2), "7ac20847       clei_u.w  w1, w1, 2");
    COMPARE(clei_u_d(w27, w27, 29), "7afddec7       clei_u.d  w27, w27, 29");
    COMPARE(clti_s_b(w19, w13, -7), "79196cc7       clti_s.b  w19, w13, -7");
    COMPARE(clti_s_h(w15, w10, -12), "793453c7       clti_s.h  w15, w10, -12");
    COMPARE(clti_s_w(w12, w12, 11), "794b6307       clti_s.w  w12, w12, 11");
    COMPARE(clti_s_d(w29, w20, -15), "7971a747       clti_s.d  w29, w20, -15");
    COMPARE(clti_u_b(w14, w9, 29), "799d4b87       clti_u.b  w14, w9, 29");
    COMPARE(clti_u_h(w24, w25, 25), "79b9ce07       clti_u.h  w24, w25, 25");
    COMPARE(clti_u_w(w1, w1, 22), "79d60847       clti_u.w  w1, w1, 22");
    COMPARE(clti_u_d(w21, w25, 1), "79e1cd47       clti_u.d  w21, w25, 1");
    COMPARE(maxi_s_b(w22, w21, 1), "7901ad86       maxi_s.b  w22, w21, 1");
    COMPARE(maxi_s_h(w29, w5, -8), "79382f46       maxi_s.h  w29, w5, -8");
    COMPARE(maxi_s_w(w1, w10, -12), "79545046       maxi_s.w  w1, w10, -12");
    COMPARE(maxi_s_d(w13, w29, -16), "7970eb46       maxi_s.d  w13, w29, -16");
    COMPARE(maxi_u_b(w20, w0, 12), "798c0506       maxi_u.b  w20, w0, 12");
    COMPARE(maxi_u_h(w1, w14, 3), "79a37046       maxi_u.h  w1, w14, 3");
    COMPARE(maxi_u_w(w27, w22, 11), "79cbb6c6       maxi_u.w  w27, w22, 11");
    COMPARE(maxi_u_d(w26, w6, 4), "79e43686       maxi_u.d  w26, w6, 4");
    COMPARE(mini_s_b(w4, w1, 1), "7a010906       mini_s.b  w4, w1, 1");
    COMPARE(mini_s_h(w27, w27, -9), "7a37dec6       mini_s.h  w27, w27, -9");
    COMPARE(mini_s_w(w28, w11, 9), "7a495f06       mini_s.w  w28, w11, 9");
    COMPARE(mini_s_d(w11, w10, 10), "7a6a52c6       mini_s.d  w11, w10, 10");
    COMPARE(mini_u_b(w18, w23, 27), "7a9bbc86       mini_u.b  w18, w23, 27");
    COMPARE(mini_u_h(w7, w26, 18), "7ab2d1c6       mini_u.h  w7, w26, 18");
    COMPARE(mini_u_w(w11, w12, 26), "7ada62c6       mini_u.w  w11, w12, 26");
    COMPARE(mini_u_d(w11, w15, 2), "7ae27ac6       mini_u.d  w11, w15, 2");
    COMPARE(subvi_b(w24, w20, 19), "7893a606       subvi.b  w24, w20, 19");
    COMPARE(subvi_h(w11, w19, 4), "78a49ac6       subvi.h  w11, w19, 4");
    COMPARE(subvi_w(w12, w10, 11), "78cb5306       subvi.w  w12, w10, 11");
    COMPARE(subvi_d(w19, w16, 7), "78e784c6       subvi.d  w19, w16, 7");
  }
  VERIFY_RUN();
}

TEST(MSA_I10) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(ldi_b(w8, 198), "7b063207       ldi.b  w8, 198");
    COMPARE(ldi_h(w20, 313), "7b29cd07       ldi.h  w20, 313");
    COMPARE(ldi_w(w24, 492), "7b4f6607       ldi.w  w24, 492");
    COMPARE(ldi_d(w27, -180), "7b7a66c7       ldi.d  w27, -180");
  }
  VERIFY_RUN();
}

TEST(MSA_I8) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(andi_b(w2, w29, 48), "7830e880       andi.b  w2, w29, 48");
    COMPARE(bmnzi_b(w6, w22, 126), "787eb181       bmnzi.b  w6, w22, 126");
    COMPARE(bmzi_b(w27, w1, 88), "79580ec1       bmzi.b  w27, w1, 88");
    COMPARE(bseli_b(w29, w3, 189), "7abd1f41       bseli.b  w29, w3, 189");
    COMPARE(nori_b(w1, w17, 56), "7a388840       nori.b  w1, w17, 56");
    COMPARE(ori_b(w26, w20, 135), "7987a680       ori.b  w26, w20, 135");
    COMPARE(shf_b(w19, w30, 105), "7869f4c2       shf.b  w19, w30, 105");
    COMPARE(shf_h(w17, w8, 76), "794c4442       shf.h  w17, w8, 76");
    COMPARE(shf_w(w14, w3, 93), "7a5d1b82       shf.w  w14, w3, 93");
    COMPARE(xori_b(w16, w10, 20), "7b145400       xori.b  w16, w10, 20");
  }
  VERIFY_RUN();
}

TEST(MSA_VEC) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(and_v(w25, w20, w27), "781ba65e       and.v  w25, w20, w27");
    COMPARE(bmnz_v(w17, w6, w7), "7887345e       bmnz.v  w17, w6, w7");
    COMPARE(bmz_v(w3, w17, w9), "78a988de       bmz.v  w3, w17, w9");
    COMPARE(bsel_v(w8, w0, w14), "78ce021e       bsel.v  w8, w0, w14");
    COMPARE(nor_v(w7, w31, w0), "7840f9de       nor.v  w7, w31, w0");
    COMPARE(or_v(w24, w26, w30), "783ed61e       or.v  w24, w26, w30");
    COMPARE(xor_v(w7, w27, w15), "786fd9de       xor.v  w7, w27, w15");
  }
  VERIFY_RUN();
}

TEST(MSA_2R) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(fill_b(w30, t1), "7b004f9e       fill.b  w30, t1");
    COMPARE(fill_h(w31, s7), "7b01bfde       fill.h  w31, s7");
    COMPARE(fill_w(w16, t8), "7b02c41e       fill.w  w16, t8");
    COMPARE(nloc_b(w21, w0), "7b08055e       nloc.b  w21, w0");
    COMPARE(nloc_h(w18, w31), "7b09fc9e       nloc.h  w18, w31");
    COMPARE(nloc_w(w2, w23), "7b0ab89e       nloc.w  w2, w23");
    COMPARE(nloc_d(w4, w10), "7b0b511e       nloc.d  w4, w10");
    COMPARE(nlzc_b(w31, w2), "7b0c17de       nlzc.b  w31, w2");
    COMPARE(nlzc_h(w27, w22), "7b0db6de       nlzc.h  w27, w22");
    COMPARE(nlzc_w(w10, w29), "7b0eea9e       nlzc.w  w10, w29");
    COMPARE(nlzc_d(w25, w9), "7b0f4e5e       nlzc.d  w25, w9");
    COMPARE(pcnt_b(w20, w18), "7b04951e       pcnt.b  w20, w18");
    COMPARE(pcnt_h(w0, w8), "7b05401e       pcnt.h  w0, w8");
    COMPARE(pcnt_w(w23, w9), "7b064dde       pcnt.w  w23, w9");
    COMPARE(pcnt_d(w21, w24), "7b07c55e       pcnt.d  w21, w24");
  }
  VERIFY_RUN();
}

TEST(MSA_2RF) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(fclass_w(w26, w12), "7b20669e       fclass.w  w26, w12");
    COMPARE(fclass_d(w24, w17), "7b218e1e       fclass.d  w24, w17");
    COMPARE(fexupl_w(w8, w0), "7b30021e       fexupl.w  w8, w0");
    COMPARE(fexupl_d(w17, w29), "7b31ec5e       fexupl.d  w17, w29");
    COMPARE(fexupr_w(w13, w4), "7b32235e       fexupr.w  w13, w4");
    COMPARE(fexupr_d(w5, w2), "7b33115e       fexupr.d  w5, w2");
    COMPARE(ffint_s_w(w20, w29), "7b3ced1e       ffint_s.w  w20, w29");
    COMPARE(ffint_s_d(w12, w15), "7b3d7b1e       ffint_s.d  w12, w15");
    COMPARE(ffint_u_w(w7, w27), "7b3ed9de       ffint_u.w  w7, w27");
    COMPARE(ffint_u_d(w19, w16), "7b3f84de       ffint_u.d  w19, w16");
    COMPARE(ffql_w(w31, w13), "7b346fde       ffql.w  w31, w13");
    COMPARE(ffql_d(w12, w13), "7b356b1e       ffql.d  w12, w13");
    COMPARE(ffqr_w(w27, w30), "7b36f6de       ffqr.w  w27, w30");
    COMPARE(ffqr_d(w30, w15), "7b377f9e       ffqr.d  w30, w15");
    COMPARE(flog2_w(w25, w31), "7b2efe5e       flog2.w  w25, w31");
    COMPARE(flog2_d(w18, w10), "7b2f549e       flog2.d  w18, w10");
    COMPARE(frint_w(w7, w15), "7b2c79de       frint.w  w7, w15");
    COMPARE(frint_d(w21, w22), "7b2db55e       frint.d  w21, w22");
    COMPARE(frcp_w(w19, w0), "7b2a04de       frcp.w  w19, w0");
    COMPARE(frcp_d(w4, w14), "7b2b711e       frcp.d  w4, w14");
    COMPARE(frsqrt_w(w12, w17), "7b288b1e       frsqrt.w  w12, w17");
    COMPARE(frsqrt_d(w23, w11), "7b295dde       frsqrt.d  w23, w11");
    COMPARE(fsqrt_w(w0, w11), "7b26581e       fsqrt.w  w0, w11");
    COMPARE(fsqrt_d(w15, w12), "7b2763de       fsqrt.d  w15, w12");
    COMPARE(ftint_s_w(w30, w5), "7b382f9e       ftint_s.w  w30, w5");
    COMPARE(ftint_s_d(w5, w23), "7b39b95e       ftint_s.d  w5, w23");
    COMPARE(ftint_u_w(w20, w14), "7b3a751e       ftint_u.w  w20, w14");
    COMPARE(ftint_u_d(w23, w21), "7b3badde       ftint_u.d  w23, w21");
    COMPARE(ftrunc_s_w(w29, w17), "7b228f5e       ftrunc_s.w  w29, w17");
    COMPARE(ftrunc_s_d(w12, w27), "7b23db1e       ftrunc_s.d  w12, w27");
    COMPARE(ftrunc_u_w(w17, w15), "7b247c5e       ftrunc_u.w  w17, w15");
    COMPARE(ftrunc_u_d(w5, w27), "7b25d95e       ftrunc_u.d  w5, w27");
  }
  VERIFY_RUN();
}

TEST(MSA_3R) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(add_a_b(w26, w9, w4), "78044e90       add_a.b  w26, w9, w4");
    COMPARE(add_a_h(w23, w27, w31), "783fddd0       add_a.h  w23, w27, w31");
    COMPARE(add_a_w(w11, w6, w22), "785632d0       add_a.w  w11, w6, w22");
    COMPARE(add_a_d(w6, w10, w0), "78605190       add_a.d  w6, w10, w0");
    COMPARE(adds_a_b(w19, w24, w19), "7893c4d0       adds_a.b  w19, w24, w19");
    COMPARE(adds_a_h(w25, w6, w4), "78a43650       adds_a.h  w25, w6, w4");
    COMPARE(adds_a_w(w25, w17, w27), "78db8e50       adds_a.w  w25, w17, w27");
    COMPARE(adds_a_d(w15, w18, w26), "78fa93d0       adds_a.d  w15, w18, w26");
    COMPARE(adds_s_b(w29, w11, w19), "79135f50       adds_s.b  w29, w11, w19");
    COMPARE(adds_s_h(w5, w23, w26), "793ab950       adds_s.h  w5, w23, w26");
    COMPARE(adds_s_w(w16, w14, w13), "794d7410       adds_s.w  w16, w14, w13");
    COMPARE(adds_s_d(w2, w14, w28), "797c7090       adds_s.d  w2, w14, w28");
    COMPARE(adds_u_b(w3, w17, w14), "798e88d0       adds_u.b  w3, w17, w14");
    COMPARE(adds_u_h(w10, w30, w4), "79a4f290       adds_u.h  w10, w30, w4");
    COMPARE(adds_u_w(w15, w18, w20), "79d493d0       adds_u.w  w15, w18, w20");
    COMPARE(adds_u_d(w30, w10, w9), "79e95790       adds_u.d  w30, w10, w9");
    COMPARE(addv_b(w24, w20, w21), "7815a60e       addv.b  w24, w20, w21");
    COMPARE(addv_h(w4, w13, w27), "783b690e       addv.h  w4, w13, w27");
    COMPARE(addv_w(w19, w11, w14), "784e5cce       addv.w  w19, w11, w14");
    COMPARE(addv_d(w2, w21, w31), "787fa88e       addv.d  w2, w21, w31");
    COMPARE(asub_s_b(w23, w16, w3), "7a0385d1       asub_s.b  w23, w16, w3");
    COMPARE(asub_s_h(w22, w17, w25), "7a398d91       asub_s.h  w22, w17, w25");
    COMPARE(asub_s_w(w24, w1, w9), "7a490e11       asub_s.w  w24, w1, w9");
    COMPARE(asub_s_d(w13, w12, w12), "7a6c6351       asub_s.d  w13, w12, w12");
    COMPARE(asub_u_b(w10, w29, w11), "7a8bea91       asub_u.b  w10, w29, w11");
    COMPARE(asub_u_h(w18, w9, w15), "7aaf4c91       asub_u.h  w18, w9, w15");
    COMPARE(asub_u_w(w10, w19, w31), "7adf9a91       asub_u.w  w10, w19, w31");
    COMPARE(asub_u_d(w17, w10, w0), "7ae05451       asub_u.d  w17, w10, w0");
    COMPARE(ave_s_b(w2, w5, w1), "7a012890       ave_s.b  w2, w5, w1");
    COMPARE(ave_s_h(w16, w19, w9), "7a299c10       ave_s.h  w16, w19, w9");
    COMPARE(ave_s_w(w17, w31, w5), "7a45fc50       ave_s.w  w17, w31, w5");
    COMPARE(ave_s_d(w27, w25, w10), "7a6aced0       ave_s.d  w27, w25, w10");
    COMPARE(ave_u_b(w16, w19, w9), "7a899c10       ave_u.b  w16, w19, w9");
    COMPARE(ave_u_h(w28, w28, w11), "7aabe710       ave_u.h  w28, w28, w11");
    COMPARE(ave_u_w(w11, w12, w11), "7acb62d0       ave_u.w  w11, w12, w11");
    COMPARE(ave_u_d(w30, w19, w28), "7afc9f90       ave_u.d  w30, w19, w28");
    COMPARE(aver_s_b(w26, w16, w2), "7b028690       aver_s.b  w26, w16, w2");
    COMPARE(aver_s_h(w31, w27, w27), "7b3bdfd0       aver_s.h  w31, w27, w27");
    COMPARE(aver_s_w(w28, w18, w25), "7b599710       aver_s.w  w28, w18, w25");
    COMPARE(aver_s_d(w29, w21, w27), "7b7baf50       aver_s.d  w29, w21, w27");
    COMPARE(aver_u_b(w29, w26, w3), "7b83d750       aver_u.b  w29, w26, w3");
    COMPARE(aver_u_h(w18, w18, w9), "7ba99490       aver_u.h  w18, w18, w9");
    COMPARE(aver_u_w(w17, w25, w29), "7bddcc50       aver_u.w  w17, w25, w29");
    COMPARE(aver_u_d(w22, w22, w19), "7bf3b590       aver_u.d  w22, w22, w19");
    COMPARE(bclr_b(w2, w15, w29), "799d788d       bclr.b  w2, w15, w29");
    COMPARE(bclr_h(w16, w21, w28), "79bcac0d       bclr.h  w16, w21, w28");
    COMPARE(bclr_w(w19, w2, w9), "79c914cd       bclr.w  w19, w2, w9");
    COMPARE(bclr_d(w27, w31, w4), "79e4fecd       bclr.d  w27, w31, w4");
    COMPARE(binsl_b(w5, w16, w24), "7b18814d       binsl.b  w5, w16, w24");
    COMPARE(binsl_h(w30, w5, w10), "7b2a2f8d       binsl.h  w30, w5, w10");
    COMPARE(binsl_w(w14, w15, w13), "7b4d7b8d       binsl.w  w14, w15, w13");
    COMPARE(binsl_d(w23, w20, w12), "7b6ca5cd       binsl.d  w23, w20, w12");
    COMPARE(binsr_b(w22, w11, w2), "7b825d8d       binsr.b  w22, w11, w2");
    COMPARE(binsr_h(w0, w26, w6), "7ba6d00d       binsr.h  w0, w26, w6");
    COMPARE(binsr_w(w26, w3, w28), "7bdc1e8d       binsr.w  w26, w3, w28");
    COMPARE(binsr_d(w0, w0, w21), "7bf5000d       binsr.d  w0, w0, w21");
    COMPARE(bneg_b(w0, w11, w24), "7a98580d       bneg.b  w0, w11, w24");
    COMPARE(bneg_h(w28, w16, w4), "7aa4870d       bneg.h  w28, w16, w4");
    COMPARE(bneg_w(w3, w26, w19), "7ad3d0cd       bneg.w  w3, w26, w19");
    COMPARE(bneg_d(w13, w29, w15), "7aefeb4d       bneg.d  w13, w29, w15");
    COMPARE(bset_b(w31, w5, w31), "7a1f2fcd       bset.b  w31, w5, w31");
    COMPARE(bset_h(w14, w12, w6), "7a26638d       bset.h  w14, w12, w6");
    COMPARE(bset_w(w31, w9, w12), "7a4c4fcd       bset.w  w31, w9, w12");
    COMPARE(bset_d(w5, w22, w5), "7a65b14d       bset.d  w5, w22, w5");
    COMPARE(ceq_b(w31, w31, w18), "7812ffcf       ceq.b  w31, w31, w18");
    COMPARE(ceq_h(w10, w27, w9), "7829da8f       ceq.h  w10, w27, w9");
    COMPARE(ceq_w(w9, w5, w14), "784e2a4f       ceq.w  w9, w5, w14");
    COMPARE(ceq_d(w5, w17, w0), "7860894f       ceq.d  w5, w17, w0");
    COMPARE(cle_s_b(w23, w4, w9), "7a0925cf       cle_s.b  w23, w4, w9");
    COMPARE(cle_s_h(w22, w27, w19), "7a33dd8f       cle_s.h  w22, w27, w19");
    COMPARE(cle_s_w(w30, w26, w10), "7a4ad78f       cle_s.w  w30, w26, w10");
    COMPARE(cle_s_d(w18, w5, w10), "7a6a2c8f       cle_s.d  w18, w5, w10");
    COMPARE(cle_u_b(w1, w25, w0), "7a80c84f       cle_u.b  w1, w25, w0");
    COMPARE(cle_u_h(w7, w0, w29), "7abd01cf       cle_u.h  w7, w0, w29");
    COMPARE(cle_u_w(w25, w18, w1), "7ac1964f       cle_u.w  w25, w18, w1");
    COMPARE(cle_u_d(w6, w0, w30), "7afe018f       cle_u.d  w6, w0, w30");
    COMPARE(clt_s_b(w25, w2, w21), "7915164f       clt_s.b  w25, w2, w21");
    COMPARE(clt_s_h(w2, w19, w9), "7929988f       clt_s.h  w2, w19, w9");
    COMPARE(clt_s_w(w23, w8, w16), "795045cf       clt_s.w  w23, w8, w16");
    COMPARE(clt_s_d(w7, w30, w12), "796cf1cf       clt_s.d  w7, w30, w12");
    COMPARE(clt_u_b(w2, w31, w13), "798df88f       clt_u.b  w2, w31, w13");
    COMPARE(clt_u_h(w16, w31, w23), "79b7fc0f       clt_u.h  w16, w31, w23");
    COMPARE(clt_u_w(w3, w24, w9), "79c9c0cf       clt_u.w  w3, w24, w9");
    COMPARE(clt_u_d(w7, w0, w1), "79e101cf       clt_u.d  w7, w0, w1");
    COMPARE(div_s_b(w29, w3, w18), "7a121f52       div_s.b  w29, w3, w18");
    COMPARE(div_s_h(w17, w16, w13), "7a2d8452       div_s.h  w17, w16, w13");
    COMPARE(div_s_w(w4, w25, w30), "7a5ec912       div_s.w  w4, w25, w30");
    COMPARE(div_s_d(w31, w9, w20), "7a744fd2       div_s.d  w31, w9, w20");
    COMPARE(div_u_b(w6, w29, w10), "7a8ae992       div_u.b  w6, w29, w10");
    COMPARE(div_u_h(w24, w21, w14), "7aaeae12       div_u.h  w24, w21, w14");
    COMPARE(div_u_w(w29, w14, w25), "7ad97752       div_u.w  w29, w14, w25");
    COMPARE(div_u_d(w31, w1, w21), "7af50fd2       div_u.d  w31, w1, w21");
    COMPARE(dotp_s_h(w23, w22, w25), "7839b5d3       dotp_s.h  w23, w22, w25");
    COMPARE(dotp_s_w(w20, w14, w5), "78457513       dotp_s.w  w20, w14, w5");
    COMPARE(dotp_s_d(w17, w2, w22), "78761453       dotp_s.d  w17, w2, w22");
    COMPARE(dotp_u_h(w13, w2, w6), "78a61353       dotp_u.h  w13, w2, w6");
    COMPARE(dotp_u_w(w15, w22, w21), "78d5b3d3       dotp_u.w  w15, w22, w21");
    COMPARE(dotp_u_d(w4, w16, w26), "78fa8113       dotp_u.d  w4, w16, w26");
    COMPARE(dpadd_s_h(w1, w28, w22), "7936e053       dpadd_s.h  w1, w28, w22");
    COMPARE(dpadd_s_w(w10, w1, w12), "794c0a93       dpadd_s.w  w10, w1, w12");
    COMPARE(dpadd_s_d(w3, w21, w27), "797ba8d3       dpadd_s.d  w3, w21, w27");
    COMPARE(dpadd_u_h(w17, w5, w20), "79b42c53       dpadd_u.h  w17, w5, w20");
    COMPARE(dpadd_u_w(w24, w8, w16), "79d04613       dpadd_u.w  w24, w8, w16");
    COMPARE(dpadd_u_d(w15, w29, w16),
            "79f0ebd3       dpadd_u.d  w15, w29, w16");
    COMPARE(dpsub_s_h(w4, w11, w12), "7a2c5913       dpsub_s.h  w4, w11, w12");
    COMPARE(dpsub_s_w(w4, w7, w6), "7a463913       dpsub_s.w  w4, w7, w6");
    COMPARE(dpsub_s_d(w31, w12, w28),
            "7a7c67d3       dpsub_s.d  w31, w12, w28");
    COMPARE(dpsub_u_h(w4, w25, w17), "7ab1c913       dpsub_u.h  w4, w25, w17");
    COMPARE(dpsub_u_w(w19, w25, w16),
            "7ad0ccd3       dpsub_u.w  w19, w25, w16");
    COMPARE(dpsub_u_d(w7, w10, w26), "7afa51d3       dpsub_u.d  w7, w10, w26");
    COMPARE(hadd_s_h(w28, w24, w2), "7a22c715       hadd_s.h  w28, w24, w2");
    COMPARE(hadd_s_w(w24, w17, w11), "7a4b8e15       hadd_s.w  w24, w17, w11");
    COMPARE(hadd_s_d(w17, w15, w20), "7a747c55       hadd_s.d  w17, w15, w20");
    COMPARE(hadd_u_h(w12, w29, w17), "7ab1eb15       hadd_u.h  w12, w29, w17");
    COMPARE(hadd_u_w(w9, w5, w6), "7ac62a55       hadd_u.w  w9, w5, w6");
    COMPARE(hadd_u_d(w1, w20, w6), "7ae6a055       hadd_u.d  w1, w20, w6");
    COMPARE(hsub_s_h(w16, w14, w29), "7b3d7415       hsub_s.h  w16, w14, w29");
    COMPARE(hsub_s_w(w9, w13, w11), "7b4b6a55       hsub_s.w  w9, w13, w11");
    COMPARE(hsub_s_d(w30, w18, w14), "7b6e9795       hsub_s.d  w30, w18, w14");
    COMPARE(hsub_u_h(w7, w12, w14), "7bae61d5       hsub_u.h  w7, w12, w14");
    COMPARE(hsub_u_w(w21, w5, w5), "7bc52d55       hsub_u.w  w21, w5, w5");
    COMPARE(hsub_u_d(w11, w12, w31), "7bff62d5       hsub_u.d  w11, w12, w31");
    COMPARE(ilvev_b(w18, w16, w30), "7b1e8494       ilvev.b  w18, w16, w30");
    COMPARE(ilvev_h(w14, w0, w13), "7b2d0394       ilvev.h  w14, w0, w13");
    COMPARE(ilvev_w(w12, w25, w22), "7b56cb14       ilvev.w  w12, w25, w22");
    COMPARE(ilvev_d(w30, w27, w3), "7b63df94       ilvev.d  w30, w27, w3");
    COMPARE(ilvl_b(w29, w3, w21), "7a151f54       ilvl.b  w29, w3, w21");
    COMPARE(ilvl_h(w27, w10, w17), "7a3156d4       ilvl.h  w27, w10, w17");
    COMPARE(ilvl_w(w6, w1, w0), "7a400994       ilvl.w  w6, w1, w0");
    COMPARE(ilvl_d(w3, w16, w24), "7a7880d4       ilvl.d  w3, w16, w24");
    COMPARE(ilvod_b(w11, w5, w20), "7b942ad4       ilvod.b  w11, w5, w20");
    COMPARE(ilvod_h(w18, w13, w31), "7bbf6c94       ilvod.h  w18, w13, w31");
    COMPARE(ilvod_w(w29, w16, w24), "7bd88754       ilvod.w  w29, w16, w24");
    COMPARE(ilvod_d(w22, w12, w29), "7bfd6594       ilvod.d  w22, w12, w29");
    COMPARE(ilvr_b(w4, w30, w6), "7a86f114       ilvr.b  w4, w30, w6");
    COMPARE(ilvr_h(w28, w19, w29), "7abd9f14       ilvr.h  w28, w19, w29");
    COMPARE(ilvr_w(w18, w20, w21), "7ad5a494       ilvr.w  w18, w20, w21");
    COMPARE(ilvr_d(w23, w30, w12), "7aecf5d4       ilvr.d  w23, w30, w12");
    COMPARE(maddv_b(w17, w31, w29), "789dfc52       maddv.b  w17, w31, w29");
    COMPARE(maddv_h(w7, w24, w9), "78a9c1d2       maddv.h  w7, w24, w9");
    COMPARE(maddv_w(w22, w22, w20), "78d4b592       maddv.w  w22, w22, w20");
    COMPARE(maddv_d(w30, w26, w20), "78f4d792       maddv.d  w30, w26, w20");
    COMPARE(max_a_b(w23, w11, w23), "7b175dce       max_a.b  w23, w11, w23");
    COMPARE(max_a_h(w20, w5, w30), "7b3e2d0e       max_a.h  w20, w5, w30");
    COMPARE(max_a_w(w7, w18, w30), "7b5e91ce       max_a.w  w7, w18, w30");
    COMPARE(max_a_d(w8, w8, w31), "7b7f420e       max_a.d  w8, w8, w31");
    COMPARE(max_s_b(w10, w1, w19), "79130a8e       max_s.b  w10, w1, w19");
    COMPARE(max_s_h(w15, w29, w17), "7931ebce       max_s.h  w15, w29, w17");
    COMPARE(max_s_w(w15, w29, w14), "794eebce       max_s.w  w15, w29, w14");
    COMPARE(max_s_d(w25, w24, w3), "7963c64e       max_s.d  w25, w24, w3");
    COMPARE(max_u_b(w12, w24, w5), "7985c30e       max_u.b  w12, w24, w5");
    COMPARE(max_u_h(w5, w6, w7), "79a7314e       max_u.h  w5, w6, w7");
    COMPARE(max_u_w(w16, w4, w7), "79c7240e       max_u.w  w16, w4, w7");
    COMPARE(max_u_d(w26, w12, w24), "79f8668e       max_u.d  w26, w12, w24");
    COMPARE(min_a_b(w4, w26, w1), "7b81d10e       min_a.b  w4, w26, w1");
    COMPARE(min_a_h(w12, w13, w31), "7bbf6b0e       min_a.h  w12, w13, w31");
    COMPARE(min_a_w(w28, w20, w0), "7bc0a70e       min_a.w  w28, w20, w0");
    COMPARE(min_a_d(w12, w20, w19), "7bf3a30e       min_a.d  w12, w20, w19");
    COMPARE(min_s_b(w19, w3, w14), "7a0e1cce       min_s.b  w19, w3, w14");
    COMPARE(min_s_h(w27, w21, w8), "7a28aece       min_s.h  w27, w21, w8");
    COMPARE(min_s_w(w0, w14, w30), "7a5e700e       min_s.w  w0, w14, w30");
    COMPARE(min_s_d(w6, w8, w21), "7a75418e       min_s.d  w6, w8, w21");
    COMPARE(min_u_b(w22, w26, w8), "7a88d58e       min_u.b  w22, w26, w8");
    COMPARE(min_u_h(w7, w27, w12), "7aacd9ce       min_u.h  w7, w27, w12");
    COMPARE(min_u_w(w8, w20, w14), "7acea20e       min_u.w  w8, w20, w14");
    COMPARE(min_u_d(w26, w14, w15), "7aef768e       min_u.d  w26, w14, w15");
    COMPARE(mod_s_b(w18, w1, w26), "7b1a0c92       mod_s.b  w18, w1, w26");
    COMPARE(mod_s_h(w31, w30, w28), "7b3cf7d2       mod_s.h  w31, w30, w28");
    COMPARE(mod_s_w(w2, w6, w13), "7b4d3092       mod_s.w  w2, w6, w13");
    COMPARE(mod_s_d(w21, w27, w22), "7b76dd52       mod_s.d  w21, w27, w22");
    COMPARE(mod_u_b(w16, w7, w13), "7b8d3c12       mod_u.b  w16, w7, w13");
    COMPARE(mod_u_h(w24, w8, w7), "7ba74612       mod_u.h  w24, w8, w7");
    COMPARE(mod_u_w(w30, w2, w17), "7bd11792       mod_u.w  w30, w2, w17");
    COMPARE(mod_u_d(w31, w2, w25), "7bf917d2       mod_u.d  w31, w2, w25");
    COMPARE(msubv_b(w14, w5, w12), "790c2b92       msubv.b  w14, w5, w12");
    COMPARE(msubv_h(w6, w7, w30), "793e3992       msubv.h  w6, w7, w30");
    COMPARE(msubv_w(w13, w2, w21), "79551352       msubv.w  w13, w2, w21");
    COMPARE(msubv_d(w16, w14, w27), "797b7412       msubv.d  w16, w14, w27");
    COMPARE(mulv_b(w20, w3, w13), "780d1d12       mulv.b  w20, w3, w13");
    COMPARE(mulv_h(w27, w26, w14), "782ed6d2       mulv.h  w27, w26, w14");
    COMPARE(mulv_w(w10, w29, w3), "7843ea92       mulv.w  w10, w29, w3");
    COMPARE(mulv_d(w7, w19, w29), "787d99d2       mulv.d  w7, w19, w29");
    COMPARE(pckev_b(w5, w27, w7), "7907d954       pckev.b  w5, w27, w7");
    COMPARE(pckev_h(w1, w4, w27), "793b2054       pckev.h  w1, w4, w27");
    COMPARE(pckev_w(w30, w20, w0), "7940a794       pckev.w  w30, w20, w0");
    COMPARE(pckev_d(w6, w1, w15), "796f0994       pckev.d  w6, w1, w15");
    COMPARE(pckod_b(w18, w28, w30), "799ee494       pckod.b  w18, w28, w30");
    COMPARE(pckod_h(w26, w5, w8), "79a82e94       pckod.h  w26, w5, w8");
    COMPARE(pckod_w(w9, w4, w2), "79c22254       pckod.w  w9, w4, w2");
    COMPARE(pckod_d(w30, w22, w20), "79f4b794       pckod.d  w30, w22, w20");
    COMPARE(sld_b(w5, w23, t4), "780cb954       sld.b  w5, w23[t4]");
    COMPARE(sld_h(w1, w23, v1), "7823b854       sld.h  w1, w23[v1]");
    COMPARE(sld_w(w20, w8, t1), "78494514       sld.w  w20, w8[t1]");
    COMPARE(sld_d(w7, w23, fp), "787eb9d4       sld.d  w7, w23[fp]");
    COMPARE(sll_b(w3, w0, w17), "781100cd       sll.b  w3, w0, w17");
    COMPARE(sll_h(w17, w27, w3), "7823dc4d       sll.h  w17, w27, w3");
    COMPARE(sll_w(w16, w7, w6), "78463c0d       sll.w  w16, w7, w6");
    COMPARE(sll_d(w9, w0, w26), "787a024d       sll.d  w9, w0, w26");
    COMPARE(splat_b(w28, w1, at), "78810f14       splat.b  w28, w1[at]");
    COMPARE(splat_h(w2, w11, t3), "78ab5894       splat.h  w2, w11[t3]");
    COMPARE(splat_w(w22, w0, t3), "78cb0594       splat.w  w22, w0[t3]");
    COMPARE(splat_d(w0, w0, v0), "78e20014       splat.d  w0, w0[v0]");
    COMPARE(sra_b(w28, w4, w17), "7891270d       sra.b  w28, w4, w17");
    COMPARE(sra_h(w13, w9, w3), "78a34b4d       sra.h  w13, w9, w3");
    COMPARE(sra_w(w27, w21, w19), "78d3aecd       sra.w  w27, w21, w19");
    COMPARE(sra_d(w30, w8, w23), "78f7478d       sra.d  w30, w8, w23");
    COMPARE(srar_b(w19, w18, w18), "789294d5       srar.b  w19, w18, w18");
    COMPARE(srar_h(w7, w23, w8), "78a8b9d5       srar.h  w7, w23, w8");
    COMPARE(srar_w(w1, w12, w2), "78c26055       srar.w  w1, w12, w2");
    COMPARE(srar_d(w21, w7, w14), "78ee3d55       srar.d  w21, w7, w14");
    COMPARE(srl_b(w12, w3, w19), "79131b0d       srl.b  w12, w3, w19");
    COMPARE(srl_h(w23, w31, w20), "7934fdcd       srl.h  w23, w31, w20");
    COMPARE(srl_w(w18, w27, w11), "794bdc8d       srl.w  w18, w27, w11");
    COMPARE(srl_d(w3, w12, w26), "797a60cd       srl.d  w3, w12, w26");
    COMPARE(srlr_b(w15, w21, w11), "790babd5       srlr.b  w15, w21, w11");
    COMPARE(srlr_h(w21, w13, w19), "79336d55       srlr.h  w21, w13, w19");
    COMPARE(srlr_w(w6, w30, w3), "7943f195       srlr.w  w6, w30, w3");
    COMPARE(srlr_d(w1, w2, w14), "796e1055       srlr.d  w1, w2, w14");
    COMPARE(subs_s_b(w25, w15, w1), "78017e51       subs_s.b  w25, w15, w1");
    COMPARE(subs_s_h(w28, w25, w22), "7836cf11       subs_s.h  w28, w25, w22");
    COMPARE(subs_s_w(w10, w12, w21), "78556291       subs_s.w  w10, w12, w21");
    COMPARE(subs_s_d(w4, w20, w18), "7872a111       subs_s.d  w4, w20, w18");
    COMPARE(subs_u_b(w21, w6, w25), "78993551       subs_u.b  w21, w6, w25");
    COMPARE(subs_u_h(w3, w10, w7), "78a750d1       subs_u.h  w3, w10, w7");
    COMPARE(subs_u_w(w9, w15, w10), "78ca7a51       subs_u.w  w9, w15, w10");
    COMPARE(subs_u_d(w7, w19, w10), "78ea99d1       subs_u.d  w7, w19, w10");
    COMPARE(subsus_u_b(w6, w7, w12), "790c3991       subsus_u.b  w6, w7, w12");
    COMPARE(subsus_u_h(w6, w29, w19),
            "7933e991       subsus_u.h  w6, w29, w19");
    COMPARE(subsus_u_w(w7, w15, w7), "794779d1       subsus_u.w  w7, w15, w7");
    COMPARE(subsus_u_d(w9, w3, w15), "796f1a51       subsus_u.d  w9, w3, w15");
    COMPARE(subsuu_s_b(w22, w3, w31),
            "799f1d91       subsuu_s.b  w22, w3, w31");
    COMPARE(subsuu_s_h(w19, w23, w22),
            "79b6bcd1       subsuu_s.h  w19, w23, w22");
    COMPARE(subsuu_s_w(w9, w10, w13),
            "79cd5251       subsuu_s.w  w9, w10, w13");
    COMPARE(subsuu_s_d(w5, w6, w0), "79e03151       subsuu_s.d  w5, w6, w0");
    COMPARE(subv_b(w6, w13, w19), "7893698e       subv.b  w6, w13, w19");
    COMPARE(subv_h(w4, w25, w12), "78acc90e       subv.h  w4, w25, w12");
    COMPARE(subv_w(w27, w27, w11), "78cbdece       subv.w  w27, w27, w11");
    COMPARE(subv_d(w9, w24, w10), "78eac24e       subv.d  w9, w24, w10");
    COMPARE(vshf_b(w3, w16, w5), "780580d5       vshf.b  w3, w16, w5");
    COMPARE(vshf_h(w20, w19, w8), "78289d15       vshf.h  w20, w19, w8");
    COMPARE(vshf_w(w16, w30, w25), "7859f415       vshf.w  w16, w30, w25");
    COMPARE(vshf_d(w19, w11, w15), "786f5cd5       vshf.d  w19, w11, w15");
  }
  VERIFY_RUN();
}

TEST(MSA_3RF) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(fadd_w(w28, w19, w28), "781c9f1b       fadd.w  w28, w19, w28");
    COMPARE(fadd_d(w13, w2, w29), "783d135b       fadd.d  w13, w2, w29");
    COMPARE(fcaf_w(w14, w11, w25), "78195b9a       fcaf.w  w14, w11, w25");
    COMPARE(fcaf_d(w1, w1, w19), "7833085a       fcaf.d  w1, w1, w19");
    COMPARE(fceq_w(w1, w23, w16), "7890b85a       fceq.w  w1, w23, w16");
    COMPARE(fceq_d(w0, w8, w16), "78b0401a       fceq.d  w0, w8, w16");
    COMPARE(fcle_w(w16, w9, w24), "79984c1a       fcle.w  w16, w9, w24");
    COMPARE(fcle_d(w27, w14, w1), "79a176da       fcle.d  w27, w14, w1");
    COMPARE(fclt_w(w28, w8, w8), "7908471a       fclt.w  w28, w8, w8");
    COMPARE(fclt_d(w30, w25, w11), "792bcf9a       fclt.d  w30, w25, w11");
    COMPARE(fcne_w(w2, w18, w23), "78d7909c       fcne.w  w2, w18, w23");
    COMPARE(fcne_d(w14, w20, w15), "78efa39c       fcne.d  w14, w20, w15");
    COMPARE(fcor_w(w10, w18, w25), "7859929c       fcor.w  w10, w18, w25");
    COMPARE(fcor_d(w17, w25, w11), "786bcc5c       fcor.d  w17, w25, w11");
    COMPARE(fcueq_w(w14, w2, w21), "78d5139a       fcueq.w  w14, w2, w21");
    COMPARE(fcueq_d(w29, w3, w7), "78e71f5a       fcueq.d  w29, w3, w7");
    COMPARE(fcule_w(w17, w5, w3), "79c32c5a       fcule.w  w17, w5, w3");
    COMPARE(fcule_d(w31, w1, w30), "79fe0fda       fcule.d  w31, w1, w30");
    COMPARE(fcult_w(w6, w25, w9), "7949c99a       fcult.w  w6, w25, w9");
    COMPARE(fcult_d(w27, w8, w17), "797146da       fcult.d  w27, w8, w17");
    COMPARE(fcun_w(w4, w20, w8), "7848a11a       fcun.w  w4, w20, w8");
    COMPARE(fcun_d(w29, w11, w3), "78635f5a       fcun.d  w29, w11, w3");
    COMPARE(fcune_w(w13, w18, w19), "7893935c       fcune.w  w13, w18, w19");
    COMPARE(fcune_d(w16, w26, w21), "78b5d41c       fcune.d  w16, w26, w21");
    COMPARE(fdiv_w(w13, w24, w2), "78c2c35b       fdiv.w  w13, w24, w2");
    COMPARE(fdiv_d(w19, w4, w25), "78f924db       fdiv.d  w19, w4, w25");
    COMPARE(fexdo_h(w8, w0, w16), "7a10021b       fexdo.h  w8, w0, w16");
    COMPARE(fexdo_w(w0, w13, w27), "7a3b681b       fexdo.w  w0, w13, w27");
    COMPARE(fexp2_w(w17, w0, w3), "79c3045b       fexp2.w  w17, w0, w3");
    COMPARE(fexp2_d(w22, w0, w10), "79ea059b       fexp2.d  w22, w0, w10");
    COMPARE(fmadd_w(w29, w6, w23), "7917375b       fmadd.w  w29, w6, w23");
    COMPARE(fmadd_d(w11, w28, w21), "7935e2db       fmadd.d  w11, w28, w21");
    COMPARE(fmax_w(w0, w23, w13), "7b8db81b       fmax.w  w0, w23, w13");
    COMPARE(fmax_d(w26, w18, w8), "7ba8969b       fmax.d  w26, w18, w8");
    COMPARE(fmax_a_w(w10, w16, w10), "7bca829b       fmax_a.w  w10, w16, w10");
    COMPARE(fmax_a_d(w30, w9, w22), "7bf64f9b       fmax_a.d  w30, w9, w22");
    COMPARE(fmin_w(w24, w1, w30), "7b1e0e1b       fmin.w  w24, w1, w30");
    COMPARE(fmin_d(w27, w27, w10), "7b2adedb       fmin.d  w27, w27, w10");
    COMPARE(fmin_a_w(w10, w29, w20), "7b54ea9b       fmin_a.w  w10, w29, w20");
    COMPARE(fmin_a_d(w13, w30, w24), "7b78f35b       fmin_a.d  w13, w30, w24");
    COMPARE(fmsub_w(w17, w25, w0), "7940cc5b       fmsub.w  w17, w25, w0");
    COMPARE(fmsub_d(w8, w18, w16), "7970921b       fmsub.d  w8, w18, w16");
    COMPARE(fmul_w(w3, w15, w15), "788f78db       fmul.w  w3, w15, w15");
    COMPARE(fmul_d(w9, w30, w10), "78aaf25b       fmul.d  w9, w30, w10");
    COMPARE(fsaf_w(w25, w5, w10), "7a0a2e5a       fsaf.w  w25, w5, w10");
    COMPARE(fsaf_d(w25, w3, w29), "7a3d1e5a       fsaf.d  w25, w3, w29");
    COMPARE(fseq_w(w11, w17, w13), "7a8d8ada       fseq.w  w11, w17, w13");
    COMPARE(fseq_d(w29, w0, w31), "7abf075a       fseq.d  w29, w0, w31");
    COMPARE(fsle_w(w30, w31, w31), "7b9fff9a       fsle.w  w30, w31, w31");
    COMPARE(fsle_d(w18, w23, w24), "7bb8bc9a       fsle.d  w18, w23, w24");
    COMPARE(fslt_w(w12, w5, w6), "7b062b1a       fslt.w  w12, w5, w6");
    COMPARE(fslt_d(w16, w26, w21), "7b35d41a       fslt.d  w16, w26, w21");
    COMPARE(fsne_w(w30, w1, w12), "7acc0f9c       fsne.w  w30, w1, w12");
    COMPARE(fsne_d(w14, w13, w23), "7af76b9c       fsne.d  w14, w13, w23");
    COMPARE(fsor_w(w27, w13, w27), "7a5b6edc       fsor.w  w27, w13, w27");
    COMPARE(fsor_d(w12, w24, w11), "7a6bc31c       fsor.d  w12, w24, w11");
    COMPARE(fsub_w(w31, w26, w1), "7841d7db       fsub.w  w31, w26, w1");
    COMPARE(fsub_d(w19, w17, w27), "787b8cdb       fsub.d  w19, w17, w27");
    COMPARE(fsueq_w(w16, w24, w25), "7ad9c41a       fsueq.w  w16, w24, w25");
    COMPARE(fsueq_d(w18, w14, w14), "7aee749a       fsueq.d  w18, w14, w14");
    COMPARE(fsule_w(w23, w30, w13), "7bcdf5da       fsule.w  w23, w30, w13");
    COMPARE(fsule_d(w2, w11, w26), "7bfa589a       fsule.d  w2, w11, w26");
    COMPARE(fsult_w(w11, w26, w22), "7b56d2da       fsult.w  w11, w26, w22");
    COMPARE(fsult_d(w6, w23, w30), "7b7eb99a       fsult.d  w6, w23, w30");
    COMPARE(fsun_w(w3, w18, w28), "7a5c90da       fsun.w  w3, w18, w28");
    COMPARE(fsun_d(w18, w11, w19), "7a735c9a       fsun.d  w18, w11, w19");
    COMPARE(fsune_w(w16, w31, w2), "7a82fc1c       fsune.w  w16, w31, w2");
    COMPARE(fsune_d(w3, w26, w17), "7ab1d0dc       fsune.d  w3, w26, w17");
    COMPARE(ftq_h(w16, w4, w24), "7a98241b       ftq.h  w16, w4, w24");
    COMPARE(ftq_w(w5, w5, w25), "7ab9295b       ftq.w  w5, w5, w25");
    COMPARE(madd_q_h(w16, w20, w10), "794aa41c       madd_q.h  w16, w20, w10");
    COMPARE(madd_q_w(w28, w2, w9), "7969171c       madd_q.w  w28, w2, w9");
    COMPARE(maddr_q_h(w8, w18, w9), "7b49921c       maddr_q.h  w8, w18, w9");
    COMPARE(maddr_q_w(w29, w12, w16),
            "7b70675c       maddr_q.w  w29, w12, w16");
    COMPARE(msub_q_h(w24, w26, w10), "798ad61c       msub_q.h  w24, w26, w10");
    COMPARE(msub_q_w(w13, w30, w28), "79bcf35c       msub_q.w  w13, w30, w28");
    COMPARE(msubr_q_h(w12, w21, w11),
            "7b8bab1c       msubr_q.h  w12, w21, w11");
    COMPARE(msubr_q_w(w1, w14, w20), "7bb4705c       msubr_q.w  w1, w14, w20");
    COMPARE(mul_q_h(w6, w16, w30), "791e819c       mul_q.h  w6, w16, w30");
    COMPARE(mul_q_w(w16, w1, w4), "79240c1c       mul_q.w  w16, w1, w4");
    COMPARE(mulr_q_h(w6, w20, w19), "7b13a19c       mulr_q.h  w6, w20, w19");
    COMPARE(mulr_q_w(w27, w1, w20), "7b340edc       mulr_q.w  w27, w1, w20");
  }
  VERIFY_RUN();
}

TEST(MSA_ELM) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(copy_s_b(t5, w8, 2), "78824359       copy_s.b  t5, w8[2]");
    COMPARE(copy_s_h(at, w25, 0), "78a0c859       copy_s.h  at, w25[0]");
    COMPARE(copy_s_w(s6, w5, 1), "78b12d99       copy_s.w  s6, w5[1]");
    COMPARE(copy_u_b(s6, w20, 4), "78c4a599       copy_u.b  s6, w20[4]");
    COMPARE(copy_u_h(s4, w4, 0), "78e02519       copy_u.h  s4, w4[0]");
    COMPARE(sldi_b(w0, w29, 4), "7804e819       sldi.b  w0, w29[4]");
    COMPARE(sldi_h(w8, w17, 0), "78208a19       sldi.h  w8, w17[0]");
    COMPARE(sldi_w(w20, w27, 2), "7832dd19       sldi.w  w20, w27[2]");
    COMPARE(sldi_d(w4, w12, 0), "78386119       sldi.d  w4, w12[0]");
    COMPARE(splati_b(w25, w3, 2), "78421e59       splati.b  w25, w3[2]");
    COMPARE(splati_h(w24, w28, 1), "7861e619       splati.h  w24, w28[1]");
    COMPARE(splati_w(w13, w18, 0), "78709359       splati.w  w13, w18[0]");
    COMPARE(splati_d(w28, w1, 0), "78780f19       splati.d  w28, w1[0]");
    COMPARE(move_v(w23, w24), "78bec5d9       move.v  w23, w24");
    COMPARE(insert_b(w23, 3, sp), "7903edd9       insert.b  w23[3], sp");
    COMPARE(insert_h(w20, 2, a1), "79222d19       insert.h  w20[2], a1");
    COMPARE(insert_w(w8, 2, s0), "79328219       insert.w  w8[2], s0");
    COMPARE(insve_b(w25, 3, w9), "79434e59       insve.b  w25[3], w9[0]");
    COMPARE(insve_h(w24, 2, w2), "79621619       insve.h  w24[2], w2[0]");
    COMPARE(insve_w(w0, 2, w13), "79726819       insve.w  w0[2], w13[0]");
    COMPARE(insve_d(w3, 0, w18), "797890d9       insve.d  w3[0], w18[0]");
    COMPARE(cfcmsa(at, MSAIR), "787e0059       cfcmsa  at, MSAIR");
    COMPARE(cfcmsa(v0, MSACSR), "787e0899       cfcmsa  v0, MSACSR");
    COMPARE(ctcmsa(MSAIR, at), "783e0819       ctcmsa  MSAIR, at");
    COMPARE(ctcmsa(MSACSR, v0), "783e1059       ctcmsa  MSACSR, v0");
  }
  VERIFY_RUN();
}

TEST(MSA_BIT) {
  SET_UP();
  if (IsMipsArchVariant(kMips32r6) && CpuFeatures::IsSupported(MIPS_SIMD)) {
    CpuFeatureScope fscope(&assm, MIPS_SIMD);

    COMPARE(bclri_b(w21, w30, 2), "79f2f549       bclri.b  w21, w30, 2");
    COMPARE(bclri_h(w24, w21, 0), "79e0ae09       bclri.h  w24, w21, 0");
    COMPARE(bclri_w(w23, w30, 3), "79c3f5c9       bclri.w  w23, w30, 3");
    COMPARE(bclri_d(w9, w11, 0), "79805a49       bclri.d  w9, w11, 0");
    COMPARE(binsli_b(w25, w12, 1), "7b716649       binsli.b  w25, w12, 1");
    COMPARE(binsli_h(w21, w22, 0), "7b60b549       binsli.h  w21, w22, 0");
    COMPARE(binsli_w(w22, w4, 0), "7b402589       binsli.w  w22, w4, 0");
    COMPARE(binsli_d(w6, w2, 6), "7b061189       binsli.d  w6, w2, 6");
    COMPARE(binsri_b(w15, w19, 0), "7bf09bc9       binsri.b  w15, w19, 0");
    COMPARE(binsri_h(w8, w30, 1), "7be1f209       binsri.h  w8, w30, 1");
    COMPARE(binsri_w(w2, w19, 5), "7bc59889       binsri.w  w2, w19, 5");
    COMPARE(binsri_d(w18, w20, 1), "7b81a489       binsri.d  w18, w20, 1");
    COMPARE(bnegi_b(w24, w19, 0), "7af09e09       bnegi.b  w24, w19, 0");
    COMPARE(bnegi_h(w28, w11, 3), "7ae35f09       bnegi.h  w28, w11, 3");
    COMPARE(bnegi_w(w1, w27, 5), "7ac5d849       bnegi.w  w1, w27, 5");
    COMPARE(bnegi_d(w4, w21, 1), "7a81a909       bnegi.d  w4, w21, 1");
    COMPARE(bseti_b(w18, w8, 0), "7a704489       bseti.b  w18, w8, 0");
    COMPARE(bseti_h(w24, w14, 2), "7a627609       bseti.h  w24, w14, 2");
    COMPARE(bseti_w(w9, w18, 4), "7a449249       bseti.w  w9, w18, 4");
    COMPARE(bseti_d(w7, w15, 1), "7a0179c9       bseti.d  w7, w15, 1");
    COMPARE(sat_s_b(w31, w31, 2), "7872ffca       sat_s.b  w31, w31, 2");
    COMPARE(sat_s_h(w19, w19, 0), "78609cca       sat_s.h  w19, w19, 0");
    COMPARE(sat_s_w(w19, w29, 0), "7840ecca       sat_s.w  w19, w29, 0");
    COMPARE(sat_s_d(w11, w22, 0), "7800b2ca       sat_s.d  w11, w22, 0");
    COMPARE(sat_u_b(w1, w13, 3), "78f3684a       sat_u.b  w1, w13, 3");
    COMPARE(sat_u_h(w30, w24, 4), "78e4c78a       sat_u.h  w30, w24, 4");
    COMPARE(sat_u_w(w31, w13, 0), "78c06fca       sat_u.w  w31, w13, 0");
    COMPARE(sat_u_d(w29, w16, 5), "7885874a       sat_u.d  w29, w16, 5");
    COMPARE(slli_b(w23, w10, 1), "787155c9       slli.b  w23, w10, 1");
    COMPARE(slli_h(w9, w18, 1), "78619249       slli.h  w9, w18, 1");
    COMPARE(slli_w(w11, w29, 4), "7844eac9       slli.w  w11, w29, 4");
    COMPARE(slli_d(w25, w20, 1), "7801a649       slli.d  w25, w20, 1");
    COMPARE(srai_b(w24, w29, 1), "78f1ee09       srai.b  w24, w29, 1");
    COMPARE(srai_h(w1, w6, 0), "78e03049       srai.h  w1, w6, 0");
    COMPARE(srai_w(w7, w26, 1), "78c1d1c9       srai.w  w7, w26, 1");
    COMPARE(srai_d(w20, w25, 3), "7883cd09       srai.d  w20, w25, 3");
    COMPARE(srari_b(w5, w25, 0), "7970c94a       srari.b  w5, w25, 0");
    COMPARE(srari_h(w7, w6, 4), "796431ca       srari.h  w7, w6, 4");
    COMPARE(srari_w(w17, w11, 5), "79455c4a       srari.w  w17, w11, 5");
    COMPARE(srari_d(w21, w25, 5), "7905cd4a       srari.d  w21, w25, 5");
    COMPARE(srli_b(w2, w0, 2), "79720089       srli.b  w2, w0, 2");
    COMPARE(srli_h(w31, w31, 2), "7962ffc9       srli.h  w31, w31, 2");
    COMPARE(srli_w(w5, w9, 4), "79444949       srli.w  w5, w9, 4");
    COMPARE(srli_d(w27, w26, 5), "7905d6c9       srli.d  w27, w26, 5");
    COMPARE(srlri_b(w18, w3, 0), "79f01c8a       srlri.b  w18, w3, 0");
    COMPARE(srlri_h(w1, w2, 3), "79e3104a       srlri.h  w1, w2, 3");
    COMPARE(srlri_w(w11, w22, 2), "79c2b2ca       srlri.w  w11, w22, 2");
    COMPARE(srlri_d(w24, w10, 6), "7986560a       srlri.d  w24, w10, 6");
  }
  VERIFY_RUN();
}

}  // namespace internal
}  // namespace v8
