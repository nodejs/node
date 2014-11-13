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
//

#include <stdlib.h>

#include "src/v8.h"

#include "src/debug.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/macro-assembler.h"
#include "src/serialize.h"
#include "test/cctest/cctest.h"

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

// Force emission of any pending literals into a pool.
#define EMIT_PENDING_LITERALS() \
  assm.CheckConstPool(true, false)


// Verify that all invocations of the COMPARE macro passed successfully.
// Exit with a failure if at least one of the tests failed.
#define VERIFY_RUN() \
if (failure) { \
    V8_Fatal(__FILE__, __LINE__, "ARM Disassembler tests failed.\n"); \
  }


TEST(Type0) {
  SET_UP();

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
    // Movw can't do setcc, so first move to ip, then the following instruction
    // moves to r5.  Mov immediate with setcc is pretty strange anyway.
    COMPARE(mov(r5, Operand(0x01234), SetCC, ne),
            "1301c234       movwne ip, #4660");
    // Emit a literal pool now, otherwise this could be dumped later, in the
    // middle of a different test.
    EMIT_PENDING_LITERALS();

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
  SET_UP();

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
  SET_UP();

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

    COMPARE(usat(r0, 1, Operand(r1)),
            "e6e10011       usat r0, #1, r1");
    COMPARE(usat(r2, 7, Operand(lr)),
            "e6e7201e       usat r2, #7, lr");
    COMPARE(usat(r3, 31, Operand(r4, LSL, 31)),
            "e6ff3f94       usat r3, #31, r4, lsl #31");
    COMPARE(usat(r8, 0, Operand(r5, ASR, 17)),
            "e6e088d5       usat r8, #0, r5, asr #17");

    COMPARE(pkhbt(r3, r4, Operand(r5, LSL, 17)),
            "e6843895       pkhbt r3, r4, r5, lsl #17");
    COMPARE(pkhtb(r3, r4, Operand(r5, ASR, 17)),
            "e68438d5       pkhtb r3, r4, r5, asr #17");
    COMPARE(uxtb(r9, Operand(r10, ROR, 0)),
            "e6ef907a       uxtb r9, r10");
    COMPARE(uxtb(r3, Operand(r4, ROR, 8)),
            "e6ef3474       uxtb r3, r4, ror #8");
    COMPARE(uxtab(r3, r4, Operand(r5, ROR, 8)),
            "e6e43475       uxtab r3, r4, r5, ror #8");
    COMPARE(uxtb16(r3, Operand(r4, ROR, 8)),
            "e6cf3474       uxtb16 r3, r4, ror #8");
  }

  COMPARE(smmla(r0, r1, r2, r3), "e7503211       smmla r0, r1, r2, r3");
  COMPARE(smmla(r10, r9, r8, r7), "e75a7819       smmla r10, r9, r8, r7");

  COMPARE(smmul(r0, r1, r2), "e750f211       smmul r0, r1, r2");
  COMPARE(smmul(r8, r9, r10), "e758fa19       smmul r8, r9, r10");

  VERIFY_RUN();
}



