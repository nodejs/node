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


static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}


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


// Setup V8 to a state where we can at least run the assembler and
// disassembler. Declare the variables and allocate the data structures used
// in the rest of the macros.
#define SETUP() \
  InitializeVM(); \
  v8::HandleScope scope; \
  byte *buffer = reinterpret_cast<byte*>(malloc(4*1024)); \
  Assembler assm(buffer, 4*1024); \
  bool failure = false;


// This macro assembles one instruction using the preallocated assembler and
// disassembles the generated instruction, comparing the output to the expected
// value. If the comparison fails an error message is printed, but the test
// continues to run until the end.
#define COMPARE(asm_, compare_string) \
  { \
    int pc_offset = assm.pc_offset(); \
    byte *pc = &buffer[pc_offset]; \
    assm.asm_; \
    if (!DisassembleAndCompare(pc, compare_string)) failure = true; \
  }


// Verify that all invocations of the COMPARE macro passed successfully.
// Exit with a failure if at least one of the tests failed.
#define VERIFY_RUN() \
if (failure) { \
    V8_Fatal(__FILE__, __LINE__, "ARM Disassembler tests failed.\n"); \
  }


TEST(Type0) {
  SETUP();

  COMPARE(and_(r0, r1, Operand(r2)),
          "e0010002       and r0, r1, r2");
  COMPARE(and_(r1, r2, Operand(r3), LeaveCC),
          "e0021003       and r1, r2, r3");
  COMPARE(and_(r2, r3, Operand(r4), SetCC),
          "e0132004       ands r2, r3, r4");
  COMPARE(and_(r3, r4, Operand(r5), LeaveCC, eq),
          "00043005       andeq r3, r4, r5");

  COMPARE(eor(r4, r5, Operand(r6, LSL, 0)),
          "e0254006       eor r4, r5, r6");
  COMPARE(eor(r4, r5, Operand(r7, LSL, 1), SetCC),
          "e0354087       eors r4, r5, r7, lsl #1");
  COMPARE(eor(r4, r5, Operand(r8, LSL, 2), LeaveCC, ne),
          "10254108       eorne r4, r5, r8, lsl #2");
  COMPARE(eor(r4, r5, Operand(r9, LSL, 3), SetCC, cs),
          "20354189       eorcss r4, r5, r9, lsl #3");

  COMPARE(sub(r5, r6, Operand(r10, LSL, 31), LeaveCC, hs),
          "20465f8a       subcs r5, r6, r10, lsl #31");
  COMPARE(sub(r5, r6, Operand(r10, LSL, 30), SetCC, cc),
          "30565f0a       subccs r5, r6, r10, lsl #30");
  COMPARE(sub(r5, r6, Operand(r10, LSL, 24), LeaveCC, lo),
          "30465c0a       subcc r5, r6, r10, lsl #24");
  COMPARE(sub(r5, r6, Operand(r10, LSL, 16), SetCC, mi),
          "4056580a       submis r5, r6, r10, lsl #16");

  COMPARE(rsb(r6, r7, Operand(fp)),
          "e067600b       rsb r6, r7, fp");
  COMPARE(rsb(r6, r7, Operand(fp, LSR, 1)),
          "e06760ab       rsb r6, r7, fp, lsr #1");
  COMPARE(rsb(r6, r7, Operand(fp, LSR, 0), SetCC),
          "e077602b       rsbs r6, r7, fp, lsr #32");
  COMPARE(rsb(r6, r7, Operand(fp, LSR, 31), LeaveCC, pl),
          "50676fab       rsbpl r6, r7, fp, lsr #31");

  COMPARE(add(r7, r8, Operand(ip, ASR, 1)),
          "e08870cc       add r7, r8, ip, asr #1");
  COMPARE(add(r7, r8, Operand(ip, ASR, 0)),
          "e088704c       add r7, r8, ip, asr #32");
  COMPARE(add(r7, r8, Operand(ip), SetCC),
          "e098700c       adds r7, r8, ip");
  COMPARE(add(r7, r8, Operand(ip, ASR, 31), SetCC, vs),
          "60987fcc       addvss r7, r8, ip, asr #31");

  COMPARE(adc(r7, fp, Operand(ip, ASR, 5)),
          "e0ab72cc       adc r7, fp, ip, asr #5");
  COMPARE(adc(r4, ip, Operand(ip, ASR, 1), LeaveCC, vc),
          "70ac40cc       adcvc r4, ip, ip, asr #1");
  COMPARE(adc(r5, sp, Operand(ip), SetCC),
          "e0bd500c       adcs r5, sp, ip");
  COMPARE(adc(r8, lr, Operand(ip, ASR, 31), SetCC, vc),
          "70be8fcc       adcvcs r8, lr, ip, asr #31");

  COMPARE(sbc(r7, r1, Operand(ip, ROR, 1), LeaveCC, hi),
          "80c170ec       sbchi r7, r1, ip, ror #1");
  COMPARE(sbc(r7, r9, Operand(ip, ROR, 4)),
          "e0c9726c       sbc r7, r9, ip, ror #4");
  COMPARE(sbc(r7, r10, Operand(ip), SetCC),
          "e0da700c       sbcs r7, r10, ip");
  COMPARE(sbc(r7, ip, Operand(ip, ROR, 31), SetCC, hi),
          "80dc7fec       sbchis r7, ip, ip, ror #31");

  COMPARE(rsc(r7, r8, Operand(ip, LSL, r0)),
          "e0e8701c       rsc r7, r8, ip, lsl r0");
  COMPARE(rsc(r7, r8, Operand(ip, LSL, r1)),
          "e0e8711c       rsc r7, r8, ip, lsl r1");
  COMPARE(rsc(r7, r8, Operand(ip), SetCC),
          "e0f8700c       rscs r7, r8, ip");
  COMPARE(rsc(r7, r8, Operand(ip, LSL, r3), SetCC, ls),
          "90f8731c       rsclss r7, r8, ip, lsl r3");

  COMPARE(tst(r7, Operand(r5, ASR, ip), ge),
          "a1170c55       tstge r7, r5, asr ip");
  COMPARE(tst(r7, Operand(r6, ASR, sp)),
          "e1170d56       tst r7, r6, asr sp");
  COMPARE(tst(r7, Operand(r7), ge),
          "a1170007       tstge r7, r7");
  COMPARE(tst(r7, Operand(r8, ASR, fp), ge),
          "a1170b58       tstge r7, r8, asr fp");

  COMPARE(teq(r7, Operand(r5, ROR, r0), lt),
          "b1370075       teqlt r7, r5, ror r0");
  COMPARE(teq(r7, Operand(r6, ROR, lr)),
          "e1370e76       teq r7, r6, ror lr");
  COMPARE(teq(r7, Operand(r7), lt),
          "b1370007       teqlt r7, r7");
  COMPARE(teq(r7, Operand(r8, ROR, r1)),
          "e1370178       teq r7, r8, ror r1");

  COMPARE(cmp(r7, Operand(r4)),
          "e1570004       cmp r7, r4");
  COMPARE(cmp(r7, Operand(r6, LSL, 1), gt),
          "c1570086       cmpgt r7, r6, lsl #1");
  COMPARE(cmp(r7, Operand(r8, LSR, 3), gt),
          "c15701a8       cmpgt r7, r8, lsr #3");
  COMPARE(cmp(r7, Operand(r8, ASR, 19)),
          "e15709c8       cmp r7, r8, asr #19");

  COMPARE(cmn(r0, Operand(r4)),
          "e1700004       cmn r0, r4");
  COMPARE(cmn(r1, Operand(r6, ROR, 1)),
          "e17100e6       cmn r1, r6, ror #1");
  COMPARE(cmn(r2, Operand(r8)),
          "e1720008       cmn r2, r8");
  COMPARE(cmn(r3, Operand(fp), le),
          "d173000b       cmnle r3, fp");

  COMPARE(orr(r7, r8, Operand(lr), LeaveCC, al),
          "e188700e       orr r7, r8, lr");
  COMPARE(orr(r7, r8, Operand(fp)),
          "e188700b       orr r7, r8, fp");
  COMPARE(orr(r7, r8, Operand(sp), SetCC),
          "e198700d       orrs r7, r8, sp");
  COMPARE(orr(r7, r8, Operand(ip), SetCC, al),
          "e198700c       orrs r7, r8, ip");

  COMPARE(mov(r0, Operand(r1), LeaveCC, eq),
          "01a00001       moveq r0, r1");
  COMPARE(mov(r0, Operand(r2)),
          "e1a00002       mov r0, r2");
  COMPARE(mov(r0, Operand(r3), SetCC),
          "e1b00003       movs r0, r3");
  COMPARE(mov(r0, Operand(r4), SetCC, pl),
          "51b00004       movpls r0, r4");

  COMPARE(bic(r0, lr, Operand(r1), LeaveCC, vs),
          "61ce0001       bicvs r0, lr, r1");
  COMPARE(bic(r0, r9, Operand(r2), LeaveCC, vc),
          "71c90002       bicvc r0, r9, r2");
  COMPARE(bic(r0, r5, Operand(r3), SetCC),
          "e1d50003       bics r0, r5, r3");
  COMPARE(bic(r0, r1, Operand(r4), SetCC, pl),
          "51d10004       bicpls r0, r1, r4");

  COMPARE(mvn(r10, Operand(r1)),
          "e1e0a001       mvn r10, r1");
  COMPARE(mvn(r9, Operand(r2)),
          "e1e09002       mvn r9, r2");
  COMPARE(mvn(r0, Operand(r3), SetCC),
          "e1f00003       mvns r0, r3");
  COMPARE(mvn(r5, Operand(r4), SetCC, cc),
          "31f05004       mvnccs r5, r4");

  // Instructions autotransformed by the assembler.
  // mov -> mvn.
  COMPARE(mov(r3, Operand(-1), LeaveCC, al),
          "e3e03000       mvn r3, #0");
  COMPARE(mov(r4, Operand(-2), SetCC, al),
          "e3f04001       mvns r4, #1");
  COMPARE(mov(r5, Operand(0x0ffffff0), SetCC, ne),
          "13f052ff       mvnnes r5, #-268435441");
  COMPARE(mov(r6, Operand(-1), LeaveCC, ne),
          "13e06000       mvnne r6, #0");

  // mvn -> mov.
  COMPARE(mvn(r3, Operand(-1), LeaveCC, al),
          "e3a03000       mov r3, #0");
  COMPARE(mvn(r4, Operand(-2), SetCC, al),
          "e3b04001       movs r4, #1");
  COMPARE(mvn(r5, Operand(0x0ffffff0), SetCC, ne),
          "13b052ff       movnes r5, #-268435441");
  COMPARE(mvn(r6, Operand(-1), LeaveCC, ne),
          "13a06000       movne r6, #0");

  // mov -> movw.
  if (CpuFeatures::IsSupported(ARMv7)) {
    COMPARE(mov(r5, Operand(0x01234), LeaveCC, ne),
            "13015234       movwne r5, #4660");
    // We only disassemble one instruction so the eor instruction is not here.
    COMPARE(eor(r5, r4, Operand(0x1234), LeaveCC, ne),
            "1301c234       movwne ip, #4660");
    // Movw can't do setcc so we don't get that here.  Mov immediate with setcc
    // is pretty strange anyway.
    COMPARE(mov(r5, Operand(0x01234), SetCC, ne),
            "159fc000       ldrne ip, [pc, #+0]");
    // We only disassemble one instruction so the eor instruction is not here.
    // The eor does the setcc so we get a movw here.
    COMPARE(eor(r5, r4, Operand(0x1234), SetCC, ne),
            "1301c234       movwne ip, #4660");

    COMPARE(movt(r5, 0x4321, ne),
            "13445321       movtne r5, #17185");
    COMPARE(movw(r5, 0xabcd, eq),
            "030a5bcd       movweq r5, #43981");
  }

  // Eor doesn't have an eor-negative variant, but we can do an mvn followed by
  // an eor to get the same effect.
  COMPARE(eor(r5, r4, Operand(0xffffff34), SetCC, ne),
          "13e0c0cb       mvnne ip, #203");

  // and <-> bic.
  COMPARE(and_(r3, r5, Operand(0xfc03ffff)),
          "e3c537ff       bic r3, r5, #66846720");
  COMPARE(bic(r3, r5, Operand(0xfc03ffff)),
          "e20537ff       and r3, r5, #66846720");

  // sub <-> add.
  COMPARE(add(r3, r5, Operand(-1024)),
          "e2453b01       sub r3, r5, #1024");
  COMPARE(sub(r3, r5, Operand(-1024)),
          "e2853b01       add r3, r5, #1024");

  // cmp <-> cmn.
  COMPARE(cmp(r3, Operand(-1024)),
          "e3730b01       cmn r3, #1024");
  COMPARE(cmn(r3, Operand(-1024)),
          "e3530b01       cmp r3, #1024");

  // Miscellaneous instructions encoded as type 0.
  COMPARE(blx(ip),
          "e12fff3c       blx ip");
  COMPARE(bkpt(0),
          "e1200070       bkpt 0");
  COMPARE(bkpt(0xffff),
          "e12fff7f       bkpt 65535");
  COMPARE(clz(r6, r7),
          "e16f6f17       clz r6, r7");

  VERIFY_RUN();
}


