// Copyright 2021 the V8 project authors. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>

#include "src/codegen/macro-assembler.h"
#include "src/debug/debug.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/execution/frames-inl.h"
#include "src/init/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using DisasmLoong64Test = TestWithIsolate;

bool DisassembleAndCompare(uint8_t* pc, const char* compare_string) {
  disasm::NameConverter converter;
  disasm::Disassembler disasm(converter);
  base::EmbeddedVector<char, 128> disasm_buffer;

  /*  if (prev_instr_compact_branch) {
      disasm.InstructionDecode(disasm_buffer, pc);
      pc += 4;
    }*/

  disasm.InstructionDecode(disasm_buffer, pc);

  if (strcmp(compare_string, disasm_buffer.begin()) != 0) {
    fprintf(stderr,
            "expected: \n"
            "%s\n"
            "disassembled: \n"
            "%s\n\n",
            compare_string, disasm_buffer.begin());
    return false;
  }
  return true;
}

// Set up V8 to a state where we can at least run the assembler and
// disassembler. Declare the variables and allocate the data structures used
// in the rest of the macros.
#define SET_UP()                                                  \
  HandleScope scope(isolate());                                   \
  uint8_t* buffer = reinterpret_cast<uint8_t*>(malloc(4 * 1024)); \
  Assembler assm(AssemblerOptions{},                              \
                 ExternalAssemblerBuffer(buffer, 4 * 1024));      \
  bool failure = false;

// This macro assembles one instruction using the preallocated assembler and
// disassembles the generated instruction, comparing the output to the expected
// value. If the comparison fails an error message is printed, but the test
// continues to run until the end.
#define COMPARE(asm_, compare_string)                                        \
  {                                                                          \
    int pc_offset = assm.pc_offset();                                        \
    uint8_t* progcounter = &buffer[pc_offset];                               \
    assm.asm_;                                                               \
    if (!DisassembleAndCompare(progcounter, compare_string)) failure = true; \
  }

// Verify that all invocations of the COMPARE macro passed successfully.
// Exit with a failure if at least one of the tests failed.
#define VERIFY_RUN()                               \
  if (failure) {                                   \
    FATAL("LOONG64 Disassembler tests failed.\n"); \
  }

#define COMPARE_PC_REL(asm_, compare_string, offset)                           \
  {                                                                            \
    int pc_offset = assm.pc_offset();                                          \
    uint8_t* progcounter = &buffer[pc_offset];                                 \
    char str_with_address[100];                                                \
    printf("%p\n", static_cast<void*>(progcounter));                           \
    snprintf(str_with_address, sizeof(str_with_address), "%s -> %p",           \
             compare_string, static_cast<void*>(progcounter + (offset * 4)));  \
    assm.asm_;                                                                 \
    if (!DisassembleAndCompare(progcounter, str_with_address)) failure = true; \
  }