TEST(Vfp) {
  SET_UP();

  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatureScope scope(&assm, VFP3);
    COMPARE(vmov(d0, r2, r3),
            "ec432b10       vmov d0, r2, r3");
    COMPARE(vmov(r2, r3, d0),
            "ec532b10       vmov r2, r3, d0");
    COMPARE(vmov(d0, d1),
            "eeb00b41       vmov.f64 d0, d1");
    COMPARE(vmov(d3, d3, eq),
            "0eb03b43       vmoveq.f64 d3, d3");

    COMPARE(vmov(s0, s31),
            "eeb00a6f       vmov.f32 s0, s31");
    COMPARE(vmov(s31, s0),
            "eef0fa40       vmov.f32 s31, s0");
    COMPARE(vmov(r0, s0),
            "ee100a10       vmov r0, s0");
    COMPARE(vmov(r10, s31),
            "ee1faa90       vmov r10, s31");
    COMPARE(vmov(s0, r0),
            "ee000a10       vmov s0, r0");
    COMPARE(vmov(s31, r10),
            "ee0faa90       vmov s31, r10");

    COMPARE(vabs(d0, d1),
            "eeb00bc1       vabs.f64 d0, d1");
    COMPARE(vabs(d3, d4, mi),
            "4eb03bc4       vabsmi.f64 d3, d4");

    COMPARE(vneg(d0, d1),
            "eeb10b41       vneg.f64 d0, d1");
    COMPARE(vneg(d3, d4, mi),
            "4eb13b44       vnegmi.f64 d3, d4");

    COMPARE(vadd(d0, d1, d2),
            "ee310b02       vadd.f64 d0, d1, d2");
    COMPARE(vadd(d3, d4, d5, mi),
            "4e343b05       vaddmi.f64 d3, d4, d5");

    COMPARE(vsub(d0, d1, d2),
            "ee310b42       vsub.f64 d0, d1, d2");
    COMPARE(vsub(d3, d4, d5, ne),
            "1e343b45       vsubne.f64 d3, d4, d5");

    COMPARE(vmul(d2, d1, d0),
            "ee212b00       vmul.f64 d2, d1, d0");
    COMPARE(vmul(d6, d4, d5, cc),
            "3e246b05       vmulcc.f64 d6, d4, d5");

    COMPARE(vdiv(d2, d2, d2),
            "ee822b02       vdiv.f64 d2, d2, d2");
    COMPARE(vdiv(d6, d7, d7, hi),
            "8e876b07       vdivhi.f64 d6, d7, d7");

    COMPARE(vcmp(d0, d1),
            "eeb40b41       vcmp.f64 d0, d1");
    COMPARE(vcmp(d0, 0.0),
            "eeb50b40       vcmp.f64 d0, #0.0");

    COMPARE(vsqrt(d0, d0),
            "eeb10bc0       vsqrt.f64 d0, d0");
    COMPARE(vsqrt(d2, d3, ne),
            "1eb12bc3       vsqrtne.f64 d2, d3");

    COMPARE(vmov(d0, 1.0),
            "eeb70b00       vmov.f64 d0, #1");
    COMPARE(vmov(d2, -13.0),
            "eeba2b0a       vmov.f64 d2, #-13");

    COMPARE(vmov(d0, VmovIndexLo, r0),
            "ee000b10       vmov.32 d0[0], r0");
    COMPARE(vmov(d0, VmovIndexHi, r0),
            "ee200b10       vmov.32 d0[1], r0");

    COMPARE(vmov(r2, VmovIndexLo, d15),
            "ee1f2b10       vmov.32 r2, d15[0]");
    COMPARE(vmov(r3, VmovIndexHi, d14),
            "ee3e3b10       vmov.32 r3, d14[1]");

    COMPARE(vldr(s0, r0, 0),
            "ed900a00       vldr s0, [r0 + 4*0]");
    COMPARE(vldr(s1, r1, 4),
            "edd10a01       vldr s1, [r1 + 4*1]");
    COMPARE(vldr(s15, r4, 16),
            "edd47a04       vldr s15, [r4 + 4*4]");
    COMPARE(vldr(s16, r5, 20),
            "ed958a05       vldr s16, [r5 + 4*5]");
    COMPARE(vldr(s31, r10, 1020),
            "eddafaff       vldr s31, [r10 + 4*255]");

    COMPARE(vstr(s0, r0, 0),
            "ed800a00       vstr s0, [r0 + 4*0]");
    COMPARE(vstr(s1, r1, 4),
            "edc10a01       vstr s1, [r1 + 4*1]");
    COMPARE(vstr(s15, r8, 8),
            "edc87a02       vstr s15, [r8 + 4*2]");
    COMPARE(vstr(s16, r9, 12),
            "ed898a03       vstr s16, [r9 + 4*3]");
    COMPARE(vstr(s31, r10, 1020),
            "edcafaff       vstr s31, [r10 + 4*255]");

    COMPARE(vldr(d0, r0, 0),
            "ed900b00       vldr d0, [r0 + 4*0]");
    COMPARE(vldr(d1, r1, 4),
            "ed911b01       vldr d1, [r1 + 4*1]");
    COMPARE(vldr(d15, r10, 1020),
            "ed9afbff       vldr d15, [r10 + 4*255]");
    COMPARE(vstr(d0, r0, 0),
            "ed800b00       vstr d0, [r0 + 4*0]");
    COMPARE(vstr(d1, r1, 4),
            "ed811b01       vstr d1, [r1 + 4*1]");
    COMPARE(vstr(d15, r10, 1020),
            "ed8afbff       vstr d15, [r10 + 4*255]");

    COMPARE(vmsr(r5),
            "eee15a10       vmsr FPSCR, r5");
    COMPARE(vmsr(r10, pl),
            "5ee1aa10       vmsrpl FPSCR, r10");
    COMPARE(vmsr(pc),
            "eee1fa10       vmsr FPSCR, APSR");
    COMPARE(vmrs(r5),
            "eef15a10       vmrs r5, FPSCR");
    COMPARE(vmrs(r10, ge),
            "aef1aa10       vmrsge r10, FPSCR");
    COMPARE(vmrs(pc),
            "eef1fa10       vmrs APSR, FPSCR");

    COMPARE(vstm(ia, r0, d1, d3),
            "ec801b06       vstmia r0, {d1-d3}");
    COMPARE(vldm(ia, r1, d2, d5),
            "ec912b08       vldmia r1, {d2-d5}");
    COMPARE(vstm(ia, r2, d0, d15),
            "ec820b20       vstmia r2, {d0-d15}");
    COMPARE(vldm(ia, r3, d0, d15),
            "ec930b20       vldmia r3, {d0-d15}");
    COMPARE(vstm(ia, r4, s1, s3),
            "ecc40a03       vstmia r4, {s1-s3}");
    COMPARE(vldm(ia, r5, s2, s5),
            "ec951a04       vldmia r5, {s2-s5}");
    COMPARE(vstm(ia, r6, s0, s31),
            "ec860a20       vstmia r6, {s0-s31}");
    COMPARE(vldm(ia, r7, s0, s31),
            "ec970a20       vldmia r7, {s0-s31}");

    COMPARE(vmla(d2, d1, d0),
            "ee012b00       vmla.f64 d2, d1, d0");
    COMPARE(vmla(d6, d4, d5, cc),
            "3e046b05       vmlacc.f64 d6, d4, d5");

    COMPARE(vmls(d2, d1, d0),
            "ee012b40       vmls.f64 d2, d1, d0");
    COMPARE(vmls(d6, d4, d5, cc),
            "3e046b45       vmlscc.f64 d6, d4, d5");

    COMPARE(vcvt_u32_f64(s0, d0),
            "eebc0bc0       vcvt.u32.f64 s0, d0");
    COMPARE(vcvt_s32_f64(s0, d0),
            "eebd0bc0       vcvt.s32.f64 s0, d0");
    COMPARE(vcvt_f64_u32(d0, s1),
            "eeb80b60       vcvt.f64.u32 d0, s1");
    COMPARE(vcvt_f64_s32(d0, s1),
            "eeb80be0       vcvt.f64.s32 d0, s1");
    COMPARE(vcvt_f32_s32(s0, s2),
            "eeb80ac1       vcvt.f32.s32 s0, s2");
    COMPARE(vcvt_f64_s32(d0, 2),
            "eeba0bcf       vcvt.f64.s32 d0, d0, #2");

    if (CpuFeatures::IsSupported(VFP32DREGS)) {
      COMPARE(vmov(d3, d27),
              "eeb03b6b       vmov.f64 d3, d27");
      COMPARE(vmov(d18, d7),
              "eef02b47       vmov.f64 d18, d7");
      COMPARE(vmov(d18, r2, r3),
              "ec432b32       vmov d18, r2, r3");
      COMPARE(vmov(r2, r3, d18),
              "ec532b32       vmov r2, r3, d18");
      COMPARE(vmov(d20, d31),
              "eef04b6f       vmov.f64 d20, d31");

      COMPARE(vabs(d16, d31),
              "eef00bef       vabs.f64 d16, d31");

      COMPARE(vneg(d16, d31),
              "eef10b6f       vneg.f64 d16, d31");

      COMPARE(vadd(d16, d17, d18),
              "ee710ba2       vadd.f64 d16, d17, d18");

      COMPARE(vsub(d16, d17, d18),
              "ee710be2       vsub.f64 d16, d17, d18");

      COMPARE(vmul(d16, d17, d18),
              "ee610ba2       vmul.f64 d16, d17, d18");

      COMPARE(vdiv(d16, d17, d18),
              "eec10ba2       vdiv.f64 d16, d17, d18");

      COMPARE(vcmp(d16, d17),
              "eef40b61       vcmp.f64 d16, d17");
      COMPARE(vcmp(d16, 0.0),
              "eef50b40       vcmp.f64 d16, #0.0");

      COMPARE(vsqrt(d16, d17),
              "eef10be1       vsqrt.f64 d16, d17");

      COMPARE(vmov(d30, 16.0),
              "eef3eb00       vmov.f64 d30, #16");

      COMPARE(vmov(d31, VmovIndexLo, r7),
              "ee0f7b90       vmov.32 d31[0], r7");
      COMPARE(vmov(d31, VmovIndexHi, r7),
              "ee2f7b90       vmov.32 d31[1], r7");

      COMPARE(vldr(d25, r0, 0),
              "edd09b00       vldr d25, [r0 + 4*0]");
      COMPARE(vldr(d26, r1, 4),
              "edd1ab01       vldr d26, [r1 + 4*1]");
      COMPARE(vldr(d31, r10, 1020),
              "eddafbff       vldr d31, [r10 + 4*255]");

      COMPARE(vstr(d16, r0, 0),
              "edc00b00       vstr d16, [r0 + 4*0]");
      COMPARE(vstr(d17, r1, 4),
              "edc11b01       vstr d17, [r1 + 4*1]");
      COMPARE(vstr(d31, r10, 1020),
              "edcafbff       vstr d31, [r10 + 4*255]");

      COMPARE(vstm(ia, r0, d16, d31),
              "ecc00b20       vstmia r0, {d16-d31}");
      COMPARE(vldm(ia, r3, d16, d31),
              "ecd30b20       vldmia r3, {d16-d31}");
      COMPARE(vstm(ia, r0, d23, d27),
              "ecc07b0a       vstmia r0, {d23-d27}");
      COMPARE(vldm(ia, r3, d23, d27),
              "ecd37b0a       vldmia r3, {d23-d27}");

      COMPARE(vmla(d16, d17, d18),
              "ee410ba2       vmla.f64 d16, d17, d18");

      COMPARE(vcvt_u32_f64(s0, d16),
              "eebc0be0       vcvt.u32.f64 s0, d16");
      COMPARE(vcvt_s32_f64(s0, d16),
              "eebd0be0       vcvt.s32.f64 s0, d16");
      COMPARE(vcvt_f64_u32(d16, s1),
              "eef80b60       vcvt.f64.u32 d16, s1");
    }
  }

  VERIFY_RUN();
}