TEST(Type1) {
  SETUP();

  COMPARE(and_(r0, r1, Operand(0x00000000)),
          "e2010000       and r0, r1, #0");
  COMPARE(and_(r1, r2, Operand(0x00000001), LeaveCC),
          "e2021001       and r1, r2, #1");
  COMPARE(and_(r2, r3, Operand(0x00000010), SetCC),
          "e2132010       ands r2, r3, #16");
  COMPARE(and_(r3, r4, Operand(0x00000100), LeaveCC, eq),
          "02043c01       andeq r3, r4, #256");
  COMPARE(and_(r4, r5, Operand(0x00001000), SetCC, ne),
          "12154a01       andnes r4, r5, #4096");

  COMPARE(eor(r4, r5, Operand(0x00001000)),
          "e2254a01       eor r4, r5, #4096");
  COMPARE(eor(r4, r4, Operand(0x00010000), LeaveCC),
          "e2244801       eor r4, r4, #65536");
  COMPARE(eor(r4, r3, Operand(0x00100000), SetCC),
          "e2334601       eors r4, r3, #1048576");
  COMPARE(eor(r4, r2, Operand(0x01000000), LeaveCC, cs),
          "22224401       eorcs r4, r2, #16777216");
  COMPARE(eor(r4, r1, Operand(0x10000000), SetCC, cc),
          "32314201       eorccs r4, r1, #268435456");

  VERIFY_RUN();
}


