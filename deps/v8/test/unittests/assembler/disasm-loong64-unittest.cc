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

TEST_F(DisasmLoong64Test, TypeOp10_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);

    COMPARE(vld(vr12, a1, 2047),
            "2c1ffcac       vld          vr12, a1, 2047(0x7ff)");
    COMPARE(vld(vr12, a1, -2048),
            "2c2000ac       vld          vr12, a1, -2048(0x800)");

    COMPARE(vst(vr12, a1, 2047),
            "2c5ffcac       vst          vr12, a1, 2047(0x7ff)");
    COMPARE(vst(vr12, a1, -2048),
            "2c6000ac       vst          vr12, a1, -2048(0x800)");

    COMPARE(vldrepl_b(vr16, a1, 2047),
            "309ffcb0       vldrepl.b    vr16, a1, 2047(0x7ff)");
    COMPARE(vldrepl_b(vr16, a1, -2048),
            "30a000b0       vldrepl.b    vr16, a1, -2048(0x800)");
    COMPARE(vstelm_b(vr16, a1, 127, 0),
            "3181fcb0       vstelm.b     vr16, a1, 127(0x7f), 0");
    COMPARE(vstelm_b(vr16, a1, -128, 15),
            "31be00b0       vstelm.b     vr16, a1, -128(0x80), 15");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp11_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);

    COMPARE(vldrepl_h(vr15, a1, 2046),
            "304ffcaf       vldrepl.h    vr15, a1, 2046(0x7fe)");
    COMPARE(vldrepl_h(vr15, a1, -2048),
            "305000af       vldrepl.h    vr15, a1, -2048(0x800)");
    COMPARE(vstelm_h(vr15, a1, 252, 0),
            "3141f8af       vstelm.h     vr15, a1, 252(0xfc), 0");
    COMPARE(vstelm_h(vr15, a1, -256, 7),
            "315e00af       vstelm.h     vr15, a1, -256(0x100), 7");
  }
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

TEST_F(DisasmLoong64Test, TypeOp12_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vfmadd_s(vr0, vr1, vr2, vr3),
            "09118820       vfmadd.s     vr0, vr1, vr2, vr3");
    COMPARE(vfmadd_d(vr0, vr1, vr2, vr3),
            "09218820       vfmadd.d     vr0, vr1, vr2, vr3");
    COMPARE(vfmsub_s(vr5, vr6, vr7, vr8),
            "09541cc5       vfmsub.s     vr5, vr6, vr7, vr8");
    COMPARE(vfmsub_d(vr5, vr6, vr7, vr8),
            "09641cc5       vfmsub.d     vr5, vr6, vr7, vr8");
    COMPARE(vfnmadd_s(vr0, vr1, vr2, vr3),
            "09918820       vfnmadd.s    vr0, vr1, vr2, vr3");
    COMPARE(vfnmadd_d(vr0, vr1, vr2, vr3),
            "09a18820       vfnmadd.d    vr0, vr1, vr2, vr3");
    COMPARE(vfnmsub_s(vr5, vr6, vr7, vr8),
            "09d41cc5       vfnmsub.s    vr5, vr6, vr7, vr8");
    COMPARE(vfnmsub_d(vr5, vr6, vr7, vr8),
            "09e41cc5       vfnmsub.d    vr5, vr6, vr7, vr8");

    COMPARE(vfcmp_cond_s(CAF, vr9, vr10, vr11),
            "0c502d49       vfcmp.caf.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CUN, vr9, vr10, vr11),
            "0c542d49       vfcmp.cun.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CEQ, vr9, vr10, vr11),
            "0c522d49       vfcmp.ceq.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CUEQ, vr9, vr10, vr11),
            "0c562d49       vfcmp.cueq.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CLT, vr9, vr10, vr11),
            "0c512d49       vfcmp.clt.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CULT, vr9, vr10, vr11),
            "0c552d49       vfcmp.cult.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CLE, vr9, vr10, vr11),
            "0c532d49       vfcmp.cle.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CULE, vr9, vr10, vr11),
            "0c572d49       vfcmp.cule.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CNE, vr9, vr10, vr11),
            "0c582d49       vfcmp.cne.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(CUNE, vr9, vr10, vr11),
            "0c5c2d49       vfcmp.cune.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(COR, vr9, vr10, vr11),
            "0c5a2d49       vfcmp.cor.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SAF, vr9, vr10, vr11),
            "0c50ad49       vfcmp.saf.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SUN, vr9, vr10, vr11),
            "0c54ad49       vfcmp.sun.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SEQ, vr9, vr10, vr11),
            "0c52ad49       vfcmp.seq.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SUEQ, vr9, vr10, vr11),
            "0c56ad49       vfcmp.sueq.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SLT_, vr9, vr10, vr11),
            "0c51ad49       vfcmp.slt.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SULT, vr9, vr10, vr11),
            "0c55ad49       vfcmp.sult.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SLE, vr9, vr10, vr11),
            "0c53ad49       vfcmp.sle.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SULE, vr9, vr10, vr11),
            "0c57ad49       vfcmp.sule.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SNE, vr9, vr10, vr11),
            "0c58ad49       vfcmp.sne.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SUNE, vr9, vr10, vr11),
            "0c5cad49       vfcmp.sune.s vr9, vr10, vr11");
    COMPARE(vfcmp_cond_s(SOR, vr9, vr10, vr11),
            "0c5aad49       vfcmp.sor.s  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CAF, vr9, vr10, vr11),
            "0c602d49       vfcmp.caf.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CUN, vr9, vr10, vr11),
            "0c642d49       vfcmp.cun.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CEQ, vr9, vr10, vr11),
            "0c622d49       vfcmp.ceq.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CUEQ, vr9, vr10, vr11),
            "0c662d49       vfcmp.cueq.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CLT, vr9, vr10, vr11),
            "0c612d49       vfcmp.clt.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CULT, vr9, vr10, vr11),
            "0c652d49       vfcmp.cult.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CLE, vr9, vr10, vr11),
            "0c632d49       vfcmp.cle.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CULE, vr9, vr10, vr11),
            "0c672d49       vfcmp.cule.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CNE, vr9, vr10, vr11),
            "0c682d49       vfcmp.cne.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(CUNE, vr9, vr10, vr11),
            "0c6c2d49       vfcmp.cune.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(COR, vr9, vr10, vr11),
            "0c6a2d49       vfcmp.cor.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SAF, vr9, vr10, vr11),
            "0c60ad49       vfcmp.saf.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SUN, vr9, vr10, vr11),
            "0c64ad49       vfcmp.sun.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SEQ, vr9, vr10, vr11),
            "0c62ad49       vfcmp.seq.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SUEQ, vr9, vr10, vr11),
            "0c66ad49       vfcmp.sueq.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SLT_, vr9, vr10, vr11),
            "0c61ad49       vfcmp.slt.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SULT, vr9, vr10, vr11),
            "0c65ad49       vfcmp.sult.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SLE, vr9, vr10, vr11),
            "0c63ad49       vfcmp.sle.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SULE, vr9, vr10, vr11),
            "0c67ad49       vfcmp.sule.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SNE, vr9, vr10, vr11),
            "0c68ad49       vfcmp.sne.d  vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SUNE, vr9, vr10, vr11),
            "0c6cad49       vfcmp.sune.d vr9, vr10, vr11");
    COMPARE(vfcmp_cond_d(SOR, vr9, vr10, vr11),
            "0c6aad49       vfcmp.sor.d  vr9, vr10, vr11");

    COMPARE(vbitsel_v(vr12, vr13, vr14, vr15),
            "0d17b9ac       vbitsel.v    vr12, vr13, vr14, vr15");
    COMPARE(vshuf_b(vr12, vr13, vr14, vr15),
            "0d57b9ac       vshuf.b      vr12, vr13, vr14, vr15");
    COMPARE(vldrepl_w(vr14, a1, 2044),
            "3027fcae       vldrepl.w    vr14, a1, 2044(0x7fc)");
    COMPARE(vldrepl_w(vr14, a1, -2048),
            "302800ae       vldrepl.w    vr14, a1, -2048(0x800)");
    COMPARE(vstelm_w(vr14, a1, 508, 0),
            "3121fcae       vstelm.w     vr14, a1, 508(0x1fc), 0");
    COMPARE(vstelm_w(vr14, a1, -512, 3),
            "312e00ae       vstelm.w     vr14, a1, -512(0x200), 3");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp13_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vldrepl_d(vr13, a1, 2040),
            "3013fcad       vldrepl.d    vr13, a1, 2040(0x7f8)");
    COMPARE(vldrepl_d(vr13, a1, -2048),
            "301400ad       vldrepl.d    vr13, a1, -2048(0x800)");
    COMPARE(vstelm_d(vr13, a1, 1016, 0),
            "3111fcad       vstelm.d     vr13, a1, 1016(0x3f8), 0");
    COMPARE(vstelm_d(vr13, a1, -512, 1),
            "311700ad       vstelm.d     vr13, a1, -512(0x600), 1");
  }
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

TEST_F(DisasmLoong64Test, TypeOp14_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vextrins_d(vr16, vr17, 255),
            "7383fe30       vextrins.d   vr16, vr17, 255");
    COMPARE(vextrins_w(vr16, vr17, 0),
            "73840230       vextrins.w   vr16, vr17, 0");
    COMPARE(vextrins_h(vr16, vr17, 255),
            "738bfe30       vextrins.h   vr16, vr17, 255");
    COMPARE(vextrins_b(vr16, vr17, 0),
            "738c0230       vextrins.b   vr16, vr17, 0");
    COMPARE(vshuf4i_b(vr16, vr17, 255),
            "7393fe30       vshuf4i.b    vr16, vr17, 255");
    COMPARE(vshuf4i_h(vr16, vr17, 0),
            "73940230       vshuf4i.h    vr16, vr17, 0");
    COMPARE(vshuf4i_w(vr16, vr17, 255),
            "739bfe30       vshuf4i.w    vr16, vr17, 255");
    COMPARE(vshuf4i_d(vr16, vr17, 0),
            "739c0230       vshuf4i.d    vr16, vr17, 0");
    COMPARE(vbitseli_b(vr16, vr17, 155),
            "73c66e30       vbitseli.b   vr16, vr17, 155");
    COMPARE(vandi_b(vr16, vr17, 12),
            "73d03230       vandi.b      vr16, vr17, 12");
    COMPARE(vori_b(vr16, vr17, 245),
            "73d7d630       vori.b       vr16, vr17, 245");
    COMPARE(vxori_b(vr16, vr17, 12),
            "73d83230       vxori.b      vr16, vr17, 12");
    COMPARE(vnori_b(vr16, vr17, 185),
            "73dee630       vnori.b      vr16, vr17, 185");
    COMPARE(vldi(vr16, 4095), "73e1fff0       vldi         vr16, 4095(0xfff)");
    COMPARE(vldi(vr16, -4096),
            "73e20010       vldi         vr16, -4096(0x1000)");
    COMPARE(vpermi_w(vr16, vr17, 254),
            "73e7fa30       vpermi.w     vr16, vr17, 254");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp15_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vsrlni_d_q(vr12, vr13, 127),
            "7343fdac       vsrlni.d.q   vr12, vr13, 127");
    COMPARE(vsrlrni_d_q(vr12, vr13, 127),
            "7347fdac       vsrlrni.d.q  vr12, vr13, 127");
    COMPARE(vssrlni_d_q(vr12, vr13, 127),
            "734bfdac       vssrlni.d.q  vr12, vr13, 127");
    COMPARE(vssrlni_du_q(vr12, vr13, 127),
            "734ffdac       vssrlni.du.q vr12, vr13, 127");
    COMPARE(vssrlrni_d_q(vr12, vr13, 127),
            "7353fdac       vssrlrni.d.q vr12, vr13, 127");
    COMPARE(vssrlrni_du_q(vr12, vr13, 127),
            "7357fdac       vssrlrni.du.q vr12, vr13, 127");
    COMPARE(vsrani_d_q(vr14, vr15, 127),
            "735bfdee       vsrani.d.q   vr14, vr15, 127");
    COMPARE(vsrarni_d_q(vr14, vr15, 127),
            "735ffdee       vsrarni.d.q  vr14, vr15, 127");
    COMPARE(vssrani_d_q(vr14, vr15, 127),
            "7363fdee       vssrani.d.q  vr14, vr15, 127");
    COMPARE(vssrani_du_q(vr14, vr15, 127),
            "7367fdee       vssrani.du.q vr14, vr15, 127");
    COMPARE(vssrarni_d_q(vr14, vr15, 127),
            "736bfdee       vssrarni.d.q vr14, vr15, 127");
    COMPARE(vssrarni_du_q(vr14, vr15, 127),
            "736ffdee       vssrarni.du.q vr14, vr15, 127");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp16_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vrotri_d(vr4, vr5, 63), "72a1fca4       vrotri.d     vr4, vr5, 63");
    COMPARE(vsrlri_d(vr4, vr5, 63), "72a5fca4       vsrlri.d     vr4, vr5, 63");
    COMPARE(vsrari_d(vr4, vr5, 63), "72a9fca4       vsrari.d     vr4, vr5, 63");
    COMPARE(vbitclri_d(vr8, vr9, 63),
            "7311fd28       vbitclri.d   vr8, vr9, 63");
    COMPARE(vbitseti_d(vr8, vr9, 63),
            "7315fd28       vbitseti.d   vr8, vr9, 63");
    COMPARE(vbitrevi_d(vr8, vr9, 63),
            "7319fd28       vbitrevi.d   vr8, vr9, 63");
    COMPARE(vsat_d(vr10, vr11, 63),
            "7325fd6a       vsat.d       vr10, vr11, 63");
    COMPARE(vsat_du(vr10, vr11, 63),
            "7329fd6a       vsat.du      vr10, vr11, 63");
    COMPARE(vslli_d(vr10, vr11, 63),
            "732dfd6a       vslli.d      vr10, vr11, 63");
    COMPARE(vsrai_b(vr10, vr11, 7),
            "73343d6a       vsrai.b      vr10, vr11, 7");
    COMPARE(vsrai_d(vr10, vr11, 63),
            "7335fd6a       vsrai.d      vr10, vr11, 63");
    COMPARE(vsrlni_w_d(vr12, vr13, 63),
            "7341fdac       vsrlni.w.d   vr12, vr13, 63");
    COMPARE(vsrlrni_w_d(vr12, vr13, 63),
            "7345fdac       vsrlrni.w.d  vr12, vr13, 63");
    COMPARE(vssrlni_w_d(vr12, vr13, 63),
            "7349fdac       vssrlni.w.d  vr12, vr13, 63");
    COMPARE(vssrlni_wu_d(vr12, vr13, 63),
            "734dfdac       vssrlni.wu.d vr12, vr13, 63");
    COMPARE(vssrlrni_w_d(vr12, vr13, 63),
            "7351fdac       vssrlrni.w.d vr12, vr13, 63");
    COMPARE(vssrlrni_wu_d(vr12, vr13, 63),
            "7355fdac       vssrlrni.wu.d vr12, vr13, 63");
    COMPARE(vsrani_w_d(vr14, vr15, 63),
            "7359fdee       vsrani.w.d   vr14, vr15, 63");
    COMPARE(vsrarni_w_d(vr14, vr15, 63),
            "735dfdee       vsrarni.w.d  vr14, vr15, 63");
    COMPARE(vssrani_w_d(vr14, vr15, 63),
            "7361fdee       vssrani.w.d  vr14, vr15, 63");
    COMPARE(vssrani_wu_d(vr14, vr15, 63),
            "7365fdee       vssrani.wu.d vr14, vr15, 63");
    COMPARE(vssrarni_w_d(vr14, vr15, 63),
            "7369fdee       vssrarni.w.d vr14, vr15, 63");
    COMPARE(vssrarni_wu_d(vr14, vr15, 63),
            "736dfdee       vssrarni.wu.d vr14, vr15, 63");
  }
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