TEST(ARMv8_vrintX_disasm) {
  SET_UP();

  if (CpuFeatures::IsSupported(ARMv8)) {
    COMPARE(vrinta(d0, d0), "feb80b40       vrinta.f64.f64 d0, d0");
    COMPARE(vrinta(d2, d3), "feb82b43       vrinta.f64.f64 d2, d3");

    COMPARE(vrintp(d0, d0), "feba0b40       vrintp.f64.f64 d0, d0");
    COMPARE(vrintp(d2, d3), "feba2b43       vrintp.f64.f64 d2, d3");

    COMPARE(vrintn(d0, d0), "feb90b40       vrintn.f64.f64 d0, d0");
    COMPARE(vrintn(d2, d3), "feb92b43       vrintn.f64.f64 d2, d3");

    COMPARE(vrintm(d0, d0), "febb0b40       vrintm.f64.f64 d0, d0");
    COMPARE(vrintm(d2, d3), "febb2b43       vrintm.f64.f64 d2, d3");

    COMPARE(vrintz(d0, d0), "eeb60bc0       vrintz.f64.f64 d0, d0");
    COMPARE(vrintz(d2, d3, ne), "1eb62bc3       vrintzne.f64.f64 d2, d3");
  }

  VERIFY_RUN();
}


TEST(Neon) {
  SET_UP();

  if (CpuFeatures::IsSupported(NEON)) {
    CpuFeatureScope scope(&assm, NEON);
      COMPARE(vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(r1)),
              "f421420f       vld1.8 {d4, d5, d6, d7}, [r1]");
      COMPARE(vst1(Neon16, NeonListOperand(d17, 4), NeonMemOperand(r9)),
              "f449124f       vst1.16 {d17, d18, d19, d20}, [r9]");
      COMPARE(vmovl(NeonU8, q3, d1),
              "f3886a11       vmovl.u8 q3, d1");
      COMPARE(vmovl(NeonU8, q4, d2),
              "f3888a12       vmovl.u8 q4, d2");
  }

  VERIFY_RUN();
}