TEST(Type3) {
  SETUP();

  if (CpuFeatures::IsSupported(ARMv7)) {
    COMPARE(ubfx(r0, r1, 5, 10),
            "e7e902d1       ubfx r0, r1, #5, #10");
    COMPARE(ubfx(r1, r0, 5, 10),
            "e7e912d0       ubfx r1, r0, #5, #10");
    COMPARE(ubfx(r0, r1, 31, 1),
            "e7e00fd1       ubfx r0, r1, #31, #1");
    COMPARE(ubfx(r1, r0, 31, 1),
            "e7e01fd0       ubfx r1, r0, #31, #1");

    COMPARE(sbfx(r0, r1, 5, 10),
            "e7a902d1       sbfx r0, r1, #5, #10");
    COMPARE(sbfx(r1, r0, 5, 10),
            "e7a912d0       sbfx r1, r0, #5, #10");
    COMPARE(sbfx(r0, r1, 31, 1),
            "e7a00fd1       sbfx r0, r1, #31, #1");
    COMPARE(sbfx(r1, r0, 31, 1),
            "e7a01fd0       sbfx r1, r0, #31, #1");

    COMPARE(bfc(r0, 5, 10),
            "e7ce029f       bfc r0, #5, #10");
    COMPARE(bfc(r1, 5, 10),
            "e7ce129f       bfc r1, #5, #10");
    COMPARE(bfc(r0, 31, 1),
            "e7df0f9f       bfc r0, #31, #1");
    COMPARE(bfc(r1, 31, 1),
            "e7df1f9f       bfc r1, #31, #1");

    COMPARE(bfi(r0, r1, 5, 10),
            "e7ce0291       bfi r0, r1, #5, #10");
    COMPARE(bfi(r1, r0, 5, 10),
            "e7ce1290       bfi r1, r0, #5, #10");
    COMPARE(bfi(r0, r1, 31, 1),
            "e7df0f91       bfi r0, r1, #31, #1");
    COMPARE(bfi(r1, r0, 31, 1),
            "e7df1f90       bfi r1, r0, #31, #1");
  }

  VERIFY_RUN();
}