TEST_F(DisasmLoong64Test, TypeOp6) {
  SET_UP();

  COMPARE(jirl(ra, t7, 0), "4c000261       jirl         ra, t7, 0x0");
  COMPARE(jirl(ra, t7, 32767), "4dfffe61       jirl         ra, t7, 0x1fffc");
  COMPARE(jirl(ra, t7, -32768), "4e000261       jirl         ra, t7, 0x20000");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp6PC) {
  SET_UP();

  COMPARE_PC_REL(beqz(t7, 1048575), "43fffe6f       beqz         t7, 0x3ffffc",
                 1048575);
  COMPARE_PC_REL(beqz(t0, -1048576), "40000190       beqz         t0, 0x400000",
                 -1048576);
  COMPARE_PC_REL(beqz(t1, 0), "400001a0       beqz         t1, 0x0", 0);

  COMPARE_PC_REL(bnez(a2, 1048575), "47fffccf       bnez         a2, 0x3ffffc",
                 1048575);
  COMPARE_PC_REL(bnez(s3, -1048576), "44000350       bnez         s3, 0x400000",
                 -1048576);
  COMPARE_PC_REL(bnez(t8, 0), "44000280       bnez         t8, 0x0", 0);

  COMPARE_PC_REL(bceqz(FCC0, 1048575),
                 "4bfffc0f       bceqz        fcc0, 0x3ffffc", 1048575);
  COMPARE_PC_REL(bceqz(FCC0, -1048576),
                 "48000010       bceqz        fcc0, 0x400000", -1048576);
  COMPARE_PC_REL(bceqz(FCC0, 0), "48000000       bceqz        fcc0, 0x0", 0);

  COMPARE_PC_REL(bcnez(FCC0, 1048575),
                 "4bfffd0f       bcnez        fcc0, 0x3ffffc", 1048575);
  COMPARE_PC_REL(bcnez(FCC0, -1048576),
                 "48000110       bcnez        fcc0, 0x400000", -1048576);
  COMPARE_PC_REL(bcnez(FCC0, 0), "48000100       bcnez        fcc0, 0x0", 0);

  COMPARE_PC_REL(b(33554431), "53fffdff       b            0x7fffffc",
                 33554431);
  COMPARE_PC_REL(b(-33554432), "50000200       b            0x8000000",
                 -33554432);
  COMPARE_PC_REL(b(0), "50000000       b            0x0", 0);

  COMPARE_PC_REL(beq(t0, a6, 32767),
                 "59fffd8a       beq          t0, a6, 0x1fffc", 32767);
  COMPARE_PC_REL(beq(t1, a0, -32768),
                 "5a0001a4       beq          t1, a0, 0x20000", -32768);
  COMPARE_PC_REL(beq(a4, t1, 0), "5800010d       beq          a4, t1, 0x0", 0);

  COMPARE_PC_REL(bne(a3, a4, 32767),
                 "5dfffce8       bne          a3, a4, 0x1fffc", 32767);
  COMPARE_PC_REL(bne(a6, a5, -32768),
                 "5e000149       bne          a6, a5, 0x20000", -32768);
  COMPARE_PC_REL(bne(a4, a5, 0), "5c000109       bne          a4, a5, 0x0", 0);

  COMPARE_PC_REL(blt(a4, a6, 32767),
                 "61fffd0a       blt          a4, a6, 0x1fffc", 32767);
  COMPARE_PC_REL(blt(a4, a5, -32768),
                 "62000109       blt          a4, a5, 0x20000", -32768);
  COMPARE_PC_REL(blt(a4, a6, 0), "6000010a       blt          a4, a6, 0x0", 0);

  COMPARE_PC_REL(bge(s7, a5, 32767),
                 "65ffffc9       bge          s7, a5, 0x1fffc", 32767);
  COMPARE_PC_REL(bge(a1, a3, -32768),
                 "660000a7       bge          a1, a3, 0x20000", -32768);
  COMPARE_PC_REL(bge(a5, s3, 0), "6400013a       bge          a5, s3, 0x0", 0);

  COMPARE_PC_REL(bltu(a5, s7, 32767),
                 "69fffd3e       bltu         a5, s7, 0x1fffc", 32767);
  COMPARE_PC_REL(bltu(a4, a5, -32768),
                 "6a000109       bltu         a4, a5, 0x20000", -32768);
  COMPARE_PC_REL(bltu(a4, t6, 0), "68000112       bltu         a4, t6, 0x0", 0);

  COMPARE_PC_REL(bgeu(a7, a6, 32767),
                 "6dfffd6a       bgeu         a7, a6, 0x1fffc", 32767);
  COMPARE_PC_REL(bgeu(a5, a3, -32768),
                 "6e000127       bgeu         a5, a3, 0x20000", -32768);
  COMPARE_PC_REL(bgeu(t2, t1, 0), "6c0001cd       bgeu         t2, t1, 0x0", 0);

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp7) {
  SET_UP();

  COMPARE(lu12i_w(a4, 524287), "14ffffe8       lu12i.w      a4, 0x7ffff");
  COMPARE(lu12i_w(a5, -524288), "15000009       lu12i.w      a5, 0x80000");
  COMPARE(lu12i_w(a6, 0), "1400000a       lu12i.w      a6, 0x0");

  COMPARE(lu32i_d(a7, 524287), "16ffffeb       lu32i.d      a7, 0x7ffff");
  COMPARE(lu32i_d(t0, -524288), "1700000c       lu32i.d      t0, 0x80000");
  COMPARE(lu32i_d(t1, 0), "1600000d       lu32i.d      t1, 0x0");

  COMPARE(pcaddi(t1, 1), "1800002d       pcaddi       t1, 0x1");
  COMPARE(pcaddi(t2, 524287), "18ffffee       pcaddi       t2, 0x7ffff");
  COMPARE(pcaddi(t3, -524288), "1900000f       pcaddi       t3, 0x80000");
  COMPARE(pcaddi(t4, 0), "18000010       pcaddi       t4, 0x0");

  COMPARE(pcalau12i(t5, 524287), "1afffff1       pcalau12i    t5, 0x7ffff");
  COMPARE(pcalau12i(t6, -524288), "1b000012       pcalau12i    t6, 0x80000");
  COMPARE(pcalau12i(a4, 0), "1a000008       pcalau12i    a4, 0x0");

  COMPARE(pcaddu12i(a5, 524287), "1cffffe9       pcaddu12i    a5, 0x7ffff");
  COMPARE(pcaddu12i(a6, -524288), "1d00000a       pcaddu12i    a6, 0x80000");
  COMPARE(pcaddu12i(a7, 0), "1c00000b       pcaddu12i    a7, 0x0");

  COMPARE(pcaddu18i(t0, 524287), "1effffec       pcaddu18i    t0, 0x7ffff");
  COMPARE(pcaddu18i(t1, -524288), "1f00000d       pcaddu18i    t1, 0x80000");
  COMPARE(pcaddu18i(t2, 0), "1e00000e       pcaddu18i    t2, 0x0");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp8) {
  SET_UP();

  COMPARE(ll_w(t2, t3, 32764),
          "207ffdee       ll.w         t2, t3, 32764(0x7ffc)");
  COMPARE(ll_w(t3, t4, -32768),
          "2080020f       ll.w         t3, t4, -32768(0x8000)");
  COMPARE(ll_w(t5, t6, 0), "20000251       ll.w         t5, t6, 0(0x0)");

  COMPARE(sc_w(a6, a7, 32764),
          "217ffd6a       sc.w         a6, a7, 32764(0x7ffc)");
  COMPARE(sc_w(t0, t1, -32768),
          "218001ac       sc.w         t0, t1, -32768(0x8000)");
  COMPARE(sc_w(t2, t3, 0), "210001ee       sc.w         t2, t3, 0(0x0)");

  COMPARE(ll_d(a0, a1, 32764),
          "227ffca4       ll.d         a0, a1, 32764(0x7ffc)");
  COMPARE(ll_d(a2, a3, -32768),
          "228000e6       ll.d         a2, a3, -32768(0x8000)");
  COMPARE(ll_d(a4, a5, 0), "22000128       ll.d         a4, a5, 0(0x0)");

  COMPARE(sc_d(t4, t5, 32764),
          "237ffe30       sc.d         t4, t5, 32764(0x7ffc)");
  COMPARE(sc_d(t6, a0, -32768),
          "23800092       sc.d         t6, a0, -32768(0x8000)");
  COMPARE(sc_d(a1, a2, 0), "230000c5       sc.d         a1, a2, 0(0x0)");

  COMPARE(ldptr_w(a4, a5, 32764),
          "247ffd28       ldptr.w      a4, a5, 32764(0x7ffc)");
  COMPARE(ldptr_w(a6, a7, -32768),
          "2480016a       ldptr.w      a6, a7, -32768(0x8000)");
  COMPARE(ldptr_w(t0, t1, 0), "240001ac       ldptr.w      t0, t1, 0(0x0)");

  COMPARE(stptr_w(a4, a5, 32764),
          "257ffd28       stptr.w      a4, a5, 32764(0x7ffc)");
  COMPARE(stptr_w(a6, a7, -32768),
          "2580016a       stptr.w      a6, a7, -32768(0x8000)");
  COMPARE(stptr_w(t0, t1, 0), "250001ac       stptr.w      t0, t1, 0(0x0)");

  COMPARE(ldptr_d(t2, t3, 32764),
          "267ffdee       ldptr.d      t2, t3, 32764(0x7ffc)");
  COMPARE(ldptr_d(t4, t5, -32768),
          "26800230       ldptr.d      t4, t5, -32768(0x8000)");
  COMPARE(ldptr_d(t6, a4, 0), "26000112       ldptr.d      t6, a4, 0(0x0)");

  COMPARE(stptr_d(a5, a6, 32764),
          "277ffd49       stptr.d      a5, a6, 32764(0x7ffc)");
  COMPARE(stptr_d(a7, t0, -32768),
          "2780018b       stptr.d      a7, t0, -32768(0x8000)");
  COMPARE(stptr_d(t1, t2, 0), "270001cd       stptr.d      t1, t2, 0(0x0)");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp10) {
  SET_UP();

  COMPARE(bstrins_w(a4, a5, 31, 16),
          "007f4128       bstrins.w    a4, a5, 31, 16");
  COMPARE(bstrins_w(a6, a7, 5, 0), "0065016a       bstrins.w    a6, a7, 5, 0");

  COMPARE(bstrins_d(a3, zero_reg, 17, 0),
          "00910007       bstrins.d    a3, zero_reg, 17, 0");
  COMPARE(bstrins_d(t1, zero_reg, 17, 0),
          "0091000d       bstrins.d    t1, zero_reg, 17, 0");

  COMPARE(bstrpick_w(t0, t1, 31, 29),
          "007ff5ac       bstrpick.w   t0, t1, 31, 29");
  COMPARE(bstrpick_w(a4, a5, 16, 0),
          "00708128       bstrpick.w   a4, a5, 16, 0");

  COMPARE(bstrpick_d(a5, a5, 31, 0),
          "00df0129       bstrpick.d   a5, a5, 31, 0");
  COMPARE(bstrpick_d(a4, a4, 25, 2),
          "00d90908       bstrpick.d   a4, a4, 25, 2");

  COMPARE(slti(t2, a5, 2047),
          "021ffd2e       slti         t2, a5, 2047(0x7ff)");
  COMPARE(slti(a7, a1, -2048),
          "022000ab       slti         a7, a1, -2048(0x800)");

  COMPARE(sltui(a7, a7, 2047),
          "025ffd6b       sltui        a7, a7, 2047(0x7ff)");
  COMPARE(sltui(t1, t1, -2048),
          "026001ad       sltui        t1, t1, -2048(0x800)");

  COMPARE(addi_w(t0, t2, 2047),
          "029ffdcc       addi.w       t0, t2, 2047(0x7ff)");
  COMPARE(addi_w(a0, a0, -2048),
          "02a00084       addi.w       a0, a0, -2048(0x800)");

  COMPARE(addi_d(a0, zero_reg, 2047),
          "02dffc04       addi.d       a0, zero_reg, 2047(0x7ff)");
  COMPARE(addi_d(t7, t7, -2048),
          "02e00273       addi.d       t7, t7, -2048(0x800)");

  COMPARE(lu52i_d(a0, a0, 2047), "031ffc84       lu52i.d      a0, a0, 0x7ff");
  COMPARE(lu52i_d(a1, a1, -2048), "032000a5       lu52i.d      a1, a1, 0x800");

  COMPARE(andi(s3, a3, 0xfff), "037ffcfa       andi         s3, a3, 0xfff");
  COMPARE(andi(a4, a4, 0), "03400108       andi         a4, a4, 0x0");

  COMPARE(ori(t6, t6, 0xfff), "03bffe52       ori          t6, t6, 0xfff");
  COMPARE(ori(t6, t6, 0), "03800252       ori          t6, t6, 0x0");

  COMPARE(xori(t1, t1, 0xfff), "03fffdad       xori         t1, t1, 0xfff");
  COMPARE(xori(a3, a3, 0x0), "03c000e7       xori         a3, a3, 0x0");

  COMPARE(ld_b(a1, a1, 2047),
          "281ffca5       ld.b         a1, a1, 2047(0x7ff)");
  COMPARE(ld_b(a4, a4, -2048),
          "28200108       ld.b         a4, a4, -2048(0x800)");

  COMPARE(ld_h(a4, a0, 2047),
          "285ffc88       ld.h         a4, a0, 2047(0x7ff)");
  COMPARE(ld_h(a4, a3, -2048),
          "286000e8       ld.h         a4, a3, -2048(0x800)");

  COMPARE(ld_w(a6, a6, 2047),
          "289ffd4a       ld.w         a6, a6, 2047(0x7ff)");
  COMPARE(ld_w(a5, a4, -2048),
          "28a00109       ld.w         a5, a4, -2048(0x800)");

  COMPARE(ld_d(a0, a3, 2047),
          "28dffce4       ld.d         a0, a3, 2047(0x7ff)");
  COMPARE(ld_d(a6, fp, -2048),
          "28e002ca       ld.d         a6, fp, -2048(0x800)");
  COMPARE(ld_d(a0, a6, 0), "28c00144       ld.d         a0, a6, 0(0x0)");

  COMPARE(st_b(a4, a0, 2047),
          "291ffc88       st.b         a4, a0, 2047(0x7ff)");
  COMPARE(st_b(a6, a5, -2048),
          "2920012a       st.b         a6, a5, -2048(0x800)");

  COMPARE(st_h(a4, a0, 2047),
          "295ffc88       st.h         a4, a0, 2047(0x7ff)");
  COMPARE(st_h(t1, t2, -2048),
          "296001cd       st.h         t1, t2, -2048(0x800)");

  COMPARE(st_w(t3, a4, 2047),
          "299ffd0f       st.w         t3, a4, 2047(0x7ff)");
  COMPARE(st_w(a3, t2, -2048),
          "29a001c7       st.w         a3, t2, -2048(0x800)");

  COMPARE(st_d(s3, sp, 2047),
          "29dffc7a       st.d         s3, sp, 2047(0x7ff)");
  COMPARE(st_d(fp, s6, -2048),
          "29e003b6       st.d         fp, s6, -2048(0x800)");

  COMPARE(ld_bu(a6, a0, 2047),
          "2a1ffc8a       ld.bu        a6, a0, 2047(0x7ff)");
  COMPARE(ld_bu(a7, a7, -2048),
          "2a20016b       ld.bu        a7, a7, -2048(0x800)");

  COMPARE(ld_hu(a7, a7, 2047),
          "2a5ffd6b       ld.hu        a7, a7, 2047(0x7ff)");
  COMPARE(ld_hu(a3, a3, -2048),
          "2a6000e7       ld.hu        a3, a3, -2048(0x800)");

  COMPARE(ld_wu(a3, a0, 2047),
          "2a9ffc87       ld.wu        a3, a0, 2047(0x7ff)");
  COMPARE(ld_wu(a3, a5, -2048),
          "2aa00127       ld.wu        a3, a5, -2048(0x800)");

  COMPARE(fld_s(f0, a3, 2047),
          "2b1ffce0       fld.s        f0, a3, 2047(0x7ff)");
  COMPARE(fld_s(f0, a1, -2048),
          "2b2000a0       fld.s        f0, a1, -2048(0x800)");

  COMPARE(fld_d(f0, a0, 2047),
          "2b9ffc80       fld.d        f0, a0, 2047(0x7ff)");
  COMPARE(fld_d(f0, fp, -2048),
          "2ba002c0       fld.d        f0, fp, -2048(0x800)");

  COMPARE(fst_d(f0, fp, 2047),
          "2bdffec0       fst.d        f0, fp, 2047(0x7ff)");
  COMPARE(fst_d(f0, a0, -2048),
          "2be00080       fst.d        f0, a0, -2048(0x800)");

  COMPARE(fst_s(f0, a5, 2047),
          "2b5ffd20       fst.s        f0, a5, 2047(0x7ff)");
  COMPARE(fst_s(f0, a3, -2048),
          "2b6000e0       fst.s        f0, a3, -2048(0x800)");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp12) {
  SET_UP();

  COMPARE(fmadd_s(f0, f1, f2, f3),
          "08118820       fmadd.s      f0, f1, f2, f3");
  COMPARE(fmadd_s(f4, f5, f6, f7),
          "081398a4       fmadd.s      f4, f5, f6, f7");

  COMPARE(fmadd_d(f8, f9, f10, f11),
          "0825a928       fmadd.d      f8, f9, f10, f11");
  COMPARE(fmadd_d(f12, f13, f14, f15),
          "0827b9ac       fmadd.d      f12, f13, f14, f15");

  COMPARE(fmsub_s(f0, f1, f2, f3),
          "08518820       fmsub.s      f0, f1, f2, f3");
  COMPARE(fmsub_s(f4, f5, f6, f7),
          "085398a4       fmsub.s      f4, f5, f6, f7");

  COMPARE(fmsub_d(f8, f9, f10, f11),
          "0865a928       fmsub.d      f8, f9, f10, f11");
  COMPARE(fmsub_d(f12, f13, f14, f15),
          "0867b9ac       fmsub.d      f12, f13, f14, f15");

  COMPARE(fnmadd_s(f0, f1, f2, f3),
          "08918820       fnmadd.s     f0, f1, f2, f3");
  COMPARE(fnmadd_s(f4, f5, f6, f7),
          "089398a4       fnmadd.s     f4, f5, f6, f7");

  COMPARE(fnmadd_d(f8, f9, f10, f11),
          "08a5a928       fnmadd.d     f8, f9, f10, f11");
  COMPARE(fnmadd_d(f12, f13, f14, f15),
          "08a7b9ac       fnmadd.d     f12, f13, f14, f15");

  COMPARE(fnmsub_s(f0, f1, f2, f3),
          "08d18820       fnmsub.s     f0, f1, f2, f3");
  COMPARE(fnmsub_s(f4, f5, f6, f7),
          "08d398a4       fnmsub.s     f4, f5, f6, f7");

  COMPARE(fnmsub_d(f8, f9, f10, f11),
          "08e5a928       fnmsub.d     f8, f9, f10, f11");
  COMPARE(fnmsub_d(f12, f13, f14, f15),
          "08e7b9ac       fnmsub.d     f12, f13, f14, f15");

  COMPARE(fcmp_cond_s(CAF, f1, f2, FCC0),
          "0c100820       fcmp.caf.s   fcc0, f1, f2");
  COMPARE(fcmp_cond_s(CUN, f5, f6, FCC0),
          "0c1418a0       fcmp.cun.s   fcc0, f5, f6");
  COMPARE(fcmp_cond_s(CEQ, f9, f10, FCC0),
          "0c122920       fcmp.ceq.s   fcc0, f9, f10");
  COMPARE(fcmp_cond_s(CUEQ, f13, f14, FCC0),
          "0c1639a0       fcmp.cueq.s  fcc0, f13, f14");

  COMPARE(fcmp_cond_s(CLT, f1, f2, FCC0),
          "0c110820       fcmp.clt.s   fcc0, f1, f2");
  COMPARE(fcmp_cond_s(CULT, f5, f6, FCC0),
          "0c1518a0       fcmp.cult.s  fcc0, f5, f6");
  COMPARE(fcmp_cond_s(CLE, f9, f10, FCC0),
          "0c132920       fcmp.cle.s   fcc0, f9, f10");
  COMPARE(fcmp_cond_s(CULE, f13, f14, FCC0),
          "0c1739a0       fcmp.cule.s  fcc0, f13, f14");

  COMPARE(fcmp_cond_s(CNE, f1, f2, FCC0),
          "0c180820       fcmp.cne.s   fcc0, f1, f2");
  COMPARE(fcmp_cond_s(COR, f5, f6, FCC0),
          "0c1a18a0       fcmp.cor.s   fcc0, f5, f6");
  COMPARE(fcmp_cond_s(CUNE, f9, f10, FCC0),
          "0c1c2920       fcmp.cune.s  fcc0, f9, f10");
  COMPARE(fcmp_cond_s(SAF, f13, f14, FCC0),
          "0c10b9a0       fcmp.saf.s   fcc0, f13, f14");

  COMPARE(fcmp_cond_s(SUN, f1, f2, FCC0),
          "0c148820       fcmp.sun.s   fcc0, f1, f2");
  COMPARE(fcmp_cond_s(SEQ, f5, f6, FCC0),
          "0c1298a0       fcmp.seq.s   fcc0, f5, f6");
  COMPARE(fcmp_cond_s(SUEQ, f9, f10, FCC0),
          "0c16a920       fcmp.sueq.s  fcc0, f9, f10");
  //  COMPARE(fcmp_cond_s(SLT, f13, f14, FCC0),
  //          "0c11b9a0       fcmp.slt.s   fcc0, f13, f14");

  COMPARE(fcmp_cond_s(SULT, f1, f2, FCC0),
          "0c158820       fcmp.sult.s  fcc0, f1, f2");
  COMPARE(fcmp_cond_s(SLE, f5, f6, FCC0),
          "0c1398a0       fcmp.sle.s   fcc0, f5, f6");
  COMPARE(fcmp_cond_s(SULE, f9, f10, FCC0),
          "0c17a920       fcmp.sule.s  fcc0, f9, f10");
  COMPARE(fcmp_cond_s(SNE, f13, f14, FCC0),
          "0c18b9a0       fcmp.sne.s   fcc0, f13, f14");
  COMPARE(fcmp_cond_s(SOR, f13, f14, FCC0),
          "0c1ab9a0       fcmp.sor.s   fcc0, f13, f14");
  COMPARE(fcmp_cond_s(SUNE, f1, f2, FCC0),
          "0c1c8820       fcmp.sune.s  fcc0, f1, f2");

  COMPARE(fcmp_cond_d(CAF, f1, f2, FCC0),
          "0c200820       fcmp.caf.d   fcc0, f1, f2");
  COMPARE(fcmp_cond_d(CUN, f5, f6, FCC0),
          "0c2418a0       fcmp.cun.d   fcc0, f5, f6");
  COMPARE(fcmp_cond_d(CEQ, f9, f10, FCC0),
          "0c222920       fcmp.ceq.d   fcc0, f9, f10");
  COMPARE(fcmp_cond_d(CUEQ, f13, f14, FCC0),
          "0c2639a0       fcmp.cueq.d  fcc0, f13, f14");

  COMPARE(fcmp_cond_d(CLT, f1, f2, FCC0),
          "0c210820       fcmp.clt.d   fcc0, f1, f2");
  COMPARE(fcmp_cond_d(CULT, f5, f6, FCC0),
          "0c2518a0       fcmp.cult.d  fcc0, f5, f6");
  COMPARE(fcmp_cond_d(CLE, f9, f10, FCC0),
          "0c232920       fcmp.cle.d   fcc0, f9, f10");
  COMPARE(fcmp_cond_d(CULE, f13, f14, FCC0),
          "0c2739a0       fcmp.cule.d  fcc0, f13, f14");

  COMPARE(fcmp_cond_d(CNE, f1, f2, FCC0),
          "0c280820       fcmp.cne.d   fcc0, f1, f2");
  COMPARE(fcmp_cond_d(COR, f5, f6, FCC0),
          "0c2a18a0       fcmp.cor.d   fcc0, f5, f6");
  COMPARE(fcmp_cond_d(CUNE, f9, f10, FCC0),
          "0c2c2920       fcmp.cune.d  fcc0, f9, f10");
  COMPARE(fcmp_cond_d(SAF, f13, f14, FCC0),
          "0c20b9a0       fcmp.saf.d   fcc0, f13, f14");

  COMPARE(fcmp_cond_d(SUN, f1, f2, FCC0),
          "0c248820       fcmp.sun.d   fcc0, f1, f2");
  COMPARE(fcmp_cond_d(SEQ, f5, f6, FCC0),
          "0c2298a0       fcmp.seq.d   fcc0, f5, f6");
  COMPARE(fcmp_cond_d(SUEQ, f9, f10, FCC0),
          "0c26a920       fcmp.sueq.d  fcc0, f9, f10");
  //  COMPARE(fcmp_cond_d(SLT, f13, f14, FCC0),
  //          "0c21b9a0       fcmp.slt.d   fcc0, f13, f14");

  COMPARE(fcmp_cond_d(SULT, f1, f2, FCC0),
          "0c258820       fcmp.sult.d  fcc0, f1, f2");
  COMPARE(fcmp_cond_d(SLE, f5, f6, FCC0),
          "0c2398a0       fcmp.sle.d   fcc0, f5, f6");
  COMPARE(fcmp_cond_d(SULE, f9, f10, FCC0),
          "0c27a920       fcmp.sule.d  fcc0, f9, f10");
  COMPARE(fcmp_cond_d(SNE, f13, f14, FCC0),
          "0c28b9a0       fcmp.sne.d   fcc0, f13, f14");
  COMPARE(fcmp_cond_d(SOR, f13, f14, FCC0),
          "0c2ab9a0       fcmp.sor.d   fcc0, f13, f14");
  COMPARE(fcmp_cond_d(SUNE, f1, f2, FCC0),
          "0c2c8820       fcmp.sune.d  fcc0, f1, f2");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp14) {
  SET_UP();

  COMPARE(alsl_w(a0, a1, a2, 1), "000418a4       alsl.w       a0, a1, a2, 1");
  COMPARE(alsl_w(a3, a4, a5, 3), "00052507       alsl.w       a3, a4, a5, 3");
  COMPARE(alsl_w(a6, a7, t0, 4), "0005b16a       alsl.w       a6, a7, t0, 4");

  COMPARE(alsl_wu(t1, t2, t3, 1), "00063dcd       alsl.wu      t1, t2, t3, 1");
  COMPARE(alsl_wu(t4, t5, t6, 3), "00074a30       alsl.wu      t4, t5, t6, 3");
  COMPARE(alsl_wu(a0, a1, a2, 4), "000798a4       alsl.wu      a0, a1, a2, 4");

  COMPARE(alsl_d(a3, a4, a5, 1), "002c2507       alsl.d       a3, a4, a5, 1");
  COMPARE(alsl_d(a6, a7, t0, 3), "002d316a       alsl.d       a6, a7, t0, 3");
  COMPARE(alsl_d(t1, t2, t3, 4), "002dbdcd       alsl.d       t1, t2, t3, 4");

  COMPARE(bytepick_w(t4, t5, t6, 0),
          "00084a30       bytepick.w   t4, t5, t6, 0");
  COMPARE(bytepick_w(a0, a1, a2, 3),
          "000998a4       bytepick.w   a0, a1, a2, 3");

  COMPARE(bytepick_d(a6, a7, t0, 0),
          "000c316a       bytepick.d   a6, a7, t0, 0");
  COMPARE(bytepick_d(t4, t5, t6, 7),
          "000fca30       bytepick.d   t4, t5, t6, 7");

  COMPARE(slli_w(a3, a3, 31), "0040fce7       slli.w       a3, a3, 31");
  COMPARE(slli_w(a6, a6, 1), "0040854a       slli.w       a6, a6, 1");

  COMPARE(slli_d(t3, t2, 63), "0041fdcf       slli.d       t3, t2, 63");
  COMPARE(slli_d(t4, a6, 1), "00410550       slli.d       t4, a6, 1");

  COMPARE(srli_w(a7, a7, 31), "0044fd6b       srli.w       a7, a7, 31");
  COMPARE(srli_w(a4, a4, 1), "00448508       srli.w       a4, a4, 1");

  COMPARE(srli_d(a4, a3, 63), "0045fce8       srli.d       a4, a3, 63");
  COMPARE(srli_d(a4, a4, 1), "00450508       srli.d       a4, a4, 1");

  COMPARE(srai_d(a0, a0, 63), "0049fc84       srai.d       a0, a0, 63");
  COMPARE(srai_d(a4, a1, 1), "004904a8       srai.d       a4, a1, 1");

  COMPARE(srai_w(s4, a3, 31), "0048fcfb       srai.w       s4, a3, 31");
  COMPARE(srai_w(s4, a5, 1), "0048853b       srai.w       s4, a5, 1");

  COMPARE(rotri_d(t7, t6, 1), "004d0653       rotri.d      t7, t6, 1");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp17) {
  SET_UP();

  COMPARE(sltu(t5, t4, a4), "0012a211       sltu         t5, t4, a4");
  COMPARE(sltu(t4, zero_reg, t4),
          "0012c010       sltu         t4, zero_reg, t4");

  COMPARE(add_w(a4, a4, a6), "00102908       add.w        a4, a4, a6");
  COMPARE(add_w(a5, a6, t3), "00103d49       add.w        a5, a6, t3");

  COMPARE(add_d(a4, t0, t1), "0010b588       add.d        a4, t0, t1");
  COMPARE(add_d(a6, a3, t1), "0010b4ea       add.d        a6, a3, t1");

  COMPARE(sub_w(a7, a7, a2), "0011196b       sub.w        a7, a7, a2");
  COMPARE(sub_w(a2, a2, s3), "001168c6       sub.w        a2, a2, s3");

  COMPARE(sub_d(s3, ra, s3), "0011e83a       sub.d        s3, ra, s3");
  COMPARE(sub_d(a0, a1, a2), "001198a4       sub.d        a0, a1, a2");

  COMPARE(slt(a5, a5, a6), "00122929       slt          a5, a5, a6");
  COMPARE(slt(a6, t3, t4), "001241ea       slt          a6, t3, t4");

  COMPARE(maskeqz(a6, a7, t0), "0013316a       maskeqz      a6, a7, t0");
  COMPARE(maskeqz(t1, t2, t3), "00133dcd       maskeqz      t1, t2, t3");

  COMPARE(masknez(a5, a5, a3), "00139d29       masknez      a5, a5, a3");
  COMPARE(masknez(a3, a4, a5), "0013a507       masknez      a3, a4, a5");

  COMPARE(or_(s3, sp, zero_reg),
          "0015007a       or           s3, sp, zero_reg");
  COMPARE(or_(a4, a0, zero_reg),
          "00150088       or           a4, a0, zero_reg");

  COMPARE(and_(sp, sp, t6), "0014c863       and          sp, sp, t6");
  COMPARE(and_(a3, a3, a7), "0014ace7       and          a3, a3, a7");

  COMPARE(nor(a7, a7, a7), "00142d6b       nor          a7, a7, a7");
  COMPARE(nor(t4, t5, t6), "00144a30       nor          t4, t5, t6");

  COMPARE(xor_(a0, a1, a2), "001598a4       xor          a0, a1, a2");
  COMPARE(xor_(a3, a4, a5), "0015a507       xor          a3, a4, a5");

  COMPARE(orn(a6, a7, t0), "0016316a       orn          a6, a7, t0");
  COMPARE(orn(t1, t2, t3), "00163dcd       orn          t1, t2, t3");

  COMPARE(andn(t4, t5, t6), "0016ca30       andn         t4, t5, t6");
  COMPARE(andn(a0, a1, a2), "001698a4       andn         a0, a1, a2");

  COMPARE(sll_w(a3, t0, a7), "00172d87       sll.w        a3, t0, a7");
  COMPARE(sll_w(a3, a4, a3), "00171d07       sll.w        a3, a4, a3");

  COMPARE(srl_w(a3, a4, a3), "00179d07       srl.w        a3, a4, a3");
  COMPARE(srl_w(a3, t1, t4), "0017c1a7       srl.w        a3, t1, t4");

  COMPARE(sra_w(a4, t4, a4), "00182208       sra.w        a4, t4, a4");
  COMPARE(sra_w(a3, t1, a6), "001829a7       sra.w        a3, t1, a6");

  COMPARE(sll_d(a3, a1, a3), "00189ca7       sll.d        a3, a1, a3");
  COMPARE(sll_d(a7, a4, t0), "0018b10b       sll.d        a7, a4, t0");

  COMPARE(srl_d(a7, a7, t0), "0019316b       srl.d        a7, a7, t0");
  COMPARE(srl_d(t0, a6, t0), "0019314c       srl.d        t0, a6, t0");

  COMPARE(sra_d(a3, a4, a5), "0019a507       sra.d        a3, a4, a5");
  COMPARE(sra_d(a6, a7, t0), "0019b16a       sra.d        a6, a7, t0");

  COMPARE(rotr_d(t1, t2, t3), "001bbdcd       rotr.d       t1, t2, t3");
  COMPARE(rotr_d(t4, t5, t6), "001bca30       rotr.d       t4, t5, t6");

  COMPARE(rotr_w(a0, a1, a2), "001b18a4       rotr.w       a0, a1, a2");
  COMPARE(rotr_w(a3, a4, a5), "001b2507       rotr.w       a3, a4, a5");

  COMPARE(mul_w(t8, a5, t7), "001c4d34       mul.w        t8, a5, t7");
  COMPARE(mul_w(t4, t5, t6), "001c4a30       mul.w        t4, t5, t6");

  COMPARE(mulh_w(s3, a3, t7), "001cccfa       mulh.w       s3, a3, t7");
  COMPARE(mulh_w(a0, a1, a2), "001c98a4       mulh.w       a0, a1, a2");

  COMPARE(mulh_wu(a6, a7, t0), "001d316a       mulh.wu      a6, a7, t0");
  COMPARE(mulh_wu(t1, t2, t3), "001d3dcd       mulh.wu      t1, t2, t3");

  COMPARE(mul_d(t2, a5, t1), "001db52e       mul.d        t2, a5, t1");
  COMPARE(mul_d(a4, a4, a5), "001da508       mul.d        a4, a4, a5");

  COMPARE(mulh_d(a3, a4, a5), "001e2507       mulh.d       a3, a4, a5");
  COMPARE(mulh_d(a6, a7, t0), "001e316a       mulh.d       a6, a7, t0");

  COMPARE(mulh_du(t1, t2, t3), "001ebdcd       mulh.du      t1, t2, t3");
  COMPARE(mulh_du(t4, t5, t6), "001eca30       mulh.du      t4, t5, t6");

  COMPARE(mulw_d_w(a0, a1, a2), "001f18a4       mulw.d.w     a0, a1, a2");
  COMPARE(mulw_d_w(a3, a4, a5), "001f2507       mulw.d.w     a3, a4, a5");

  COMPARE(mulw_d_wu(a6, a7, t0), "001fb16a       mulw.d.wu    a6, a7, t0");
  COMPARE(mulw_d_wu(t1, t2, t3), "001fbdcd       mulw.d.wu    t1, t2, t3");

  COMPARE(div_w(a5, a5, a3), "00201d29       div.w        a5, a5, a3");
  COMPARE(div_w(t4, t5, t6), "00204a30       div.w        t4, t5, t6");

  COMPARE(mod_w(a6, t3, a6), "0020a9ea       mod.w        a6, t3, a6");
  COMPARE(mod_w(a3, a4, a3), "00209d07       mod.w        a3, a4, a3");

  COMPARE(div_wu(t1, t2, t3), "00213dcd       div.wu       t1, t2, t3");
  COMPARE(div_wu(t4, t5, t6), "00214a30       div.wu       t4, t5, t6");

  COMPARE(mod_wu(a0, a1, a2), "002198a4       mod.wu       a0, a1, a2");
  COMPARE(mod_wu(a3, a4, a5), "0021a507       mod.wu       a3, a4, a5");

  COMPARE(div_d(t0, t0, a6), "0022298c       div.d        t0, t0, a6");
  COMPARE(div_d(a7, a7, a5), "0022256b       div.d        a7, a7, a5");

  COMPARE(mod_d(a6, a7, t0), "0022b16a       mod.d        a6, a7, t0");
  COMPARE(mod_d(t1, t2, t3), "0022bdcd       mod.d        t1, t2, t3");

  COMPARE(div_du(t4, t5, t6), "00234a30       div.du       t4, t5, t6");
  COMPARE(div_du(a0, a1, a2), "002318a4       div.du       a0, a1, a2");

  COMPARE(mod_du(a3, a4, a5), "0023a507       mod.du       a3, a4, a5");
  COMPARE(mod_du(a6, a7, t0), "0023b16a       mod.du       a6, a7, t0");

  COMPARE(fadd_s(f3, f4, f5), "01009483       fadd.s       f3, f4, f5");
  COMPARE(fadd_s(f6, f7, f8), "0100a0e6       fadd.s       f6, f7, f8");

  COMPARE(fadd_d(f0, f1, f0), "01010020       fadd.d       f0, f1, f0");
  COMPARE(fadd_d(f0, f1, f2), "01010820       fadd.d       f0, f1, f2");

  COMPARE(fsub_s(f9, f10, f11), "0102ad49       fsub.s       f9, f10, f11");
  COMPARE(fsub_s(f12, f13, f14), "0102b9ac       fsub.s       f12, f13, f14");

  COMPARE(fsub_d(f30, f0, f30), "0103781e       fsub.d       f30, f0, f30");
  COMPARE(fsub_d(f0, f0, f1), "01030400       fsub.d       f0, f0, f1");

  COMPARE(fmul_s(f15, f16, f17), "0104c60f       fmul.s       f15, f16, f17");
  COMPARE(fmul_s(f18, f19, f20), "0104d272       fmul.s       f18, f19, f20");

  COMPARE(fmul_d(f0, f0, f1), "01050400       fmul.d       f0, f0, f1");
  COMPARE(fmul_d(f0, f0, f0), "01050000       fmul.d       f0, f0, f0");

  COMPARE(fdiv_s(f0, f1, f2), "01068820       fdiv.s       f0, f1, f2");
  COMPARE(fdiv_s(f3, f4, f5), "01069483       fdiv.s       f3, f4, f5");

  COMPARE(fdiv_d(f0, f0, f1), "01070400       fdiv.d       f0, f0, f1");
  COMPARE(fdiv_d(f0, f1, f0), "01070020       fdiv.d       f0, f1, f0");

  COMPARE(fmax_s(f9, f10, f11), "0108ad49       fmax.s       f9, f10, f11");
  COMPARE(fmin_s(f6, f7, f8), "010aa0e6       fmin.s       f6, f7, f8");

  COMPARE(fmax_d(f0, f1, f0), "01090020       fmax.d       f0, f1, f0");
  COMPARE(fmin_d(f0, f1, f0), "010b0020       fmin.d       f0, f1, f0");

  COMPARE(fmaxa_s(f12, f13, f14), "010cb9ac       fmaxa.s      f12, f13, f14");
  COMPARE(fmina_s(f15, f16, f17), "010ec60f       fmina.s      f15, f16, f17");

  COMPARE(fmaxa_d(f18, f19, f20), "010d5272       fmaxa.d      f18, f19, f20");
  COMPARE(fmina_d(f0, f1, f2), "010f0820       fmina.d      f0, f1, f2");

  COMPARE(ldx_b(a0, a1, a2), "380018a4       ldx.b        a0, a1, a2");
  COMPARE(ldx_h(a3, a4, a5), "38042507       ldx.h        a3, a4, a5");
  COMPARE(ldx_w(a6, a7, t0), "3808316a       ldx.w        a6, a7, t0");

  COMPARE(stx_b(t1, t2, t3), "38103dcd       stx.b        t1, t2, t3");
  COMPARE(stx_h(t4, t5, t6), "38144a30       stx.h        t4, t5, t6");
  COMPARE(stx_w(a0, a1, a2), "381818a4       stx.w        a0, a1, a2");

  COMPARE(ldx_bu(a3, a4, a5), "38202507       ldx.bu       a3, a4, a5");
  COMPARE(ldx_hu(a6, a7, t0), "3824316a       ldx.hu       a6, a7, t0");
  COMPARE(ldx_wu(t1, t2, t3), "38283dcd       ldx.wu       t1, t2, t3");

  COMPARE(ldx_d(a2, s6, t6), "380c4ba6       ldx.d        a2, s6, t6");
  COMPARE(ldx_d(t7, s6, t6), "380c4bb3       ldx.d        t7, s6, t6");

  COMPARE(stx_d(a4, a3, t6), "381c48e8       stx.d        a4, a3, t6");
  COMPARE(stx_d(a0, a3, t6), "381c48e4       stx.d        a0, a3, t6");

  COMPARE(dbar(0), "38720000       dbar         0x0(0)");
  COMPARE(ibar(5555), "387295b3       ibar         0x15b3(5555)");

  COMPARE(break_(0), "002a0000       break        code: 0x0(0)");
  COMPARE(break_(0x3fc0), "002a3fc0       break        code: 0x3fc0(16320)");

  COMPARE(fldx_s(f3, a4, a5), "38302503       fldx.s       f3, a4, a5");
  COMPARE(fldx_d(f6, a7, t0), "38343166       fldx.d       f6, a7, t0");

  COMPARE(fstx_s(f1, t2, t3), "38383dc1       fstx.s       f1, t2, t3");
  COMPARE(fstx_d(f4, t5, t6), "383c4a24       fstx.d       f4, t5, t6");

  COMPARE(amswap_w(a4, a5, a6), "38602548       amswap.w     a4, a5, a6");
  COMPARE(amswap_d(a7, t0, t1), "3860b1ab       amswap.d     a7, t0, t1");

  COMPARE(amadd_w(t2, t3, t4), "38613e0e       amadd.w      t2, t3, t4");
  COMPARE(amadd_d(t5, t6, a0), "3861c891       amadd.d      t5, t6, a0");

  COMPARE(amand_w(a1, a2, a3), "386218e5       amand.w      a1, a2, a3");
  COMPARE(amand_d(a4, a5, a6), "3862a548       amand.d      a4, a5, a6");

  COMPARE(amor_w(a7, t0, t1), "386331ab       amor.w       a7, t0, t1");
  COMPARE(amor_d(t2, t3, t4), "3863be0e       amor.d       t2, t3, t4");

  COMPARE(amxor_w(t5, t6, a0), "38644891       amxor.w      t5, t6, a0");
  COMPARE(amxor_d(a1, a2, a3), "386498e5       amxor.d      a1, a2, a3");

  COMPARE(ammax_w(a4, a5, a6), "38652548       ammax.w      a4, a5, a6");
  COMPARE(ammax_d(a7, t0, t1), "3865b1ab       ammax.d      a7, t0, t1");

  COMPARE(ammin_w(t2, t3, t4), "38663e0e       ammin.w      t2, t3, t4");
  COMPARE(ammin_d(t5, t6, a0), "3866c891       ammin.d      t5, t6, a0");

  COMPARE(ammax_wu(a1, a2, a3), "386718e5       ammax.wu     a1, a2, a3");
  COMPARE(ammax_du(a4, a5, a6), "3867a548       ammax.du     a4, a5, a6");

  COMPARE(ammin_wu(a7, t0, t1), "386831ab       ammin.wu     a7, t0, t1");
  COMPARE(ammin_du(t2, t3, t4), "3868be0e       ammin.du     t2, t3, t4");

  COMPARE(ammax_db_d(a0, a1, a2), "386e94c4       ammax_db.d   a0, a1, a2");
  COMPARE(ammax_db_du(a3, a4, a5), "3870a127       ammax_db.du  a3, a4, a5");

  COMPARE(ammax_db_w(a6, a7, t0), "386e2d8a       ammax_db.w   a6, a7, t0");
  COMPARE(ammax_db_wu(t1, t2, t3), "387039ed       ammax_db.wu  t1, t2, t3");

  COMPARE(ammin_db_d(t4, t5, t6), "386fc650       ammin_db.d   t4, t5, t6");
  COMPARE(ammin_db_du(a0, a1, a2), "387194c4       ammin_db.du  a0, a1, a2");

  COMPARE(ammin_db_wu(a3, a4, a5), "38712127       ammin_db.wu  a3, a4, a5");
  COMPARE(ammin_db_w(a6, a7, t0), "386f2d8a       ammin_db.w   a6, a7, t0");

  COMPARE(fscaleb_s(f0, f1, f2), "01108820       fscaleb.s    f0, f1, f2");
  COMPARE(fscaleb_d(f3, f4, f5), "01111483       fscaleb.d    f3, f4, f5");

  COMPARE(fcopysign_s(f6, f7, f8), "0112a0e6       fcopysign.s  f6, f7, f8");
  COMPARE(fcopysign_d(f9, f10, f12),
          "01133149       fcopysign.d  f9, f10, f12");

  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp22) {
  SET_UP();

  COMPARE(clz_w(a3, a0), "00001487       clz.w        a3, a0");
  COMPARE(ctz_w(a0, a1), "00001ca4       ctz.w        a0, a1");
  COMPARE(clz_d(a2, a3), "000024e6       clz.d        a2, a3");
  COMPARE(ctz_d(a4, a5), "00002d28       ctz.d        a4, a5");

  COMPARE(clo_w(a0, a1), "000010a4       clo.w        a0, a1");
  COMPARE(cto_w(a2, a3), "000018e6       cto.w        a2, a3");
  COMPARE(clo_d(a4, a5), "00002128       clo.d        a4, a5");
  COMPARE(cto_d(a6, a7), "0000296a       cto.d        a6, a7");

  COMPARE(revb_2h(a6, a7), "0000316a       revb.2h      a6, a7");
  COMPARE(revb_4h(t0, t1), "000035ac       revb.4h      t0, t1");
  COMPARE(revb_2w(t2, t3), "000039ee       revb.2w      t2, t3");
  COMPARE(revb_d(t4, t5), "00003e30       revb.d       t4, t5");

  COMPARE(revh_2w(a0, a1), "000040a4       revh.2w      a0, a1");
  COMPARE(revh_d(a2, a3), "000044e6       revh.d       a2, a3");

  COMPARE(bitrev_4b(a4, a5), "00004928       bitrev.4b    a4, a5");
  COMPARE(bitrev_8b(a6, a7), "00004d6a       bitrev.8b    a6, a7");
  COMPARE(bitrev_w(t0, t1), "000051ac       bitrev.w     t0, t1");
  COMPARE(bitrev_d(t2, t3), "000055ee       bitrev.d     t2, t3");

  COMPARE(ext_w_b(t4, t5), "00005e30       ext.w.b      t4, t5");
  COMPARE(ext_w_h(a0, a1), "000058a4       ext.w.h      a0, a1");

  COMPARE(fabs_s(f2, f3), "01140462       fabs.s       f2, f3");
  COMPARE(fabs_d(f0, f0), "01140800       fabs.d       f0, f0");

  COMPARE(fneg_s(f0, f1), "01141420       fneg.s       f0, f1");
  COMPARE(fneg_d(f0, f0), "01141800       fneg.d       f0, f0");

  COMPARE(fsqrt_s(f4, f5), "011444a4       fsqrt.s      f4, f5");
  COMPARE(fsqrt_d(f0, f0), "01144800       fsqrt.d      f0, f0");

  COMPARE(fmov_s(f6, f7), "011494e6       fmov.s       f6, f7");
  COMPARE(fmov_d(f0, f1), "01149820       fmov.d       f0, f1");
  COMPARE(fmov_d(f1, f0), "01149801       fmov.d       f1, f0");

  COMPARE(movgr2fr_d(f0, t6), "0114aa40       movgr2fr.d   f0, t6");
  COMPARE(movgr2fr_d(f1, t6), "0114aa41       movgr2fr.d   f1, t6");

  COMPARE(movgr2fr_w(f30, a3), "0114a4fe       movgr2fr.w   f30, a3");
  COMPARE(movgr2fr_w(f30, a0), "0114a49e       movgr2fr.w   f30, a0");

  COMPARE(movgr2frh_w(f30, t6), "0114ae5e       movgr2frh.w  f30, t6");
  COMPARE(movgr2frh_w(f0, a3), "0114ace0       movgr2frh.w  f0, a3");

  COMPARE(movfr2gr_s(a3, f30), "0114b7c7       movfr2gr.s   a3, f30");

  COMPARE(movfr2gr_d(a6, f30), "0114bbca       movfr2gr.d   a6, f30");
  COMPARE(movfr2gr_d(t7, f30), "0114bbd3       movfr2gr.d   t7, f30");

  COMPARE(movfrh2gr_s(a5, f0), "0114bc09       movfrh2gr.s  a5, f0");
  COMPARE(movfrh2gr_s(a4, f0), "0114bc08       movfrh2gr.s  a4, f0");

  COMPARE(movgr2fcsr(a2), "0114c0c0       movgr2fcsr   fcsr, a2");
  COMPARE(movfcsr2gr(a4), "0114c808       movfcsr2gr   a4, fcsr");

  COMPARE(movfr2cf(FCC0, f0), "0114d000       movfr2cf     fcc0, f0");
  COMPARE(movcf2fr(f1, FCC1), "0114d421       movcf2fr     f1, fcc1");

  COMPARE(movgr2cf(FCC2, a0), "0114d882       movgr2cf     fcc2, a0");
  COMPARE(movcf2gr(a1, FCC3), "0114dc65       movcf2gr     a1, fcc3");

  COMPARE(fcvt_s_d(f0, f0), "01191800       fcvt.s.d     f0, f0");
  COMPARE(fcvt_d_s(f0, f0), "01192400       fcvt.d.s     f0, f0");

  COMPARE(ftintrm_w_s(f8, f9), "011a0528       ftintrm.w.s  f8, f9");
  COMPARE(ftintrm_w_d(f10, f11), "011a096a       ftintrm.w.d  f10, f11");
  COMPARE(ftintrm_l_s(f12, f13), "011a25ac       ftintrm.l.s  f12, f13");
  COMPARE(ftintrm_l_d(f14, f15), "011a29ee       ftintrm.l.d  f14, f15");

  COMPARE(ftintrp_w_s(f16, f17), "011a4630       ftintrp.w.s  f16, f17");
  COMPARE(ftintrp_w_d(f18, f19), "011a4a72       ftintrp.w.d  f18, f19");
  COMPARE(ftintrp_l_s(f20, f21), "011a66b4       ftintrp.l.s  f20, f21");
  COMPARE(ftintrp_l_d(f0, f1), "011a6820       ftintrp.l.d  f0, f1");

  COMPARE(ftintrz_w_s(f30, f4), "011a849e       ftintrz.w.s  f30, f4");
  COMPARE(ftintrz_w_d(f30, f4), "011a889e       ftintrz.w.d  f30, f4");
  COMPARE(ftintrz_l_s(f30, f0), "011aa41e       ftintrz.l.s  f30, f0");
  COMPARE(ftintrz_l_d(f30, f30), "011aabde       ftintrz.l.d  f30, f30");

  COMPARE(ftintrne_w_s(f2, f3), "011ac462       ftintrne.w.s f2, f3");
  COMPARE(ftintrne_w_d(f4, f5), "011ac8a4       ftintrne.w.d f4, f5");
  COMPARE(ftintrne_l_s(f6, f7), "011ae4e6       ftintrne.l.s f6, f7");
  COMPARE(ftintrne_l_d(f8, f9), "011ae928       ftintrne.l.d f8, f9");

  COMPARE(ftint_w_s(f10, f11), "011b056a       ftint.w.s    f10, f11");
  COMPARE(ftint_w_d(f12, f13), "011b09ac       ftint.w.d    f12, f13");
  COMPARE(ftint_l_s(f14, f15), "011b25ee       ftint.l.s    f14, f15");
  COMPARE(ftint_l_d(f16, f17), "011b2a30       ftint.l.d    f16, f17");

  COMPARE(ffint_s_w(f18, f19), "011d1272       ffint.s.w    f18, f19");
  COMPARE(ffint_s_l(f20, f21), "011d1ab4       ffint.s.l    f20, f21");
  COMPARE(ffint_d_w(f0, f1), "011d2020       ffint.d.w    f0, f1");
  COMPARE(ffint_d_l(f2, f3), "011d2862       ffint.d.l    f2, f3");

  COMPARE(frint_s(f4, f5), "011e44a4       frint.s      f4, f5");
  COMPARE(frint_d(f6, f7), "011e48e6       frint.d      f6, f7");

  COMPARE(frecip_s(f8, f9), "01145528       frecip.s     f8, f9");
  COMPARE(frecip_d(f10, f11), "0114596a       frecip.d     f10, f11");

  COMPARE(frsqrt_s(f12, f13), "011465ac       frsqrt.s     f12, f13");
  COMPARE(frsqrt_d(f14, f15), "011469ee       frsqrt.d     f14, f15");

  COMPARE(fclass_s(f16, f17), "01143630       fclass.s     f16, f17");
  COMPARE(fclass_d(f18, f19), "01143a72       fclass.d     f18, f19");

  COMPARE(flogb_s(f20, f21), "011426b4       flogb.s      f20, f21");
  COMPARE(flogb_d(f0, f1), "01142820       flogb.d      f0, f1");

  VERIFY_RUN();
}

}  // namespace internal
}  // namespace v8