TEST_F(DisasmLoong64Test, TypeOp17_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vldx(vr17, a2, a3), "38401cd1       vldx         vr17, a2, a3");
    COMPARE(vstx(vr18, a4, a5), "38442512       vstx         vr18, a4, a5");
    COMPARE(vseq_b(vr19, vr20, vr21),
            "70005693       vseq.b       vr19, vr20, vr21");
    COMPARE(vseq_h(vr22, vr23, vr24),
            "7000e2f6       vseq.h       vr22, vr23, vr24");
    COMPARE(vseq_w(vr19, vr20, vr21),
            "70015693       vseq.w       vr19, vr20, vr21");
    COMPARE(vseq_d(vr19, vr20, vr21),
            "7001d693       vseq.d       vr19, vr20, vr21");
    COMPARE(vsle_b(vr19, vr20, vr21),
            "70025693       vsle.b       vr19, vr20, vr21");
    COMPARE(vsle_h(vr19, vr20, vr21),
            "7002d693       vsle.h       vr19, vr20, vr21");
    COMPARE(vsle_w(vr19, vr20, vr21),
            "70035693       vsle.w       vr19, vr20, vr21");
    COMPARE(vsle_d(vr19, vr20, vr21),
            "7003d693       vsle.d       vr19, vr20, vr21");
    COMPARE(vsle_bu(vr19, vr20, vr21),
            "70045693       vsle.bu      vr19, vr20, vr21");
    COMPARE(vsle_hu(vr19, vr20, vr21),
            "7004d693       vsle.hu      vr19, vr20, vr21");
    COMPARE(vsle_wu(vr19, vr20, vr21),
            "70055693       vsle.wu      vr19, vr20, vr21");
    COMPARE(vsle_du(vr19, vr20, vr21),
            "7005d693       vsle.du      vr19, vr20, vr21");
    COMPARE(vslt_b(vr19, vr20, vr21),
            "70065693       vslt.b       vr19, vr20, vr21");
    COMPARE(vslt_h(vr19, vr20, vr21),
            "7006d693       vslt.h       vr19, vr20, vr21");
    COMPARE(vslt_w(vr19, vr20, vr21),
            "70075693       vslt.w       vr19, vr20, vr21");
    COMPARE(vslt_d(vr19, vr20, vr21),
            "7007d693       vslt.d       vr19, vr20, vr21");
    COMPARE(vslt_bu(vr19, vr20, vr21),
            "70085693       vslt.bu      vr19, vr20, vr21");
    COMPARE(vslt_hu(vr19, vr20, vr21),
            "7008d693       vslt.hu      vr19, vr20, vr21");
    COMPARE(vslt_wu(vr19, vr20, vr21),
            "70095693       vslt.wu      vr19, vr20, vr21");
    COMPARE(vslt_du(vr19, vr20, vr21),
            "7009d693       vslt.du      vr19, vr20, vr21");
    COMPARE(vadd_b(vr22, vr23, vr24),
            "700a62f6       vadd.b       vr22, vr23, vr24");
    COMPARE(vadd_h(vr22, vr23, vr24),
            "700ae2f6       vadd.h       vr22, vr23, vr24");
    COMPARE(vadd_w(vr22, vr23, vr24),
            "700b62f6       vadd.w       vr22, vr23, vr24");
    COMPARE(vadd_d(vr22, vr23, vr24),
            "700be2f6       vadd.d       vr22, vr23, vr24");
    COMPARE(vsub_b(vr25, vr26, vr27),
            "700c6f59       vsub.b       vr25, vr26, vr27");
    COMPARE(vsub_h(vr25, vr26, vr27),
            "700cef59       vsub.h       vr25, vr26, vr27");
    COMPARE(vsub_w(vr25, vr26, vr27),
            "700d6f59       vsub.w       vr25, vr26, vr27");
    COMPARE(vsub_d(vr25, vr26, vr27),
            "700def59       vsub.d       vr25, vr26, vr27");
    COMPARE(vaddwev_h_b(vr28, vr29, vr30),
            "701e7bbc       vaddwev.h.b  vr28, vr29, vr30");
    COMPARE(vaddwev_w_h(vr28, vr29, vr30),
            "701efbbc       vaddwev.w.h  vr28, vr29, vr30");
    COMPARE(vaddwev_d_w(vr28, vr29, vr30),
            "701f7bbc       vaddwev.d.w  vr28, vr29, vr30");
    COMPARE(vaddwev_q_d(vr28, vr29, vr30),
            "701ffbbc       vaddwev.q.d  vr28, vr29, vr30");
    COMPARE(vsubwev_h_b(vr28, vr29, vr30),
            "70207bbc       vsubwev.h.b  vr28, vr29, vr30");
    COMPARE(vsubwev_w_h(vr28, vr29, vr30),
            "7020fbbc       vsubwev.w.h  vr28, vr29, vr30");
    COMPARE(vsubwev_d_w(vr28, vr29, vr30),
            "70217bbc       vsubwev.d.w  vr28, vr29, vr30");
    COMPARE(vsubwev_q_d(vr28, vr29, vr30),
            "7021fbbc       vsubwev.q.d  vr28, vr29, vr30");
    COMPARE(vaddwod_h_b(vr28, vr29, vr30),
            "70227bbc       vaddwod.h.b  vr28, vr29, vr30");
    COMPARE(vaddwod_w_h(vr28, vr29, vr30),
            "7022fbbc       vaddwod.w.h  vr28, vr29, vr30");
    COMPARE(vaddwod_d_w(vr28, vr29, vr30),
            "70237bbc       vaddwod.d.w  vr28, vr29, vr30");
    COMPARE(vaddwod_q_d(vr28, vr29, vr30),
            "7023fbbc       vaddwod.q.d  vr28, vr29, vr30");
    COMPARE(vsubwod_h_b(vr28, vr29, vr30),
            "70247bbc       vsubwod.h.b  vr28, vr29, vr30");
    COMPARE(vsubwod_w_h(vr28, vr29, vr30),
            "7024fbbc       vsubwod.w.h  vr28, vr29, vr30");
    COMPARE(vsubwod_d_w(vr28, vr29, vr30),
            "70257bbc       vsubwod.d.w  vr28, vr29, vr30");
    COMPARE(vsubwod_q_d(vr28, vr29, vr30),
            "7025fbbc       vsubwod.q.d  vr28, vr29, vr30");
    COMPARE(vaddwev_h_bu(vr28, vr29, vr30),
            "702e7bbc       vaddwev.h.bu vr28, vr29, vr30");
    COMPARE(vaddwev_w_hu(vr28, vr29, vr30),
            "702efbbc       vaddwev.w.hu vr28, vr29, vr30");
    COMPARE(vaddwev_d_wu(vr28, vr29, vr30),
            "702f7bbc       vaddwev.d.wu vr28, vr29, vr30");
    COMPARE(vaddwev_q_du(vr28, vr29, vr30),
            "702ffbbc       vaddwev.q.du vr28, vr29, vr30");
    COMPARE(vsubwev_h_bu(vr28, vr29, vr30),
            "70307bbc       vsubwev.h.bu vr28, vr29, vr30");
    COMPARE(vsubwev_w_hu(vr28, vr29, vr30),
            "7030fbbc       vsubwev.w.hu vr28, vr29, vr30");
    COMPARE(vsubwev_d_wu(vr28, vr29, vr30),
            "70317bbc       vsubwev.d.wu vr28, vr29, vr30");
    COMPARE(vsubwev_q_du(vr28, vr29, vr30),
            "7031fbbc       vsubwev.q.du vr28, vr29, vr30");
    COMPARE(vaddwod_h_bu(vr28, vr29, vr30),
            "70327bbc       vaddwod.h.bu vr28, vr29, vr30");
    COMPARE(vaddwod_w_hu(vr28, vr29, vr30),
            "7032fbbc       vaddwod.w.hu vr28, vr29, vr30");
    COMPARE(vaddwod_d_wu(vr28, vr29, vr30),
            "70337bbc       vaddwod.d.wu vr28, vr29, vr30");
    COMPARE(vaddwod_q_du(vr28, vr29, vr30),
            "7033fbbc       vaddwod.q.du vr28, vr29, vr30");
    COMPARE(vsubwod_h_bu(vr28, vr29, vr30),
            "70347bbc       vsubwod.h.bu vr28, vr29, vr30");
    COMPARE(vsubwod_w_hu(vr28, vr29, vr30),
            "7034fbbc       vsubwod.w.hu vr28, vr29, vr30");
    COMPARE(vsubwod_d_wu(vr28, vr29, vr30),
            "70357bbc       vsubwod.d.wu vr28, vr29, vr30");
    COMPARE(vsubwod_q_du(vr28, vr29, vr30),
            "7035fbbc       vsubwod.q.du vr28, vr29, vr30");
    COMPARE(vaddwev_h_bu_b(vr28, vr29, vr30),
            "703e7bbc       vaddwev.h.bu.b vr28, vr29, vr30");
    COMPARE(vaddwev_w_hu_h(vr28, vr29, vr30),
            "703efbbc       vaddwev.w.hu.h vr28, vr29, vr30");
    COMPARE(vaddwev_d_wu_w(vr28, vr29, vr30),
            "703f7bbc       vaddwev.d.wu.w vr28, vr29, vr30");
    COMPARE(vaddwev_q_du_d(vr28, vr29, vr30),
            "703ffbbc       vaddwev.q.du.d vr28, vr29, vr30");
    COMPARE(vaddwod_h_bu_b(vr28, vr29, vr30),
            "70407bbc       vaddwod.h.bu.b vr28, vr29, vr30");
    COMPARE(vaddwod_w_hu_h(vr28, vr29, vr30),
            "7040fbbc       vaddwod.w.hu.h vr28, vr29, vr30");
    COMPARE(vaddwod_d_wu_w(vr28, vr29, vr30),
            "70417bbc       vaddwod.d.wu.w vr28, vr29, vr30");
    COMPARE(vaddwod_q_du_d(vr28, vr29, vr30),
            "7041fbbc       vaddwod.q.du.d vr28, vr29, vr30");
    COMPARE(vsadd_b(vr0, vr1, vr2),
            "70460820       vsadd.b      vr0, vr1, vr2");
    COMPARE(vsadd_h(vr0, vr1, vr2),
            "70468820       vsadd.h      vr0, vr1, vr2");
    COMPARE(vsadd_w(vr0, vr1, vr2),
            "70470820       vsadd.w      vr0, vr1, vr2");
    COMPARE(vsadd_d(vr0, vr1, vr2),
            "70478820       vsadd.d      vr0, vr1, vr2");
    COMPARE(vssub_b(vr0, vr1, vr2),
            "70480820       vssub.b      vr0, vr1, vr2");
    COMPARE(vssub_h(vr0, vr1, vr2),
            "70488820       vssub.h      vr0, vr1, vr2");
    COMPARE(vssub_w(vr0, vr1, vr2),
            "70490820       vssub.w      vr0, vr1, vr2");
    COMPARE(vssub_d(vr0, vr1, vr2),
            "70498820       vssub.d      vr0, vr1, vr2");
    COMPARE(vsadd_bu(vr0, vr1, vr2),
            "704a0820       vsadd.bu     vr0, vr1, vr2");
    COMPARE(vsadd_hu(vr0, vr1, vr2),
            "704a8820       vsadd.hu     vr0, vr1, vr2");
    COMPARE(vsadd_wu(vr0, vr1, vr2),
            "704b0820       vsadd.wu     vr0, vr1, vr2");
    COMPARE(vsadd_du(vr0, vr1, vr2),
            "704b8820       vsadd.du     vr0, vr1, vr2");
    COMPARE(vssub_bu(vr0, vr1, vr2),
            "704c0820       vssub.bu     vr0, vr1, vr2");
    COMPARE(vssub_hu(vr0, vr1, vr2),
            "704c8820       vssub.hu     vr0, vr1, vr2");
    COMPARE(vssub_wu(vr0, vr1, vr2),
            "704d0820       vssub.wu     vr0, vr1, vr2");
    COMPARE(vssub_du(vr0, vr1, vr2),
            "704d8820       vssub.du     vr0, vr1, vr2");
    COMPARE(vhaddw_h_b(vr3, vr4, vr5),
            "70541483       vhaddw.h.b   vr3, vr4, vr5");
    COMPARE(vhaddw_w_h(vr3, vr4, vr5),
            "70549483       vhaddw.w.h   vr3, vr4, vr5");
    COMPARE(vhaddw_d_w(vr3, vr4, vr5),
            "70551483       vhaddw.d.w   vr3, vr4, vr5");
    COMPARE(vhaddw_q_d(vr3, vr4, vr5),
            "70559483       vhaddw.q.d   vr3, vr4, vr5");
    COMPARE(vhsubw_h_b(vr3, vr4, vr5),
            "70561483       vhsubw.h.b   vr3, vr4, vr5");
    COMPARE(vhsubw_w_h(vr3, vr4, vr5),
            "70569483       vhsubw.w.h   vr3, vr4, vr5");
    COMPARE(vhsubw_d_w(vr3, vr4, vr5),
            "70571483       vhsubw.d.w   vr3, vr4, vr5");
    COMPARE(vhsubw_q_d(vr3, vr4, vr5),
            "70579483       vhsubw.q.d   vr3, vr4, vr5");
    COMPARE(vhaddw_hu_bu(vr3, vr4, vr5),
            "70581483       vhaddw.hu.bu vr3, vr4, vr5");
    COMPARE(vhaddw_wu_hu(vr3, vr4, vr5),
            "70589483       vhaddw.wu.hu vr3, vr4, vr5");
    COMPARE(vhaddw_du_wu(vr3, vr4, vr5),
            "70591483       vhaddw.du.wu vr3, vr4, vr5");
    COMPARE(vhaddw_qu_du(vr3, vr4, vr5),
            "70599483       vhaddw.qu.du vr3, vr4, vr5");
    COMPARE(vhsubw_hu_bu(vr3, vr4, vr5),
            "705a1483       vhsubw.hu.bu vr3, vr4, vr5");
    COMPARE(vhsubw_wu_hu(vr3, vr4, vr5),
            "705a9483       vhsubw.wu.hu vr3, vr4, vr5");
    COMPARE(vhsubw_du_wu(vr3, vr4, vr5),
            "705b1483       vhsubw.du.wu vr3, vr4, vr5");
    COMPARE(vhsubw_qu_du(vr3, vr4, vr5),
            "705b9483       vhsubw.qu.du vr3, vr4, vr5");
    COMPARE(vadda_b(vr6, vr7, vr8),
            "705c20e6       vadda.b      vr6, vr7, vr8");
    COMPARE(vadda_h(vr6, vr7, vr8),
            "705ca0e6       vadda.h      vr6, vr7, vr8");
    COMPARE(vadda_w(vr6, vr7, vr8),
            "705d20e6       vadda.w      vr6, vr7, vr8");
    COMPARE(vadda_d(vr6, vr7, vr8),
            "705da0e6       vadda.d      vr6, vr7, vr8");
    COMPARE(vabsd_b(vr9, vr10, vr11),
            "70602d49       vabsd.b      vr9, vr10, vr11");
    COMPARE(vabsd_h(vr9, vr10, vr11),
            "7060ad49       vabsd.h      vr9, vr10, vr11");
    COMPARE(vabsd_w(vr9, vr10, vr11),
            "70612d49       vabsd.w      vr9, vr10, vr11");
    COMPARE(vabsd_d(vr9, vr10, vr11),
            "7061ad49       vabsd.d      vr9, vr10, vr11");
    COMPARE(vabsd_bu(vr9, vr10, vr11),
            "70622d49       vabsd.bu     vr9, vr10, vr11");
    COMPARE(vabsd_hu(vr9, vr10, vr11),
            "7062ad49       vabsd.hu     vr9, vr10, vr11");
    COMPARE(vabsd_wu(vr9, vr10, vr11),
            "70632d49       vabsd.wu     vr9, vr10, vr11");
    COMPARE(vabsd_du(vr9, vr10, vr11),
            "7063ad49       vabsd.du     vr9, vr10, vr11");
    COMPARE(vavg_b(vr12, vr13, vr14),
            "706439ac       vavg.b       vr12, vr13, vr14");
    COMPARE(vavg_h(vr12, vr13, vr14),
            "7064b9ac       vavg.h       vr12, vr13, vr14");
    COMPARE(vavg_w(vr12, vr13, vr14),
            "706539ac       vavg.w       vr12, vr13, vr14");
    COMPARE(vavg_d(vr12, vr13, vr14),
            "7065b9ac       vavg.d       vr12, vr13, vr14");
    COMPARE(vavg_bu(vr12, vr13, vr14),
            "706639ac       vavg.bu      vr12, vr13, vr14");
    COMPARE(vavg_hu(vr12, vr13, vr14),
            "7066b9ac       vavg.hu      vr12, vr13, vr14");
    COMPARE(vavg_wu(vr12, vr13, vr14),
            "706739ac       vavg.wu      vr12, vr13, vr14");
    COMPARE(vavg_du(vr12, vr13, vr14),
            "7067b9ac       vavg.du      vr12, vr13, vr14");
    COMPARE(vavgr_b(vr12, vr13, vr14),
            "706839ac       vavgr.b      vr12, vr13, vr14");
    COMPARE(vavgr_h(vr12, vr13, vr14),
            "7068b9ac       vavgr.h      vr12, vr13, vr14");
    COMPARE(vavgr_w(vr12, vr13, vr14),
            "706939ac       vavgr.w      vr12, vr13, vr14");
    COMPARE(vavgr_d(vr12, vr13, vr14),
            "7069b9ac       vavgr.d      vr12, vr13, vr14");
    COMPARE(vavgr_bu(vr12, vr13, vr14),
            "706a39ac       vavgr.bu     vr12, vr13, vr14");
    COMPARE(vavgr_hu(vr12, vr13, vr14),
            "706ab9ac       vavgr.hu     vr12, vr13, vr14");
    COMPARE(vavgr_wu(vr12, vr13, vr14),
            "706b39ac       vavgr.wu     vr12, vr13, vr14");
    COMPARE(vavgr_du(vr12, vr13, vr14),
            "706bb9ac       vavgr.du     vr12, vr13, vr14");
    COMPARE(vmax_b(vr15, vr16, vr17),
            "7070460f       vmax.b       vr15, vr16, vr17");
    COMPARE(vmax_h(vr15, vr16, vr17),
            "7070c60f       vmax.h       vr15, vr16, vr17");
    COMPARE(vmax_w(vr15, vr16, vr17),
            "7071460f       vmax.w       vr15, vr16, vr17");
    COMPARE(vmax_d(vr15, vr16, vr17),
            "7071c60f       vmax.d       vr15, vr16, vr17");
    COMPARE(vmin_b(vr15, vr16, vr17),
            "7072460f       vmin.b       vr15, vr16, vr17");
    COMPARE(vmin_h(vr15, vr16, vr17),
            "7072c60f       vmin.h       vr15, vr16, vr17");
    COMPARE(vmin_w(vr15, vr16, vr17),
            "7073460f       vmin.w       vr15, vr16, vr17");
    COMPARE(vmin_d(vr15, vr16, vr17),
            "7073c60f       vmin.d       vr15, vr16, vr17");
    COMPARE(vmax_bu(vr15, vr16, vr17),
            "7074460f       vmax.bu      vr15, vr16, vr17");
    COMPARE(vmax_hu(vr15, vr16, vr17),
            "7074c60f       vmax.hu      vr15, vr16, vr17");
    COMPARE(vmax_wu(vr15, vr16, vr17),
            "7075460f       vmax.wu      vr15, vr16, vr17");
    COMPARE(vmax_du(vr15, vr16, vr17),
            "7075c60f       vmax.du      vr15, vr16, vr17");
    COMPARE(vmin_bu(vr15, vr16, vr17),
            "7076460f       vmin.bu      vr15, vr16, vr17");
    COMPARE(vmin_hu(vr15, vr16, vr17),
            "7076c60f       vmin.hu      vr15, vr16, vr17");
    COMPARE(vmin_wu(vr15, vr16, vr17),
            "7077460f       vmin.wu      vr15, vr16, vr17");
    COMPARE(vmin_du(vr15, vr16, vr17),
            "7077c60f       vmin.du      vr15, vr16, vr17");
    COMPARE(vmul_b(vr18, vr19, vr20),
            "70845272       vmul.b       vr18, vr19, vr20");
    COMPARE(vmul_h(vr18, vr19, vr20),
            "7084d272       vmul.h       vr18, vr19, vr20");
    COMPARE(vmul_w(vr18, vr19, vr20),
            "70855272       vmul.w       vr18, vr19, vr20");
    COMPARE(vmul_d(vr18, vr19, vr20),
            "7085d272       vmul.d       vr18, vr19, vr20");
    COMPARE(vmuh_b(vr18, vr19, vr20),
            "70865272       vmuh.b       vr18, vr19, vr20");
    COMPARE(vmuh_h(vr18, vr19, vr20),
            "7086d272       vmuh.h       vr18, vr19, vr20");
    COMPARE(vmuh_w(vr18, vr19, vr20),
            "70875272       vmuh.w       vr18, vr19, vr20");
    COMPARE(vmuh_d(vr18, vr19, vr20),
            "7087d272       vmuh.d       vr18, vr19, vr20");
    COMPARE(vmuh_bu(vr18, vr19, vr20),
            "70885272       vmuh.bu      vr18, vr19, vr20");
    COMPARE(vmuh_hu(vr18, vr19, vr20),
            "7088d272       vmuh.hu      vr18, vr19, vr20");
    COMPARE(vmuh_wu(vr18, vr19, vr20),
            "70895272       vmuh.wu      vr18, vr19, vr20");
    COMPARE(vmuh_du(vr18, vr19, vr20),
            "7089d272       vmuh.du      vr18, vr19, vr20");
    COMPARE(vmulwev_h_b(vr21, vr22, vr23),
            "70905ed5       vmulwev.h.b  vr21, vr22, vr23");
    COMPARE(vmulwev_w_h(vr21, vr22, vr23),
            "7090ded5       vmulwev.w.h  vr21, vr22, vr23");
    COMPARE(vmulwev_d_w(vr21, vr22, vr23),
            "70915ed5       vmulwev.d.w  vr21, vr22, vr23");
    COMPARE(vmulwev_q_d(vr21, vr22, vr23),
            "7091ded5       vmulwev.q.d  vr21, vr22, vr23");
    COMPARE(vmulwod_h_b(vr21, vr22, vr23),
            "70925ed5       vmulwod.h.b  vr21, vr22, vr23");
    COMPARE(vmulwod_w_h(vr21, vr22, vr23),
            "7092ded5       vmulwod.w.h  vr21, vr22, vr23");
    COMPARE(vmulwod_d_w(vr21, vr22, vr23),
            "70935ed5       vmulwod.d.w  vr21, vr22, vr23");
    COMPARE(vmulwod_q_d(vr21, vr22, vr23),
            "7093ded5       vmulwod.q.d  vr21, vr22, vr23");
    COMPARE(vmulwev_h_bu(vr21, vr22, vr23),
            "70985ed5       vmulwev.h.bu vr21, vr22, vr23");
    COMPARE(vmulwev_w_hu(vr21, vr22, vr23),
            "7098ded5       vmulwev.w.hu vr21, vr22, vr23");
    COMPARE(vmulwev_d_wu(vr21, vr22, vr23),
            "70995ed5       vmulwev.d.wu vr21, vr22, vr23");
    COMPARE(vmulwev_q_du(vr21, vr22, vr23),
            "7099ded5       vmulwev.q.du vr21, vr22, vr23");
    COMPARE(vmulwod_h_bu(vr21, vr22, vr23),
            "709a5ed5       vmulwod.h.bu vr21, vr22, vr23");
    COMPARE(vmulwod_w_hu(vr21, vr22, vr23),
            "709aded5       vmulwod.w.hu vr21, vr22, vr23");
    COMPARE(vmulwod_d_wu(vr21, vr22, vr23),
            "709b5ed5       vmulwod.d.wu vr21, vr22, vr23");
    COMPARE(vmulwod_q_du(vr21, vr22, vr23),
            "709bded5       vmulwod.q.du vr21, vr22, vr23");
    COMPARE(vmulwev_h_bu_b(vr21, vr22, vr23),
            "70a05ed5       vmulwev.h.bu.b vr21, vr22, vr23");
    COMPARE(vmulwev_w_hu_h(vr21, vr22, vr23),
            "70a0ded5       vmulwev.w.hu.h vr21, vr22, vr23");
    COMPARE(vmulwev_d_wu_w(vr21, vr22, vr23),
            "70a15ed5       vmulwev.d.wu.w vr21, vr22, vr23");
    COMPARE(vmulwev_q_du_d(vr21, vr22, vr23),
            "70a1ded5       vmulwev.q.du.d vr21, vr22, vr23");
    COMPARE(vmulwod_h_bu_b(vr21, vr22, vr23),
            "70a25ed5       vmulwod.h.bu.b vr21, vr22, vr23");
    COMPARE(vmulwod_w_hu_h(vr21, vr22, vr23),
            "70a2ded5       vmulwod.w.hu.h vr21, vr22, vr23");
    COMPARE(vmulwod_d_wu_w(vr21, vr22, vr23),
            "70a35ed5       vmulwod.d.wu.w vr21, vr22, vr23");
    COMPARE(vmulwod_q_du_d(vr21, vr22, vr23),
            "70a3ded5       vmulwod.q.du.d vr21, vr22, vr23");
    COMPARE(vmadd_b(vr24, vr25, vr26),
            "70a86b38       vmadd.b      vr24, vr25, vr26");
    COMPARE(vmadd_h(vr24, vr25, vr26),
            "70a8eb38       vmadd.h      vr24, vr25, vr26");
    COMPARE(vmadd_w(vr24, vr25, vr26),
            "70a96b38       vmadd.w      vr24, vr25, vr26");
    COMPARE(vmadd_d(vr24, vr25, vr26),
            "70a9eb38       vmadd.d      vr24, vr25, vr26");
    COMPARE(vmsub_b(vr24, vr25, vr26),
            "70aa6b38       vmsub.b      vr24, vr25, vr26");
    COMPARE(vmsub_h(vr24, vr25, vr26),
            "70aaeb38       vmsub.h      vr24, vr25, vr26");
    COMPARE(vmsub_w(vr24, vr25, vr26),
            "70ab6b38       vmsub.w      vr24, vr25, vr26");
    COMPARE(vmsub_d(vr24, vr25, vr26),
            "70abeb38       vmsub.d      vr24, vr25, vr26");
    COMPARE(vmaddwev_h_b(vr27, vr28, vr29),
            "70ac779b       vmaddwev.h.b vr27, vr28, vr29");
    COMPARE(vmaddwev_w_h(vr27, vr28, vr29),
            "70acf79b       vmaddwev.w.h vr27, vr28, vr29");
    COMPARE(vmaddwev_d_w(vr27, vr28, vr29),
            "70ad779b       vmaddwev.d.w vr27, vr28, vr29");
    COMPARE(vmaddwev_q_d(vr27, vr28, vr29),
            "70adf79b       vmaddwev.q.d vr27, vr28, vr29");
    COMPARE(vmaddwod_h_b(vr27, vr28, vr29),
            "70ae779b       vmaddwod.h.b vr27, vr28, vr29");
    COMPARE(vmaddwod_w_h(vr27, vr28, vr29),
            "70aef79b       vmaddwod.w.h vr27, vr28, vr29");
    COMPARE(vmaddwod_d_w(vr27, vr28, vr29),
            "70af779b       vmaddwod.d.w vr27, vr28, vr29");
    COMPARE(vmaddwod_q_d(vr27, vr28, vr29),
            "70aff79b       vmaddwod.q.d vr27, vr28, vr29");
    COMPARE(vmaddwev_h_bu(vr27, vr28, vr29),
            "70b4779b       vmaddwev.h.bu vr27, vr28, vr29");
    COMPARE(vmaddwev_w_hu(vr27, vr28, vr29),
            "70b4f79b       vmaddwev.w.hu vr27, vr28, vr29");
    COMPARE(vmaddwev_d_wu(vr27, vr28, vr29),
            "70b5779b       vmaddwev.d.wu vr27, vr28, vr29");
    COMPARE(vmaddwev_q_du(vr27, vr28, vr29),
            "70b5f79b       vmaddwev.q.du vr27, vr28, vr29");
    COMPARE(vmaddwod_h_bu(vr27, vr28, vr29),
            "70b6779b       vmaddwod.h.bu vr27, vr28, vr29");
    COMPARE(vmaddwod_w_hu(vr27, vr28, vr29),
            "70b6f79b       vmaddwod.w.hu vr27, vr28, vr29");
    COMPARE(vmaddwod_d_wu(vr27, vr28, vr29),
            "70b7779b       vmaddwod.d.wu vr27, vr28, vr29");
    COMPARE(vmaddwod_q_du(vr27, vr28, vr29),
            "70b7f79b       vmaddwod.q.du vr27, vr28, vr29");
    COMPARE(vmaddwev_h_bu_b(vr27, vr28, vr29),
            "70bc779b       vmaddwev.h.bu.b vr27, vr28, vr29");
    COMPARE(vmaddwev_w_hu_h(vr27, vr28, vr29),
            "70bcf79b       vmaddwev.w.hu.h vr27, vr28, vr29");
    COMPARE(vmaddwev_d_wu_w(vr27, vr28, vr29),
            "70bd779b       vmaddwev.d.wu.w vr27, vr28, vr29");
    COMPARE(vmaddwev_q_du_d(vr27, vr28, vr29),
            "70bdf79b       vmaddwev.q.du.d vr27, vr28, vr29");
    COMPARE(vmaddwod_h_bu_b(vr27, vr28, vr29),
            "70be779b       vmaddwod.h.bu.b vr27, vr28, vr29");
    COMPARE(vmaddwod_w_hu_h(vr27, vr28, vr29),
            "70bef79b       vmaddwod.w.hu.h vr27, vr28, vr29");
    COMPARE(vmaddwod_d_wu_w(vr27, vr28, vr29),
            "70bf779b       vmaddwod.d.wu.w vr27, vr28, vr29");
    COMPARE(vmaddwod_q_du_d(vr27, vr28, vr29),
            "70bff79b       vmaddwod.q.du.d vr27, vr28, vr29");
    COMPARE(vdiv_b(vr0, vr1, vr2), "70e00820       vdiv.b       vr0, vr1, vr2");
    COMPARE(vdiv_h(vr0, vr1, vr2), "70e08820       vdiv.h       vr0, vr1, vr2");
    COMPARE(vdiv_w(vr0, vr1, vr2), "70e10820       vdiv.w       vr0, vr1, vr2");
    COMPARE(vdiv_d(vr0, vr1, vr2), "70e18820       vdiv.d       vr0, vr1, vr2");
    COMPARE(vmod_b(vr0, vr1, vr2), "70e20820       vmod.b       vr0, vr1, vr2");
    COMPARE(vmod_h(vr0, vr1, vr2), "70e28820       vmod.h       vr0, vr1, vr2");
    COMPARE(vmod_w(vr0, vr1, vr2), "70e30820       vmod.w       vr0, vr1, vr2");
    COMPARE(vmod_d(vr0, vr1, vr2), "70e38820       vmod.d       vr0, vr1, vr2");
    COMPARE(vdiv_bu(vr0, vr1, vr2),
            "70e40820       vdiv.bu      vr0, vr1, vr2");
    COMPARE(vdiv_hu(vr0, vr1, vr2),
            "70e48820       vdiv.hu      vr0, vr1, vr2");
    COMPARE(vdiv_wu(vr0, vr1, vr2),
            "70e50820       vdiv.wu      vr0, vr1, vr2");
    COMPARE(vdiv_du(vr0, vr1, vr2),
            "70e58820       vdiv.du      vr0, vr1, vr2");
    COMPARE(vmod_bu(vr0, vr1, vr2),
            "70e60820       vmod.bu      vr0, vr1, vr2");
    COMPARE(vmod_hu(vr0, vr1, vr2),
            "70e68820       vmod.hu      vr0, vr1, vr2");
    COMPARE(vmod_wu(vr0, vr1, vr2),
            "70e70820       vmod.wu      vr0, vr1, vr2");
    COMPARE(vmod_du(vr0, vr1, vr2),
            "70e78820       vmod.du      vr0, vr1, vr2");
    COMPARE(vsll_b(vr3, vr4, vr5), "70e81483       vsll.b       vr3, vr4, vr5");
    COMPARE(vsll_h(vr3, vr4, vr5), "70e89483       vsll.h       vr3, vr4, vr5");
    COMPARE(vsll_w(vr3, vr4, vr5), "70e91483       vsll.w       vr3, vr4, vr5");
    COMPARE(vsll_d(vr3, vr4, vr5), "70e99483       vsll.d       vr3, vr4, vr5");
    COMPARE(vsrl_b(vr3, vr4, vr5), "70ea1483       vsrl.b       vr3, vr4, vr5");
    COMPARE(vsrl_h(vr3, vr4, vr5), "70ea9483       vsrl.h       vr3, vr4, vr5");
    COMPARE(vsrl_w(vr3, vr4, vr5), "70eb1483       vsrl.w       vr3, vr4, vr5");
    COMPARE(vsrl_d(vr3, vr4, vr5), "70eb9483       vsrl.d       vr3, vr4, vr5");
    COMPARE(vsra_b(vr3, vr4, vr5), "70ec1483       vsra.b       vr3, vr4, vr5");
    COMPARE(vsra_h(vr3, vr4, vr5), "70ec9483       vsra.h       vr3, vr4, vr5");
    COMPARE(vsra_w(vr3, vr4, vr5), "70ed1483       vsra.w       vr3, vr4, vr5");
    COMPARE(vsra_d(vr3, vr4, vr5), "70ed9483       vsra.d       vr3, vr4, vr5");
    COMPARE(vrotr_b(vr6, vr7, vr8),
            "70ee20e6       vrotr.b      vr6, vr7, vr8");
    COMPARE(vrotr_h(vr6, vr7, vr8),
            "70eea0e6       vrotr.h      vr6, vr7, vr8");
    COMPARE(vrotr_w(vr6, vr7, vr8),
            "70ef20e6       vrotr.w      vr6, vr7, vr8");
    COMPARE(vrotr_d(vr6, vr7, vr8),
            "70efa0e6       vrotr.d      vr6, vr7, vr8");
    COMPARE(vsrlr_b(vr9, vr10, vr11),
            "70f02d49       vsrlr.b      vr9, vr10, vr11");
    COMPARE(vsrlr_h(vr9, vr10, vr11),
            "70f0ad49       vsrlr.h      vr9, vr10, vr11");
    COMPARE(vsrlr_w(vr9, vr10, vr11),
            "70f12d49       vsrlr.w      vr9, vr10, vr11");
    COMPARE(vsrlr_d(vr9, vr10, vr11),
            "70f1ad49       vsrlr.d      vr9, vr10, vr11");
    COMPARE(vsrar_b(vr9, vr10, vr11),
            "70f22d49       vsrar.b      vr9, vr10, vr11");
    COMPARE(vsrar_h(vr9, vr10, vr11),
            "70f2ad49       vsrar.h      vr9, vr10, vr11");
    COMPARE(vsrar_w(vr9, vr10, vr11),
            "70f32d49       vsrar.w      vr9, vr10, vr11");
    COMPARE(vsrar_d(vr9, vr10, vr11),
            "70f3ad49       vsrar.d      vr9, vr10, vr11");
    COMPARE(vsrln_b_h(vr12, vr13, vr14),
            "70f4b9ac       vsrln.b.h    vr12, vr13, vr14");
    COMPARE(vsrln_h_w(vr12, vr13, vr14),
            "70f539ac       vsrln.h.w    vr12, vr13, vr14");
    COMPARE(vsrln_w_d(vr12, vr13, vr14),
            "70f5b9ac       vsrln.w.d    vr12, vr13, vr14");
    COMPARE(vsran_b_h(vr12, vr13, vr14),
            "70f6b9ac       vsran.b.h    vr12, vr13, vr14");
    COMPARE(vsran_h_w(vr12, vr13, vr14),
            "70f739ac       vsran.h.w    vr12, vr13, vr14");
    COMPARE(vsran_w_d(vr12, vr13, vr14),
            "70f7b9ac       vsran.w.d    vr12, vr13, vr14");
    COMPARE(vsrlrn_b_h(vr12, vr13, vr14),
            "70f8b9ac       vsrlrn.b.h   vr12, vr13, vr14");
    COMPARE(vsrlrn_h_w(vr12, vr13, vr14),
            "70f939ac       vsrlrn.h.w   vr12, vr13, vr14");
    COMPARE(vsrlrn_w_d(vr12, vr13, vr14),
            "70f9b9ac       vsrlrn.w.d   vr12, vr13, vr14");
    COMPARE(vsrarn_b_h(vr12, vr13, vr14),
            "70fab9ac       vsrarn.b.h   vr12, vr13, vr14");
    COMPARE(vsrarn_h_w(vr12, vr13, vr14),
            "70fb39ac       vsrarn.h.w   vr12, vr13, vr14");
    COMPARE(vsrarn_w_d(vr12, vr13, vr14),
            "70fbb9ac       vsrarn.w.d   vr12, vr13, vr14");
    COMPARE(vssrln_b_h(vr15, vr16, vr17),
            "70fcc60f       vssrln.b.h   vr15, vr16, vr17");
    COMPARE(vssrln_h_w(vr15, vr16, vr17),
            "70fd460f       vssrln.h.w   vr15, vr16, vr17");
    COMPARE(vssrln_w_d(vr15, vr16, vr17),
            "70fdc60f       vssrln.w.d   vr15, vr16, vr17");
    COMPARE(vssran_b_h(vr15, vr16, vr17),
            "70fec60f       vssran.b.h   vr15, vr16, vr17");
    COMPARE(vssran_h_w(vr15, vr16, vr17),
            "70ff460f       vssran.h.w   vr15, vr16, vr17");
    COMPARE(vssran_w_d(vr15, vr16, vr17),
            "70ffc60f       vssran.w.d   vr15, vr16, vr17");
    COMPARE(vssrlrn_b_h(vr15, vr16, vr17),
            "7100c60f       vssrlrn.b.h  vr15, vr16, vr17");
    COMPARE(vssrlrn_h_w(vr15, vr16, vr17),
            "7101460f       vssrlrn.h.w  vr15, vr16, vr17");
    COMPARE(vssrlrn_w_d(vr15, vr16, vr17),
            "7101c60f       vssrlrn.w.d  vr15, vr16, vr17");
    COMPARE(vssrarn_b_h(vr15, vr16, vr17),
            "7102c60f       vssrarn.b.h  vr15, vr16, vr17");
    COMPARE(vssrarn_h_w(vr15, vr16, vr17),
            "7103460f       vssrarn.h.w  vr15, vr16, vr17");
    COMPARE(vssrarn_w_d(vr15, vr16, vr17),
            "7103c60f       vssrarn.w.d  vr15, vr16, vr17");
    COMPARE(vssrln_bu_h(vr18, vr19, vr20),
            "7104d272       vssrln.bu.h  vr18, vr19, vr20");
    COMPARE(vssrln_hu_w(vr18, vr19, vr20),
            "71055272       vssrln.hu.w  vr18, vr19, vr20");
    COMPARE(vssrln_wu_d(vr18, vr19, vr20),
            "7105d272       vssrln.wu.d  vr18, vr19, vr20");
    COMPARE(vssran_bu_h(vr18, vr19, vr20),
            "7106d272       vssran.bu.h  vr18, vr19, vr20");
    COMPARE(vssran_hu_w(vr18, vr19, vr20),
            "71075272       vssran.hu.w  vr18, vr19, vr20");
    COMPARE(vssran_wu_d(vr18, vr19, vr20),
            "7107d272       vssran.wu.d  vr18, vr19, vr20");
    COMPARE(vssrlrn_bu_h(vr18, vr19, vr20),
            "7108d272       vssrlrn.bu.h vr18, vr19, vr20");
    COMPARE(vssrlrn_hu_w(vr18, vr19, vr20),
            "71095272       vssrlrn.hu.w vr18, vr19, vr20");
    COMPARE(vssrlrn_wu_d(vr18, vr19, vr20),
            "7109d272       vssrlrn.wu.d vr18, vr19, vr20");
    COMPARE(vssrarn_bu_h(vr18, vr19, vr20),
            "710ad272       vssrarn.bu.h vr18, vr19, vr20");
    COMPARE(vssrarn_hu_w(vr18, vr19, vr20),
            "710b5272       vssrarn.hu.w vr18, vr19, vr20");
    COMPARE(vssrarn_wu_d(vr18, vr19, vr20),
            "710bd272       vssrarn.wu.d vr18, vr19, vr20");
    COMPARE(vbitclr_b(vr21, vr22, vr23),
            "710c5ed5       vbitclr.b    vr21, vr22, vr23");
    COMPARE(vbitclr_h(vr21, vr22, vr23),
            "710cded5       vbitclr.h    vr21, vr22, vr23");
    COMPARE(vbitclr_w(vr21, vr22, vr23),
            "710d5ed5       vbitclr.w    vr21, vr22, vr23");
    COMPARE(vbitclr_d(vr21, vr22, vr23),
            "710dded5       vbitclr.d    vr21, vr22, vr23");
    COMPARE(vbitset_b(vr21, vr22, vr23),
            "710e5ed5       vbitset.b    vr21, vr22, vr23");
    COMPARE(vbitset_h(vr21, vr22, vr23),
            "710eded5       vbitset.h    vr21, vr22, vr23");
    COMPARE(vbitset_w(vr21, vr22, vr23),
            "710f5ed5       vbitset.w    vr21, vr22, vr23");
    COMPARE(vbitset_d(vr21, vr22, vr23),
            "710fded5       vbitset.d    vr21, vr22, vr23");
    COMPARE(vbitrev_b(vr21, vr22, vr23),
            "71105ed5       vbitrev.b    vr21, vr22, vr23");
    COMPARE(vbitrev_h(vr21, vr22, vr23),
            "7110ded5       vbitrev.h    vr21, vr22, vr23");
    COMPARE(vbitrev_w(vr21, vr22, vr23),
            "71115ed5       vbitrev.w    vr21, vr22, vr23");
    COMPARE(vbitrev_d(vr21, vr22, vr23),
            "7111ded5       vbitrev.d    vr21, vr22, vr23");
    COMPARE(vpackev_b(vr24, vr25, vr26),
            "71166b38       vpackev.b    vr24, vr25, vr26");
    COMPARE(vpackev_h(vr24, vr25, vr26),
            "7116eb38       vpackev.h    vr24, vr25, vr26");
    COMPARE(vpackev_w(vr24, vr25, vr26),
            "71176b38       vpackev.w    vr24, vr25, vr26");
    COMPARE(vpackev_d(vr24, vr25, vr26),
            "7117eb38       vpackev.d    vr24, vr25, vr26");
    COMPARE(vpackod_b(vr24, vr25, vr26),
            "71186b38       vpackod.b    vr24, vr25, vr26");
    COMPARE(vpackod_h(vr24, vr25, vr26),
            "7118eb38       vpackod.h    vr24, vr25, vr26");
    COMPARE(vpackod_w(vr24, vr25, vr26),
            "71196b38       vpackod.w    vr24, vr25, vr26");
    COMPARE(vpackod_d(vr24, vr25, vr26),
            "7119eb38       vpackod.d    vr24, vr25, vr26");
    COMPARE(vilvl_b(vr27, vr28, vr29),
            "711a779b       vilvl.b      vr27, vr28, vr29");
    COMPARE(vilvl_h(vr27, vr28, vr29),
            "711af79b       vilvl.h      vr27, vr28, vr29");
    COMPARE(vilvl_w(vr27, vr28, vr29),
            "711b779b       vilvl.w      vr27, vr28, vr29");
    COMPARE(vilvl_d(vr27, vr28, vr29),
            "711bf79b       vilvl.d      vr27, vr28, vr29");
    COMPARE(vilvh_b(vr27, vr28, vr29),
            "711c779b       vilvh.b      vr27, vr28, vr29");
    COMPARE(vilvh_h(vr27, vr28, vr29),
            "711cf79b       vilvh.h      vr27, vr28, vr29");
    COMPARE(vilvh_w(vr27, vr28, vr29),
            "711d779b       vilvh.w      vr27, vr28, vr29");
    COMPARE(vilvh_d(vr27, vr28, vr29),
            "711df79b       vilvh.d      vr27, vr28, vr29");
    COMPARE(vpickev_b(vr30, vr31, vr0),
            "711e03fe       vpickev.b    vr30, vr31, vr0");
    COMPARE(vpickev_h(vr30, vr31, vr0),
            "711e83fe       vpickev.h    vr30, vr31, vr0");
    COMPARE(vpickev_w(vr30, vr31, vr0),
            "711f03fe       vpickev.w    vr30, vr31, vr0");
    COMPARE(vpickev_d(vr30, vr31, vr0),
            "711f83fe       vpickev.d    vr30, vr31, vr0");
    COMPARE(vpickod_b(vr30, vr31, vr0),
            "712003fe       vpickod.b    vr30, vr31, vr0");
    COMPARE(vpickod_h(vr30, vr31, vr0),
            "712083fe       vpickod.h    vr30, vr31, vr0");
    COMPARE(vpickod_w(vr30, vr31, vr0),
            "712103fe       vpickod.w    vr30, vr31, vr0");
    COMPARE(vpickod_d(vr30, vr31, vr0),
            "712183fe       vpickod.d    vr30, vr31, vr0");
    COMPARE(vreplve_b(vr30, vr31, t0),
            "712233fe       vreplve.b    vr30, vr31, t0");
    COMPARE(vreplve_h(vr30, vr31, t0),
            "7122b3fe       vreplve.h    vr30, vr31, t0");
    COMPARE(vreplve_w(vr30, vr31, t0),
            "712333fe       vreplve.w    vr30, vr31, t0");
    COMPARE(vreplve_d(vr30, vr31, t0),
            "7123b3fe       vreplve.d    vr30, vr31, t0");
    COMPARE(vand_v(vr1, vr2, vr3), "71260c41       vand.v       vr1, vr2, vr3");
    COMPARE(vor_v(vr1, vr2, vr3), "71268c41       vor.v        vr1, vr2, vr3");
    COMPARE(vxor_v(vr1, vr2, vr3), "71270c41       vxor.v       vr1, vr2, vr3");
    COMPARE(vnor_v(vr1, vr2, vr3), "71278c41       vnor.v       vr1, vr2, vr3");
    COMPARE(vandn_v(vr1, vr2, vr3),
            "71280c41       vandn.v      vr1, vr2, vr3");
    COMPARE(vorn_v(vr1, vr2, vr3), "71288c41       vorn.v       vr1, vr2, vr3");
    COMPARE(vfrstp_b(vr1, vr2, vr3),
            "712b0c41       vfrstp.b     vr1, vr2, vr3");
    COMPARE(vfrstp_h(vr1, vr2, vr3),
            "712b8c41       vfrstp.h     vr1, vr2, vr3");
    COMPARE(vadd_q(vr1, vr2, vr3), "712d0c41       vadd.q       vr1, vr2, vr3");
    COMPARE(vsub_q(vr1, vr2, vr3), "712d8c41       vsub.q       vr1, vr2, vr3");
    COMPARE(vsigncov_b(vr4, vr5, vr6),
            "712e18a4       vsigncov.b   vr4, vr5, vr6");
    COMPARE(vsigncov_h(vr4, vr5, vr6),
            "712e98a4       vsigncov.h   vr4, vr5, vr6");
    COMPARE(vsigncov_w(vr4, vr5, vr6),
            "712f18a4       vsigncov.w   vr4, vr5, vr6");
    COMPARE(vsigncov_d(vr4, vr5, vr6),
            "712f98a4       vsigncov.d   vr4, vr5, vr6");
    COMPARE(vfadd_s(vr4, vr5, vr6),
            "713098a4       vfadd.s      vr4, vr5, vr6");
    COMPARE(vfadd_d(vr4, vr5, vr6),
            "713118a4       vfadd.d      vr4, vr5, vr6");
    COMPARE(vfsub_s(vr4, vr5, vr6),
            "713298a4       vfsub.s      vr4, vr5, vr6");
    COMPARE(vfsub_d(vr4, vr5, vr6),
            "713318a4       vfsub.d      vr4, vr5, vr6");
    COMPARE(vfmul_s(vr4, vr5, vr6),
            "713898a4       vfmul.s      vr4, vr5, vr6");
    COMPARE(vfmul_d(vr4, vr5, vr6),
            "713918a4       vfmul.d      vr4, vr5, vr6");
    COMPARE(vfdiv_s(vr4, vr5, vr6),
            "713a98a4       vfdiv.s      vr4, vr5, vr6");
    COMPARE(vfdiv_d(vr4, vr5, vr6),
            "713b18a4       vfdiv.d      vr4, vr5, vr6");
    COMPARE(vfmax_s(vr4, vr5, vr6),
            "713c98a4       vfmax.s      vr4, vr5, vr6");
    COMPARE(vfmax_d(vr4, vr5, vr6),
            "713d18a4       vfmax.d      vr4, vr5, vr6");
    COMPARE(vfmin_s(vr4, vr5, vr6),
            "713e98a4       vfmin.s      vr4, vr5, vr6");
    COMPARE(vfmin_d(vr4, vr5, vr6),
            "713f18a4       vfmin.d      vr4, vr5, vr6");
    COMPARE(vfmaxa_s(vr4, vr5, vr6),
            "714098a4       vfmaxa.s     vr4, vr5, vr6");
    COMPARE(vfmaxa_d(vr4, vr5, vr6),
            "714118a4       vfmaxa.d     vr4, vr5, vr6");
    COMPARE(vfmina_s(vr4, vr5, vr6),
            "714298a4       vfmina.s     vr4, vr5, vr6");
    COMPARE(vfmina_d(vr4, vr5, vr6),
            "714318a4       vfmina.d     vr4, vr5, vr6");
    COMPARE(vfcvt_h_s(vr7, vr8, vr9),
            "71462507       vfcvt.h.s    vr7, vr8, vr9");
    COMPARE(vfcvt_s_d(vr7, vr8, vr9),
            "7146a507       vfcvt.s.d    vr7, vr8, vr9");
    COMPARE(vffint_s_l(vr10, vr11, vr12),
            "7148316a       vffint.s.l   vr10, vr11, vr12");
    COMPARE(vftint_w_d(vr10, vr11, vr12),
            "7149b16a       vftint.w.d   vr10, vr11, vr12");
    COMPARE(vftintrm_w_d(vr10, vr11, vr12),
            "714a316a       vftintrm.w.d vr10, vr11, vr12");
    COMPARE(vftintrp_w_d(vr10, vr11, vr12),
            "714ab16a       vftintrp.w.d vr10, vr11, vr12");
    COMPARE(vftintrz_w_d(vr10, vr11, vr12),
            "714b316a       vftintrz.w.d vr10, vr11, vr12");
    COMPARE(vftintrne_w_d(vr10, vr11, vr12),
            "714bb16a       vftintrne.w.d vr10, vr11, vr12");
    COMPARE(vshuf_h(vr13, vr14, vr15),
            "717abdcd       vshuf.h      vr13, vr14, vr15");
    COMPARE(vshuf_w(vr13, vr14, vr15),
            "717b3dcd       vshuf.w      vr13, vr14, vr15");
    COMPARE(vshuf_d(vr13, vr14, vr15),
            "717bbdcd       vshuf.d      vr13, vr14, vr15");
    COMPARE(vseqi_b(vr13, vr14, 15),
            "72803dcd       vseqi.b      vr13, vr14, 15(0xf)");
    COMPARE(vseqi_b(vr13, vr14, -16),
            "728041cd       vseqi.b      vr13, vr14, -16(0x10)");
    COMPARE(vseqi_h(vr13, vr14, 15),
            "7280bdcd       vseqi.h      vr13, vr14, 15(0xf)");
    COMPARE(vseqi_h(vr13, vr14, -16),
            "7280c1cd       vseqi.h      vr13, vr14, -16(0x10)");
    COMPARE(vseqi_w(vr13, vr14, 15),
            "72813dcd       vseqi.w      vr13, vr14, 15(0xf)");
    COMPARE(vseqi_w(vr13, vr14, -16),
            "728141cd       vseqi.w      vr13, vr14, -16(0x10)");
    COMPARE(vseqi_d(vr13, vr14, 15),
            "7281bdcd       vseqi.d      vr13, vr14, 15(0xf)");
    COMPARE(vseqi_d(vr13, vr14, -16),
            "7281c1cd       vseqi.d      vr13, vr14, -16(0x10)");
    COMPARE(vslei_b(vr13, vr14, 15),
            "72823dcd       vslei.b      vr13, vr14, 15(0xf)");
    COMPARE(vslei_b(vr13, vr14, -16),
            "728241cd       vslei.b      vr13, vr14, -16(0x10)");
    COMPARE(vslei_h(vr13, vr14, 15),
            "7282bdcd       vslei.h      vr13, vr14, 15(0xf)");
    COMPARE(vslei_h(vr13, vr14, -16),
            "7282c1cd       vslei.h      vr13, vr14, -16(0x10)");
    COMPARE(vslei_w(vr13, vr14, 15),
            "72833dcd       vslei.w      vr13, vr14, 15(0xf)");
    COMPARE(vslei_w(vr13, vr14, -16),
            "728341cd       vslei.w      vr13, vr14, -16(0x10)");
    COMPARE(vslei_d(vr13, vr14, 15),
            "7283bdcd       vslei.d      vr13, vr14, 15(0xf)");
    COMPARE(vslei_d(vr13, vr14, -16),
            "7283c1cd       vslei.d      vr13, vr14, -16(0x10)");
    COMPARE(vslei_bu(vr13, vr14, 15),
            "72843dcd       vslei.bu     vr13, vr14, 15");
    COMPARE(vslei_bu(vr13, vr14, 17),
            "728445cd       vslei.bu     vr13, vr14, 17");
    COMPARE(vslei_hu(vr13, vr14, 15),
            "7284bdcd       vslei.hu     vr13, vr14, 15");
    COMPARE(vslei_hu(vr13, vr14, 17),
            "7284c5cd       vslei.hu     vr13, vr14, 17");
    COMPARE(vslei_wu(vr13, vr14, 15),
            "72853dcd       vslei.wu     vr13, vr14, 15");
    COMPARE(vslei_wu(vr13, vr14, 17),
            "728545cd       vslei.wu     vr13, vr14, 17");
    COMPARE(vslei_du(vr13, vr14, 15),
            "7285bdcd       vslei.du     vr13, vr14, 15");
    COMPARE(vslei_du(vr13, vr14, 17),
            "7285c5cd       vslei.du     vr13, vr14, 17");
    COMPARE(vslti_b(vr13, vr14, 15),
            "72863dcd       vslti.b      vr13, vr14, 15(0xf)");
    COMPARE(vslti_b(vr13, vr14, -16),
            "728641cd       vslti.b      vr13, vr14, -16(0x10)");
    COMPARE(vslti_h(vr13, vr14, 15),
            "7286bdcd       vslti.h      vr13, vr14, 15(0xf)");
    COMPARE(vslti_h(vr13, vr14, -16),
            "7286c1cd       vslti.h      vr13, vr14, -16(0x10)");
    COMPARE(vslti_w(vr13, vr14, 15),
            "72873dcd       vslti.w      vr13, vr14, 15(0xf)");
    COMPARE(vslti_w(vr13, vr14, -16),
            "728741cd       vslti.w      vr13, vr14, -16(0x10)");
    COMPARE(vslti_d(vr13, vr14, 15),
            "7287bdcd       vslti.d      vr13, vr14, 15(0xf)");
    COMPARE(vslti_d(vr13, vr14, -16),
            "7287c1cd       vslti.d      vr13, vr14, -16(0x10)");
    COMPARE(vslti_bu(vr13, vr14, 17),
            "728845cd       vslti.bu     vr13, vr14, 17");
    COMPARE(vslti_hu(vr13, vr14, 17),
            "7288c5cd       vslti.hu     vr13, vr14, 17");
    COMPARE(vslti_wu(vr13, vr14, 17),
            "728945cd       vslti.wu     vr13, vr14, 17");
    COMPARE(vslti_du(vr13, vr14, 17),
            "7289c5cd       vslti.du     vr13, vr14, 17");
    COMPARE(vaddi_bu(vr16, vr17, 17),
            "728a4630       vaddi.bu     vr16, vr17, 17");
    COMPARE(vaddi_hu(vr16, vr17, 18),
            "728aca30       vaddi.hu     vr16, vr17, 18");
    COMPARE(vaddi_wu(vr16, vr17, 18),
            "728b4a30       vaddi.wu     vr16, vr17, 18");
    COMPARE(vaddi_du(vr16, vr17, 18),
            "728bca30       vaddi.du     vr16, vr17, 18");
    COMPARE(vsubi_bu(vr16, vr17, 18),
            "728c4a30       vsubi.bu     vr16, vr17, 18");
    COMPARE(vsubi_hu(vr16, vr17, 18),
            "728cca30       vsubi.hu     vr16, vr17, 18");
    COMPARE(vsubi_wu(vr16, vr17, 18),
            "728d4a30       vsubi.wu     vr16, vr17, 18");
    COMPARE(vsubi_du(vr16, vr17, 18),
            "728dca30       vsubi.du     vr16, vr17, 18");
    COMPARE(vbsll_v(vr16, vr17, 18),
            "728e4a30       vbsll.v      vr16, vr17, 18");
    COMPARE(vbsrl_v(vr16, vr17, 18),
            "728eca30       vbsrl.v      vr16, vr17, 18");
    COMPARE(vmaxi_b(vr19, vr20, 15),
            "72903e93       vmaxi.b      vr19, vr20, 15(0xf)");
    COMPARE(vmaxi_b(vr19, vr20, -16),
            "72904293       vmaxi.b      vr19, vr20, -16(0x10)");
    COMPARE(vmaxi_h(vr19, vr20, 15),
            "7290be93       vmaxi.h      vr19, vr20, 15(0xf)");
    COMPARE(vmaxi_h(vr19, vr20, -16),
            "7290c293       vmaxi.h      vr19, vr20, -16(0x10)");
    COMPARE(vmaxi_w(vr19, vr20, 15),
            "72913e93       vmaxi.w      vr19, vr20, 15(0xf)");
    COMPARE(vmaxi_w(vr19, vr20, -16),
            "72914293       vmaxi.w      vr19, vr20, -16(0x10)");
    COMPARE(vmaxi_d(vr19, vr20, 15),
            "7291be93       vmaxi.d      vr19, vr20, 15(0xf)");
    COMPARE(vmaxi_d(vr19, vr20, -16),
            "7291c293       vmaxi.d      vr19, vr20, -16(0x10)");
    COMPARE(vmini_b(vr19, vr20, 15),
            "72923e93       vmini.b      vr19, vr20, 15(0xf)");
    COMPARE(vmini_b(vr19, vr20, -16),
            "72924293       vmini.b      vr19, vr20, -16(0x10)");
    COMPARE(vmini_h(vr19, vr20, 15),
            "7292be93       vmini.h      vr19, vr20, 15(0xf)");
    COMPARE(vmini_h(vr19, vr20, -16),
            "7292c293       vmini.h      vr19, vr20, -16(0x10)");
    COMPARE(vmini_w(vr19, vr20, 15),
            "72933e93       vmini.w      vr19, vr20, 15(0xf)");
    COMPARE(vmini_w(vr19, vr20, -16),
            "72934293       vmini.w      vr19, vr20, -16(0x10)");
    COMPARE(vmini_d(vr19, vr20, 15),
            "7293be93       vmini.d      vr19, vr20, 15(0xf)");
    COMPARE(vmini_d(vr19, vr20, -16),
            "7293c293       vmini.d      vr19, vr20, -16(0x10)");
    COMPARE(vmaxi_bu(vr19, vr20, 20),
            "72945293       vmaxi.bu     vr19, vr20, 20");
    COMPARE(vmaxi_hu(vr19, vr20, 21),
            "7294d693       vmaxi.hu     vr19, vr20, 21");
    COMPARE(vmaxi_wu(vr19, vr20, 22),
            "72955a93       vmaxi.wu     vr19, vr20, 22");
    COMPARE(vmaxi_du(vr19, vr20, 23),
            "7295de93       vmaxi.du     vr19, vr20, 23");
    COMPARE(vmini_bu(vr19, vr20, 24),
            "72966293       vmini.bu     vr19, vr20, 24");
    COMPARE(vmini_hu(vr19, vr20, 25),
            "7296e693       vmini.hu     vr19, vr20, 25");
    COMPARE(vmini_wu(vr19, vr20, 17),
            "72974693       vmini.wu     vr19, vr20, 17");
    COMPARE(vmini_du(vr19, vr20, 17),
            "7297c693       vmini.du     vr19, vr20, 17");
    COMPARE(vfrstpi_b(vr19, vr20, 18),
            "729a4a93       vfrstpi.b    vr19, vr20, 18");
    COMPARE(vfrstpi_h(vr19, vr20, 18),
            "729aca93       vfrstpi.h    vr19, vr20, 18");
    COMPARE(vrotri_w(vr4, vr5, 31), "72a0fca4       vrotri.w     vr4, vr5, 31");
    COMPARE(vsrlri_w(vr4, vr5, 31), "72a4fca4       vsrlri.w     vr4, vr5, 31");
    COMPARE(vsrari_w(vr4, vr5, 31), "72a8fca4       vsrari.w     vr4, vr5, 31");
    COMPARE(vsllwil_d_w(vr6, vr7, 31),
            "7308fce6       vsllwil.d.w  vr6, vr7, 31");
    COMPARE(vsllwil_du_wu(vr6, vr7, 31),
            "730cfce6       vsllwil.du.wu vr6, vr7, 31");
    COMPARE(vbitclri_w(vr8, vr9, 31),
            "7310fd28       vbitclri.w   vr8, vr9, 31");
    COMPARE(vbitseti_w(vr8, vr9, 31),
            "7314fd28       vbitseti.w   vr8, vr9, 31");
    COMPARE(vbitrevi_w(vr8, vr9, 31),
            "7318fd28       vbitrevi.w   vr8, vr9, 31");
    COMPARE(vsat_w(vr10, vr11, 31),
            "7324fd6a       vsat.w       vr10, vr11, 31");
    COMPARE(vsat_wu(vr10, vr11, 31),
            "7328fd6a       vsat.wu      vr10, vr11, 31");
    COMPARE(vslli_w(vr10, vr11, 31),
            "732cfd6a       vslli.w      vr10, vr11, 31");
    COMPARE(vsrli_w(vr10, vr11, 31),
            "7330fd6a       vsrli.w      vr10, vr11, 31");
    COMPARE(vsrai_w(vr10, vr11, 31),
            "7334fd6a       vsrai.w      vr10, vr11, 31");
    COMPARE(vsrlni_h_w(vr12, vr13, 31),
            "7340fdac       vsrlni.h.w   vr12, vr13, 31");
    COMPARE(vsrlrni_h_w(vr12, vr13, 31),
            "7344fdac       vsrlrni.h.w  vr12, vr13, 31");
    COMPARE(vssrlni_h_w(vr12, vr13, 31),
            "7348fdac       vssrlni.h.w  vr12, vr13, 31");
    COMPARE(vssrlni_hu_w(vr12, vr13, 31),
            "734cfdac       vssrlni.hu.w vr12, vr13, 31");
    COMPARE(vssrlrni_h_w(vr12, vr13, 31),
            "7350fdac       vssrlrni.h.w vr12, vr13, 31");
    COMPARE(vssrlrni_hu_w(vr12, vr13, 31),
            "7354fdac       vssrlrni.hu.w vr12, vr13, 31");
    COMPARE(vsrani_h_w(vr14, vr15, 31),
            "7358fdee       vsrani.h.w   vr14, vr15, 31");
    COMPARE(vsrarni_h_w(vr14, vr15, 31),
            "735cfdee       vsrarni.h.w  vr14, vr15, 31");
    COMPARE(vssrani_h_w(vr14, vr15, 31),
            "7360fdee       vssrani.h.w  vr14, vr15, 31");
    COMPARE(vssrani_hu_w(vr14, vr15, 31),
            "7364fdee       vssrani.hu.w vr14, vr15, 31");
    COMPARE(vssrarni_h_w(vr14, vr15, 31),
            "7368fdee       vssrarni.h.w vr14, vr15, 31");
    COMPARE(vssrarni_hu_w(vr14, vr15, 31),
            "736cfdee       vssrarni.hu.w vr14, vr15, 31");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp18_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vrotri_h(vr4, vr5, 15), "72a07ca4       vrotri.h     vr4, vr5, 15");
    COMPARE(vsrlri_h(vr4, vr5, 15), "72a47ca4       vsrlri.h     vr4, vr5, 15");
    COMPARE(vsrari_h(vr4, vr5, 15), "72a87ca4       vsrari.h     vr4, vr5, 15");
    COMPARE(vinsgr2vr_b(vr6, a7, 15),
            "72ebbd66       vinsgr2vr.b  vr6, a7, 15");
    COMPARE(vpickve2gr_b(a6, vr7, 15),
            "72efbcea       vpickve2gr.b a6, vr7, 15");
    COMPARE(vpickve2gr_bu(a6, vr7, 15),
            "72f3bcea       vpickve2gr.bu a6, vr7, 15");
    COMPARE(vreplvei_b(vr6, vr7, 15),
            "72f7bce6       vreplvei.b   vr6, vr7, 15");
    COMPARE(vsllwil_w_h(vr6, vr7, 15),
            "73087ce6       vsllwil.w.h  vr6, vr7, 15");
    COMPARE(vsllwil_wu_hu(vr6, vr7, 15),
            "730c7ce6       vsllwil.wu.hu vr6, vr7, 15");
    COMPARE(vbitclri_h(vr8, vr9, 15),
            "73107d28       vbitclri.h   vr8, vr9, 15");
    COMPARE(vbitseti_h(vr8, vr9, 15),
            "73147d28       vbitseti.h   vr8, vr9, 15");
    COMPARE(vbitrevi_h(vr8, vr9, 15),
            "73187d28       vbitrevi.h   vr8, vr9, 15");
    COMPARE(vsat_h(vr10, vr11, 15),
            "73247d6a       vsat.h       vr10, vr11, 15");
    COMPARE(vsat_hu(vr10, vr11, 15),
            "73287d6a       vsat.hu      vr10, vr11, 15");
    COMPARE(vslli_h(vr10, vr11, 15),
            "732c7d6a       vslli.h      vr10, vr11, 15");
    COMPARE(vsrli_h(vr10, vr11, 15),
            "73307d6a       vsrli.h      vr10, vr11, 15");
    COMPARE(vsrai_h(vr10, vr11, 15),
            "73347d6a       vsrai.h      vr10, vr11, 15");
    COMPARE(vsrlni_b_h(vr12, vr13, 15),
            "73407dac       vsrlni.b.h   vr12, vr13, 15");
    COMPARE(vsrlrni_b_h(vr12, vr13, 15),
            "73447dac       vsrlrni.b.h  vr12, vr13, 15");
    COMPARE(vssrlni_b_h(vr12, vr13, 15),
            "73487dac       vssrlni.b.h  vr12, vr13, 15");
    COMPARE(vssrlni_bu_h(vr12, vr13, 15),
            "734c7dac       vssrlni.bu.h vr12, vr13, 15");
    COMPARE(vssrlrni_b_h(vr12, vr13, 15),
            "73507dac       vssrlrni.b.h vr12, vr13, 15");
    COMPARE(vssrlrni_bu_h(vr12, vr13, 15),
            "73547dac       vssrlrni.bu.h vr12, vr13, 15");
    COMPARE(vsrani_b_h(vr14, vr15, 15),
            "73587dee       vsrani.b.h   vr14, vr15, 15");
    COMPARE(vsrarni_b_h(vr14, vr15, 15),
            "735c7dee       vsrarni.b.h  vr14, vr15, 15");
    COMPARE(vssrani_b_h(vr14, vr15, 15),
            "73607dee       vssrani.b.h  vr14, vr15, 15");
    COMPARE(vssrani_bu_h(vr14, vr15, 15),
            "73647dee       vssrani.bu.h vr14, vr15, 15");
    COMPARE(vssrarni_b_h(vr14, vr15, 15),
            "73687dee       vssrarni.b.h vr14, vr15, 15");
    COMPARE(vssrarni_bu_h(vr14, vr15, 15),
            "736c7dee       vssrarni.bu.h vr14, vr15, 15");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp19_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vrotri_b(vr4, vr5, 7), "72a03ca4       vrotri.b     vr4, vr5, 7");
    COMPARE(vsrlri_b(vr4, vr5, 7), "72a43ca4       vsrlri.b     vr4, vr5, 7");
    COMPARE(vsrari_b(vr4, vr5, 7), "72a83ca4       vsrari.b     vr4, vr5, 7");
    COMPARE(vinsgr2vr_h(vr6, a7, 7), "72ebdd66       vinsgr2vr.h  vr6, a7, 7");
    COMPARE(vpickve2gr_h(a6, vr7, 7), "72efdcea       vpickve2gr.h a6, vr7, 7");
    COMPARE(vpickve2gr_hu(a6, vr7, 7),
            "72f3dcea       vpickve2gr.hu a6, vr7, 7");
    COMPARE(vreplvei_h(vr6, vr7, 7), "72f7dce6       vreplvei.h   vr6, vr7, 7");
    COMPARE(vsllwil_h_b(vr6, vr7, 7),
            "73083ce6       vsllwil.h.b  vr6, vr7, 7");
    COMPARE(vsllwil_hu_bu(vr6, vr7, 7),
            "730c3ce6       vsllwil.hu.bu vr6, vr7, 7");
    COMPARE(vbitclri_b(vr8, vr9, 7), "73103d28       vbitclri.b   vr8, vr9, 7");
    COMPARE(vbitseti_b(vr8, vr9, 7), "73143d28       vbitseti.b   vr8, vr9, 7");
    COMPARE(vbitrevi_b(vr8, vr9, 7), "73183d28       vbitrevi.b   vr8, vr9, 7");
    COMPARE(vsat_b(vr10, vr11, 7), "73243d6a       vsat.b       vr10, vr11, 7");
    COMPARE(vsat_bu(vr10, vr11, 7),
            "73283d6a       vsat.bu      vr10, vr11, 7");
    COMPARE(vslli_b(vr10, vr11, 7),
            "732c3d6a       vslli.b      vr10, vr11, 7");
    COMPARE(vsrli_b(vr10, vr11, 7),
            "73303d6a       vsrli.b      vr10, vr11, 7");
    COMPARE(vsrai_b(vr10, vr11, 7),
            "73343d6a       vsrai.b      vr10, vr11, 7");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp20_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vinsgr2vr_w(vr6, a7, 3), "72ebed66       vinsgr2vr.w  vr6, a7, 3");
    COMPARE(vpickve2gr_w(a6, vr7, 3), "72efecea       vpickve2gr.w a6, vr7, 3");
    COMPARE(vpickve2gr_wu(a6, vr7, 3),
            "72f3ecea       vpickve2gr.wu a6, vr7, 3");
    COMPARE(vreplvei_w(vr6, vr7, 3), "72f7ece6       vreplvei.w   vr6, vr7, 3");
  }
  VERIFY_RUN();
}

