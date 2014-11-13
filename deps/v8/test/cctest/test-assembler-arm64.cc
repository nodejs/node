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
#include <stdlib.h>
#include <string.h>
#include <cmath>
#include <limits>

#include "src/v8.h"

#include "src/arm64/decoder-arm64-inl.h"
#include "src/arm64/disasm-arm64.h"
#include "src/arm64/simulator-arm64.h"
#include "src/arm64/utils-arm64.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-utils-arm64.h"

using namespace v8::internal;

// Test infrastructure.
//
// Tests are functions which accept no parameters and have no return values.
// The testing code should not perform an explicit return once completed. For
// example to test the mov immediate instruction a very simple test would be:
//
//   TEST(mov_x0_one) {
//     SETUP();
//
//     START();
//     __ mov(x0, Operand(1));
//     END();
//
//     RUN();
//
//     CHECK_EQUAL_64(1, x0);
//
//     TEARDOWN();
//   }
//
// Within a START ... END block all registers but sp can be modified. sp has to
// be explicitly saved/restored. The END() macro replaces the function return
// so it may appear multiple times in a test if the test has multiple exit
// points.
//
// Once the test has been run all integer and floating point registers as well
// as flags are accessible through a RegisterDump instance, see
// utils-arm64.cc for more info on RegisterDump.
//
// We provide some helper assert to handle common cases:
//
//   CHECK_EQUAL_32(int32_t, int_32t)
//   CHECK_EQUAL_FP32(float, float)
//   CHECK_EQUAL_32(int32_t, W register)
//   CHECK_EQUAL_FP32(float, S register)
//   CHECK_EQUAL_64(int64_t, int_64t)
//   CHECK_EQUAL_FP64(double, double)
//   CHECK_EQUAL_64(int64_t, X register)
//   CHECK_EQUAL_64(X register, X register)
//   CHECK_EQUAL_FP64(double, D register)
//
// e.g. CHECK_EQUAL_64(0.5, d30);
//
// If more advance computation is required before the assert then access the
// RegisterDump named core directly:
//
//   CHECK_EQUAL_64(0x1234, core.xreg(0) & 0xffff);


#if 0  // TODO(all): enable.
static v8::Persistent<v8::Context> env;

static void InitializeVM() {
  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}
#endif

#define __ masm.

#define BUF_SIZE 8192
#define SETUP() SETUP_SIZE(BUF_SIZE)

#define INIT_V8()                                                              \
  CcTest::InitializeVM();                                                      \

#ifdef USE_SIMULATOR

// Run tests with the simulator.
#define SETUP_SIZE(buf_size)                    \
  Isolate* isolate = Isolate::Current();        \
  HandleScope scope(isolate);                   \
  DCHECK(isolate != NULL);                      \
  byte* buf = new byte[buf_size];               \
  MacroAssembler masm(isolate, buf, buf_size);  \
  Decoder<DispatchingDecoderVisitor>* decoder = \
      new Decoder<DispatchingDecoderVisitor>(); \
  Simulator simulator(decoder);                 \
  PrintDisassembler* pdis = NULL;               \
  RegisterDump core;

/*  if (Cctest::trace_sim()) {                                                 \
    pdis = new PrintDisassembler(stdout);                                      \
    decoder.PrependVisitor(pdis);                                              \
  }                                                                            \
  */

// Reset the assembler and simulator, so that instructions can be generated,
// but don't actually emit any code. This can be used by tests that need to
// emit instructions at the start of the buffer. Note that START_AFTER_RESET
// must be called before any callee-saved register is modified, and before an
// END is encountered.
//
// Most tests should call START, rather than call RESET directly.
#define RESET()                                                                \
  __ Reset();                                                                  \
  simulator.ResetState();

#define START_AFTER_RESET()                                                    \
  __ SetStackPointer(csp);                                                     \
  __ PushCalleeSavedRegisters();                                               \
  __ Debug("Start test.", __LINE__, TRACE_ENABLE | LOG_ALL);

#define START()                                                                \
  RESET();                                                                     \
  START_AFTER_RESET();

#define RUN()                                                                  \
  simulator.RunFrom(reinterpret_cast<Instruction*>(buf))

#define END()                                                                  \
  __ Debug("End test.", __LINE__, TRACE_DISABLE | LOG_ALL);                    \
  core.Dump(&masm);                                                            \
  __ PopCalleeSavedRegisters();                                                \
  __ Ret();                                                                    \
  __ GetCode(NULL);

#define TEARDOWN()                                                             \
  delete pdis;                                                                 \
  delete[] buf;

#else  // ifdef USE_SIMULATOR.
// Run the test on real hardware or models.
#define SETUP_SIZE(buf_size)                                                   \
  Isolate* isolate = Isolate::Current();                                       \
  HandleScope scope(isolate);                                                  \
  DCHECK(isolate != NULL);                                                     \
  byte* buf = new byte[buf_size];                                              \
  MacroAssembler masm(isolate, buf, buf_size);                                 \
  RegisterDump core;

#define RESET()                                                                \
  __ Reset();                                                                  \
  /* Reset the machine state (like simulator.ResetState()). */                 \
  __ Msr(NZCV, xzr);                                                           \
  __ Msr(FPCR, xzr);


#define START_AFTER_RESET()                                                    \
  __ SetStackPointer(csp);                                                     \
  __ PushCalleeSavedRegisters();

#define START()                                                                \
  RESET();                                                                     \
  START_AFTER_RESET();

#define RUN()                                                \
  CpuFeatures::FlushICache(buf, masm.SizeOfGeneratedCode()); \
  {                                                          \
    void (*test_function)(void);                             \
    memcpy(&test_function, &buf, sizeof(buf));               \
    test_function();                                         \
  }

#define END()                                                                  \
  core.Dump(&masm);                                                            \
  __ PopCalleeSavedRegisters();                                                \
  __ Ret();                                                                    \
  __ GetCode(NULL);

#define TEARDOWN()                                                             \
  delete[] buf;

#endif  // ifdef USE_SIMULATOR.

#define CHECK_EQUAL_NZCV(expected)                                            \
  CHECK(EqualNzcv(expected, core.flags_nzcv()))

#define CHECK_EQUAL_REGISTERS(expected)                                       \
  CHECK(EqualRegisters(&expected, &core))

#define CHECK_EQUAL_32(expected, result)                                      \
  CHECK(Equal32(static_cast<uint32_t>(expected), &core, result))

#define CHECK_EQUAL_FP32(expected, result)                                    \
  CHECK(EqualFP32(expected, &core, result))

#define CHECK_EQUAL_64(expected, result)                                      \
  CHECK(Equal64(expected, &core, result))

#define CHECK_EQUAL_FP64(expected, result)                                    \
  CHECK(EqualFP64(expected, &core, result))

#ifdef DEBUG
#define DCHECK_LITERAL_POOL_SIZE(expected)                                     \
  CHECK((expected) == (__ LiteralPoolSize()))
#else
#define DCHECK_LITERAL_POOL_SIZE(expected)                                     \
  ((void) 0)
#endif


TEST(stack_ops) {
  INIT_V8();
  SETUP();

  START();
  // save csp.
  __ Mov(x29, csp);

  // Set the csp to a known value.
  __ Mov(x16, 0x1000);
  __ Mov(csp, x16);
  __ Mov(x0, csp);

  // Add immediate to the csp, and move the result to a normal register.
  __ Add(csp, csp, Operand(0x50));
  __ Mov(x1, csp);

  // Add extended to the csp, and move the result to a normal register.
  __ Mov(x17, 0xfff);
  __ Add(csp, csp, Operand(x17, SXTB));
  __ Mov(x2, csp);

  // Create an csp using a logical instruction, and move to normal register.
  __ Orr(csp, xzr, Operand(0x1fff));
  __ Mov(x3, csp);

  // Write wcsp using a logical instruction.
  __ Orr(wcsp, wzr, Operand(0xfffffff8L));
  __ Mov(x4, csp);

  // Write csp, and read back wcsp.
  __ Orr(csp, xzr, Operand(0xfffffff8L));
  __ Mov(w5, wcsp);

  //  restore csp.
  __ Mov(csp, x29);
  END();

  RUN();

  CHECK_EQUAL_64(0x1000, x0);
  CHECK_EQUAL_64(0x1050, x1);
  CHECK_EQUAL_64(0x104f, x2);
  CHECK_EQUAL_64(0x1fff, x3);
  CHECK_EQUAL_64(0xfffffff8, x4);
  CHECK_EQUAL_64(0xfffffff8, x5);

  TEARDOWN();
}


TEST(mvn) {
  INIT_V8();
  SETUP();

  START();
  __ Mvn(w0, 0xfff);
  __ Mvn(x1, 0xfff);
  __ Mvn(w2, Operand(w0, LSL, 1));
  __ Mvn(x3, Operand(x1, LSL, 2));
  __ Mvn(w4, Operand(w0, LSR, 3));
  __ Mvn(x5, Operand(x1, LSR, 4));
  __ Mvn(w6, Operand(w0, ASR, 11));
  __ Mvn(x7, Operand(x1, ASR, 12));
  __ Mvn(w8, Operand(w0, ROR, 13));
  __ Mvn(x9, Operand(x1, ROR, 14));
  __ Mvn(w10, Operand(w2, UXTB));
  __ Mvn(x11, Operand(x2, SXTB, 1));
  __ Mvn(w12, Operand(w2, UXTH, 2));
  __ Mvn(x13, Operand(x2, SXTH, 3));
  __ Mvn(x14, Operand(w2, UXTW, 4));
  __ Mvn(x15, Operand(w2, SXTW, 4));
  END();

  RUN();

  CHECK_EQUAL_64(0xfffff000, x0);
  CHECK_EQUAL_64(0xfffffffffffff000UL, x1);
  CHECK_EQUAL_64(0x00001fff, x2);
  CHECK_EQUAL_64(0x0000000000003fffUL, x3);
  CHECK_EQUAL_64(0xe00001ff, x4);
  CHECK_EQUAL_64(0xf0000000000000ffUL, x5);
  CHECK_EQUAL_64(0x00000001, x6);
  CHECK_EQUAL_64(0x0, x7);
  CHECK_EQUAL_64(0x7ff80000, x8);
  CHECK_EQUAL_64(0x3ffc000000000000UL, x9);
  CHECK_EQUAL_64(0xffffff00, x10);
  CHECK_EQUAL_64(0x0000000000000001UL, x11);
  CHECK_EQUAL_64(0xffff8003, x12);
  CHECK_EQUAL_64(0xffffffffffff0007UL, x13);
  CHECK_EQUAL_64(0xfffffffffffe000fUL, x14);
  CHECK_EQUAL_64(0xfffffffffffe000fUL, x15);

  TEARDOWN();
}


TEST(mov) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xffffffffffffffffL);
  __ Mov(x1, 0xffffffffffffffffL);
  __ Mov(x2, 0xffffffffffffffffL);
  __ Mov(x3, 0xffffffffffffffffL);

  __ Mov(x0, 0x0123456789abcdefL);

  __ movz(x1, 0xabcdL << 16);
  __ movk(x2, 0xabcdL << 32);
  __ movn(x3, 0xabcdL << 48);

  __ Mov(x4, 0x0123456789abcdefL);
  __ Mov(x5, x4);

  __ Mov(w6, -1);

  // Test that moves back to the same register have the desired effect. This
  // is a no-op for X registers, and a truncation for W registers.
  __ Mov(x7, 0x0123456789abcdefL);
  __ Mov(x7, x7);
  __ Mov(x8, 0x0123456789abcdefL);
  __ Mov(w8, w8);
  __ Mov(x9, 0x0123456789abcdefL);
  __ Mov(x9, Operand(x9));
  __ Mov(x10, 0x0123456789abcdefL);
  __ Mov(w10, Operand(w10));

  __ Mov(w11, 0xfff);
  __ Mov(x12, 0xfff);
  __ Mov(w13, Operand(w11, LSL, 1));
  __ Mov(x14, Operand(x12, LSL, 2));
  __ Mov(w15, Operand(w11, LSR, 3));
  __ Mov(x18, Operand(x12, LSR, 4));
  __ Mov(w19, Operand(w11, ASR, 11));
  __ Mov(x20, Operand(x12, ASR, 12));
  __ Mov(w21, Operand(w11, ROR, 13));
  __ Mov(x22, Operand(x12, ROR, 14));
  __ Mov(w23, Operand(w13, UXTB));
  __ Mov(x24, Operand(x13, SXTB, 1));
  __ Mov(w25, Operand(w13, UXTH, 2));
  __ Mov(x26, Operand(x13, SXTH, 3));
  __ Mov(x27, Operand(w13, UXTW, 4));
  END();

  RUN();

  CHECK_EQUAL_64(0x0123456789abcdefL, x0);
  CHECK_EQUAL_64(0x00000000abcd0000L, x1);
  CHECK_EQUAL_64(0xffffabcdffffffffL, x2);
  CHECK_EQUAL_64(0x5432ffffffffffffL, x3);
  CHECK_EQUAL_64(x4, x5);
  CHECK_EQUAL_32(-1, w6);
  CHECK_EQUAL_64(0x0123456789abcdefL, x7);
  CHECK_EQUAL_32(0x89abcdefL, w8);
  CHECK_EQUAL_64(0x0123456789abcdefL, x9);
  CHECK_EQUAL_32(0x89abcdefL, w10);
  CHECK_EQUAL_64(0x00000fff, x11);
  CHECK_EQUAL_64(0x0000000000000fffUL, x12);
  CHECK_EQUAL_64(0x00001ffe, x13);
  CHECK_EQUAL_64(0x0000000000003ffcUL, x14);
  CHECK_EQUAL_64(0x000001ff, x15);
  CHECK_EQUAL_64(0x00000000000000ffUL, x18);
  CHECK_EQUAL_64(0x00000001, x19);
  CHECK_EQUAL_64(0x0, x20);
  CHECK_EQUAL_64(0x7ff80000, x21);
  CHECK_EQUAL_64(0x3ffc000000000000UL, x22);
  CHECK_EQUAL_64(0x000000fe, x23);
  CHECK_EQUAL_64(0xfffffffffffffffcUL, x24);
  CHECK_EQUAL_64(0x00007ff8, x25);
  CHECK_EQUAL_64(0x000000000000fff0UL, x26);
  CHECK_EQUAL_64(0x000000000001ffe0UL, x27);

  TEARDOWN();
}


TEST(mov_imm_w) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w0, 0xffffffffL);
  __ Mov(w1, 0xffff1234L);
  __ Mov(w2, 0x1234ffffL);
  __ Mov(w3, 0x00000000L);
  __ Mov(w4, 0x00001234L);
  __ Mov(w5, 0x12340000L);
  __ Mov(w6, 0x12345678L);
  __ Mov(w7, (int32_t)0x80000000);
  __ Mov(w8, (int32_t)0xffff0000);
  __ Mov(w9, kWMinInt);
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffffL, x0);
  CHECK_EQUAL_64(0xffff1234L, x1);
  CHECK_EQUAL_64(0x1234ffffL, x2);
  CHECK_EQUAL_64(0x00000000L, x3);
  CHECK_EQUAL_64(0x00001234L, x4);
  CHECK_EQUAL_64(0x12340000L, x5);
  CHECK_EQUAL_64(0x12345678L, x6);
  CHECK_EQUAL_64(0x80000000L, x7);
  CHECK_EQUAL_64(0xffff0000L, x8);
  CHECK_EQUAL_32(kWMinInt, w9);

  TEARDOWN();
}


TEST(mov_imm_x) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xffffffffffffffffL);
  __ Mov(x1, 0xffffffffffff1234L);
  __ Mov(x2, 0xffffffff12345678L);
  __ Mov(x3, 0xffff1234ffff5678L);
  __ Mov(x4, 0x1234ffffffff5678L);
  __ Mov(x5, 0x1234ffff5678ffffL);
  __ Mov(x6, 0x12345678ffffffffL);
  __ Mov(x7, 0x1234ffffffffffffL);
  __ Mov(x8, 0x123456789abcffffL);
  __ Mov(x9, 0x12345678ffff9abcL);
  __ Mov(x10, 0x1234ffff56789abcL);
  __ Mov(x11, 0xffff123456789abcL);
  __ Mov(x12, 0x0000000000000000L);
  __ Mov(x13, 0x0000000000001234L);
  __ Mov(x14, 0x0000000012345678L);
  __ Mov(x15, 0x0000123400005678L);
  __ Mov(x18, 0x1234000000005678L);
  __ Mov(x19, 0x1234000056780000L);
  __ Mov(x20, 0x1234567800000000L);
  __ Mov(x21, 0x1234000000000000L);
  __ Mov(x22, 0x123456789abc0000L);
  __ Mov(x23, 0x1234567800009abcL);
  __ Mov(x24, 0x1234000056789abcL);
  __ Mov(x25, 0x0000123456789abcL);
  __ Mov(x26, 0x123456789abcdef0L);
  __ Mov(x27, 0xffff000000000001L);
  __ Mov(x28, 0x8000ffff00000000L);
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffffffff1234L, x1);
  CHECK_EQUAL_64(0xffffffff12345678L, x2);
  CHECK_EQUAL_64(0xffff1234ffff5678L, x3);
  CHECK_EQUAL_64(0x1234ffffffff5678L, x4);
  CHECK_EQUAL_64(0x1234ffff5678ffffL, x5);
  CHECK_EQUAL_64(0x12345678ffffffffL, x6);
  CHECK_EQUAL_64(0x1234ffffffffffffL, x7);
  CHECK_EQUAL_64(0x123456789abcffffL, x8);
  CHECK_EQUAL_64(0x12345678ffff9abcL, x9);
  CHECK_EQUAL_64(0x1234ffff56789abcL, x10);
  CHECK_EQUAL_64(0xffff123456789abcL, x11);
  CHECK_EQUAL_64(0x0000000000000000L, x12);
  CHECK_EQUAL_64(0x0000000000001234L, x13);
  CHECK_EQUAL_64(0x0000000012345678L, x14);
  CHECK_EQUAL_64(0x0000123400005678L, x15);
  CHECK_EQUAL_64(0x1234000000005678L, x18);
  CHECK_EQUAL_64(0x1234000056780000L, x19);
  CHECK_EQUAL_64(0x1234567800000000L, x20);
  CHECK_EQUAL_64(0x1234000000000000L, x21);
  CHECK_EQUAL_64(0x123456789abc0000L, x22);
  CHECK_EQUAL_64(0x1234567800009abcL, x23);
  CHECK_EQUAL_64(0x1234000056789abcL, x24);
  CHECK_EQUAL_64(0x0000123456789abcL, x25);
  CHECK_EQUAL_64(0x123456789abcdef0L, x26);
  CHECK_EQUAL_64(0xffff000000000001L, x27);
  CHECK_EQUAL_64(0x8000ffff00000000L, x28);

  TEARDOWN();
}


TEST(orr) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xf0f0);
  __ Mov(x1, 0xf00000ff);

  __ Orr(x2, x0, Operand(x1));
  __ Orr(w3, w0, Operand(w1, LSL, 28));
  __ Orr(x4, x0, Operand(x1, LSL, 32));
  __ Orr(x5, x0, Operand(x1, LSR, 4));
  __ Orr(w6, w0, Operand(w1, ASR, 4));
  __ Orr(x7, x0, Operand(x1, ASR, 4));
  __ Orr(w8, w0, Operand(w1, ROR, 12));
  __ Orr(x9, x0, Operand(x1, ROR, 12));
  __ Orr(w10, w0, Operand(0xf));
  __ Orr(x11, x0, Operand(0xf0000000f0000000L));
  END();

  RUN();

  CHECK_EQUAL_64(0xf000f0ff, x2);
  CHECK_EQUAL_64(0xf000f0f0, x3);
  CHECK_EQUAL_64(0xf00000ff0000f0f0L, x4);
  CHECK_EQUAL_64(0x0f00f0ff, x5);
  CHECK_EQUAL_64(0xff00f0ff, x6);
  CHECK_EQUAL_64(0x0f00f0ff, x7);
  CHECK_EQUAL_64(0x0ffff0f0, x8);
  CHECK_EQUAL_64(0x0ff00000000ff0f0L, x9);
  CHECK_EQUAL_64(0xf0ff, x10);
  CHECK_EQUAL_64(0xf0000000f000f0f0L, x11);

  TEARDOWN();
}


TEST(orr_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0x8000000080008080UL);
  __ Orr(w6, w0, Operand(w1, UXTB));
  __ Orr(x7, x0, Operand(x1, UXTH, 1));
  __ Orr(w8, w0, Operand(w1, UXTW, 2));
  __ Orr(x9, x0, Operand(x1, UXTX, 3));
  __ Orr(w10, w0, Operand(w1, SXTB));
  __ Orr(x11, x0, Operand(x1, SXTH, 1));
  __ Orr(x12, x0, Operand(x1, SXTW, 2));
  __ Orr(x13, x0, Operand(x1, SXTX, 3));
  END();

  RUN();

  CHECK_EQUAL_64(0x00000081, x6);
  CHECK_EQUAL_64(0x00010101, x7);
  CHECK_EQUAL_64(0x00020201, x8);
  CHECK_EQUAL_64(0x0000000400040401UL, x9);
  CHECK_EQUAL_64(0x00000000ffffff81UL, x10);
  CHECK_EQUAL_64(0xffffffffffff0101UL, x11);
  CHECK_EQUAL_64(0xfffffffe00020201UL, x12);
  CHECK_EQUAL_64(0x0000000400040401UL, x13);

  TEARDOWN();
}


TEST(bitwise_wide_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0xf0f0f0f0f0f0f0f0UL);

  __ Orr(x10, x0, Operand(0x1234567890abcdefUL));
  __ Orr(w11, w1, Operand(0x90abcdef));

  __ Orr(w12, w0, kWMinInt);
  __ Eor(w13, w0, kWMinInt);
  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(0xf0f0f0f0f0f0f0f0UL, x1);
  CHECK_EQUAL_64(0x1234567890abcdefUL, x10);
  CHECK_EQUAL_64(0xf0fbfdffUL, x11);
  CHECK_EQUAL_32(kWMinInt, w12);
  CHECK_EQUAL_32(kWMinInt, w13);

  TEARDOWN();
}


TEST(orn) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xf0f0);
  __ Mov(x1, 0xf00000ff);

  __ Orn(x2, x0, Operand(x1));
  __ Orn(w3, w0, Operand(w1, LSL, 4));
  __ Orn(x4, x0, Operand(x1, LSL, 4));
  __ Orn(x5, x0, Operand(x1, LSR, 1));
  __ Orn(w6, w0, Operand(w1, ASR, 1));
  __ Orn(x7, x0, Operand(x1, ASR, 1));
  __ Orn(w8, w0, Operand(w1, ROR, 16));
  __ Orn(x9, x0, Operand(x1, ROR, 16));
  __ Orn(w10, w0, Operand(0xffff));
  __ Orn(x11, x0, Operand(0xffff0000ffffL));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffff0ffffff0L, x2);
  CHECK_EQUAL_64(0xfffff0ff, x3);
  CHECK_EQUAL_64(0xfffffff0fffff0ffL, x4);
  CHECK_EQUAL_64(0xffffffff87fffff0L, x5);
  CHECK_EQUAL_64(0x07fffff0, x6);
  CHECK_EQUAL_64(0xffffffff87fffff0L, x7);
  CHECK_EQUAL_64(0xff00ffff, x8);
  CHECK_EQUAL_64(0xff00ffffffffffffL, x9);
  CHECK_EQUAL_64(0xfffff0f0, x10);
  CHECK_EQUAL_64(0xffff0000fffff0f0L, x11);

  TEARDOWN();
}


TEST(orn_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0x8000000080008081UL);
  __ Orn(w6, w0, Operand(w1, UXTB));
  __ Orn(x7, x0, Operand(x1, UXTH, 1));
  __ Orn(w8, w0, Operand(w1, UXTW, 2));
  __ Orn(x9, x0, Operand(x1, UXTX, 3));
  __ Orn(w10, w0, Operand(w1, SXTB));
  __ Orn(x11, x0, Operand(x1, SXTH, 1));
  __ Orn(x12, x0, Operand(x1, SXTW, 2));
  __ Orn(x13, x0, Operand(x1, SXTX, 3));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffff7f, x6);
  CHECK_EQUAL_64(0xfffffffffffefefdUL, x7);
  CHECK_EQUAL_64(0xfffdfdfb, x8);
  CHECK_EQUAL_64(0xfffffffbfffbfbf7UL, x9);
  CHECK_EQUAL_64(0x0000007f, x10);
  CHECK_EQUAL_64(0x0000fefd, x11);
  CHECK_EQUAL_64(0x00000001fffdfdfbUL, x12);
  CHECK_EQUAL_64(0xfffffffbfffbfbf7UL, x13);

  TEARDOWN();
}


TEST(and_) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xfff0);
  __ Mov(x1, 0xf00000ff);

  __ And(x2, x0, Operand(x1));
  __ And(w3, w0, Operand(w1, LSL, 4));
  __ And(x4, x0, Operand(x1, LSL, 4));
  __ And(x5, x0, Operand(x1, LSR, 1));
  __ And(w6, w0, Operand(w1, ASR, 20));
  __ And(x7, x0, Operand(x1, ASR, 20));
  __ And(w8, w0, Operand(w1, ROR, 28));
  __ And(x9, x0, Operand(x1, ROR, 28));
  __ And(w10, w0, Operand(0xff00));
  __ And(x11, x0, Operand(0xff));
  END();

  RUN();

  CHECK_EQUAL_64(0x000000f0, x2);
  CHECK_EQUAL_64(0x00000ff0, x3);
  CHECK_EQUAL_64(0x00000ff0, x4);
  CHECK_EQUAL_64(0x00000070, x5);
  CHECK_EQUAL_64(0x0000ff00, x6);
  CHECK_EQUAL_64(0x00000f00, x7);
  CHECK_EQUAL_64(0x00000ff0, x8);
  CHECK_EQUAL_64(0x00000000, x9);
  CHECK_EQUAL_64(0x0000ff00, x10);
  CHECK_EQUAL_64(0x000000f0, x11);

  TEARDOWN();
}


TEST(and_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xffffffffffffffffUL);
  __ Mov(x1, 0x8000000080008081UL);
  __ And(w6, w0, Operand(w1, UXTB));
  __ And(x7, x0, Operand(x1, UXTH, 1));
  __ And(w8, w0, Operand(w1, UXTW, 2));
  __ And(x9, x0, Operand(x1, UXTX, 3));
  __ And(w10, w0, Operand(w1, SXTB));
  __ And(x11, x0, Operand(x1, SXTH, 1));
  __ And(x12, x0, Operand(x1, SXTW, 2));
  __ And(x13, x0, Operand(x1, SXTX, 3));
  END();

  RUN();

  CHECK_EQUAL_64(0x00000081, x6);
  CHECK_EQUAL_64(0x00010102, x7);
  CHECK_EQUAL_64(0x00020204, x8);
  CHECK_EQUAL_64(0x0000000400040408UL, x9);
  CHECK_EQUAL_64(0xffffff81, x10);
  CHECK_EQUAL_64(0xffffffffffff0102UL, x11);
  CHECK_EQUAL_64(0xfffffffe00020204UL, x12);
  CHECK_EQUAL_64(0x0000000400040408UL, x13);

  TEARDOWN();
}


TEST(ands) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0xf00000ff);
  __ Ands(w0, w1, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0xf00000ff, x0);

  START();
  __ Mov(x0, 0xfff0);
  __ Mov(x1, 0xf00000ff);
  __ Ands(w0, w0, Operand(w1, LSR, 4));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  START();
  __ Mov(x0, 0x8000000000000000L);
  __ Mov(x1, 0x00000001);
  __ Ands(x0, x0, Operand(x1, ROR, 1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x8000000000000000L, x0);

  START();
  __ Mov(x0, 0xfff0);
  __ Ands(w0, w0, Operand(0xf));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  START();
  __ Mov(x0, 0xff000000);
  __ Ands(w0, w0, Operand(0x80000000));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x80000000, x0);

  TEARDOWN();
}


TEST(bic) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xfff0);
  __ Mov(x1, 0xf00000ff);

  __ Bic(x2, x0, Operand(x1));
  __ Bic(w3, w0, Operand(w1, LSL, 4));
  __ Bic(x4, x0, Operand(x1, LSL, 4));
  __ Bic(x5, x0, Operand(x1, LSR, 1));
  __ Bic(w6, w0, Operand(w1, ASR, 20));
  __ Bic(x7, x0, Operand(x1, ASR, 20));
  __ Bic(w8, w0, Operand(w1, ROR, 28));
  __ Bic(x9, x0, Operand(x1, ROR, 24));
  __ Bic(x10, x0, Operand(0x1f));
  __ Bic(x11, x0, Operand(0x100));

  // Test bic into csp when the constant cannot be encoded in the immediate
  // field.
  // Use x20 to preserve csp. We check for the result via x21 because the
  // test infrastructure requires that csp be restored to its original value.
  __ Mov(x20, csp);
  __ Mov(x0, 0xffffff);
  __ Bic(csp, x0, Operand(0xabcdef));
  __ Mov(x21, csp);
  __ Mov(csp, x20);
  END();

  RUN();

  CHECK_EQUAL_64(0x0000ff00, x2);
  CHECK_EQUAL_64(0x0000f000, x3);
  CHECK_EQUAL_64(0x0000f000, x4);
  CHECK_EQUAL_64(0x0000ff80, x5);
  CHECK_EQUAL_64(0x000000f0, x6);
  CHECK_EQUAL_64(0x0000f0f0, x7);
  CHECK_EQUAL_64(0x0000f000, x8);
  CHECK_EQUAL_64(0x0000ff00, x9);
  CHECK_EQUAL_64(0x0000ffe0, x10);
  CHECK_EQUAL_64(0x0000fef0, x11);

  CHECK_EQUAL_64(0x543210, x21);

  TEARDOWN();
}


TEST(bic_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xffffffffffffffffUL);
  __ Mov(x1, 0x8000000080008081UL);
  __ Bic(w6, w0, Operand(w1, UXTB));
  __ Bic(x7, x0, Operand(x1, UXTH, 1));
  __ Bic(w8, w0, Operand(w1, UXTW, 2));
  __ Bic(x9, x0, Operand(x1, UXTX, 3));
  __ Bic(w10, w0, Operand(w1, SXTB));
  __ Bic(x11, x0, Operand(x1, SXTH, 1));
  __ Bic(x12, x0, Operand(x1, SXTW, 2));
  __ Bic(x13, x0, Operand(x1, SXTX, 3));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffff7e, x6);
  CHECK_EQUAL_64(0xfffffffffffefefdUL, x7);
  CHECK_EQUAL_64(0xfffdfdfb, x8);
  CHECK_EQUAL_64(0xfffffffbfffbfbf7UL, x9);
  CHECK_EQUAL_64(0x0000007e, x10);
  CHECK_EQUAL_64(0x0000fefd, x11);
  CHECK_EQUAL_64(0x00000001fffdfdfbUL, x12);
  CHECK_EQUAL_64(0xfffffffbfffbfbf7UL, x13);

  TEARDOWN();
}


TEST(bics) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0xffff);
  __ Bics(w0, w1, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  START();
  __ Mov(x0, 0xffffffff);
  __ Bics(w0, w0, Operand(w0, LSR, 1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x80000000, x0);

  START();
  __ Mov(x0, 0x8000000000000000L);
  __ Mov(x1, 0x00000001);
  __ Bics(x0, x0, Operand(x1, ROR, 1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  START();
  __ Mov(x0, 0xffffffffffffffffL);
  __ Bics(x0, x0, Operand(0x7fffffffffffffffL));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x8000000000000000L, x0);

  START();
  __ Mov(w0, 0xffff0000);
  __ Bics(w0, w0, Operand(0xfffffff0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  TEARDOWN();
}


TEST(eor) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xfff0);
  __ Mov(x1, 0xf00000ff);

  __ Eor(x2, x0, Operand(x1));
  __ Eor(w3, w0, Operand(w1, LSL, 4));
  __ Eor(x4, x0, Operand(x1, LSL, 4));
  __ Eor(x5, x0, Operand(x1, LSR, 1));
  __ Eor(w6, w0, Operand(w1, ASR, 20));
  __ Eor(x7, x0, Operand(x1, ASR, 20));
  __ Eor(w8, w0, Operand(w1, ROR, 28));
  __ Eor(x9, x0, Operand(x1, ROR, 28));
  __ Eor(w10, w0, Operand(0xff00ff00));
  __ Eor(x11, x0, Operand(0xff00ff00ff00ff00L));
  END();

  RUN();

  CHECK_EQUAL_64(0xf000ff0f, x2);
  CHECK_EQUAL_64(0x0000f000, x3);
  CHECK_EQUAL_64(0x0000000f0000f000L, x4);
  CHECK_EQUAL_64(0x7800ff8f, x5);
  CHECK_EQUAL_64(0xffff00f0, x6);
  CHECK_EQUAL_64(0x0000f0f0, x7);
  CHECK_EQUAL_64(0x0000f00f, x8);
  CHECK_EQUAL_64(0x00000ff00000ffffL, x9);
  CHECK_EQUAL_64(0xff0000f0, x10);
  CHECK_EQUAL_64(0xff00ff00ff0000f0L, x11);

  TEARDOWN();
}


TEST(eor_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0x1111111111111111UL);
  __ Mov(x1, 0x8000000080008081UL);
  __ Eor(w6, w0, Operand(w1, UXTB));
  __ Eor(x7, x0, Operand(x1, UXTH, 1));
  __ Eor(w8, w0, Operand(w1, UXTW, 2));
  __ Eor(x9, x0, Operand(x1, UXTX, 3));
  __ Eor(w10, w0, Operand(w1, SXTB));
  __ Eor(x11, x0, Operand(x1, SXTH, 1));
  __ Eor(x12, x0, Operand(x1, SXTW, 2));
  __ Eor(x13, x0, Operand(x1, SXTX, 3));
  END();

  RUN();

  CHECK_EQUAL_64(0x11111190, x6);
  CHECK_EQUAL_64(0x1111111111101013UL, x7);
  CHECK_EQUAL_64(0x11131315, x8);
  CHECK_EQUAL_64(0x1111111511151519UL, x9);
  CHECK_EQUAL_64(0xeeeeee90, x10);
  CHECK_EQUAL_64(0xeeeeeeeeeeee1013UL, x11);
  CHECK_EQUAL_64(0xeeeeeeef11131315UL, x12);
  CHECK_EQUAL_64(0x1111111511151519UL, x13);

  TEARDOWN();
}


TEST(eon) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xfff0);
  __ Mov(x1, 0xf00000ff);

  __ Eon(x2, x0, Operand(x1));
  __ Eon(w3, w0, Operand(w1, LSL, 4));
  __ Eon(x4, x0, Operand(x1, LSL, 4));
  __ Eon(x5, x0, Operand(x1, LSR, 1));
  __ Eon(w6, w0, Operand(w1, ASR, 20));
  __ Eon(x7, x0, Operand(x1, ASR, 20));
  __ Eon(w8, w0, Operand(w1, ROR, 28));
  __ Eon(x9, x0, Operand(x1, ROR, 28));
  __ Eon(w10, w0, Operand(0x03c003c0));
  __ Eon(x11, x0, Operand(0x0000100000001000L));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffff0fff00f0L, x2);
  CHECK_EQUAL_64(0xffff0fff, x3);
  CHECK_EQUAL_64(0xfffffff0ffff0fffL, x4);
  CHECK_EQUAL_64(0xffffffff87ff0070L, x5);
  CHECK_EQUAL_64(0x0000ff0f, x6);
  CHECK_EQUAL_64(0xffffffffffff0f0fL, x7);
  CHECK_EQUAL_64(0xffff0ff0, x8);
  CHECK_EQUAL_64(0xfffff00fffff0000L, x9);
  CHECK_EQUAL_64(0xfc3f03cf, x10);
  CHECK_EQUAL_64(0xffffefffffff100fL, x11);

  TEARDOWN();
}


TEST(eon_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0x1111111111111111UL);
  __ Mov(x1, 0x8000000080008081UL);
  __ Eon(w6, w0, Operand(w1, UXTB));
  __ Eon(x7, x0, Operand(x1, UXTH, 1));
  __ Eon(w8, w0, Operand(w1, UXTW, 2));
  __ Eon(x9, x0, Operand(x1, UXTX, 3));
  __ Eon(w10, w0, Operand(w1, SXTB));
  __ Eon(x11, x0, Operand(x1, SXTH, 1));
  __ Eon(x12, x0, Operand(x1, SXTW, 2));
  __ Eon(x13, x0, Operand(x1, SXTX, 3));
  END();

  RUN();

  CHECK_EQUAL_64(0xeeeeee6f, x6);
  CHECK_EQUAL_64(0xeeeeeeeeeeefefecUL, x7);
  CHECK_EQUAL_64(0xeeececea, x8);
  CHECK_EQUAL_64(0xeeeeeeeaeeeaeae6UL, x9);
  CHECK_EQUAL_64(0x1111116f, x10);
  CHECK_EQUAL_64(0x111111111111efecUL, x11);
  CHECK_EQUAL_64(0x11111110eeececeaUL, x12);
  CHECK_EQUAL_64(0xeeeeeeeaeeeaeae6UL, x13);

  TEARDOWN();
}


TEST(mul) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Mov(x17, 1);
  __ Mov(x18, 0xffffffff);
  __ Mov(x19, 0xffffffffffffffffUL);

  __ Mul(w0, w16, w16);
  __ Mul(w1, w16, w17);
  __ Mul(w2, w17, w18);
  __ Mul(w3, w18, w19);
  __ Mul(x4, x16, x16);
  __ Mul(x5, x17, x18);
  __ Mul(x6, x18, x19);
  __ Mul(x7, x19, x19);
  __ Smull(x8, w17, w18);
  __ Smull(x9, w18, w18);
  __ Smull(x10, w19, w19);
  __ Mneg(w11, w16, w16);
  __ Mneg(w12, w16, w17);
  __ Mneg(w13, w17, w18);
  __ Mneg(w14, w18, w19);
  __ Mneg(x20, x16, x16);
  __ Mneg(x21, x17, x18);
  __ Mneg(x22, x18, x19);
  __ Mneg(x23, x19, x19);
  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(0, x1);
  CHECK_EQUAL_64(0xffffffff, x2);
  CHECK_EQUAL_64(1, x3);
  CHECK_EQUAL_64(0, x4);
  CHECK_EQUAL_64(0xffffffff, x5);
  CHECK_EQUAL_64(0xffffffff00000001UL, x6);
  CHECK_EQUAL_64(1, x7);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0, x12);
  CHECK_EQUAL_64(1, x13);
  CHECK_EQUAL_64(0xffffffff, x14);
  CHECK_EQUAL_64(0, x20);
  CHECK_EQUAL_64(0xffffffff00000001UL, x21);
  CHECK_EQUAL_64(0xffffffff, x22);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x23);

  TEARDOWN();
}


static void SmullHelper(int64_t expected, int64_t a, int64_t b) {
  SETUP();
  START();
  __ Mov(w0, a);
  __ Mov(w1, b);
  __ Smull(x2, w0, w1);
  END();
  RUN();
  CHECK_EQUAL_64(expected, x2);
  TEARDOWN();
}


TEST(smull) {
  INIT_V8();
  SmullHelper(0, 0, 0);
  SmullHelper(1, 1, 1);
  SmullHelper(-1, -1, 1);
  SmullHelper(1, -1, -1);
  SmullHelper(0xffffffff80000000, 0x80000000, 1);
  SmullHelper(0x0000000080000000, 0x00010000, 0x00008000);
}


TEST(madd) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Mov(x17, 1);
  __ Mov(x18, 0xffffffff);
  __ Mov(x19, 0xffffffffffffffffUL);

  __ Madd(w0, w16, w16, w16);
  __ Madd(w1, w16, w16, w17);
  __ Madd(w2, w16, w16, w18);
  __ Madd(w3, w16, w16, w19);
  __ Madd(w4, w16, w17, w17);
  __ Madd(w5, w17, w17, w18);
  __ Madd(w6, w17, w17, w19);
  __ Madd(w7, w17, w18, w16);
  __ Madd(w8, w17, w18, w18);
  __ Madd(w9, w18, w18, w17);
  __ Madd(w10, w18, w19, w18);
  __ Madd(w11, w19, w19, w19);

  __ Madd(x12, x16, x16, x16);
  __ Madd(x13, x16, x16, x17);
  __ Madd(x14, x16, x16, x18);
  __ Madd(x15, x16, x16, x19);
  __ Madd(x20, x16, x17, x17);
  __ Madd(x21, x17, x17, x18);
  __ Madd(x22, x17, x17, x19);
  __ Madd(x23, x17, x18, x16);
  __ Madd(x24, x17, x18, x18);
  __ Madd(x25, x18, x18, x17);
  __ Madd(x26, x18, x19, x18);
  __ Madd(x27, x19, x19, x19);

  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(0xffffffff, x2);
  CHECK_EQUAL_64(0xffffffff, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0, x6);
  CHECK_EQUAL_64(0xffffffff, x7);
  CHECK_EQUAL_64(0xfffffffe, x8);
  CHECK_EQUAL_64(2, x9);
  CHECK_EQUAL_64(0, x10);
  CHECK_EQUAL_64(0, x11);

  CHECK_EQUAL_64(0, x12);
  CHECK_EQUAL_64(1, x13);
  CHECK_EQUAL_64(0xffffffff, x14);
  CHECK_EQUAL_64(0xffffffffffffffff, x15);
  CHECK_EQUAL_64(1, x20);
  CHECK_EQUAL_64(0x100000000UL, x21);
  CHECK_EQUAL_64(0, x22);
  CHECK_EQUAL_64(0xffffffff, x23);
  CHECK_EQUAL_64(0x1fffffffe, x24);
  CHECK_EQUAL_64(0xfffffffe00000002UL, x25);
  CHECK_EQUAL_64(0, x26);
  CHECK_EQUAL_64(0, x27);

  TEARDOWN();
}


TEST(msub) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Mov(x17, 1);
  __ Mov(x18, 0xffffffff);
  __ Mov(x19, 0xffffffffffffffffUL);

  __ Msub(w0, w16, w16, w16);
  __ Msub(w1, w16, w16, w17);
  __ Msub(w2, w16, w16, w18);
  __ Msub(w3, w16, w16, w19);
  __ Msub(w4, w16, w17, w17);
  __ Msub(w5, w17, w17, w18);
  __ Msub(w6, w17, w17, w19);
  __ Msub(w7, w17, w18, w16);
  __ Msub(w8, w17, w18, w18);
  __ Msub(w9, w18, w18, w17);
  __ Msub(w10, w18, w19, w18);
  __ Msub(w11, w19, w19, w19);

  __ Msub(x12, x16, x16, x16);
  __ Msub(x13, x16, x16, x17);
  __ Msub(x14, x16, x16, x18);
  __ Msub(x15, x16, x16, x19);
  __ Msub(x20, x16, x17, x17);
  __ Msub(x21, x17, x17, x18);
  __ Msub(x22, x17, x17, x19);
  __ Msub(x23, x17, x18, x16);
  __ Msub(x24, x17, x18, x18);
  __ Msub(x25, x18, x18, x17);
  __ Msub(x26, x18, x19, x18);
  __ Msub(x27, x19, x19, x19);

  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(0xffffffff, x2);
  CHECK_EQUAL_64(0xffffffff, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(0xfffffffe, x5);
  CHECK_EQUAL_64(0xfffffffe, x6);
  CHECK_EQUAL_64(1, x7);
  CHECK_EQUAL_64(0, x8);
  CHECK_EQUAL_64(0, x9);
  CHECK_EQUAL_64(0xfffffffe, x10);
  CHECK_EQUAL_64(0xfffffffe, x11);

  CHECK_EQUAL_64(0, x12);
  CHECK_EQUAL_64(1, x13);
  CHECK_EQUAL_64(0xffffffff, x14);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x15);
  CHECK_EQUAL_64(1, x20);
  CHECK_EQUAL_64(0xfffffffeUL, x21);
  CHECK_EQUAL_64(0xfffffffffffffffeUL, x22);
  CHECK_EQUAL_64(0xffffffff00000001UL, x23);
  CHECK_EQUAL_64(0, x24);
  CHECK_EQUAL_64(0x200000000UL, x25);
  CHECK_EQUAL_64(0x1fffffffeUL, x26);
  CHECK_EQUAL_64(0xfffffffffffffffeUL, x27);

  TEARDOWN();
}


TEST(smulh) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x20, 0);
  __ Mov(x21, 1);
  __ Mov(x22, 0x0000000100000000L);
  __ Mov(x23, 0x12345678);
  __ Mov(x24, 0x0123456789abcdefL);
  __ Mov(x25, 0x0000000200000000L);
  __ Mov(x26, 0x8000000000000000UL);
  __ Mov(x27, 0xffffffffffffffffUL);
  __ Mov(x28, 0x5555555555555555UL);
  __ Mov(x29, 0xaaaaaaaaaaaaaaaaUL);

  __ Smulh(x0, x20, x24);
  __ Smulh(x1, x21, x24);
  __ Smulh(x2, x22, x23);
  __ Smulh(x3, x22, x24);
  __ Smulh(x4, x24, x25);
  __ Smulh(x5, x23, x27);
  __ Smulh(x6, x26, x26);
  __ Smulh(x7, x26, x27);
  __ Smulh(x8, x27, x27);
  __ Smulh(x9, x28, x28);
  __ Smulh(x10, x28, x29);
  __ Smulh(x11, x29, x29);
  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(0, x1);
  CHECK_EQUAL_64(0, x2);
  CHECK_EQUAL_64(0x01234567, x3);
  CHECK_EQUAL_64(0x02468acf, x4);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x5);
  CHECK_EQUAL_64(0x4000000000000000UL, x6);
  CHECK_EQUAL_64(0, x7);
  CHECK_EQUAL_64(0, x8);
  CHECK_EQUAL_64(0x1c71c71c71c71c71UL, x9);
  CHECK_EQUAL_64(0xe38e38e38e38e38eUL, x10);
  CHECK_EQUAL_64(0x1c71c71c71c71c72UL, x11);

  TEARDOWN();
}


TEST(smaddl_umaddl) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x17, 1);
  __ Mov(x18, 0xffffffff);
  __ Mov(x19, 0xffffffffffffffffUL);
  __ Mov(x20, 4);
  __ Mov(x21, 0x200000000UL);

  __ Smaddl(x9, w17, w18, x20);
  __ Smaddl(x10, w18, w18, x20);
  __ Smaddl(x11, w19, w19, x20);
  __ Smaddl(x12, w19, w19, x21);
  __ Umaddl(x13, w17, w18, x20);
  __ Umaddl(x14, w18, w18, x20);
  __ Umaddl(x15, w19, w19, x20);
  __ Umaddl(x22, w19, w19, x21);
  END();

  RUN();

  CHECK_EQUAL_64(3, x9);
  CHECK_EQUAL_64(5, x10);
  CHECK_EQUAL_64(5, x11);
  CHECK_EQUAL_64(0x200000001UL, x12);
  CHECK_EQUAL_64(0x100000003UL, x13);
  CHECK_EQUAL_64(0xfffffffe00000005UL, x14);
  CHECK_EQUAL_64(0xfffffffe00000005UL, x15);
  CHECK_EQUAL_64(0x1, x22);

  TEARDOWN();
}


TEST(smsubl_umsubl) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x17, 1);
  __ Mov(x18, 0xffffffff);
  __ Mov(x19, 0xffffffffffffffffUL);
  __ Mov(x20, 4);
  __ Mov(x21, 0x200000000UL);

  __ Smsubl(x9, w17, w18, x20);
  __ Smsubl(x10, w18, w18, x20);
  __ Smsubl(x11, w19, w19, x20);
  __ Smsubl(x12, w19, w19, x21);
  __ Umsubl(x13, w17, w18, x20);
  __ Umsubl(x14, w18, w18, x20);
  __ Umsubl(x15, w19, w19, x20);
  __ Umsubl(x22, w19, w19, x21);
  END();

  RUN();

  CHECK_EQUAL_64(5, x9);
  CHECK_EQUAL_64(3, x10);
  CHECK_EQUAL_64(3, x11);
  CHECK_EQUAL_64(0x1ffffffffUL, x12);
  CHECK_EQUAL_64(0xffffffff00000005UL, x13);
  CHECK_EQUAL_64(0x200000003UL, x14);
  CHECK_EQUAL_64(0x200000003UL, x15);
  CHECK_EQUAL_64(0x3ffffffffUL, x22);

  TEARDOWN();
}


TEST(div) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 1);
  __ Mov(x17, 0xffffffff);
  __ Mov(x18, 0xffffffffffffffffUL);
  __ Mov(x19, 0x80000000);
  __ Mov(x20, 0x8000000000000000UL);
  __ Mov(x21, 2);

  __ Udiv(w0, w16, w16);
  __ Udiv(w1, w17, w16);
  __ Sdiv(w2, w16, w16);
  __ Sdiv(w3, w16, w17);
  __ Sdiv(w4, w17, w18);

  __ Udiv(x5, x16, x16);
  __ Udiv(x6, x17, x18);
  __ Sdiv(x7, x16, x16);
  __ Sdiv(x8, x16, x17);
  __ Sdiv(x9, x17, x18);

  __ Udiv(w10, w19, w21);
  __ Sdiv(w11, w19, w21);
  __ Udiv(x12, x19, x21);
  __ Sdiv(x13, x19, x21);
  __ Udiv(x14, x20, x21);
  __ Sdiv(x15, x20, x21);

  __ Udiv(w22, w19, w17);
  __ Sdiv(w23, w19, w17);
  __ Udiv(x24, x20, x18);
  __ Sdiv(x25, x20, x18);

  __ Udiv(x26, x16, x21);
  __ Sdiv(x27, x16, x21);
  __ Udiv(x28, x18, x21);
  __ Sdiv(x29, x18, x21);

  __ Mov(x17, 0);
  __ Udiv(w18, w16, w17);
  __ Sdiv(w19, w16, w17);
  __ Udiv(x20, x16, x17);
  __ Sdiv(x21, x16, x17);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(0xffffffff, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0xffffffff, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(1, x5);
  CHECK_EQUAL_64(0, x6);
  CHECK_EQUAL_64(1, x7);
  CHECK_EQUAL_64(0, x8);
  CHECK_EQUAL_64(0xffffffff00000001UL, x9);
  CHECK_EQUAL_64(0x40000000, x10);
  CHECK_EQUAL_64(0xC0000000, x11);
  CHECK_EQUAL_64(0x40000000, x12);
  CHECK_EQUAL_64(0x40000000, x13);
  CHECK_EQUAL_64(0x4000000000000000UL, x14);
  CHECK_EQUAL_64(0xC000000000000000UL, x15);
  CHECK_EQUAL_64(0, x22);
  CHECK_EQUAL_64(0x80000000, x23);
  CHECK_EQUAL_64(0, x24);
  CHECK_EQUAL_64(0x8000000000000000UL, x25);
  CHECK_EQUAL_64(0, x26);
  CHECK_EQUAL_64(0, x27);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x28);
  CHECK_EQUAL_64(0, x29);
  CHECK_EQUAL_64(0, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0, x20);
  CHECK_EQUAL_64(0, x21);

  TEARDOWN();
}


TEST(rbit_rev) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x24, 0xfedcba9876543210UL);
  __ Rbit(w0, w24);
  __ Rbit(x1, x24);
  __ Rev16(w2, w24);
  __ Rev16(x3, x24);
  __ Rev(w4, w24);
  __ Rev32(x5, x24);
  __ Rev(x6, x24);
  END();

  RUN();

  CHECK_EQUAL_64(0x084c2a6e, x0);
  CHECK_EQUAL_64(0x084c2a6e195d3b7fUL, x1);
  CHECK_EQUAL_64(0x54761032, x2);
  CHECK_EQUAL_64(0xdcfe98ba54761032UL, x3);
  CHECK_EQUAL_64(0x10325476, x4);
  CHECK_EQUAL_64(0x98badcfe10325476UL, x5);
  CHECK_EQUAL_64(0x1032547698badcfeUL, x6);

  TEARDOWN();
}


TEST(clz_cls) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x24, 0x0008000000800000UL);
  __ Mov(x25, 0xff800000fff80000UL);
  __ Mov(x26, 0);
  __ Clz(w0, w24);
  __ Clz(x1, x24);
  __ Clz(w2, w25);
  __ Clz(x3, x25);
  __ Clz(w4, w26);
  __ Clz(x5, x26);
  __ Cls(w6, w24);
  __ Cls(x7, x24);
  __ Cls(w8, w25);
  __ Cls(x9, x25);
  __ Cls(w10, w26);
  __ Cls(x11, x26);
  END();

  RUN();

  CHECK_EQUAL_64(8, x0);
  CHECK_EQUAL_64(12, x1);
  CHECK_EQUAL_64(0, x2);
  CHECK_EQUAL_64(0, x3);
  CHECK_EQUAL_64(32, x4);
  CHECK_EQUAL_64(64, x5);
  CHECK_EQUAL_64(7, x6);
  CHECK_EQUAL_64(11, x7);
  CHECK_EQUAL_64(12, x8);
  CHECK_EQUAL_64(8, x9);
  CHECK_EQUAL_64(31, x10);
  CHECK_EQUAL_64(63, x11);

  TEARDOWN();
}


TEST(label) {
  INIT_V8();
  SETUP();

  Label label_1, label_2, label_3, label_4;

  START();
  __ Mov(x0, 0x1);
  __ Mov(x1, 0x0);
  __ Mov(x22, lr);    // Save lr.

  __ B(&label_1);
  __ B(&label_1);
  __ B(&label_1);     // Multiple branches to the same label.
  __ Mov(x0, 0x0);
  __ Bind(&label_2);
  __ B(&label_3);     // Forward branch.
  __ Mov(x0, 0x0);
  __ Bind(&label_1);
  __ B(&label_2);     // Backward branch.
  __ Mov(x0, 0x0);
  __ Bind(&label_3);
  __ Bl(&label_4);
  END();

  __ Bind(&label_4);
  __ Mov(x1, 0x1);
  __ Mov(lr, x22);
  END();

  RUN();

  CHECK_EQUAL_64(0x1, x0);
  CHECK_EQUAL_64(0x1, x1);

  TEARDOWN();
}


TEST(branch_at_start) {
  INIT_V8();
  SETUP();

  Label good, exit;

  // Test that branches can exist at the start of the buffer. (This is a
  // boundary condition in the label-handling code.) To achieve this, we have
  // to work around the code generated by START.
  RESET();
  __ B(&good);

  START_AFTER_RESET();
  __ Mov(x0, 0x0);
  END();

  __ Bind(&exit);
  START_AFTER_RESET();
  __ Mov(x0, 0x1);
  END();

  __ Bind(&good);
  __ B(&exit);
  END();

  RUN();

  CHECK_EQUAL_64(0x1, x0);
  TEARDOWN();
}


TEST(adr) {
  INIT_V8();
  SETUP();

  Label label_1, label_2, label_3, label_4;

  START();
  __ Mov(x0, 0x0);        // Set to non-zero to indicate failure.
  __ Adr(x1, &label_3);   // Set to zero to indicate success.

  __ Adr(x2, &label_1);   // Multiple forward references to the same label.
  __ Adr(x3, &label_1);
  __ Adr(x4, &label_1);

  __ Bind(&label_2);
  __ Eor(x5, x2, Operand(x3));  // Ensure that x2,x3 and x4 are identical.
  __ Eor(x6, x2, Operand(x4));
  __ Orr(x0, x0, Operand(x5));
  __ Orr(x0, x0, Operand(x6));
  __ Br(x2);  // label_1, label_3

  __ Bind(&label_3);
  __ Adr(x2, &label_3);   // Self-reference (offset 0).
  __ Eor(x1, x1, Operand(x2));
  __ Adr(x2, &label_4);   // Simple forward reference.
  __ Br(x2);  // label_4

  __ Bind(&label_1);
  __ Adr(x2, &label_3);   // Multiple reverse references to the same label.
  __ Adr(x3, &label_3);
  __ Adr(x4, &label_3);
  __ Adr(x5, &label_2);   // Simple reverse reference.
  __ Br(x5);  // label_2

  __ Bind(&label_4);
  END();

  RUN();

  CHECK_EQUAL_64(0x0, x0);
  CHECK_EQUAL_64(0x0, x1);

  TEARDOWN();
}


TEST(adr_far) {
  INIT_V8();

  int max_range = 1 << (Instruction::ImmPCRelRangeBitwidth - 1);
  SETUP_SIZE(max_range + 1000 * kInstructionSize);

  Label done, fail;
  Label test_near, near_forward, near_backward;
  Label test_far, far_forward, far_backward;

  START();
  __ Mov(x0, 0x0);

  __ Bind(&test_near);
  __ Adr(x10, &near_forward, MacroAssembler::kAdrFar);
  __ Br(x10);
  __ B(&fail);
  __ Bind(&near_backward);
  __ Orr(x0, x0, 1 << 1);
  __ B(&test_far);

  __ Bind(&near_forward);
  __ Orr(x0, x0, 1 << 0);
  __ Adr(x10, &near_backward, MacroAssembler::kAdrFar);
  __ Br(x10);

  __ Bind(&test_far);
  __ Adr(x10, &far_forward, MacroAssembler::kAdrFar);
  __ Br(x10);
  __ B(&fail);
  __ Bind(&far_backward);
  __ Orr(x0, x0, 1 << 3);
  __ B(&done);

  for (unsigned i = 0; i < max_range / kInstructionSize + 1; ++i) {
    if (i % 100 == 0) {
      // If we do land in this code, we do not want to execute so many nops
      // before reaching the end of test (especially if tracing is activated).
      __ b(&fail);
    } else {
      __ nop();
    }
  }


  __ Bind(&far_forward);
  __ Orr(x0, x0, 1 << 2);
  __ Adr(x10, &far_backward, MacroAssembler::kAdrFar);
  __ Br(x10);

  __ B(&done);
  __ Bind(&fail);
  __ Orr(x0, x0, 1 << 4);
  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0xf, x0);

  TEARDOWN();
}


TEST(branch_cond) {
  INIT_V8();
  SETUP();

  Label wrong;

  START();
  __ Mov(x0, 0x1);
  __ Mov(x1, 0x1);
  __ Mov(x2, 0x8000000000000000L);

  // For each 'cmp' instruction below, condition codes other than the ones
  // following it would branch.

  __ Cmp(x1, 0);
  __ B(&wrong, eq);
  __ B(&wrong, lo);
  __ B(&wrong, mi);
  __ B(&wrong, vs);
  __ B(&wrong, ls);
  __ B(&wrong, lt);
  __ B(&wrong, le);
  Label ok_1;
  __ B(&ok_1, ne);
  __ Mov(x0, 0x0);
  __ Bind(&ok_1);

  __ Cmp(x1, 1);
  __ B(&wrong, ne);
  __ B(&wrong, lo);
  __ B(&wrong, mi);
  __ B(&wrong, vs);
  __ B(&wrong, hi);
  __ B(&wrong, lt);
  __ B(&wrong, gt);
  Label ok_2;
  __ B(&ok_2, pl);
  __ Mov(x0, 0x0);
  __ Bind(&ok_2);

  __ Cmp(x1, 2);
  __ B(&wrong, eq);
  __ B(&wrong, hs);
  __ B(&wrong, pl);
  __ B(&wrong, vs);
  __ B(&wrong, hi);
  __ B(&wrong, ge);
  __ B(&wrong, gt);
  Label ok_3;
  __ B(&ok_3, vc);
  __ Mov(x0, 0x0);
  __ Bind(&ok_3);

  __ Cmp(x2, 1);
  __ B(&wrong, eq);
  __ B(&wrong, lo);
  __ B(&wrong, mi);
  __ B(&wrong, vc);
  __ B(&wrong, ls);
  __ B(&wrong, ge);
  __ B(&wrong, gt);
  Label ok_4;
  __ B(&ok_4, le);
  __ Mov(x0, 0x0);
  __ Bind(&ok_4);

  Label ok_5;
  __ b(&ok_5, al);
  __ Mov(x0, 0x0);
  __ Bind(&ok_5);

  Label ok_6;
  __ b(&ok_6, nv);
  __ Mov(x0, 0x0);
  __ Bind(&ok_6);

  END();

  __ Bind(&wrong);
  __ Mov(x0, 0x0);
  END();

  RUN();

  CHECK_EQUAL_64(0x1, x0);

  TEARDOWN();
}


TEST(branch_to_reg) {
  INIT_V8();
  SETUP();

  // Test br.
  Label fn1, after_fn1;

  START();
  __ Mov(x29, lr);

  __ Mov(x1, 0);
  __ B(&after_fn1);

  __ Bind(&fn1);
  __ Mov(x0, lr);
  __ Mov(x1, 42);
  __ Br(x0);

  __ Bind(&after_fn1);
  __ Bl(&fn1);

  // Test blr.
  Label fn2, after_fn2;

  __ Mov(x2, 0);
  __ B(&after_fn2);

  __ Bind(&fn2);
  __ Mov(x0, lr);
  __ Mov(x2, 84);
  __ Blr(x0);

  __ Bind(&after_fn2);
  __ Bl(&fn2);
  __ Mov(x3, lr);

  __ Mov(lr, x29);
  END();

  RUN();

  CHECK_EQUAL_64(core.xreg(3) + kInstructionSize, x0);
  CHECK_EQUAL_64(42, x1);
  CHECK_EQUAL_64(84, x2);

  TEARDOWN();
}


TEST(compare_branch) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0);
  __ Mov(x2, 0);
  __ Mov(x3, 0);
  __ Mov(x4, 0);
  __ Mov(x5, 0);
  __ Mov(x16, 0);
  __ Mov(x17, 42);

  Label zt, zt_end;
  __ Cbz(w16, &zt);
  __ B(&zt_end);
  __ Bind(&zt);
  __ Mov(x0, 1);
  __ Bind(&zt_end);

  Label zf, zf_end;
  __ Cbz(x17, &zf);
  __ B(&zf_end);
  __ Bind(&zf);
  __ Mov(x1, 1);
  __ Bind(&zf_end);

  Label nzt, nzt_end;
  __ Cbnz(w17, &nzt);
  __ B(&nzt_end);
  __ Bind(&nzt);
  __ Mov(x2, 1);
  __ Bind(&nzt_end);

  Label nzf, nzf_end;
  __ Cbnz(x16, &nzf);
  __ B(&nzf_end);
  __ Bind(&nzf);
  __ Mov(x3, 1);
  __ Bind(&nzf_end);

  __ Mov(x18, 0xffffffff00000000UL);

  Label a, a_end;
  __ Cbz(w18, &a);
  __ B(&a_end);
  __ Bind(&a);
  __ Mov(x4, 1);
  __ Bind(&a_end);

  Label b, b_end;
  __ Cbnz(w18, &b);
  __ B(&b_end);
  __ Bind(&b);
  __ Mov(x5, 1);
  __ Bind(&b_end);

  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(0, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(0, x5);

  TEARDOWN();
}


TEST(test_branch) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0);
  __ Mov(x2, 0);
  __ Mov(x3, 0);
  __ Mov(x16, 0xaaaaaaaaaaaaaaaaUL);

  Label bz, bz_end;
  __ Tbz(w16, 0, &bz);
  __ B(&bz_end);
  __ Bind(&bz);
  __ Mov(x0, 1);
  __ Bind(&bz_end);

  Label bo, bo_end;
  __ Tbz(x16, 63, &bo);
  __ B(&bo_end);
  __ Bind(&bo);
  __ Mov(x1, 1);
  __ Bind(&bo_end);

  Label nbz, nbz_end;
  __ Tbnz(x16, 61, &nbz);
  __ B(&nbz_end);
  __ Bind(&nbz);
  __ Mov(x2, 1);
  __ Bind(&nbz_end);

  Label nbo, nbo_end;
  __ Tbnz(w16, 2, &nbo);
  __ B(&nbo_end);
  __ Bind(&nbo);
  __ Mov(x3, 1);
  __ Bind(&nbo_end);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(0, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0, x3);

  TEARDOWN();
}


TEST(far_branch_backward) {
  INIT_V8();

  // Test that the MacroAssembler correctly resolves backward branches to labels
  // that are outside the immediate range of branch instructions.
  int max_range =
    std::max(Instruction::ImmBranchRange(TestBranchType),
             std::max(Instruction::ImmBranchRange(CompareBranchType),
                      Instruction::ImmBranchRange(CondBranchType)));

  SETUP_SIZE(max_range + 1000 * kInstructionSize);

  START();

  Label done, fail;
  Label test_tbz, test_cbz, test_bcond;
  Label success_tbz, success_cbz, success_bcond;

  __ Mov(x0, 0);
  __ Mov(x1, 1);
  __ Mov(x10, 0);

  __ B(&test_tbz);
  __ Bind(&success_tbz);
  __ Orr(x0, x0, 1 << 0);
  __ B(&test_cbz);
  __ Bind(&success_cbz);
  __ Orr(x0, x0, 1 << 1);
  __ B(&test_bcond);
  __ Bind(&success_bcond);
  __ Orr(x0, x0, 1 << 2);

  __ B(&done);

  // Generate enough code to overflow the immediate range of the three types of
  // branches below.
  for (unsigned i = 0; i < max_range / kInstructionSize + 1; ++i) {
    if (i % 100 == 0) {
      // If we do land in this code, we do not want to execute so many nops
      // before reaching the end of test (especially if tracing is activated).
      __ B(&fail);
    } else {
      __ Nop();
    }
  }
  __ B(&fail);

  __ Bind(&test_tbz);
  __ Tbz(x10, 7, &success_tbz);
  __ Bind(&test_cbz);
  __ Cbz(x10, &success_cbz);
  __ Bind(&test_bcond);
  __ Cmp(x10, 0);
  __ B(eq, &success_bcond);

  // For each out-of-range branch instructions, at least two instructions should
  // have been generated.
  CHECK_GE(7 * kInstructionSize, __ SizeOfCodeGeneratedSince(&test_tbz));

  __ Bind(&fail);
  __ Mov(x1, 0);
  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x7, x0);
  CHECK_EQUAL_64(0x1, x1);

  TEARDOWN();
}


TEST(far_branch_simple_veneer) {
  INIT_V8();

  // Test that the MacroAssembler correctly emits veneers for forward branches
  // to labels that are outside the immediate range of branch instructions.
  int max_range =
    std::max(Instruction::ImmBranchRange(TestBranchType),
             std::max(Instruction::ImmBranchRange(CompareBranchType),
                      Instruction::ImmBranchRange(CondBranchType)));

  SETUP_SIZE(max_range + 1000 * kInstructionSize);

  START();

  Label done, fail;
  Label test_tbz, test_cbz, test_bcond;
  Label success_tbz, success_cbz, success_bcond;

  __ Mov(x0, 0);
  __ Mov(x1, 1);
  __ Mov(x10, 0);

  __ Bind(&test_tbz);
  __ Tbz(x10, 7, &success_tbz);
  __ Bind(&test_cbz);
  __ Cbz(x10, &success_cbz);
  __ Bind(&test_bcond);
  __ Cmp(x10, 0);
  __ B(eq, &success_bcond);

  // Generate enough code to overflow the immediate range of the three types of
  // branches below.
  for (unsigned i = 0; i < max_range / kInstructionSize + 1; ++i) {
    if (i % 100 == 0) {
      // If we do land in this code, we do not want to execute so many nops
      // before reaching the end of test (especially if tracing is activated).
      // Also, the branches give the MacroAssembler the opportunity to emit the
      // veneers.
      __ B(&fail);
    } else {
      __ Nop();
    }
  }
  __ B(&fail);

  __ Bind(&success_tbz);
  __ Orr(x0, x0, 1 << 0);
  __ B(&test_cbz);
  __ Bind(&success_cbz);
  __ Orr(x0, x0, 1 << 1);
  __ B(&test_bcond);
  __ Bind(&success_bcond);
  __ Orr(x0, x0, 1 << 2);

  __ B(&done);
  __ Bind(&fail);
  __ Mov(x1, 0);
  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x7, x0);
  CHECK_EQUAL_64(0x1, x1);

  TEARDOWN();
}


TEST(far_branch_veneer_link_chain) {
  INIT_V8();

  // Test that the MacroAssembler correctly emits veneers for forward branches
  // that target out-of-range labels and are part of multiple instructions
  // jumping to that label.
  //
  // We test the three situations with the different types of instruction:
  // (1)- When the branch is at the start of the chain with tbz.
  // (2)- When the branch is in the middle of the chain with cbz.
  // (3)- When the branch is at the end of the chain with bcond.
  int max_range =
    std::max(Instruction::ImmBranchRange(TestBranchType),
             std::max(Instruction::ImmBranchRange(CompareBranchType),
                      Instruction::ImmBranchRange(CondBranchType)));

  SETUP_SIZE(max_range + 1000 * kInstructionSize);

  START();

  Label skip, fail, done;
  Label test_tbz, test_cbz, test_bcond;
  Label success_tbz, success_cbz, success_bcond;

  __ Mov(x0, 0);
  __ Mov(x1, 1);
  __ Mov(x10, 0);

  __ B(&skip);
  // Branches at the start of the chain for situations (2) and (3).
  __ B(&success_cbz);
  __ B(&success_bcond);
  __ Nop();
  __ B(&success_bcond);
  __ B(&success_cbz);
  __ Bind(&skip);

  __ Bind(&test_tbz);
  __ Tbz(x10, 7, &success_tbz);
  __ Bind(&test_cbz);
  __ Cbz(x10, &success_cbz);
  __ Bind(&test_bcond);
  __ Cmp(x10, 0);
  __ B(eq, &success_bcond);

  skip.Unuse();
  __ B(&skip);
  // Branches at the end of the chain for situations (1) and (2).
  __ B(&success_cbz);
  __ B(&success_tbz);
  __ Nop();
  __ B(&success_tbz);
  __ B(&success_cbz);
  __ Bind(&skip);

  // Generate enough code to overflow the immediate range of the three types of
  // branches below.
  for (unsigned i = 0; i < max_range / kInstructionSize + 1; ++i) {
    if (i % 100 == 0) {
      // If we do land in this code, we do not want to execute so many nops
      // before reaching the end of test (especially if tracing is activated).
      // Also, the branches give the MacroAssembler the opportunity to emit the
      // veneers.
      __ B(&fail);
    } else {
      __ Nop();
    }
  }
  __ B(&fail);

  __ Bind(&success_tbz);
  __ Orr(x0, x0, 1 << 0);
  __ B(&test_cbz);
  __ Bind(&success_cbz);
  __ Orr(x0, x0, 1 << 1);
  __ B(&test_bcond);
  __ Bind(&success_bcond);
  __ Orr(x0, x0, 1 << 2);

  __ B(&done);
  __ Bind(&fail);
  __ Mov(x1, 0);
  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x7, x0);
  CHECK_EQUAL_64(0x1, x1);

  TEARDOWN();
}


TEST(far_branch_veneer_broken_link_chain) {
  INIT_V8();

  // Check that the MacroAssembler correctly handles the situation when removing
  // a branch from the link chain of a label and the two links on each side of
  // the removed branch cannot be linked together (out of range).
  //
  // We test with tbz because it has a small range.
  int max_range = Instruction::ImmBranchRange(TestBranchType);
  int inter_range = max_range / 2 + max_range / 10;

  SETUP_SIZE(3 * inter_range + 1000 * kInstructionSize);

  START();

  Label skip, fail, done;
  Label test_1, test_2, test_3;
  Label far_target;

  __ Mov(x0, 0);  // Indicates the origin of the branch.
  __ Mov(x1, 1);
  __ Mov(x10, 0);

  // First instruction in the label chain.
  __ Bind(&test_1);
  __ Mov(x0, 1);
  __ B(&far_target);

  for (unsigned i = 0; i < inter_range / kInstructionSize; ++i) {
    if (i % 100 == 0) {
      // Do not allow generating veneers. They should not be needed.
      __ b(&fail);
    } else {
      __ Nop();
    }
  }

  // Will need a veneer to point to reach the target.
  __ Bind(&test_2);
  __ Mov(x0, 2);
  __ Tbz(x10, 7, &far_target);

  for (unsigned i = 0; i < inter_range / kInstructionSize; ++i) {
    if (i % 100 == 0) {
      // Do not allow generating veneers. They should not be needed.
      __ b(&fail);
    } else {
      __ Nop();
    }
  }

  // Does not need a veneer to reach the target, but the initial branch
  // instruction is out of range.
  __ Bind(&test_3);
  __ Mov(x0, 3);
  __ Tbz(x10, 7, &far_target);

  for (unsigned i = 0; i < inter_range / kInstructionSize; ++i) {
    if (i % 100 == 0) {
      // Allow generating veneers.
      __ B(&fail);
    } else {
      __ Nop();
    }
  }

  __ B(&fail);

  __ Bind(&far_target);
  __ Cmp(x0, 1);
  __ B(eq, &test_2);
  __ Cmp(x0, 2);
  __ B(eq, &test_3);

  __ B(&done);
  __ Bind(&fail);
  __ Mov(x1, 0);
  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x3, x0);
  CHECK_EQUAL_64(0x1, x1);

  TEARDOWN();
}


TEST(branch_type) {
  INIT_V8();

  SETUP();

  Label fail, done;

  START();
  __ Mov(x0, 0x0);
  __ Mov(x10, 0x7);
  __ Mov(x11, 0x0);

  // Test non taken branches.
  __ Cmp(x10, 0x7);
  __ B(&fail, ne);
  __ B(&fail, never);
  __ B(&fail, reg_zero, x10);
  __ B(&fail, reg_not_zero, x11);
  __ B(&fail, reg_bit_clear, x10, 0);
  __ B(&fail, reg_bit_set, x10, 3);

  // Test taken branches.
  Label l1, l2, l3, l4, l5;
  __ Cmp(x10, 0x7);
  __ B(&l1, eq);
  __ B(&fail);
  __ Bind(&l1);
  __ B(&l2, always);
  __ B(&fail);
  __ Bind(&l2);
  __ B(&l3, reg_not_zero, x10);
  __ B(&fail);
  __ Bind(&l3);
  __ B(&l4, reg_bit_clear, x10, 15);
  __ B(&fail);
  __ Bind(&l4);
  __ B(&l5, reg_bit_set, x10, 1);
  __ B(&fail);
  __ Bind(&l5);

  __ B(&done);

  __ Bind(&fail);
  __ Mov(x0, 0x1);

  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x0, x0);

  TEARDOWN();
}


TEST(ldr_str_offset) {
  INIT_V8();
  SETUP();

  uint64_t src[2] = {0xfedcba9876543210UL, 0x0123456789abcdefUL};
  uint64_t dst[5] = {0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Ldr(w0, MemOperand(x17));
  __ Str(w0, MemOperand(x18));
  __ Ldr(w1, MemOperand(x17, 4));
  __ Str(w1, MemOperand(x18, 12));
  __ Ldr(x2, MemOperand(x17, 8));
  __ Str(x2, MemOperand(x18, 16));
  __ Ldrb(w3, MemOperand(x17, 1));
  __ Strb(w3, MemOperand(x18, 25));
  __ Ldrh(w4, MemOperand(x17, 2));
  __ Strh(w4, MemOperand(x18, 33));
  END();

  RUN();

  CHECK_EQUAL_64(0x76543210, x0);
  CHECK_EQUAL_64(0x76543210, dst[0]);
  CHECK_EQUAL_64(0xfedcba98, x1);
  CHECK_EQUAL_64(0xfedcba9800000000UL, dst[1]);
  CHECK_EQUAL_64(0x0123456789abcdefUL, x2);
  CHECK_EQUAL_64(0x0123456789abcdefUL, dst[2]);
  CHECK_EQUAL_64(0x32, x3);
  CHECK_EQUAL_64(0x3200, dst[3]);
  CHECK_EQUAL_64(0x7654, x4);
  CHECK_EQUAL_64(0x765400, dst[4]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base, x18);

  TEARDOWN();
}


TEST(ldr_str_wide) {
  INIT_V8();
  SETUP();

  uint32_t src[8192];
  uint32_t dst[8192];
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);
  memset(src, 0xaa, 8192 * sizeof(src[0]));
  memset(dst, 0xaa, 8192 * sizeof(dst[0]));
  src[0] = 0;
  src[6144] = 6144;
  src[8191] = 8191;

  START();
  __ Mov(x22, src_base);
  __ Mov(x23, dst_base);
  __ Mov(x24, src_base);
  __ Mov(x25, dst_base);
  __ Mov(x26, src_base);
  __ Mov(x27, dst_base);

  __ Ldr(w0, MemOperand(x22, 8191 * sizeof(src[0])));
  __ Str(w0, MemOperand(x23, 8191 * sizeof(dst[0])));
  __ Ldr(w1, MemOperand(x24, 4096 * sizeof(src[0]), PostIndex));
  __ Str(w1, MemOperand(x25, 4096 * sizeof(dst[0]), PostIndex));
  __ Ldr(w2, MemOperand(x26, 6144 * sizeof(src[0]), PreIndex));
  __ Str(w2, MemOperand(x27, 6144 * sizeof(dst[0]), PreIndex));
  END();

  RUN();

  CHECK_EQUAL_32(8191, w0);
  CHECK_EQUAL_32(8191, dst[8191]);
  CHECK_EQUAL_64(src_base, x22);
  CHECK_EQUAL_64(dst_base, x23);
  CHECK_EQUAL_32(0, w1);
  CHECK_EQUAL_32(0, dst[0]);
  CHECK_EQUAL_64(src_base + 4096 * sizeof(src[0]), x24);
  CHECK_EQUAL_64(dst_base + 4096 * sizeof(dst[0]), x25);
  CHECK_EQUAL_32(6144, w2);
  CHECK_EQUAL_32(6144, dst[6144]);
  CHECK_EQUAL_64(src_base + 6144 * sizeof(src[0]), x26);
  CHECK_EQUAL_64(dst_base + 6144 * sizeof(dst[0]), x27);

  TEARDOWN();
}


TEST(ldr_str_preindex) {
  INIT_V8();
  SETUP();

  uint64_t src[2] = {0xfedcba9876543210UL, 0x0123456789abcdefUL};
  uint64_t dst[6] = {0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base);
  __ Mov(x20, dst_base);
  __ Mov(x21, src_base + 16);
  __ Mov(x22, dst_base + 40);
  __ Mov(x23, src_base);
  __ Mov(x24, dst_base);
  __ Mov(x25, src_base);
  __ Mov(x26, dst_base);
  __ Ldr(w0, MemOperand(x17, 4, PreIndex));
  __ Str(w0, MemOperand(x18, 12, PreIndex));
  __ Ldr(x1, MemOperand(x19, 8, PreIndex));
  __ Str(x1, MemOperand(x20, 16, PreIndex));
  __ Ldr(w2, MemOperand(x21, -4, PreIndex));
  __ Str(w2, MemOperand(x22, -4, PreIndex));
  __ Ldrb(w3, MemOperand(x23, 1, PreIndex));
  __ Strb(w3, MemOperand(x24, 25, PreIndex));
  __ Ldrh(w4, MemOperand(x25, 3, PreIndex));
  __ Strh(w4, MemOperand(x26, 41, PreIndex));
  END();

  RUN();

  CHECK_EQUAL_64(0xfedcba98, x0);
  CHECK_EQUAL_64(0xfedcba9800000000UL, dst[1]);
  CHECK_EQUAL_64(0x0123456789abcdefUL, x1);
  CHECK_EQUAL_64(0x0123456789abcdefUL, dst[2]);
  CHECK_EQUAL_64(0x01234567, x2);
  CHECK_EQUAL_64(0x0123456700000000UL, dst[4]);
  CHECK_EQUAL_64(0x32, x3);
  CHECK_EQUAL_64(0x3200, dst[3]);
  CHECK_EQUAL_64(0x9876, x4);
  CHECK_EQUAL_64(0x987600, dst[5]);
  CHECK_EQUAL_64(src_base + 4, x17);
  CHECK_EQUAL_64(dst_base + 12, x18);
  CHECK_EQUAL_64(src_base + 8, x19);
  CHECK_EQUAL_64(dst_base + 16, x20);
  CHECK_EQUAL_64(src_base + 12, x21);
  CHECK_EQUAL_64(dst_base + 36, x22);
  CHECK_EQUAL_64(src_base + 1, x23);
  CHECK_EQUAL_64(dst_base + 25, x24);
  CHECK_EQUAL_64(src_base + 3, x25);
  CHECK_EQUAL_64(dst_base + 41, x26);

  TEARDOWN();
}


TEST(ldr_str_postindex) {
  INIT_V8();
  SETUP();

  uint64_t src[2] = {0xfedcba9876543210UL, 0x0123456789abcdefUL};
  uint64_t dst[6] = {0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base + 4);
  __ Mov(x18, dst_base + 12);
  __ Mov(x19, src_base + 8);
  __ Mov(x20, dst_base + 16);
  __ Mov(x21, src_base + 8);
  __ Mov(x22, dst_base + 32);
  __ Mov(x23, src_base + 1);
  __ Mov(x24, dst_base + 25);
  __ Mov(x25, src_base + 3);
  __ Mov(x26, dst_base + 41);
  __ Ldr(w0, MemOperand(x17, 4, PostIndex));
  __ Str(w0, MemOperand(x18, 12, PostIndex));
  __ Ldr(x1, MemOperand(x19, 8, PostIndex));
  __ Str(x1, MemOperand(x20, 16, PostIndex));
  __ Ldr(x2, MemOperand(x21, -8, PostIndex));
  __ Str(x2, MemOperand(x22, -32, PostIndex));
  __ Ldrb(w3, MemOperand(x23, 1, PostIndex));
  __ Strb(w3, MemOperand(x24, 5, PostIndex));
  __ Ldrh(w4, MemOperand(x25, -3, PostIndex));
  __ Strh(w4, MemOperand(x26, -41, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_64(0xfedcba98, x0);
  CHECK_EQUAL_64(0xfedcba9800000000UL, dst[1]);
  CHECK_EQUAL_64(0x0123456789abcdefUL, x1);
  CHECK_EQUAL_64(0x0123456789abcdefUL, dst[2]);
  CHECK_EQUAL_64(0x0123456789abcdefUL, x2);
  CHECK_EQUAL_64(0x0123456789abcdefUL, dst[4]);
  CHECK_EQUAL_64(0x32, x3);
  CHECK_EQUAL_64(0x3200, dst[3]);
  CHECK_EQUAL_64(0x9876, x4);
  CHECK_EQUAL_64(0x987600, dst[5]);
  CHECK_EQUAL_64(src_base + 8, x17);
  CHECK_EQUAL_64(dst_base + 24, x18);
  CHECK_EQUAL_64(src_base + 16, x19);
  CHECK_EQUAL_64(dst_base + 32, x20);
  CHECK_EQUAL_64(src_base, x21);
  CHECK_EQUAL_64(dst_base, x22);
  CHECK_EQUAL_64(src_base + 2, x23);
  CHECK_EQUAL_64(dst_base + 30, x24);
  CHECK_EQUAL_64(src_base, x25);
  CHECK_EQUAL_64(dst_base, x26);

  TEARDOWN();
}


TEST(load_signed) {
  INIT_V8();
  SETUP();

  uint32_t src[2] = {0x80008080, 0x7fff7f7f};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x24, src_base);
  __ Ldrsb(w0, MemOperand(x24));
  __ Ldrsb(w1, MemOperand(x24, 4));
  __ Ldrsh(w2, MemOperand(x24));
  __ Ldrsh(w3, MemOperand(x24, 4));
  __ Ldrsb(x4, MemOperand(x24));
  __ Ldrsb(x5, MemOperand(x24, 4));
  __ Ldrsh(x6, MemOperand(x24));
  __ Ldrsh(x7, MemOperand(x24, 4));
  __ Ldrsw(x8, MemOperand(x24));
  __ Ldrsw(x9, MemOperand(x24, 4));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffff80, x0);
  CHECK_EQUAL_64(0x0000007f, x1);
  CHECK_EQUAL_64(0xffff8080, x2);
  CHECK_EQUAL_64(0x00007f7f, x3);
  CHECK_EQUAL_64(0xffffffffffffff80UL, x4);
  CHECK_EQUAL_64(0x000000000000007fUL, x5);
  CHECK_EQUAL_64(0xffffffffffff8080UL, x6);
  CHECK_EQUAL_64(0x0000000000007f7fUL, x7);
  CHECK_EQUAL_64(0xffffffff80008080UL, x8);
  CHECK_EQUAL_64(0x000000007fff7f7fUL, x9);

  TEARDOWN();
}


TEST(load_store_regoffset) {
  INIT_V8();
  SETUP();

  uint32_t src[3] = {1, 2, 3};
  uint32_t dst[4] = {0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Mov(x18, src_base + 3 * sizeof(src[0]));
  __ Mov(x19, dst_base + 3 * sizeof(dst[0]));
  __ Mov(x20, dst_base + 4 * sizeof(dst[0]));
  __ Mov(x24, 0);
  __ Mov(x25, 4);
  __ Mov(x26, -4);
  __ Mov(x27, 0xfffffffc);  // 32-bit -4.
  __ Mov(x28, 0xfffffffe);  // 32-bit -2.
  __ Mov(x29, 0xffffffff);  // 32-bit -1.

  __ Ldr(w0, MemOperand(x16, x24));
  __ Ldr(x1, MemOperand(x16, x25));
  __ Ldr(w2, MemOperand(x18, x26));
  __ Ldr(w3, MemOperand(x18, x27, SXTW));
  __ Ldr(w4, MemOperand(x18, x28, SXTW, 2));
  __ Str(w0, MemOperand(x17, x24));
  __ Str(x1, MemOperand(x17, x25));
  __ Str(w2, MemOperand(x20, x29, SXTW, 2));
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(0x0000000300000002UL, x1);
  CHECK_EQUAL_64(3, x2);
  CHECK_EQUAL_64(3, x3);
  CHECK_EQUAL_64(2, x4);
  CHECK_EQUAL_32(1, dst[0]);
  CHECK_EQUAL_32(2, dst[1]);
  CHECK_EQUAL_32(3, dst[2]);
  CHECK_EQUAL_32(3, dst[3]);

  TEARDOWN();
}


TEST(load_store_float) {
  INIT_V8();
  SETUP();

  float src[3] = {1.0, 2.0, 3.0};
  float dst[3] = {0.0, 0.0, 0.0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base);
  __ Mov(x20, dst_base);
  __ Mov(x21, src_base);
  __ Mov(x22, dst_base);
  __ Ldr(s0, MemOperand(x17, sizeof(src[0])));
  __ Str(s0, MemOperand(x18, sizeof(dst[0]), PostIndex));
  __ Ldr(s1, MemOperand(x19, sizeof(src[0]), PostIndex));
  __ Str(s1, MemOperand(x20, 2 * sizeof(dst[0]), PreIndex));
  __ Ldr(s2, MemOperand(x21, 2 * sizeof(src[0]), PreIndex));
  __ Str(s2, MemOperand(x22, sizeof(dst[0])));
  END();

  RUN();

  CHECK_EQUAL_FP32(2.0, s0);
  CHECK_EQUAL_FP32(2.0, dst[0]);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(1.0, dst[2]);
  CHECK_EQUAL_FP32(3.0, s2);
  CHECK_EQUAL_FP32(3.0, dst[1]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base + sizeof(dst[0]), x18);
  CHECK_EQUAL_64(src_base + sizeof(src[0]), x19);
  CHECK_EQUAL_64(dst_base + 2 * sizeof(dst[0]), x20);
  CHECK_EQUAL_64(src_base + 2 * sizeof(src[0]), x21);
  CHECK_EQUAL_64(dst_base, x22);

  TEARDOWN();
}


TEST(load_store_double) {
  INIT_V8();
  SETUP();

  double src[3] = {1.0, 2.0, 3.0};
  double dst[3] = {0.0, 0.0, 0.0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base);
  __ Mov(x20, dst_base);
  __ Mov(x21, src_base);
  __ Mov(x22, dst_base);
  __ Ldr(d0, MemOperand(x17, sizeof(src[0])));
  __ Str(d0, MemOperand(x18, sizeof(dst[0]), PostIndex));
  __ Ldr(d1, MemOperand(x19, sizeof(src[0]), PostIndex));
  __ Str(d1, MemOperand(x20, 2 * sizeof(dst[0]), PreIndex));
  __ Ldr(d2, MemOperand(x21, 2 * sizeof(src[0]), PreIndex));
  __ Str(d2, MemOperand(x22, sizeof(dst[0])));
  END();

  RUN();

  CHECK_EQUAL_FP64(2.0, d0);
  CHECK_EQUAL_FP64(2.0, dst[0]);
  CHECK_EQUAL_FP64(1.0, d1);
  CHECK_EQUAL_FP64(1.0, dst[2]);
  CHECK_EQUAL_FP64(3.0, d2);
  CHECK_EQUAL_FP64(3.0, dst[1]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base + sizeof(dst[0]), x18);
  CHECK_EQUAL_64(src_base + sizeof(src[0]), x19);
  CHECK_EQUAL_64(dst_base + 2 * sizeof(dst[0]), x20);
  CHECK_EQUAL_64(src_base + 2 * sizeof(src[0]), x21);
  CHECK_EQUAL_64(dst_base, x22);

  TEARDOWN();
}


TEST(ldp_stp_float) {
  INIT_V8();
  SETUP();

  float src[2] = {1.0, 2.0};
  float dst[3] = {0.0, 0.0, 0.0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Ldp(s31, s0, MemOperand(x16, 2 * sizeof(src[0]), PostIndex));
  __ Stp(s0, s31, MemOperand(x17, sizeof(dst[1]), PreIndex));
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s31);
  CHECK_EQUAL_FP32(2.0, s0);
  CHECK_EQUAL_FP32(0.0, dst[0]);
  CHECK_EQUAL_FP32(2.0, dst[1]);
  CHECK_EQUAL_FP32(1.0, dst[2]);
  CHECK_EQUAL_64(src_base + 2 * sizeof(src[0]), x16);
  CHECK_EQUAL_64(dst_base + sizeof(dst[1]), x17);

  TEARDOWN();
}


TEST(ldp_stp_double) {
  INIT_V8();
  SETUP();

  double src[2] = {1.0, 2.0};
  double dst[3] = {0.0, 0.0, 0.0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Ldp(d31, d0, MemOperand(x16, 2 * sizeof(src[0]), PostIndex));
  __ Stp(d0, d31, MemOperand(x17, sizeof(dst[1]), PreIndex));
  END();

  RUN();

  CHECK_EQUAL_FP64(1.0, d31);
  CHECK_EQUAL_FP64(2.0, d0);
  CHECK_EQUAL_FP64(0.0, dst[0]);
  CHECK_EQUAL_FP64(2.0, dst[1]);
  CHECK_EQUAL_FP64(1.0, dst[2]);
  CHECK_EQUAL_64(src_base + 2 * sizeof(src[0]), x16);
  CHECK_EQUAL_64(dst_base + sizeof(dst[1]), x17);

  TEARDOWN();
}


TEST(ldp_stp_offset) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677UL, 0x8899aabbccddeeffUL,
                     0xffeeddccbbaa9988UL};
  uint64_t dst[7] = {0, 0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Mov(x18, src_base + 24);
  __ Mov(x19, dst_base + 56);
  __ Ldp(w0, w1, MemOperand(x16));
  __ Ldp(w2, w3, MemOperand(x16, 4));
  __ Ldp(x4, x5, MemOperand(x16, 8));
  __ Ldp(w6, w7, MemOperand(x18, -12));
  __ Ldp(x8, x9, MemOperand(x18, -16));
  __ Stp(w0, w1, MemOperand(x17));
  __ Stp(w2, w3, MemOperand(x17, 8));
  __ Stp(x4, x5, MemOperand(x17, 16));
  __ Stp(w6, w7, MemOperand(x19, -24));
  __ Stp(x8, x9, MemOperand(x19, -16));
  END();

  RUN();

  CHECK_EQUAL_64(0x44556677, x0);
  CHECK_EQUAL_64(0x00112233, x1);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[0]);
  CHECK_EQUAL_64(0x00112233, x2);
  CHECK_EQUAL_64(0xccddeeff, x3);
  CHECK_EQUAL_64(0xccddeeff00112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x4);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[2]);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x5);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[3]);
  CHECK_EQUAL_64(0x8899aabb, x6);
  CHECK_EQUAL_64(0xbbaa9988, x7);
  CHECK_EQUAL_64(0xbbaa99888899aabbUL, dst[4]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x8);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[5]);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x9);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[6]);
  CHECK_EQUAL_64(src_base, x16);
  CHECK_EQUAL_64(dst_base, x17);
  CHECK_EQUAL_64(src_base + 24, x18);
  CHECK_EQUAL_64(dst_base + 56, x19);

  TEARDOWN();
}


TEST(ldp_stp_offset_wide) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677, 0x8899aabbccddeeff,
                     0xffeeddccbbaa9988};
  uint64_t dst[7] = {0, 0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);
  // Move base too far from the array to force multiple instructions
  // to be emitted.
  const int64_t base_offset = 1024;

  START();
  __ Mov(x20, src_base - base_offset);
  __ Mov(x21, dst_base - base_offset);
  __ Mov(x18, src_base + base_offset + 24);
  __ Mov(x19, dst_base + base_offset + 56);
  __ Ldp(w0, w1, MemOperand(x20, base_offset));
  __ Ldp(w2, w3, MemOperand(x20, base_offset + 4));
  __ Ldp(x4, x5, MemOperand(x20, base_offset + 8));
  __ Ldp(w6, w7, MemOperand(x18, -12 - base_offset));
  __ Ldp(x8, x9, MemOperand(x18, -16 - base_offset));
  __ Stp(w0, w1, MemOperand(x21, base_offset));
  __ Stp(w2, w3, MemOperand(x21, base_offset + 8));
  __ Stp(x4, x5, MemOperand(x21, base_offset + 16));
  __ Stp(w6, w7, MemOperand(x19, -24 - base_offset));
  __ Stp(x8, x9, MemOperand(x19, -16 - base_offset));
  END();

  RUN();

  CHECK_EQUAL_64(0x44556677, x0);
  CHECK_EQUAL_64(0x00112233, x1);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[0]);
  CHECK_EQUAL_64(0x00112233, x2);
  CHECK_EQUAL_64(0xccddeeff, x3);
  CHECK_EQUAL_64(0xccddeeff00112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x4);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[2]);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x5);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[3]);
  CHECK_EQUAL_64(0x8899aabb, x6);
  CHECK_EQUAL_64(0xbbaa9988, x7);
  CHECK_EQUAL_64(0xbbaa99888899aabbUL, dst[4]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x8);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[5]);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x9);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[6]);
  CHECK_EQUAL_64(src_base - base_offset, x20);
  CHECK_EQUAL_64(dst_base - base_offset, x21);
  CHECK_EQUAL_64(src_base + base_offset + 24, x18);
  CHECK_EQUAL_64(dst_base + base_offset + 56, x19);

  TEARDOWN();
}


TEST(ldnp_stnp_offset) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677UL, 0x8899aabbccddeeffUL,
                     0xffeeddccbbaa9988UL};
  uint64_t dst[7] = {0, 0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Mov(x18, src_base + 24);
  __ Mov(x19, dst_base + 56);
  __ Ldnp(w0, w1, MemOperand(x16));
  __ Ldnp(w2, w3, MemOperand(x16, 4));
  __ Ldnp(x4, x5, MemOperand(x16, 8));
  __ Ldnp(w6, w7, MemOperand(x18, -12));
  __ Ldnp(x8, x9, MemOperand(x18, -16));
  __ Stnp(w0, w1, MemOperand(x17));
  __ Stnp(w2, w3, MemOperand(x17, 8));
  __ Stnp(x4, x5, MemOperand(x17, 16));
  __ Stnp(w6, w7, MemOperand(x19, -24));
  __ Stnp(x8, x9, MemOperand(x19, -16));
  END();

  RUN();

  CHECK_EQUAL_64(0x44556677, x0);
  CHECK_EQUAL_64(0x00112233, x1);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[0]);
  CHECK_EQUAL_64(0x00112233, x2);
  CHECK_EQUAL_64(0xccddeeff, x3);
  CHECK_EQUAL_64(0xccddeeff00112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x4);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[2]);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x5);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[3]);
  CHECK_EQUAL_64(0x8899aabb, x6);
  CHECK_EQUAL_64(0xbbaa9988, x7);
  CHECK_EQUAL_64(0xbbaa99888899aabbUL, dst[4]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x8);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[5]);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x9);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[6]);
  CHECK_EQUAL_64(src_base, x16);
  CHECK_EQUAL_64(dst_base, x17);
  CHECK_EQUAL_64(src_base + 24, x18);
  CHECK_EQUAL_64(dst_base + 56, x19);

  TEARDOWN();
}


TEST(ldp_stp_preindex) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677UL, 0x8899aabbccddeeffUL,
                     0xffeeddccbbaa9988UL};
  uint64_t dst[5] = {0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Mov(x18, dst_base + 16);
  __ Ldp(w0, w1, MemOperand(x16, 4, PreIndex));
  __ Mov(x19, x16);
  __ Ldp(w2, w3, MemOperand(x16, -4, PreIndex));
  __ Stp(w2, w3, MemOperand(x17, 4, PreIndex));
  __ Mov(x20, x17);
  __ Stp(w0, w1, MemOperand(x17, -4, PreIndex));
  __ Ldp(x4, x5, MemOperand(x16, 8, PreIndex));
  __ Mov(x21, x16);
  __ Ldp(x6, x7, MemOperand(x16, -8, PreIndex));
  __ Stp(x7, x6, MemOperand(x18, 8, PreIndex));
  __ Mov(x22, x18);
  __ Stp(x5, x4, MemOperand(x18, -8, PreIndex));
  END();

  RUN();

  CHECK_EQUAL_64(0x00112233, x0);
  CHECK_EQUAL_64(0xccddeeff, x1);
  CHECK_EQUAL_64(0x44556677, x2);
  CHECK_EQUAL_64(0x00112233, x3);
  CHECK_EQUAL_64(0xccddeeff00112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x4);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x5);
  CHECK_EQUAL_64(0x0011223344556677UL, x6);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x7);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[3]);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[4]);
  CHECK_EQUAL_64(src_base, x16);
  CHECK_EQUAL_64(dst_base, x17);
  CHECK_EQUAL_64(dst_base + 16, x18);
  CHECK_EQUAL_64(src_base + 4, x19);
  CHECK_EQUAL_64(dst_base + 4, x20);
  CHECK_EQUAL_64(src_base + 8, x21);
  CHECK_EQUAL_64(dst_base + 24, x22);

  TEARDOWN();
}


TEST(ldp_stp_preindex_wide) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677, 0x8899aabbccddeeff,
                     0xffeeddccbbaa9988};
  uint64_t dst[5] = {0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);
  // Move base too far from the array to force multiple instructions
  // to be emitted.
  const int64_t base_offset = 1024;

  START();
  __ Mov(x24, src_base - base_offset);
  __ Mov(x25, dst_base + base_offset);
  __ Mov(x18, dst_base + base_offset + 16);
  __ Ldp(w0, w1, MemOperand(x24, base_offset + 4, PreIndex));
  __ Mov(x19, x24);
  __ Mov(x24, src_base - base_offset + 4);
  __ Ldp(w2, w3, MemOperand(x24, base_offset - 4, PreIndex));
  __ Stp(w2, w3, MemOperand(x25, 4 - base_offset, PreIndex));
  __ Mov(x20, x25);
  __ Mov(x25, dst_base + base_offset + 4);
  __ Mov(x24, src_base - base_offset);
  __ Stp(w0, w1, MemOperand(x25, -4 - base_offset, PreIndex));
  __ Ldp(x4, x5, MemOperand(x24, base_offset + 8, PreIndex));
  __ Mov(x21, x24);
  __ Mov(x24, src_base - base_offset + 8);
  __ Ldp(x6, x7, MemOperand(x24, base_offset - 8, PreIndex));
  __ Stp(x7, x6, MemOperand(x18, 8 - base_offset, PreIndex));
  __ Mov(x22, x18);
  __ Mov(x18, dst_base + base_offset + 16 + 8);
  __ Stp(x5, x4, MemOperand(x18, -8 - base_offset, PreIndex));
  END();

  RUN();

  CHECK_EQUAL_64(0x00112233, x0);
  CHECK_EQUAL_64(0xccddeeff, x1);
  CHECK_EQUAL_64(0x44556677, x2);
  CHECK_EQUAL_64(0x00112233, x3);
  CHECK_EQUAL_64(0xccddeeff00112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x4);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x5);
  CHECK_EQUAL_64(0x0011223344556677UL, x6);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x7);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[3]);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[4]);
  CHECK_EQUAL_64(src_base, x24);
  CHECK_EQUAL_64(dst_base, x25);
  CHECK_EQUAL_64(dst_base + 16, x18);
  CHECK_EQUAL_64(src_base + 4, x19);
  CHECK_EQUAL_64(dst_base + 4, x20);
  CHECK_EQUAL_64(src_base + 8, x21);
  CHECK_EQUAL_64(dst_base + 24, x22);

  TEARDOWN();
}


TEST(ldp_stp_postindex) {
  INIT_V8();
  SETUP();

  uint64_t src[4] = {0x0011223344556677UL, 0x8899aabbccddeeffUL,
                     0xffeeddccbbaa9988UL, 0x7766554433221100UL};
  uint64_t dst[5] = {0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Mov(x18, dst_base + 16);
  __ Ldp(w0, w1, MemOperand(x16, 4, PostIndex));
  __ Mov(x19, x16);
  __ Ldp(w2, w3, MemOperand(x16, -4, PostIndex));
  __ Stp(w2, w3, MemOperand(x17, 4, PostIndex));
  __ Mov(x20, x17);
  __ Stp(w0, w1, MemOperand(x17, -4, PostIndex));
  __ Ldp(x4, x5, MemOperand(x16, 8, PostIndex));
  __ Mov(x21, x16);
  __ Ldp(x6, x7, MemOperand(x16, -8, PostIndex));
  __ Stp(x7, x6, MemOperand(x18, 8, PostIndex));
  __ Mov(x22, x18);
  __ Stp(x5, x4, MemOperand(x18, -8, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_64(0x44556677, x0);
  CHECK_EQUAL_64(0x00112233, x1);
  CHECK_EQUAL_64(0x00112233, x2);
  CHECK_EQUAL_64(0xccddeeff, x3);
  CHECK_EQUAL_64(0x4455667700112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x0011223344556677UL, x4);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x5);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x6);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x7);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[3]);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[4]);
  CHECK_EQUAL_64(src_base, x16);
  CHECK_EQUAL_64(dst_base, x17);
  CHECK_EQUAL_64(dst_base + 16, x18);
  CHECK_EQUAL_64(src_base + 4, x19);
  CHECK_EQUAL_64(dst_base + 4, x20);
  CHECK_EQUAL_64(src_base + 8, x21);
  CHECK_EQUAL_64(dst_base + 24, x22);

  TEARDOWN();
}


TEST(ldp_stp_postindex_wide) {
  INIT_V8();
  SETUP();

  uint64_t src[4] = {0x0011223344556677, 0x8899aabbccddeeff, 0xffeeddccbbaa9988,
                     0x7766554433221100};
  uint64_t dst[5] = {0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);
  // Move base too far from the array to force multiple instructions
  // to be emitted.
  const int64_t base_offset = 1024;

  START();
  __ Mov(x24, src_base);
  __ Mov(x25, dst_base);
  __ Mov(x18, dst_base + 16);
  __ Ldp(w0, w1, MemOperand(x24, base_offset + 4, PostIndex));
  __ Mov(x19, x24);
  __ Sub(x24, x24, base_offset);
  __ Ldp(w2, w3, MemOperand(x24, base_offset - 4, PostIndex));
  __ Stp(w2, w3, MemOperand(x25, 4 - base_offset, PostIndex));
  __ Mov(x20, x25);
  __ Sub(x24, x24, base_offset);
  __ Add(x25, x25, base_offset);
  __ Stp(w0, w1, MemOperand(x25, -4 - base_offset, PostIndex));
  __ Ldp(x4, x5, MemOperand(x24, base_offset + 8, PostIndex));
  __ Mov(x21, x24);
  __ Sub(x24, x24, base_offset);
  __ Ldp(x6, x7, MemOperand(x24, base_offset - 8, PostIndex));
  __ Stp(x7, x6, MemOperand(x18, 8 - base_offset, PostIndex));
  __ Mov(x22, x18);
  __ Add(x18, x18, base_offset);
  __ Stp(x5, x4, MemOperand(x18, -8 - base_offset, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_64(0x44556677, x0);
  CHECK_EQUAL_64(0x00112233, x1);
  CHECK_EQUAL_64(0x00112233, x2);
  CHECK_EQUAL_64(0xccddeeff, x3);
  CHECK_EQUAL_64(0x4455667700112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x0011223344556677UL, x4);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x5);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, x6);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, x7);
  CHECK_EQUAL_64(0xffeeddccbbaa9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899aabbccddeeffUL, dst[3]);
  CHECK_EQUAL_64(0x0011223344556677UL, dst[4]);
  CHECK_EQUAL_64(src_base + base_offset, x24);
  CHECK_EQUAL_64(dst_base - base_offset, x25);
  CHECK_EQUAL_64(dst_base - base_offset + 16, x18);
  CHECK_EQUAL_64(src_base + base_offset + 4, x19);
  CHECK_EQUAL_64(dst_base - base_offset + 4, x20);
  CHECK_EQUAL_64(src_base + base_offset + 8, x21);
  CHECK_EQUAL_64(dst_base - base_offset + 24, x22);

  TEARDOWN();
}


TEST(ldp_sign_extend) {
  INIT_V8();
  SETUP();

  uint32_t src[2] = {0x80000000, 0x7fffffff};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x24, src_base);
  __ Ldpsw(x0, x1, MemOperand(x24));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffff80000000UL, x0);
  CHECK_EQUAL_64(0x000000007fffffffUL, x1);

  TEARDOWN();
}


TEST(ldur_stur) {
  INIT_V8();
  SETUP();

  int64_t src[2] = {0x0123456789abcdefUL, 0x0123456789abcdefUL};
  int64_t dst[5] = {0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base + 16);
  __ Mov(x20, dst_base + 32);
  __ Mov(x21, dst_base + 40);
  __ Ldr(w0, MemOperand(x17, 1));
  __ Str(w0, MemOperand(x18, 2));
  __ Ldr(x1, MemOperand(x17, 3));
  __ Str(x1, MemOperand(x18, 9));
  __ Ldr(w2, MemOperand(x19, -9));
  __ Str(w2, MemOperand(x20, -5));
  __ Ldrb(w3, MemOperand(x19, -1));
  __ Strb(w3, MemOperand(x21, -1));
  END();

  RUN();

  CHECK_EQUAL_64(0x6789abcd, x0);
  CHECK_EQUAL_64(0x6789abcd0000L, dst[0]);
  CHECK_EQUAL_64(0xabcdef0123456789L, x1);
  CHECK_EQUAL_64(0xcdef012345678900L, dst[1]);
  CHECK_EQUAL_64(0x000000ab, dst[2]);
  CHECK_EQUAL_64(0xabcdef01, x2);
  CHECK_EQUAL_64(0x00abcdef01000000L, dst[3]);
  CHECK_EQUAL_64(0x00000001, x3);
  CHECK_EQUAL_64(0x0100000000000000L, dst[4]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base, x18);
  CHECK_EQUAL_64(src_base + 16, x19);
  CHECK_EQUAL_64(dst_base + 32, x20);

  TEARDOWN();
}


#if 0  // TODO(all) enable.
// TODO(rodolph): Adapt w16 Literal tests for RelocInfo.
TEST(ldr_literal) {
  INIT_V8();
  SETUP();

  START();
  __ Ldr(x2, 0x1234567890abcdefUL);
  __ Ldr(w3, 0xfedcba09);
  __ Ldr(d13, 1.234);
  __ Ldr(s25, 2.5);
  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890abcdefUL, x2);
  CHECK_EQUAL_64(0xfedcba09, x3);
  CHECK_EQUAL_FP64(1.234, d13);
  CHECK_EQUAL_FP32(2.5, s25);

  TEARDOWN();
}


static void LdrLiteralRangeHelper(ptrdiff_t range_,
                                  LiteralPoolEmitOption option,
                                  bool expect_dump) {
  DCHECK(range_ > 0);
  SETUP_SIZE(range_ + 1024);

  Label label_1, label_2;

  size_t range = static_cast<size_t>(range_);
  size_t code_size = 0;
  size_t pool_guard_size;

  if (option == NoJumpRequired) {
    // Space for an explicit branch.
    pool_guard_size = sizeof(Instr);
  } else {
    pool_guard_size = 0;
  }

  START();
  // Force a pool dump so the pool starts off empty.
  __ EmitLiteralPool(JumpRequired);
  DCHECK_LITERAL_POOL_SIZE(0);

  __ Ldr(x0, 0x1234567890abcdefUL);
  __ Ldr(w1, 0xfedcba09);
  __ Ldr(d0, 1.234);
  __ Ldr(s1, 2.5);
  DCHECK_LITERAL_POOL_SIZE(4);

  code_size += 4 * sizeof(Instr);

  // Check that the requested range (allowing space for a branch over the pool)
  // can be handled by this test.
  DCHECK((code_size + pool_guard_size) <= range);

  // Emit NOPs up to 'range', leaving space for the pool guard.
  while ((code_size + pool_guard_size) < range) {
    __ Nop();
    code_size += sizeof(Instr);
  }

  // Emit the guard sequence before the literal pool.
  if (option == NoJumpRequired) {
    __ B(&label_1);
    code_size += sizeof(Instr);
  }

  DCHECK(code_size == range);
  DCHECK_LITERAL_POOL_SIZE(4);

  // Possibly generate a literal pool.
  __ CheckLiteralPool(option);
  __ Bind(&label_1);
  if (expect_dump) {
    DCHECK_LITERAL_POOL_SIZE(0);
  } else {
    DCHECK_LITERAL_POOL_SIZE(4);
  }

  // Force a pool flush to check that a second pool functions correctly.
  __ EmitLiteralPool(JumpRequired);
  DCHECK_LITERAL_POOL_SIZE(0);

  // These loads should be after the pool (and will require a new one).
  __ Ldr(x4, 0x34567890abcdef12UL);
  __ Ldr(w5, 0xdcba09fe);
  __ Ldr(d4, 123.4);
  __ Ldr(s5, 250.0);
  DCHECK_LITERAL_POOL_SIZE(4);
  END();

  RUN();

  // Check that the literals loaded correctly.
  CHECK_EQUAL_64(0x1234567890abcdefUL, x0);
  CHECK_EQUAL_64(0xfedcba09, x1);
  CHECK_EQUAL_FP64(1.234, d0);
  CHECK_EQUAL_FP32(2.5, s1);
  CHECK_EQUAL_64(0x34567890abcdef12UL, x4);
  CHECK_EQUAL_64(0xdcba09fe, x5);
  CHECK_EQUAL_FP64(123.4, d4);
  CHECK_EQUAL_FP32(250.0, s5);

  TEARDOWN();
}


TEST(ldr_literal_range_1) {
  INIT_V8();
  LdrLiteralRangeHelper(kRecommendedLiteralPoolRange,
                        NoJumpRequired,
                        true);
}


TEST(ldr_literal_range_2) {
  INIT_V8();
  LdrLiteralRangeHelper(kRecommendedLiteralPoolRange-sizeof(Instr),
                        NoJumpRequired,
                        false);
}


TEST(ldr_literal_range_3) {
  INIT_V8();
  LdrLiteralRangeHelper(2 * kRecommendedLiteralPoolRange,
                        JumpRequired,
                        true);
}


TEST(ldr_literal_range_4) {
  INIT_V8();
  LdrLiteralRangeHelper(2 * kRecommendedLiteralPoolRange-sizeof(Instr),
                        JumpRequired,
                        false);
}


TEST(ldr_literal_range_5) {
  INIT_V8();
  LdrLiteralRangeHelper(kLiteralPoolCheckInterval,
                        JumpRequired,
                        false);
}


TEST(ldr_literal_range_6) {
  INIT_V8();
  LdrLiteralRangeHelper(kLiteralPoolCheckInterval-sizeof(Instr),
                        JumpRequired,
                        false);
}
#endif

TEST(add_sub_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0x0);
  __ Mov(x1, 0x1111);
  __ Mov(x2, 0xffffffffffffffffL);
  __ Mov(x3, 0x8000000000000000L);

  __ Add(x10, x0, Operand(0x123));
  __ Add(x11, x1, Operand(0x122000));
  __ Add(x12, x0, Operand(0xabc << 12));
  __ Add(x13, x2, Operand(1));

  __ Add(w14, w0, Operand(0x123));
  __ Add(w15, w1, Operand(0x122000));
  __ Add(w16, w0, Operand(0xabc << 12));
  __ Add(w17, w2, Operand(1));

  __ Sub(x20, x0, Operand(0x1));
  __ Sub(x21, x1, Operand(0x111));
  __ Sub(x22, x1, Operand(0x1 << 12));
  __ Sub(x23, x3, Operand(1));

  __ Sub(w24, w0, Operand(0x1));
  __ Sub(w25, w1, Operand(0x111));
  __ Sub(w26, w1, Operand(0x1 << 12));
  __ Sub(w27, w3, Operand(1));
  END();

  RUN();

  CHECK_EQUAL_64(0x123, x10);
  CHECK_EQUAL_64(0x123111, x11);
  CHECK_EQUAL_64(0xabc000, x12);
  CHECK_EQUAL_64(0x0, x13);

  CHECK_EQUAL_32(0x123, w14);
  CHECK_EQUAL_32(0x123111, w15);
  CHECK_EQUAL_32(0xabc000, w16);
  CHECK_EQUAL_32(0x0, w17);

  CHECK_EQUAL_64(0xffffffffffffffffL, x20);
  CHECK_EQUAL_64(0x1000, x21);
  CHECK_EQUAL_64(0x111, x22);
  CHECK_EQUAL_64(0x7fffffffffffffffL, x23);

  CHECK_EQUAL_32(0xffffffff, w24);
  CHECK_EQUAL_32(0x1000, w25);
  CHECK_EQUAL_32(0x111, w26);
  CHECK_EQUAL_32(0xffffffff, w27);

  TEARDOWN();
}


TEST(add_sub_wide_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0x0);
  __ Mov(x1, 0x1);

  __ Add(x10, x0, Operand(0x1234567890abcdefUL));
  __ Add(x11, x1, Operand(0xffffffff));

  __ Add(w12, w0, Operand(0x12345678));
  __ Add(w13, w1, Operand(0xffffffff));

  __ Add(w18, w0, Operand(kWMinInt));
  __ Sub(w19, w0, Operand(kWMinInt));

  __ Sub(x20, x0, Operand(0x1234567890abcdefUL));
  __ Sub(w21, w0, Operand(0x12345678));
  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890abcdefUL, x10);
  CHECK_EQUAL_64(0x100000000UL, x11);

  CHECK_EQUAL_32(0x12345678, w12);
  CHECK_EQUAL_64(0x0, x13);

  CHECK_EQUAL_32(kWMinInt, w18);
  CHECK_EQUAL_32(kWMinInt, w19);

  CHECK_EQUAL_64(-0x1234567890abcdefUL, x20);
  CHECK_EQUAL_32(-0x12345678, w21);

  TEARDOWN();
}


TEST(add_sub_shifted) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0x0123456789abcdefL);
  __ Mov(x2, 0xfedcba9876543210L);
  __ Mov(x3, 0xffffffffffffffffL);

  __ Add(x10, x1, Operand(x2));
  __ Add(x11, x0, Operand(x1, LSL, 8));
  __ Add(x12, x0, Operand(x1, LSR, 8));
  __ Add(x13, x0, Operand(x1, ASR, 8));
  __ Add(x14, x0, Operand(x2, ASR, 8));
  __ Add(w15, w0, Operand(w1, ASR, 8));
  __ Add(w18, w3, Operand(w1, ROR, 8));
  __ Add(x19, x3, Operand(x1, ROR, 8));

  __ Sub(x20, x3, Operand(x2));
  __ Sub(x21, x3, Operand(x1, LSL, 8));
  __ Sub(x22, x3, Operand(x1, LSR, 8));
  __ Sub(x23, x3, Operand(x1, ASR, 8));
  __ Sub(x24, x3, Operand(x2, ASR, 8));
  __ Sub(w25, w3, Operand(w1, ASR, 8));
  __ Sub(w26, w3, Operand(w1, ROR, 8));
  __ Sub(x27, x3, Operand(x1, ROR, 8));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffffffffffffL, x10);
  CHECK_EQUAL_64(0x23456789abcdef00L, x11);
  CHECK_EQUAL_64(0x000123456789abcdL, x12);
  CHECK_EQUAL_64(0x000123456789abcdL, x13);
  CHECK_EQUAL_64(0xfffedcba98765432L, x14);
  CHECK_EQUAL_64(0xff89abcd, x15);
  CHECK_EQUAL_64(0xef89abcc, x18);
  CHECK_EQUAL_64(0xef0123456789abccL, x19);

  CHECK_EQUAL_64(0x0123456789abcdefL, x20);
  CHECK_EQUAL_64(0xdcba9876543210ffL, x21);
  CHECK_EQUAL_64(0xfffedcba98765432L, x22);
  CHECK_EQUAL_64(0xfffedcba98765432L, x23);
  CHECK_EQUAL_64(0x000123456789abcdL, x24);
  CHECK_EQUAL_64(0x00765432, x25);
  CHECK_EQUAL_64(0x10765432, x26);
  CHECK_EQUAL_64(0x10fedcba98765432L, x27);

  TEARDOWN();
}


TEST(add_sub_extended) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0x0123456789abcdefL);
  __ Mov(x2, 0xfedcba9876543210L);
  __ Mov(w3, 0x80);

  __ Add(x10, x0, Operand(x1, UXTB, 0));
  __ Add(x11, x0, Operand(x1, UXTB, 1));
  __ Add(x12, x0, Operand(x1, UXTH, 2));
  __ Add(x13, x0, Operand(x1, UXTW, 4));

  __ Add(x14, x0, Operand(x1, SXTB, 0));
  __ Add(x15, x0, Operand(x1, SXTB, 1));
  __ Add(x16, x0, Operand(x1, SXTH, 2));
  __ Add(x17, x0, Operand(x1, SXTW, 3));
  __ Add(x18, x0, Operand(x2, SXTB, 0));
  __ Add(x19, x0, Operand(x2, SXTB, 1));
  __ Add(x20, x0, Operand(x2, SXTH, 2));
  __ Add(x21, x0, Operand(x2, SXTW, 3));

  __ Add(x22, x1, Operand(x2, SXTB, 1));
  __ Sub(x23, x1, Operand(x2, SXTB, 1));

  __ Add(w24, w1, Operand(w2, UXTB, 2));
  __ Add(w25, w0, Operand(w1, SXTB, 0));
  __ Add(w26, w0, Operand(w1, SXTB, 1));
  __ Add(w27, w2, Operand(w1, SXTW, 3));

  __ Add(w28, w0, Operand(w1, SXTW, 3));
  __ Add(x29, x0, Operand(w1, SXTW, 3));

  __ Sub(x30, x0, Operand(w3, SXTB, 1));
  END();

  RUN();

  CHECK_EQUAL_64(0xefL, x10);
  CHECK_EQUAL_64(0x1deL, x11);
  CHECK_EQUAL_64(0x337bcL, x12);
  CHECK_EQUAL_64(0x89abcdef0L, x13);

  CHECK_EQUAL_64(0xffffffffffffffefL, x14);
  CHECK_EQUAL_64(0xffffffffffffffdeL, x15);
  CHECK_EQUAL_64(0xffffffffffff37bcL, x16);
  CHECK_EQUAL_64(0xfffffffc4d5e6f78L, x17);
  CHECK_EQUAL_64(0x10L, x18);
  CHECK_EQUAL_64(0x20L, x19);
  CHECK_EQUAL_64(0xc840L, x20);
  CHECK_EQUAL_64(0x3b2a19080L, x21);

  CHECK_EQUAL_64(0x0123456789abce0fL, x22);
  CHECK_EQUAL_64(0x0123456789abcdcfL, x23);

  CHECK_EQUAL_32(0x89abce2f, w24);
  CHECK_EQUAL_32(0xffffffef, w25);
  CHECK_EQUAL_32(0xffffffde, w26);
  CHECK_EQUAL_32(0xc3b2a188, w27);

  CHECK_EQUAL_32(0x4d5e6f78, w28);
  CHECK_EQUAL_64(0xfffffffc4d5e6f78L, x29);

  CHECK_EQUAL_64(256, x30);

  TEARDOWN();
}


TEST(add_sub_negative) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 4687);
  __ Mov(x2, 0x1122334455667788);
  __ Mov(w3, 0x11223344);
  __ Mov(w4, 400000);

  __ Add(x10, x0, -42);
  __ Add(x11, x1, -687);
  __ Add(x12, x2, -0x88);

  __ Sub(x13, x0, -600);
  __ Sub(x14, x1, -313);
  __ Sub(x15, x2, -0x555);

  __ Add(w19, w3, -0x344);
  __ Add(w20, w4, -2000);

  __ Sub(w21, w3, -0xbc);
  __ Sub(w22, w4, -2000);
  END();

  RUN();

  CHECK_EQUAL_64(-42, x10);
  CHECK_EQUAL_64(4000, x11);
  CHECK_EQUAL_64(0x1122334455667700, x12);

  CHECK_EQUAL_64(600, x13);
  CHECK_EQUAL_64(5000, x14);
  CHECK_EQUAL_64(0x1122334455667cdd, x15);

  CHECK_EQUAL_32(0x11223000, w19);
  CHECK_EQUAL_32(398000, w20);

  CHECK_EQUAL_32(0x11223400, w21);
  CHECK_EQUAL_32(402000, w22);

  TEARDOWN();
}


TEST(add_sub_zero) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0);
  __ Mov(x2, 0);

  Label blob1;
  __ Bind(&blob1);
  __ Add(x0, x0, 0);
  __ Sub(x1, x1, 0);
  __ Sub(x2, x2, xzr);
  CHECK_EQ(0, __ SizeOfCodeGeneratedSince(&blob1));

  Label blob2;
  __ Bind(&blob2);
  __ Add(w3, w3, 0);
  CHECK_NE(0, __ SizeOfCodeGeneratedSince(&blob2));

  Label blob3;
  __ Bind(&blob3);
  __ Sub(w3, w3, wzr);
  CHECK_NE(0, __ SizeOfCodeGeneratedSince(&blob3));

  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(0, x1);
  CHECK_EQUAL_64(0, x2);

  TEARDOWN();
}


TEST(claim_drop_zero) {
  INIT_V8();
  SETUP();

  START();

  Label start;
  __ Bind(&start);
  __ Claim(0);
  __ Drop(0);
  __ Claim(xzr, 8);
  __ Drop(xzr, 8);
  __ Claim(xzr, 0);
  __ Drop(xzr, 0);
  __ Claim(x7, 0);
  __ Drop(x7, 0);
  __ ClaimBySMI(xzr, 8);
  __ DropBySMI(xzr, 8);
  __ ClaimBySMI(xzr, 0);
  __ DropBySMI(xzr, 0);
  CHECK_EQ(0, __ SizeOfCodeGeneratedSince(&start));

  END();

  RUN();

  TEARDOWN();
}


TEST(neg) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xf123456789abcdefL);

  // Immediate.
  __ Neg(x1, 0x123);
  __ Neg(w2, 0x123);

  // Shifted.
  __ Neg(x3, Operand(x0, LSL, 1));
  __ Neg(w4, Operand(w0, LSL, 2));
  __ Neg(x5, Operand(x0, LSR, 3));
  __ Neg(w6, Operand(w0, LSR, 4));
  __ Neg(x7, Operand(x0, ASR, 5));
  __ Neg(w8, Operand(w0, ASR, 6));

  // Extended.
  __ Neg(w9, Operand(w0, UXTB));
  __ Neg(x10, Operand(x0, SXTB, 1));
  __ Neg(w11, Operand(w0, UXTH, 2));
  __ Neg(x12, Operand(x0, SXTH, 3));
  __ Neg(w13, Operand(w0, UXTW, 4));
  __ Neg(x14, Operand(x0, SXTW, 4));
  END();

  RUN();

  CHECK_EQUAL_64(0xfffffffffffffeddUL, x1);
  CHECK_EQUAL_64(0xfffffedd, x2);
  CHECK_EQUAL_64(0x1db97530eca86422UL, x3);
  CHECK_EQUAL_64(0xd950c844, x4);
  CHECK_EQUAL_64(0xe1db97530eca8643UL, x5);
  CHECK_EQUAL_64(0xf7654322, x6);
  CHECK_EQUAL_64(0x0076e5d4c3b2a191UL, x7);
  CHECK_EQUAL_64(0x01d950c9, x8);
  CHECK_EQUAL_64(0xffffff11, x9);
  CHECK_EQUAL_64(0x0000000000000022UL, x10);
  CHECK_EQUAL_64(0xfffcc844, x11);
  CHECK_EQUAL_64(0x0000000000019088UL, x12);
  CHECK_EQUAL_64(0x65432110, x13);
  CHECK_EQUAL_64(0x0000000765432110UL, x14);

  TEARDOWN();
}


TEST(adc_sbc_shift) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 1);
  __ Mov(x2, 0x0123456789abcdefL);
  __ Mov(x3, 0xfedcba9876543210L);
  __ Mov(x4, 0xffffffffffffffffL);

  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));

  __ Adc(x5, x2, Operand(x3));
  __ Adc(x6, x0, Operand(x1, LSL, 60));
  __ Sbc(x7, x4, Operand(x3, LSR, 4));
  __ Adc(x8, x2, Operand(x3, ASR, 4));
  __ Adc(x9, x2, Operand(x3, ROR, 8));

  __ Adc(w10, w2, Operand(w3));
  __ Adc(w11, w0, Operand(w1, LSL, 30));
  __ Sbc(w12, w4, Operand(w3, LSR, 4));
  __ Adc(w13, w2, Operand(w3, ASR, 4));
  __ Adc(w14, w2, Operand(w3, ROR, 8));

  // Set the C flag.
  __ Cmp(w0, Operand(w0));

  __ Adc(x18, x2, Operand(x3));
  __ Adc(x19, x0, Operand(x1, LSL, 60));
  __ Sbc(x20, x4, Operand(x3, LSR, 4));
  __ Adc(x21, x2, Operand(x3, ASR, 4));
  __ Adc(x22, x2, Operand(x3, ROR, 8));

  __ Adc(w23, w2, Operand(w3));
  __ Adc(w24, w0, Operand(w1, LSL, 30));
  __ Sbc(w25, w4, Operand(w3, LSR, 4));
  __ Adc(w26, w2, Operand(w3, ASR, 4));
  __ Adc(w27, w2, Operand(w3, ROR, 8));
  END();

  RUN();

  CHECK_EQUAL_64(0xffffffffffffffffL, x5);
  CHECK_EQUAL_64(1L << 60, x6);
  CHECK_EQUAL_64(0xf0123456789abcddL, x7);
  CHECK_EQUAL_64(0x0111111111111110L, x8);
  CHECK_EQUAL_64(0x1222222222222221L, x9);

  CHECK_EQUAL_32(0xffffffff, w10);
  CHECK_EQUAL_32(1 << 30, w11);
  CHECK_EQUAL_32(0xf89abcdd, w12);
  CHECK_EQUAL_32(0x91111110, w13);
  CHECK_EQUAL_32(0x9a222221, w14);

  CHECK_EQUAL_64(0xffffffffffffffffL + 1, x18);
  CHECK_EQUAL_64((1L << 60) + 1, x19);
  CHECK_EQUAL_64(0xf0123456789abcddL + 1, x20);
  CHECK_EQUAL_64(0x0111111111111110L + 1, x21);
  CHECK_EQUAL_64(0x1222222222222221L + 1, x22);

  CHECK_EQUAL_32(0xffffffff + 1, w23);
  CHECK_EQUAL_32((1 << 30) + 1, w24);
  CHECK_EQUAL_32(0xf89abcdd + 1, w25);
  CHECK_EQUAL_32(0x91111110 + 1, w26);
  CHECK_EQUAL_32(0x9a222221 + 1, w27);

  // Check that adc correctly sets the condition flags.
  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0xffffffffffffffffL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);
  CHECK_EQUAL_64(0, x10);

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0x8000000000000000L);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1, ASR, 63));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);
  CHECK_EQUAL_64(0, x10);

  START();
  __ Mov(x0, 0x10);
  __ Mov(x1, 0x07ffffffffffffffL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1, LSL, 4));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);
  CHECK_EQUAL_64(0x8000000000000000L, x10);

  // Check that sbc correctly sets the condition flags.
  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0xffffffffffffffffL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Sbcs(x10, x0, Operand(x1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0, x10);

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0xffffffffffffffffL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Sbcs(x10, x0, Operand(x1, LSR, 1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x8000000000000001L, x10);

  START();
  __ Mov(x0, 0);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Sbcs(x10, x0, Operand(0xffffffffffffffffL));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0, x10);

  START()
  __ Mov(w0, 0x7fffffff);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Ngcs(w10, w0);
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x80000000, x10);

  START();
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Ngcs(x10, 0x7fffffffffffffffL);
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x8000000000000000L, x10);

  START()
  __ Mov(x0, 0);
  // Set the C flag.
  __ Cmp(x0, Operand(x0));
  __ Sbcs(x10, x0, Operand(1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0xffffffffffffffffL, x10);

  START()
  __ Mov(x0, 0);
  // Set the C flag.
  __ Cmp(x0, Operand(x0));
  __ Ngcs(x10, 0x7fffffffffffffffL);
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x8000000000000001L, x10);

  TEARDOWN();
}


TEST(adc_sbc_extend) {
  INIT_V8();
  SETUP();

  START();
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));

  __ Mov(x0, 0);
  __ Mov(x1, 1);
  __ Mov(x2, 0x0123456789abcdefL);

  __ Adc(x10, x1, Operand(w2, UXTB, 1));
  __ Adc(x11, x1, Operand(x2, SXTH, 2));
  __ Sbc(x12, x1, Operand(w2, UXTW, 4));
  __ Adc(x13, x1, Operand(x2, UXTX, 4));

  __ Adc(w14, w1, Operand(w2, UXTB, 1));
  __ Adc(w15, w1, Operand(w2, SXTH, 2));
  __ Adc(w9, w1, Operand(w2, UXTW, 4));

  // Set the C flag.
  __ Cmp(w0, Operand(w0));

  __ Adc(x20, x1, Operand(w2, UXTB, 1));
  __ Adc(x21, x1, Operand(x2, SXTH, 2));
  __ Sbc(x22, x1, Operand(w2, UXTW, 4));
  __ Adc(x23, x1, Operand(x2, UXTX, 4));

  __ Adc(w24, w1, Operand(w2, UXTB, 1));
  __ Adc(w25, w1, Operand(w2, SXTH, 2));
  __ Adc(w26, w1, Operand(w2, UXTW, 4));
  END();

  RUN();

  CHECK_EQUAL_64(0x1df, x10);
  CHECK_EQUAL_64(0xffffffffffff37bdL, x11);
  CHECK_EQUAL_64(0xfffffff765432110L, x12);
  CHECK_EQUAL_64(0x123456789abcdef1L, x13);

  CHECK_EQUAL_32(0x1df, w14);
  CHECK_EQUAL_32(0xffff37bd, w15);
  CHECK_EQUAL_32(0x9abcdef1, w9);

  CHECK_EQUAL_64(0x1df + 1, x20);
  CHECK_EQUAL_64(0xffffffffffff37bdL + 1, x21);
  CHECK_EQUAL_64(0xfffffff765432110L + 1, x22);
  CHECK_EQUAL_64(0x123456789abcdef1L + 1, x23);

  CHECK_EQUAL_32(0x1df + 1, w24);
  CHECK_EQUAL_32(0xffff37bd + 1, w25);
  CHECK_EQUAL_32(0x9abcdef1 + 1, w26);

  // Check that adc correctly sets the condition flags.
  START();
  __ Mov(x0, 0xff);
  __ Mov(x1, 0xffffffffffffffffL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1, SXTX, 1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(CFlag);

  START();
  __ Mov(x0, 0x7fffffffffffffffL);
  __ Mov(x1, 1);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1, UXTB, 2));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  START();
  __ Mov(x0, 0x7fffffffffffffffL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  TEARDOWN();
}


TEST(adc_sbc_wide_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);

  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));

  __ Adc(x7, x0, Operand(0x1234567890abcdefUL));
  __ Adc(w8, w0, Operand(0xffffffff));
  __ Sbc(x9, x0, Operand(0x1234567890abcdefUL));
  __ Sbc(w10, w0, Operand(0xffffffff));
  __ Ngc(x11, Operand(0xffffffff00000000UL));
  __ Ngc(w12, Operand(0xffff0000));

  // Set the C flag.
  __ Cmp(w0, Operand(w0));

  __ Adc(x18, x0, Operand(0x1234567890abcdefUL));
  __ Adc(w19, w0, Operand(0xffffffff));
  __ Sbc(x20, x0, Operand(0x1234567890abcdefUL));
  __ Sbc(w21, w0, Operand(0xffffffff));
  __ Ngc(x22, Operand(0xffffffff00000000UL));
  __ Ngc(w23, Operand(0xffff0000));
  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890abcdefUL, x7);
  CHECK_EQUAL_64(0xffffffff, x8);
  CHECK_EQUAL_64(0xedcba9876f543210UL, x9);
  CHECK_EQUAL_64(0, x10);
  CHECK_EQUAL_64(0xffffffff, x11);
  CHECK_EQUAL_64(0xffff, x12);

  CHECK_EQUAL_64(0x1234567890abcdefUL + 1, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0xedcba9876f543211UL, x20);
  CHECK_EQUAL_64(1, x21);
  CHECK_EQUAL_64(0x100000000UL, x22);
  CHECK_EQUAL_64(0x10000, x23);

  TEARDOWN();
}


TEST(flags) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0x1111111111111111L);
  __ Neg(x10, Operand(x0));
  __ Neg(x11, Operand(x1));
  __ Neg(w12, Operand(w1));
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Ngc(x13, Operand(x0));
  // Set the C flag.
  __ Cmp(x0, Operand(x0));
  __ Ngc(w14, Operand(w0));
  END();

  RUN();

  CHECK_EQUAL_64(0, x10);
  CHECK_EQUAL_64(-0x1111111111111111L, x11);
  CHECK_EQUAL_32(-0x11111111, w12);
  CHECK_EQUAL_64(-1L, x13);
  CHECK_EQUAL_32(0, w14);

  START();
  __ Mov(x0, 0);
  __ Cmp(x0, Operand(x0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);

  START();
  __ Mov(w0, 0);
  __ Cmp(w0, Operand(w0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0x1111111111111111L);
  __ Cmp(x0, Operand(x1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);

  START();
  __ Mov(w0, 0);
  __ Mov(w1, 0x11111111);
  __ Cmp(w0, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);

  START();
  __ Mov(x1, 0x1111111111111111L);
  __ Cmp(x1, Operand(0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(CFlag);

  START();
  __ Mov(w1, 0x11111111);
  __ Cmp(w1, Operand(0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(CFlag);

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0x7fffffffffffffffL);
  __ Cmn(x1, Operand(x0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  START();
  __ Mov(w0, 1);
  __ Mov(w1, 0x7fffffff);
  __ Cmn(w1, Operand(w0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0xffffffffffffffffL);
  __ Cmn(x1, Operand(x0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);

  START();
  __ Mov(w0, 1);
  __ Mov(w1, 0xffffffff);
  __ Cmn(w1, Operand(w0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);

  START();
  __ Mov(w0, 0);
  __ Mov(w1, 1);
  // Clear the C flag.
  __ Adds(w0, w0, Operand(0));
  __ Ngcs(w0, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);

  START();
  __ Mov(w0, 0);
  __ Mov(w1, 0);
  // Set the C flag.
  __ Cmp(w0, Operand(w0));
  __ Ngcs(w0, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);

  TEARDOWN();
}


TEST(cmp_shift) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x18, 0xf0000000);
  __ Mov(x19, 0xf000000010000000UL);
  __ Mov(x20, 0xf0000000f0000000UL);
  __ Mov(x21, 0x7800000078000000UL);
  __ Mov(x22, 0x3c0000003c000000UL);
  __ Mov(x23, 0x8000000780000000UL);
  __ Mov(x24, 0x0000000f00000000UL);
  __ Mov(x25, 0x00000003c0000000UL);
  __ Mov(x26, 0x8000000780000000UL);
  __ Mov(x27, 0xc0000003);

  __ Cmp(w20, Operand(w21, LSL, 1));
  __ Mrs(x0, NZCV);

  __ Cmp(x20, Operand(x22, LSL, 2));
  __ Mrs(x1, NZCV);

  __ Cmp(w19, Operand(w23, LSR, 3));
  __ Mrs(x2, NZCV);

  __ Cmp(x18, Operand(x24, LSR, 4));
  __ Mrs(x3, NZCV);

  __ Cmp(w20, Operand(w25, ASR, 2));
  __ Mrs(x4, NZCV);

  __ Cmp(x20, Operand(x26, ASR, 3));
  __ Mrs(x5, NZCV);

  __ Cmp(w27, Operand(w22, ROR, 28));
  __ Mrs(x6, NZCV);

  __ Cmp(x20, Operand(x21, ROR, 31));
  __ Mrs(x7, NZCV);
  END();

  RUN();

  CHECK_EQUAL_32(ZCFlag, w0);
  CHECK_EQUAL_32(ZCFlag, w1);
  CHECK_EQUAL_32(ZCFlag, w2);
  CHECK_EQUAL_32(ZCFlag, w3);
  CHECK_EQUAL_32(ZCFlag, w4);
  CHECK_EQUAL_32(ZCFlag, w5);
  CHECK_EQUAL_32(ZCFlag, w6);
  CHECK_EQUAL_32(ZCFlag, w7);

  TEARDOWN();
}


TEST(cmp_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w20, 0x2);
  __ Mov(w21, 0x1);
  __ Mov(x22, 0xffffffffffffffffUL);
  __ Mov(x23, 0xff);
  __ Mov(x24, 0xfffffffffffffffeUL);
  __ Mov(x25, 0xffff);
  __ Mov(x26, 0xffffffff);

  __ Cmp(w20, Operand(w21, LSL, 1));
  __ Mrs(x0, NZCV);

  __ Cmp(x22, Operand(x23, SXTB, 0));
  __ Mrs(x1, NZCV);

  __ Cmp(x24, Operand(x23, SXTB, 1));
  __ Mrs(x2, NZCV);

  __ Cmp(x24, Operand(x23, UXTB, 1));
  __ Mrs(x3, NZCV);

  __ Cmp(w22, Operand(w25, UXTH));
  __ Mrs(x4, NZCV);

  __ Cmp(x22, Operand(x25, SXTH));
  __ Mrs(x5, NZCV);

  __ Cmp(x22, Operand(x26, UXTW));
  __ Mrs(x6, NZCV);

  __ Cmp(x24, Operand(x26, SXTW, 1));
  __ Mrs(x7, NZCV);
  END();

  RUN();

  CHECK_EQUAL_32(ZCFlag, w0);
  CHECK_EQUAL_32(ZCFlag, w1);
  CHECK_EQUAL_32(ZCFlag, w2);
  CHECK_EQUAL_32(NCFlag, w3);
  CHECK_EQUAL_32(NCFlag, w4);
  CHECK_EQUAL_32(ZCFlag, w5);
  CHECK_EQUAL_32(NCFlag, w6);
  CHECK_EQUAL_32(ZCFlag, w7);

  TEARDOWN();
}


TEST(ccmp) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w16, 0);
  __ Mov(w17, 1);
  __ Cmp(w16, w16);
  __ Ccmp(w16, w17, NCFlag, eq);
  __ Mrs(x0, NZCV);

  __ Cmp(w16, w16);
  __ Ccmp(w16, w17, NCFlag, ne);
  __ Mrs(x1, NZCV);

  __ Cmp(x16, x16);
  __ Ccmn(x16, 2, NZCVFlag, eq);
  __ Mrs(x2, NZCV);

  __ Cmp(x16, x16);
  __ Ccmn(x16, 2, NZCVFlag, ne);
  __ Mrs(x3, NZCV);

  __ ccmp(x16, x16, NZCVFlag, al);
  __ Mrs(x4, NZCV);

  __ ccmp(x16, x16, NZCVFlag, nv);
  __ Mrs(x5, NZCV);

  END();

  RUN();

  CHECK_EQUAL_32(NFlag, w0);
  CHECK_EQUAL_32(NCFlag, w1);
  CHECK_EQUAL_32(NoFlag, w2);
  CHECK_EQUAL_32(NZCVFlag, w3);
  CHECK_EQUAL_32(ZCFlag, w4);
  CHECK_EQUAL_32(ZCFlag, w5);

  TEARDOWN();
}


TEST(ccmp_wide_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w20, 0);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(w20, Operand(0x12345678), NZCVFlag, eq);
  __ Mrs(x0, NZCV);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(x20, Operand(0xffffffffffffffffUL), NZCVFlag, eq);
  __ Mrs(x1, NZCV);
  END();

  RUN();

  CHECK_EQUAL_32(NFlag, w0);
  CHECK_EQUAL_32(NoFlag, w1);

  TEARDOWN();
}


TEST(ccmp_shift_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w20, 0x2);
  __ Mov(w21, 0x1);
  __ Mov(x22, 0xffffffffffffffffUL);
  __ Mov(x23, 0xff);
  __ Mov(x24, 0xfffffffffffffffeUL);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(w20, Operand(w21, LSL, 1), NZCVFlag, eq);
  __ Mrs(x0, NZCV);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(x22, Operand(x23, SXTB, 0), NZCVFlag, eq);
  __ Mrs(x1, NZCV);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(x24, Operand(x23, SXTB, 1), NZCVFlag, eq);
  __ Mrs(x2, NZCV);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(x24, Operand(x23, UXTB, 1), NZCVFlag, eq);
  __ Mrs(x3, NZCV);

  __ Cmp(w20, Operand(w20));
  __ Ccmp(x24, Operand(x23, UXTB, 1), NZCVFlag, ne);
  __ Mrs(x4, NZCV);
  END();

  RUN();

  CHECK_EQUAL_32(ZCFlag, w0);
  CHECK_EQUAL_32(ZCFlag, w1);
  CHECK_EQUAL_32(ZCFlag, w2);
  CHECK_EQUAL_32(NCFlag, w3);
  CHECK_EQUAL_32(NZCVFlag, w4);

  TEARDOWN();
}


TEST(csel) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Mov(x24, 0x0000000f0000000fUL);
  __ Mov(x25, 0x0000001f0000001fUL);
  __ Mov(x26, 0);
  __ Mov(x27, 0);

  __ Cmp(w16, 0);
  __ Csel(w0, w24, w25, eq);
  __ Csel(w1, w24, w25, ne);
  __ Csinc(w2, w24, w25, mi);
  __ Csinc(w3, w24, w25, pl);

  __ csel(w13, w24, w25, al);
  __ csel(x14, x24, x25, nv);

  __ Cmp(x16, 1);
  __ Csinv(x4, x24, x25, gt);
  __ Csinv(x5, x24, x25, le);
  __ Csneg(x6, x24, x25, hs);
  __ Csneg(x7, x24, x25, lo);

  __ Cset(w8, ne);
  __ Csetm(w9, ne);
  __ Cinc(x10, x25, ne);
  __ Cinv(x11, x24, ne);
  __ Cneg(x12, x24, ne);

  __ csel(w15, w24, w25, al);
  __ csel(x18, x24, x25, nv);

  __ CzeroX(x24, ne);
  __ CzeroX(x25, eq);

  __ CmovX(x26, x25, ne);
  __ CmovX(x27, x25, eq);
  END();

  RUN();

  CHECK_EQUAL_64(0x0000000f, x0);
  CHECK_EQUAL_64(0x0000001f, x1);
  CHECK_EQUAL_64(0x00000020, x2);
  CHECK_EQUAL_64(0x0000000f, x3);
  CHECK_EQUAL_64(0xffffffe0ffffffe0UL, x4);
  CHECK_EQUAL_64(0x0000000f0000000fUL, x5);
  CHECK_EQUAL_64(0xffffffe0ffffffe1UL, x6);
  CHECK_EQUAL_64(0x0000000f0000000fUL, x7);
  CHECK_EQUAL_64(0x00000001, x8);
  CHECK_EQUAL_64(0xffffffff, x9);
  CHECK_EQUAL_64(0x0000001f00000020UL, x10);
  CHECK_EQUAL_64(0xfffffff0fffffff0UL, x11);
  CHECK_EQUAL_64(0xfffffff0fffffff1UL, x12);
  CHECK_EQUAL_64(0x0000000f, x13);
  CHECK_EQUAL_64(0x0000000f0000000fUL, x14);
  CHECK_EQUAL_64(0x0000000f, x15);
  CHECK_EQUAL_64(0x0000000f0000000fUL, x18);
  CHECK_EQUAL_64(0, x24);
  CHECK_EQUAL_64(0x0000001f0000001fUL, x25);
  CHECK_EQUAL_64(0x0000001f0000001fUL, x26);
  CHECK_EQUAL_64(0, x27);

  TEARDOWN();
}


TEST(csel_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x18, 0);
  __ Mov(x19, 0x80000000);
  __ Mov(x20, 0x8000000000000000UL);

  __ Cmp(x18, Operand(0));
  __ Csel(w0, w19, -2, ne);
  __ Csel(w1, w19, -1, ne);
  __ Csel(w2, w19, 0, ne);
  __ Csel(w3, w19, 1, ne);
  __ Csel(w4, w19, 2, ne);
  __ Csel(w5, w19, Operand(w19, ASR, 31), ne);
  __ Csel(w6, w19, Operand(w19, ROR, 1), ne);
  __ Csel(w7, w19, 3, eq);

  __ Csel(x8, x20, -2, ne);
  __ Csel(x9, x20, -1, ne);
  __ Csel(x10, x20, 0, ne);
  __ Csel(x11, x20, 1, ne);
  __ Csel(x12, x20, 2, ne);
  __ Csel(x13, x20, Operand(x20, ASR, 63), ne);
  __ Csel(x14, x20, Operand(x20, ROR, 1), ne);
  __ Csel(x15, x20, 3, eq);

  END();

  RUN();

  CHECK_EQUAL_32(-2, w0);
  CHECK_EQUAL_32(-1, w1);
  CHECK_EQUAL_32(0, w2);
  CHECK_EQUAL_32(1, w3);
  CHECK_EQUAL_32(2, w4);
  CHECK_EQUAL_32(-1, w5);
  CHECK_EQUAL_32(0x40000000, w6);
  CHECK_EQUAL_32(0x80000000, w7);

  CHECK_EQUAL_64(-2, x8);
  CHECK_EQUAL_64(-1, x9);
  CHECK_EQUAL_64(0, x10);
  CHECK_EQUAL_64(1, x11);
  CHECK_EQUAL_64(2, x12);
  CHECK_EQUAL_64(-1, x13);
  CHECK_EQUAL_64(0x4000000000000000UL, x14);
  CHECK_EQUAL_64(0x8000000000000000UL, x15);

  TEARDOWN();
}


TEST(lslv) {
  INIT_V8();
  SETUP();

  uint64_t value = 0x0123456789abcdefUL;
  int shift[] = {1, 3, 5, 9, 17, 33};

  START();
  __ Mov(x0, value);
  __ Mov(w1, shift[0]);
  __ Mov(w2, shift[1]);
  __ Mov(w3, shift[2]);
  __ Mov(w4, shift[3]);
  __ Mov(w5, shift[4]);
  __ Mov(w6, shift[5]);

  __ lslv(x0, x0, xzr);

  __ Lsl(x16, x0, x1);
  __ Lsl(x17, x0, x2);
  __ Lsl(x18, x0, x3);
  __ Lsl(x19, x0, x4);
  __ Lsl(x20, x0, x5);
  __ Lsl(x21, x0, x6);

  __ Lsl(w22, w0, w1);
  __ Lsl(w23, w0, w2);
  __ Lsl(w24, w0, w3);
  __ Lsl(w25, w0, w4);
  __ Lsl(w26, w0, w5);
  __ Lsl(w27, w0, w6);
  END();

  RUN();

  CHECK_EQUAL_64(value, x0);
  CHECK_EQUAL_64(value << (shift[0] & 63), x16);
  CHECK_EQUAL_64(value << (shift[1] & 63), x17);
  CHECK_EQUAL_64(value << (shift[2] & 63), x18);
  CHECK_EQUAL_64(value << (shift[3] & 63), x19);
  CHECK_EQUAL_64(value << (shift[4] & 63), x20);
  CHECK_EQUAL_64(value << (shift[5] & 63), x21);
  CHECK_EQUAL_32(value << (shift[0] & 31), w22);
  CHECK_EQUAL_32(value << (shift[1] & 31), w23);
  CHECK_EQUAL_32(value << (shift[2] & 31), w24);
  CHECK_EQUAL_32(value << (shift[3] & 31), w25);
  CHECK_EQUAL_32(value << (shift[4] & 31), w26);
  CHECK_EQUAL_32(value << (shift[5] & 31), w27);

  TEARDOWN();
}


TEST(lsrv) {
  INIT_V8();
  SETUP();

  uint64_t value = 0x0123456789abcdefUL;
  int shift[] = {1, 3, 5, 9, 17, 33};

  START();
  __ Mov(x0, value);
  __ Mov(w1, shift[0]);
  __ Mov(w2, shift[1]);
  __ Mov(w3, shift[2]);
  __ Mov(w4, shift[3]);
  __ Mov(w5, shift[4]);
  __ Mov(w6, shift[5]);

  __ lsrv(x0, x0, xzr);

  __ Lsr(x16, x0, x1);
  __ Lsr(x17, x0, x2);
  __ Lsr(x18, x0, x3);
  __ Lsr(x19, x0, x4);
  __ Lsr(x20, x0, x5);
  __ Lsr(x21, x0, x6);

  __ Lsr(w22, w0, w1);
  __ Lsr(w23, w0, w2);
  __ Lsr(w24, w0, w3);
  __ Lsr(w25, w0, w4);
  __ Lsr(w26, w0, w5);
  __ Lsr(w27, w0, w6);
  END();

  RUN();

  CHECK_EQUAL_64(value, x0);
  CHECK_EQUAL_64(value >> (shift[0] & 63), x16);
  CHECK_EQUAL_64(value >> (shift[1] & 63), x17);
  CHECK_EQUAL_64(value >> (shift[2] & 63), x18);
  CHECK_EQUAL_64(value >> (shift[3] & 63), x19);
  CHECK_EQUAL_64(value >> (shift[4] & 63), x20);
  CHECK_EQUAL_64(value >> (shift[5] & 63), x21);

  value &= 0xffffffffUL;
  CHECK_EQUAL_32(value >> (shift[0] & 31), w22);
  CHECK_EQUAL_32(value >> (shift[1] & 31), w23);
  CHECK_EQUAL_32(value >> (shift[2] & 31), w24);
  CHECK_EQUAL_32(value >> (shift[3] & 31), w25);
  CHECK_EQUAL_32(value >> (shift[4] & 31), w26);
  CHECK_EQUAL_32(value >> (shift[5] & 31), w27);

  TEARDOWN();
}


TEST(asrv) {
  INIT_V8();
  SETUP();

  int64_t value = 0xfedcba98fedcba98UL;
  int shift[] = {1, 3, 5, 9, 17, 33};

  START();
  __ Mov(x0, value);
  __ Mov(w1, shift[0]);
  __ Mov(w2, shift[1]);
  __ Mov(w3, shift[2]);
  __ Mov(w4, shift[3]);
  __ Mov(w5, shift[4]);
  __ Mov(w6, shift[5]);

  __ asrv(x0, x0, xzr);

  __ Asr(x16, x0, x1);
  __ Asr(x17, x0, x2);
  __ Asr(x18, x0, x3);
  __ Asr(x19, x0, x4);
  __ Asr(x20, x0, x5);
  __ Asr(x21, x0, x6);

  __ Asr(w22, w0, w1);
  __ Asr(w23, w0, w2);
  __ Asr(w24, w0, w3);
  __ Asr(w25, w0, w4);
  __ Asr(w26, w0, w5);
  __ Asr(w27, w0, w6);
  END();

  RUN();

  CHECK_EQUAL_64(value, x0);
  CHECK_EQUAL_64(value >> (shift[0] & 63), x16);
  CHECK_EQUAL_64(value >> (shift[1] & 63), x17);
  CHECK_EQUAL_64(value >> (shift[2] & 63), x18);
  CHECK_EQUAL_64(value >> (shift[3] & 63), x19);
  CHECK_EQUAL_64(value >> (shift[4] & 63), x20);
  CHECK_EQUAL_64(value >> (shift[5] & 63), x21);

  int32_t value32 = static_cast<int32_t>(value & 0xffffffffUL);
  CHECK_EQUAL_32(value32 >> (shift[0] & 31), w22);
  CHECK_EQUAL_32(value32 >> (shift[1] & 31), w23);
  CHECK_EQUAL_32(value32 >> (shift[2] & 31), w24);
  CHECK_EQUAL_32(value32 >> (shift[3] & 31), w25);
  CHECK_EQUAL_32(value32 >> (shift[4] & 31), w26);
  CHECK_EQUAL_32(value32 >> (shift[5] & 31), w27);

  TEARDOWN();
}


TEST(rorv) {
  INIT_V8();
  SETUP();

  uint64_t value = 0x0123456789abcdefUL;
  int shift[] = {4, 8, 12, 16, 24, 36};

  START();
  __ Mov(x0, value);
  __ Mov(w1, shift[0]);
  __ Mov(w2, shift[1]);
  __ Mov(w3, shift[2]);
  __ Mov(w4, shift[3]);
  __ Mov(w5, shift[4]);
  __ Mov(w6, shift[5]);

  __ rorv(x0, x0, xzr);

  __ Ror(x16, x0, x1);
  __ Ror(x17, x0, x2);
  __ Ror(x18, x0, x3);
  __ Ror(x19, x0, x4);
  __ Ror(x20, x0, x5);
  __ Ror(x21, x0, x6);

  __ Ror(w22, w0, w1);
  __ Ror(w23, w0, w2);
  __ Ror(w24, w0, w3);
  __ Ror(w25, w0, w4);
  __ Ror(w26, w0, w5);
  __ Ror(w27, w0, w6);
  END();

  RUN();

  CHECK_EQUAL_64(value, x0);
  CHECK_EQUAL_64(0xf0123456789abcdeUL, x16);
  CHECK_EQUAL_64(0xef0123456789abcdUL, x17);
  CHECK_EQUAL_64(0xdef0123456789abcUL, x18);
  CHECK_EQUAL_64(0xcdef0123456789abUL, x19);
  CHECK_EQUAL_64(0xabcdef0123456789UL, x20);
  CHECK_EQUAL_64(0x789abcdef0123456UL, x21);
  CHECK_EQUAL_32(0xf89abcde, w22);
  CHECK_EQUAL_32(0xef89abcd, w23);
  CHECK_EQUAL_32(0xdef89abc, w24);
  CHECK_EQUAL_32(0xcdef89ab, w25);
  CHECK_EQUAL_32(0xabcdef89, w26);
  CHECK_EQUAL_32(0xf89abcde, w27);

  TEARDOWN();
}


TEST(bfm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789abcdefL);

  __ Mov(x10, 0x8888888888888888L);
  __ Mov(x11, 0x8888888888888888L);
  __ Mov(x12, 0x8888888888888888L);
  __ Mov(x13, 0x8888888888888888L);
  __ Mov(w20, 0x88888888);
  __ Mov(w21, 0x88888888);

  __ bfm(x10, x1, 16, 31);
  __ bfm(x11, x1, 32, 15);

  __ bfm(w20, w1, 16, 23);
  __ bfm(w21, w1, 24, 15);

  // Aliases.
  __ Bfi(x12, x1, 16, 8);
  __ Bfxil(x13, x1, 16, 8);
  END();

  RUN();


  CHECK_EQUAL_64(0x88888888888889abL, x10);
  CHECK_EQUAL_64(0x8888cdef88888888L, x11);

  CHECK_EQUAL_32(0x888888ab, w20);
  CHECK_EQUAL_32(0x88cdef88, w21);

  CHECK_EQUAL_64(0x8888888888ef8888L, x12);
  CHECK_EQUAL_64(0x88888888888888abL, x13);

  TEARDOWN();
}


TEST(sbfm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789abcdefL);
  __ Mov(x2, 0xfedcba9876543210L);

  __ sbfm(x10, x1, 16, 31);
  __ sbfm(x11, x1, 32, 15);
  __ sbfm(x12, x1, 32, 47);
  __ sbfm(x13, x1, 48, 35);

  __ sbfm(w14, w1, 16, 23);
  __ sbfm(w15, w1, 24, 15);
  __ sbfm(w16, w2, 16, 23);
  __ sbfm(w17, w2, 24, 15);

  // Aliases.
  __ Asr(x18, x1, 32);
  __ Asr(x19, x2, 32);
  __ Sbfiz(x20, x1, 8, 16);
  __ Sbfiz(x21, x2, 8, 16);
  __ Sbfx(x22, x1, 8, 16);
  __ Sbfx(x23, x2, 8, 16);
  __ Sxtb(x24, w1);
  __ Sxtb(x25, x2);
  __ Sxth(x26, w1);
  __ Sxth(x27, x2);
  __ Sxtw(x28, w1);
  __ Sxtw(x29, x2);
  END();

  RUN();


  CHECK_EQUAL_64(0xffffffffffff89abL, x10);
  CHECK_EQUAL_64(0xffffcdef00000000L, x11);
  CHECK_EQUAL_64(0x4567L, x12);
  CHECK_EQUAL_64(0x789abcdef0000L, x13);

  CHECK_EQUAL_32(0xffffffab, w14);
  CHECK_EQUAL_32(0xffcdef00, w15);
  CHECK_EQUAL_32(0x54, w16);
  CHECK_EQUAL_32(0x00321000, w17);

  CHECK_EQUAL_64(0x01234567L, x18);
  CHECK_EQUAL_64(0xfffffffffedcba98L, x19);
  CHECK_EQUAL_64(0xffffffffffcdef00L, x20);
  CHECK_EQUAL_64(0x321000L, x21);
  CHECK_EQUAL_64(0xffffffffffffabcdL, x22);
  CHECK_EQUAL_64(0x5432L, x23);
  CHECK_EQUAL_64(0xffffffffffffffefL, x24);
  CHECK_EQUAL_64(0x10, x25);
  CHECK_EQUAL_64(0xffffffffffffcdefL, x26);
  CHECK_EQUAL_64(0x3210, x27);
  CHECK_EQUAL_64(0xffffffff89abcdefL, x28);
  CHECK_EQUAL_64(0x76543210, x29);

  TEARDOWN();
}


TEST(ubfm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789abcdefL);
  __ Mov(x2, 0xfedcba9876543210L);

  __ Mov(x10, 0x8888888888888888L);
  __ Mov(x11, 0x8888888888888888L);

  __ ubfm(x10, x1, 16, 31);
  __ ubfm(x11, x1, 32, 15);
  __ ubfm(x12, x1, 32, 47);
  __ ubfm(x13, x1, 48, 35);

  __ ubfm(w25, w1, 16, 23);
  __ ubfm(w26, w1, 24, 15);
  __ ubfm(w27, w2, 16, 23);
  __ ubfm(w28, w2, 24, 15);

  // Aliases
  __ Lsl(x15, x1, 63);
  __ Lsl(x16, x1, 0);
  __ Lsr(x17, x1, 32);
  __ Ubfiz(x18, x1, 8, 16);
  __ Ubfx(x19, x1, 8, 16);
  __ Uxtb(x20, x1);
  __ Uxth(x21, x1);
  __ Uxtw(x22, x1);
  END();

  RUN();

  CHECK_EQUAL_64(0x00000000000089abL, x10);
  CHECK_EQUAL_64(0x0000cdef00000000L, x11);
  CHECK_EQUAL_64(0x4567L, x12);
  CHECK_EQUAL_64(0x789abcdef0000L, x13);

  CHECK_EQUAL_32(0x000000ab, w25);
  CHECK_EQUAL_32(0x00cdef00, w26);
  CHECK_EQUAL_32(0x54, w27);
  CHECK_EQUAL_32(0x00321000, w28);

  CHECK_EQUAL_64(0x8000000000000000L, x15);
  CHECK_EQUAL_64(0x0123456789abcdefL, x16);
  CHECK_EQUAL_64(0x01234567L, x17);
  CHECK_EQUAL_64(0xcdef00L, x18);
  CHECK_EQUAL_64(0xabcdL, x19);
  CHECK_EQUAL_64(0xefL, x20);
  CHECK_EQUAL_64(0xcdefL, x21);
  CHECK_EQUAL_64(0x89abcdefL, x22);

  TEARDOWN();
}


TEST(extr) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789abcdefL);
  __ Mov(x2, 0xfedcba9876543210L);

  __ Extr(w10, w1, w2, 0);
  __ Extr(x11, x1, x2, 0);
  __ Extr(w12, w1, w2, 1);
  __ Extr(x13, x2, x1, 2);

  __ Ror(w20, w1, 0);
  __ Ror(x21, x1, 0);
  __ Ror(w22, w2, 17);
  __ Ror(w23, w1, 31);
  __ Ror(x24, x2, 1);
  __ Ror(x25, x1, 63);
  END();

  RUN();

  CHECK_EQUAL_64(0x76543210, x10);
  CHECK_EQUAL_64(0xfedcba9876543210L, x11);
  CHECK_EQUAL_64(0xbb2a1908, x12);
  CHECK_EQUAL_64(0x0048d159e26af37bUL, x13);
  CHECK_EQUAL_64(0x89abcdef, x20);
  CHECK_EQUAL_64(0x0123456789abcdefL, x21);
  CHECK_EQUAL_64(0x19083b2a, x22);
  CHECK_EQUAL_64(0x13579bdf, x23);
  CHECK_EQUAL_64(0x7f6e5d4c3b2a1908UL, x24);
  CHECK_EQUAL_64(0x02468acf13579bdeUL, x25);

  TEARDOWN();
}


TEST(fmov_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s11, 1.0);
  __ Fmov(d22, -13.0);
  __ Fmov(s1, 255.0);
  __ Fmov(d2, 12.34567);
  __ Fmov(s3, 0.0);
  __ Fmov(d4, 0.0);
  __ Fmov(s5, kFP32PositiveInfinity);
  __ Fmov(d6, kFP64NegativeInfinity);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s11);
  CHECK_EQUAL_FP64(-13.0, d22);
  CHECK_EQUAL_FP32(255.0, s1);
  CHECK_EQUAL_FP64(12.34567, d2);
  CHECK_EQUAL_FP32(0.0, s3);
  CHECK_EQUAL_FP64(0.0, d4);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s5);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d6);

  TEARDOWN();
}


TEST(fmov_reg) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s20, 1.0);
  __ Fmov(w10, s20);
  __ Fmov(s30, w10);
  __ Fmov(s5, s20);
  __ Fmov(d1, -13.0);
  __ Fmov(x1, d1);
  __ Fmov(d2, x1);
  __ Fmov(d4, d1);
  __ Fmov(d6, rawbits_to_double(0x0123456789abcdefL));
  __ Fmov(s6, s6);
  END();

  RUN();

  CHECK_EQUAL_32(float_to_rawbits(1.0), w10);
  CHECK_EQUAL_FP32(1.0, s30);
  CHECK_EQUAL_FP32(1.0, s5);
  CHECK_EQUAL_64(double_to_rawbits(-13.0), x1);
  CHECK_EQUAL_FP64(-13.0, d2);
  CHECK_EQUAL_FP64(-13.0, d4);
  CHECK_EQUAL_FP32(rawbits_to_float(0x89abcdef), s6);

  TEARDOWN();
}


TEST(fadd) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s14, -0.0f);
  __ Fmov(s15, kFP32PositiveInfinity);
  __ Fmov(s16, kFP32NegativeInfinity);
  __ Fmov(s17, 3.25f);
  __ Fmov(s18, 1.0f);
  __ Fmov(s19, 0.0f);

  __ Fmov(d26, -0.0);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0.0);
  __ Fmov(d30, -2.0);
  __ Fmov(d31, 2.25);

  __ Fadd(s0, s17, s18);
  __ Fadd(s1, s18, s19);
  __ Fadd(s2, s14, s18);
  __ Fadd(s3, s15, s18);
  __ Fadd(s4, s16, s18);
  __ Fadd(s5, s15, s16);
  __ Fadd(s6, s16, s15);

  __ Fadd(d7, d30, d31);
  __ Fadd(d8, d29, d31);
  __ Fadd(d9, d26, d31);
  __ Fadd(d10, d27, d31);
  __ Fadd(d11, d28, d31);
  __ Fadd(d12, d27, d28);
  __ Fadd(d13, d28, d27);
  END();

  RUN();

  CHECK_EQUAL_FP32(4.25, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(1.0, s2);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s3);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s4);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s5);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s6);
  CHECK_EQUAL_FP64(0.25, d7);
  CHECK_EQUAL_FP64(2.25, d8);
  CHECK_EQUAL_FP64(2.25, d9);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d10);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d11);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d12);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d13);

  TEARDOWN();
}


TEST(fsub) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s14, -0.0f);
  __ Fmov(s15, kFP32PositiveInfinity);
  __ Fmov(s16, kFP32NegativeInfinity);
  __ Fmov(s17, 3.25f);
  __ Fmov(s18, 1.0f);
  __ Fmov(s19, 0.0f);

  __ Fmov(d26, -0.0);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0.0);
  __ Fmov(d30, -2.0);
  __ Fmov(d31, 2.25);

  __ Fsub(s0, s17, s18);
  __ Fsub(s1, s18, s19);
  __ Fsub(s2, s14, s18);
  __ Fsub(s3, s18, s15);
  __ Fsub(s4, s18, s16);
  __ Fsub(s5, s15, s15);
  __ Fsub(s6, s16, s16);

  __ Fsub(d7, d30, d31);
  __ Fsub(d8, d29, d31);
  __ Fsub(d9, d26, d31);
  __ Fsub(d10, d31, d27);
  __ Fsub(d11, d31, d28);
  __ Fsub(d12, d27, d27);
  __ Fsub(d13, d28, d28);
  END();

  RUN();

  CHECK_EQUAL_FP32(2.25, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(-1.0, s2);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s3);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s4);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s5);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s6);
  CHECK_EQUAL_FP64(-4.25, d7);
  CHECK_EQUAL_FP64(-2.25, d8);
  CHECK_EQUAL_FP64(-2.25, d9);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d10);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d11);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d12);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d13);

  TEARDOWN();
}


TEST(fmul) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s14, -0.0f);
  __ Fmov(s15, kFP32PositiveInfinity);
  __ Fmov(s16, kFP32NegativeInfinity);
  __ Fmov(s17, 3.25f);
  __ Fmov(s18, 2.0f);
  __ Fmov(s19, 0.0f);
  __ Fmov(s20, -2.0f);

  __ Fmov(d26, -0.0);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0.0);
  __ Fmov(d30, -2.0);
  __ Fmov(d31, 2.25);

  __ Fmul(s0, s17, s18);
  __ Fmul(s1, s18, s19);
  __ Fmul(s2, s14, s14);
  __ Fmul(s3, s15, s20);
  __ Fmul(s4, s16, s20);
  __ Fmul(s5, s15, s19);
  __ Fmul(s6, s19, s16);

  __ Fmul(d7, d30, d31);
  __ Fmul(d8, d29, d31);
  __ Fmul(d9, d26, d26);
  __ Fmul(d10, d27, d30);
  __ Fmul(d11, d28, d30);
  __ Fmul(d12, d27, d29);
  __ Fmul(d13, d29, d28);
  END();

  RUN();

  CHECK_EQUAL_FP32(6.5, s0);
  CHECK_EQUAL_FP32(0.0, s1);
  CHECK_EQUAL_FP32(0.0, s2);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s3);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s4);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s5);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s6);
  CHECK_EQUAL_FP64(-4.5, d7);
  CHECK_EQUAL_FP64(0.0, d8);
  CHECK_EQUAL_FP64(0.0, d9);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d10);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d11);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d12);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d13);

  TEARDOWN();
}


static void FmaddFmsubHelper(double n, double m, double a,
                             double fmadd, double fmsub,
                             double fnmadd, double fnmsub) {
  SETUP();
  START();

  __ Fmov(d0, n);
  __ Fmov(d1, m);
  __ Fmov(d2, a);
  __ Fmadd(d28, d0, d1, d2);
  __ Fmsub(d29, d0, d1, d2);
  __ Fnmadd(d30, d0, d1, d2);
  __ Fnmsub(d31, d0, d1, d2);

  END();
  RUN();

  CHECK_EQUAL_FP64(fmadd, d28);
  CHECK_EQUAL_FP64(fmsub, d29);
  CHECK_EQUAL_FP64(fnmadd, d30);
  CHECK_EQUAL_FP64(fnmsub, d31);

  TEARDOWN();
}


TEST(fmadd_fmsub_double) {
  INIT_V8();

  // It's hard to check the result of fused operations because the only way to
  // calculate the result is using fma, which is what the simulator uses anyway.
  // TODO(jbramley): Add tests to check behaviour against a hardware trace.

  // Basic operation.
  FmaddFmsubHelper(1.0, 2.0, 3.0, 5.0, 1.0, -5.0, -1.0);
  FmaddFmsubHelper(-1.0, 2.0, 3.0, 1.0, 5.0, -1.0, -5.0);

  // Check the sign of exact zeroes.
  //               n     m     a     fmadd  fmsub  fnmadd fnmsub
  FmaddFmsubHelper(-0.0, +0.0, -0.0, -0.0,  +0.0,  +0.0,  +0.0);
  FmaddFmsubHelper(+0.0, +0.0, -0.0, +0.0,  -0.0,  +0.0,  +0.0);
  FmaddFmsubHelper(+0.0, +0.0, +0.0, +0.0,  +0.0,  -0.0,  +0.0);
  FmaddFmsubHelper(-0.0, +0.0, +0.0, +0.0,  +0.0,  +0.0,  -0.0);
  FmaddFmsubHelper(+0.0, -0.0, -0.0, -0.0,  +0.0,  +0.0,  +0.0);
  FmaddFmsubHelper(-0.0, -0.0, -0.0, +0.0,  -0.0,  +0.0,  +0.0);
  FmaddFmsubHelper(-0.0, -0.0, +0.0, +0.0,  +0.0,  -0.0,  +0.0);
  FmaddFmsubHelper(+0.0, -0.0, +0.0, +0.0,  +0.0,  +0.0,  -0.0);

  // Check NaN generation.
  FmaddFmsubHelper(kFP64PositiveInfinity, 0.0, 42.0,
                   kFP64DefaultNaN, kFP64DefaultNaN,
                   kFP64DefaultNaN, kFP64DefaultNaN);
  FmaddFmsubHelper(0.0, kFP64PositiveInfinity, 42.0,
                   kFP64DefaultNaN, kFP64DefaultNaN,
                   kFP64DefaultNaN, kFP64DefaultNaN);
  FmaddFmsubHelper(kFP64PositiveInfinity, 1.0, kFP64PositiveInfinity,
                   kFP64PositiveInfinity,   //  inf + ( inf * 1) = inf
                   kFP64DefaultNaN,         //  inf + (-inf * 1) = NaN
                   kFP64NegativeInfinity,   // -inf + (-inf * 1) = -inf
                   kFP64DefaultNaN);        // -inf + ( inf * 1) = NaN
  FmaddFmsubHelper(kFP64NegativeInfinity, 1.0, kFP64PositiveInfinity,
                   kFP64DefaultNaN,         //  inf + (-inf * 1) = NaN
                   kFP64PositiveInfinity,   //  inf + ( inf * 1) = inf
                   kFP64DefaultNaN,         // -inf + ( inf * 1) = NaN
                   kFP64NegativeInfinity);  // -inf + (-inf * 1) = -inf
}


static void FmaddFmsubHelper(float n, float m, float a,
                             float fmadd, float fmsub,
                             float fnmadd, float fnmsub) {
  SETUP();
  START();

  __ Fmov(s0, n);
  __ Fmov(s1, m);
  __ Fmov(s2, a);
  __ Fmadd(s28, s0, s1, s2);
  __ Fmsub(s29, s0, s1, s2);
  __ Fnmadd(s30, s0, s1, s2);
  __ Fnmsub(s31, s0, s1, s2);

  END();
  RUN();

  CHECK_EQUAL_FP32(fmadd, s28);
  CHECK_EQUAL_FP32(fmsub, s29);
  CHECK_EQUAL_FP32(fnmadd, s30);
  CHECK_EQUAL_FP32(fnmsub, s31);

  TEARDOWN();
}


TEST(fmadd_fmsub_float) {
  INIT_V8();
  // It's hard to check the result of fused operations because the only way to
  // calculate the result is using fma, which is what the simulator uses anyway.
  // TODO(jbramley): Add tests to check behaviour against a hardware trace.

  // Basic operation.
  FmaddFmsubHelper(1.0f, 2.0f, 3.0f, 5.0f, 1.0f, -5.0f, -1.0f);
  FmaddFmsubHelper(-1.0f, 2.0f, 3.0f, 1.0f, 5.0f, -1.0f, -5.0f);

  // Check the sign of exact zeroes.
  //               n      m      a      fmadd  fmsub  fnmadd fnmsub
  FmaddFmsubHelper(-0.0f, +0.0f, -0.0f, -0.0f, +0.0f, +0.0f, +0.0f);
  FmaddFmsubHelper(+0.0f, +0.0f, -0.0f, +0.0f, -0.0f, +0.0f, +0.0f);
  FmaddFmsubHelper(+0.0f, +0.0f, +0.0f, +0.0f, +0.0f, -0.0f, +0.0f);
  FmaddFmsubHelper(-0.0f, +0.0f, +0.0f, +0.0f, +0.0f, +0.0f, -0.0f);
  FmaddFmsubHelper(+0.0f, -0.0f, -0.0f, -0.0f, +0.0f, +0.0f, +0.0f);
  FmaddFmsubHelper(-0.0f, -0.0f, -0.0f, +0.0f, -0.0f, +0.0f, +0.0f);
  FmaddFmsubHelper(-0.0f, -0.0f, +0.0f, +0.0f, +0.0f, -0.0f, +0.0f);
  FmaddFmsubHelper(+0.0f, -0.0f, +0.0f, +0.0f, +0.0f, +0.0f, -0.0f);

  // Check NaN generation.
  FmaddFmsubHelper(kFP32PositiveInfinity, 0.0f, 42.0f,
                   kFP32DefaultNaN, kFP32DefaultNaN,
                   kFP32DefaultNaN, kFP32DefaultNaN);
  FmaddFmsubHelper(0.0f, kFP32PositiveInfinity, 42.0f,
                   kFP32DefaultNaN, kFP32DefaultNaN,
                   kFP32DefaultNaN, kFP32DefaultNaN);
  FmaddFmsubHelper(kFP32PositiveInfinity, 1.0f, kFP32PositiveInfinity,
                   kFP32PositiveInfinity,   //  inf + ( inf * 1) = inf
                   kFP32DefaultNaN,         //  inf + (-inf * 1) = NaN
                   kFP32NegativeInfinity,   // -inf + (-inf * 1) = -inf
                   kFP32DefaultNaN);        // -inf + ( inf * 1) = NaN
  FmaddFmsubHelper(kFP32NegativeInfinity, 1.0f, kFP32PositiveInfinity,
                   kFP32DefaultNaN,         //  inf + (-inf * 1) = NaN
                   kFP32PositiveInfinity,   //  inf + ( inf * 1) = inf
                   kFP32DefaultNaN,         // -inf + ( inf * 1) = NaN
                   kFP32NegativeInfinity);  // -inf + (-inf * 1) = -inf
}


TEST(fmadd_fmsub_double_nans) {
  INIT_V8();
  // Make sure that NaN propagation works correctly.
  double s1 = rawbits_to_double(0x7ff5555511111111);
  double s2 = rawbits_to_double(0x7ff5555522222222);
  double sa = rawbits_to_double(0x7ff55555aaaaaaaa);
  double q1 = rawbits_to_double(0x7ffaaaaa11111111);
  double q2 = rawbits_to_double(0x7ffaaaaa22222222);
  double qa = rawbits_to_double(0x7ffaaaaaaaaaaaaa);
  DCHECK(IsSignallingNaN(s1));
  DCHECK(IsSignallingNaN(s2));
  DCHECK(IsSignallingNaN(sa));
  DCHECK(IsQuietNaN(q1));
  DCHECK(IsQuietNaN(q2));
  DCHECK(IsQuietNaN(qa));

  // The input NaNs after passing through ProcessNaN.
  double s1_proc = rawbits_to_double(0x7ffd555511111111);
  double s2_proc = rawbits_to_double(0x7ffd555522222222);
  double sa_proc = rawbits_to_double(0x7ffd5555aaaaaaaa);
  double q1_proc = q1;
  double q2_proc = q2;
  double qa_proc = qa;
  DCHECK(IsQuietNaN(s1_proc));
  DCHECK(IsQuietNaN(s2_proc));
  DCHECK(IsQuietNaN(sa_proc));
  DCHECK(IsQuietNaN(q1_proc));
  DCHECK(IsQuietNaN(q2_proc));
  DCHECK(IsQuietNaN(qa_proc));

  // Negated NaNs as it would be done on ARMv8 hardware.
  double s1_proc_neg = rawbits_to_double(0xfffd555511111111);
  double sa_proc_neg = rawbits_to_double(0xfffd5555aaaaaaaa);
  double q1_proc_neg = rawbits_to_double(0xfffaaaaa11111111);
  double qa_proc_neg = rawbits_to_double(0xfffaaaaaaaaaaaaa);
  DCHECK(IsQuietNaN(s1_proc_neg));
  DCHECK(IsQuietNaN(sa_proc_neg));
  DCHECK(IsQuietNaN(q1_proc_neg));
  DCHECK(IsQuietNaN(qa_proc_neg));

  // Quiet NaNs are propagated.
  FmaddFmsubHelper(q1, 0, 0, q1_proc, q1_proc_neg, q1_proc_neg, q1_proc);
  FmaddFmsubHelper(0, q2, 0, q2_proc, q2_proc, q2_proc, q2_proc);
  FmaddFmsubHelper(0, 0, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);
  FmaddFmsubHelper(q1, q2, 0, q1_proc, q1_proc_neg, q1_proc_neg, q1_proc);
  FmaddFmsubHelper(0, q2, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);
  FmaddFmsubHelper(q1, 0, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);
  FmaddFmsubHelper(q1, q2, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);

  // Signalling NaNs are propagated, and made quiet.
  FmaddFmsubHelper(s1, 0, 0, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(0, s2, 0, s2_proc, s2_proc, s2_proc, s2_proc);
  FmaddFmsubHelper(0, 0, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, 0, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(0, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, 0, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);

  // Signalling NaNs take precedence over quiet NaNs.
  FmaddFmsubHelper(s1, q2, qa, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(q1, s2, qa, s2_proc, s2_proc, s2_proc, s2_proc);
  FmaddFmsubHelper(q1, q2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, qa, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(q1, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, q2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);

  // A NaN generated by the intermediate op1 * op2 overrides a quiet NaN in a.
  FmaddFmsubHelper(0, kFP64PositiveInfinity, qa,
                   kFP64DefaultNaN, kFP64DefaultNaN,
                   kFP64DefaultNaN, kFP64DefaultNaN);
  FmaddFmsubHelper(kFP64PositiveInfinity, 0, qa,
                   kFP64DefaultNaN, kFP64DefaultNaN,
                   kFP64DefaultNaN, kFP64DefaultNaN);
  FmaddFmsubHelper(0, kFP64NegativeInfinity, qa,
                   kFP64DefaultNaN, kFP64DefaultNaN,
                   kFP64DefaultNaN, kFP64DefaultNaN);
  FmaddFmsubHelper(kFP64NegativeInfinity, 0, qa,
                   kFP64DefaultNaN, kFP64DefaultNaN,
                   kFP64DefaultNaN, kFP64DefaultNaN);
}


TEST(fmadd_fmsub_float_nans) {
  INIT_V8();
  // Make sure that NaN propagation works correctly.
  float s1 = rawbits_to_float(0x7f951111);
  float s2 = rawbits_to_float(0x7f952222);
  float sa = rawbits_to_float(0x7f95aaaa);
  float q1 = rawbits_to_float(0x7fea1111);
  float q2 = rawbits_to_float(0x7fea2222);
  float qa = rawbits_to_float(0x7feaaaaa);
  DCHECK(IsSignallingNaN(s1));
  DCHECK(IsSignallingNaN(s2));
  DCHECK(IsSignallingNaN(sa));
  DCHECK(IsQuietNaN(q1));
  DCHECK(IsQuietNaN(q2));
  DCHECK(IsQuietNaN(qa));

  // The input NaNs after passing through ProcessNaN.
  float s1_proc = rawbits_to_float(0x7fd51111);
  float s2_proc = rawbits_to_float(0x7fd52222);
  float sa_proc = rawbits_to_float(0x7fd5aaaa);
  float q1_proc = q1;
  float q2_proc = q2;
  float qa_proc = qa;
  DCHECK(IsQuietNaN(s1_proc));
  DCHECK(IsQuietNaN(s2_proc));
  DCHECK(IsQuietNaN(sa_proc));
  DCHECK(IsQuietNaN(q1_proc));
  DCHECK(IsQuietNaN(q2_proc));
  DCHECK(IsQuietNaN(qa_proc));

  // Negated NaNs as it would be done on ARMv8 hardware.
  float s1_proc_neg = rawbits_to_float(0xffd51111);
  float sa_proc_neg = rawbits_to_float(0xffd5aaaa);
  float q1_proc_neg = rawbits_to_float(0xffea1111);
  float qa_proc_neg = rawbits_to_float(0xffeaaaaa);
  DCHECK(IsQuietNaN(s1_proc_neg));
  DCHECK(IsQuietNaN(sa_proc_neg));
  DCHECK(IsQuietNaN(q1_proc_neg));
  DCHECK(IsQuietNaN(qa_proc_neg));

  // Quiet NaNs are propagated.
  FmaddFmsubHelper(q1, 0, 0, q1_proc, q1_proc_neg, q1_proc_neg, q1_proc);
  FmaddFmsubHelper(0, q2, 0, q2_proc, q2_proc, q2_proc, q2_proc);
  FmaddFmsubHelper(0, 0, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);
  FmaddFmsubHelper(q1, q2, 0, q1_proc, q1_proc_neg, q1_proc_neg, q1_proc);
  FmaddFmsubHelper(0, q2, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);
  FmaddFmsubHelper(q1, 0, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);
  FmaddFmsubHelper(q1, q2, qa, qa_proc, qa_proc, qa_proc_neg, qa_proc_neg);

  // Signalling NaNs are propagated, and made quiet.
  FmaddFmsubHelper(s1, 0, 0, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(0, s2, 0, s2_proc, s2_proc, s2_proc, s2_proc);
  FmaddFmsubHelper(0, 0, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, 0, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(0, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, 0, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);

  // Signalling NaNs take precedence over quiet NaNs.
  FmaddFmsubHelper(s1, q2, qa, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(q1, s2, qa, s2_proc, s2_proc, s2_proc, s2_proc);
  FmaddFmsubHelper(q1, q2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, qa, s1_proc, s1_proc_neg, s1_proc_neg, s1_proc);
  FmaddFmsubHelper(q1, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, q2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);
  FmaddFmsubHelper(s1, s2, sa, sa_proc, sa_proc, sa_proc_neg, sa_proc_neg);

  // A NaN generated by the intermediate op1 * op2 overrides a quiet NaN in a.
  FmaddFmsubHelper(0, kFP32PositiveInfinity, qa,
                   kFP32DefaultNaN, kFP32DefaultNaN,
                   kFP32DefaultNaN, kFP32DefaultNaN);
  FmaddFmsubHelper(kFP32PositiveInfinity, 0, qa,
                   kFP32DefaultNaN, kFP32DefaultNaN,
                   kFP32DefaultNaN, kFP32DefaultNaN);
  FmaddFmsubHelper(0, kFP32NegativeInfinity, qa,
                   kFP32DefaultNaN, kFP32DefaultNaN,
                   kFP32DefaultNaN, kFP32DefaultNaN);
  FmaddFmsubHelper(kFP32NegativeInfinity, 0, qa,
                   kFP32DefaultNaN, kFP32DefaultNaN,
                   kFP32DefaultNaN, kFP32DefaultNaN);
}


TEST(fdiv) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s14, -0.0f);
  __ Fmov(s15, kFP32PositiveInfinity);
  __ Fmov(s16, kFP32NegativeInfinity);
  __ Fmov(s17, 3.25f);
  __ Fmov(s18, 2.0f);
  __ Fmov(s19, 2.0f);
  __ Fmov(s20, -2.0f);

  __ Fmov(d26, -0.0);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0.0);
  __ Fmov(d30, -2.0);
  __ Fmov(d31, 2.25);

  __ Fdiv(s0, s17, s18);
  __ Fdiv(s1, s18, s19);
  __ Fdiv(s2, s14, s18);
  __ Fdiv(s3, s18, s15);
  __ Fdiv(s4, s18, s16);
  __ Fdiv(s5, s15, s16);
  __ Fdiv(s6, s14, s14);

  __ Fdiv(d7, d31, d30);
  __ Fdiv(d8, d29, d31);
  __ Fdiv(d9, d26, d31);
  __ Fdiv(d10, d31, d27);
  __ Fdiv(d11, d31, d28);
  __ Fdiv(d12, d28, d27);
  __ Fdiv(d13, d29, d29);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.625f, s0);
  CHECK_EQUAL_FP32(1.0f, s1);
  CHECK_EQUAL_FP32(-0.0f, s2);
  CHECK_EQUAL_FP32(0.0f, s3);
  CHECK_EQUAL_FP32(-0.0f, s4);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s5);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s6);
  CHECK_EQUAL_FP64(-1.125, d7);
  CHECK_EQUAL_FP64(0.0, d8);
  CHECK_EQUAL_FP64(-0.0, d9);
  CHECK_EQUAL_FP64(0.0, d10);
  CHECK_EQUAL_FP64(-0.0, d11);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d12);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d13);

  TEARDOWN();
}


static float MinMaxHelper(float n,
                          float m,
                          bool min,
                          float quiet_nan_substitute = 0.0) {
  uint32_t raw_n = float_to_rawbits(n);
  uint32_t raw_m = float_to_rawbits(m);

  if (std::isnan(n) && ((raw_n & kSQuietNanMask) == 0)) {
    // n is signalling NaN.
    return rawbits_to_float(raw_n | kSQuietNanMask);
  } else if (std::isnan(m) && ((raw_m & kSQuietNanMask) == 0)) {
    // m is signalling NaN.
    return rawbits_to_float(raw_m | kSQuietNanMask);
  } else if (quiet_nan_substitute == 0.0) {
    if (std::isnan(n)) {
      // n is quiet NaN.
      return n;
    } else if (std::isnan(m)) {
      // m is quiet NaN.
      return m;
    }
  } else {
    // Substitute n or m if one is quiet, but not both.
    if (std::isnan(n) && !std::isnan(m)) {
      // n is quiet NaN: replace with substitute.
      n = quiet_nan_substitute;
    } else if (!std::isnan(n) && std::isnan(m)) {
      // m is quiet NaN: replace with substitute.
      m = quiet_nan_substitute;
    }
  }

  if ((n == 0.0) && (m == 0.0) &&
      (copysign(1.0, n) != copysign(1.0, m))) {
    return min ? -0.0 : 0.0;
  }

  return min ? fminf(n, m) : fmaxf(n, m);
}


static double MinMaxHelper(double n,
                           double m,
                           bool min,
                           double quiet_nan_substitute = 0.0) {
  uint64_t raw_n = double_to_rawbits(n);
  uint64_t raw_m = double_to_rawbits(m);

  if (std::isnan(n) && ((raw_n & kDQuietNanMask) == 0)) {
    // n is signalling NaN.
    return rawbits_to_double(raw_n | kDQuietNanMask);
  } else if (std::isnan(m) && ((raw_m & kDQuietNanMask) == 0)) {
    // m is signalling NaN.
    return rawbits_to_double(raw_m | kDQuietNanMask);
  } else if (quiet_nan_substitute == 0.0) {
    if (std::isnan(n)) {
      // n is quiet NaN.
      return n;
    } else if (std::isnan(m)) {
      // m is quiet NaN.
      return m;
    }
  } else {
    // Substitute n or m if one is quiet, but not both.
    if (std::isnan(n) && !std::isnan(m)) {
      // n is quiet NaN: replace with substitute.
      n = quiet_nan_substitute;
    } else if (!std::isnan(n) && std::isnan(m)) {
      // m is quiet NaN: replace with substitute.
      m = quiet_nan_substitute;
    }
  }

  if ((n == 0.0) && (m == 0.0) &&
      (copysign(1.0, n) != copysign(1.0, m))) {
    return min ? -0.0 : 0.0;
  }

  return min ? fmin(n, m) : fmax(n, m);
}


static void FminFmaxDoubleHelper(double n, double m, double min, double max,
                                 double minnm, double maxnm) {
  SETUP();

  START();
  __ Fmov(d0, n);
  __ Fmov(d1, m);
  __ Fmin(d28, d0, d1);
  __ Fmax(d29, d0, d1);
  __ Fminnm(d30, d0, d1);
  __ Fmaxnm(d31, d0, d1);
  END();

  RUN();

  CHECK_EQUAL_FP64(min, d28);
  CHECK_EQUAL_FP64(max, d29);
  CHECK_EQUAL_FP64(minnm, d30);
  CHECK_EQUAL_FP64(maxnm, d31);

  TEARDOWN();
}


TEST(fmax_fmin_d) {
  INIT_V8();
  // Use non-standard NaNs to check that the payload bits are preserved.
  double snan = rawbits_to_double(0x7ff5555512345678);
  double qnan = rawbits_to_double(0x7ffaaaaa87654321);

  double snan_processed = rawbits_to_double(0x7ffd555512345678);
  double qnan_processed = qnan;

  DCHECK(IsSignallingNaN(snan));
  DCHECK(IsQuietNaN(qnan));
  DCHECK(IsQuietNaN(snan_processed));
  DCHECK(IsQuietNaN(qnan_processed));

  // Bootstrap tests.
  FminFmaxDoubleHelper(0, 0, 0, 0, 0, 0);
  FminFmaxDoubleHelper(0, 1, 0, 1, 0, 1);
  FminFmaxDoubleHelper(kFP64PositiveInfinity, kFP64NegativeInfinity,
                       kFP64NegativeInfinity, kFP64PositiveInfinity,
                       kFP64NegativeInfinity, kFP64PositiveInfinity);
  FminFmaxDoubleHelper(snan, 0,
                       snan_processed, snan_processed,
                       snan_processed, snan_processed);
  FminFmaxDoubleHelper(0, snan,
                       snan_processed, snan_processed,
                       snan_processed, snan_processed);
  FminFmaxDoubleHelper(qnan, 0,
                       qnan_processed, qnan_processed,
                       0, 0);
  FminFmaxDoubleHelper(0, qnan,
                       qnan_processed, qnan_processed,
                       0, 0);
  FminFmaxDoubleHelper(qnan, snan,
                       snan_processed, snan_processed,
                       snan_processed, snan_processed);
  FminFmaxDoubleHelper(snan, qnan,
                       snan_processed, snan_processed,
                       snan_processed, snan_processed);

  // Iterate over all combinations of inputs.
  double inputs[] = { DBL_MAX, DBL_MIN, 1.0, 0.0,
                      -DBL_MAX, -DBL_MIN, -1.0, -0.0,
                      kFP64PositiveInfinity, kFP64NegativeInfinity,
                      kFP64QuietNaN, kFP64SignallingNaN };

  const int count = sizeof(inputs) / sizeof(inputs[0]);

  for (int in = 0; in < count; in++) {
    double n = inputs[in];
    for (int im = 0; im < count; im++) {
      double m = inputs[im];
      FminFmaxDoubleHelper(n, m,
                           MinMaxHelper(n, m, true),
                           MinMaxHelper(n, m, false),
                           MinMaxHelper(n, m, true, kFP64PositiveInfinity),
                           MinMaxHelper(n, m, false, kFP64NegativeInfinity));
    }
  }
}


static void FminFmaxFloatHelper(float n, float m, float min, float max,
                                float minnm, float maxnm) {
  SETUP();

  START();
  __ Fmov(s0, n);
  __ Fmov(s1, m);
  __ Fmin(s28, s0, s1);
  __ Fmax(s29, s0, s1);
  __ Fminnm(s30, s0, s1);
  __ Fmaxnm(s31, s0, s1);
  END();

  RUN();

  CHECK_EQUAL_FP32(min, s28);
  CHECK_EQUAL_FP32(max, s29);
  CHECK_EQUAL_FP32(minnm, s30);
  CHECK_EQUAL_FP32(maxnm, s31);

  TEARDOWN();
}


TEST(fmax_fmin_s) {
  INIT_V8();
  // Use non-standard NaNs to check that the payload bits are preserved.
  float snan = rawbits_to_float(0x7f951234);
  float qnan = rawbits_to_float(0x7fea8765);

  float snan_processed = rawbits_to_float(0x7fd51234);
  float qnan_processed = qnan;

  DCHECK(IsSignallingNaN(snan));
  DCHECK(IsQuietNaN(qnan));
  DCHECK(IsQuietNaN(snan_processed));
  DCHECK(IsQuietNaN(qnan_processed));

  // Bootstrap tests.
  FminFmaxFloatHelper(0, 0, 0, 0, 0, 0);
  FminFmaxFloatHelper(0, 1, 0, 1, 0, 1);
  FminFmaxFloatHelper(kFP32PositiveInfinity, kFP32NegativeInfinity,
                      kFP32NegativeInfinity, kFP32PositiveInfinity,
                      kFP32NegativeInfinity, kFP32PositiveInfinity);
  FminFmaxFloatHelper(snan, 0,
                      snan_processed, snan_processed,
                      snan_processed, snan_processed);
  FminFmaxFloatHelper(0, snan,
                      snan_processed, snan_processed,
                      snan_processed, snan_processed);
  FminFmaxFloatHelper(qnan, 0,
                      qnan_processed, qnan_processed,
                      0, 0);
  FminFmaxFloatHelper(0, qnan,
                      qnan_processed, qnan_processed,
                      0, 0);
  FminFmaxFloatHelper(qnan, snan,
                      snan_processed, snan_processed,
                      snan_processed, snan_processed);
  FminFmaxFloatHelper(snan, qnan,
                      snan_processed, snan_processed,
                      snan_processed, snan_processed);

  // Iterate over all combinations of inputs.
  float inputs[] = { FLT_MAX, FLT_MIN, 1.0, 0.0,
                     -FLT_MAX, -FLT_MIN, -1.0, -0.0,
                     kFP32PositiveInfinity, kFP32NegativeInfinity,
                     kFP32QuietNaN, kFP32SignallingNaN };

  const int count = sizeof(inputs) / sizeof(inputs[0]);

  for (int in = 0; in < count; in++) {
    float n = inputs[in];
    for (int im = 0; im < count; im++) {
      float m = inputs[im];
      FminFmaxFloatHelper(n, m,
                          MinMaxHelper(n, m, true),
                          MinMaxHelper(n, m, false),
                          MinMaxHelper(n, m, true, kFP32PositiveInfinity),
                          MinMaxHelper(n, m, false, kFP32NegativeInfinity));
    }
  }
}


TEST(fccmp) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 0.0);
  __ Fmov(s17, 0.5);
  __ Fmov(d18, -0.5);
  __ Fmov(d19, -1.0);
  __ Mov(x20, 0);

  __ Cmp(x20, 0);
  __ Fccmp(s16, s16, NoFlag, eq);
  __ Mrs(x0, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(s16, s16, VFlag, ne);
  __ Mrs(x1, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(s16, s17, CFlag, ge);
  __ Mrs(x2, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(s16, s17, CVFlag, lt);
  __ Mrs(x3, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(d18, d18, ZFlag, le);
  __ Mrs(x4, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(d18, d18, ZVFlag, gt);
  __ Mrs(x5, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(d18, d19, ZCVFlag, ls);
  __ Mrs(x6, NZCV);

  __ Cmp(x20, 0);
  __ Fccmp(d18, d19, NFlag, hi);
  __ Mrs(x7, NZCV);

  __ fccmp(s16, s16, NFlag, al);
  __ Mrs(x8, NZCV);

  __ fccmp(d18, d18, NFlag, nv);
  __ Mrs(x9, NZCV);

  END();

  RUN();

  CHECK_EQUAL_32(ZCFlag, w0);
  CHECK_EQUAL_32(VFlag, w1);
  CHECK_EQUAL_32(NFlag, w2);
  CHECK_EQUAL_32(CVFlag, w3);
  CHECK_EQUAL_32(ZCFlag, w4);
  CHECK_EQUAL_32(ZVFlag, w5);
  CHECK_EQUAL_32(CFlag, w6);
  CHECK_EQUAL_32(NFlag, w7);
  CHECK_EQUAL_32(ZCFlag, w8);
  CHECK_EQUAL_32(ZCFlag, w9);

  TEARDOWN();
}


TEST(fcmp) {
  INIT_V8();
  SETUP();

  START();

  // Some of these tests require a floating-point scratch register assigned to
  // the macro assembler, but most do not.
  {
    // We're going to mess around with the available scratch registers in this
    // test. A UseScratchRegisterScope will make sure that they are restored to
    // the default values once we're finished.
    UseScratchRegisterScope temps(&masm);
    masm.FPTmpList()->set_list(0);

    __ Fmov(s8, 0.0);
    __ Fmov(s9, 0.5);
    __ Mov(w18, 0x7f800001);  // Single precision NaN.
    __ Fmov(s18, w18);

    __ Fcmp(s8, s8);
    __ Mrs(x0, NZCV);
    __ Fcmp(s8, s9);
    __ Mrs(x1, NZCV);
    __ Fcmp(s9, s8);
    __ Mrs(x2, NZCV);
    __ Fcmp(s8, s18);
    __ Mrs(x3, NZCV);
    __ Fcmp(s18, s18);
    __ Mrs(x4, NZCV);
    __ Fcmp(s8, 0.0);
    __ Mrs(x5, NZCV);
    masm.FPTmpList()->set_list(d0.Bit());
    __ Fcmp(s8, 255.0);
    masm.FPTmpList()->set_list(0);
    __ Mrs(x6, NZCV);

    __ Fmov(d19, 0.0);
    __ Fmov(d20, 0.5);
    __ Mov(x21, 0x7ff0000000000001UL);   // Double precision NaN.
    __ Fmov(d21, x21);

    __ Fcmp(d19, d19);
    __ Mrs(x10, NZCV);
    __ Fcmp(d19, d20);
    __ Mrs(x11, NZCV);
    __ Fcmp(d20, d19);
    __ Mrs(x12, NZCV);
    __ Fcmp(d19, d21);
    __ Mrs(x13, NZCV);
    __ Fcmp(d21, d21);
    __ Mrs(x14, NZCV);
    __ Fcmp(d19, 0.0);
    __ Mrs(x15, NZCV);
    masm.FPTmpList()->set_list(d0.Bit());
    __ Fcmp(d19, 12.3456);
    masm.FPTmpList()->set_list(0);
    __ Mrs(x16, NZCV);
  }

  END();

  RUN();

  CHECK_EQUAL_32(ZCFlag, w0);
  CHECK_EQUAL_32(NFlag, w1);
  CHECK_EQUAL_32(CFlag, w2);
  CHECK_EQUAL_32(CVFlag, w3);
  CHECK_EQUAL_32(CVFlag, w4);
  CHECK_EQUAL_32(ZCFlag, w5);
  CHECK_EQUAL_32(NFlag, w6);
  CHECK_EQUAL_32(ZCFlag, w10);
  CHECK_EQUAL_32(NFlag, w11);
  CHECK_EQUAL_32(CFlag, w12);
  CHECK_EQUAL_32(CVFlag, w13);
  CHECK_EQUAL_32(CVFlag, w14);
  CHECK_EQUAL_32(ZCFlag, w15);
  CHECK_EQUAL_32(NFlag, w16);

  TEARDOWN();
}


TEST(fcsel) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 2.0);
  __ Fmov(d18, 3.0);
  __ Fmov(d19, 4.0);

  __ Cmp(x16, 0);
  __ Fcsel(s0, s16, s17, eq);
  __ Fcsel(s1, s16, s17, ne);
  __ Fcsel(d2, d18, d19, eq);
  __ Fcsel(d3, d18, d19, ne);
  __ fcsel(s4, s16, s17, al);
  __ fcsel(d5, d18, d19, nv);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(2.0, s1);
  CHECK_EQUAL_FP64(3.0, d2);
  CHECK_EQUAL_FP64(4.0, d3);
  CHECK_EQUAL_FP32(1.0, s4);
  CHECK_EQUAL_FP64(3.0, d5);

  TEARDOWN();
}


TEST(fneg) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 0.0);
  __ Fmov(s18, kFP32PositiveInfinity);
  __ Fmov(d19, 1.0);
  __ Fmov(d20, 0.0);
  __ Fmov(d21, kFP64PositiveInfinity);

  __ Fneg(s0, s16);
  __ Fneg(s1, s0);
  __ Fneg(s2, s17);
  __ Fneg(s3, s2);
  __ Fneg(s4, s18);
  __ Fneg(s5, s4);
  __ Fneg(d6, d19);
  __ Fneg(d7, d6);
  __ Fneg(d8, d20);
  __ Fneg(d9, d8);
  __ Fneg(d10, d21);
  __ Fneg(d11, d10);
  END();

  RUN();

  CHECK_EQUAL_FP32(-1.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(-0.0, s2);
  CHECK_EQUAL_FP32(0.0, s3);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s4);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s5);
  CHECK_EQUAL_FP64(-1.0, d6);
  CHECK_EQUAL_FP64(1.0, d7);
  CHECK_EQUAL_FP64(-0.0, d8);
  CHECK_EQUAL_FP64(0.0, d9);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d10);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d11);

  TEARDOWN();
}


TEST(fabs) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, -1.0);
  __ Fmov(s17, -0.0);
  __ Fmov(s18, kFP32NegativeInfinity);
  __ Fmov(d19, -1.0);
  __ Fmov(d20, -0.0);
  __ Fmov(d21, kFP64NegativeInfinity);

  __ Fabs(s0, s16);
  __ Fabs(s1, s0);
  __ Fabs(s2, s17);
  __ Fabs(s3, s18);
  __ Fabs(d4, d19);
  __ Fabs(d5, d4);
  __ Fabs(d6, d20);
  __ Fabs(d7, d21);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(0.0, s2);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s3);
  CHECK_EQUAL_FP64(1.0, d4);
  CHECK_EQUAL_FP64(1.0, d5);
  CHECK_EQUAL_FP64(0.0, d6);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d7);

  TEARDOWN();
}


TEST(fsqrt) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 0.0);
  __ Fmov(s17, 1.0);
  __ Fmov(s18, 0.25);
  __ Fmov(s19, 65536.0);
  __ Fmov(s20, -0.0);
  __ Fmov(s21, kFP32PositiveInfinity);
  __ Fmov(s22, -1.0);
  __ Fmov(d23, 0.0);
  __ Fmov(d24, 1.0);
  __ Fmov(d25, 0.25);
  __ Fmov(d26, 4294967296.0);
  __ Fmov(d27, -0.0);
  __ Fmov(d28, kFP64PositiveInfinity);
  __ Fmov(d29, -1.0);

  __ Fsqrt(s0, s16);
  __ Fsqrt(s1, s17);
  __ Fsqrt(s2, s18);
  __ Fsqrt(s3, s19);
  __ Fsqrt(s4, s20);
  __ Fsqrt(s5, s21);
  __ Fsqrt(s6, s22);
  __ Fsqrt(d7, d23);
  __ Fsqrt(d8, d24);
  __ Fsqrt(d9, d25);
  __ Fsqrt(d10, d26);
  __ Fsqrt(d11, d27);
  __ Fsqrt(d12, d28);
  __ Fsqrt(d13, d29);
  END();

  RUN();

  CHECK_EQUAL_FP32(0.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(0.5, s2);
  CHECK_EQUAL_FP32(256.0, s3);
  CHECK_EQUAL_FP32(-0.0, s4);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s5);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s6);
  CHECK_EQUAL_FP64(0.0, d7);
  CHECK_EQUAL_FP64(1.0, d8);
  CHECK_EQUAL_FP64(0.5, d9);
  CHECK_EQUAL_FP64(65536.0, d10);
  CHECK_EQUAL_FP64(-0.0, d11);
  CHECK_EQUAL_FP64(kFP32PositiveInfinity, d12);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d13);

  TEARDOWN();
}


TEST(frinta) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, 1.9);
  __ Fmov(s20, 2.5);
  __ Fmov(s21, -1.5);
  __ Fmov(s22, -2.5);
  __ Fmov(s23, kFP32PositiveInfinity);
  __ Fmov(s24, kFP32NegativeInfinity);
  __ Fmov(s25, 0.0);
  __ Fmov(s26, -0.0);
  __ Fmov(s27, -0.2);

  __ Frinta(s0, s16);
  __ Frinta(s1, s17);
  __ Frinta(s2, s18);
  __ Frinta(s3, s19);
  __ Frinta(s4, s20);
  __ Frinta(s5, s21);
  __ Frinta(s6, s22);
  __ Frinta(s7, s23);
  __ Frinta(s8, s24);
  __ Frinta(s9, s25);
  __ Frinta(s10, s26);
  __ Frinta(s11, s27);

  __ Fmov(d16, 1.0);
  __ Fmov(d17, 1.1);
  __ Fmov(d18, 1.5);
  __ Fmov(d19, 1.9);
  __ Fmov(d20, 2.5);
  __ Fmov(d21, -1.5);
  __ Fmov(d22, -2.5);
  __ Fmov(d23, kFP32PositiveInfinity);
  __ Fmov(d24, kFP32NegativeInfinity);
  __ Fmov(d25, 0.0);
  __ Fmov(d26, -0.0);
  __ Fmov(d27, -0.2);

  __ Frinta(d12, d16);
  __ Frinta(d13, d17);
  __ Frinta(d14, d18);
  __ Frinta(d15, d19);
  __ Frinta(d16, d20);
  __ Frinta(d17, d21);
  __ Frinta(d18, d22);
  __ Frinta(d19, d23);
  __ Frinta(d20, d24);
  __ Frinta(d21, d25);
  __ Frinta(d22, d26);
  __ Frinta(d23, d27);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(2.0, s2);
  CHECK_EQUAL_FP32(2.0, s3);
  CHECK_EQUAL_FP32(3.0, s4);
  CHECK_EQUAL_FP32(-2.0, s5);
  CHECK_EQUAL_FP32(-3.0, s6);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s7);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s8);
  CHECK_EQUAL_FP32(0.0, s9);
  CHECK_EQUAL_FP32(-0.0, s10);
  CHECK_EQUAL_FP32(-0.0, s11);
  CHECK_EQUAL_FP64(1.0, d12);
  CHECK_EQUAL_FP64(1.0, d13);
  CHECK_EQUAL_FP64(2.0, d14);
  CHECK_EQUAL_FP64(2.0, d15);
  CHECK_EQUAL_FP64(3.0, d16);
  CHECK_EQUAL_FP64(-2.0, d17);
  CHECK_EQUAL_FP64(-3.0, d18);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d19);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d20);
  CHECK_EQUAL_FP64(0.0, d21);
  CHECK_EQUAL_FP64(-0.0, d22);
  CHECK_EQUAL_FP64(-0.0, d23);

  TEARDOWN();
}


TEST(frintm) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, 1.9);
  __ Fmov(s20, 2.5);
  __ Fmov(s21, -1.5);
  __ Fmov(s22, -2.5);
  __ Fmov(s23, kFP32PositiveInfinity);
  __ Fmov(s24, kFP32NegativeInfinity);
  __ Fmov(s25, 0.0);
  __ Fmov(s26, -0.0);
  __ Fmov(s27, -0.2);

  __ Frintm(s0, s16);
  __ Frintm(s1, s17);
  __ Frintm(s2, s18);
  __ Frintm(s3, s19);
  __ Frintm(s4, s20);
  __ Frintm(s5, s21);
  __ Frintm(s6, s22);
  __ Frintm(s7, s23);
  __ Frintm(s8, s24);
  __ Frintm(s9, s25);
  __ Frintm(s10, s26);
  __ Frintm(s11, s27);

  __ Fmov(d16, 1.0);
  __ Fmov(d17, 1.1);
  __ Fmov(d18, 1.5);
  __ Fmov(d19, 1.9);
  __ Fmov(d20, 2.5);
  __ Fmov(d21, -1.5);
  __ Fmov(d22, -2.5);
  __ Fmov(d23, kFP32PositiveInfinity);
  __ Fmov(d24, kFP32NegativeInfinity);
  __ Fmov(d25, 0.0);
  __ Fmov(d26, -0.0);
  __ Fmov(d27, -0.2);

  __ Frintm(d12, d16);
  __ Frintm(d13, d17);
  __ Frintm(d14, d18);
  __ Frintm(d15, d19);
  __ Frintm(d16, d20);
  __ Frintm(d17, d21);
  __ Frintm(d18, d22);
  __ Frintm(d19, d23);
  __ Frintm(d20, d24);
  __ Frintm(d21, d25);
  __ Frintm(d22, d26);
  __ Frintm(d23, d27);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(1.0, s2);
  CHECK_EQUAL_FP32(1.0, s3);
  CHECK_EQUAL_FP32(2.0, s4);
  CHECK_EQUAL_FP32(-2.0, s5);
  CHECK_EQUAL_FP32(-3.0, s6);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s7);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s8);
  CHECK_EQUAL_FP32(0.0, s9);
  CHECK_EQUAL_FP32(-0.0, s10);
  CHECK_EQUAL_FP32(-1.0, s11);
  CHECK_EQUAL_FP64(1.0, d12);
  CHECK_EQUAL_FP64(1.0, d13);
  CHECK_EQUAL_FP64(1.0, d14);
  CHECK_EQUAL_FP64(1.0, d15);
  CHECK_EQUAL_FP64(2.0, d16);
  CHECK_EQUAL_FP64(-2.0, d17);
  CHECK_EQUAL_FP64(-3.0, d18);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d19);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d20);
  CHECK_EQUAL_FP64(0.0, d21);
  CHECK_EQUAL_FP64(-0.0, d22);
  CHECK_EQUAL_FP64(-1.0, d23);

  TEARDOWN();
}


TEST(frintn) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, 1.9);
  __ Fmov(s20, 2.5);
  __ Fmov(s21, -1.5);
  __ Fmov(s22, -2.5);
  __ Fmov(s23, kFP32PositiveInfinity);
  __ Fmov(s24, kFP32NegativeInfinity);
  __ Fmov(s25, 0.0);
  __ Fmov(s26, -0.0);
  __ Fmov(s27, -0.2);

  __ Frintn(s0, s16);
  __ Frintn(s1, s17);
  __ Frintn(s2, s18);
  __ Frintn(s3, s19);
  __ Frintn(s4, s20);
  __ Frintn(s5, s21);
  __ Frintn(s6, s22);
  __ Frintn(s7, s23);
  __ Frintn(s8, s24);
  __ Frintn(s9, s25);
  __ Frintn(s10, s26);
  __ Frintn(s11, s27);

  __ Fmov(d16, 1.0);
  __ Fmov(d17, 1.1);
  __ Fmov(d18, 1.5);
  __ Fmov(d19, 1.9);
  __ Fmov(d20, 2.5);
  __ Fmov(d21, -1.5);
  __ Fmov(d22, -2.5);
  __ Fmov(d23, kFP32PositiveInfinity);
  __ Fmov(d24, kFP32NegativeInfinity);
  __ Fmov(d25, 0.0);
  __ Fmov(d26, -0.0);
  __ Fmov(d27, -0.2);

  __ Frintn(d12, d16);
  __ Frintn(d13, d17);
  __ Frintn(d14, d18);
  __ Frintn(d15, d19);
  __ Frintn(d16, d20);
  __ Frintn(d17, d21);
  __ Frintn(d18, d22);
  __ Frintn(d19, d23);
  __ Frintn(d20, d24);
  __ Frintn(d21, d25);
  __ Frintn(d22, d26);
  __ Frintn(d23, d27);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(2.0, s2);
  CHECK_EQUAL_FP32(2.0, s3);
  CHECK_EQUAL_FP32(2.0, s4);
  CHECK_EQUAL_FP32(-2.0, s5);
  CHECK_EQUAL_FP32(-2.0, s6);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s7);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s8);
  CHECK_EQUAL_FP32(0.0, s9);
  CHECK_EQUAL_FP32(-0.0, s10);
  CHECK_EQUAL_FP32(-0.0, s11);
  CHECK_EQUAL_FP64(1.0, d12);
  CHECK_EQUAL_FP64(1.0, d13);
  CHECK_EQUAL_FP64(2.0, d14);
  CHECK_EQUAL_FP64(2.0, d15);
  CHECK_EQUAL_FP64(2.0, d16);
  CHECK_EQUAL_FP64(-2.0, d17);
  CHECK_EQUAL_FP64(-2.0, d18);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d19);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d20);
  CHECK_EQUAL_FP64(0.0, d21);
  CHECK_EQUAL_FP64(-0.0, d22);
  CHECK_EQUAL_FP64(-0.0, d23);

  TEARDOWN();
}


TEST(frintp) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, 1.9);
  __ Fmov(s20, 2.5);
  __ Fmov(s21, -1.5);
  __ Fmov(s22, -2.5);
  __ Fmov(s23, kFP32PositiveInfinity);
  __ Fmov(s24, kFP32NegativeInfinity);
  __ Fmov(s25, 0.0);
  __ Fmov(s26, -0.0);
  __ Fmov(s27, -0.2);

  __ Frintp(s0, s16);
  __ Frintp(s1, s17);
  __ Frintp(s2, s18);
  __ Frintp(s3, s19);
  __ Frintp(s4, s20);
  __ Frintp(s5, s21);
  __ Frintp(s6, s22);
  __ Frintp(s7, s23);
  __ Frintp(s8, s24);
  __ Frintp(s9, s25);
  __ Frintp(s10, s26);
  __ Frintp(s11, s27);

  __ Fmov(d16, -0.5);
  __ Fmov(d17, -0.8);
  __ Fmov(d18, 1.5);
  __ Fmov(d19, 1.9);
  __ Fmov(d20, 2.5);
  __ Fmov(d21, -1.5);
  __ Fmov(d22, -2.5);
  __ Fmov(d23, kFP32PositiveInfinity);
  __ Fmov(d24, kFP32NegativeInfinity);
  __ Fmov(d25, 0.0);
  __ Fmov(d26, -0.0);
  __ Fmov(d27, -0.2);

  __ Frintp(d12, d16);
  __ Frintp(d13, d17);
  __ Frintp(d14, d18);
  __ Frintp(d15, d19);
  __ Frintp(d16, d20);
  __ Frintp(d17, d21);
  __ Frintp(d18, d22);
  __ Frintp(d19, d23);
  __ Frintp(d20, d24);
  __ Frintp(d21, d25);
  __ Frintp(d22, d26);
  __ Frintp(d23, d27);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(2.0, s1);
  CHECK_EQUAL_FP32(2.0, s2);
  CHECK_EQUAL_FP32(2.0, s3);
  CHECK_EQUAL_FP32(3.0, s4);
  CHECK_EQUAL_FP32(-1.0, s5);
  CHECK_EQUAL_FP32(-2.0, s6);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s7);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s8);
  CHECK_EQUAL_FP32(0.0, s9);
  CHECK_EQUAL_FP32(-0.0, s10);
  CHECK_EQUAL_FP32(-0.0, s11);
  CHECK_EQUAL_FP64(-0.0, d12);
  CHECK_EQUAL_FP64(-0.0, d13);
  CHECK_EQUAL_FP64(2.0, d14);
  CHECK_EQUAL_FP64(2.0, d15);
  CHECK_EQUAL_FP64(3.0, d16);
  CHECK_EQUAL_FP64(-1.0, d17);
  CHECK_EQUAL_FP64(-2.0, d18);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d19);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d20);
  CHECK_EQUAL_FP64(0.0, d21);
  CHECK_EQUAL_FP64(-0.0, d22);
  CHECK_EQUAL_FP64(-0.0, d23);

  TEARDOWN();
}


TEST(frintz) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, 1.9);
  __ Fmov(s20, 2.5);
  __ Fmov(s21, -1.5);
  __ Fmov(s22, -2.5);
  __ Fmov(s23, kFP32PositiveInfinity);
  __ Fmov(s24, kFP32NegativeInfinity);
  __ Fmov(s25, 0.0);
  __ Fmov(s26, -0.0);

  __ Frintz(s0, s16);
  __ Frintz(s1, s17);
  __ Frintz(s2, s18);
  __ Frintz(s3, s19);
  __ Frintz(s4, s20);
  __ Frintz(s5, s21);
  __ Frintz(s6, s22);
  __ Frintz(s7, s23);
  __ Frintz(s8, s24);
  __ Frintz(s9, s25);
  __ Frintz(s10, s26);

  __ Fmov(d16, 1.0);
  __ Fmov(d17, 1.1);
  __ Fmov(d18, 1.5);
  __ Fmov(d19, 1.9);
  __ Fmov(d20, 2.5);
  __ Fmov(d21, -1.5);
  __ Fmov(d22, -2.5);
  __ Fmov(d23, kFP32PositiveInfinity);
  __ Fmov(d24, kFP32NegativeInfinity);
  __ Fmov(d25, 0.0);
  __ Fmov(d26, -0.0);

  __ Frintz(d11, d16);
  __ Frintz(d12, d17);
  __ Frintz(d13, d18);
  __ Frintz(d14, d19);
  __ Frintz(d15, d20);
  __ Frintz(d16, d21);
  __ Frintz(d17, d22);
  __ Frintz(d18, d23);
  __ Frintz(d19, d24);
  __ Frintz(d20, d25);
  __ Frintz(d21, d26);
  END();

  RUN();

  CHECK_EQUAL_FP32(1.0, s0);
  CHECK_EQUAL_FP32(1.0, s1);
  CHECK_EQUAL_FP32(1.0, s2);
  CHECK_EQUAL_FP32(1.0, s3);
  CHECK_EQUAL_FP32(2.0, s4);
  CHECK_EQUAL_FP32(-1.0, s5);
  CHECK_EQUAL_FP32(-2.0, s6);
  CHECK_EQUAL_FP32(kFP32PositiveInfinity, s7);
  CHECK_EQUAL_FP32(kFP32NegativeInfinity, s8);
  CHECK_EQUAL_FP32(0.0, s9);
  CHECK_EQUAL_FP32(-0.0, s10);
  CHECK_EQUAL_FP64(1.0, d11);
  CHECK_EQUAL_FP64(1.0, d12);
  CHECK_EQUAL_FP64(1.0, d13);
  CHECK_EQUAL_FP64(1.0, d14);
  CHECK_EQUAL_FP64(2.0, d15);
  CHECK_EQUAL_FP64(-1.0, d16);
  CHECK_EQUAL_FP64(-2.0, d17);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d18);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d19);
  CHECK_EQUAL_FP64(0.0, d20);
  CHECK_EQUAL_FP64(-0.0, d21);

  TEARDOWN();
}


TEST(fcvt_ds) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, 1.9);
  __ Fmov(s20, 2.5);
  __ Fmov(s21, -1.5);
  __ Fmov(s22, -2.5);
  __ Fmov(s23, kFP32PositiveInfinity);
  __ Fmov(s24, kFP32NegativeInfinity);
  __ Fmov(s25, 0.0);
  __ Fmov(s26, -0.0);
  __ Fmov(s27, FLT_MAX);
  __ Fmov(s28, FLT_MIN);
  __ Fmov(s29, rawbits_to_float(0x7fc12345));   // Quiet NaN.
  __ Fmov(s30, rawbits_to_float(0x7f812345));   // Signalling NaN.

  __ Fcvt(d0, s16);
  __ Fcvt(d1, s17);
  __ Fcvt(d2, s18);
  __ Fcvt(d3, s19);
  __ Fcvt(d4, s20);
  __ Fcvt(d5, s21);
  __ Fcvt(d6, s22);
  __ Fcvt(d7, s23);
  __ Fcvt(d8, s24);
  __ Fcvt(d9, s25);
  __ Fcvt(d10, s26);
  __ Fcvt(d11, s27);
  __ Fcvt(d12, s28);
  __ Fcvt(d13, s29);
  __ Fcvt(d14, s30);
  END();

  RUN();

  CHECK_EQUAL_FP64(1.0f, d0);
  CHECK_EQUAL_FP64(1.1f, d1);
  CHECK_EQUAL_FP64(1.5f, d2);
  CHECK_EQUAL_FP64(1.9f, d3);
  CHECK_EQUAL_FP64(2.5f, d4);
  CHECK_EQUAL_FP64(-1.5f, d5);
  CHECK_EQUAL_FP64(-2.5f, d6);
  CHECK_EQUAL_FP64(kFP64PositiveInfinity, d7);
  CHECK_EQUAL_FP64(kFP64NegativeInfinity, d8);
  CHECK_EQUAL_FP64(0.0f, d9);
  CHECK_EQUAL_FP64(-0.0f, d10);
  CHECK_EQUAL_FP64(FLT_MAX, d11);
  CHECK_EQUAL_FP64(FLT_MIN, d12);

  // Check that the NaN payload is preserved according to ARM64 conversion
  // rules:
  //  - The sign bit is preserved.
  //  - The top bit of the mantissa is forced to 1 (making it a quiet NaN).
  //  - The remaining mantissa bits are copied until they run out.
  //  - The low-order bits that haven't already been assigned are set to 0.
  CHECK_EQUAL_FP64(rawbits_to_double(0x7ff82468a0000000), d13);
  CHECK_EQUAL_FP64(rawbits_to_double(0x7ff82468a0000000), d14);

  TEARDOWN();
}


TEST(fcvt_sd) {
  INIT_V8();
  // There are a huge number of corner-cases to check, so this test iterates
  // through a list. The list is then negated and checked again (since the sign
  // is irrelevant in ties-to-even rounding), so the list shouldn't include any
  // negative values.
  //
  // Note that this test only checks ties-to-even rounding, because that is all
  // that the simulator supports.
  struct {double in; float expected;} test[] = {
    // Check some simple conversions.
    {0.0, 0.0f},
    {1.0, 1.0f},
    {1.5, 1.5f},
    {2.0, 2.0f},
    {FLT_MAX, FLT_MAX},
    //  - The smallest normalized float.
    {pow(2.0, -126), powf(2, -126)},
    //  - Normal floats that need (ties-to-even) rounding.
    //    For normalized numbers:
    //         bit 29 (0x0000000020000000) is the lowest-order bit which will
    //                                     fit in the float's mantissa.
    {rawbits_to_double(0x3ff0000000000000), rawbits_to_float(0x3f800000)},
    {rawbits_to_double(0x3ff0000000000001), rawbits_to_float(0x3f800000)},
    {rawbits_to_double(0x3ff0000010000000), rawbits_to_float(0x3f800000)},
    {rawbits_to_double(0x3ff0000010000001), rawbits_to_float(0x3f800001)},
    {rawbits_to_double(0x3ff0000020000000), rawbits_to_float(0x3f800001)},
    {rawbits_to_double(0x3ff0000020000001), rawbits_to_float(0x3f800001)},
    {rawbits_to_double(0x3ff0000030000000), rawbits_to_float(0x3f800002)},
    {rawbits_to_double(0x3ff0000030000001), rawbits_to_float(0x3f800002)},
    {rawbits_to_double(0x3ff0000040000000), rawbits_to_float(0x3f800002)},
    {rawbits_to_double(0x3ff0000040000001), rawbits_to_float(0x3f800002)},
    {rawbits_to_double(0x3ff0000050000000), rawbits_to_float(0x3f800002)},
    {rawbits_to_double(0x3ff0000050000001), rawbits_to_float(0x3f800003)},
    {rawbits_to_double(0x3ff0000060000000), rawbits_to_float(0x3f800003)},
    //  - A mantissa that overflows into the exponent during rounding.
    {rawbits_to_double(0x3feffffff0000000), rawbits_to_float(0x3f800000)},
    //  - The largest double that rounds to a normal float.
    {rawbits_to_double(0x47efffffefffffff), rawbits_to_float(0x7f7fffff)},

    // Doubles that are too big for a float.
    {kFP64PositiveInfinity, kFP32PositiveInfinity},
    {DBL_MAX, kFP32PositiveInfinity},
    //  - The smallest exponent that's too big for a float.
    {pow(2.0, 128), kFP32PositiveInfinity},
    //  - This exponent is in range, but the value rounds to infinity.
    {rawbits_to_double(0x47effffff0000000), kFP32PositiveInfinity},

    // Doubles that are too small for a float.
    //  - The smallest (subnormal) double.
    {DBL_MIN, 0.0},
    //  - The largest double which is too small for a subnormal float.
    {rawbits_to_double(0x3690000000000000), rawbits_to_float(0x00000000)},

    // Normal doubles that become subnormal floats.
    //  - The largest subnormal float.
    {rawbits_to_double(0x380fffffc0000000), rawbits_to_float(0x007fffff)},
    //  - The smallest subnormal float.
    {rawbits_to_double(0x36a0000000000000), rawbits_to_float(0x00000001)},
    //  - Subnormal floats that need (ties-to-even) rounding.
    //    For these subnormals:
    //         bit 34 (0x0000000400000000) is the lowest-order bit which will
    //                                     fit in the float's mantissa.
    {rawbits_to_double(0x37c159e000000000), rawbits_to_float(0x00045678)},
    {rawbits_to_double(0x37c159e000000001), rawbits_to_float(0x00045678)},
    {rawbits_to_double(0x37c159e200000000), rawbits_to_float(0x00045678)},
    {rawbits_to_double(0x37c159e200000001), rawbits_to_float(0x00045679)},
    {rawbits_to_double(0x37c159e400000000), rawbits_to_float(0x00045679)},
    {rawbits_to_double(0x37c159e400000001), rawbits_to_float(0x00045679)},
    {rawbits_to_double(0x37c159e600000000), rawbits_to_float(0x0004567a)},
    {rawbits_to_double(0x37c159e600000001), rawbits_to_float(0x0004567a)},
    {rawbits_to_double(0x37c159e800000000), rawbits_to_float(0x0004567a)},
    {rawbits_to_double(0x37c159e800000001), rawbits_to_float(0x0004567a)},
    {rawbits_to_double(0x37c159ea00000000), rawbits_to_float(0x0004567a)},
    {rawbits_to_double(0x37c159ea00000001), rawbits_to_float(0x0004567b)},
    {rawbits_to_double(0x37c159ec00000000), rawbits_to_float(0x0004567b)},
    //  - The smallest double which rounds up to become a subnormal float.
    {rawbits_to_double(0x3690000000000001), rawbits_to_float(0x00000001)},

    // Check NaN payload preservation.
    {rawbits_to_double(0x7ff82468a0000000), rawbits_to_float(0x7fc12345)},
    {rawbits_to_double(0x7ff82468bfffffff), rawbits_to_float(0x7fc12345)},
    //  - Signalling NaNs become quiet NaNs.
    {rawbits_to_double(0x7ff02468a0000000), rawbits_to_float(0x7fc12345)},
    {rawbits_to_double(0x7ff02468bfffffff), rawbits_to_float(0x7fc12345)},
    {rawbits_to_double(0x7ff000001fffffff), rawbits_to_float(0x7fc00000)},
  };
  int count = sizeof(test) / sizeof(test[0]);

  for (int i = 0; i < count; i++) {
    double in = test[i].in;
    float expected = test[i].expected;

    // We only expect positive input.
    DCHECK(std::signbit(in) == 0);
    DCHECK(std::signbit(expected) == 0);

    SETUP();
    START();

    __ Fmov(d10, in);
    __ Fcvt(s20, d10);

    __ Fmov(d11, -in);
    __ Fcvt(s21, d11);

    END();
    RUN();
    CHECK_EQUAL_FP32(expected, s20);
    CHECK_EQUAL_FP32(-expected, s21);
    TEARDOWN();
  }
}


TEST(fcvtas) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 2.5);
  __ Fmov(s3, -2.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0x7fffff80);  // Largest float < INT32_MAX.
  __ Fneg(s7, s6);          // Smallest float > INT32_MIN.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 2.5);
  __ Fmov(d11, -2.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, kWMaxInt - 1);
  __ Fmov(d15, kWMinInt + 1);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 2.5);
  __ Fmov(s19, -2.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0x7fffff8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 2.5);
  __ Fmov(d26, -2.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7ffffffffffffc00UL);   // Largest double < INT64_MAX.
  __ Fneg(d30, d29);                    // Smallest double > INT64_MIN.

  __ Fcvtas(w0, s0);
  __ Fcvtas(w1, s1);
  __ Fcvtas(w2, s2);
  __ Fcvtas(w3, s3);
  __ Fcvtas(w4, s4);
  __ Fcvtas(w5, s5);
  __ Fcvtas(w6, s6);
  __ Fcvtas(w7, s7);
  __ Fcvtas(w8, d8);
  __ Fcvtas(w9, d9);
  __ Fcvtas(w10, d10);
  __ Fcvtas(w11, d11);
  __ Fcvtas(w12, d12);
  __ Fcvtas(w13, d13);
  __ Fcvtas(w14, d14);
  __ Fcvtas(w15, d15);
  __ Fcvtas(x17, s17);
  __ Fcvtas(x18, s18);
  __ Fcvtas(x19, s19);
  __ Fcvtas(x20, s20);
  __ Fcvtas(x21, s21);
  __ Fcvtas(x22, s22);
  __ Fcvtas(x23, s23);
  __ Fcvtas(x24, d24);
  __ Fcvtas(x25, d25);
  __ Fcvtas(x26, d26);
  __ Fcvtas(x27, d27);
  __ Fcvtas(x28, d28);
  __ Fcvtas(x29, d29);
  __ Fcvtas(x30, d30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(3, x2);
  CHECK_EQUAL_64(0xfffffffd, x3);
  CHECK_EQUAL_64(0x7fffffff, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7fffff80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(3, x10);
  CHECK_EQUAL_64(0xfffffffd, x11);
  CHECK_EQUAL_64(0x7fffffff, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7ffffffe, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(3, x18);
  CHECK_EQUAL_64(0xfffffffffffffffdUL, x19);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7fffff8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(3, x25);
  CHECK_EQUAL_64(0xfffffffffffffffdUL, x26);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x27);
  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7ffffffffffffc00UL, x29);
  CHECK_EQUAL_64(0x8000000000000400UL, x30);

  TEARDOWN();
}


TEST(fcvtau) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 2.5);
  __ Fmov(s3, -2.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0xffffff00);  // Largest float < UINT32_MAX.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 2.5);
  __ Fmov(d11, -2.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, 0xfffffffe);
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 2.5);
  __ Fmov(s19, -2.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0xffffff0000000000UL);  // Largest float < UINT64_MAX.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 2.5);
  __ Fmov(d26, -2.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0xfffffffffffff800UL);  // Largest double < UINT64_MAX.
  __ Fmov(s30, 0x100000000UL);

  __ Fcvtau(w0, s0);
  __ Fcvtau(w1, s1);
  __ Fcvtau(w2, s2);
  __ Fcvtau(w3, s3);
  __ Fcvtau(w4, s4);
  __ Fcvtau(w5, s5);
  __ Fcvtau(w6, s6);
  __ Fcvtau(w8, d8);
  __ Fcvtau(w9, d9);
  __ Fcvtau(w10, d10);
  __ Fcvtau(w11, d11);
  __ Fcvtau(w12, d12);
  __ Fcvtau(w13, d13);
  __ Fcvtau(w14, d14);
  __ Fcvtau(w15, d15);
  __ Fcvtau(x16, s16);
  __ Fcvtau(x17, s17);
  __ Fcvtau(x18, s18);
  __ Fcvtau(x19, s19);
  __ Fcvtau(x20, s20);
  __ Fcvtau(x21, s21);
  __ Fcvtau(x22, s22);
  __ Fcvtau(x24, d24);
  __ Fcvtau(x25, d25);
  __ Fcvtau(x26, d26);
  __ Fcvtau(x27, d27);
  __ Fcvtau(x28, d28);
  __ Fcvtau(x29, d29);
  __ Fcvtau(w30, s30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(3, x2);
  CHECK_EQUAL_64(0, x3);
  CHECK_EQUAL_64(0xffffffff, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0xffffff00, x6);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(3, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xffffffff, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0xfffffffe, x14);
  CHECK_EQUAL_64(1, x16);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(3, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x20);
  CHECK_EQUAL_64(0, x21);
  CHECK_EQUAL_64(0xffffff0000000000UL, x22);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(3, x25);
  CHECK_EQUAL_64(0, x26);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x27);
  CHECK_EQUAL_64(0, x28);
  CHECK_EQUAL_64(0xfffffffffffff800UL, x29);
  CHECK_EQUAL_64(0xffffffff, x30);

  TEARDOWN();
}


TEST(fcvtms) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 1.5);
  __ Fmov(s3, -1.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0x7fffff80);  // Largest float < INT32_MAX.
  __ Fneg(s7, s6);          // Smallest float > INT32_MIN.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, kWMaxInt - 1);
  __ Fmov(d15, kWMinInt + 1);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0x7fffff8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7ffffffffffffc00UL);   // Largest double < INT64_MAX.
  __ Fneg(d30, d29);                    // Smallest double > INT64_MIN.

  __ Fcvtms(w0, s0);
  __ Fcvtms(w1, s1);
  __ Fcvtms(w2, s2);
  __ Fcvtms(w3, s3);
  __ Fcvtms(w4, s4);
  __ Fcvtms(w5, s5);
  __ Fcvtms(w6, s6);
  __ Fcvtms(w7, s7);
  __ Fcvtms(w8, d8);
  __ Fcvtms(w9, d9);
  __ Fcvtms(w10, d10);
  __ Fcvtms(w11, d11);
  __ Fcvtms(w12, d12);
  __ Fcvtms(w13, d13);
  __ Fcvtms(w14, d14);
  __ Fcvtms(w15, d15);
  __ Fcvtms(x17, s17);
  __ Fcvtms(x18, s18);
  __ Fcvtms(x19, s19);
  __ Fcvtms(x20, s20);
  __ Fcvtms(x21, s21);
  __ Fcvtms(x22, s22);
  __ Fcvtms(x23, s23);
  __ Fcvtms(x24, d24);
  __ Fcvtms(x25, d25);
  __ Fcvtms(x26, d26);
  __ Fcvtms(x27, d27);
  __ Fcvtms(x28, d28);
  __ Fcvtms(x29, d29);
  __ Fcvtms(x30, d30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0xfffffffe, x3);
  CHECK_EQUAL_64(0x7fffffff, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7fffff80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0xfffffffe, x11);
  CHECK_EQUAL_64(0x7fffffff, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7ffffffe, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0xfffffffffffffffeUL, x19);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7fffff8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0xfffffffffffffffeUL, x26);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x27);
  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7ffffffffffffc00UL, x29);
  CHECK_EQUAL_64(0x8000000000000400UL, x30);

  TEARDOWN();
}


TEST(fcvtmu) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 1.5);
  __ Fmov(s3, -1.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0x7fffff80);  // Largest float < INT32_MAX.
  __ Fneg(s7, s6);          // Smallest float > INT32_MIN.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, kWMaxInt - 1);
  __ Fmov(d15, kWMinInt + 1);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0x7fffff8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7ffffffffffffc00UL);   // Largest double < INT64_MAX.
  __ Fneg(d30, d29);                    // Smallest double > INT64_MIN.

  __ Fcvtmu(w0, s0);
  __ Fcvtmu(w1, s1);
  __ Fcvtmu(w2, s2);
  __ Fcvtmu(w3, s3);
  __ Fcvtmu(w4, s4);
  __ Fcvtmu(w5, s5);
  __ Fcvtmu(w6, s6);
  __ Fcvtmu(w7, s7);
  __ Fcvtmu(w8, d8);
  __ Fcvtmu(w9, d9);
  __ Fcvtmu(w10, d10);
  __ Fcvtmu(w11, d11);
  __ Fcvtmu(w12, d12);
  __ Fcvtmu(w13, d13);
  __ Fcvtmu(w14, d14);
  __ Fcvtmu(x17, s17);
  __ Fcvtmu(x18, s18);
  __ Fcvtmu(x19, s19);
  __ Fcvtmu(x20, s20);
  __ Fcvtmu(x21, s21);
  __ Fcvtmu(x22, s22);
  __ Fcvtmu(x23, s23);
  __ Fcvtmu(x24, d24);
  __ Fcvtmu(x25, d25);
  __ Fcvtmu(x26, d26);
  __ Fcvtmu(x27, d27);
  __ Fcvtmu(x28, d28);
  __ Fcvtmu(x29, d29);
  __ Fcvtmu(x30, d30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0, x3);
  CHECK_EQUAL_64(0xffffffff, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0x7fffff80, x6);
  CHECK_EQUAL_64(0, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xffffffff, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0x7ffffffe, x14);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0x0UL, x19);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x20);
  CHECK_EQUAL_64(0x0UL, x21);
  CHECK_EQUAL_64(0x7fffff8000000000UL, x22);
  CHECK_EQUAL_64(0x0UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0x0UL, x26);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x27);
  CHECK_EQUAL_64(0x0UL, x28);
  CHECK_EQUAL_64(0x7ffffffffffffc00UL, x29);
  CHECK_EQUAL_64(0x0UL, x30);

  TEARDOWN();
}


TEST(fcvtns) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 1.5);
  __ Fmov(s3, -1.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0x7fffff80);  // Largest float < INT32_MAX.
  __ Fneg(s7, s6);          // Smallest float > INT32_MIN.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, kWMaxInt - 1);
  __ Fmov(d15, kWMinInt + 1);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0x7fffff8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7ffffffffffffc00UL);   // Largest double < INT64_MAX.
  __ Fneg(d30, d29);                    // Smallest double > INT64_MIN.

  __ Fcvtns(w0, s0);
  __ Fcvtns(w1, s1);
  __ Fcvtns(w2, s2);
  __ Fcvtns(w3, s3);
  __ Fcvtns(w4, s4);
  __ Fcvtns(w5, s5);
  __ Fcvtns(w6, s6);
  __ Fcvtns(w7, s7);
  __ Fcvtns(w8, d8);
  __ Fcvtns(w9, d9);
  __ Fcvtns(w10, d10);
  __ Fcvtns(w11, d11);
  __ Fcvtns(w12, d12);
  __ Fcvtns(w13, d13);
  __ Fcvtns(w14, d14);
  __ Fcvtns(w15, d15);
  __ Fcvtns(x17, s17);
  __ Fcvtns(x18, s18);
  __ Fcvtns(x19, s19);
  __ Fcvtns(x20, s20);
  __ Fcvtns(x21, s21);
  __ Fcvtns(x22, s22);
  __ Fcvtns(x23, s23);
  __ Fcvtns(x24, d24);
  __ Fcvtns(x25, d25);
  __ Fcvtns(x26, d26);
  __ Fcvtns(x27, d27);
//  __ Fcvtns(x28, d28);
  __ Fcvtns(x29, d29);
  __ Fcvtns(x30, d30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(2, x2);
  CHECK_EQUAL_64(0xfffffffe, x3);
  CHECK_EQUAL_64(0x7fffffff, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7fffff80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(2, x10);
  CHECK_EQUAL_64(0xfffffffe, x11);
  CHECK_EQUAL_64(0x7fffffff, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7ffffffe, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(2, x18);
  CHECK_EQUAL_64(0xfffffffffffffffeUL, x19);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7fffff8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(2, x25);
  CHECK_EQUAL_64(0xfffffffffffffffeUL, x26);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x27);
//  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7ffffffffffffc00UL, x29);
  CHECK_EQUAL_64(0x8000000000000400UL, x30);

  TEARDOWN();
}


TEST(fcvtnu) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 1.5);
  __ Fmov(s3, -1.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0xffffff00);  // Largest float < UINT32_MAX.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, 0xfffffffe);
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0xffffff0000000000UL);   // Largest float < UINT64_MAX.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0xfffffffffffff800UL);   // Largest double < UINT64_MAX.
  __ Fmov(s30, 0x100000000UL);

  __ Fcvtnu(w0, s0);
  __ Fcvtnu(w1, s1);
  __ Fcvtnu(w2, s2);
  __ Fcvtnu(w3, s3);
  __ Fcvtnu(w4, s4);
  __ Fcvtnu(w5, s5);
  __ Fcvtnu(w6, s6);
  __ Fcvtnu(w8, d8);
  __ Fcvtnu(w9, d9);
  __ Fcvtnu(w10, d10);
  __ Fcvtnu(w11, d11);
  __ Fcvtnu(w12, d12);
  __ Fcvtnu(w13, d13);
  __ Fcvtnu(w14, d14);
  __ Fcvtnu(w15, d15);
  __ Fcvtnu(x16, s16);
  __ Fcvtnu(x17, s17);
  __ Fcvtnu(x18, s18);
  __ Fcvtnu(x19, s19);
  __ Fcvtnu(x20, s20);
  __ Fcvtnu(x21, s21);
  __ Fcvtnu(x22, s22);
  __ Fcvtnu(x24, d24);
  __ Fcvtnu(x25, d25);
  __ Fcvtnu(x26, d26);
  __ Fcvtnu(x27, d27);
//  __ Fcvtnu(x28, d28);
  __ Fcvtnu(x29, d29);
  __ Fcvtnu(w30, s30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(2, x2);
  CHECK_EQUAL_64(0, x3);
  CHECK_EQUAL_64(0xffffffff, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0xffffff00, x6);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(2, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xffffffff, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0xfffffffe, x14);
  CHECK_EQUAL_64(1, x16);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(2, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x20);
  CHECK_EQUAL_64(0, x21);
  CHECK_EQUAL_64(0xffffff0000000000UL, x22);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(2, x25);
  CHECK_EQUAL_64(0, x26);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x27);
//  CHECK_EQUAL_64(0, x28);
  CHECK_EQUAL_64(0xfffffffffffff800UL, x29);
  CHECK_EQUAL_64(0xffffffff, x30);

  TEARDOWN();
}


TEST(fcvtzs) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 1.5);
  __ Fmov(s3, -1.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0x7fffff80);  // Largest float < INT32_MAX.
  __ Fneg(s7, s6);          // Smallest float > INT32_MIN.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, kWMaxInt - 1);
  __ Fmov(d15, kWMinInt + 1);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0x7fffff8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7ffffffffffffc00UL);   // Largest double < INT64_MAX.
  __ Fneg(d30, d29);                    // Smallest double > INT64_MIN.

  __ Fcvtzs(w0, s0);
  __ Fcvtzs(w1, s1);
  __ Fcvtzs(w2, s2);
  __ Fcvtzs(w3, s3);
  __ Fcvtzs(w4, s4);
  __ Fcvtzs(w5, s5);
  __ Fcvtzs(w6, s6);
  __ Fcvtzs(w7, s7);
  __ Fcvtzs(w8, d8);
  __ Fcvtzs(w9, d9);
  __ Fcvtzs(w10, d10);
  __ Fcvtzs(w11, d11);
  __ Fcvtzs(w12, d12);
  __ Fcvtzs(w13, d13);
  __ Fcvtzs(w14, d14);
  __ Fcvtzs(w15, d15);
  __ Fcvtzs(x17, s17);
  __ Fcvtzs(x18, s18);
  __ Fcvtzs(x19, s19);
  __ Fcvtzs(x20, s20);
  __ Fcvtzs(x21, s21);
  __ Fcvtzs(x22, s22);
  __ Fcvtzs(x23, s23);
  __ Fcvtzs(x24, d24);
  __ Fcvtzs(x25, d25);
  __ Fcvtzs(x26, d26);
  __ Fcvtzs(x27, d27);
  __ Fcvtzs(x28, d28);
  __ Fcvtzs(x29, d29);
  __ Fcvtzs(x30, d30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0xffffffff, x3);
  CHECK_EQUAL_64(0x7fffffff, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7fffff80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0xffffffff, x11);
  CHECK_EQUAL_64(0x7fffffff, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7ffffffe, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x19);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7fffff8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x26);
  CHECK_EQUAL_64(0x7fffffffffffffffUL, x27);
  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7ffffffffffffc00UL, x29);
  CHECK_EQUAL_64(0x8000000000000400UL, x30);

  TEARDOWN();
}


TEST(fcvtzu) {
  INIT_V8();
  SETUP();

  START();
  __ Fmov(s0, 1.0);
  __ Fmov(s1, 1.1);
  __ Fmov(s2, 1.5);
  __ Fmov(s3, -1.5);
  __ Fmov(s4, kFP32PositiveInfinity);
  __ Fmov(s5, kFP32NegativeInfinity);
  __ Fmov(s6, 0x7fffff80);  // Largest float < INT32_MAX.
  __ Fneg(s7, s6);          // Smallest float > INT32_MIN.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, kWMaxInt - 1);
  __ Fmov(d15, kWMinInt + 1);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0x7fffff8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7ffffffffffffc00UL);   // Largest double < INT64_MAX.
  __ Fneg(d30, d29);                    // Smallest double > INT64_MIN.

  __ Fcvtzu(w0, s0);
  __ Fcvtzu(w1, s1);
  __ Fcvtzu(w2, s2);
  __ Fcvtzu(w3, s3);
  __ Fcvtzu(w4, s4);
  __ Fcvtzu(w5, s5);
  __ Fcvtzu(w6, s6);
  __ Fcvtzu(w7, s7);
  __ Fcvtzu(w8, d8);
  __ Fcvtzu(w9, d9);
  __ Fcvtzu(w10, d10);
  __ Fcvtzu(w11, d11);
  __ Fcvtzu(w12, d12);
  __ Fcvtzu(w13, d13);
  __ Fcvtzu(w14, d14);
  __ Fcvtzu(x17, s17);
  __ Fcvtzu(x18, s18);
  __ Fcvtzu(x19, s19);
  __ Fcvtzu(x20, s20);
  __ Fcvtzu(x21, s21);
  __ Fcvtzu(x22, s22);
  __ Fcvtzu(x23, s23);
  __ Fcvtzu(x24, d24);
  __ Fcvtzu(x25, d25);
  __ Fcvtzu(x26, d26);
  __ Fcvtzu(x27, d27);
  __ Fcvtzu(x28, d28);
  __ Fcvtzu(x29, d29);
  __ Fcvtzu(x30, d30);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);
  CHECK_EQUAL_64(1, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0, x3);
  CHECK_EQUAL_64(0xffffffff, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0x7fffff80, x6);
  CHECK_EQUAL_64(0, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xffffffff, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0x7ffffffe, x14);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0x0UL, x19);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x20);
  CHECK_EQUAL_64(0x0UL, x21);
  CHECK_EQUAL_64(0x7fffff8000000000UL, x22);
  CHECK_EQUAL_64(0x0UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0x0UL, x26);
  CHECK_EQUAL_64(0xffffffffffffffffUL, x27);
  CHECK_EQUAL_64(0x0UL, x28);
  CHECK_EQUAL_64(0x7ffffffffffffc00UL, x29);
  CHECK_EQUAL_64(0x0UL, x30);

  TEARDOWN();
}


// Test that scvtf and ucvtf can convert the 64-bit input into the expected
// value. All possible values of 'fbits' are tested. The expected value is
// modified accordingly in each case.
//
// The expected value is specified as the bit encoding of the expected double
// produced by scvtf (expected_scvtf_bits) as well as ucvtf
// (expected_ucvtf_bits).
//
// Where the input value is representable by int32_t or uint32_t, conversions
// from W registers will also be tested.
static void TestUScvtfHelper(uint64_t in,
                             uint64_t expected_scvtf_bits,
                             uint64_t expected_ucvtf_bits) {
  uint64_t u64 = in;
  uint32_t u32 = u64 & 0xffffffff;
  int64_t s64 = static_cast<int64_t>(in);
  int32_t s32 = s64 & 0x7fffffff;

  bool cvtf_s32 = (s64 == s32);
  bool cvtf_u32 = (u64 == u32);

  double results_scvtf_x[65];
  double results_ucvtf_x[65];
  double results_scvtf_w[33];
  double results_ucvtf_w[33];

  SETUP();
  START();

  __ Mov(x0, reinterpret_cast<int64_t>(results_scvtf_x));
  __ Mov(x1, reinterpret_cast<int64_t>(results_ucvtf_x));
  __ Mov(x2, reinterpret_cast<int64_t>(results_scvtf_w));
  __ Mov(x3, reinterpret_cast<int64_t>(results_ucvtf_w));

  __ Mov(x10, s64);

  // Corrupt the top word, in case it is accidentally used during W-register
  // conversions.
  __ Mov(x11, 0x5555555555555555);
  __ Bfi(x11, x10, 0, kWRegSizeInBits);

  // Test integer conversions.
  __ Scvtf(d0, x10);
  __ Ucvtf(d1, x10);
  __ Scvtf(d2, w11);
  __ Ucvtf(d3, w11);
  __ Str(d0, MemOperand(x0));
  __ Str(d1, MemOperand(x1));
  __ Str(d2, MemOperand(x2));
  __ Str(d3, MemOperand(x3));

  // Test all possible values of fbits.
  for (int fbits = 1; fbits <= 32; fbits++) {
    __ Scvtf(d0, x10, fbits);
    __ Ucvtf(d1, x10, fbits);
    __ Scvtf(d2, w11, fbits);
    __ Ucvtf(d3, w11, fbits);
    __ Str(d0, MemOperand(x0, fbits * kDRegSize));
    __ Str(d1, MemOperand(x1, fbits * kDRegSize));
    __ Str(d2, MemOperand(x2, fbits * kDRegSize));
    __ Str(d3, MemOperand(x3, fbits * kDRegSize));
  }

  // Conversions from W registers can only handle fbits values <= 32, so just
  // test conversions from X registers for 32 < fbits <= 64.
  for (int fbits = 33; fbits <= 64; fbits++) {
    __ Scvtf(d0, x10, fbits);
    __ Ucvtf(d1, x10, fbits);
    __ Str(d0, MemOperand(x0, fbits * kDRegSize));
    __ Str(d1, MemOperand(x1, fbits * kDRegSize));
  }

  END();
  RUN();

  // Check the results.
  double expected_scvtf_base = rawbits_to_double(expected_scvtf_bits);
  double expected_ucvtf_base = rawbits_to_double(expected_ucvtf_bits);

  for (int fbits = 0; fbits <= 32; fbits++) {
    double expected_scvtf = expected_scvtf_base / pow(2.0, fbits);
    double expected_ucvtf = expected_ucvtf_base / pow(2.0, fbits);
    CHECK_EQUAL_FP64(expected_scvtf, results_scvtf_x[fbits]);
    CHECK_EQUAL_FP64(expected_ucvtf, results_ucvtf_x[fbits]);
    if (cvtf_s32) CHECK_EQUAL_FP64(expected_scvtf, results_scvtf_w[fbits]);
    if (cvtf_u32) CHECK_EQUAL_FP64(expected_ucvtf, results_ucvtf_w[fbits]);
  }
  for (int fbits = 33; fbits <= 64; fbits++) {
    double expected_scvtf = expected_scvtf_base / pow(2.0, fbits);
    double expected_ucvtf = expected_ucvtf_base / pow(2.0, fbits);
    CHECK_EQUAL_FP64(expected_scvtf, results_scvtf_x[fbits]);
    CHECK_EQUAL_FP64(expected_ucvtf, results_ucvtf_x[fbits]);
  }

  TEARDOWN();
}


TEST(scvtf_ucvtf_double) {
  INIT_V8();
  // Simple conversions of positive numbers which require no rounding; the
  // results should not depened on the rounding mode, and ucvtf and scvtf should
  // produce the same result.
  TestUScvtfHelper(0x0000000000000000, 0x0000000000000000, 0x0000000000000000);
  TestUScvtfHelper(0x0000000000000001, 0x3ff0000000000000, 0x3ff0000000000000);
  TestUScvtfHelper(0x0000000040000000, 0x41d0000000000000, 0x41d0000000000000);
  TestUScvtfHelper(0x0000000100000000, 0x41f0000000000000, 0x41f0000000000000);
  TestUScvtfHelper(0x4000000000000000, 0x43d0000000000000, 0x43d0000000000000);
  // Test mantissa extremities.
  TestUScvtfHelper(0x4000000000000400, 0x43d0000000000001, 0x43d0000000000001);
  // The largest int32_t that fits in a double.
  TestUScvtfHelper(0x000000007fffffff, 0x41dfffffffc00000, 0x41dfffffffc00000);
  // Values that would be negative if treated as an int32_t.
  TestUScvtfHelper(0x00000000ffffffff, 0x41efffffffe00000, 0x41efffffffe00000);
  TestUScvtfHelper(0x0000000080000000, 0x41e0000000000000, 0x41e0000000000000);
  TestUScvtfHelper(0x0000000080000001, 0x41e0000000200000, 0x41e0000000200000);
  // The largest int64_t that fits in a double.
  TestUScvtfHelper(0x7ffffffffffffc00, 0x43dfffffffffffff, 0x43dfffffffffffff);
  // Check for bit pattern reproduction.
  TestUScvtfHelper(0x0123456789abcde0, 0x43723456789abcde, 0x43723456789abcde);
  TestUScvtfHelper(0x0000000012345678, 0x41b2345678000000, 0x41b2345678000000);

  // Simple conversions of negative int64_t values. These require no rounding,
  // and the results should not depend on the rounding mode.
  TestUScvtfHelper(0xffffffffc0000000, 0xc1d0000000000000, 0x43effffffff80000);
  TestUScvtfHelper(0xffffffff00000000, 0xc1f0000000000000, 0x43efffffffe00000);
  TestUScvtfHelper(0xc000000000000000, 0xc3d0000000000000, 0x43e8000000000000);

  // Conversions which require rounding.
  TestUScvtfHelper(0x1000000000000000, 0x43b0000000000000, 0x43b0000000000000);
  TestUScvtfHelper(0x1000000000000001, 0x43b0000000000000, 0x43b0000000000000);
  TestUScvtfHelper(0x1000000000000080, 0x43b0000000000000, 0x43b0000000000000);
  TestUScvtfHelper(0x1000000000000081, 0x43b0000000000001, 0x43b0000000000001);
  TestUScvtfHelper(0x1000000000000100, 0x43b0000000000001, 0x43b0000000000001);
  TestUScvtfHelper(0x1000000000000101, 0x43b0000000000001, 0x43b0000000000001);
  TestUScvtfHelper(0x1000000000000180, 0x43b0000000000002, 0x43b0000000000002);
  TestUScvtfHelper(0x1000000000000181, 0x43b0000000000002, 0x43b0000000000002);
  TestUScvtfHelper(0x1000000000000200, 0x43b0000000000002, 0x43b0000000000002);
  TestUScvtfHelper(0x1000000000000201, 0x43b0000000000002, 0x43b0000000000002);
  TestUScvtfHelper(0x1000000000000280, 0x43b0000000000002, 0x43b0000000000002);
  TestUScvtfHelper(0x1000000000000281, 0x43b0000000000003, 0x43b0000000000003);
  TestUScvtfHelper(0x1000000000000300, 0x43b0000000000003, 0x43b0000000000003);
  // Check rounding of negative int64_t values (and large uint64_t values).
  TestUScvtfHelper(0x8000000000000000, 0xc3e0000000000000, 0x43e0000000000000);
  TestUScvtfHelper(0x8000000000000001, 0xc3e0000000000000, 0x43e0000000000000);
  TestUScvtfHelper(0x8000000000000200, 0xc3e0000000000000, 0x43e0000000000000);
  TestUScvtfHelper(0x8000000000000201, 0xc3dfffffffffffff, 0x43e0000000000000);
  TestUScvtfHelper(0x8000000000000400, 0xc3dfffffffffffff, 0x43e0000000000000);
  TestUScvtfHelper(0x8000000000000401, 0xc3dfffffffffffff, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000600, 0xc3dffffffffffffe, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000601, 0xc3dffffffffffffe, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000800, 0xc3dffffffffffffe, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000801, 0xc3dffffffffffffe, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000a00, 0xc3dffffffffffffe, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000a01, 0xc3dffffffffffffd, 0x43e0000000000001);
  TestUScvtfHelper(0x8000000000000c00, 0xc3dffffffffffffd, 0x43e0000000000002);
  // Round up to produce a result that's too big for the input to represent.
  TestUScvtfHelper(0x7ffffffffffffe00, 0x43e0000000000000, 0x43e0000000000000);
  TestUScvtfHelper(0x7fffffffffffffff, 0x43e0000000000000, 0x43e0000000000000);
  TestUScvtfHelper(0xfffffffffffffc00, 0xc090000000000000, 0x43f0000000000000);
  TestUScvtfHelper(0xffffffffffffffff, 0xbff0000000000000, 0x43f0000000000000);
}


// The same as TestUScvtfHelper, but convert to floats.
static void TestUScvtf32Helper(uint64_t in,
                               uint32_t expected_scvtf_bits,
                               uint32_t expected_ucvtf_bits) {
  uint64_t u64 = in;
  uint32_t u32 = u64 & 0xffffffff;
  int64_t s64 = static_cast<int64_t>(in);
  int32_t s32 = s64 & 0x7fffffff;

  bool cvtf_s32 = (s64 == s32);
  bool cvtf_u32 = (u64 == u32);

  float results_scvtf_x[65];
  float results_ucvtf_x[65];
  float results_scvtf_w[33];
  float results_ucvtf_w[33];

  SETUP();
  START();

  __ Mov(x0, reinterpret_cast<int64_t>(results_scvtf_x));
  __ Mov(x1, reinterpret_cast<int64_t>(results_ucvtf_x));
  __ Mov(x2, reinterpret_cast<int64_t>(results_scvtf_w));
  __ Mov(x3, reinterpret_cast<int64_t>(results_ucvtf_w));

  __ Mov(x10, s64);

  // Corrupt the top word, in case it is accidentally used during W-register
  // conversions.
  __ Mov(x11, 0x5555555555555555);
  __ Bfi(x11, x10, 0, kWRegSizeInBits);

  // Test integer conversions.
  __ Scvtf(s0, x10);
  __ Ucvtf(s1, x10);
  __ Scvtf(s2, w11);
  __ Ucvtf(s3, w11);
  __ Str(s0, MemOperand(x0));
  __ Str(s1, MemOperand(x1));
  __ Str(s2, MemOperand(x2));
  __ Str(s3, MemOperand(x3));

  // Test all possible values of fbits.
  for (int fbits = 1; fbits <= 32; fbits++) {
    __ Scvtf(s0, x10, fbits);
    __ Ucvtf(s1, x10, fbits);
    __ Scvtf(s2, w11, fbits);
    __ Ucvtf(s3, w11, fbits);
    __ Str(s0, MemOperand(x0, fbits * kSRegSize));
    __ Str(s1, MemOperand(x1, fbits * kSRegSize));
    __ Str(s2, MemOperand(x2, fbits * kSRegSize));
    __ Str(s3, MemOperand(x3, fbits * kSRegSize));
  }

  // Conversions from W registers can only handle fbits values <= 32, so just
  // test conversions from X registers for 32 < fbits <= 64.
  for (int fbits = 33; fbits <= 64; fbits++) {
    __ Scvtf(s0, x10, fbits);
    __ Ucvtf(s1, x10, fbits);
    __ Str(s0, MemOperand(x0, fbits * kSRegSize));
    __ Str(s1, MemOperand(x1, fbits * kSRegSize));
  }

  END();
  RUN();

  // Check the results.
  float expected_scvtf_base = rawbits_to_float(expected_scvtf_bits);
  float expected_ucvtf_base = rawbits_to_float(expected_ucvtf_bits);

  for (int fbits = 0; fbits <= 32; fbits++) {
    float expected_scvtf = expected_scvtf_base / powf(2, fbits);
    float expected_ucvtf = expected_ucvtf_base / powf(2, fbits);
    CHECK_EQUAL_FP32(expected_scvtf, results_scvtf_x[fbits]);
    CHECK_EQUAL_FP32(expected_ucvtf, results_ucvtf_x[fbits]);
    if (cvtf_s32) CHECK_EQUAL_FP32(expected_scvtf, results_scvtf_w[fbits]);
    if (cvtf_u32) CHECK_EQUAL_FP32(expected_ucvtf, results_ucvtf_w[fbits]);
    break;
  }
  for (int fbits = 33; fbits <= 64; fbits++) {
    break;
    float expected_scvtf = expected_scvtf_base / powf(2, fbits);
    float expected_ucvtf = expected_ucvtf_base / powf(2, fbits);
    CHECK_EQUAL_FP32(expected_scvtf, results_scvtf_x[fbits]);
    CHECK_EQUAL_FP32(expected_ucvtf, results_ucvtf_x[fbits]);
  }

  TEARDOWN();
}


TEST(scvtf_ucvtf_float) {
  INIT_V8();
  // Simple conversions of positive numbers which require no rounding; the
  // results should not depened on the rounding mode, and ucvtf and scvtf should
  // produce the same result.
  TestUScvtf32Helper(0x0000000000000000, 0x00000000, 0x00000000);
  TestUScvtf32Helper(0x0000000000000001, 0x3f800000, 0x3f800000);
  TestUScvtf32Helper(0x0000000040000000, 0x4e800000, 0x4e800000);
  TestUScvtf32Helper(0x0000000100000000, 0x4f800000, 0x4f800000);
  TestUScvtf32Helper(0x4000000000000000, 0x5e800000, 0x5e800000);
  // Test mantissa extremities.
  TestUScvtf32Helper(0x0000000000800001, 0x4b000001, 0x4b000001);
  TestUScvtf32Helper(0x4000008000000000, 0x5e800001, 0x5e800001);
  // The largest int32_t that fits in a float.
  TestUScvtf32Helper(0x000000007fffff80, 0x4effffff, 0x4effffff);
  // Values that would be negative if treated as an int32_t.
  TestUScvtf32Helper(0x00000000ffffff00, 0x4f7fffff, 0x4f7fffff);
  TestUScvtf32Helper(0x0000000080000000, 0x4f000000, 0x4f000000);
  TestUScvtf32Helper(0x0000000080000100, 0x4f000001, 0x4f000001);
  // The largest int64_t that fits in a float.
  TestUScvtf32Helper(0x7fffff8000000000, 0x5effffff, 0x5effffff);
  // Check for bit pattern reproduction.
  TestUScvtf32Helper(0x0000000000876543, 0x4b076543, 0x4b076543);

  // Simple conversions of negative int64_t values. These require no rounding,
  // and the results should not depend on the rounding mode.
  TestUScvtf32Helper(0xfffffc0000000000, 0xd4800000, 0x5f7ffffc);
  TestUScvtf32Helper(0xc000000000000000, 0xde800000, 0x5f400000);

  // Conversions which require rounding.
  TestUScvtf32Helper(0x0000800000000000, 0x57000000, 0x57000000);
  TestUScvtf32Helper(0x0000800000000001, 0x57000000, 0x57000000);
  TestUScvtf32Helper(0x0000800000800000, 0x57000000, 0x57000000);
  TestUScvtf32Helper(0x0000800000800001, 0x57000001, 0x57000001);
  TestUScvtf32Helper(0x0000800001000000, 0x57000001, 0x57000001);
  TestUScvtf32Helper(0x0000800001000001, 0x57000001, 0x57000001);
  TestUScvtf32Helper(0x0000800001800000, 0x57000002, 0x57000002);
  TestUScvtf32Helper(0x0000800001800001, 0x57000002, 0x57000002);
  TestUScvtf32Helper(0x0000800002000000, 0x57000002, 0x57000002);
  TestUScvtf32Helper(0x0000800002000001, 0x57000002, 0x57000002);
  TestUScvtf32Helper(0x0000800002800000, 0x57000002, 0x57000002);
  TestUScvtf32Helper(0x0000800002800001, 0x57000003, 0x57000003);
  TestUScvtf32Helper(0x0000800003000000, 0x57000003, 0x57000003);
  // Check rounding of negative int64_t values (and large uint64_t values).
  TestUScvtf32Helper(0x8000000000000000, 0xdf000000, 0x5f000000);
  TestUScvtf32Helper(0x8000000000000001, 0xdf000000, 0x5f000000);
  TestUScvtf32Helper(0x8000004000000000, 0xdf000000, 0x5f000000);
  TestUScvtf32Helper(0x8000004000000001, 0xdeffffff, 0x5f000000);
  TestUScvtf32Helper(0x8000008000000000, 0xdeffffff, 0x5f000000);
  TestUScvtf32Helper(0x8000008000000001, 0xdeffffff, 0x5f000001);
  TestUScvtf32Helper(0x800000c000000000, 0xdefffffe, 0x5f000001);
  TestUScvtf32Helper(0x800000c000000001, 0xdefffffe, 0x5f000001);
  TestUScvtf32Helper(0x8000010000000000, 0xdefffffe, 0x5f000001);
  TestUScvtf32Helper(0x8000010000000001, 0xdefffffe, 0x5f000001);
  TestUScvtf32Helper(0x8000014000000000, 0xdefffffe, 0x5f000001);
  TestUScvtf32Helper(0x8000014000000001, 0xdefffffd, 0x5f000001);
  TestUScvtf32Helper(0x8000018000000000, 0xdefffffd, 0x5f000002);
  // Round up to produce a result that's too big for the input to represent.
  TestUScvtf32Helper(0x000000007fffffc0, 0x4f000000, 0x4f000000);
  TestUScvtf32Helper(0x000000007fffffff, 0x4f000000, 0x4f000000);
  TestUScvtf32Helper(0x00000000ffffff80, 0x4f800000, 0x4f800000);
  TestUScvtf32Helper(0x00000000ffffffff, 0x4f800000, 0x4f800000);
  TestUScvtf32Helper(0x7fffffc000000000, 0x5f000000, 0x5f000000);
  TestUScvtf32Helper(0x7fffffffffffffff, 0x5f000000, 0x5f000000);
  TestUScvtf32Helper(0xffffff8000000000, 0xd3000000, 0x5f800000);
  TestUScvtf32Helper(0xffffffffffffffff, 0xbf800000, 0x5f800000);
}


TEST(system_mrs) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w0, 0);
  __ Mov(w1, 1);
  __ Mov(w2, 0x80000000);

  // Set the Z and C flags.
  __ Cmp(w0, w0);
  __ Mrs(x3, NZCV);

  // Set the N flag.
  __ Cmp(w0, w1);
  __ Mrs(x4, NZCV);

  // Set the Z, C and V flags.
  __ Adds(w0, w2, w2);
  __ Mrs(x5, NZCV);

  // Read the default FPCR.
  __ Mrs(x6, FPCR);
  END();

  RUN();

  // NZCV
  CHECK_EQUAL_32(ZCFlag, w3);
  CHECK_EQUAL_32(NFlag, w4);
  CHECK_EQUAL_32(ZCVFlag, w5);

  // FPCR
  // The default FPCR on Linux-based platforms is 0.
  CHECK_EQUAL_32(0, w6);

  TEARDOWN();
}


TEST(system_msr) {
  INIT_V8();
  // All FPCR fields that must be implemented: AHP, DN, FZ, RMode
  const uint64_t fpcr_core = 0x07c00000;

  // All FPCR fields (including fields which may be read-as-zero):
  //  Stride, Len
  //  IDE, IXE, UFE, OFE, DZE, IOE
  const uint64_t fpcr_all = fpcr_core | 0x00379f00;

  SETUP();

  START();
  __ Mov(w0, 0);
  __ Mov(w1, 0x7fffffff);

  __ Mov(x7, 0);

  __ Mov(x10, NVFlag);
  __ Cmp(w0, w0);     // Set Z and C.
  __ Msr(NZCV, x10);  // Set N and V.
  // The Msr should have overwritten every flag set by the Cmp.
  __ Cinc(x7, x7, mi);  // N
  __ Cinc(x7, x7, ne);  // !Z
  __ Cinc(x7, x7, lo);  // !C
  __ Cinc(x7, x7, vs);  // V

  __ Mov(x10, ZCFlag);
  __ Cmn(w1, w1);     // Set N and V.
  __ Msr(NZCV, x10);  // Set Z and C.
  // The Msr should have overwritten every flag set by the Cmn.
  __ Cinc(x7, x7, pl);  // !N
  __ Cinc(x7, x7, eq);  // Z
  __ Cinc(x7, x7, hs);  // C
  __ Cinc(x7, x7, vc);  // !V

  // All core FPCR fields must be writable.
  __ Mov(x8, fpcr_core);
  __ Msr(FPCR, x8);
  __ Mrs(x8, FPCR);

  // All FPCR fields, including optional ones. This part of the test doesn't
  // achieve much other than ensuring that supported fields can be cleared by
  // the next test.
  __ Mov(x9, fpcr_all);
  __ Msr(FPCR, x9);
  __ Mrs(x9, FPCR);
  __ And(x9, x9, fpcr_core);

  // The undefined bits must ignore writes.
  // It's conceivable that a future version of the architecture could use these
  // fields (making this test fail), but in the meantime this is a useful test
  // for the simulator.
  __ Mov(x10, ~fpcr_all);
  __ Msr(FPCR, x10);
  __ Mrs(x10, FPCR);

  END();

  RUN();

  // We should have incremented x7 (from 0) exactly 8 times.
  CHECK_EQUAL_64(8, x7);

  CHECK_EQUAL_64(fpcr_core, x8);
  CHECK_EQUAL_64(fpcr_core, x9);
  CHECK_EQUAL_64(0, x10);

  TEARDOWN();
}


TEST(system_nop) {
  INIT_V8();
  SETUP();
  RegisterDump before;

  START();
  before.Dump(&masm);
  __ Nop();
  END();

  RUN();

  CHECK_EQUAL_REGISTERS(before);
  CHECK_EQUAL_NZCV(before.flags_nzcv());

  TEARDOWN();
}


TEST(zero_dest) {
  INIT_V8();
  SETUP();
  RegisterDump before;

  START();
  // Preserve the system stack pointer, in case we clobber it.
  __ Mov(x30, csp);
  // Initialize the other registers used in this test.
  uint64_t literal_base = 0x0100001000100101UL;
  __ Mov(x0, 0);
  __ Mov(x1, literal_base);
  for (unsigned i = 2; i < x30.code(); i++) {
    __ Add(Register::XRegFromCode(i), Register::XRegFromCode(i-1), x1);
  }
  before.Dump(&masm);

  // All of these instructions should be NOPs in these forms, but have
  // alternate forms which can write into the stack pointer.
  __ add(xzr, x0, x1);
  __ add(xzr, x1, xzr);
  __ add(xzr, xzr, x1);

  __ and_(xzr, x0, x2);
  __ and_(xzr, x2, xzr);
  __ and_(xzr, xzr, x2);

  __ bic(xzr, x0, x3);
  __ bic(xzr, x3, xzr);
  __ bic(xzr, xzr, x3);

  __ eon(xzr, x0, x4);
  __ eon(xzr, x4, xzr);
  __ eon(xzr, xzr, x4);

  __ eor(xzr, x0, x5);
  __ eor(xzr, x5, xzr);
  __ eor(xzr, xzr, x5);

  __ orr(xzr, x0, x6);
  __ orr(xzr, x6, xzr);
  __ orr(xzr, xzr, x6);

  __ sub(xzr, x0, x7);
  __ sub(xzr, x7, xzr);
  __ sub(xzr, xzr, x7);

  // Swap the saved system stack pointer with the real one. If csp was written
  // during the test, it will show up in x30. This is done because the test
  // framework assumes that csp will be valid at the end of the test.
  __ Mov(x29, x30);
  __ Mov(x30, csp);
  __ Mov(csp, x29);
  // We used x29 as a scratch register, so reset it to make sure it doesn't
  // trigger a test failure.
  __ Add(x29, x28, x1);
  END();

  RUN();

  CHECK_EQUAL_REGISTERS(before);
  CHECK_EQUAL_NZCV(before.flags_nzcv());

  TEARDOWN();
}


TEST(zero_dest_setflags) {
  INIT_V8();
  SETUP();
  RegisterDump before;

  START();
  // Preserve the system stack pointer, in case we clobber it.
  __ Mov(x30, csp);
  // Initialize the other registers used in this test.
  uint64_t literal_base = 0x0100001000100101UL;
  __ Mov(x0, 0);
  __ Mov(x1, literal_base);
  for (int i = 2; i < 30; i++) {
    __ Add(Register::XRegFromCode(i), Register::XRegFromCode(i-1), x1);
  }
  before.Dump(&masm);

  // All of these instructions should only write to the flags in these forms,
  // but have alternate forms which can write into the stack pointer.
  __ adds(xzr, x0, Operand(x1, UXTX));
  __ adds(xzr, x1, Operand(xzr, UXTX));
  __ adds(xzr, x1, 1234);
  __ adds(xzr, x0, x1);
  __ adds(xzr, x1, xzr);
  __ adds(xzr, xzr, x1);

  __ ands(xzr, x2, ~0xf);
  __ ands(xzr, xzr, ~0xf);
  __ ands(xzr, x0, x2);
  __ ands(xzr, x2, xzr);
  __ ands(xzr, xzr, x2);

  __ bics(xzr, x3, ~0xf);
  __ bics(xzr, xzr, ~0xf);
  __ bics(xzr, x0, x3);
  __ bics(xzr, x3, xzr);
  __ bics(xzr, xzr, x3);

  __ subs(xzr, x0, Operand(x3, UXTX));
  __ subs(xzr, x3, Operand(xzr, UXTX));
  __ subs(xzr, x3, 1234);
  __ subs(xzr, x0, x3);
  __ subs(xzr, x3, xzr);
  __ subs(xzr, xzr, x3);

  // Swap the saved system stack pointer with the real one. If csp was written
  // during the test, it will show up in x30. This is done because the test
  // framework assumes that csp will be valid at the end of the test.
  __ Mov(x29, x30);
  __ Mov(x30, csp);
  __ Mov(csp, x29);
  // We used x29 as a scratch register, so reset it to make sure it doesn't
  // trigger a test failure.
  __ Add(x29, x28, x1);
  END();

  RUN();

  CHECK_EQUAL_REGISTERS(before);

  TEARDOWN();
}


TEST(register_bit) {
  // No code generation takes place in this test, so no need to setup and
  // teardown.

  // Simple tests.
  CHECK(x0.Bit() == (1UL << 0));
  CHECK(x1.Bit() == (1UL << 1));
  CHECK(x10.Bit() == (1UL << 10));

  // AAPCS64 definitions.
  CHECK(fp.Bit() == (1UL << kFramePointerRegCode));
  CHECK(lr.Bit() == (1UL << kLinkRegCode));

  // Fixed (hardware) definitions.
  CHECK(xzr.Bit() == (1UL << kZeroRegCode));

  // Internal ABI definitions.
  CHECK(jssp.Bit() == (1UL << kJSSPCode));
  CHECK(csp.Bit() == (1UL << kSPRegInternalCode));
  CHECK(csp.Bit() != xzr.Bit());

  // xn.Bit() == wn.Bit() at all times, for the same n.
  CHECK(x0.Bit() == w0.Bit());
  CHECK(x1.Bit() == w1.Bit());
  CHECK(x10.Bit() == w10.Bit());
  CHECK(jssp.Bit() == wjssp.Bit());
  CHECK(xzr.Bit() == wzr.Bit());
  CHECK(csp.Bit() == wcsp.Bit());
}


TEST(stack_pointer_override) {
  // This test generates some stack maintenance code, but the test only checks
  // the reported state.
  INIT_V8();
  SETUP();
  START();

  // The default stack pointer in V8 is jssp, but for compatibility with W16,
  // the test framework sets it to csp before calling the test.
  CHECK(csp.Is(__ StackPointer()));
  __ SetStackPointer(x0);
  CHECK(x0.Is(__ StackPointer()));
  __ SetStackPointer(jssp);
  CHECK(jssp.Is(__ StackPointer()));
  __ SetStackPointer(csp);
  CHECK(csp.Is(__ StackPointer()));

  END();
  RUN();
  TEARDOWN();
}


TEST(peek_poke_simple) {
  INIT_V8();
  SETUP();
  START();

  static const RegList x0_to_x3 = x0.Bit() | x1.Bit() | x2.Bit() | x3.Bit();
  static const RegList x10_to_x13 = x10.Bit() | x11.Bit() |
                                    x12.Bit() | x13.Bit();

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  // Initialize the registers.
  __ Mov(x0, literal_base);
  __ Add(x1, x0, x0);
  __ Add(x2, x1, x0);
  __ Add(x3, x2, x0);

  __ Claim(4);

  // Simple exchange.
  //  After this test:
  //    x0-x3 should be unchanged.
  //    w10-w13 should contain the lower words of x0-x3.
  __ Poke(x0, 0);
  __ Poke(x1, 8);
  __ Poke(x2, 16);
  __ Poke(x3, 24);
  Clobber(&masm, x0_to_x3);
  __ Peek(x0, 0);
  __ Peek(x1, 8);
  __ Peek(x2, 16);
  __ Peek(x3, 24);

  __ Poke(w0, 0);
  __ Poke(w1, 4);
  __ Poke(w2, 8);
  __ Poke(w3, 12);
  Clobber(&masm, x10_to_x13);
  __ Peek(w10, 0);
  __ Peek(w11, 4);
  __ Peek(w12, 8);
  __ Peek(w13, 12);

  __ Drop(4);

  END();
  RUN();

  CHECK_EQUAL_64(literal_base * 1, x0);
  CHECK_EQUAL_64(literal_base * 2, x1);
  CHECK_EQUAL_64(literal_base * 3, x2);
  CHECK_EQUAL_64(literal_base * 4, x3);

  CHECK_EQUAL_64((literal_base * 1) & 0xffffffff, x10);
  CHECK_EQUAL_64((literal_base * 2) & 0xffffffff, x11);
  CHECK_EQUAL_64((literal_base * 3) & 0xffffffff, x12);
  CHECK_EQUAL_64((literal_base * 4) & 0xffffffff, x13);

  TEARDOWN();
}


TEST(peek_poke_unaligned) {
  INIT_V8();
  SETUP();
  START();

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  // Initialize the registers.
  __ Mov(x0, literal_base);
  __ Add(x1, x0, x0);
  __ Add(x2, x1, x0);
  __ Add(x3, x2, x0);
  __ Add(x4, x3, x0);
  __ Add(x5, x4, x0);
  __ Add(x6, x5, x0);

  __ Claim(4);

  // Unaligned exchanges.
  //  After this test:
  //    x0-x6 should be unchanged.
  //    w10-w12 should contain the lower words of x0-x2.
  __ Poke(x0, 1);
  Clobber(&masm, x0.Bit());
  __ Peek(x0, 1);
  __ Poke(x1, 2);
  Clobber(&masm, x1.Bit());
  __ Peek(x1, 2);
  __ Poke(x2, 3);
  Clobber(&masm, x2.Bit());
  __ Peek(x2, 3);
  __ Poke(x3, 4);
  Clobber(&masm, x3.Bit());
  __ Peek(x3, 4);
  __ Poke(x4, 5);
  Clobber(&masm, x4.Bit());
  __ Peek(x4, 5);
  __ Poke(x5, 6);
  Clobber(&masm, x5.Bit());
  __ Peek(x5, 6);
  __ Poke(x6, 7);
  Clobber(&masm, x6.Bit());
  __ Peek(x6, 7);

  __ Poke(w0, 1);
  Clobber(&masm, w10.Bit());
  __ Peek(w10, 1);
  __ Poke(w1, 2);
  Clobber(&masm, w11.Bit());
  __ Peek(w11, 2);
  __ Poke(w2, 3);
  Clobber(&masm, w12.Bit());
  __ Peek(w12, 3);

  __ Drop(4);

  END();
  RUN();

  CHECK_EQUAL_64(literal_base * 1, x0);
  CHECK_EQUAL_64(literal_base * 2, x1);
  CHECK_EQUAL_64(literal_base * 3, x2);
  CHECK_EQUAL_64(literal_base * 4, x3);
  CHECK_EQUAL_64(literal_base * 5, x4);
  CHECK_EQUAL_64(literal_base * 6, x5);
  CHECK_EQUAL_64(literal_base * 7, x6);

  CHECK_EQUAL_64((literal_base * 1) & 0xffffffff, x10);
  CHECK_EQUAL_64((literal_base * 2) & 0xffffffff, x11);
  CHECK_EQUAL_64((literal_base * 3) & 0xffffffff, x12);

  TEARDOWN();
}


TEST(peek_poke_endianness) {
  INIT_V8();
  SETUP();
  START();

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  // Initialize the registers.
  __ Mov(x0, literal_base);
  __ Add(x1, x0, x0);

  __ Claim(4);

  // Endianness tests.
  //  After this section:
  //    x4 should match x0[31:0]:x0[63:32]
  //    w5 should match w1[15:0]:w1[31:16]
  __ Poke(x0, 0);
  __ Poke(x0, 8);
  __ Peek(x4, 4);

  __ Poke(w1, 0);
  __ Poke(w1, 4);
  __ Peek(w5, 2);

  __ Drop(4);

  END();
  RUN();

  uint64_t x0_expected = literal_base * 1;
  uint64_t x1_expected = literal_base * 2;
  uint64_t x4_expected = (x0_expected << 32) | (x0_expected >> 32);
  uint64_t x5_expected = ((x1_expected << 16) & 0xffff0000) |
                         ((x1_expected >> 16) & 0x0000ffff);

  CHECK_EQUAL_64(x0_expected, x0);
  CHECK_EQUAL_64(x1_expected, x1);
  CHECK_EQUAL_64(x4_expected, x4);
  CHECK_EQUAL_64(x5_expected, x5);

  TEARDOWN();
}


TEST(peek_poke_mixed) {
  INIT_V8();
  SETUP();
  START();

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  // Initialize the registers.
  __ Mov(x0, literal_base);
  __ Add(x1, x0, x0);
  __ Add(x2, x1, x0);
  __ Add(x3, x2, x0);

  __ Claim(4);

  // Mix with other stack operations.
  //  After this section:
  //    x0-x3 should be unchanged.
  //    x6 should match x1[31:0]:x0[63:32]
  //    w7 should match x1[15:0]:x0[63:48]
  __ Poke(x1, 8);
  __ Poke(x0, 0);
  {
    DCHECK(__ StackPointer().Is(csp));
    __ Mov(x4, __ StackPointer());
    __ SetStackPointer(x4);

    __ Poke(wzr, 0);    // Clobber the space we're about to drop.
    __ Drop(1, kWRegSize);
    __ Peek(x6, 0);
    __ Claim(1);
    __ Peek(w7, 10);
    __ Poke(x3, 28);
    __ Poke(xzr, 0);    // Clobber the space we're about to drop.
    __ Drop(1);
    __ Poke(x2, 12);
    __ Push(w0);

    __ Mov(csp, __ StackPointer());
    __ SetStackPointer(csp);
  }

  __ Pop(x0, x1, x2, x3);

  END();
  RUN();

  uint64_t x0_expected = literal_base * 1;
  uint64_t x1_expected = literal_base * 2;
  uint64_t x2_expected = literal_base * 3;
  uint64_t x3_expected = literal_base * 4;
  uint64_t x6_expected = (x1_expected << 32) | (x0_expected >> 32);
  uint64_t x7_expected = ((x1_expected << 16) & 0xffff0000) |
                         ((x0_expected >> 48) & 0x0000ffff);

  CHECK_EQUAL_64(x0_expected, x0);
  CHECK_EQUAL_64(x1_expected, x1);
  CHECK_EQUAL_64(x2_expected, x2);
  CHECK_EQUAL_64(x3_expected, x3);
  CHECK_EQUAL_64(x6_expected, x6);
  CHECK_EQUAL_64(x7_expected, x7);

  TEARDOWN();
}


// This enum is used only as an argument to the push-pop test helpers.
enum PushPopMethod {
  // Push or Pop using the Push and Pop methods, with blocks of up to four
  // registers. (Smaller blocks will be used if necessary.)
  PushPopByFour,

  // Use Push<Size>RegList and Pop<Size>RegList to transfer the registers.
  PushPopRegList
};


// The maximum number of registers that can be used by the PushPopJssp* tests,
// where a reg_count field is provided.
static int const kPushPopJsspMaxRegCount = -1;

// Test a simple push-pop pattern:
//  * Claim <claim> bytes to set the stack alignment.
//  * Push <reg_count> registers with size <reg_size>.
//  * Clobber the register contents.
//  * Pop <reg_count> registers to restore the original contents.
//  * Drop <claim> bytes to restore the original stack pointer.
//
// Different push and pop methods can be specified independently to test for
// proper word-endian behaviour.
static void PushPopJsspSimpleHelper(int reg_count,
                                    int claim,
                                    int reg_size,
                                    PushPopMethod push_method,
                                    PushPopMethod pop_method) {
  SETUP();

  START();

  // Registers in the TmpList can be used by the macro assembler for debug code
  // (for example in 'Pop'), so we can't use them here. We can't use jssp
  // because it will be the stack pointer for this test.
  static RegList const allowed = ~(masm.TmpList()->list() | jssp.Bit());
  if (reg_count == kPushPopJsspMaxRegCount) {
    reg_count = CountSetBits(allowed, kNumberOfRegisters);
  }
  // Work out which registers to use, based on reg_size.
  Register r[kNumberOfRegisters];
  Register x[kNumberOfRegisters];
  RegList list = PopulateRegisterArray(NULL, x, r, reg_size, reg_count,
                                       allowed);

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  {
    DCHECK(__ StackPointer().Is(csp));
    __ Mov(jssp, __ StackPointer());
    __ SetStackPointer(jssp);

    int i;

    // Initialize the registers.
    for (i = 0; i < reg_count; i++) {
      // Always write into the X register, to ensure that the upper word is
      // properly ignored by Push when testing W registers.
      if (!x[i].IsZero()) {
        __ Mov(x[i], literal_base * i);
      }
    }

    // Claim memory first, as requested.
    __ Claim(claim, kByteSizeInBytes);

    switch (push_method) {
      case PushPopByFour:
        // Push high-numbered registers first (to the highest addresses).
        for (i = reg_count; i >= 4; i -= 4) {
          __ Push(r[i-1], r[i-2], r[i-3], r[i-4]);
        }
        // Finish off the leftovers.
        switch (i) {
          case 3:  __ Push(r[2], r[1], r[0]); break;
          case 2:  __ Push(r[1], r[0]);       break;
          case 1:  __ Push(r[0]);             break;
          default: DCHECK(i == 0);            break;
        }
        break;
      case PushPopRegList:
        __ PushSizeRegList(list, reg_size);
        break;
    }

    // Clobber all the registers, to ensure that they get repopulated by Pop.
    Clobber(&masm, list);

    switch (pop_method) {
      case PushPopByFour:
        // Pop low-numbered registers first (from the lowest addresses).
        for (i = 0; i <= (reg_count-4); i += 4) {
          __ Pop(r[i], r[i+1], r[i+2], r[i+3]);
        }
        // Finish off the leftovers.
        switch (reg_count - i) {
          case 3:  __ Pop(r[i], r[i+1], r[i+2]); break;
          case 2:  __ Pop(r[i], r[i+1]);         break;
          case 1:  __ Pop(r[i]);                 break;
          default: DCHECK(i == reg_count);       break;
        }
        break;
      case PushPopRegList:
        __ PopSizeRegList(list, reg_size);
        break;
    }

    // Drop memory to restore jssp.
    __ Drop(claim, kByteSizeInBytes);

    __ Mov(csp, __ StackPointer());
    __ SetStackPointer(csp);
  }

  END();

  RUN();

  // Check that the register contents were preserved.
  // Always use CHECK_EQUAL_64, even when testing W registers, so we can test
  // that the upper word was properly cleared by Pop.
  literal_base &= (0xffffffffffffffffUL >> (64-reg_size));
  for (int i = 0; i < reg_count; i++) {
    if (x[i].IsZero()) {
      CHECK_EQUAL_64(0, x[i]);
    } else {
      CHECK_EQUAL_64(literal_base * i, x[i]);
    }
  }

  TEARDOWN();
}


TEST(push_pop_jssp_simple_32) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    for (int count = 0; count <= 8; count++) {
      PushPopJsspSimpleHelper(count, claim, kWRegSizeInBits,
                              PushPopByFour, PushPopByFour);
      PushPopJsspSimpleHelper(count, claim, kWRegSizeInBits,
                              PushPopByFour, PushPopRegList);
      PushPopJsspSimpleHelper(count, claim, kWRegSizeInBits,
                              PushPopRegList, PushPopByFour);
      PushPopJsspSimpleHelper(count, claim, kWRegSizeInBits,
                              PushPopRegList, PushPopRegList);
    }
    // Test with the maximum number of registers.
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kWRegSizeInBits,
                            PushPopByFour, PushPopByFour);
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kWRegSizeInBits,
                            PushPopByFour, PushPopRegList);
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kWRegSizeInBits,
                            PushPopRegList, PushPopByFour);
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kWRegSizeInBits,
                            PushPopRegList, PushPopRegList);
  }
}


TEST(push_pop_jssp_simple_64) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    for (int count = 0; count <= 8; count++) {
      PushPopJsspSimpleHelper(count, claim, kXRegSizeInBits,
                              PushPopByFour, PushPopByFour);
      PushPopJsspSimpleHelper(count, claim, kXRegSizeInBits,
                              PushPopByFour, PushPopRegList);
      PushPopJsspSimpleHelper(count, claim, kXRegSizeInBits,
                              PushPopRegList, PushPopByFour);
      PushPopJsspSimpleHelper(count, claim, kXRegSizeInBits,
                              PushPopRegList, PushPopRegList);
    }
    // Test with the maximum number of registers.
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kXRegSizeInBits,
                            PushPopByFour, PushPopByFour);
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kXRegSizeInBits,
                            PushPopByFour, PushPopRegList);
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kXRegSizeInBits,
                            PushPopRegList, PushPopByFour);
    PushPopJsspSimpleHelper(kPushPopJsspMaxRegCount, claim, kXRegSizeInBits,
                            PushPopRegList, PushPopRegList);
  }
}


// The maximum number of registers that can be used by the PushPopFPJssp* tests,
// where a reg_count field is provided.
static int const kPushPopFPJsspMaxRegCount = -1;

// Test a simple push-pop pattern:
//  * Claim <claim> bytes to set the stack alignment.
//  * Push <reg_count> FP registers with size <reg_size>.
//  * Clobber the register contents.
//  * Pop <reg_count> FP registers to restore the original contents.
//  * Drop <claim> bytes to restore the original stack pointer.
//
// Different push and pop methods can be specified independently to test for
// proper word-endian behaviour.
static void PushPopFPJsspSimpleHelper(int reg_count,
                                      int claim,
                                      int reg_size,
                                      PushPopMethod push_method,
                                      PushPopMethod pop_method) {
  SETUP();

  START();

  // We can use any floating-point register. None of them are reserved for
  // debug code, for example.
  static RegList const allowed = ~0;
  if (reg_count == kPushPopFPJsspMaxRegCount) {
    reg_count = CountSetBits(allowed, kNumberOfFPRegisters);
  }
  // Work out which registers to use, based on reg_size.
  FPRegister v[kNumberOfRegisters];
  FPRegister d[kNumberOfRegisters];
  RegList list = PopulateFPRegisterArray(NULL, d, v, reg_size, reg_count,
                                         allowed);

  // The literal base is chosen to have two useful properties:
  //  * When multiplied (using an integer) by small values (such as a register
  //    index), this value is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  //  * It is never a floating-point NaN, and will therefore always compare
  //    equal to itself.
  uint64_t literal_base = 0x0100001000100101UL;

  {
    DCHECK(__ StackPointer().Is(csp));
    __ Mov(jssp, __ StackPointer());
    __ SetStackPointer(jssp);

    int i;

    // Initialize the registers, using X registers to load the literal.
    __ Mov(x0, 0);
    __ Mov(x1, literal_base);
    for (i = 0; i < reg_count; i++) {
      // Always write into the D register, to ensure that the upper word is
      // properly ignored by Push when testing S registers.
      __ Fmov(d[i], x0);
      // Calculate the next literal.
      __ Add(x0, x0, x1);
    }

    // Claim memory first, as requested.
    __ Claim(claim, kByteSizeInBytes);

    switch (push_method) {
      case PushPopByFour:
        // Push high-numbered registers first (to the highest addresses).
        for (i = reg_count; i >= 4; i -= 4) {
          __ Push(v[i-1], v[i-2], v[i-3], v[i-4]);
        }
        // Finish off the leftovers.
        switch (i) {
          case 3:  __ Push(v[2], v[1], v[0]); break;
          case 2:  __ Push(v[1], v[0]);       break;
          case 1:  __ Push(v[0]);             break;
          default: DCHECK(i == 0);            break;
        }
        break;
      case PushPopRegList:
        __ PushSizeRegList(list, reg_size, CPURegister::kFPRegister);
        break;
    }

    // Clobber all the registers, to ensure that they get repopulated by Pop.
    ClobberFP(&masm, list);

    switch (pop_method) {
      case PushPopByFour:
        // Pop low-numbered registers first (from the lowest addresses).
        for (i = 0; i <= (reg_count-4); i += 4) {
          __ Pop(v[i], v[i+1], v[i+2], v[i+3]);
        }
        // Finish off the leftovers.
        switch (reg_count - i) {
          case 3:  __ Pop(v[i], v[i+1], v[i+2]); break;
          case 2:  __ Pop(v[i], v[i+1]);         break;
          case 1:  __ Pop(v[i]);                 break;
          default: DCHECK(i == reg_count);       break;
        }
        break;
      case PushPopRegList:
        __ PopSizeRegList(list, reg_size, CPURegister::kFPRegister);
        break;
    }

    // Drop memory to restore jssp.
    __ Drop(claim, kByteSizeInBytes);

    __ Mov(csp, __ StackPointer());
    __ SetStackPointer(csp);
  }

  END();

  RUN();

  // Check that the register contents were preserved.
  // Always use CHECK_EQUAL_FP64, even when testing S registers, so we can
  // test that the upper word was properly cleared by Pop.
  literal_base &= (0xffffffffffffffffUL >> (64-reg_size));
  for (int i = 0; i < reg_count; i++) {
    uint64_t literal = literal_base * i;
    double expected;
    memcpy(&expected, &literal, sizeof(expected));
    CHECK_EQUAL_FP64(expected, d[i]);
  }

  TEARDOWN();
}


TEST(push_pop_fp_jssp_simple_32) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    for (int count = 0; count <= 8; count++) {
      PushPopFPJsspSimpleHelper(count, claim, kSRegSizeInBits,
                                PushPopByFour, PushPopByFour);
      PushPopFPJsspSimpleHelper(count, claim, kSRegSizeInBits,
                                PushPopByFour, PushPopRegList);
      PushPopFPJsspSimpleHelper(count, claim, kSRegSizeInBits,
                                PushPopRegList, PushPopByFour);
      PushPopFPJsspSimpleHelper(count, claim, kSRegSizeInBits,
                                PushPopRegList, PushPopRegList);
    }
    // Test with the maximum number of registers.
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kSRegSizeInBits,
                              PushPopByFour, PushPopByFour);
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kSRegSizeInBits,
                              PushPopByFour, PushPopRegList);
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kSRegSizeInBits,
                              PushPopRegList, PushPopByFour);
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kSRegSizeInBits,
                              PushPopRegList, PushPopRegList);
  }
}


TEST(push_pop_fp_jssp_simple_64) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    for (int count = 0; count <= 8; count++) {
      PushPopFPJsspSimpleHelper(count, claim, kDRegSizeInBits,
                                PushPopByFour, PushPopByFour);
      PushPopFPJsspSimpleHelper(count, claim, kDRegSizeInBits,
                                PushPopByFour, PushPopRegList);
      PushPopFPJsspSimpleHelper(count, claim, kDRegSizeInBits,
                                PushPopRegList, PushPopByFour);
      PushPopFPJsspSimpleHelper(count, claim, kDRegSizeInBits,
                                PushPopRegList, PushPopRegList);
    }
    // Test with the maximum number of registers.
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kDRegSizeInBits,
                              PushPopByFour, PushPopByFour);
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kDRegSizeInBits,
                              PushPopByFour, PushPopRegList);
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kDRegSizeInBits,
                              PushPopRegList, PushPopByFour);
    PushPopFPJsspSimpleHelper(kPushPopFPJsspMaxRegCount, claim, kDRegSizeInBits,
                              PushPopRegList, PushPopRegList);
  }
}


// Push and pop data using an overlapping combination of Push/Pop and
// RegList-based methods.
static void PushPopJsspMixedMethodsHelper(int claim, int reg_size) {
  SETUP();

  // Registers x8 and x9 are used by the macro assembler for debug code (for
  // example in 'Pop'), so we can't use them here. We can't use jssp because it
  // will be the stack pointer for this test.
  static RegList const allowed =
      ~(x8.Bit() | x9.Bit() | jssp.Bit() | xzr.Bit());
  // Work out which registers to use, based on reg_size.
  Register r[10];
  Register x[10];
  PopulateRegisterArray(NULL, x, r, reg_size, 10, allowed);

  // Calculate some handy register lists.
  RegList r0_to_r3 = 0;
  for (int i = 0; i <= 3; i++) {
    r0_to_r3 |= x[i].Bit();
  }
  RegList r4_to_r5 = 0;
  for (int i = 4; i <= 5; i++) {
    r4_to_r5 |= x[i].Bit();
  }
  RegList r6_to_r9 = 0;
  for (int i = 6; i <= 9; i++) {
    r6_to_r9 |= x[i].Bit();
  }

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  START();
  {
    DCHECK(__ StackPointer().Is(csp));
    __ Mov(jssp, __ StackPointer());
    __ SetStackPointer(jssp);

    // Claim memory first, as requested.
    __ Claim(claim, kByteSizeInBytes);

    __ Mov(x[3], literal_base * 3);
    __ Mov(x[2], literal_base * 2);
    __ Mov(x[1], literal_base * 1);
    __ Mov(x[0], literal_base * 0);

    __ PushSizeRegList(r0_to_r3, reg_size);
    __ Push(r[3], r[2]);

    Clobber(&masm, r0_to_r3);
    __ PopSizeRegList(r0_to_r3, reg_size);

    __ Push(r[2], r[1], r[3], r[0]);

    Clobber(&masm, r4_to_r5);
    __ Pop(r[4], r[5]);
    Clobber(&masm, r6_to_r9);
    __ Pop(r[6], r[7], r[8], r[9]);

    // Drop memory to restore jssp.
    __ Drop(claim, kByteSizeInBytes);

    __ Mov(csp, __ StackPointer());
    __ SetStackPointer(csp);
  }

  END();

  RUN();

  // Always use CHECK_EQUAL_64, even when testing W registers, so we can test
  // that the upper word was properly cleared by Pop.
  literal_base &= (0xffffffffffffffffUL >> (64-reg_size));

  CHECK_EQUAL_64(literal_base * 3, x[9]);
  CHECK_EQUAL_64(literal_base * 2, x[8]);
  CHECK_EQUAL_64(literal_base * 0, x[7]);
  CHECK_EQUAL_64(literal_base * 3, x[6]);
  CHECK_EQUAL_64(literal_base * 1, x[5]);
  CHECK_EQUAL_64(literal_base * 2, x[4]);

  TEARDOWN();
}


TEST(push_pop_jssp_mixed_methods_64) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    PushPopJsspMixedMethodsHelper(claim, kXRegSizeInBits);
  }
}


TEST(push_pop_jssp_mixed_methods_32) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    PushPopJsspMixedMethodsHelper(claim, kWRegSizeInBits);
  }
}


// Push and pop data using overlapping X- and W-sized quantities.
static void PushPopJsspWXOverlapHelper(int reg_count, int claim) {
  // This test emits rather a lot of code.
  SETUP_SIZE(BUF_SIZE * 2);

  // Work out which registers to use, based on reg_size.
  Register tmp = x8;
  static RegList const allowed = ~(tmp.Bit() | jssp.Bit());
  if (reg_count == kPushPopJsspMaxRegCount) {
    reg_count = CountSetBits(allowed, kNumberOfRegisters);
  }
  Register w[kNumberOfRegisters];
  Register x[kNumberOfRegisters];
  RegList list = PopulateRegisterArray(w, x, NULL, 0, reg_count, allowed);

  // The number of W-sized slots we expect to pop. When we pop, we alternate
  // between W and X registers, so we need reg_count*1.5 W-sized slots.
  int const requested_w_slots = reg_count + reg_count / 2;

  // Track what _should_ be on the stack, using W-sized slots.
  static int const kMaxWSlots = kNumberOfRegisters + kNumberOfRegisters / 2;
  uint32_t stack[kMaxWSlots];
  for (int i = 0; i < kMaxWSlots; i++) {
    stack[i] = 0xdeadbeef;
  }

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  static uint64_t const literal_base = 0x0100001000100101UL;
  static uint64_t const literal_base_hi = literal_base >> 32;
  static uint64_t const literal_base_lo = literal_base & 0xffffffff;
  static uint64_t const literal_base_w = literal_base & 0xffffffff;

  START();
  {
    DCHECK(__ StackPointer().Is(csp));
    __ Mov(jssp, __ StackPointer());
    __ SetStackPointer(jssp);

    // Initialize the registers.
    for (int i = 0; i < reg_count; i++) {
      // Always write into the X register, to ensure that the upper word is
      // properly ignored by Push when testing W registers.
      if (!x[i].IsZero()) {
        __ Mov(x[i], literal_base * i);
      }
    }

    // Claim memory first, as requested.
    __ Claim(claim, kByteSizeInBytes);

    // The push-pop pattern is as follows:
    // Push:           Pop:
    //  x[0](hi)   ->   w[0]
    //  x[0](lo)   ->   x[1](hi)
    //  w[1]       ->   x[1](lo)
    //  w[1]       ->   w[2]
    //  x[2](hi)   ->   x[2](hi)
    //  x[2](lo)   ->   x[2](lo)
    //  x[2](hi)   ->   w[3]
    //  x[2](lo)   ->   x[4](hi)
    //  x[2](hi)   ->   x[4](lo)
    //  x[2](lo)   ->   w[5]
    //  w[3]       ->   x[5](hi)
    //  w[3]       ->   x[6](lo)
    //  w[3]       ->   w[7]
    //  w[3]       ->   x[8](hi)
    //  x[4](hi)   ->   x[8](lo)
    //  x[4](lo)   ->   w[9]
    // ... pattern continues ...
    //
    // That is, registers are pushed starting with the lower numbers,
    // alternating between x and w registers, and pushing i%4+1 copies of each,
    // where i is the register number.
    // Registers are popped starting with the higher numbers one-by-one,
    // alternating between x and w registers, but only popping one at a time.
    //
    // This pattern provides a wide variety of alignment effects and overlaps.

    // ---- Push ----

    int active_w_slots = 0;
    for (int i = 0; active_w_slots < requested_w_slots; i++) {
      DCHECK(i < reg_count);
      // In order to test various arguments to PushMultipleTimes, and to try to
      // exercise different alignment and overlap effects, we push each
      // register a different number of times.
      int times = i % 4 + 1;
      if (i & 1) {
        // Push odd-numbered registers as W registers.
        if (i & 2) {
          __ PushMultipleTimes(w[i], times);
        } else {
          // Use a register to specify the count.
          __ Mov(tmp.W(), times);
          __ PushMultipleTimes(w[i], tmp.W());
        }
        // Fill in the expected stack slots.
        for (int j = 0; j < times; j++) {
          if (w[i].Is(wzr)) {
            // The zero register always writes zeroes.
            stack[active_w_slots++] = 0;
          } else {
            stack[active_w_slots++] = literal_base_w * i;
          }
        }
      } else {
        // Push even-numbered registers as X registers.
        if (i & 2) {
          __ PushMultipleTimes(x[i], times);
        } else {
          // Use a register to specify the count.
          __ Mov(tmp, times);
          __ PushMultipleTimes(x[i], tmp);
        }
        // Fill in the expected stack slots.
        for (int j = 0; j < times; j++) {
          if (x[i].IsZero()) {
            // The zero register always writes zeroes.
            stack[active_w_slots++] = 0;
            stack[active_w_slots++] = 0;
          } else {
            stack[active_w_slots++] = literal_base_hi * i;
            stack[active_w_slots++] = literal_base_lo * i;
          }
        }
      }
    }
    // Because we were pushing several registers at a time, we probably pushed
    // more than we needed to.
    if (active_w_slots > requested_w_slots) {
      __ Drop(active_w_slots - requested_w_slots, kWRegSize);
      // Bump the number of active W-sized slots back to where it should be,
      // and fill the empty space with a dummy value.
      do {
        stack[active_w_slots--] = 0xdeadbeef;
      } while (active_w_slots > requested_w_slots);
    }

    // ---- Pop ----

    Clobber(&masm, list);

    // If popping an even number of registers, the first one will be X-sized.
    // Otherwise, the first one will be W-sized.
    bool next_is_64 = !(reg_count & 1);
    for (int i = reg_count-1; i >= 0; i--) {
      if (next_is_64) {
        __ Pop(x[i]);
        active_w_slots -= 2;
      } else {
        __ Pop(w[i]);
        active_w_slots -= 1;
      }
      next_is_64 = !next_is_64;
    }
    DCHECK(active_w_slots == 0);

    // Drop memory to restore jssp.
    __ Drop(claim, kByteSizeInBytes);

    __ Mov(csp, __ StackPointer());
    __ SetStackPointer(csp);
  }

  END();

  RUN();

  int slot = 0;
  for (int i = 0; i < reg_count; i++) {
    // Even-numbered registers were written as W registers.
    // Odd-numbered registers were written as X registers.
    bool expect_64 = (i & 1);
    uint64_t expected;

    if (expect_64) {
      uint64_t hi = stack[slot++];
      uint64_t lo = stack[slot++];
      expected = (hi << 32) | lo;
    } else {
      expected = stack[slot++];
    }

    // Always use CHECK_EQUAL_64, even when testing W registers, so we can
    // test that the upper word was properly cleared by Pop.
    if (x[i].IsZero()) {
      CHECK_EQUAL_64(0, x[i]);
    } else {
      CHECK_EQUAL_64(expected, x[i]);
    }
  }
  DCHECK(slot == requested_w_slots);

  TEARDOWN();
}


TEST(push_pop_jssp_wx_overlap) {
  INIT_V8();
  for (int claim = 0; claim <= 8; claim++) {
    for (int count = 1; count <= 8; count++) {
      PushPopJsspWXOverlapHelper(count, claim);
      PushPopJsspWXOverlapHelper(count, claim);
      PushPopJsspWXOverlapHelper(count, claim);
      PushPopJsspWXOverlapHelper(count, claim);
    }
    // Test with the maximum number of registers.
    PushPopJsspWXOverlapHelper(kPushPopJsspMaxRegCount, claim);
    PushPopJsspWXOverlapHelper(kPushPopJsspMaxRegCount, claim);
    PushPopJsspWXOverlapHelper(kPushPopJsspMaxRegCount, claim);
    PushPopJsspWXOverlapHelper(kPushPopJsspMaxRegCount, claim);
  }
}


TEST(push_pop_csp) {
  INIT_V8();
  SETUP();

  START();

  DCHECK(csp.Is(__ StackPointer()));

  __ Mov(x3, 0x3333333333333333UL);
  __ Mov(x2, 0x2222222222222222UL);
  __ Mov(x1, 0x1111111111111111UL);
  __ Mov(x0, 0x0000000000000000UL);
  __ Claim(2);
  __ PushXRegList(x0.Bit() | x1.Bit() | x2.Bit() | x3.Bit());
  __ Push(x3, x2);
  __ PopXRegList(x0.Bit() | x1.Bit() | x2.Bit() | x3.Bit());
  __ Push(x2, x1, x3, x0);
  __ Pop(x4, x5);
  __ Pop(x6, x7, x8, x9);

  __ Claim(2);
  __ PushWRegList(w0.Bit() | w1.Bit() | w2.Bit() | w3.Bit());
  __ Push(w3, w1, w2, w0);
  __ PopWRegList(w10.Bit() | w11.Bit() | w12.Bit() | w13.Bit());
  __ Pop(w14, w15, w16, w17);

  __ Claim(2);
  __ Push(w2, w2, w1, w1);
  __ Push(x3, x3);
  __ Pop(w18, w19, w20, w21);
  __ Pop(x22, x23);

  __ Claim(2);
  __ PushXRegList(x1.Bit() | x22.Bit());
  __ PopXRegList(x24.Bit() | x26.Bit());

  __ Claim(2);
  __ PushWRegList(w1.Bit() | w2.Bit() | w4.Bit() | w22.Bit());
  __ PopWRegList(w25.Bit() | w27.Bit() | w28.Bit() | w29.Bit());

  __ Claim(2);
  __ PushXRegList(0);
  __ PopXRegList(0);
  __ PushXRegList(0xffffffff);
  __ PopXRegList(0xffffffff);
  __ Drop(12);

  END();

  RUN();

  CHECK_EQUAL_64(0x1111111111111111UL, x3);
  CHECK_EQUAL_64(0x0000000000000000UL, x2);
  CHECK_EQUAL_64(0x3333333333333333UL, x1);
  CHECK_EQUAL_64(0x2222222222222222UL, x0);
  CHECK_EQUAL_64(0x3333333333333333UL, x9);
  CHECK_EQUAL_64(0x2222222222222222UL, x8);
  CHECK_EQUAL_64(0x0000000000000000UL, x7);
  CHECK_EQUAL_64(0x3333333333333333UL, x6);
  CHECK_EQUAL_64(0x1111111111111111UL, x5);
  CHECK_EQUAL_64(0x2222222222222222UL, x4);

  CHECK_EQUAL_32(0x11111111U, w13);
  CHECK_EQUAL_32(0x33333333U, w12);
  CHECK_EQUAL_32(0x00000000U, w11);
  CHECK_EQUAL_32(0x22222222U, w10);
  CHECK_EQUAL_32(0x11111111U, w17);
  CHECK_EQUAL_32(0x00000000U, w16);
  CHECK_EQUAL_32(0x33333333U, w15);
  CHECK_EQUAL_32(0x22222222U, w14);

  CHECK_EQUAL_32(0x11111111U, w18);
  CHECK_EQUAL_32(0x11111111U, w19);
  CHECK_EQUAL_32(0x11111111U, w20);
  CHECK_EQUAL_32(0x11111111U, w21);
  CHECK_EQUAL_64(0x3333333333333333UL, x22);
  CHECK_EQUAL_64(0x0000000000000000UL, x23);

  CHECK_EQUAL_64(0x3333333333333333UL, x24);
  CHECK_EQUAL_64(0x3333333333333333UL, x26);

  CHECK_EQUAL_32(0x33333333U, w25);
  CHECK_EQUAL_32(0x00000000U, w27);
  CHECK_EQUAL_32(0x22222222U, w28);
  CHECK_EQUAL_32(0x33333333U, w29);
  TEARDOWN();
}


TEST(push_queued) {
  INIT_V8();
  SETUP();

  START();

  DCHECK(__ StackPointer().Is(csp));
  __ Mov(jssp, __ StackPointer());
  __ SetStackPointer(jssp);

  MacroAssembler::PushPopQueue queue(&masm);

  // Queue up registers.
  queue.Queue(x0);
  queue.Queue(x1);
  queue.Queue(x2);
  queue.Queue(x3);

  queue.Queue(w4);
  queue.Queue(w5);
  queue.Queue(w6);

  queue.Queue(d0);
  queue.Queue(d1);

  queue.Queue(s2);

  __ Mov(x0, 0x1234000000000000);
  __ Mov(x1, 0x1234000100010001);
  __ Mov(x2, 0x1234000200020002);
  __ Mov(x3, 0x1234000300030003);
  __ Mov(w4, 0x12340004);
  __ Mov(w5, 0x12340005);
  __ Mov(w6, 0x12340006);
  __ Fmov(d0, 123400.0);
  __ Fmov(d1, 123401.0);
  __ Fmov(s2, 123402.0);

  // Actually push them.
  queue.PushQueued();

  Clobber(&masm, CPURegList(CPURegister::kRegister, kXRegSizeInBits, 0, 6));
  Clobber(&masm, CPURegList(CPURegister::kFPRegister, kDRegSizeInBits, 0, 2));

  // Pop them conventionally.
  __ Pop(s2);
  __ Pop(d1, d0);
  __ Pop(w6, w5, w4);
  __ Pop(x3, x2, x1, x0);

  __ Mov(csp, __ StackPointer());
  __ SetStackPointer(csp);

  END();

  RUN();

  CHECK_EQUAL_64(0x1234000000000000, x0);
  CHECK_EQUAL_64(0x1234000100010001, x1);
  CHECK_EQUAL_64(0x1234000200020002, x2);
  CHECK_EQUAL_64(0x1234000300030003, x3);

  CHECK_EQUAL_32(0x12340004, w4);
  CHECK_EQUAL_32(0x12340005, w5);
  CHECK_EQUAL_32(0x12340006, w6);

  CHECK_EQUAL_FP64(123400.0, d0);
  CHECK_EQUAL_FP64(123401.0, d1);

  CHECK_EQUAL_FP32(123402.0, s2);

  TEARDOWN();
}


TEST(pop_queued) {
  INIT_V8();
  SETUP();

  START();

  DCHECK(__ StackPointer().Is(csp));
  __ Mov(jssp, __ StackPointer());
  __ SetStackPointer(jssp);

  MacroAssembler::PushPopQueue queue(&masm);

  __ Mov(x0, 0x1234000000000000);
  __ Mov(x1, 0x1234000100010001);
  __ Mov(x2, 0x1234000200020002);
  __ Mov(x3, 0x1234000300030003);
  __ Mov(w4, 0x12340004);
  __ Mov(w5, 0x12340005);
  __ Mov(w6, 0x12340006);
  __ Fmov(d0, 123400.0);
  __ Fmov(d1, 123401.0);
  __ Fmov(s2, 123402.0);

  // Push registers conventionally.
  __ Push(x0, x1, x2, x3);
  __ Push(w4, w5, w6);
  __ Push(d0, d1);
  __ Push(s2);

  // Queue up a pop.
  queue.Queue(s2);

  queue.Queue(d1);
  queue.Queue(d0);

  queue.Queue(w6);
  queue.Queue(w5);
  queue.Queue(w4);

  queue.Queue(x3);
  queue.Queue(x2);
  queue.Queue(x1);
  queue.Queue(x0);

  Clobber(&masm, CPURegList(CPURegister::kRegister, kXRegSizeInBits, 0, 6));
  Clobber(&masm, CPURegList(CPURegister::kFPRegister, kDRegSizeInBits, 0, 2));

  // Actually pop them.
  queue.PopQueued();

  __ Mov(csp, __ StackPointer());
  __ SetStackPointer(csp);

  END();

  RUN();

  CHECK_EQUAL_64(0x1234000000000000, x0);
  CHECK_EQUAL_64(0x1234000100010001, x1);
  CHECK_EQUAL_64(0x1234000200020002, x2);
  CHECK_EQUAL_64(0x1234000300030003, x3);

  CHECK_EQUAL_64(0x0000000012340004, x4);
  CHECK_EQUAL_64(0x0000000012340005, x5);
  CHECK_EQUAL_64(0x0000000012340006, x6);

  CHECK_EQUAL_FP64(123400.0, d0);
  CHECK_EQUAL_FP64(123401.0, d1);

  CHECK_EQUAL_FP32(123402.0, s2);

  TEARDOWN();
}


TEST(jump_both_smi) {
  INIT_V8();
  SETUP();

  Label cond_pass_00, cond_pass_01, cond_pass_10, cond_pass_11;
  Label cond_fail_00, cond_fail_01, cond_fail_10, cond_fail_11;
  Label return1, return2, return3, done;

  START();

  __ Mov(x0, 0x5555555500000001UL);  // A pointer.
  __ Mov(x1, 0xaaaaaaaa00000001UL);  // A pointer.
  __ Mov(x2, 0x1234567800000000UL);  // A smi.
  __ Mov(x3, 0x8765432100000000UL);  // A smi.
  __ Mov(x4, 0xdead);
  __ Mov(x5, 0xdead);
  __ Mov(x6, 0xdead);
  __ Mov(x7, 0xdead);

  __ JumpIfBothSmi(x0, x1, &cond_pass_00, &cond_fail_00);
  __ Bind(&return1);
  __ JumpIfBothSmi(x0, x2, &cond_pass_01, &cond_fail_01);
  __ Bind(&return2);
  __ JumpIfBothSmi(x2, x1, &cond_pass_10, &cond_fail_10);
  __ Bind(&return3);
  __ JumpIfBothSmi(x2, x3, &cond_pass_11, &cond_fail_11);

  __ Bind(&cond_fail_00);
  __ Mov(x4, 0);
  __ B(&return1);
  __ Bind(&cond_pass_00);
  __ Mov(x4, 1);
  __ B(&return1);

  __ Bind(&cond_fail_01);
  __ Mov(x5, 0);
  __ B(&return2);
  __ Bind(&cond_pass_01);
  __ Mov(x5, 1);
  __ B(&return2);

  __ Bind(&cond_fail_10);
  __ Mov(x6, 0);
  __ B(&return3);
  __ Bind(&cond_pass_10);
  __ Mov(x6, 1);
  __ B(&return3);

  __ Bind(&cond_fail_11);
  __ Mov(x7, 0);
  __ B(&done);
  __ Bind(&cond_pass_11);
  __ Mov(x7, 1);

  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x5555555500000001UL, x0);
  CHECK_EQUAL_64(0xaaaaaaaa00000001UL, x1);
  CHECK_EQUAL_64(0x1234567800000000UL, x2);
  CHECK_EQUAL_64(0x8765432100000000UL, x3);
  CHECK_EQUAL_64(0, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0, x6);
  CHECK_EQUAL_64(1, x7);

  TEARDOWN();
}


TEST(jump_either_smi) {
  INIT_V8();
  SETUP();

  Label cond_pass_00, cond_pass_01, cond_pass_10, cond_pass_11;
  Label cond_fail_00, cond_fail_01, cond_fail_10, cond_fail_11;
  Label return1, return2, return3, done;

  START();

  __ Mov(x0, 0x5555555500000001UL);  // A pointer.
  __ Mov(x1, 0xaaaaaaaa00000001UL);  // A pointer.
  __ Mov(x2, 0x1234567800000000UL);  // A smi.
  __ Mov(x3, 0x8765432100000000UL);  // A smi.
  __ Mov(x4, 0xdead);
  __ Mov(x5, 0xdead);
  __ Mov(x6, 0xdead);
  __ Mov(x7, 0xdead);

  __ JumpIfEitherSmi(x0, x1, &cond_pass_00, &cond_fail_00);
  __ Bind(&return1);
  __ JumpIfEitherSmi(x0, x2, &cond_pass_01, &cond_fail_01);
  __ Bind(&return2);
  __ JumpIfEitherSmi(x2, x1, &cond_pass_10, &cond_fail_10);
  __ Bind(&return3);
  __ JumpIfEitherSmi(x2, x3, &cond_pass_11, &cond_fail_11);

  __ Bind(&cond_fail_00);
  __ Mov(x4, 0);
  __ B(&return1);
  __ Bind(&cond_pass_00);
  __ Mov(x4, 1);
  __ B(&return1);

  __ Bind(&cond_fail_01);
  __ Mov(x5, 0);
  __ B(&return2);
  __ Bind(&cond_pass_01);
  __ Mov(x5, 1);
  __ B(&return2);

  __ Bind(&cond_fail_10);
  __ Mov(x6, 0);
  __ B(&return3);
  __ Bind(&cond_pass_10);
  __ Mov(x6, 1);
  __ B(&return3);

  __ Bind(&cond_fail_11);
  __ Mov(x7, 0);
  __ B(&done);
  __ Bind(&cond_pass_11);
  __ Mov(x7, 1);

  __ Bind(&done);

  END();

  RUN();

  CHECK_EQUAL_64(0x5555555500000001UL, x0);
  CHECK_EQUAL_64(0xaaaaaaaa00000001UL, x1);
  CHECK_EQUAL_64(0x1234567800000000UL, x2);
  CHECK_EQUAL_64(0x8765432100000000UL, x3);
  CHECK_EQUAL_64(0, x4);
  CHECK_EQUAL_64(1, x5);
  CHECK_EQUAL_64(1, x6);
  CHECK_EQUAL_64(1, x7);

  TEARDOWN();
}


TEST(noreg) {
  // This test doesn't generate any code, but it verifies some invariants
  // related to NoReg.
  CHECK(NoReg.Is(NoFPReg));
  CHECK(NoFPReg.Is(NoReg));
  CHECK(NoReg.Is(NoCPUReg));
  CHECK(NoCPUReg.Is(NoReg));
  CHECK(NoFPReg.Is(NoCPUReg));
  CHECK(NoCPUReg.Is(NoFPReg));

  CHECK(NoReg.IsNone());
  CHECK(NoFPReg.IsNone());
  CHECK(NoCPUReg.IsNone());
}


TEST(isvalid) {
  // This test doesn't generate any code, but it verifies some invariants
  // related to IsValid().
  CHECK(!NoReg.IsValid());
  CHECK(!NoFPReg.IsValid());
  CHECK(!NoCPUReg.IsValid());

  CHECK(x0.IsValid());
  CHECK(w0.IsValid());
  CHECK(x30.IsValid());
  CHECK(w30.IsValid());
  CHECK(xzr.IsValid());
  CHECK(wzr.IsValid());

  CHECK(csp.IsValid());
  CHECK(wcsp.IsValid());

  CHECK(d0.IsValid());
  CHECK(s0.IsValid());
  CHECK(d31.IsValid());
  CHECK(s31.IsValid());

  CHECK(x0.IsValidRegister());
  CHECK(w0.IsValidRegister());
  CHECK(xzr.IsValidRegister());
  CHECK(wzr.IsValidRegister());
  CHECK(csp.IsValidRegister());
  CHECK(wcsp.IsValidRegister());
  CHECK(!x0.IsValidFPRegister());
  CHECK(!w0.IsValidFPRegister());
  CHECK(!xzr.IsValidFPRegister());
  CHECK(!wzr.IsValidFPRegister());
  CHECK(!csp.IsValidFPRegister());
  CHECK(!wcsp.IsValidFPRegister());

  CHECK(d0.IsValidFPRegister());
  CHECK(s0.IsValidFPRegister());
  CHECK(!d0.IsValidRegister());
  CHECK(!s0.IsValidRegister());

  // Test the same as before, but using CPURegister types. This shouldn't make
  // any difference.
  CHECK(static_cast<CPURegister>(x0).IsValid());
  CHECK(static_cast<CPURegister>(w0).IsValid());
  CHECK(static_cast<CPURegister>(x30).IsValid());
  CHECK(static_cast<CPURegister>(w30).IsValid());
  CHECK(static_cast<CPURegister>(xzr).IsValid());
  CHECK(static_cast<CPURegister>(wzr).IsValid());

  CHECK(static_cast<CPURegister>(csp).IsValid());
  CHECK(static_cast<CPURegister>(wcsp).IsValid());

  CHECK(static_cast<CPURegister>(d0).IsValid());
  CHECK(static_cast<CPURegister>(s0).IsValid());
  CHECK(static_cast<CPURegister>(d31).IsValid());
  CHECK(static_cast<CPURegister>(s31).IsValid());

  CHECK(static_cast<CPURegister>(x0).IsValidRegister());
  CHECK(static_cast<CPURegister>(w0).IsValidRegister());
  CHECK(static_cast<CPURegister>(xzr).IsValidRegister());
  CHECK(static_cast<CPURegister>(wzr).IsValidRegister());
  CHECK(static_cast<CPURegister>(csp).IsValidRegister());
  CHECK(static_cast<CPURegister>(wcsp).IsValidRegister());
  CHECK(!static_cast<CPURegister>(x0).IsValidFPRegister());
  CHECK(!static_cast<CPURegister>(w0).IsValidFPRegister());
  CHECK(!static_cast<CPURegister>(xzr).IsValidFPRegister());
  CHECK(!static_cast<CPURegister>(wzr).IsValidFPRegister());
  CHECK(!static_cast<CPURegister>(csp).IsValidFPRegister());
  CHECK(!static_cast<CPURegister>(wcsp).IsValidFPRegister());

  CHECK(static_cast<CPURegister>(d0).IsValidFPRegister());
  CHECK(static_cast<CPURegister>(s0).IsValidFPRegister());
  CHECK(!static_cast<CPURegister>(d0).IsValidRegister());
  CHECK(!static_cast<CPURegister>(s0).IsValidRegister());
}


TEST(cpureglist_utils_x) {
  // This test doesn't generate any code, but it verifies the behaviour of
  // the CPURegList utility methods.

  // Test a list of X registers.
  CPURegList test(x0, x1, x2, x3);

  CHECK(test.IncludesAliasOf(x0));
  CHECK(test.IncludesAliasOf(x1));
  CHECK(test.IncludesAliasOf(x2));
  CHECK(test.IncludesAliasOf(x3));
  CHECK(test.IncludesAliasOf(w0));
  CHECK(test.IncludesAliasOf(w1));
  CHECK(test.IncludesAliasOf(w2));
  CHECK(test.IncludesAliasOf(w3));

  CHECK(!test.IncludesAliasOf(x4));
  CHECK(!test.IncludesAliasOf(x30));
  CHECK(!test.IncludesAliasOf(xzr));
  CHECK(!test.IncludesAliasOf(csp));
  CHECK(!test.IncludesAliasOf(w4));
  CHECK(!test.IncludesAliasOf(w30));
  CHECK(!test.IncludesAliasOf(wzr));
  CHECK(!test.IncludesAliasOf(wcsp));

  CHECK(!test.IncludesAliasOf(d0));
  CHECK(!test.IncludesAliasOf(d1));
  CHECK(!test.IncludesAliasOf(d2));
  CHECK(!test.IncludesAliasOf(d3));
  CHECK(!test.IncludesAliasOf(s0));
  CHECK(!test.IncludesAliasOf(s1));
  CHECK(!test.IncludesAliasOf(s2));
  CHECK(!test.IncludesAliasOf(s3));

  CHECK(!test.IsEmpty());

  CHECK(test.type() == x0.type());

  CHECK(test.PopHighestIndex().Is(x3));
  CHECK(test.PopLowestIndex().Is(x0));

  CHECK(test.IncludesAliasOf(x1));
  CHECK(test.IncludesAliasOf(x2));
  CHECK(test.IncludesAliasOf(w1));
  CHECK(test.IncludesAliasOf(w2));
  CHECK(!test.IncludesAliasOf(x0));
  CHECK(!test.IncludesAliasOf(x3));
  CHECK(!test.IncludesAliasOf(w0));
  CHECK(!test.IncludesAliasOf(w3));

  CHECK(test.PopHighestIndex().Is(x2));
  CHECK(test.PopLowestIndex().Is(x1));

  CHECK(!test.IncludesAliasOf(x1));
  CHECK(!test.IncludesAliasOf(x2));
  CHECK(!test.IncludesAliasOf(w1));
  CHECK(!test.IncludesAliasOf(w2));

  CHECK(test.IsEmpty());
}


TEST(cpureglist_utils_w) {
  // This test doesn't generate any code, but it verifies the behaviour of
  // the CPURegList utility methods.

  // Test a list of W registers.
  CPURegList test(w10, w11, w12, w13);

  CHECK(test.IncludesAliasOf(x10));
  CHECK(test.IncludesAliasOf(x11));
  CHECK(test.IncludesAliasOf(x12));
  CHECK(test.IncludesAliasOf(x13));
  CHECK(test.IncludesAliasOf(w10));
  CHECK(test.IncludesAliasOf(w11));
  CHECK(test.IncludesAliasOf(w12));
  CHECK(test.IncludesAliasOf(w13));

  CHECK(!test.IncludesAliasOf(x0));
  CHECK(!test.IncludesAliasOf(x9));
  CHECK(!test.IncludesAliasOf(x14));
  CHECK(!test.IncludesAliasOf(x30));
  CHECK(!test.IncludesAliasOf(xzr));
  CHECK(!test.IncludesAliasOf(csp));
  CHECK(!test.IncludesAliasOf(w0));
  CHECK(!test.IncludesAliasOf(w9));
  CHECK(!test.IncludesAliasOf(w14));
  CHECK(!test.IncludesAliasOf(w30));
  CHECK(!test.IncludesAliasOf(wzr));
  CHECK(!test.IncludesAliasOf(wcsp));

  CHECK(!test.IncludesAliasOf(d10));
  CHECK(!test.IncludesAliasOf(d11));
  CHECK(!test.IncludesAliasOf(d12));
  CHECK(!test.IncludesAliasOf(d13));
  CHECK(!test.IncludesAliasOf(s10));
  CHECK(!test.IncludesAliasOf(s11));
  CHECK(!test.IncludesAliasOf(s12));
  CHECK(!test.IncludesAliasOf(s13));

  CHECK(!test.IsEmpty());

  CHECK(test.type() == w10.type());

  CHECK(test.PopHighestIndex().Is(w13));
  CHECK(test.PopLowestIndex().Is(w10));

  CHECK(test.IncludesAliasOf(x11));
  CHECK(test.IncludesAliasOf(x12));
  CHECK(test.IncludesAliasOf(w11));
  CHECK(test.IncludesAliasOf(w12));
  CHECK(!test.IncludesAliasOf(x10));
  CHECK(!test.IncludesAliasOf(x13));
  CHECK(!test.IncludesAliasOf(w10));
  CHECK(!test.IncludesAliasOf(w13));

  CHECK(test.PopHighestIndex().Is(w12));
  CHECK(test.PopLowestIndex().Is(w11));

  CHECK(!test.IncludesAliasOf(x11));
  CHECK(!test.IncludesAliasOf(x12));
  CHECK(!test.IncludesAliasOf(w11));
  CHECK(!test.IncludesAliasOf(w12));

  CHECK(test.IsEmpty());
}


TEST(cpureglist_utils_d) {
  // This test doesn't generate any code, but it verifies the behaviour of
  // the CPURegList utility methods.

  // Test a list of D registers.
  CPURegList test(d20, d21, d22, d23);

  CHECK(test.IncludesAliasOf(d20));
  CHECK(test.IncludesAliasOf(d21));
  CHECK(test.IncludesAliasOf(d22));
  CHECK(test.IncludesAliasOf(d23));
  CHECK(test.IncludesAliasOf(s20));
  CHECK(test.IncludesAliasOf(s21));
  CHECK(test.IncludesAliasOf(s22));
  CHECK(test.IncludesAliasOf(s23));

  CHECK(!test.IncludesAliasOf(d0));
  CHECK(!test.IncludesAliasOf(d19));
  CHECK(!test.IncludesAliasOf(d24));
  CHECK(!test.IncludesAliasOf(d31));
  CHECK(!test.IncludesAliasOf(s0));
  CHECK(!test.IncludesAliasOf(s19));
  CHECK(!test.IncludesAliasOf(s24));
  CHECK(!test.IncludesAliasOf(s31));

  CHECK(!test.IncludesAliasOf(x20));
  CHECK(!test.IncludesAliasOf(x21));
  CHECK(!test.IncludesAliasOf(x22));
  CHECK(!test.IncludesAliasOf(x23));
  CHECK(!test.IncludesAliasOf(w20));
  CHECK(!test.IncludesAliasOf(w21));
  CHECK(!test.IncludesAliasOf(w22));
  CHECK(!test.IncludesAliasOf(w23));

  CHECK(!test.IncludesAliasOf(xzr));
  CHECK(!test.IncludesAliasOf(wzr));
  CHECK(!test.IncludesAliasOf(csp));
  CHECK(!test.IncludesAliasOf(wcsp));

  CHECK(!test.IsEmpty());

  CHECK(test.type() == d20.type());

  CHECK(test.PopHighestIndex().Is(d23));
  CHECK(test.PopLowestIndex().Is(d20));

  CHECK(test.IncludesAliasOf(d21));
  CHECK(test.IncludesAliasOf(d22));
  CHECK(test.IncludesAliasOf(s21));
  CHECK(test.IncludesAliasOf(s22));
  CHECK(!test.IncludesAliasOf(d20));
  CHECK(!test.IncludesAliasOf(d23));
  CHECK(!test.IncludesAliasOf(s20));
  CHECK(!test.IncludesAliasOf(s23));

  CHECK(test.PopHighestIndex().Is(d22));
  CHECK(test.PopLowestIndex().Is(d21));

  CHECK(!test.IncludesAliasOf(d21));
  CHECK(!test.IncludesAliasOf(d22));
  CHECK(!test.IncludesAliasOf(s21));
  CHECK(!test.IncludesAliasOf(s22));

  CHECK(test.IsEmpty());
}


TEST(cpureglist_utils_s) {
  // This test doesn't generate any code, but it verifies the behaviour of
  // the CPURegList utility methods.

  // Test a list of S registers.
  CPURegList test(s20, s21, s22, s23);

  // The type and size mechanisms are already covered, so here we just test
  // that lists of S registers alias individual D registers.

  CHECK(test.IncludesAliasOf(d20));
  CHECK(test.IncludesAliasOf(d21));
  CHECK(test.IncludesAliasOf(d22));
  CHECK(test.IncludesAliasOf(d23));
  CHECK(test.IncludesAliasOf(s20));
  CHECK(test.IncludesAliasOf(s21));
  CHECK(test.IncludesAliasOf(s22));
  CHECK(test.IncludesAliasOf(s23));
}


TEST(cpureglist_utils_empty) {
  // This test doesn't generate any code, but it verifies the behaviour of
  // the CPURegList utility methods.

  // Test an empty list.
  // Empty lists can have type and size properties. Check that we can create
  // them, and that they are empty.
  CPURegList reg32(CPURegister::kRegister, kWRegSizeInBits, 0);
  CPURegList reg64(CPURegister::kRegister, kXRegSizeInBits, 0);
  CPURegList fpreg32(CPURegister::kFPRegister, kSRegSizeInBits, 0);
  CPURegList fpreg64(CPURegister::kFPRegister, kDRegSizeInBits, 0);

  CHECK(reg32.IsEmpty());
  CHECK(reg64.IsEmpty());
  CHECK(fpreg32.IsEmpty());
  CHECK(fpreg64.IsEmpty());

  CHECK(reg32.PopLowestIndex().IsNone());
  CHECK(reg64.PopLowestIndex().IsNone());
  CHECK(fpreg32.PopLowestIndex().IsNone());
  CHECK(fpreg64.PopLowestIndex().IsNone());

  CHECK(reg32.PopHighestIndex().IsNone());
  CHECK(reg64.PopHighestIndex().IsNone());
  CHECK(fpreg32.PopHighestIndex().IsNone());
  CHECK(fpreg64.PopHighestIndex().IsNone());

  CHECK(reg32.IsEmpty());
  CHECK(reg64.IsEmpty());
  CHECK(fpreg32.IsEmpty());
  CHECK(fpreg64.IsEmpty());
}


TEST(printf) {
  INIT_V8();
  SETUP_SIZE(BUF_SIZE * 2);
  START();

  char const * test_plain_string = "Printf with no arguments.\n";
  char const * test_substring = "'This is a substring.'";
  RegisterDump before;

  // Initialize x29 to the value of the stack pointer. We will use x29 as a
  // temporary stack pointer later, and initializing it in this way allows the
  // RegisterDump check to pass.
  __ Mov(x29, __ StackPointer());

  // Test simple integer arguments.
  __ Mov(x0, 1234);
  __ Mov(x1, 0x1234);

  // Test simple floating-point arguments.
  __ Fmov(d0, 1.234);

  // Test pointer (string) arguments.
  __ Mov(x2, reinterpret_cast<uintptr_t>(test_substring));

  // Test the maximum number of arguments, and sign extension.
  __ Mov(w3, 0xffffffff);
  __ Mov(w4, 0xffffffff);
  __ Mov(x5, 0xffffffffffffffff);
  __ Mov(x6, 0xffffffffffffffff);
  __ Fmov(s1, 1.234);
  __ Fmov(s2, 2.345);
  __ Fmov(d3, 3.456);
  __ Fmov(d4, 4.567);

  // Test printing callee-saved registers.
  __ Mov(x28, 0x123456789abcdef);
  __ Fmov(d10, 42.0);

  // Test with three arguments.
  __ Mov(x10, 3);
  __ Mov(x11, 40);
  __ Mov(x12, 500);

  // A single character.
  __ Mov(w13, 'x');

  // Check that we don't clobber any registers.
  before.Dump(&masm);

  __ Printf(test_plain_string);   // NOLINT(runtime/printf)
  __ Printf("x0: %" PRId64 ", x1: 0x%08" PRIx64 "\n", x0, x1);
  __ Printf("w5: %" PRId32 ", x5: %" PRId64"\n", w5, x5);
  __ Printf("d0: %f\n", d0);
  __ Printf("Test %%s: %s\n", x2);
  __ Printf("w3(uint32): %" PRIu32 "\nw4(int32): %" PRId32 "\n"
            "x5(uint64): %" PRIu64 "\nx6(int64): %" PRId64 "\n",
            w3, w4, x5, x6);
  __ Printf("%%f: %f\n%%g: %g\n%%e: %e\n%%E: %E\n", s1, s2, d3, d4);
  __ Printf("0x%" PRIx32 ", 0x%" PRIx64 "\n", w28, x28);
  __ Printf("%g\n", d10);
  __ Printf("%%%%%s%%%c%%\n", x2, w13);

  // Print the stack pointer (csp).
  DCHECK(csp.Is(__ StackPointer()));
  __ Printf("StackPointer(csp): 0x%016" PRIx64 ", 0x%08" PRIx32 "\n",
            __ StackPointer(), __ StackPointer().W());

  // Test with a different stack pointer.
  const Register old_stack_pointer = __ StackPointer();
  __ Mov(x29, old_stack_pointer);
  __ SetStackPointer(x29);
  // Print the stack pointer (not csp).
  __ Printf("StackPointer(not csp): 0x%016" PRIx64 ", 0x%08" PRIx32 "\n",
            __ StackPointer(), __ StackPointer().W());
  __ Mov(old_stack_pointer, __ StackPointer());
  __ SetStackPointer(old_stack_pointer);

  // Test with three arguments.
  __ Printf("3=%u, 4=%u, 5=%u\n", x10, x11, x12);

  // Mixed argument types.
  __ Printf("w3: %" PRIu32 ", s1: %f, x5: %" PRIu64 ", d3: %f\n",
            w3, s1, x5, d3);
  __ Printf("s1: %f, d3: %f, w3: %" PRId32 ", x5: %" PRId64 "\n",
            s1, d3, w3, x5);

  END();
  RUN();

  // We cannot easily test the output of the Printf sequences, and because
  // Printf preserves all registers by default, we can't look at the number of
  // bytes that were printed. However, the printf_no_preserve test should check
  // that, and here we just test that we didn't clobber any registers.
  CHECK_EQUAL_REGISTERS(before);

  TEARDOWN();
}


TEST(printf_no_preserve) {
  INIT_V8();
  SETUP();
  START();

  char const * test_plain_string = "Printf with no arguments.\n";
  char const * test_substring = "'This is a substring.'";

  __ PrintfNoPreserve(test_plain_string);
  __ Mov(x19, x0);

  // Test simple integer arguments.
  __ Mov(x0, 1234);
  __ Mov(x1, 0x1234);
  __ PrintfNoPreserve("x0: %" PRId64", x1: 0x%08" PRIx64 "\n", x0, x1);
  __ Mov(x20, x0);

  // Test simple floating-point arguments.
  __ Fmov(d0, 1.234);
  __ PrintfNoPreserve("d0: %f\n", d0);
  __ Mov(x21, x0);

  // Test pointer (string) arguments.
  __ Mov(x2, reinterpret_cast<uintptr_t>(test_substring));
  __ PrintfNoPreserve("Test %%s: %s\n", x2);
  __ Mov(x22, x0);

  // Test the maximum number of arguments, and sign extension.
  __ Mov(w3, 0xffffffff);
  __ Mov(w4, 0xffffffff);
  __ Mov(x5, 0xffffffffffffffff);
  __ Mov(x6, 0xffffffffffffffff);
  __ PrintfNoPreserve("w3(uint32): %" PRIu32 "\nw4(int32): %" PRId32 "\n"
                      "x5(uint64): %" PRIu64 "\nx6(int64): %" PRId64 "\n",
                      w3, w4, x5, x6);
  __ Mov(x23, x0);

  __ Fmov(s1, 1.234);
  __ Fmov(s2, 2.345);
  __ Fmov(d3, 3.456);
  __ Fmov(d4, 4.567);
  __ PrintfNoPreserve("%%f: %f\n%%g: %g\n%%e: %e\n%%E: %E\n", s1, s2, d3, d4);
  __ Mov(x24, x0);

  // Test printing callee-saved registers.
  __ Mov(x28, 0x123456789abcdef);
  __ PrintfNoPreserve("0x%" PRIx32 ", 0x%" PRIx64 "\n", w28, x28);
  __ Mov(x25, x0);

  __ Fmov(d10, 42.0);
  __ PrintfNoPreserve("%g\n", d10);
  __ Mov(x26, x0);

  // Test with a different stack pointer.
  const Register old_stack_pointer = __ StackPointer();
  __ Mov(x29, old_stack_pointer);
  __ SetStackPointer(x29);
  // Print the stack pointer (not csp).
  __ PrintfNoPreserve(
      "StackPointer(not csp): 0x%016" PRIx64 ", 0x%08" PRIx32 "\n",
      __ StackPointer(), __ StackPointer().W());
  __ Mov(x27, x0);
  __ Mov(old_stack_pointer, __ StackPointer());
  __ SetStackPointer(old_stack_pointer);

  // Test with three arguments.
  __ Mov(x3, 3);
  __ Mov(x4, 40);
  __ Mov(x5, 500);
  __ PrintfNoPreserve("3=%u, 4=%u, 5=%u\n", x3, x4, x5);
  __ Mov(x28, x0);

  // Mixed argument types.
  __ Mov(w3, 0xffffffff);
  __ Fmov(s1, 1.234);
  __ Mov(x5, 0xffffffffffffffff);
  __ Fmov(d3, 3.456);
  __ PrintfNoPreserve("w3: %" PRIu32 ", s1: %f, x5: %" PRIu64 ", d3: %f\n",
                      w3, s1, x5, d3);
  __ Mov(x29, x0);

  END();
  RUN();

  // We cannot easily test the exact output of the Printf sequences, but we can
  // use the return code to check that the string length was correct.

  // Printf with no arguments.
  CHECK_EQUAL_64(strlen(test_plain_string), x19);
  // x0: 1234, x1: 0x00001234
  CHECK_EQUAL_64(25, x20);
  // d0: 1.234000
  CHECK_EQUAL_64(13, x21);
  // Test %s: 'This is a substring.'
  CHECK_EQUAL_64(32, x22);
  // w3(uint32): 4294967295
  // w4(int32): -1
  // x5(uint64): 18446744073709551615
  // x6(int64): -1
  CHECK_EQUAL_64(23 + 14 + 33 + 14, x23);
  // %f: 1.234000
  // %g: 2.345
  // %e: 3.456000e+00
  // %E: 4.567000E+00
  CHECK_EQUAL_64(13 + 10 + 17 + 17, x24);
  // 0x89abcdef, 0x123456789abcdef
  CHECK_EQUAL_64(30, x25);
  // 42
  CHECK_EQUAL_64(3, x26);
  // StackPointer(not csp): 0x00007fb037ae2370, 0x37ae2370
  // Note: This is an example value, but the field width is fixed here so the
  // string length is still predictable.
  CHECK_EQUAL_64(54, x27);
  // 3=3, 4=40, 5=500
  CHECK_EQUAL_64(17, x28);
  // w3: 4294967295, s1: 1.234000, x5: 18446744073709551615, d3: 3.456000
  CHECK_EQUAL_64(69, x29);

  TEARDOWN();
}


// This is a V8-specific test.
static void CopyFieldsHelper(CPURegList temps) {
  static const uint64_t kLiteralBase = 0x0100001000100101UL;
  static const uint64_t src[] = {kLiteralBase * 1,
                                 kLiteralBase * 2,
                                 kLiteralBase * 3,
                                 kLiteralBase * 4,
                                 kLiteralBase * 5,
                                 kLiteralBase * 6,
                                 kLiteralBase * 7,
                                 kLiteralBase * 8,
                                 kLiteralBase * 9,
                                 kLiteralBase * 10,
                                 kLiteralBase * 11};
  static const uint64_t src_tagged =
      reinterpret_cast<uint64_t>(src) + kHeapObjectTag;

  static const unsigned kTestCount = sizeof(src) / sizeof(src[0]) + 1;
  uint64_t* dst[kTestCount];
  uint64_t dst_tagged[kTestCount];

  // The first test will be to copy 0 fields. The destination (and source)
  // should not be accessed in any way.
  dst[0] = NULL;
  dst_tagged[0] = kHeapObjectTag;

  // Allocate memory for each other test. Each test <n> will have <n> fields.
  // This is intended to exercise as many paths in CopyFields as possible.
  for (unsigned i = 1; i < kTestCount; i++) {
    dst[i] = new uint64_t[i];
    memset(dst[i], 0, i * sizeof(kLiteralBase));
    dst_tagged[i] = reinterpret_cast<uint64_t>(dst[i]) + kHeapObjectTag;
  }

  SETUP();
  START();

  __ Mov(x0, dst_tagged[0]);
  __ Mov(x1, 0);
  __ CopyFields(x0, x1, temps, 0);
  for (unsigned i = 1; i < kTestCount; i++) {
    __ Mov(x0, dst_tagged[i]);
    __ Mov(x1, src_tagged);
    __ CopyFields(x0, x1, temps, i);
  }

  END();
  RUN();
  TEARDOWN();

  for (unsigned i = 1; i < kTestCount; i++) {
    for (unsigned j = 0; j < i; j++) {
      CHECK(src[j] == dst[i][j]);
    }
    delete [] dst[i];
  }
}


// This is a V8-specific test.
TEST(copyfields) {
  INIT_V8();
  CopyFieldsHelper(CPURegList(x10));
  CopyFieldsHelper(CPURegList(x10, x11));
  CopyFieldsHelper(CPURegList(x10, x11, x12));
  CopyFieldsHelper(CPURegList(x10, x11, x12, x13));
}


TEST(blr_lr) {
  // A simple test to check that the simulator correcty handle "blr lr".
  INIT_V8();
  SETUP();

  START();
  Label target;
  Label end;

  __ Mov(x0, 0x0);
  __ Adr(lr, &target);

  __ Blr(lr);
  __ Mov(x0, 0xdeadbeef);
  __ B(&end);

  __ Bind(&target);
  __ Mov(x0, 0xc001c0de);

  __ Bind(&end);
  END();

  RUN();

  CHECK_EQUAL_64(0xc001c0de, x0);

  TEARDOWN();
}


TEST(barriers) {
  // Generate all supported barriers, this is just a smoke test
  INIT_V8();
  SETUP();

  START();

  // DMB
  __ Dmb(FullSystem, BarrierAll);
  __ Dmb(FullSystem, BarrierReads);
  __ Dmb(FullSystem, BarrierWrites);
  __ Dmb(FullSystem, BarrierOther);

  __ Dmb(InnerShareable, BarrierAll);
  __ Dmb(InnerShareable, BarrierReads);
  __ Dmb(InnerShareable, BarrierWrites);
  __ Dmb(InnerShareable, BarrierOther);

  __ Dmb(NonShareable, BarrierAll);
  __ Dmb(NonShareable, BarrierReads);
  __ Dmb(NonShareable, BarrierWrites);
  __ Dmb(NonShareable, BarrierOther);

  __ Dmb(OuterShareable, BarrierAll);
  __ Dmb(OuterShareable, BarrierReads);
  __ Dmb(OuterShareable, BarrierWrites);
  __ Dmb(OuterShareable, BarrierOther);

  // DSB
  __ Dsb(FullSystem, BarrierAll);
  __ Dsb(FullSystem, BarrierReads);
  __ Dsb(FullSystem, BarrierWrites);
  __ Dsb(FullSystem, BarrierOther);

  __ Dsb(InnerShareable, BarrierAll);
  __ Dsb(InnerShareable, BarrierReads);
  __ Dsb(InnerShareable, BarrierWrites);
  __ Dsb(InnerShareable, BarrierOther);

  __ Dsb(NonShareable, BarrierAll);
  __ Dsb(NonShareable, BarrierReads);
  __ Dsb(NonShareable, BarrierWrites);
  __ Dsb(NonShareable, BarrierOther);

  __ Dsb(OuterShareable, BarrierAll);
  __ Dsb(OuterShareable, BarrierReads);
  __ Dsb(OuterShareable, BarrierWrites);
  __ Dsb(OuterShareable, BarrierOther);

  // ISB
  __ Isb();

  END();

  RUN();

  TEARDOWN();
}


TEST(process_nan_double) {
  INIT_V8();
  // Make sure that NaN propagation works correctly.
  double sn = rawbits_to_double(0x7ff5555511111111);
  double qn = rawbits_to_double(0x7ffaaaaa11111111);
  DCHECK(IsSignallingNaN(sn));
  DCHECK(IsQuietNaN(qn));

  // The input NaNs after passing through ProcessNaN.
  double sn_proc = rawbits_to_double(0x7ffd555511111111);
  double qn_proc = qn;
  DCHECK(IsQuietNaN(sn_proc));
  DCHECK(IsQuietNaN(qn_proc));

  SETUP();
  START();

  // Execute a number of instructions which all use ProcessNaN, and check that
  // they all handle the NaN correctly.
  __ Fmov(d0, sn);
  __ Fmov(d10, qn);

  // Operations that always propagate NaNs unchanged, even signalling NaNs.
  //   - Signalling NaN
  __ Fmov(d1, d0);
  __ Fabs(d2, d0);
  __ Fneg(d3, d0);
  //   - Quiet NaN
  __ Fmov(d11, d10);
  __ Fabs(d12, d10);
  __ Fneg(d13, d10);

  // Operations that use ProcessNaN.
  //   - Signalling NaN
  __ Fsqrt(d4, d0);
  __ Frinta(d5, d0);
  __ Frintn(d6, d0);
  __ Frintz(d7, d0);
  //   - Quiet NaN
  __ Fsqrt(d14, d10);
  __ Frinta(d15, d10);
  __ Frintn(d16, d10);
  __ Frintz(d17, d10);

  // The behaviour of fcvt is checked in TEST(fcvt_sd).

  END();
  RUN();

  uint64_t qn_raw = double_to_rawbits(qn);
  uint64_t sn_raw = double_to_rawbits(sn);

  //   - Signalling NaN
  CHECK_EQUAL_FP64(sn, d1);
  CHECK_EQUAL_FP64(rawbits_to_double(sn_raw & ~kDSignMask), d2);
  CHECK_EQUAL_FP64(rawbits_to_double(sn_raw ^ kDSignMask), d3);
  //   - Quiet NaN
  CHECK_EQUAL_FP64(qn, d11);
  CHECK_EQUAL_FP64(rawbits_to_double(qn_raw & ~kDSignMask), d12);
  CHECK_EQUAL_FP64(rawbits_to_double(qn_raw ^ kDSignMask), d13);

  //   - Signalling NaN
  CHECK_EQUAL_FP64(sn_proc, d4);
  CHECK_EQUAL_FP64(sn_proc, d5);
  CHECK_EQUAL_FP64(sn_proc, d6);
  CHECK_EQUAL_FP64(sn_proc, d7);
  //   - Quiet NaN
  CHECK_EQUAL_FP64(qn_proc, d14);
  CHECK_EQUAL_FP64(qn_proc, d15);
  CHECK_EQUAL_FP64(qn_proc, d16);
  CHECK_EQUAL_FP64(qn_proc, d17);

  TEARDOWN();
}


TEST(process_nan_float) {
  INIT_V8();
  // Make sure that NaN propagation works correctly.
  float sn = rawbits_to_float(0x7f951111);
  float qn = rawbits_to_float(0x7fea1111);
  DCHECK(IsSignallingNaN(sn));
  DCHECK(IsQuietNaN(qn));

  // The input NaNs after passing through ProcessNaN.
  float sn_proc = rawbits_to_float(0x7fd51111);
  float qn_proc = qn;
  DCHECK(IsQuietNaN(sn_proc));
  DCHECK(IsQuietNaN(qn_proc));

  SETUP();
  START();

  // Execute a number of instructions which all use ProcessNaN, and check that
  // they all handle the NaN correctly.
  __ Fmov(s0, sn);
  __ Fmov(s10, qn);

  // Operations that always propagate NaNs unchanged, even signalling NaNs.
  //   - Signalling NaN
  __ Fmov(s1, s0);
  __ Fabs(s2, s0);
  __ Fneg(s3, s0);
  //   - Quiet NaN
  __ Fmov(s11, s10);
  __ Fabs(s12, s10);
  __ Fneg(s13, s10);

  // Operations that use ProcessNaN.
  //   - Signalling NaN
  __ Fsqrt(s4, s0);
  __ Frinta(s5, s0);
  __ Frintn(s6, s0);
  __ Frintz(s7, s0);
  //   - Quiet NaN
  __ Fsqrt(s14, s10);
  __ Frinta(s15, s10);
  __ Frintn(s16, s10);
  __ Frintz(s17, s10);

  // The behaviour of fcvt is checked in TEST(fcvt_sd).

  END();
  RUN();

  uint32_t qn_raw = float_to_rawbits(qn);
  uint32_t sn_raw = float_to_rawbits(sn);

  //   - Signalling NaN
  CHECK_EQUAL_FP32(sn, s1);
  CHECK_EQUAL_FP32(rawbits_to_float(sn_raw & ~kSSignMask), s2);
  CHECK_EQUAL_FP32(rawbits_to_float(sn_raw ^ kSSignMask), s3);
  //   - Quiet NaN
  CHECK_EQUAL_FP32(qn, s11);
  CHECK_EQUAL_FP32(rawbits_to_float(qn_raw & ~kSSignMask), s12);
  CHECK_EQUAL_FP32(rawbits_to_float(qn_raw ^ kSSignMask), s13);

  //   - Signalling NaN
  CHECK_EQUAL_FP32(sn_proc, s4);
  CHECK_EQUAL_FP32(sn_proc, s5);
  CHECK_EQUAL_FP32(sn_proc, s6);
  CHECK_EQUAL_FP32(sn_proc, s7);
  //   - Quiet NaN
  CHECK_EQUAL_FP32(qn_proc, s14);
  CHECK_EQUAL_FP32(qn_proc, s15);
  CHECK_EQUAL_FP32(qn_proc, s16);
  CHECK_EQUAL_FP32(qn_proc, s17);

  TEARDOWN();
}


static void ProcessNaNsHelper(double n, double m, double expected) {
  DCHECK(std::isnan(n) || std::isnan(m));
  DCHECK(std::isnan(expected));

  SETUP();
  START();

  // Execute a number of instructions which all use ProcessNaNs, and check that
  // they all propagate NaNs correctly.
  __ Fmov(d0, n);
  __ Fmov(d1, m);

  __ Fadd(d2, d0, d1);
  __ Fsub(d3, d0, d1);
  __ Fmul(d4, d0, d1);
  __ Fdiv(d5, d0, d1);
  __ Fmax(d6, d0, d1);
  __ Fmin(d7, d0, d1);

  END();
  RUN();

  CHECK_EQUAL_FP64(expected, d2);
  CHECK_EQUAL_FP64(expected, d3);
  CHECK_EQUAL_FP64(expected, d4);
  CHECK_EQUAL_FP64(expected, d5);
  CHECK_EQUAL_FP64(expected, d6);
  CHECK_EQUAL_FP64(expected, d7);

  TEARDOWN();
}


TEST(process_nans_double) {
  INIT_V8();
  // Make sure that NaN propagation works correctly.
  double sn = rawbits_to_double(0x7ff5555511111111);
  double sm = rawbits_to_double(0x7ff5555522222222);
  double qn = rawbits_to_double(0x7ffaaaaa11111111);
  double qm = rawbits_to_double(0x7ffaaaaa22222222);
  DCHECK(IsSignallingNaN(sn));
  DCHECK(IsSignallingNaN(sm));
  DCHECK(IsQuietNaN(qn));
  DCHECK(IsQuietNaN(qm));

  // The input NaNs after passing through ProcessNaN.
  double sn_proc = rawbits_to_double(0x7ffd555511111111);
  double sm_proc = rawbits_to_double(0x7ffd555522222222);
  double qn_proc = qn;
  double qm_proc = qm;
  DCHECK(IsQuietNaN(sn_proc));
  DCHECK(IsQuietNaN(sm_proc));
  DCHECK(IsQuietNaN(qn_proc));
  DCHECK(IsQuietNaN(qm_proc));

  // Quiet NaNs are propagated.
  ProcessNaNsHelper(qn, 0, qn_proc);
  ProcessNaNsHelper(0, qm, qm_proc);
  ProcessNaNsHelper(qn, qm, qn_proc);

  // Signalling NaNs are propagated, and made quiet.
  ProcessNaNsHelper(sn, 0, sn_proc);
  ProcessNaNsHelper(0, sm, sm_proc);
  ProcessNaNsHelper(sn, sm, sn_proc);

  // Signalling NaNs take precedence over quiet NaNs.
  ProcessNaNsHelper(sn, qm, sn_proc);
  ProcessNaNsHelper(qn, sm, sm_proc);
  ProcessNaNsHelper(sn, sm, sn_proc);
}


static void ProcessNaNsHelper(float n, float m, float expected) {
  DCHECK(std::isnan(n) || std::isnan(m));
  DCHECK(std::isnan(expected));

  SETUP();
  START();

  // Execute a number of instructions which all use ProcessNaNs, and check that
  // they all propagate NaNs correctly.
  __ Fmov(s0, n);
  __ Fmov(s1, m);

  __ Fadd(s2, s0, s1);
  __ Fsub(s3, s0, s1);
  __ Fmul(s4, s0, s1);
  __ Fdiv(s5, s0, s1);
  __ Fmax(s6, s0, s1);
  __ Fmin(s7, s0, s1);

  END();
  RUN();

  CHECK_EQUAL_FP32(expected, s2);
  CHECK_EQUAL_FP32(expected, s3);
  CHECK_EQUAL_FP32(expected, s4);
  CHECK_EQUAL_FP32(expected, s5);
  CHECK_EQUAL_FP32(expected, s6);
  CHECK_EQUAL_FP32(expected, s7);

  TEARDOWN();
}


TEST(process_nans_float) {
  INIT_V8();
  // Make sure that NaN propagation works correctly.
  float sn = rawbits_to_float(0x7f951111);
  float sm = rawbits_to_float(0x7f952222);
  float qn = rawbits_to_float(0x7fea1111);
  float qm = rawbits_to_float(0x7fea2222);
  DCHECK(IsSignallingNaN(sn));
  DCHECK(IsSignallingNaN(sm));
  DCHECK(IsQuietNaN(qn));
  DCHECK(IsQuietNaN(qm));

  // The input NaNs after passing through ProcessNaN.
  float sn_proc = rawbits_to_float(0x7fd51111);
  float sm_proc = rawbits_to_float(0x7fd52222);
  float qn_proc = qn;
  float qm_proc = qm;
  DCHECK(IsQuietNaN(sn_proc));
  DCHECK(IsQuietNaN(sm_proc));
  DCHECK(IsQuietNaN(qn_proc));
  DCHECK(IsQuietNaN(qm_proc));

  // Quiet NaNs are propagated.
  ProcessNaNsHelper(qn, 0, qn_proc);
  ProcessNaNsHelper(0, qm, qm_proc);
  ProcessNaNsHelper(qn, qm, qn_proc);

  // Signalling NaNs are propagated, and made quiet.
  ProcessNaNsHelper(sn, 0, sn_proc);
  ProcessNaNsHelper(0, sm, sm_proc);
  ProcessNaNsHelper(sn, sm, sn_proc);

  // Signalling NaNs take precedence over quiet NaNs.
  ProcessNaNsHelper(sn, qm, sn_proc);
  ProcessNaNsHelper(qn, sm, sm_proc);
  ProcessNaNsHelper(sn, sm, sn_proc);
}


static void DefaultNaNHelper(float n, float m, float a) {
  DCHECK(std::isnan(n) || std::isnan(m) || std::isnan(a));

  bool test_1op = std::isnan(n);
  bool test_2op = std::isnan(n) || std::isnan(m);

  SETUP();
  START();

  // Enable Default-NaN mode in the FPCR.
  __ Mrs(x0, FPCR);
  __ Orr(x1, x0, DN_mask);
  __ Msr(FPCR, x1);

  // Execute a number of instructions which all use ProcessNaNs, and check that
  // they all produce the default NaN.
  __ Fmov(s0, n);
  __ Fmov(s1, m);
  __ Fmov(s2, a);

  if (test_1op) {
    // Operations that always propagate NaNs unchanged, even signalling NaNs.
    __ Fmov(s10, s0);
    __ Fabs(s11, s0);
    __ Fneg(s12, s0);

    // Operations that use ProcessNaN.
    __ Fsqrt(s13, s0);
    __ Frinta(s14, s0);
    __ Frintn(s15, s0);
    __ Frintz(s16, s0);

    // Fcvt usually has special NaN handling, but it respects default-NaN mode.
    __ Fcvt(d17, s0);
  }

  if (test_2op) {
    __ Fadd(s18, s0, s1);
    __ Fsub(s19, s0, s1);
    __ Fmul(s20, s0, s1);
    __ Fdiv(s21, s0, s1);
    __ Fmax(s22, s0, s1);
    __ Fmin(s23, s0, s1);
  }

  __ Fmadd(s24, s0, s1, s2);
  __ Fmsub(s25, s0, s1, s2);
  __ Fnmadd(s26, s0, s1, s2);
  __ Fnmsub(s27, s0, s1, s2);

  // Restore FPCR.
  __ Msr(FPCR, x0);

  END();
  RUN();

  if (test_1op) {
    uint32_t n_raw = float_to_rawbits(n);
    CHECK_EQUAL_FP32(n, s10);
    CHECK_EQUAL_FP32(rawbits_to_float(n_raw & ~kSSignMask), s11);
    CHECK_EQUAL_FP32(rawbits_to_float(n_raw ^ kSSignMask), s12);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s13);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s14);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s15);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s16);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d17);
  }

  if (test_2op) {
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s18);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s19);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s20);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s21);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s22);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s23);
  }

  CHECK_EQUAL_FP32(kFP32DefaultNaN, s24);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s25);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s26);
  CHECK_EQUAL_FP32(kFP32DefaultNaN, s27);

  TEARDOWN();
}


TEST(default_nan_float) {
  INIT_V8();
  float sn = rawbits_to_float(0x7f951111);
  float sm = rawbits_to_float(0x7f952222);
  float sa = rawbits_to_float(0x7f95aaaa);
  float qn = rawbits_to_float(0x7fea1111);
  float qm = rawbits_to_float(0x7fea2222);
  float qa = rawbits_to_float(0x7feaaaaa);
  DCHECK(IsSignallingNaN(sn));
  DCHECK(IsSignallingNaN(sm));
  DCHECK(IsSignallingNaN(sa));
  DCHECK(IsQuietNaN(qn));
  DCHECK(IsQuietNaN(qm));
  DCHECK(IsQuietNaN(qa));

  //   - Signalling NaNs
  DefaultNaNHelper(sn, 0.0f, 0.0f);
  DefaultNaNHelper(0.0f, sm, 0.0f);
  DefaultNaNHelper(0.0f, 0.0f, sa);
  DefaultNaNHelper(sn, sm, 0.0f);
  DefaultNaNHelper(0.0f, sm, sa);
  DefaultNaNHelper(sn, 0.0f, sa);
  DefaultNaNHelper(sn, sm, sa);
  //   - Quiet NaNs
  DefaultNaNHelper(qn, 0.0f, 0.0f);
  DefaultNaNHelper(0.0f, qm, 0.0f);
  DefaultNaNHelper(0.0f, 0.0f, qa);
  DefaultNaNHelper(qn, qm, 0.0f);
  DefaultNaNHelper(0.0f, qm, qa);
  DefaultNaNHelper(qn, 0.0f, qa);
  DefaultNaNHelper(qn, qm, qa);
  //   - Mixed NaNs
  DefaultNaNHelper(qn, sm, sa);
  DefaultNaNHelper(sn, qm, sa);
  DefaultNaNHelper(sn, sm, qa);
  DefaultNaNHelper(qn, qm, sa);
  DefaultNaNHelper(sn, qm, qa);
  DefaultNaNHelper(qn, sm, qa);
  DefaultNaNHelper(qn, qm, qa);
}


static void DefaultNaNHelper(double n, double m, double a) {
  DCHECK(std::isnan(n) || std::isnan(m) || std::isnan(a));

  bool test_1op = std::isnan(n);
  bool test_2op = std::isnan(n) || std::isnan(m);

  SETUP();
  START();

  // Enable Default-NaN mode in the FPCR.
  __ Mrs(x0, FPCR);
  __ Orr(x1, x0, DN_mask);
  __ Msr(FPCR, x1);

  // Execute a number of instructions which all use ProcessNaNs, and check that
  // they all produce the default NaN.
  __ Fmov(d0, n);
  __ Fmov(d1, m);
  __ Fmov(d2, a);

  if (test_1op) {
    // Operations that always propagate NaNs unchanged, even signalling NaNs.
    __ Fmov(d10, d0);
    __ Fabs(d11, d0);
    __ Fneg(d12, d0);

    // Operations that use ProcessNaN.
    __ Fsqrt(d13, d0);
    __ Frinta(d14, d0);
    __ Frintn(d15, d0);
    __ Frintz(d16, d0);

    // Fcvt usually has special NaN handling, but it respects default-NaN mode.
    __ Fcvt(s17, d0);
  }

  if (test_2op) {
    __ Fadd(d18, d0, d1);
    __ Fsub(d19, d0, d1);
    __ Fmul(d20, d0, d1);
    __ Fdiv(d21, d0, d1);
    __ Fmax(d22, d0, d1);
    __ Fmin(d23, d0, d1);
  }

  __ Fmadd(d24, d0, d1, d2);
  __ Fmsub(d25, d0, d1, d2);
  __ Fnmadd(d26, d0, d1, d2);
  __ Fnmsub(d27, d0, d1, d2);

  // Restore FPCR.
  __ Msr(FPCR, x0);

  END();
  RUN();

  if (test_1op) {
    uint64_t n_raw = double_to_rawbits(n);
    CHECK_EQUAL_FP64(n, d10);
    CHECK_EQUAL_FP64(rawbits_to_double(n_raw & ~kDSignMask), d11);
    CHECK_EQUAL_FP64(rawbits_to_double(n_raw ^ kDSignMask), d12);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d13);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d14);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d15);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d16);
    CHECK_EQUAL_FP32(kFP32DefaultNaN, s17);
  }

  if (test_2op) {
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d18);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d19);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d20);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d21);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d22);
    CHECK_EQUAL_FP64(kFP64DefaultNaN, d23);
  }

  CHECK_EQUAL_FP64(kFP64DefaultNaN, d24);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d25);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d26);
  CHECK_EQUAL_FP64(kFP64DefaultNaN, d27);

  TEARDOWN();
}


TEST(default_nan_double) {
  INIT_V8();
  double sn = rawbits_to_double(0x7ff5555511111111);
  double sm = rawbits_to_double(0x7ff5555522222222);
  double sa = rawbits_to_double(0x7ff55555aaaaaaaa);
  double qn = rawbits_to_double(0x7ffaaaaa11111111);
  double qm = rawbits_to_double(0x7ffaaaaa22222222);
  double qa = rawbits_to_double(0x7ffaaaaaaaaaaaaa);
  DCHECK(IsSignallingNaN(sn));
  DCHECK(IsSignallingNaN(sm));
  DCHECK(IsSignallingNaN(sa));
  DCHECK(IsQuietNaN(qn));
  DCHECK(IsQuietNaN(qm));
  DCHECK(IsQuietNaN(qa));

  //   - Signalling NaNs
  DefaultNaNHelper(sn, 0.0, 0.0);
  DefaultNaNHelper(0.0, sm, 0.0);
  DefaultNaNHelper(0.0, 0.0, sa);
  DefaultNaNHelper(sn, sm, 0.0);
  DefaultNaNHelper(0.0, sm, sa);
  DefaultNaNHelper(sn, 0.0, sa);
  DefaultNaNHelper(sn, sm, sa);
  //   - Quiet NaNs
  DefaultNaNHelper(qn, 0.0, 0.0);
  DefaultNaNHelper(0.0, qm, 0.0);
  DefaultNaNHelper(0.0, 0.0, qa);
  DefaultNaNHelper(qn, qm, 0.0);
  DefaultNaNHelper(0.0, qm, qa);
  DefaultNaNHelper(qn, 0.0, qa);
  DefaultNaNHelper(qn, qm, qa);
  //   - Mixed NaNs
  DefaultNaNHelper(qn, sm, sa);
  DefaultNaNHelper(sn, qm, sa);
  DefaultNaNHelper(sn, sm, qa);
  DefaultNaNHelper(qn, qm, sa);
  DefaultNaNHelper(sn, qm, qa);
  DefaultNaNHelper(qn, sm, qa);
  DefaultNaNHelper(qn, qm, qa);
}


TEST(call_no_relocation) {
  Address call_start;
  Address return_address;

  INIT_V8();
  SETUP();

  START();

  Label function;
  Label test;

  __ B(&test);

  __ Bind(&function);
  __ Mov(x0, 0x1);
  __ Ret();

  __ Bind(&test);
  __ Mov(x0, 0x0);
  __ Push(lr, xzr);
  {
    Assembler::BlockConstPoolScope scope(&masm);
    call_start = buf + __ pc_offset();
    __ Call(buf + function.pos(), RelocInfo::NONE64);
    return_address = buf + __ pc_offset();
  }
  __ Pop(xzr, lr);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);

  // The return_address_from_call_start function doesn't currently encounter any
  // non-relocatable sequences, so we check it here to make sure it works.
  // TODO(jbramley): Once Crankshaft is complete, decide if we need to support
  // non-relocatable calls at all.
  CHECK(return_address ==
        Assembler::return_address_from_call_start(call_start));

  TEARDOWN();
}


static void AbsHelperX(int64_t value) {
  int64_t expected;

  SETUP();
  START();

  Label fail;
  Label done;

  __ Mov(x0, 0);
  __ Mov(x1, value);

  if (value != kXMinInt) {
    expected = labs(value);

    Label next;
    // The result is representable.
    __ Abs(x10, x1);
    __ Abs(x11, x1, &fail);
    __ Abs(x12, x1, &fail, &next);
    __ Bind(&next);
    __ Abs(x13, x1, NULL, &done);
  } else {
    // labs is undefined for kXMinInt but our implementation in the
    // MacroAssembler will return kXMinInt in such a case.
    expected = kXMinInt;

    Label next;
    // The result is not representable.
    __ Abs(x10, x1);
    __ Abs(x11, x1, NULL, &fail);
    __ Abs(x12, x1, &next, &fail);
    __ Bind(&next);
    __ Abs(x13, x1, &done);
  }

  __ Bind(&fail);
  __ Mov(x0, -1);

  __ Bind(&done);

  END();
  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(value, x1);
  CHECK_EQUAL_64(expected, x10);
  CHECK_EQUAL_64(expected, x11);
  CHECK_EQUAL_64(expected, x12);
  CHECK_EQUAL_64(expected, x13);

  TEARDOWN();
}


static void AbsHelperW(int32_t value) {
  int32_t expected;

  SETUP();
  START();

  Label fail;
  Label done;

  __ Mov(w0, 0);
  // TODO(jbramley): The cast is needed to avoid a sign-extension bug in VIXL.
  // Once it is fixed, we should remove the cast.
  __ Mov(w1, static_cast<uint32_t>(value));

  if (value != kWMinInt) {
    expected = abs(value);

    Label next;
    // The result is representable.
    __ Abs(w10, w1);
    __ Abs(w11, w1, &fail);
    __ Abs(w12, w1, &fail, &next);
    __ Bind(&next);
    __ Abs(w13, w1, NULL, &done);
  } else {
    // abs is undefined for kWMinInt but our implementation in the
    // MacroAssembler will return kWMinInt in such a case.
    expected = kWMinInt;

    Label next;
    // The result is not representable.
    __ Abs(w10, w1);
    __ Abs(w11, w1, NULL, &fail);
    __ Abs(w12, w1, &next, &fail);
    __ Bind(&next);
    __ Abs(w13, w1, &done);
  }

  __ Bind(&fail);
  __ Mov(w0, -1);

  __ Bind(&done);

  END();
  RUN();

  CHECK_EQUAL_32(0, w0);
  CHECK_EQUAL_32(value, w1);
  CHECK_EQUAL_32(expected, w10);
  CHECK_EQUAL_32(expected, w11);
  CHECK_EQUAL_32(expected, w12);
  CHECK_EQUAL_32(expected, w13);

  TEARDOWN();
}


TEST(abs) {
  INIT_V8();
  AbsHelperX(0);
  AbsHelperX(42);
  AbsHelperX(-42);
  AbsHelperX(kXMinInt);
  AbsHelperX(kXMaxInt);

  AbsHelperW(0);
  AbsHelperW(42);
  AbsHelperW(-42);
  AbsHelperW(kWMinInt);
  AbsHelperW(kWMaxInt);
}


TEST(pool_size) {
  INIT_V8();
  SETUP();

  // This test does not execute any code. It only tests that the size of the
  // pools is read correctly from the RelocInfo.

  Label exit;
  __ b(&exit);

  const unsigned constant_pool_size = 312;
  const unsigned veneer_pool_size = 184;

  __ RecordConstPool(constant_pool_size);
  for (unsigned i = 0; i < constant_pool_size / 4; ++i) {
    __ dc32(0);
  }

  __ RecordVeneerPool(masm.pc_offset(), veneer_pool_size);
  for (unsigned i = 0; i < veneer_pool_size / kInstructionSize; ++i) {
    __ nop();
  }

  __ bind(&exit);

  HandleScope handle_scope(isolate);
  CodeDesc desc;
  masm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(desc, 0, masm.CodeObject());

  unsigned pool_count = 0;
  int pool_mask = RelocInfo::ModeMask(RelocInfo::CONST_POOL) |
                  RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  for (RelocIterator it(*code, pool_mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (RelocInfo::IsConstPool(info->rmode())) {
      DCHECK(info->data() == constant_pool_size);
      ++pool_count;
    }
    if (RelocInfo::IsVeneerPool(info->rmode())) {
      DCHECK(info->data() == veneer_pool_size);
      ++pool_count;
    }
  }

  DCHECK(pool_count == 2);

  TEARDOWN();
}
