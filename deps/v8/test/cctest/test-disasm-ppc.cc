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

#include "src/debug/debug.h"
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/frames-inl.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

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
#define SET_UP()                                            \
  CcTest::InitializeVM();                                   \
  Isolate* isolate = CcTest::i_isolate();                   \
  HandleScope scope(isolate);                               \
  byte* buffer = reinterpret_cast<byte*>(malloc(4 * 1024));  \
  Assembler assm(AssemblerOptions{},                         \
                 ExternalAssemblerBuffer(buffer, 4 * 1024)); \
  bool failure = false;

// This macro assembles one instruction using the preallocated assembler and
// disassembles the generated instruction, comparing the output to the expected
// value. If the comparison fails an error message is printed, but the test
// continues to run until the end.
#define COMPARE(asm_, compare_string)                                        \
  {                                                                          \
    int pc_offset = assm.pc_offset();                                        \
    byte* progcounter = &buffer[pc_offset];                                  \
    assm.asm_;                                                               \
    if (!DisassembleAndCompare(progcounter, compare_string)) failure = true; \
  }

// Force emission of any pending literals into a pool.
#define EMIT_PENDING_LITERALS() assm.CheckConstPool(true, false)


// Verify that all invocations of the COMPARE macro passed successfully.
// Exit with a failure if at least one of the tests failed.
#define VERIFY_RUN()                                                  \
  if (failure) {                                                      \
    V8_Fatal(__FILE__, __LINE__, "PPC Disassembler tests failed.\n"); \
  }

TEST(DisasmPPC) {
  SET_UP();

  COMPARE(addc(r9, r7, r9), "7d274814       addc    r9, r7, r9");
  COMPARE(addic(r3, r5, Operand(20)), "30650014       addic   r3, r5, 20");
  COMPARE(addi(r0, ip, Operand(63)), "380c003f       addi    r0, ip, 63");
  COMPARE(add(r5, r7, r0), "7ca70214       add     r5, r7, r0");
  COMPARE(addze(r0, r0, LeaveOE, SetRC), "7c000195       addze.   r0, r0");
  COMPARE(andi(r0, r3, Operand(4)), "70600004       andi.   r0, r3, 4");
  COMPARE(and_(r3, r6, r5), "7cc32838       and     r3, r6, r5");
  COMPARE(and_(r6, r0, r6, SetRC), "7c063039       and.    r6, r0, r6");
  // skipping branches (for now?)
  COMPARE(bctr(), "4e800420       bctr");
  COMPARE(bctrl(), "4e800421       bctrl");
  COMPARE(blr(), "4e800020       blr");
// skipping call - only used in simulator
#if V8_TARGET_ARCH_PPC64
  COMPARE(cmpi(r0, Operand(5)), "2fa00005       cmpi    r0, 5");
#else
  COMPARE(cmpi(r0, Operand(5)), "2f800005       cmpi    r0, 5");
#endif
#if V8_TARGET_ARCH_PPC64
  COMPARE(cmpl(r6, r7), "7fa63840       cmpl    r6, r7");
#else
  COMPARE(cmpl(r6, r7), "7f863840       cmpl    r6, r7");
#endif
#if V8_TARGET_ARCH_PPC64
  COMPARE(cmp(r5, r11), "7fa55800       cmp     r5, r11");
#else
  COMPARE(cmp(r5, r11), "7f855800       cmp     r5, r11");
#endif
  // skipping crxor - incomplete disassembly
  COMPARE(lbz(r4, MemOperand(r4, 7)), "88840007       lbz     r4, 7(r4)");
  COMPARE(lfd(d0, MemOperand(sp, 128)), "c8010080       lfd     d0, 128(sp)");
  COMPARE(li(r0, Operand(16)), "38000010       li      r0, 16");
  COMPARE(lis(r8, Operand(22560)), "3d005820       lis     r8, 22560");
  COMPARE(lwz(ip, MemOperand(r19, 44)), "8193002c       lwz     ip, 44(r19)");
  COMPARE(lwzx(r0, MemOperand(r5, ip)), "7c05602e       lwzx    r0, r5, ip");
  COMPARE(mflr(r0), "7c0802a6       mflr    r0");
  COMPARE(mr(r15, r4), "7c8f2378       mr      r15, r4");
  COMPARE(mtctr(r0), "7c0903a6       mtctr   r0");
  COMPARE(mtlr(r15), "7de803a6       mtlr    r15");
  COMPARE(ori(r8, r8, Operand(42849)), "6108a761       ori     r8, r8, 42849");
  COMPARE(orx(r5, r3, r4), "7c652378       or      r5, r3, r4");
  COMPARE(rlwinm(r4, r3, 2, 0, 29), "5464103a       rlwinm  r4, r3, 2, 0, 29");
  COMPARE(rlwinm(r0, r3, 0, 31, 31, SetRC),
          "546007ff       rlwinm. r0, r3, 0, 31, 31");
  COMPARE(srawi(r3, r6, 1), "7cc30e70       srawi   r3,r6,1");
  COMPARE(stb(r5, MemOperand(r11, 11)), "98ab000b       stb     r5, 11(r11)");
  COMPARE(stfd(d2, MemOperand(sp, 8)), "d8410008       stfd    d2, 8(sp)");
  COMPARE(stw(r16, MemOperand(sp, 64)), "92010040       stw     r16, 64(sp)");
  COMPARE(stwu(r3, MemOperand(sp, -4)), "9461fffc       stwu    r3, -4(sp)");
  COMPARE(sub(r3, r3, r4), "7c641850       subf    r3, r4, r3");
  COMPARE(sub(r0, r9, r8, LeaveOE, SetRC), "7c084851       subf.   r0, r8, r9");
  COMPARE(xor_(r6, r5, r4), "7ca62278       xor     r6, r5, r4");

  VERIFY_RUN();
}

}  // namespace internal
}  // namespace v8