TEST_F(DisasmLoong64Test, TypeOp21_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vinsgr2vr_d(vr6, a7, 1), "72ebf566       vinsgr2vr.d  vr6, a7, 1");
    COMPARE(vpickve2gr_d(a6, vr7, 1), "72eff4ea       vpickve2gr.d a6, vr7, 1");
    COMPARE(vpickve2gr_du(a6, vr7, 1),
            "72f3f4ea       vpickve2gr.du a6, vr7, 1");
    COMPARE(vreplvei_d(vr6, vr7, 1), "72f7f4e6       vreplvei.d   vr6, vr7, 1");
  }
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

TEST_F(DisasmLoong64Test, TypeOp22_LSX) {
  SET_UP();
  if (CpuFeatures::IsSupported(LSX)) {
    CpuFeatureScope fscope(&assm, LSX);
    COMPARE(vclo_b(vr21, vr22), "729c02d5       vclo.b       vr21, vr22");
    COMPARE(vclo_h(vr21, vr22), "729c06d5       vclo.h       vr21, vr22");
    COMPARE(vclo_w(vr21, vr22), "729c0ad5       vclo.w       vr21, vr22");
    COMPARE(vclo_d(vr21, vr22), "729c0ed5       vclo.d       vr21, vr22");
    COMPARE(vclz_b(vr21, vr22), "729c12d5       vclz.b       vr21, vr22");
    COMPARE(vclz_h(vr21, vr22), "729c16d5       vclz.h       vr21, vr22");
    COMPARE(vclz_w(vr21, vr22), "729c1ad5       vclz.w       vr21, vr22");
    COMPARE(vclz_d(vr21, vr22), "729c1ed5       vclz.d       vr21, vr22");
    COMPARE(vpcnt_b(vr23, vr24), "729c2317       vpcnt.b      vr23, vr24");
    COMPARE(vpcnt_h(vr23, vr24), "729c2717       vpcnt.h      vr23, vr24");
    COMPARE(vpcnt_w(vr23, vr24), "729c2b17       vpcnt.w      vr23, vr24");
    COMPARE(vpcnt_d(vr23, vr24), "729c2f17       vpcnt.d      vr23, vr24");
    COMPARE(vneg_b(vr23, vr24), "729c3317       vneg.b       vr23, vr24");
    COMPARE(vneg_h(vr23, vr24), "729c3717       vneg.h       vr23, vr24");
    COMPARE(vneg_w(vr23, vr24), "729c3b17       vneg.w       vr23, vr24");
    COMPARE(vneg_d(vr23, vr24), "729c3f17       vneg.d       vr23, vr24");
    COMPARE(vmskltz_b(vr23, vr24), "729c4317       vmskltz.b    vr23, vr24");
    COMPARE(vmskltz_h(vr23, vr24), "729c4717       vmskltz.h    vr23, vr24");
    COMPARE(vmskltz_w(vr23, vr24), "729c4b17       vmskltz.w    vr23, vr24");
    COMPARE(vmskltz_d(vr23, vr24), "729c4f17       vmskltz.d    vr23, vr24");
    COMPARE(vmskgez_b(vr23, vr24), "729c5317       vmskgez.b    vr23, vr24");
    COMPARE(vmsknz_b(vr23, vr24), "729c6317       vmsknz.b     vr23, vr24");
    COMPARE(vflogb_s(vr28, vr29), "729cc7bc       vflogb.s     vr28, vr29");
    COMPARE(vflogb_d(vr28, vr29), "729ccbbc       vflogb.d     vr28, vr29");
    COMPARE(vfclass_s(vr28, vr29), "729cd7bc       vfclass.s    vr28, vr29");
    COMPARE(vfclass_d(vr28, vr29), "729cdbbc       vfclass.d    vr28, vr29");
    COMPARE(vfsqrt_s(vr30, vr31), "729ce7fe       vfsqrt.s     vr30, vr31");
    COMPARE(vfsqrt_d(vr30, vr31), "729cebfe       vfsqrt.d     vr30, vr31");
    COMPARE(vfrecip_s(vr30, vr31), "729cf7fe       vfrecip.s    vr30, vr31");
    COMPARE(vfrecip_d(vr30, vr31), "729cfbfe       vfrecip.d    vr30, vr31");
    COMPARE(vfrsqrt_s(vr30, vr31), "729d07fe       vfrsqrt.s    vr30, vr31");
    COMPARE(vfrsqrt_d(vr30, vr31), "729d0bfe       vfrsqrt.d    vr30, vr31");
    COMPARE(vfrint_s(vr30, vr31), "729d37fe       vfrint.s     vr30, vr31");
    COMPARE(vfrint_d(vr30, vr31), "729d3bfe       vfrint.d     vr30, vr31");
    COMPARE(vfrintrm_s(vr30, vr31), "729d47fe       vfrintrm.s   vr30, vr31");
    COMPARE(vfrintrm_d(vr30, vr31), "729d4bfe       vfrintrm.d   vr30, vr31");
    COMPARE(vfrintrp_s(vr30, vr31), "729d57fe       vfrintrp.s   vr30, vr31");
    COMPARE(vfrintrp_d(vr30, vr31), "729d5bfe       vfrintrp.d   vr30, vr31");
    COMPARE(vfrintrz_s(vr30, vr31), "729d67fe       vfrintrz.s   vr30, vr31");
    COMPARE(vfrintrz_d(vr30, vr31), "729d6bfe       vfrintrz.d   vr30, vr31");
    COMPARE(vfrintrne_s(vr30, vr31), "729d77fe       vfrintrne.s  vr30, vr31");
    COMPARE(vfrintrne_d(vr30, vr31), "729d7bfe       vfrintrne.d  vr30, vr31");
    COMPARE(vfcvtl_s_h(vr0, vr1), "729de820       vfcvtl.s.h   vr0, vr1");
    COMPARE(vfcvth_s_h(vr0, vr1), "729dec20       vfcvth.s.h   vr0, vr1");
    COMPARE(vfcvtl_d_s(vr0, vr1), "729df020       vfcvtl.d.s   vr0, vr1");
    COMPARE(vfcvth_d_s(vr0, vr1), "729df420       vfcvth.d.s   vr0, vr1");
    COMPARE(vffint_s_w(vr0, vr1), "729e0020       vffint.s.w   vr0, vr1");
    COMPARE(vffint_s_wu(vr0, vr1), "729e0420       vffint.s.wu  vr0, vr1");
    COMPARE(vffint_d_l(vr0, vr1), "729e0820       vffint.d.l   vr0, vr1");
    COMPARE(vffint_d_lu(vr0, vr1), "729e0c20       vffint.d.lu  vr0, vr1");
    COMPARE(vffintl_d_w(vr0, vr1), "729e1020       vffintl.d.w  vr0, vr1");
    COMPARE(vffinth_d_w(vr0, vr1), "729e1420       vffinth.d.w  vr0, vr1");
    COMPARE(vftint_w_s(vr2, vr3), "729e3062       vftint.w.s   vr2, vr3");
    COMPARE(vftint_l_d(vr2, vr3), "729e3462       vftint.l.d   vr2, vr3");
    COMPARE(vftintrm_w_s(vr2, vr3), "729e3862       vftintrm.w.s vr2, vr3");
    COMPARE(vftintrm_l_d(vr2, vr3), "729e3c62       vftintrm.l.d vr2, vr3");
    COMPARE(vftintrp_w_s(vr2, vr3), "729e4062       vftintrp.w.s vr2, vr3");
    COMPARE(vftintrp_l_d(vr2, vr3), "729e4462       vftintrp.l.d vr2, vr3");
    COMPARE(vftintrz_w_s(vr2, vr3), "729e4862       vftintrz.w.s vr2, vr3");
    COMPARE(vftintrz_l_d(vr2, vr3), "729e4c62       vftintrz.l.d vr2, vr3");
    COMPARE(vftintrne_w_s(vr2, vr3), "729e5062       vftintrne.w.s vr2, vr3");
    COMPARE(vftintrne_l_d(vr2, vr3), "729e5462       vftintrne.l.d vr2, vr3");
    COMPARE(vftint_wu_s(vr2, vr3), "729e5862       vftint.wu.s  vr2, vr3");
    COMPARE(vftint_lu_d(vr2, vr3), "729e5c62       vftint.lu.d  vr2, vr3");
    COMPARE(vftintrz_wu_s(vr2, vr3), "729e7062       vftintrz.wu.s vr2, vr3");
    COMPARE(vftintrz_lu_d(vr2, vr3), "729e7462       vftintrz.lu.d vr2, vr3");
    COMPARE(vftintl_l_s(vr2, vr3), "729e8062       vftintl.l.s  vr2, vr3");
    COMPARE(vftinth_l_s(vr2, vr3), "729e8462       vftinth.l.s  vr2, vr3");
    COMPARE(vftintrml_l_s(vr2, vr3), "729e8862       vftintrml.l.s vr2, vr3");
    COMPARE(vftintrmh_l_s(vr2, vr3), "729e8c62       vftintrmh.l.s vr2, vr3");
    COMPARE(vftintrpl_l_s(vr2, vr3), "729e9062       vftintrpl.l.s vr2, vr3");
    COMPARE(vftintrph_l_s(vr2, vr3), "729e9462       vftintrph.l.s vr2, vr3");
    COMPARE(vftintrzl_l_s(vr2, vr3), "729e9862       vftintrzl.l.s vr2, vr3");
    COMPARE(vftintrzh_l_s(vr2, vr3), "729e9c62       vftintrzh.l.s vr2, vr3");
    COMPARE(vftintrnel_l_s(vr2, vr3), "729ea062       vftintrnel.l.s vr2, vr3");
    COMPARE(vftintrneh_l_s(vr2, vr3), "729ea462       vftintrneh.l.s vr2, vr3");
    COMPARE(vexth_h_b(vr4, vr5), "729ee0a4       vexth.h.b    vr4, vr5");
    COMPARE(vexth_w_h(vr4, vr5), "729ee4a4       vexth.w.h    vr4, vr5");
    COMPARE(vexth_d_w(vr4, vr5), "729ee8a4       vexth.d.w    vr4, vr5");
    COMPARE(vexth_q_d(vr4, vr5), "729eeca4       vexth.q.d    vr4, vr5");
    COMPARE(vexth_hu_bu(vr4, vr5), "729ef0a4       vexth.hu.bu  vr4, vr5");
    COMPARE(vexth_wu_hu(vr4, vr5), "729ef4a4       vexth.wu.hu  vr4, vr5");
    COMPARE(vexth_du_wu(vr4, vr5), "729ef8a4       vexth.du.wu  vr4, vr5");
    COMPARE(vexth_qu_du(vr4, vr5), "729efca4       vexth.qu.du  vr4, vr5");
    COMPARE(vreplgr2vr_b(vr4, a5), "729f0124       vreplgr2vr.b vr4, a5");
    COMPARE(vreplgr2vr_h(vr4, a5), "729f0524       vreplgr2vr.h vr4, a5");
    COMPARE(vreplgr2vr_w(vr4, a5), "729f0924       vreplgr2vr.w vr4, a5");
    COMPARE(vreplgr2vr_d(vr4, a5), "729f0d24       vreplgr2vr.d vr4, a5");
    COMPARE(vextl_q_d(vr8, vr9), "73090128       vextl.q.d    vr8, vr9");
    COMPARE(vextl_qu_du(vr8, vr9), "730d0128       vextl.qu.du  vr8, vr9");
    COMPARE(vseteqz_v(FCC0, vr25), "729c9b20       vseteqz.v    fcc0, vr25");
    COMPARE(vsetnez_v(FCC0, vr25), "729c9f20       vsetnez.v    fcc0, vr25");
    COMPARE(vsetanyeqz_b(FCC1, vr26), "729ca341       vsetanyeqz.b fcc1, vr26");
    COMPARE(vsetanyeqz_h(FCC1, vr26), "729ca741       vsetanyeqz.h fcc1, vr26");
    COMPARE(vsetanyeqz_w(FCC1, vr26), "729cab41       vsetanyeqz.w fcc1, vr26");
    COMPARE(vsetanyeqz_d(FCC1, vr26), "729caf41       vsetanyeqz.d fcc1, vr26");
    COMPARE(vsetallnez_b(FCC2, vr27), "729cb362       vsetallnez.b fcc2, vr27");
    COMPARE(vsetallnez_h(FCC2, vr27), "729cb762       vsetallnez.h fcc2, vr27");
    COMPARE(vsetallnez_w(FCC2, vr27), "729cbb62       vsetallnez.w fcc2, vr27");
    COMPARE(vsetallnez_d(FCC2, vr27), "729cbf62       vsetallnez.d fcc2, vr27");
  }
  VERIFY_RUN();
}

}  // namespace internal
}  // namespace v8