TEST(LoadStore) {
  SET_UP();

  COMPARE(ldrb(r0, MemOperand(r1)),
          "e5d10000       ldrb r0, [r1, #+0]");
  COMPARE(ldrb(r2, MemOperand(r3, 42)),
          "e5d3202a       ldrb r2, [r3, #+42]");
  COMPARE(ldrb(r4, MemOperand(r5, -42)),
          "e555402a       ldrb r4, [r5, #-42]");
  COMPARE(ldrb(r6, MemOperand(r7, 42, PostIndex)),
          "e4d7602a       ldrb r6, [r7], #+42");
  COMPARE(ldrb(r8, MemOperand(r9, -42, PostIndex)),
          "e459802a       ldrb r8, [r9], #-42");
  COMPARE(ldrb(r10, MemOperand(fp, 42, PreIndex)),
          "e5fba02a       ldrb r10, [fp, #+42]!");
  COMPARE(ldrb(ip, MemOperand(sp, -42, PreIndex)),
          "e57dc02a       ldrb ip, [sp, #-42]!");
  COMPARE(ldrb(r0, MemOperand(r1, r2)),
          "e7d10002       ldrb r0, [r1, +r2]");
  COMPARE(ldrb(r0, MemOperand(r1, r2, NegOffset)),
          "e7510002       ldrb r0, [r1, -r2]");
  COMPARE(ldrb(r0, MemOperand(r1, r2, PostIndex)),
          "e6d10002       ldrb r0, [r1], +r2");
  COMPARE(ldrb(r0, MemOperand(r1, r2, NegPostIndex)),
          "e6510002       ldrb r0, [r1], -r2");
  COMPARE(ldrb(r0, MemOperand(r1, r2, PreIndex)),
          "e7f10002       ldrb r0, [r1, +r2]!");
  COMPARE(ldrb(r0, MemOperand(r1, r2, NegPreIndex)),
          "e7710002       ldrb r0, [r1, -r2]!");

  COMPARE(strb(r0, MemOperand(r1)),
          "e5c10000       strb r0, [r1, #+0]");
  COMPARE(strb(r2, MemOperand(r3, 42)),
          "e5c3202a       strb r2, [r3, #+42]");
  COMPARE(strb(r4, MemOperand(r5, -42)),
          "e545402a       strb r4, [r5, #-42]");
  COMPARE(strb(r6, MemOperand(r7, 42, PostIndex)),
          "e4c7602a       strb r6, [r7], #+42");
  COMPARE(strb(r8, MemOperand(r9, -42, PostIndex)),
          "e449802a       strb r8, [r9], #-42");
  COMPARE(strb(r10, MemOperand(fp, 42, PreIndex)),
          "e5eba02a       strb r10, [fp, #+42]!");
  COMPARE(strb(ip, MemOperand(sp, -42, PreIndex)),
          "e56dc02a       strb ip, [sp, #-42]!");
  COMPARE(strb(r0, MemOperand(r1, r2)),
          "e7c10002       strb r0, [r1, +r2]");
  COMPARE(strb(r0, MemOperand(r1, r2, NegOffset)),
          "e7410002       strb r0, [r1, -r2]");
  COMPARE(strb(r0, MemOperand(r1, r2, PostIndex)),
          "e6c10002       strb r0, [r1], +r2");
  COMPARE(strb(r0, MemOperand(r1, r2, NegPostIndex)),
          "e6410002       strb r0, [r1], -r2");
  COMPARE(strb(r0, MemOperand(r1, r2, PreIndex)),
          "e7e10002       strb r0, [r1, +r2]!");
  COMPARE(strb(r0, MemOperand(r1, r2, NegPreIndex)),
          "e7610002       strb r0, [r1, -r2]!");

  COMPARE(ldrh(r0, MemOperand(r1)),
          "e1d100b0       ldrh r0, [r1, #+0]");
  COMPARE(ldrh(r2, MemOperand(r3, 42)),
          "e1d322ba       ldrh r2, [r3, #+42]");
  COMPARE(ldrh(r4, MemOperand(r5, -42)),
          "e15542ba       ldrh r4, [r5, #-42]");
  COMPARE(ldrh(r6, MemOperand(r7, 42, PostIndex)),
          "e0d762ba       ldrh r6, [r7], #+42");
  COMPARE(ldrh(r8, MemOperand(r9, -42, PostIndex)),
          "e05982ba       ldrh r8, [r9], #-42");
  COMPARE(ldrh(r10, MemOperand(fp, 42, PreIndex)),
          "e1fba2ba       ldrh r10, [fp, #+42]!");
  COMPARE(ldrh(ip, MemOperand(sp, -42, PreIndex)),
          "e17dc2ba       ldrh ip, [sp, #-42]!");
  COMPARE(ldrh(r0, MemOperand(r1, r2)),
          "e19100b2       ldrh r0, [r1, +r2]");
  COMPARE(ldrh(r0, MemOperand(r1, r2, NegOffset)),
          "e11100b2       ldrh r0, [r1, -r2]");
  COMPARE(ldrh(r0, MemOperand(r1, r2, PostIndex)),
          "e09100b2       ldrh r0, [r1], +r2");
  COMPARE(ldrh(r0, MemOperand(r1, r2, NegPostIndex)),
          "e01100b2       ldrh r0, [r1], -r2");
  COMPARE(ldrh(r0, MemOperand(r1, r2, PreIndex)),
          "e1b100b2       ldrh r0, [r1, +r2]!");
  COMPARE(ldrh(r0, MemOperand(r1, r2, NegPreIndex)),
          "e13100b2       ldrh r0, [r1, -r2]!");

  COMPARE(strh(r0, MemOperand(r1)),
          "e1c100b0       strh r0, [r1, #+0]");
  COMPARE(strh(r2, MemOperand(r3, 42)),
          "e1c322ba       strh r2, [r3, #+42]");
  COMPARE(strh(r4, MemOperand(r5, -42)),
          "e14542ba       strh r4, [r5, #-42]");
  COMPARE(strh(r6, MemOperand(r7, 42, PostIndex)),
          "e0c762ba       strh r6, [r7], #+42");
  COMPARE(strh(r8, MemOperand(r9, -42, PostIndex)),
          "e04982ba       strh r8, [r9], #-42");
  COMPARE(strh(r10, MemOperand(fp, 42, PreIndex)),
          "e1eba2ba       strh r10, [fp, #+42]!");
  COMPARE(strh(ip, MemOperand(sp, -42, PreIndex)),
          "e16dc2ba       strh ip, [sp, #-42]!");
  COMPARE(strh(r0, MemOperand(r1, r2)),
          "e18100b2       strh r0, [r1, +r2]");
  COMPARE(strh(r0, MemOperand(r1, r2, NegOffset)),
          "e10100b2       strh r0, [r1, -r2]");
  COMPARE(strh(r0, MemOperand(r1, r2, PostIndex)),
          "e08100b2       strh r0, [r1], +r2");
  COMPARE(strh(r0, MemOperand(r1, r2, NegPostIndex)),
          "e00100b2       strh r0, [r1], -r2");
  COMPARE(strh(r0, MemOperand(r1, r2, PreIndex)),
          "e1a100b2       strh r0, [r1, +r2]!");
  COMPARE(strh(r0, MemOperand(r1, r2, NegPreIndex)),
          "e12100b2       strh r0, [r1, -r2]!");

  COMPARE(ldr(r0, MemOperand(r1)),
          "e5910000       ldr r0, [r1, #+0]");
  COMPARE(ldr(r2, MemOperand(r3, 42)),
          "e593202a       ldr r2, [r3, #+42]");
  COMPARE(ldr(r4, MemOperand(r5, -42)),
          "e515402a       ldr r4, [r5, #-42]");
  COMPARE(ldr(r6, MemOperand(r7, 42, PostIndex)),
          "e497602a       ldr r6, [r7], #+42");
  COMPARE(ldr(r8, MemOperand(r9, -42, PostIndex)),
          "e419802a       ldr r8, [r9], #-42");
  COMPARE(ldr(r10, MemOperand(fp, 42, PreIndex)),
          "e5bba02a       ldr r10, [fp, #+42]!");
  COMPARE(ldr(ip, MemOperand(sp, -42, PreIndex)),
          "e53dc02a       ldr ip, [sp, #-42]!");
  COMPARE(ldr(r0, MemOperand(r1, r2)),
          "e7910002       ldr r0, [r1, +r2]");
  COMPARE(ldr(r0, MemOperand(r1, r2, NegOffset)),
          "e7110002       ldr r0, [r1, -r2]");
  COMPARE(ldr(r0, MemOperand(r1, r2, PostIndex)),
          "e6910002       ldr r0, [r1], +r2");
  COMPARE(ldr(r0, MemOperand(r1, r2, NegPostIndex)),
          "e6110002       ldr r0, [r1], -r2");
  COMPARE(ldr(r0, MemOperand(r1, r2, PreIndex)),
          "e7b10002       ldr r0, [r1, +r2]!");
  COMPARE(ldr(r0, MemOperand(r1, r2, NegPreIndex)),
          "e7310002       ldr r0, [r1, -r2]!");

  COMPARE(str(r0, MemOperand(r1)),
          "e5810000       str r0, [r1, #+0]");
  COMPARE(str(r2, MemOperand(r3, 42)),
          "e583202a       str r2, [r3, #+42]");
  COMPARE(str(r4, MemOperand(r5, -42)),
          "e505402a       str r4, [r5, #-42]");
  COMPARE(str(r6, MemOperand(r7, 42, PostIndex)),
          "e487602a       str r6, [r7], #+42");
  COMPARE(str(r8, MemOperand(r9, -42, PostIndex)),
          "e409802a       str r8, [r9], #-42");
  COMPARE(str(r10, MemOperand(fp, 42, PreIndex)),
          "e5aba02a       str r10, [fp, #+42]!");
  COMPARE(str(ip, MemOperand(sp, -42, PreIndex)),
          "e52dc02a       str ip, [sp, #-42]!");
  COMPARE(str(r0, MemOperand(r1, r2)),
          "e7810002       str r0, [r1, +r2]");
  COMPARE(str(r0, MemOperand(r1, r2, NegOffset)),
          "e7010002       str r0, [r1, -r2]");
  COMPARE(str(r0, MemOperand(r1, r2, PostIndex)),
          "e6810002       str r0, [r1], +r2");
  COMPARE(str(r0, MemOperand(r1, r2, NegPostIndex)),
          "e6010002       str r0, [r1], -r2");
  COMPARE(str(r0, MemOperand(r1, r2, PreIndex)),
          "e7a10002       str r0, [r1, +r2]!");
  COMPARE(str(r0, MemOperand(r1, r2, NegPreIndex)),
          "e7210002       str r0, [r1, -r2]!");

  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatureScope scope(&assm, ARMv7);
    COMPARE(ldrd(r0, r1, MemOperand(r1)),
            "e1c100d0       ldrd r0, [r1, #+0]");
    COMPARE(ldrd(r2, r3, MemOperand(r3, 127)),
            "e1c327df       ldrd r2, [r3, #+127]");
    COMPARE(ldrd(r4, r5, MemOperand(r5, -127)),
            "e14547df       ldrd r4, [r5, #-127]");
    COMPARE(ldrd(r6, r7, MemOperand(r7, 127, PostIndex)),
            "e0c767df       ldrd r6, [r7], #+127");
    COMPARE(ldrd(r8, r9, MemOperand(r9, -127, PostIndex)),
            "e04987df       ldrd r8, [r9], #-127");
    COMPARE(ldrd(r10, fp, MemOperand(fp, 127, PreIndex)),
            "e1eba7df       ldrd r10, [fp, #+127]!");
    COMPARE(ldrd(ip, sp, MemOperand(sp, -127, PreIndex)),
            "e16dc7df       ldrd ip, [sp, #-127]!");

    COMPARE(strd(r0, r1, MemOperand(r1)),
            "e1c100f0       strd r0, [r1, #+0]");
    COMPARE(strd(r2, r3, MemOperand(r3, 127)),
            "e1c327ff       strd r2, [r3, #+127]");
    COMPARE(strd(r4, r5, MemOperand(r5, -127)),
            "e14547ff       strd r4, [r5, #-127]");
    COMPARE(strd(r6, r7, MemOperand(r7, 127, PostIndex)),
            "e0c767ff       strd r6, [r7], #+127");
    COMPARE(strd(r8, r9, MemOperand(r9, -127, PostIndex)),
            "e04987ff       strd r8, [r9], #-127");
    COMPARE(strd(r10, fp, MemOperand(fp, 127, PreIndex)),
            "e1eba7ff       strd r10, [fp, #+127]!");
    COMPARE(strd(ip, sp, MemOperand(sp, -127, PreIndex)),
            "e16dc7ff       strd ip, [sp, #-127]!");

    COMPARE(pld(MemOperand(r1, 0)),
            "f5d1f000       pld [r1]");
    COMPARE(pld(MemOperand(r2, 128)),
            "f5d2f080       pld [r2, #+128]");
  }

  VERIFY_RUN();
}
