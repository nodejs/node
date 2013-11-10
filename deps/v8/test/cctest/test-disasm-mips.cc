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

#include "v8.h"

#include "debug.h"
#include "disasm.h"
#include "disassembler.h"
#include "macro-assembler.h"
#include "serialize.h"
#include "cctest.h"

using namespace v8::internal;


bool DisassembleAndCompare(byte* pc, const char* compare_string) {
  disasm::NameConverter converter;
  disasm::Disassembler disasm(converter);
  EmbeddedVector<char, 128> disasm_buffer;

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

  if (kArchVariant != kLoongson) {
    COMPARE(mul(a0, a1, a2),
            "70a62002       mul     a0, a1, a2");
    COMPARE(mul(t2, t3, t4),
            "716c5002       mul     t2, t3, t4");
    COMPARE(mul(v0, v1, s0),
            "70701002       mul     v0, v1, s0");
  }

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

  if (kArchVariant == kMips32r2) {
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

  if (kArchVariant != kLoongson) {
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

    COMPARE(clz(a0, a1),
            "70a42020       clz     a0, a1");
    COMPARE(clz(s6, s7),
            "72f6b020       clz     s6, s7");
    COMPARE(clz(v0, v1),
            "70621020       clz     v0, v1");
  }

  if (kArchVariant == kMips32r2) {
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

  VERIFY_RUN();
}