TEST(Vfp) {
  SETUP();

  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    COMPARE(vmov(d0, d1),
            "eeb00b41       vmov.f64 d0, d1");
    COMPARE(vmov(d3, d3, eq),
            "0eb03b43       vmov.f64eq d3, d3");

    COMPARE(vadd(d0, d1, d2),
            "ee310b02       vadd.f64 d0, d1, d2");
    COMPARE(vadd(d3, d4, d5, mi),
            "4e343b05       vadd.f64mi d3, d4, d5");

    COMPARE(vsub(d0, d1, d2),
            "ee310b42       vsub.f64 d0, d1, d2");
    COMPARE(vsub(d3, d4, d5, ne),
            "1e343b45       vsub.f64ne d3, d4, d5");

    COMPARE(vmul(d2, d1, d0),
            "ee212b00       vmul.f64 d2, d1, d0");
    COMPARE(vmul(d6, d4, d5, cc),
            "3e246b05       vmul.f64cc d6, d4, d5");

    COMPARE(vdiv(d2, d2, d2),
            "ee822b02       vdiv.f64 d2, d2, d2");
    COMPARE(vdiv(d6, d7, d7, hi),
            "8e876b07       vdiv.f64hi d6, d7, d7");

    COMPARE(vsqrt(d0, d0),
            "eeb10bc0       vsqrt.f64 d0, d0");
    COMPARE(vsqrt(d2, d3, ne),
            "1eb12bc3       vsqrt.f64ne d2, d3");
  }

  VERIFY_RUN();
}
