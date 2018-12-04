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

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/decoder-arm64-inl.h"
#include "src/arm64/disasm-arm64.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/arm64/simulator-arm64.h"
#include "src/arm64/utils-arm64.h"
#include "src/base/platform/platform.h"
#include "src/base/utils/random-number-generator.h"
#include "src/heap/factory.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-utils-arm64.h"
#include "test/common/assembler-tester.h"

namespace v8 {
namespace internal {

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
//   CHECK_EQUAL_64(0x1234, core.xreg(0) & 0xFFFF);

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
#define SETUP_SIZE(buf_size)                                   \
  Isolate* isolate = CcTest::i_isolate();                      \
  HandleScope scope(isolate);                                  \
  CHECK_NOT_NULL(isolate);                                     \
  byte* buf = new byte[buf_size];                              \
  MacroAssembler masm(isolate, buf, buf_size,                  \
                      v8::internal::CodeObjectRequired::kYes); \
  Decoder<DispatchingDecoderVisitor>* decoder =                \
      new Decoder<DispatchingDecoderVisitor>();                \
  Simulator simulator(decoder);                                \
  PrintDisassembler* pdis = nullptr;                           \
  RegisterDump core;                                           \
  if (i::FLAG_trace_sim) {                                     \
    pdis = new PrintDisassembler(stdout);                      \
    decoder->PrependVisitor(pdis);                             \
  }

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
  __ PushCalleeSavedRegisters();                                               \
  __ Debug("Start test.", __LINE__, TRACE_ENABLE | LOG_ALL);

#define START()                                                                \
  RESET();                                                                     \
  START_AFTER_RESET();

#define RUN()                                                                  \
  simulator.RunFrom(reinterpret_cast<Instruction*>(buf))

#define END()                                               \
  __ Debug("End test.", __LINE__, TRACE_DISABLE | LOG_ALL); \
  core.Dump(&masm);                                         \
  __ PopCalleeSavedRegisters();                             \
  __ Ret();                                                 \
  __ GetCode(masm.isolate(), nullptr);

#define TEARDOWN()                                                             \
  delete pdis;                                                                 \
  delete[] buf;

#else  // ifdef USE_SIMULATOR.
// Run the test on real hardware or models.
#define SETUP_SIZE(buf_size)                                     \
  Isolate* isolate = CcTest::i_isolate();                        \
  HandleScope scope(isolate);                                    \
  CHECK_NOT_NULL(isolate);                                       \
  size_t allocated;                                              \
  byte* buf = AllocateAssemblerBuffer(&allocated, buf_size);     \
  MacroAssembler masm(isolate, buf, static_cast<int>(allocated), \
                      v8::internal::CodeObjectRequired::kYes);   \
  RegisterDump core;

#define RESET()                                                \
  MakeAssemblerBufferWritable(buf, allocated);                 \
  __ Reset();                                                  \
  /* Reset the machine state (like simulator.ResetState()). */ \
  __ Msr(NZCV, xzr);                                           \
  __ Msr(FPCR, xzr);

#define START_AFTER_RESET()                                                    \
  __ PushCalleeSavedRegisters();

#define START()                                                                \
  RESET();                                                                     \
  START_AFTER_RESET();

#define RUN()                                              \
  MakeAssemblerBufferExecutable(buf, allocated);           \
  {                                                        \
    void (*test_function)(void);                           \
    memcpy(&test_function, &buf, sizeof(buf));             \
    test_function();                                       \
  }

#define END()                   \
  core.Dump(&masm);             \
  __ PopCalleeSavedRegisters(); \
  __ Ret();                     \
  __ GetCode(masm.isolate(), nullptr);

#define TEARDOWN() \
  CHECK(v8::internal::FreePages(GetPlatformPageAllocator(), buf, allocated));

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

// Expected values for 128-bit comparisons are passed as two 64-bit values,
// where expected_h (high) is <127:64> and expected_l (low) is <63:0>.
#define CHECK_EQUAL_128(expected_h, expected_l, result) \
  CHECK(Equal128(expected_h, expected_l, &core, result))

#ifdef DEBUG
#define CHECK_CONSTANT_POOL_SIZE(expected) \
  CHECK_EQ(expected, __ GetConstantPoolEntriesSizeForTesting())
#else
#define CHECK_CONSTANT_POOL_SIZE(expected) ((void)0)
#endif


TEST(stack_ops) {
  INIT_V8();
  SETUP();

  START();
  // save sp.
  __ Mov(x29, sp);

  // Set the sp to a known value.
  __ Mov(x16, 0x1000);
  __ Mov(sp, x16);
  __ Mov(x0, sp);

  // Add immediate to the sp, and move the result to a normal register.
  __ Add(sp, sp, Operand(0x50));
  __ Mov(x1, sp);

  // Add extended to the sp, and move the result to a normal register.
  __ Mov(x17, 0xFFF);
  __ Add(sp, sp, Operand(x17, SXTB));
  __ Mov(x2, sp);

  // Create an sp using a logical instruction, and move to normal register.
  __ Orr(sp, xzr, Operand(0x1FFF));
  __ Mov(x3, sp);

  // Write wsp using a logical instruction.
  __ Orr(wsp, wzr, Operand(0xFFFFFFF8L));
  __ Mov(x4, sp);

  // Write sp, and read back wsp.
  __ Orr(sp, xzr, Operand(0xFFFFFFF8L));
  __ Mov(w5, wsp);

  //  restore sp.
  __ Mov(sp, x29);
  END();

  RUN();

  CHECK_EQUAL_64(0x1000, x0);
  CHECK_EQUAL_64(0x1050, x1);
  CHECK_EQUAL_64(0x104F, x2);
  CHECK_EQUAL_64(0x1FFF, x3);
  CHECK_EQUAL_64(0xFFFFFFF8, x4);
  CHECK_EQUAL_64(0xFFFFFFF8, x5);

  TEARDOWN();
}


TEST(mvn) {
  INIT_V8();
  SETUP();

  START();
  __ Mvn(w0, 0xFFF);
  __ Mvn(x1, 0xFFF);
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

  CHECK_EQUAL_64(0xFFFFF000, x0);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFF000UL, x1);
  CHECK_EQUAL_64(0x00001FFF, x2);
  CHECK_EQUAL_64(0x0000000000003FFFUL, x3);
  CHECK_EQUAL_64(0xE00001FF, x4);
  CHECK_EQUAL_64(0xF0000000000000FFUL, x5);
  CHECK_EQUAL_64(0x00000001, x6);
  CHECK_EQUAL_64(0x0, x7);
  CHECK_EQUAL_64(0x7FF80000, x8);
  CHECK_EQUAL_64(0x3FFC000000000000UL, x9);
  CHECK_EQUAL_64(0xFFFFFF00, x10);
  CHECK_EQUAL_64(0x0000000000000001UL, x11);
  CHECK_EQUAL_64(0xFFFF8003, x12);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF0007UL, x13);
  CHECK_EQUAL_64(0xFFFFFFFFFFFE000FUL, x14);
  CHECK_EQUAL_64(0xFFFFFFFFFFFE000FUL, x15);

  TEARDOWN();
}


TEST(mov) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xFFFFFFFFFFFFFFFFL);
  __ Mov(x1, 0xFFFFFFFFFFFFFFFFL);
  __ Mov(x2, 0xFFFFFFFFFFFFFFFFL);
  __ Mov(x3, 0xFFFFFFFFFFFFFFFFL);

  __ Mov(x0, 0x0123456789ABCDEFL);

  __ movz(x1, 0xABCDL << 16);
  __ movk(x2, 0xABCDL << 32);
  __ movn(x3, 0xABCDL << 48);

  __ Mov(x4, 0x0123456789ABCDEFL);
  __ Mov(x5, x4);

  __ Mov(w6, -1);

  // Test that moves back to the same register have the desired effect. This
  // is a no-op for X registers, and a truncation for W registers.
  __ Mov(x7, 0x0123456789ABCDEFL);
  __ Mov(x7, x7);
  __ Mov(x8, 0x0123456789ABCDEFL);
  __ Mov(w8, w8);
  __ Mov(x9, 0x0123456789ABCDEFL);
  __ Mov(x9, Operand(x9));
  __ Mov(x10, 0x0123456789ABCDEFL);
  __ Mov(w10, Operand(w10));

  __ Mov(w11, 0xFFF);
  __ Mov(x12, 0xFFF);
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

  CHECK_EQUAL_64(0x0123456789ABCDEFL, x0);
  CHECK_EQUAL_64(0x00000000ABCD0000L, x1);
  CHECK_EQUAL_64(0xFFFFABCDFFFFFFFFL, x2);
  CHECK_EQUAL_64(0x5432FFFFFFFFFFFFL, x3);
  CHECK_EQUAL_64(x4, x5);
  CHECK_EQUAL_32(-1, w6);
  CHECK_EQUAL_64(0x0123456789ABCDEFL, x7);
  CHECK_EQUAL_32(0x89ABCDEFL, w8);
  CHECK_EQUAL_64(0x0123456789ABCDEFL, x9);
  CHECK_EQUAL_32(0x89ABCDEFL, w10);
  CHECK_EQUAL_64(0x00000FFF, x11);
  CHECK_EQUAL_64(0x0000000000000FFFUL, x12);
  CHECK_EQUAL_64(0x00001FFE, x13);
  CHECK_EQUAL_64(0x0000000000003FFCUL, x14);
  CHECK_EQUAL_64(0x000001FF, x15);
  CHECK_EQUAL_64(0x00000000000000FFUL, x18);
  CHECK_EQUAL_64(0x00000001, x19);
  CHECK_EQUAL_64(0x0, x20);
  CHECK_EQUAL_64(0x7FF80000, x21);
  CHECK_EQUAL_64(0x3FFC000000000000UL, x22);
  CHECK_EQUAL_64(0x000000FE, x23);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFCUL, x24);
  CHECK_EQUAL_64(0x00007FF8, x25);
  CHECK_EQUAL_64(0x000000000000FFF0UL, x26);
  CHECK_EQUAL_64(0x000000000001FFE0UL, x27);

  TEARDOWN();
}


TEST(mov_imm_w) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(w0, 0xFFFFFFFFL);
  __ Mov(w1, 0xFFFF1234L);
  __ Mov(w2, 0x1234FFFFL);
  __ Mov(w3, 0x00000000L);
  __ Mov(w4, 0x00001234L);
  __ Mov(w5, 0x12340000L);
  __ Mov(w6, 0x12345678L);
  __ Mov(w7, (int32_t)0x80000000);
  __ Mov(w8, (int32_t)0xFFFF0000);
  __ Mov(w9, kWMinInt);
  END();

  RUN();

  CHECK_EQUAL_64(0xFFFFFFFFL, x0);
  CHECK_EQUAL_64(0xFFFF1234L, x1);
  CHECK_EQUAL_64(0x1234FFFFL, x2);
  CHECK_EQUAL_64(0x00000000L, x3);
  CHECK_EQUAL_64(0x00001234L, x4);
  CHECK_EQUAL_64(0x12340000L, x5);
  CHECK_EQUAL_64(0x12345678L, x6);
  CHECK_EQUAL_64(0x80000000L, x7);
  CHECK_EQUAL_64(0xFFFF0000L, x8);
  CHECK_EQUAL_32(kWMinInt, w9);

  TEARDOWN();
}


TEST(mov_imm_x) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xFFFFFFFFFFFFFFFFL);
  __ Mov(x1, 0xFFFFFFFFFFFF1234L);
  __ Mov(x2, 0xFFFFFFFF12345678L);
  __ Mov(x3, 0xFFFF1234FFFF5678L);
  __ Mov(x4, 0x1234FFFFFFFF5678L);
  __ Mov(x5, 0x1234FFFF5678FFFFL);
  __ Mov(x6, 0x12345678FFFFFFFFL);
  __ Mov(x7, 0x1234FFFFFFFFFFFFL);
  __ Mov(x8, 0x123456789ABCFFFFL);
  __ Mov(x9, 0x12345678FFFF9ABCL);
  __ Mov(x10, 0x1234FFFF56789ABCL);
  __ Mov(x11, 0xFFFF123456789ABCL);
  __ Mov(x12, 0x0000000000000000L);
  __ Mov(x13, 0x0000000000001234L);
  __ Mov(x14, 0x0000000012345678L);
  __ Mov(x15, 0x0000123400005678L);
  __ Mov(x18, 0x1234000000005678L);
  __ Mov(x19, 0x1234000056780000L);
  __ Mov(x20, 0x1234567800000000L);
  __ Mov(x21, 0x1234000000000000L);
  __ Mov(x22, 0x123456789ABC0000L);
  __ Mov(x23, 0x1234567800009ABCL);
  __ Mov(x24, 0x1234000056789ABCL);
  __ Mov(x25, 0x0000123456789ABCL);
  __ Mov(x26, 0x123456789ABCDEF0L);
  __ Mov(x27, 0xFFFF000000000001L);
  __ Mov(x28, 0x8000FFFF00000000L);
  END();

  RUN();

  CHECK_EQUAL_64(0xFFFFFFFFFFFF1234L, x1);
  CHECK_EQUAL_64(0xFFFFFFFF12345678L, x2);
  CHECK_EQUAL_64(0xFFFF1234FFFF5678L, x3);
  CHECK_EQUAL_64(0x1234FFFFFFFF5678L, x4);
  CHECK_EQUAL_64(0x1234FFFF5678FFFFL, x5);
  CHECK_EQUAL_64(0x12345678FFFFFFFFL, x6);
  CHECK_EQUAL_64(0x1234FFFFFFFFFFFFL, x7);
  CHECK_EQUAL_64(0x123456789ABCFFFFL, x8);
  CHECK_EQUAL_64(0x12345678FFFF9ABCL, x9);
  CHECK_EQUAL_64(0x1234FFFF56789ABCL, x10);
  CHECK_EQUAL_64(0xFFFF123456789ABCL, x11);
  CHECK_EQUAL_64(0x0000000000000000L, x12);
  CHECK_EQUAL_64(0x0000000000001234L, x13);
  CHECK_EQUAL_64(0x0000000012345678L, x14);
  CHECK_EQUAL_64(0x0000123400005678L, x15);
  CHECK_EQUAL_64(0x1234000000005678L, x18);
  CHECK_EQUAL_64(0x1234000056780000L, x19);
  CHECK_EQUAL_64(0x1234567800000000L, x20);
  CHECK_EQUAL_64(0x1234000000000000L, x21);
  CHECK_EQUAL_64(0x123456789ABC0000L, x22);
  CHECK_EQUAL_64(0x1234567800009ABCL, x23);
  CHECK_EQUAL_64(0x1234000056789ABCL, x24);
  CHECK_EQUAL_64(0x0000123456789ABCL, x25);
  CHECK_EQUAL_64(0x123456789ABCDEF0L, x26);
  CHECK_EQUAL_64(0xFFFF000000000001L, x27);
  CHECK_EQUAL_64(0x8000FFFF00000000L, x28);

  TEARDOWN();
}


TEST(orr) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xF0F0);
  __ Mov(x1, 0xF00000FF);

  __ Orr(x2, x0, Operand(x1));
  __ Orr(w3, w0, Operand(w1, LSL, 28));
  __ Orr(x4, x0, Operand(x1, LSL, 32));
  __ Orr(x5, x0, Operand(x1, LSR, 4));
  __ Orr(w6, w0, Operand(w1, ASR, 4));
  __ Orr(x7, x0, Operand(x1, ASR, 4));
  __ Orr(w8, w0, Operand(w1, ROR, 12));
  __ Orr(x9, x0, Operand(x1, ROR, 12));
  __ Orr(w10, w0, Operand(0xF));
  __ Orr(x11, x0, Operand(0xF0000000F0000000L));
  END();

  RUN();

  CHECK_EQUAL_64(0xF000F0FF, x2);
  CHECK_EQUAL_64(0xF000F0F0, x3);
  CHECK_EQUAL_64(0xF00000FF0000F0F0L, x4);
  CHECK_EQUAL_64(0x0F00F0FF, x5);
  CHECK_EQUAL_64(0xFF00F0FF, x6);
  CHECK_EQUAL_64(0x0F00F0FF, x7);
  CHECK_EQUAL_64(0x0FFFF0F0, x8);
  CHECK_EQUAL_64(0x0FF00000000FF0F0L, x9);
  CHECK_EQUAL_64(0xF0FF, x10);
  CHECK_EQUAL_64(0xF0000000F000F0F0L, x11);

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
  CHECK_EQUAL_64(0x00000000FFFFFF81UL, x10);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF0101UL, x11);
  CHECK_EQUAL_64(0xFFFFFFFE00020201UL, x12);
  CHECK_EQUAL_64(0x0000000400040401UL, x13);

  TEARDOWN();
}


TEST(bitwise_wide_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0xF0F0F0F0F0F0F0F0UL);

  __ Orr(x10, x0, Operand(0x1234567890ABCDEFUL));
  __ Orr(w11, w1, Operand(0x90ABCDEF));

  __ Orr(w12, w0, kWMinInt);
  __ Eor(w13, w0, kWMinInt);
  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(0xF0F0F0F0F0F0F0F0UL, x1);
  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x10);
  CHECK_EQUAL_64(0xF0FBFDFFUL, x11);
  CHECK_EQUAL_32(kWMinInt, w12);
  CHECK_EQUAL_32(kWMinInt, w13);

  TEARDOWN();
}


TEST(orn) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xF0F0);
  __ Mov(x1, 0xF00000FF);

  __ Orn(x2, x0, Operand(x1));
  __ Orn(w3, w0, Operand(w1, LSL, 4));
  __ Orn(x4, x0, Operand(x1, LSL, 4));
  __ Orn(x5, x0, Operand(x1, LSR, 1));
  __ Orn(w6, w0, Operand(w1, ASR, 1));
  __ Orn(x7, x0, Operand(x1, ASR, 1));
  __ Orn(w8, w0, Operand(w1, ROR, 16));
  __ Orn(x9, x0, Operand(x1, ROR, 16));
  __ Orn(w10, w0, Operand(0xFFFF));
  __ Orn(x11, x0, Operand(0xFFFF0000FFFFL));
  END();

  RUN();

  CHECK_EQUAL_64(0xFFFFFFFF0FFFFFF0L, x2);
  CHECK_EQUAL_64(0xFFFFF0FF, x3);
  CHECK_EQUAL_64(0xFFFFFFF0FFFFF0FFL, x4);
  CHECK_EQUAL_64(0xFFFFFFFF87FFFFF0L, x5);
  CHECK_EQUAL_64(0x07FFFFF0, x6);
  CHECK_EQUAL_64(0xFFFFFFFF87FFFFF0L, x7);
  CHECK_EQUAL_64(0xFF00FFFF, x8);
  CHECK_EQUAL_64(0xFF00FFFFFFFFFFFFL, x9);
  CHECK_EQUAL_64(0xFFFFF0F0, x10);
  CHECK_EQUAL_64(0xFFFF0000FFFFF0F0L, x11);

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

  CHECK_EQUAL_64(0xFFFFFF7F, x6);
  CHECK_EQUAL_64(0xFFFFFFFFFFFEFEFDUL, x7);
  CHECK_EQUAL_64(0xFFFDFDFB, x8);
  CHECK_EQUAL_64(0xFFFFFFFBFFFBFBF7UL, x9);
  CHECK_EQUAL_64(0x0000007F, x10);
  CHECK_EQUAL_64(0x0000FEFD, x11);
  CHECK_EQUAL_64(0x00000001FFFDFDFBUL, x12);
  CHECK_EQUAL_64(0xFFFFFFFBFFFBFBF7UL, x13);

  TEARDOWN();
}


TEST(and_) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xFFF0);
  __ Mov(x1, 0xF00000FF);

  __ And(x2, x0, Operand(x1));
  __ And(w3, w0, Operand(w1, LSL, 4));
  __ And(x4, x0, Operand(x1, LSL, 4));
  __ And(x5, x0, Operand(x1, LSR, 1));
  __ And(w6, w0, Operand(w1, ASR, 20));
  __ And(x7, x0, Operand(x1, ASR, 20));
  __ And(w8, w0, Operand(w1, ROR, 28));
  __ And(x9, x0, Operand(x1, ROR, 28));
  __ And(w10, w0, Operand(0xFF00));
  __ And(x11, x0, Operand(0xFF));
  END();

  RUN();

  CHECK_EQUAL_64(0x000000F0, x2);
  CHECK_EQUAL_64(0x00000FF0, x3);
  CHECK_EQUAL_64(0x00000FF0, x4);
  CHECK_EQUAL_64(0x00000070, x5);
  CHECK_EQUAL_64(0x0000FF00, x6);
  CHECK_EQUAL_64(0x00000F00, x7);
  CHECK_EQUAL_64(0x00000FF0, x8);
  CHECK_EQUAL_64(0x00000000, x9);
  CHECK_EQUAL_64(0x0000FF00, x10);
  CHECK_EQUAL_64(0x000000F0, x11);

  TEARDOWN();
}


TEST(and_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xFFFFFFFFFFFFFFFFUL);
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
  CHECK_EQUAL_64(0xFFFFFF81, x10);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF0102UL, x11);
  CHECK_EQUAL_64(0xFFFFFFFE00020204UL, x12);
  CHECK_EQUAL_64(0x0000000400040408UL, x13);

  TEARDOWN();
}


TEST(ands) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0xF00000FF);
  __ Ands(w0, w1, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0xF00000FF, x0);

  START();
  __ Mov(x0, 0xFFF0);
  __ Mov(x1, 0xF00000FF);
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
  __ Mov(x0, 0xFFF0);
  __ Ands(w0, w0, Operand(0xF));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  START();
  __ Mov(x0, 0xFF000000);
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
  __ Mov(x0, 0xFFF0);
  __ Mov(x1, 0xF00000FF);

  __ Bic(x2, x0, Operand(x1));
  __ Bic(w3, w0, Operand(w1, LSL, 4));
  __ Bic(x4, x0, Operand(x1, LSL, 4));
  __ Bic(x5, x0, Operand(x1, LSR, 1));
  __ Bic(w6, w0, Operand(w1, ASR, 20));
  __ Bic(x7, x0, Operand(x1, ASR, 20));
  __ Bic(w8, w0, Operand(w1, ROR, 28));
  __ Bic(x9, x0, Operand(x1, ROR, 24));
  __ Bic(x10, x0, Operand(0x1F));
  __ Bic(x11, x0, Operand(0x100));

  // Test bic into sp when the constant cannot be encoded in the immediate
  // field.
  // Use x20 to preserve sp. We check for the result via x21 because the
  // test infrastructure requires that sp be restored to its original value.
  __ Mov(x20, sp);
  __ Mov(x0, 0xFFFFFF);
  __ Bic(sp, x0, Operand(0xABCDEF));
  __ Mov(x21, sp);
  __ Mov(sp, x20);
  END();

  RUN();

  CHECK_EQUAL_64(0x0000FF00, x2);
  CHECK_EQUAL_64(0x0000F000, x3);
  CHECK_EQUAL_64(0x0000F000, x4);
  CHECK_EQUAL_64(0x0000FF80, x5);
  CHECK_EQUAL_64(0x000000F0, x6);
  CHECK_EQUAL_64(0x0000F0F0, x7);
  CHECK_EQUAL_64(0x0000F000, x8);
  CHECK_EQUAL_64(0x0000FF00, x9);
  CHECK_EQUAL_64(0x0000FFE0, x10);
  CHECK_EQUAL_64(0x0000FEF0, x11);

  CHECK_EQUAL_64(0x543210, x21);

  TEARDOWN();
}


TEST(bic_extend) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xFFFFFFFFFFFFFFFFUL);
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

  CHECK_EQUAL_64(0xFFFFFF7E, x6);
  CHECK_EQUAL_64(0xFFFFFFFFFFFEFEFDUL, x7);
  CHECK_EQUAL_64(0xFFFDFDFB, x8);
  CHECK_EQUAL_64(0xFFFFFFFBFFFBFBF7UL, x9);
  CHECK_EQUAL_64(0x0000007E, x10);
  CHECK_EQUAL_64(0x0000FEFD, x11);
  CHECK_EQUAL_64(0x00000001FFFDFDFBUL, x12);
  CHECK_EQUAL_64(0xFFFFFFFBFFFBFBF7UL, x13);

  TEARDOWN();
}


TEST(bics) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0xFFFF);
  __ Bics(w0, w1, Operand(w1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZFlag);
  CHECK_EQUAL_64(0x00000000, x0);

  START();
  __ Mov(x0, 0xFFFFFFFF);
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
  __ Mov(x0, 0xFFFFFFFFFFFFFFFFL);
  __ Bics(x0, x0, Operand(0x7FFFFFFFFFFFFFFFL));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NFlag);
  CHECK_EQUAL_64(0x8000000000000000L, x0);

  START();
  __ Mov(w0, 0xFFFF0000);
  __ Bics(w0, w0, Operand(0xFFFFFFF0));
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
  __ Mov(x0, 0xFFF0);
  __ Mov(x1, 0xF00000FF);

  __ Eor(x2, x0, Operand(x1));
  __ Eor(w3, w0, Operand(w1, LSL, 4));
  __ Eor(x4, x0, Operand(x1, LSL, 4));
  __ Eor(x5, x0, Operand(x1, LSR, 1));
  __ Eor(w6, w0, Operand(w1, ASR, 20));
  __ Eor(x7, x0, Operand(x1, ASR, 20));
  __ Eor(w8, w0, Operand(w1, ROR, 28));
  __ Eor(x9, x0, Operand(x1, ROR, 28));
  __ Eor(w10, w0, Operand(0xFF00FF00));
  __ Eor(x11, x0, Operand(0xFF00FF00FF00FF00L));
  END();

  RUN();

  CHECK_EQUAL_64(0xF000FF0F, x2);
  CHECK_EQUAL_64(0x0000F000, x3);
  CHECK_EQUAL_64(0x0000000F0000F000L, x4);
  CHECK_EQUAL_64(0x7800FF8F, x5);
  CHECK_EQUAL_64(0xFFFF00F0, x6);
  CHECK_EQUAL_64(0x0000F0F0, x7);
  CHECK_EQUAL_64(0x0000F00F, x8);
  CHECK_EQUAL_64(0x00000FF00000FFFFL, x9);
  CHECK_EQUAL_64(0xFF0000F0, x10);
  CHECK_EQUAL_64(0xFF00FF00FF0000F0L, x11);

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
  CHECK_EQUAL_64(0xEEEEEE90, x10);
  CHECK_EQUAL_64(0xEEEEEEEEEEEE1013UL, x11);
  CHECK_EQUAL_64(0xEEEEEEEF11131315UL, x12);
  CHECK_EQUAL_64(0x1111111511151519UL, x13);

  TEARDOWN();
}


TEST(eon) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xFFF0);
  __ Mov(x1, 0xF00000FF);

  __ Eon(x2, x0, Operand(x1));
  __ Eon(w3, w0, Operand(w1, LSL, 4));
  __ Eon(x4, x0, Operand(x1, LSL, 4));
  __ Eon(x5, x0, Operand(x1, LSR, 1));
  __ Eon(w6, w0, Operand(w1, ASR, 20));
  __ Eon(x7, x0, Operand(x1, ASR, 20));
  __ Eon(w8, w0, Operand(w1, ROR, 28));
  __ Eon(x9, x0, Operand(x1, ROR, 28));
  __ Eon(w10, w0, Operand(0x03C003C0));
  __ Eon(x11, x0, Operand(0x0000100000001000L));
  END();

  RUN();

  CHECK_EQUAL_64(0xFFFFFFFF0FFF00F0L, x2);
  CHECK_EQUAL_64(0xFFFF0FFF, x3);
  CHECK_EQUAL_64(0xFFFFFFF0FFFF0FFFL, x4);
  CHECK_EQUAL_64(0xFFFFFFFF87FF0070L, x5);
  CHECK_EQUAL_64(0x0000FF0F, x6);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF0F0FL, x7);
  CHECK_EQUAL_64(0xFFFF0FF0, x8);
  CHECK_EQUAL_64(0xFFFFF00FFFFF0000L, x9);
  CHECK_EQUAL_64(0xFC3F03CF, x10);
  CHECK_EQUAL_64(0xFFFFEFFFFFFF100FL, x11);

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

  CHECK_EQUAL_64(0xEEEEEE6F, x6);
  CHECK_EQUAL_64(0xEEEEEEEEEEEFEFECUL, x7);
  CHECK_EQUAL_64(0xEEECECEA, x8);
  CHECK_EQUAL_64(0xEEEEEEEAEEEAEAE6UL, x9);
  CHECK_EQUAL_64(0x1111116F, x10);
  CHECK_EQUAL_64(0x111111111111EFECUL, x11);
  CHECK_EQUAL_64(0x11111110EEECECEAUL, x12);
  CHECK_EQUAL_64(0xEEEEEEEAEEEAEAE6UL, x13);

  TEARDOWN();
}


TEST(mul) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Mov(x17, 1);
  __ Mov(x18, 0xFFFFFFFF);
  __ Mov(x19, 0xFFFFFFFFFFFFFFFFUL);

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
  CHECK_EQUAL_64(0xFFFFFFFF, x2);
  CHECK_EQUAL_64(1, x3);
  CHECK_EQUAL_64(0, x4);
  CHECK_EQUAL_64(0xFFFFFFFF, x5);
  CHECK_EQUAL_64(0xFFFFFFFF00000001UL, x6);
  CHECK_EQUAL_64(1, x7);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0, x12);
  CHECK_EQUAL_64(1, x13);
  CHECK_EQUAL_64(0xFFFFFFFF, x14);
  CHECK_EQUAL_64(0, x20);
  CHECK_EQUAL_64(0xFFFFFFFF00000001UL, x21);
  CHECK_EQUAL_64(0xFFFFFFFF, x22);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x23);

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
  SmullHelper(0xFFFFFFFF80000000, 0x80000000, 1);
  SmullHelper(0x0000000080000000, 0x00010000, 0x00008000);
}


TEST(madd) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 0);
  __ Mov(x17, 1);
  __ Mov(x18, 0xFFFFFFFF);
  __ Mov(x19, 0xFFFFFFFFFFFFFFFFUL);

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
  CHECK_EQUAL_64(0xFFFFFFFF, x2);
  CHECK_EQUAL_64(0xFFFFFFFF, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0, x6);
  CHECK_EQUAL_64(0xFFFFFFFF, x7);
  CHECK_EQUAL_64(0xFFFFFFFE, x8);
  CHECK_EQUAL_64(2, x9);
  CHECK_EQUAL_64(0, x10);
  CHECK_EQUAL_64(0, x11);

  CHECK_EQUAL_64(0, x12);
  CHECK_EQUAL_64(1, x13);
  CHECK_EQUAL_64(0xFFFFFFFF, x14);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFF, x15);
  CHECK_EQUAL_64(1, x20);
  CHECK_EQUAL_64(0x100000000UL, x21);
  CHECK_EQUAL_64(0, x22);
  CHECK_EQUAL_64(0xFFFFFFFF, x23);
  CHECK_EQUAL_64(0x1FFFFFFFE, x24);
  CHECK_EQUAL_64(0xFFFFFFFE00000002UL, x25);
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
  __ Mov(x18, 0xFFFFFFFF);
  __ Mov(x19, 0xFFFFFFFFFFFFFFFFUL);

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
  CHECK_EQUAL_64(0xFFFFFFFF, x2);
  CHECK_EQUAL_64(0xFFFFFFFF, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(0xFFFFFFFE, x5);
  CHECK_EQUAL_64(0xFFFFFFFE, x6);
  CHECK_EQUAL_64(1, x7);
  CHECK_EQUAL_64(0, x8);
  CHECK_EQUAL_64(0, x9);
  CHECK_EQUAL_64(0xFFFFFFFE, x10);
  CHECK_EQUAL_64(0xFFFFFFFE, x11);

  CHECK_EQUAL_64(0, x12);
  CHECK_EQUAL_64(1, x13);
  CHECK_EQUAL_64(0xFFFFFFFF, x14);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x15);
  CHECK_EQUAL_64(1, x20);
  CHECK_EQUAL_64(0xFFFFFFFEUL, x21);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFEUL, x22);
  CHECK_EQUAL_64(0xFFFFFFFF00000001UL, x23);
  CHECK_EQUAL_64(0, x24);
  CHECK_EQUAL_64(0x200000000UL, x25);
  CHECK_EQUAL_64(0x1FFFFFFFEUL, x26);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFEUL, x27);

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
  __ Mov(x24, 0x0123456789ABCDEFL);
  __ Mov(x25, 0x0000000200000000L);
  __ Mov(x26, 0x8000000000000000UL);
  __ Mov(x27, 0xFFFFFFFFFFFFFFFFUL);
  __ Mov(x28, 0x5555555555555555UL);
  __ Mov(x29, 0xAAAAAAAAAAAAAAAAUL);

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
  CHECK_EQUAL_64(0x02468ACF, x4);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x5);
  CHECK_EQUAL_64(0x4000000000000000UL, x6);
  CHECK_EQUAL_64(0, x7);
  CHECK_EQUAL_64(0, x8);
  CHECK_EQUAL_64(0x1C71C71C71C71C71UL, x9);
  CHECK_EQUAL_64(0xE38E38E38E38E38EUL, x10);
  CHECK_EQUAL_64(0x1C71C71C71C71C72UL, x11);

  TEARDOWN();
}


TEST(smaddl_umaddl) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x17, 1);
  __ Mov(x18, 0xFFFFFFFF);
  __ Mov(x19, 0xFFFFFFFFFFFFFFFFUL);
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
  CHECK_EQUAL_64(0xFFFFFFFE00000005UL, x14);
  CHECK_EQUAL_64(0xFFFFFFFE00000005UL, x15);
  CHECK_EQUAL_64(0x1, x22);

  TEARDOWN();
}


TEST(smsubl_umsubl) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x17, 1);
  __ Mov(x18, 0xFFFFFFFF);
  __ Mov(x19, 0xFFFFFFFFFFFFFFFFUL);
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
  CHECK_EQUAL_64(0x1FFFFFFFFUL, x12);
  CHECK_EQUAL_64(0xFFFFFFFF00000005UL, x13);
  CHECK_EQUAL_64(0x200000003UL, x14);
  CHECK_EQUAL_64(0x200000003UL, x15);
  CHECK_EQUAL_64(0x3FFFFFFFFUL, x22);

  TEARDOWN();
}


TEST(div) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x16, 1);
  __ Mov(x17, 0xFFFFFFFF);
  __ Mov(x18, 0xFFFFFFFFFFFFFFFFUL);
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
  CHECK_EQUAL_64(0xFFFFFFFF, x1);
  CHECK_EQUAL_64(1, x2);
  CHECK_EQUAL_64(0xFFFFFFFF, x3);
  CHECK_EQUAL_64(1, x4);
  CHECK_EQUAL_64(1, x5);
  CHECK_EQUAL_64(0, x6);
  CHECK_EQUAL_64(1, x7);
  CHECK_EQUAL_64(0, x8);
  CHECK_EQUAL_64(0xFFFFFFFF00000001UL, x9);
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
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x28);
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
  __ Mov(x24, 0xFEDCBA9876543210UL);
  __ Rbit(w0, w24);
  __ Rbit(x1, x24);
  __ Rev16(w2, w24);
  __ Rev16(x3, x24);
  __ Rev(w4, w24);
  __ Rev32(x5, x24);
  __ Rev(x6, x24);
  END();

  RUN();

  CHECK_EQUAL_64(0x084C2A6E, x0);
  CHECK_EQUAL_64(0x084C2A6E195D3B7FUL, x1);
  CHECK_EQUAL_64(0x54761032, x2);
  CHECK_EQUAL_64(0xDCFE98BA54761032UL, x3);
  CHECK_EQUAL_64(0x10325476, x4);
  CHECK_EQUAL_64(0x98BADCFE10325476UL, x5);
  CHECK_EQUAL_64(0x1032547698BADCFEUL, x6);

  TEARDOWN();
}


TEST(clz_cls) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x24, 0x0008000000800000UL);
  __ Mov(x25, 0xFF800000FFF80000UL);
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
  SETUP_SIZE(max_range + 1000 * kInstrSize);

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

  for (int i = 0; i < max_range / kInstrSize + 1; ++i) {
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

  CHECK_EQUAL_64(0xF, x0);

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

  CHECK_EQUAL_64(core.xreg(3) + kInstrSize, x0);
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

  __ Mov(x18, 0xFFFFFFFF00000000UL);

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
  __ Mov(x16, 0xAAAAAAAAAAAAAAAAUL);

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

  SETUP_SIZE(max_range + 1000 * kInstrSize);

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
  for (int i = 0; i < max_range / kInstrSize + 1; ++i) {
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
  CHECK_GE(7 * kInstrSize, __ SizeOfCodeGeneratedSince(&test_tbz));

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

  SETUP_SIZE(max_range + 1000 * kInstrSize);

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
  for (int i = 0; i < max_range / kInstrSize + 1; ++i) {
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

  SETUP_SIZE(max_range + 1000 * kInstrSize);

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
  for (int i = 0; i < max_range / kInstrSize + 1; ++i) {
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

  SETUP_SIZE(3 * inter_range + 1000 * kInstrSize);

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

  for (int i = 0; i < inter_range / kInstrSize; ++i) {
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

  for (int i = 0; i < inter_range / kInstrSize; ++i) {
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

  for (int i = 0; i < inter_range / kInstrSize; ++i) {
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

  uint64_t src[2] = {0xFEDCBA9876543210UL, 0x0123456789ABCDEFUL};
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
  CHECK_EQUAL_64(0xFEDCBA98, x1);
  CHECK_EQUAL_64(0xFEDCBA9800000000UL, dst[1]);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, x2);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, dst[2]);
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
  memset(src, 0xAA, 8192 * sizeof(src[0]));
  memset(dst, 0xAA, 8192 * sizeof(dst[0]));
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

  uint64_t src[2] = {0xFEDCBA9876543210UL, 0x0123456789ABCDEFUL};
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

  CHECK_EQUAL_64(0xFEDCBA98, x0);
  CHECK_EQUAL_64(0xFEDCBA9800000000UL, dst[1]);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, x1);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, dst[2]);
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

  uint64_t src[2] = {0xFEDCBA9876543210UL, 0x0123456789ABCDEFUL};
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

  CHECK_EQUAL_64(0xFEDCBA98, x0);
  CHECK_EQUAL_64(0xFEDCBA9800000000UL, dst[1]);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, x1);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, dst[2]);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, x2);
  CHECK_EQUAL_64(0x0123456789ABCDEFUL, dst[4]);
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

  uint32_t src[2] = {0x80008080, 0x7FFF7F7F};
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

  CHECK_EQUAL_64(0xFFFFFF80, x0);
  CHECK_EQUAL_64(0x0000007F, x1);
  CHECK_EQUAL_64(0xFFFF8080, x2);
  CHECK_EQUAL_64(0x00007F7F, x3);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFF80UL, x4);
  CHECK_EQUAL_64(0x000000000000007FUL, x5);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF8080UL, x6);
  CHECK_EQUAL_64(0x0000000000007F7FUL, x7);
  CHECK_EQUAL_64(0xFFFFFFFF80008080UL, x8);
  CHECK_EQUAL_64(0x000000007FFF7F7FUL, x9);

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
  __ Mov(x27, 0xFFFFFFFC);  // 32-bit -4.
  __ Mov(x28, 0xFFFFFFFE);  // 32-bit -2.
  __ Mov(x29, 0xFFFFFFFF);  // 32-bit -1.

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

TEST(load_store_b) {
  INIT_V8();
  SETUP();

  uint8_t src[3] = {0x12, 0x23, 0x34};
  uint8_t dst[3] = {0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base);
  __ Mov(x20, dst_base);
  __ Mov(x21, src_base);
  __ Mov(x22, dst_base);
  __ Ldr(b0, MemOperand(x17, sizeof(src[0])));
  __ Str(b0, MemOperand(x18, sizeof(dst[0]), PostIndex));
  __ Ldr(b1, MemOperand(x19, sizeof(src[0]), PostIndex));
  __ Str(b1, MemOperand(x20, 2 * sizeof(dst[0]), PreIndex));
  __ Ldr(b2, MemOperand(x21, 2 * sizeof(src[0]), PreIndex));
  __ Str(b2, MemOperand(x22, sizeof(dst[0])));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x23, q0);
  CHECK_EQUAL_64(0x23, dst[0]);
  CHECK_EQUAL_128(0, 0x12, q1);
  CHECK_EQUAL_64(0x12, dst[2]);
  CHECK_EQUAL_128(0, 0x34, q2);
  CHECK_EQUAL_64(0x34, dst[1]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base + sizeof(dst[0]), x18);
  CHECK_EQUAL_64(src_base + sizeof(src[0]), x19);
  CHECK_EQUAL_64(dst_base + 2 * sizeof(dst[0]), x20);
  CHECK_EQUAL_64(src_base + 2 * sizeof(src[0]), x21);
  CHECK_EQUAL_64(dst_base, x22);

  TEARDOWN();
}

TEST(load_store_h) {
  INIT_V8();
  SETUP();

  uint16_t src[3] = {0x1234, 0x2345, 0x3456};
  uint16_t dst[3] = {0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base);
  __ Mov(x20, dst_base);
  __ Mov(x21, src_base);
  __ Mov(x22, dst_base);
  __ Ldr(h0, MemOperand(x17, sizeof(src[0])));
  __ Str(h0, MemOperand(x18, sizeof(dst[0]), PostIndex));
  __ Ldr(h1, MemOperand(x19, sizeof(src[0]), PostIndex));
  __ Str(h1, MemOperand(x20, 2 * sizeof(dst[0]), PreIndex));
  __ Ldr(h2, MemOperand(x21, 2 * sizeof(src[0]), PreIndex));
  __ Str(h2, MemOperand(x22, sizeof(dst[0])));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x2345, q0);
  CHECK_EQUAL_64(0x2345, dst[0]);
  CHECK_EQUAL_128(0, 0x1234, q1);
  CHECK_EQUAL_64(0x1234, dst[2]);
  CHECK_EQUAL_128(0, 0x3456, q2);
  CHECK_EQUAL_64(0x3456, dst[1]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base + sizeof(dst[0]), x18);
  CHECK_EQUAL_64(src_base + sizeof(src[0]), x19);
  CHECK_EQUAL_64(dst_base + 2 * sizeof(dst[0]), x20);
  CHECK_EQUAL_64(src_base + 2 * sizeof(src[0]), x21);
  CHECK_EQUAL_64(dst_base, x22);

  TEARDOWN();
}

TEST(load_store_q) {
  INIT_V8();
  SETUP();

  uint8_t src[48] = {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x01, 0x23,
                     0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x21, 0x43, 0x65, 0x87,
                     0xA9, 0xCB, 0xED, 0x0F, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
                     0xDE, 0xF0, 0x24, 0x46, 0x68, 0x8A, 0xAC, 0xCE, 0xE0, 0x02,
                     0x42, 0x64, 0x86, 0xA8, 0xCA, 0xEC, 0x0E, 0x20};

  uint64_t dst[6] = {0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, dst_base);
  __ Mov(x19, src_base);
  __ Mov(x20, dst_base);
  __ Mov(x21, src_base);
  __ Mov(x22, dst_base);
  __ Ldr(q0, MemOperand(x17, 16));
  __ Str(q0, MemOperand(x18, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Str(q1, MemOperand(x20, 32, PreIndex));
  __ Ldr(q2, MemOperand(x21, 32, PreIndex));
  __ Str(q2, MemOperand(x22, 16));
  END();

  RUN();

  CHECK_EQUAL_128(0xF0DEBC9A78563412, 0x0FEDCBA987654321, q0);
  CHECK_EQUAL_64(0x0FEDCBA987654321, dst[0]);
  CHECK_EQUAL_64(0xF0DEBC9A78563412, dst[1]);
  CHECK_EQUAL_128(0xEFCDAB8967452301, 0xFEDCBA9876543210, q1);
  CHECK_EQUAL_64(0xFEDCBA9876543210, dst[4]);
  CHECK_EQUAL_64(0xEFCDAB8967452301, dst[5]);
  CHECK_EQUAL_128(0x200EECCAA8866442, 0x02E0CEAC8A684624, q2);
  CHECK_EQUAL_64(0x02E0CEAC8A684624, dst[2]);
  CHECK_EQUAL_64(0x200EECCAA8866442, dst[3]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base + 16, x18);
  CHECK_EQUAL_64(src_base + 16, x19);
  CHECK_EQUAL_64(dst_base + 32, x20);
  CHECK_EQUAL_64(src_base + 32, x21);
  CHECK_EQUAL_64(dst_base, x22);

  TEARDOWN();
}

TEST(neon_ld1_d) {
  INIT_V8();
  SETUP();

  uint8_t src[32 + 5];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ldr(q2, MemOperand(x17));  // Initialise top 64-bits of Q register.
  __ Ld1(v2.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v3.V8B(), v4.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v5.V4H(), v6.V4H(), v7.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v16.V2S(), v17.V2S(), v18.V2S(), v19.V2S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v30.V2S(), v31.V2S(), v0.V2S(), v1.V2S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v20.V1D(), v21.V1D(), v22.V1D(), v23.V1D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0706050403020100, q2);
  CHECK_EQUAL_128(0, 0x0807060504030201, q3);
  CHECK_EQUAL_128(0, 0x100F0E0D0C0B0A09, q4);
  CHECK_EQUAL_128(0, 0x0908070605040302, q5);
  CHECK_EQUAL_128(0, 0x11100F0E0D0C0B0A, q6);
  CHECK_EQUAL_128(0, 0x1918171615141312, q7);
  CHECK_EQUAL_128(0, 0x0A09080706050403, q16);
  CHECK_EQUAL_128(0, 0x1211100F0E0D0C0B, q17);
  CHECK_EQUAL_128(0, 0x1A19181716151413, q18);
  CHECK_EQUAL_128(0, 0x2221201F1E1D1C1B, q19);
  CHECK_EQUAL_128(0, 0x0B0A090807060504, q30);
  CHECK_EQUAL_128(0, 0x131211100F0E0D0C, q31);
  CHECK_EQUAL_128(0, 0x1B1A191817161514, q0);
  CHECK_EQUAL_128(0, 0x232221201F1E1D1C, q1);
  CHECK_EQUAL_128(0, 0x0C0B0A0908070605, q20);
  CHECK_EQUAL_128(0, 0x14131211100F0E0D, q21);
  CHECK_EQUAL_128(0, 0x1C1B1A1918171615, q22);
  CHECK_EQUAL_128(0, 0x24232221201F1E1D, q23);

  TEARDOWN();
}

TEST(neon_ld1_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[32 + 5];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, src_base + 5);
  __ Mov(x23, 1);
  __ Ldr(q2, MemOperand(x17));  // Initialise top 64-bits of Q register.
  __ Ld1(v2.V8B(), MemOperand(x17, x23, PostIndex));
  __ Ld1(v3.V8B(), v4.V8B(), MemOperand(x18, 16, PostIndex));
  __ Ld1(v5.V4H(), v6.V4H(), v7.V4H(), MemOperand(x19, 24, PostIndex));
  __ Ld1(v16.V2S(), v17.V2S(), v18.V2S(), v19.V2S(),
         MemOperand(x20, 32, PostIndex));
  __ Ld1(v30.V2S(), v31.V2S(), v0.V2S(), v1.V2S(),
         MemOperand(x21, 32, PostIndex));
  __ Ld1(v20.V1D(), v21.V1D(), v22.V1D(), v23.V1D(),
         MemOperand(x22, 32, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0706050403020100, q2);
  CHECK_EQUAL_128(0, 0x0807060504030201, q3);
  CHECK_EQUAL_128(0, 0x100F0E0D0C0B0A09, q4);
  CHECK_EQUAL_128(0, 0x0908070605040302, q5);
  CHECK_EQUAL_128(0, 0x11100F0E0D0C0B0A, q6);
  CHECK_EQUAL_128(0, 0x1918171615141312, q7);
  CHECK_EQUAL_128(0, 0x0A09080706050403, q16);
  CHECK_EQUAL_128(0, 0x1211100F0E0D0C0B, q17);
  CHECK_EQUAL_128(0, 0x1A19181716151413, q18);
  CHECK_EQUAL_128(0, 0x2221201F1E1D1C1B, q19);
  CHECK_EQUAL_128(0, 0x0B0A090807060504, q30);
  CHECK_EQUAL_128(0, 0x131211100F0E0D0C, q31);
  CHECK_EQUAL_128(0, 0x1B1A191817161514, q0);
  CHECK_EQUAL_128(0, 0x232221201F1E1D1C, q1);
  CHECK_EQUAL_128(0, 0x0C0B0A0908070605, q20);
  CHECK_EQUAL_128(0, 0x14131211100F0E0D, q21);
  CHECK_EQUAL_128(0, 0x1C1B1A1918171615, q22);
  CHECK_EQUAL_128(0, 0x24232221201F1E1D, q23);
  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 16, x18);
  CHECK_EQUAL_64(src_base + 2 + 24, x19);
  CHECK_EQUAL_64(src_base + 3 + 32, x20);
  CHECK_EQUAL_64(src_base + 4 + 32, x21);
  CHECK_EQUAL_64(src_base + 5 + 32, x22);

  TEARDOWN();
}

TEST(neon_ld1_q) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld1(v2.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v3.V16B(), v4.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v5.V8H(), v6.V8H(), v7.V8H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v16.V4S(), v17.V4S(), v18.V4S(), v19.V4S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1(v30.V2D(), v31.V2D(), v0.V2D(), v1.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q2);
  CHECK_EQUAL_128(0x100F0E0D0C0B0A09, 0x0807060504030201, q3);
  CHECK_EQUAL_128(0x201F1E1D1C1B1A19, 0x1817161514131211, q4);
  CHECK_EQUAL_128(0x11100F0E0D0C0B0A, 0x0908070605040302, q5);
  CHECK_EQUAL_128(0x21201F1E1D1C1B1A, 0x1918171615141312, q6);
  CHECK_EQUAL_128(0x31302F2E2D2C2B2A, 0x2928272625242322, q7);
  CHECK_EQUAL_128(0x1211100F0E0D0C0B, 0x0A09080706050403, q16);
  CHECK_EQUAL_128(0x2221201F1E1D1C1B, 0x1A19181716151413, q17);
  CHECK_EQUAL_128(0x3231302F2E2D2C2B, 0x2A29282726252423, q18);
  CHECK_EQUAL_128(0x4241403F3E3D3C3B, 0x3A39383736353433, q19);
  CHECK_EQUAL_128(0x131211100F0E0D0C, 0x0B0A090807060504, q30);
  CHECK_EQUAL_128(0x232221201F1E1D1C, 0x1B1A191817161514, q31);
  CHECK_EQUAL_128(0x333231302F2E2D2C, 0x2B2A292827262524, q0);
  CHECK_EQUAL_128(0x434241403F3E3D3C, 0x3B3A393837363534, q1);

  TEARDOWN();
}

TEST(neon_ld1_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);
  __ Ld1(v2.V16B(), MemOperand(x17, x22, PostIndex));
  __ Ld1(v3.V16B(), v4.V16B(), MemOperand(x18, 32, PostIndex));
  __ Ld1(v5.V8H(), v6.V8H(), v7.V8H(), MemOperand(x19, 48, PostIndex));
  __ Ld1(v16.V4S(), v17.V4S(), v18.V4S(), v19.V4S(),
         MemOperand(x20, 64, PostIndex));
  __ Ld1(v30.V2D(), v31.V2D(), v0.V2D(), v1.V2D(),
         MemOperand(x21, 64, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q2);
  CHECK_EQUAL_128(0x100F0E0D0C0B0A09, 0x0807060504030201, q3);
  CHECK_EQUAL_128(0x201F1E1D1C1B1A19, 0x1817161514131211, q4);
  CHECK_EQUAL_128(0x11100F0E0D0C0B0A, 0x0908070605040302, q5);
  CHECK_EQUAL_128(0x21201F1E1D1C1B1A, 0x1918171615141312, q6);
  CHECK_EQUAL_128(0x31302F2E2D2C2B2A, 0x2928272625242322, q7);
  CHECK_EQUAL_128(0x1211100F0E0D0C0B, 0x0A09080706050403, q16);
  CHECK_EQUAL_128(0x2221201F1E1D1C1B, 0x1A19181716151413, q17);
  CHECK_EQUAL_128(0x3231302F2E2D2C2B, 0x2A29282726252423, q18);
  CHECK_EQUAL_128(0x4241403F3E3D3C3B, 0x3A39383736353433, q19);
  CHECK_EQUAL_128(0x131211100F0E0D0C, 0x0B0A090807060504, q30);
  CHECK_EQUAL_128(0x232221201F1E1D1C, 0x1B1A191817161514, q31);
  CHECK_EQUAL_128(0x333231302F2E2D2C, 0x2B2A292827262524, q0);
  CHECK_EQUAL_128(0x434241403F3E3D3C, 0x3B3A393837363534, q1);
  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 32, x18);
  CHECK_EQUAL_64(src_base + 2 + 48, x19);
  CHECK_EQUAL_64(src_base + 3 + 64, x20);
  CHECK_EQUAL_64(src_base + 4 + 64, x21);

  TEARDOWN();
}

TEST(neon_ld1_lane) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();

  // Test loading whole register by element.
  __ Mov(x17, src_base);
  for (int i = 15; i >= 0; i--) {
    __ Ld1(v0.B(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 7; i >= 0; i--) {
    __ Ld1(v1.H(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 3; i >= 0; i--) {
    __ Ld1(v2.S(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 1; i >= 0; i--) {
    __ Ld1(v3.D(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  // Test loading a single element into an initialised register.
  __ Mov(x17, src_base);
  __ Ldr(q4, MemOperand(x17));
  __ Ld1(v4.B(), 4, MemOperand(x17));
  __ Ldr(q5, MemOperand(x17));
  __ Ld1(v5.H(), 3, MemOperand(x17));
  __ Ldr(q6, MemOperand(x17));
  __ Ld1(v6.S(), 2, MemOperand(x17));
  __ Ldr(q7, MemOperand(x17));
  __ Ld1(v7.D(), 1, MemOperand(x17));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q0);
  CHECK_EQUAL_128(0x0100020103020403, 0x0504060507060807, q1);
  CHECK_EQUAL_128(0x0302010004030201, 0x0504030206050403, q2);
  CHECK_EQUAL_128(0x0706050403020100, 0x0807060504030201, q3);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q4);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q5);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q6);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q7);

  TEARDOWN();
}

TEST(neon_ld2_d) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld2(v2.V8B(), v3.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v4.V8B(), v5.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v6.V4H(), v7.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v31.V2S(), v0.V2S(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0E0C0A0806040200, q2);
  CHECK_EQUAL_128(0, 0x0F0D0B0907050301, q3);
  CHECK_EQUAL_128(0, 0x0F0D0B0907050301, q4);
  CHECK_EQUAL_128(0, 0x100E0C0A08060402, q5);
  CHECK_EQUAL_128(0, 0x0F0E0B0A07060302, q6);
  CHECK_EQUAL_128(0, 0x11100D0C09080504, q7);
  CHECK_EQUAL_128(0, 0x0E0D0C0B06050403, q31);
  CHECK_EQUAL_128(0, 0x1211100F0A090807, q0);

  TEARDOWN();
}

TEST(neon_ld2_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[32 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);
  __ Ld2(v2.V8B(), v3.V8B(), MemOperand(x17, x22, PostIndex));
  __ Ld2(v4.V8B(), v5.V8B(), MemOperand(x18, 16, PostIndex));
  __ Ld2(v5.V4H(), v6.V4H(), MemOperand(x19, 16, PostIndex));
  __ Ld2(v16.V2S(), v17.V2S(), MemOperand(x20, 16, PostIndex));
  __ Ld2(v31.V2S(), v0.V2S(), MemOperand(x21, 16, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0E0C0A0806040200, q2);
  CHECK_EQUAL_128(0, 0x0F0D0B0907050301, q3);
  CHECK_EQUAL_128(0, 0x0F0D0B0907050301, q4);
  CHECK_EQUAL_128(0, 0x0F0E0B0A07060302, q5);
  CHECK_EQUAL_128(0, 0x11100D0C09080504, q6);
  CHECK_EQUAL_128(0, 0x0E0D0C0B06050403, q16);
  CHECK_EQUAL_128(0, 0x1211100F0A090807, q17);
  CHECK_EQUAL_128(0, 0x0F0E0D0C07060504, q31);
  CHECK_EQUAL_128(0, 0x131211100B0A0908, q0);

  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 16, x18);
  CHECK_EQUAL_64(src_base + 2 + 16, x19);
  CHECK_EQUAL_64(src_base + 3 + 16, x20);
  CHECK_EQUAL_64(src_base + 4 + 16, x21);

  TEARDOWN();
}

TEST(neon_ld2_q) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld2(v2.V16B(), v3.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v4.V16B(), v5.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v6.V8H(), v7.V8H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v16.V4S(), v17.V4S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2(v31.V2D(), v0.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x1E1C1A1816141210, 0x0E0C0A0806040200, q2);
  CHECK_EQUAL_128(0x1F1D1B1917151311, 0x0F0D0B0907050301, q3);
  CHECK_EQUAL_128(0x1F1D1B1917151311, 0x0F0D0B0907050301, q4);
  CHECK_EQUAL_128(0x201E1C1A18161412, 0x100E0C0A08060402, q5);
  CHECK_EQUAL_128(0x1F1E1B1A17161312, 0x0F0E0B0A07060302, q6);
  CHECK_EQUAL_128(0x21201D1C19181514, 0x11100D0C09080504, q7);
  CHECK_EQUAL_128(0x1E1D1C1B16151413, 0x0E0D0C0B06050403, q16);
  CHECK_EQUAL_128(0x2221201F1A191817, 0x1211100F0A090807, q17);
  CHECK_EQUAL_128(0x1B1A191817161514, 0x0B0A090807060504, q31);
  CHECK_EQUAL_128(0x232221201F1E1D1C, 0x131211100F0E0D0C, q0);

  TEARDOWN();
}

TEST(neon_ld2_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);
  __ Ld2(v2.V16B(), v3.V16B(), MemOperand(x17, x22, PostIndex));
  __ Ld2(v4.V16B(), v5.V16B(), MemOperand(x18, 32, PostIndex));
  __ Ld2(v6.V8H(), v7.V8H(), MemOperand(x19, 32, PostIndex));
  __ Ld2(v16.V4S(), v17.V4S(), MemOperand(x20, 32, PostIndex));
  __ Ld2(v31.V2D(), v0.V2D(), MemOperand(x21, 32, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x1E1C1A1816141210, 0x0E0C0A0806040200, q2);
  CHECK_EQUAL_128(0x1F1D1B1917151311, 0x0F0D0B0907050301, q3);
  CHECK_EQUAL_128(0x1F1D1B1917151311, 0x0F0D0B0907050301, q4);
  CHECK_EQUAL_128(0x201E1C1A18161412, 0x100E0C0A08060402, q5);
  CHECK_EQUAL_128(0x1F1E1B1A17161312, 0x0F0E0B0A07060302, q6);
  CHECK_EQUAL_128(0x21201D1C19181514, 0x11100D0C09080504, q7);
  CHECK_EQUAL_128(0x1E1D1C1B16151413, 0x0E0D0C0B06050403, q16);
  CHECK_EQUAL_128(0x2221201F1A191817, 0x1211100F0A090807, q17);
  CHECK_EQUAL_128(0x1B1A191817161514, 0x0B0A090807060504, q31);
  CHECK_EQUAL_128(0x232221201F1E1D1C, 0x131211100F0E0D0C, q0);

  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 32, x18);
  CHECK_EQUAL_64(src_base + 2 + 32, x19);
  CHECK_EQUAL_64(src_base + 3 + 32, x20);
  CHECK_EQUAL_64(src_base + 4 + 32, x21);

  TEARDOWN();
}

TEST(neon_ld2_lane) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();

  // Test loading whole register by element.
  __ Mov(x17, src_base);
  for (int i = 15; i >= 0; i--) {
    __ Ld2(v0.B(), v1.B(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 7; i >= 0; i--) {
    __ Ld2(v2.H(), v3.H(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 3; i >= 0; i--) {
    __ Ld2(v4.S(), v5.S(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 1; i >= 0; i--) {
    __ Ld2(v6.D(), v7.D(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  // Test loading a single element into an initialised register.
  __ Mov(x17, src_base);
  __ Mov(x4, x17);
  __ Ldr(q8, MemOperand(x4, 16, PostIndex));
  __ Ldr(q9, MemOperand(x4));
  __ Ld2(v8_.B(), v9.B(), 4, MemOperand(x17));
  __ Mov(x5, x17);
  __ Ldr(q10, MemOperand(x5, 16, PostIndex));
  __ Ldr(q11, MemOperand(x5));
  __ Ld2(v10.H(), v11.H(), 3, MemOperand(x17));
  __ Mov(x6, x17);
  __ Ldr(q12, MemOperand(x6, 16, PostIndex));
  __ Ldr(q13, MemOperand(x6));
  __ Ld2(v12.S(), v13.S(), 2, MemOperand(x17));
  __ Mov(x7, x17);
  __ Ldr(q14, MemOperand(x7, 16, PostIndex));
  __ Ldr(q15, MemOperand(x7));
  __ Ld2(v14.D(), v15.D(), 1, MemOperand(x17));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q0);
  CHECK_EQUAL_128(0x0102030405060708, 0x090A0B0C0D0E0F10, q1);
  CHECK_EQUAL_128(0x0100020103020403, 0x0504060507060807, q2);
  CHECK_EQUAL_128(0x0302040305040605, 0x0706080709080A09, q3);
  CHECK_EQUAL_128(0x0302010004030201, 0x0504030206050403, q4);
  CHECK_EQUAL_128(0x0706050408070605, 0x090807060A090807, q5);
  CHECK_EQUAL_128(0x0706050403020100, 0x0807060504030201, q6);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x100F0E0D0C0B0A09, q7);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q8);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716150113121110, q9);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q10);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0302151413121110, q11);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q12);
  CHECK_EQUAL_128(0x1F1E1D1C07060504, 0x1716151413121110, q13);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q14);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x1716151413121110, q15);

  TEARDOWN();
}

TEST(neon_ld2_lane_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Mov(x19, src_base);
  __ Mov(x20, src_base);
  __ Mov(x21, src_base);
  __ Mov(x22, src_base);
  __ Mov(x23, src_base);
  __ Mov(x24, src_base);

  // Test loading whole register by element.
  for (int i = 15; i >= 0; i--) {
    __ Ld2(v0.B(), v1.B(), i, MemOperand(x17, 2, PostIndex));
  }

  for (int i = 7; i >= 0; i--) {
    __ Ld2(v2.H(), v3.H(), i, MemOperand(x18, 4, PostIndex));
  }

  for (int i = 3; i >= 0; i--) {
    __ Ld2(v4.S(), v5.S(), i, MemOperand(x19, 8, PostIndex));
  }

  for (int i = 1; i >= 0; i--) {
    __ Ld2(v6.D(), v7.D(), i, MemOperand(x20, 16, PostIndex));
  }

  // Test loading a single element into an initialised register.
  __ Mov(x25, 1);
  __ Mov(x4, x21);
  __ Ldr(q8, MemOperand(x4, 16, PostIndex));
  __ Ldr(q9, MemOperand(x4));
  __ Ld2(v8_.B(), v9.B(), 4, MemOperand(x21, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x5, x22);
  __ Ldr(q10, MemOperand(x5, 16, PostIndex));
  __ Ldr(q11, MemOperand(x5));
  __ Ld2(v10.H(), v11.H(), 3, MemOperand(x22, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x6, x23);
  __ Ldr(q12, MemOperand(x6, 16, PostIndex));
  __ Ldr(q13, MemOperand(x6));
  __ Ld2(v12.S(), v13.S(), 2, MemOperand(x23, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x7, x24);
  __ Ldr(q14, MemOperand(x7, 16, PostIndex));
  __ Ldr(q15, MemOperand(x7));
  __ Ld2(v14.D(), v15.D(), 1, MemOperand(x24, x25, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x00020406080A0C0E, 0x10121416181A1C1E, q0);
  CHECK_EQUAL_128(0x01030507090B0D0F, 0x11131517191B1D1F, q1);
  CHECK_EQUAL_128(0x0100050409080D0C, 0x1110151419181D1C, q2);
  CHECK_EQUAL_128(0x030207060B0A0F0E, 0x131217161B1A1F1E, q3);
  CHECK_EQUAL_128(0x030201000B0A0908, 0x131211101B1A1918, q4);
  CHECK_EQUAL_128(0x070605040F0E0D0C, 0x171615141F1E1D1C, q5);
  CHECK_EQUAL_128(0x0706050403020100, 0x1716151413121110, q6);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x1F1E1D1C1B1A1918, q7);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q8);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716150113121110, q9);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q10);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0302151413121110, q11);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q12);
  CHECK_EQUAL_128(0x1F1E1D1C07060504, 0x1716151413121110, q13);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q14);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x1716151413121110, q15);

  CHECK_EQUAL_64(src_base + 32, x17);
  CHECK_EQUAL_64(src_base + 32, x18);
  CHECK_EQUAL_64(src_base + 32, x19);
  CHECK_EQUAL_64(src_base + 32, x20);
  CHECK_EQUAL_64(src_base + 1, x21);
  CHECK_EQUAL_64(src_base + 2, x22);
  CHECK_EQUAL_64(src_base + 3, x23);
  CHECK_EQUAL_64(src_base + 4, x24);

  TEARDOWN();
}

TEST(neon_ld2_alllanes) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld2r(v0.V8B(), v1.V8B(), MemOperand(x17));
  __ Add(x17, x17, 2);
  __ Ld2r(v2.V16B(), v3.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2r(v4.V4H(), v5.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2r(v6.V8H(), v7.V8H(), MemOperand(x17));
  __ Add(x17, x17, 4);
  __ Ld2r(v8_.V2S(), v9.V2S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld2r(v10.V4S(), v11.V4S(), MemOperand(x17));
  __ Add(x17, x17, 8);
  __ Ld2r(v12.V2D(), v13.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x0000000000000000, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0000000000000000, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0x0303030303030303, 0x0303030303030303, q2);
  CHECK_EQUAL_128(0x0404040404040404, 0x0404040404040404, q3);
  CHECK_EQUAL_128(0x0000000000000000, 0x0504050405040504, q4);
  CHECK_EQUAL_128(0x0000000000000000, 0x0706070607060706, q5);
  CHECK_EQUAL_128(0x0605060506050605, 0x0605060506050605, q6);
  CHECK_EQUAL_128(0x0807080708070807, 0x0807080708070807, q7);
  CHECK_EQUAL_128(0x0000000000000000, 0x0C0B0A090C0B0A09, q8);
  CHECK_EQUAL_128(0x0000000000000000, 0x100F0E0D100F0E0D, q9);
  CHECK_EQUAL_128(0x0D0C0B0A0D0C0B0A, 0x0D0C0B0A0D0C0B0A, q10);
  CHECK_EQUAL_128(0x11100F0E11100F0E, 0x11100F0E11100F0E, q11);
  CHECK_EQUAL_128(0x1918171615141312, 0x1918171615141312, q12);
  CHECK_EQUAL_128(0x21201F1E1D1C1B1A, 0x21201F1E1D1C1B1A, q13);

  TEARDOWN();
}

TEST(neon_ld2_alllanes_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld2r(v0.V8B(), v1.V8B(), MemOperand(x17, 2, PostIndex));
  __ Ld2r(v2.V16B(), v3.V16B(), MemOperand(x17, x18, PostIndex));
  __ Ld2r(v4.V4H(), v5.V4H(), MemOperand(x17, x18, PostIndex));
  __ Ld2r(v6.V8H(), v7.V8H(), MemOperand(x17, 4, PostIndex));
  __ Ld2r(v8_.V2S(), v9.V2S(), MemOperand(x17, x18, PostIndex));
  __ Ld2r(v10.V4S(), v11.V4S(), MemOperand(x17, 8, PostIndex));
  __ Ld2r(v12.V2D(), v13.V2D(), MemOperand(x17, 16, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x0000000000000000, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0000000000000000, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0x0303030303030303, 0x0303030303030303, q2);
  CHECK_EQUAL_128(0x0404040404040404, 0x0404040404040404, q3);
  CHECK_EQUAL_128(0x0000000000000000, 0x0504050405040504, q4);
  CHECK_EQUAL_128(0x0000000000000000, 0x0706070607060706, q5);
  CHECK_EQUAL_128(0x0605060506050605, 0x0605060506050605, q6);
  CHECK_EQUAL_128(0x0807080708070807, 0x0807080708070807, q7);
  CHECK_EQUAL_128(0x0000000000000000, 0x0C0B0A090C0B0A09, q8);
  CHECK_EQUAL_128(0x0000000000000000, 0x100F0E0D100F0E0D, q9);
  CHECK_EQUAL_128(0x0D0C0B0A0D0C0B0A, 0x0D0C0B0A0D0C0B0A, q10);
  CHECK_EQUAL_128(0x11100F0E11100F0E, 0x11100F0E11100F0E, q11);
  CHECK_EQUAL_128(0x1918171615141312, 0x1918171615141312, q12);
  CHECK_EQUAL_128(0x21201F1E1D1C1B1A, 0x21201F1E1D1C1B1A, q13);
  CHECK_EQUAL_64(src_base + 34, x17);

  TEARDOWN();
}

TEST(neon_ld3_d) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld3(v2.V8B(), v3.V8B(), v4.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v5.V8B(), v6.V8B(), v7.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v8_.V4H(), v9.V4H(), v10.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v31.V2S(), v0.V2S(), v1.V2S(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x15120F0C09060300, q2);
  CHECK_EQUAL_128(0, 0x1613100D0A070401, q3);
  CHECK_EQUAL_128(0, 0x1714110E0B080502, q4);
  CHECK_EQUAL_128(0, 0x1613100D0A070401, q5);
  CHECK_EQUAL_128(0, 0x1714110E0B080502, q6);
  CHECK_EQUAL_128(0, 0x1815120F0C090603, q7);
  CHECK_EQUAL_128(0, 0x15140F0E09080302, q8);
  CHECK_EQUAL_128(0, 0x171611100B0A0504, q9);
  CHECK_EQUAL_128(0, 0x191813120D0C0706, q10);
  CHECK_EQUAL_128(0, 0x1211100F06050403, q31);
  CHECK_EQUAL_128(0, 0x161514130A090807, q0);
  CHECK_EQUAL_128(0, 0x1A1918170E0D0C0B, q1);

  TEARDOWN();
}

TEST(neon_ld3_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[32 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);
  __ Ld3(v2.V8B(), v3.V8B(), v4.V8B(), MemOperand(x17, x22, PostIndex));
  __ Ld3(v5.V8B(), v6.V8B(), v7.V8B(), MemOperand(x18, 24, PostIndex));
  __ Ld3(v8_.V4H(), v9.V4H(), v10.V4H(), MemOperand(x19, 24, PostIndex));
  __ Ld3(v11.V2S(), v12.V2S(), v13.V2S(), MemOperand(x20, 24, PostIndex));
  __ Ld3(v31.V2S(), v0.V2S(), v1.V2S(), MemOperand(x21, 24, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x15120F0C09060300, q2);
  CHECK_EQUAL_128(0, 0x1613100D0A070401, q3);
  CHECK_EQUAL_128(0, 0x1714110E0B080502, q4);
  CHECK_EQUAL_128(0, 0x1613100D0A070401, q5);
  CHECK_EQUAL_128(0, 0x1714110E0B080502, q6);
  CHECK_EQUAL_128(0, 0x1815120F0C090603, q7);
  CHECK_EQUAL_128(0, 0x15140F0E09080302, q8);
  CHECK_EQUAL_128(0, 0x171611100B0A0504, q9);
  CHECK_EQUAL_128(0, 0x191813120D0C0706, q10);
  CHECK_EQUAL_128(0, 0x1211100F06050403, q11);
  CHECK_EQUAL_128(0, 0x161514130A090807, q12);
  CHECK_EQUAL_128(0, 0x1A1918170E0D0C0B, q13);
  CHECK_EQUAL_128(0, 0x1312111007060504, q31);
  CHECK_EQUAL_128(0, 0x171615140B0A0908, q0);
  CHECK_EQUAL_128(0, 0x1B1A19180F0E0D0C, q1);

  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 24, x18);
  CHECK_EQUAL_64(src_base + 2 + 24, x19);
  CHECK_EQUAL_64(src_base + 3 + 24, x20);
  CHECK_EQUAL_64(src_base + 4 + 24, x21);

  TEARDOWN();
}

TEST(neon_ld3_q) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld3(v2.V16B(), v3.V16B(), v4.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v5.V16B(), v6.V16B(), v7.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v8_.V8H(), v9.V8H(), v10.V8H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v11.V4S(), v12.V4S(), v13.V4S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3(v31.V2D(), v0.V2D(), v1.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x2D2A2724211E1B18, 0x15120F0C09060300, q2);
  CHECK_EQUAL_128(0x2E2B2825221F1C19, 0x1613100D0A070401, q3);
  CHECK_EQUAL_128(0x2F2C292623201D1A, 0x1714110E0B080502, q4);
  CHECK_EQUAL_128(0x2E2B2825221F1C19, 0x1613100D0A070401, q5);
  CHECK_EQUAL_128(0x2F2C292623201D1A, 0x1714110E0B080502, q6);
  CHECK_EQUAL_128(0x302D2A2724211E1B, 0x1815120F0C090603, q7);
  CHECK_EQUAL_128(0x2D2C272621201B1A, 0x15140F0E09080302, q8);
  CHECK_EQUAL_128(0x2F2E292823221D1C, 0x171611100B0A0504, q9);
  CHECK_EQUAL_128(0x31302B2A25241F1E, 0x191813120D0C0706, q10);
  CHECK_EQUAL_128(0x2A2928271E1D1C1B, 0x1211100F06050403, q11);
  CHECK_EQUAL_128(0x2E2D2C2B2221201F, 0x161514130A090807, q12);
  CHECK_EQUAL_128(0x3231302F26252423, 0x1A1918170E0D0C0B, q13);
  CHECK_EQUAL_128(0x232221201F1E1D1C, 0x0B0A090807060504, q31);
  CHECK_EQUAL_128(0x2B2A292827262524, 0x131211100F0E0D0C, q0);
  CHECK_EQUAL_128(0x333231302F2E2D2C, 0x1B1A191817161514, q1);

  TEARDOWN();
}

TEST(neon_ld3_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);

  __ Ld3(v2.V16B(), v3.V16B(), v4.V16B(), MemOperand(x17, x22, PostIndex));
  __ Ld3(v5.V16B(), v6.V16B(), v7.V16B(), MemOperand(x18, 48, PostIndex));
  __ Ld3(v8_.V8H(), v9.V8H(), v10.V8H(), MemOperand(x19, 48, PostIndex));
  __ Ld3(v11.V4S(), v12.V4S(), v13.V4S(), MemOperand(x20, 48, PostIndex));
  __ Ld3(v31.V2D(), v0.V2D(), v1.V2D(), MemOperand(x21, 48, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x2D2A2724211E1B18, 0x15120F0C09060300, q2);
  CHECK_EQUAL_128(0x2E2B2825221F1C19, 0x1613100D0A070401, q3);
  CHECK_EQUAL_128(0x2F2C292623201D1A, 0x1714110E0B080502, q4);
  CHECK_EQUAL_128(0x2E2B2825221F1C19, 0x1613100D0A070401, q5);
  CHECK_EQUAL_128(0x2F2C292623201D1A, 0x1714110E0B080502, q6);
  CHECK_EQUAL_128(0x302D2A2724211E1B, 0x1815120F0C090603, q7);
  CHECK_EQUAL_128(0x2D2C272621201B1A, 0x15140F0E09080302, q8);
  CHECK_EQUAL_128(0x2F2E292823221D1C, 0x171611100B0A0504, q9);
  CHECK_EQUAL_128(0x31302B2A25241F1E, 0x191813120D0C0706, q10);
  CHECK_EQUAL_128(0x2A2928271E1D1C1B, 0x1211100F06050403, q11);
  CHECK_EQUAL_128(0x2E2D2C2B2221201F, 0x161514130A090807, q12);
  CHECK_EQUAL_128(0x3231302F26252423, 0x1A1918170E0D0C0B, q13);
  CHECK_EQUAL_128(0x232221201F1E1D1C, 0x0B0A090807060504, q31);
  CHECK_EQUAL_128(0x2B2A292827262524, 0x131211100F0E0D0C, q0);
  CHECK_EQUAL_128(0x333231302F2E2D2C, 0x1B1A191817161514, q1);

  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 48, x18);
  CHECK_EQUAL_64(src_base + 2 + 48, x19);
  CHECK_EQUAL_64(src_base + 3 + 48, x20);
  CHECK_EQUAL_64(src_base + 4 + 48, x21);

  TEARDOWN();
}

TEST(neon_ld3_lane) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();

  // Test loading whole register by element.
  __ Mov(x17, src_base);
  for (int i = 15; i >= 0; i--) {
    __ Ld3(v0.B(), v1.B(), v2.B(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 7; i >= 0; i--) {
    __ Ld3(v3.H(), v4.H(), v5.H(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 3; i >= 0; i--) {
    __ Ld3(v6.S(), v7.S(), v8_.S(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 1; i >= 0; i--) {
    __ Ld3(v9.D(), v10.D(), v11.D(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  // Test loading a single element into an initialised register.
  __ Mov(x17, src_base);
  __ Mov(x4, x17);
  __ Ldr(q12, MemOperand(x4, 16, PostIndex));
  __ Ldr(q13, MemOperand(x4, 16, PostIndex));
  __ Ldr(q14, MemOperand(x4));
  __ Ld3(v12.B(), v13.B(), v14.B(), 4, MemOperand(x17));
  __ Mov(x5, x17);
  __ Ldr(q15, MemOperand(x5, 16, PostIndex));
  __ Ldr(q16, MemOperand(x5, 16, PostIndex));
  __ Ldr(q17, MemOperand(x5));
  __ Ld3(v15.H(), v16.H(), v17.H(), 3, MemOperand(x17));
  __ Mov(x6, x17);
  __ Ldr(q18, MemOperand(x6, 16, PostIndex));
  __ Ldr(q19, MemOperand(x6, 16, PostIndex));
  __ Ldr(q20, MemOperand(x6));
  __ Ld3(v18.S(), v19.S(), v20.S(), 2, MemOperand(x17));
  __ Mov(x7, x17);
  __ Ldr(q21, MemOperand(x7, 16, PostIndex));
  __ Ldr(q22, MemOperand(x7, 16, PostIndex));
  __ Ldr(q23, MemOperand(x7));
  __ Ld3(v21.D(), v22.D(), v23.D(), 1, MemOperand(x17));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q0);
  CHECK_EQUAL_128(0x0102030405060708, 0x090A0B0C0D0E0F10, q1);
  CHECK_EQUAL_128(0x0203040506070809, 0x0A0B0C0D0E0F1011, q2);
  CHECK_EQUAL_128(0x0100020103020403, 0x0504060507060807, q3);
  CHECK_EQUAL_128(0x0302040305040605, 0x0706080709080A09, q4);
  CHECK_EQUAL_128(0x0504060507060807, 0x09080A090B0A0C0B, q5);
  CHECK_EQUAL_128(0x0302010004030201, 0x0504030206050403, q6);
  CHECK_EQUAL_128(0x0706050408070605, 0x090807060A090807, q7);
  CHECK_EQUAL_128(0x0B0A09080C0B0A09, 0x0D0C0B0A0E0D0C0B, q8);
  CHECK_EQUAL_128(0x0706050403020100, 0x0807060504030201, q9);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x100F0E0D0C0B0A09, q10);
  CHECK_EQUAL_128(0x1716151413121110, 0x1817161514131211, q11);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q12);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716150113121110, q13);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726250223222120, q14);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q15);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0302151413121110, q16);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x0504252423222120, q17);

  TEARDOWN();
}

TEST(neon_ld3_lane_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();

  // Test loading whole register by element.
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Mov(x19, src_base);
  __ Mov(x20, src_base);
  __ Mov(x21, src_base);
  __ Mov(x22, src_base);
  __ Mov(x23, src_base);
  __ Mov(x24, src_base);
  for (int i = 15; i >= 0; i--) {
    __ Ld3(v0.B(), v1.B(), v2.B(), i, MemOperand(x17, 3, PostIndex));
  }

  for (int i = 7; i >= 0; i--) {
    __ Ld3(v3.H(), v4.H(), v5.H(), i, MemOperand(x18, 6, PostIndex));
  }

  for (int i = 3; i >= 0; i--) {
    __ Ld3(v6.S(), v7.S(), v8_.S(), i, MemOperand(x19, 12, PostIndex));
  }

  for (int i = 1; i >= 0; i--) {
    __ Ld3(v9.D(), v10.D(), v11.D(), i, MemOperand(x20, 24, PostIndex));
  }

  // Test loading a single element into an initialised register.
  __ Mov(x25, 1);
  __ Mov(x4, x21);
  __ Ldr(q12, MemOperand(x4, 16, PostIndex));
  __ Ldr(q13, MemOperand(x4, 16, PostIndex));
  __ Ldr(q14, MemOperand(x4));
  __ Ld3(v12.B(), v13.B(), v14.B(), 4, MemOperand(x21, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x5, x22);
  __ Ldr(q15, MemOperand(x5, 16, PostIndex));
  __ Ldr(q16, MemOperand(x5, 16, PostIndex));
  __ Ldr(q17, MemOperand(x5));
  __ Ld3(v15.H(), v16.H(), v17.H(), 3, MemOperand(x22, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x6, x23);
  __ Ldr(q18, MemOperand(x6, 16, PostIndex));
  __ Ldr(q19, MemOperand(x6, 16, PostIndex));
  __ Ldr(q20, MemOperand(x6));
  __ Ld3(v18.S(), v19.S(), v20.S(), 2, MemOperand(x23, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x7, x24);
  __ Ldr(q21, MemOperand(x7, 16, PostIndex));
  __ Ldr(q22, MemOperand(x7, 16, PostIndex));
  __ Ldr(q23, MemOperand(x7));
  __ Ld3(v21.D(), v22.D(), v23.D(), 1, MemOperand(x24, x25, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x000306090C0F1215, 0x181B1E2124272A2D, q0);
  CHECK_EQUAL_128(0x0104070A0D101316, 0x191C1F2225282B2E, q1);
  CHECK_EQUAL_128(0x0205080B0E111417, 0x1A1D202326292C2F, q2);
  CHECK_EQUAL_128(0x010007060D0C1312, 0x19181F1E25242B2A, q3);
  CHECK_EQUAL_128(0x030209080F0E1514, 0x1B1A212027262D2C, q4);
  CHECK_EQUAL_128(0x05040B0A11101716, 0x1D1C232229282F2E, q5);
  CHECK_EQUAL_128(0x030201000F0E0D0C, 0x1B1A191827262524, q6);
  CHECK_EQUAL_128(0x0706050413121110, 0x1F1E1D1C2B2A2928, q7);
  CHECK_EQUAL_128(0x0B0A090817161514, 0x232221202F2E2D2C, q8);
  CHECK_EQUAL_128(0x0706050403020100, 0x1F1E1D1C1B1A1918, q9);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x2726252423222120, q10);
  CHECK_EQUAL_128(0x1716151413121110, 0x2F2E2D2C2B2A2928, q11);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q12);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716150113121110, q13);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726250223222120, q14);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q15);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0302151413121110, q16);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x0504252423222120, q17);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q18);
  CHECK_EQUAL_128(0x1F1E1D1C07060504, 0x1716151413121110, q19);
  CHECK_EQUAL_128(0x2F2E2D2C0B0A0908, 0x2726252423222120, q20);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q21);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x1716151413121110, q22);
  CHECK_EQUAL_128(0x1716151413121110, 0x2726252423222120, q23);

  CHECK_EQUAL_64(src_base + 48, x17);
  CHECK_EQUAL_64(src_base + 48, x18);
  CHECK_EQUAL_64(src_base + 48, x19);
  CHECK_EQUAL_64(src_base + 48, x20);
  CHECK_EQUAL_64(src_base + 1, x21);
  CHECK_EQUAL_64(src_base + 2, x22);
  CHECK_EQUAL_64(src_base + 3, x23);
  CHECK_EQUAL_64(src_base + 4, x24);

  TEARDOWN();
}

TEST(neon_ld3_alllanes) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld3r(v0.V8B(), v1.V8B(), v2.V8B(), MemOperand(x17));
  __ Add(x17, x17, 3);
  __ Ld3r(v3.V16B(), v4.V16B(), v5.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3r(v6.V4H(), v7.V4H(), v8_.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3r(v9.V8H(), v10.V8H(), v11.V8H(), MemOperand(x17));
  __ Add(x17, x17, 6);
  __ Ld3r(v12.V2S(), v13.V2S(), v14.V2S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld3r(v15.V4S(), v16.V4S(), v17.V4S(), MemOperand(x17));
  __ Add(x17, x17, 12);
  __ Ld3r(v18.V2D(), v19.V2D(), v20.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x0000000000000000, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0000000000000000, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0x0000000000000000, 0x0303030303030303, q2);
  CHECK_EQUAL_128(0x0404040404040404, 0x0404040404040404, q3);
  CHECK_EQUAL_128(0x0505050505050505, 0x0505050505050505, q4);
  CHECK_EQUAL_128(0x0606060606060606, 0x0606060606060606, q5);
  CHECK_EQUAL_128(0x0000000000000000, 0x0605060506050605, q6);
  CHECK_EQUAL_128(0x0000000000000000, 0x0807080708070807, q7);
  CHECK_EQUAL_128(0x0000000000000000, 0x0A090A090A090A09, q8);
  CHECK_EQUAL_128(0x0706070607060706, 0x0706070607060706, q9);
  CHECK_EQUAL_128(0x0908090809080908, 0x0908090809080908, q10);
  CHECK_EQUAL_128(0x0B0A0B0A0B0A0B0A, 0x0B0A0B0A0B0A0B0A, q11);
  CHECK_EQUAL_128(0x0000000000000000, 0x0F0E0D0C0F0E0D0C, q12);
  CHECK_EQUAL_128(0x0000000000000000, 0x1312111013121110, q13);
  CHECK_EQUAL_128(0x0000000000000000, 0x1716151417161514, q14);
  CHECK_EQUAL_128(0x100F0E0D100F0E0D, 0x100F0E0D100F0E0D, q15);
  CHECK_EQUAL_128(0x1413121114131211, 0x1413121114131211, q16);
  CHECK_EQUAL_128(0x1817161518171615, 0x1817161518171615, q17);
  CHECK_EQUAL_128(0x201F1E1D1C1B1A19, 0x201F1E1D1C1B1A19, q18);
  CHECK_EQUAL_128(0x2827262524232221, 0x2827262524232221, q19);
  CHECK_EQUAL_128(0x302F2E2D2C2B2A29, 0x302F2E2D2C2B2A29, q20);

  TEARDOWN();
}

TEST(neon_ld3_alllanes_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld3r(v0.V8B(), v1.V8B(), v2.V8B(), MemOperand(x17, 3, PostIndex));
  __ Ld3r(v3.V16B(), v4.V16B(), v5.V16B(), MemOperand(x17, x18, PostIndex));
  __ Ld3r(v6.V4H(), v7.V4H(), v8_.V4H(), MemOperand(x17, x18, PostIndex));
  __ Ld3r(v9.V8H(), v10.V8H(), v11.V8H(), MemOperand(x17, 6, PostIndex));
  __ Ld3r(v12.V2S(), v13.V2S(), v14.V2S(), MemOperand(x17, x18, PostIndex));
  __ Ld3r(v15.V4S(), v16.V4S(), v17.V4S(), MemOperand(x17, 12, PostIndex));
  __ Ld3r(v18.V2D(), v19.V2D(), v20.V2D(), MemOperand(x17, 24, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x0000000000000000, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0000000000000000, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0x0000000000000000, 0x0303030303030303, q2);
  CHECK_EQUAL_128(0x0404040404040404, 0x0404040404040404, q3);
  CHECK_EQUAL_128(0x0505050505050505, 0x0505050505050505, q4);
  CHECK_EQUAL_128(0x0606060606060606, 0x0606060606060606, q5);
  CHECK_EQUAL_128(0x0000000000000000, 0x0605060506050605, q6);
  CHECK_EQUAL_128(0x0000000000000000, 0x0807080708070807, q7);
  CHECK_EQUAL_128(0x0000000000000000, 0x0A090A090A090A09, q8);
  CHECK_EQUAL_128(0x0706070607060706, 0x0706070607060706, q9);
  CHECK_EQUAL_128(0x0908090809080908, 0x0908090809080908, q10);
  CHECK_EQUAL_128(0x0B0A0B0A0B0A0B0A, 0x0B0A0B0A0B0A0B0A, q11);
  CHECK_EQUAL_128(0x0000000000000000, 0x0F0E0D0C0F0E0D0C, q12);
  CHECK_EQUAL_128(0x0000000000000000, 0x1312111013121110, q13);
  CHECK_EQUAL_128(0x0000000000000000, 0x1716151417161514, q14);
  CHECK_EQUAL_128(0x100F0E0D100F0E0D, 0x100F0E0D100F0E0D, q15);
  CHECK_EQUAL_128(0x1413121114131211, 0x1413121114131211, q16);
  CHECK_EQUAL_128(0x1817161518171615, 0x1817161518171615, q17);
  CHECK_EQUAL_128(0x201F1E1D1C1B1A19, 0x201F1E1D1C1B1A19, q18);
  CHECK_EQUAL_128(0x2827262524232221, 0x2827262524232221, q19);
  CHECK_EQUAL_128(0x302F2E2D2C2B2A29, 0x302F2E2D2C2B2A29, q20);

  TEARDOWN();
}

TEST(neon_ld4_d) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld4(v2.V8B(), v3.V8B(), v4.V8B(), v5.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v6.V8B(), v7.V8B(), v8_.V8B(), v9.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v10.V4H(), v11.V4H(), v12.V4H(), v13.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v30.V2S(), v31.V2S(), v0.V2S(), v1.V2S(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x1C1814100C080400, q2);
  CHECK_EQUAL_128(0, 0x1D1915110D090501, q3);
  CHECK_EQUAL_128(0, 0x1E1A16120E0A0602, q4);
  CHECK_EQUAL_128(0, 0x1F1B17130F0B0703, q5);
  CHECK_EQUAL_128(0, 0x1D1915110D090501, q6);
  CHECK_EQUAL_128(0, 0x1E1A16120E0A0602, q7);
  CHECK_EQUAL_128(0, 0x1F1B17130F0B0703, q8);
  CHECK_EQUAL_128(0, 0x201C1814100C0804, q9);
  CHECK_EQUAL_128(0, 0x1B1A13120B0A0302, q10);
  CHECK_EQUAL_128(0, 0x1D1C15140D0C0504, q11);
  CHECK_EQUAL_128(0, 0x1F1E17160F0E0706, q12);
  CHECK_EQUAL_128(0, 0x2120191811100908, q13);
  CHECK_EQUAL_128(0, 0x1615141306050403, q30);
  CHECK_EQUAL_128(0, 0x1A1918170A090807, q31);
  CHECK_EQUAL_128(0, 0x1E1D1C1B0E0D0C0B, q0);
  CHECK_EQUAL_128(0, 0x2221201F1211100F, q1);

  TEARDOWN();
}

TEST(neon_ld4_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[32 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);
  __ Ld4(v2.V8B(), v3.V8B(), v4.V8B(), v5.V8B(),
         MemOperand(x17, x22, PostIndex));
  __ Ld4(v6.V8B(), v7.V8B(), v8_.V8B(), v9.V8B(),
         MemOperand(x18, 32, PostIndex));
  __ Ld4(v10.V4H(), v11.V4H(), v12.V4H(), v13.V4H(),
         MemOperand(x19, 32, PostIndex));
  __ Ld4(v14.V2S(), v15.V2S(), v16.V2S(), v17.V2S(),
         MemOperand(x20, 32, PostIndex));
  __ Ld4(v30.V2S(), v31.V2S(), v0.V2S(), v1.V2S(),
         MemOperand(x21, 32, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x1C1814100C080400, q2);
  CHECK_EQUAL_128(0, 0x1D1915110D090501, q3);
  CHECK_EQUAL_128(0, 0x1E1A16120E0A0602, q4);
  CHECK_EQUAL_128(0, 0x1F1B17130F0B0703, q5);
  CHECK_EQUAL_128(0, 0x1D1915110D090501, q6);
  CHECK_EQUAL_128(0, 0x1E1A16120E0A0602, q7);
  CHECK_EQUAL_128(0, 0x1F1B17130F0B0703, q8);
  CHECK_EQUAL_128(0, 0x201C1814100C0804, q9);
  CHECK_EQUAL_128(0, 0x1B1A13120B0A0302, q10);
  CHECK_EQUAL_128(0, 0x1D1C15140D0C0504, q11);
  CHECK_EQUAL_128(0, 0x1F1E17160F0E0706, q12);
  CHECK_EQUAL_128(0, 0x2120191811100908, q13);
  CHECK_EQUAL_128(0, 0x1615141306050403, q14);
  CHECK_EQUAL_128(0, 0x1A1918170A090807, q15);
  CHECK_EQUAL_128(0, 0x1E1D1C1B0E0D0C0B, q16);
  CHECK_EQUAL_128(0, 0x2221201F1211100F, q17);
  CHECK_EQUAL_128(0, 0x1716151407060504, q30);
  CHECK_EQUAL_128(0, 0x1B1A19180B0A0908, q31);
  CHECK_EQUAL_128(0, 0x1F1E1D1C0F0E0D0C, q0);
  CHECK_EQUAL_128(0, 0x2322212013121110, q1);

  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 32, x18);
  CHECK_EQUAL_64(src_base + 2 + 32, x19);
  CHECK_EQUAL_64(src_base + 3 + 32, x20);
  CHECK_EQUAL_64(src_base + 4 + 32, x21);
  TEARDOWN();
}

TEST(neon_ld4_q) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ld4(v2.V16B(), v3.V16B(), v4.V16B(), v5.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v6.V16B(), v7.V16B(), v8_.V16B(), v9.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v10.V8H(), v11.V8H(), v12.V8H(), v13.V8H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v14.V4S(), v15.V4S(), v16.V4S(), v17.V4S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4(v18.V2D(), v19.V2D(), v20.V2D(), v21.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x3C3834302C282420, 0x1C1814100C080400, q2);
  CHECK_EQUAL_128(0x3D3935312D292521, 0x1D1915110D090501, q3);
  CHECK_EQUAL_128(0x3E3A36322E2A2622, 0x1E1A16120E0A0602, q4);
  CHECK_EQUAL_128(0x3F3B37332F2B2723, 0x1F1B17130F0B0703, q5);
  CHECK_EQUAL_128(0x3D3935312D292521, 0x1D1915110D090501, q6);
  CHECK_EQUAL_128(0x3E3A36322E2A2622, 0x1E1A16120E0A0602, q7);
  CHECK_EQUAL_128(0x3F3B37332F2B2723, 0x1F1B17130F0B0703, q8);
  CHECK_EQUAL_128(0x403C3834302C2824, 0x201C1814100C0804, q9);
  CHECK_EQUAL_128(0x3B3A33322B2A2322, 0x1B1A13120B0A0302, q10);
  CHECK_EQUAL_128(0x3D3C35342D2C2524, 0x1D1C15140D0C0504, q11);
  CHECK_EQUAL_128(0x3F3E37362F2E2726, 0x1F1E17160F0E0706, q12);
  CHECK_EQUAL_128(0x4140393831302928, 0x2120191811100908, q13);
  CHECK_EQUAL_128(0x3635343326252423, 0x1615141306050403, q14);
  CHECK_EQUAL_128(0x3A3938372A292827, 0x1A1918170A090807, q15);
  CHECK_EQUAL_128(0x3E3D3C3B2E2D2C2B, 0x1E1D1C1B0E0D0C0B, q16);
  CHECK_EQUAL_128(0x4241403F3231302F, 0x2221201F1211100F, q17);
  CHECK_EQUAL_128(0x2B2A292827262524, 0x0B0A090807060504, q18);
  CHECK_EQUAL_128(0x333231302F2E2D2C, 0x131211100F0E0D0C, q19);
  CHECK_EQUAL_128(0x3B3A393837363534, 0x1B1A191817161514, q20);
  CHECK_EQUAL_128(0x434241403F3E3D3C, 0x232221201F1E1D1C, q21);
  TEARDOWN();
}

TEST(neon_ld4_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 4];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base + 1);
  __ Mov(x19, src_base + 2);
  __ Mov(x20, src_base + 3);
  __ Mov(x21, src_base + 4);
  __ Mov(x22, 1);

  __ Ld4(v2.V16B(), v3.V16B(), v4.V16B(), v5.V16B(),
         MemOperand(x17, x22, PostIndex));
  __ Ld4(v6.V16B(), v7.V16B(), v8_.V16B(), v9.V16B(),
         MemOperand(x18, 64, PostIndex));
  __ Ld4(v10.V8H(), v11.V8H(), v12.V8H(), v13.V8H(),
         MemOperand(x19, 64, PostIndex));
  __ Ld4(v14.V4S(), v15.V4S(), v16.V4S(), v17.V4S(),
         MemOperand(x20, 64, PostIndex));
  __ Ld4(v30.V2D(), v31.V2D(), v0.V2D(), v1.V2D(),
         MemOperand(x21, 64, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x3C3834302C282420, 0x1C1814100C080400, q2);
  CHECK_EQUAL_128(0x3D3935312D292521, 0x1D1915110D090501, q3);
  CHECK_EQUAL_128(0x3E3A36322E2A2622, 0x1E1A16120E0A0602, q4);
  CHECK_EQUAL_128(0x3F3B37332F2B2723, 0x1F1B17130F0B0703, q5);
  CHECK_EQUAL_128(0x3D3935312D292521, 0x1D1915110D090501, q6);
  CHECK_EQUAL_128(0x3E3A36322E2A2622, 0x1E1A16120E0A0602, q7);
  CHECK_EQUAL_128(0x3F3B37332F2B2723, 0x1F1B17130F0B0703, q8);
  CHECK_EQUAL_128(0x403C3834302C2824, 0x201C1814100C0804, q9);
  CHECK_EQUAL_128(0x3B3A33322B2A2322, 0x1B1A13120B0A0302, q10);
  CHECK_EQUAL_128(0x3D3C35342D2C2524, 0x1D1C15140D0C0504, q11);
  CHECK_EQUAL_128(0x3F3E37362F2E2726, 0x1F1E17160F0E0706, q12);
  CHECK_EQUAL_128(0x4140393831302928, 0x2120191811100908, q13);
  CHECK_EQUAL_128(0x3635343326252423, 0x1615141306050403, q14);
  CHECK_EQUAL_128(0x3A3938372A292827, 0x1A1918170A090807, q15);
  CHECK_EQUAL_128(0x3E3D3C3B2E2D2C2B, 0x1E1D1C1B0E0D0C0B, q16);
  CHECK_EQUAL_128(0x4241403F3231302F, 0x2221201F1211100F, q17);
  CHECK_EQUAL_128(0x2B2A292827262524, 0x0B0A090807060504, q30);
  CHECK_EQUAL_128(0x333231302F2E2D2C, 0x131211100F0E0D0C, q31);
  CHECK_EQUAL_128(0x3B3A393837363534, 0x1B1A191817161514, q0);
  CHECK_EQUAL_128(0x434241403F3E3D3C, 0x232221201F1E1D1C, q1);

  CHECK_EQUAL_64(src_base + 1, x17);
  CHECK_EQUAL_64(src_base + 1 + 64, x18);
  CHECK_EQUAL_64(src_base + 2 + 64, x19);
  CHECK_EQUAL_64(src_base + 3 + 64, x20);
  CHECK_EQUAL_64(src_base + 4 + 64, x21);

  TEARDOWN();
}

TEST(neon_ld4_lane) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();

  // Test loading whole register by element.
  __ Mov(x17, src_base);
  for (int i = 15; i >= 0; i--) {
    __ Ld4(v0.B(), v1.B(), v2.B(), v3.B(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 7; i >= 0; i--) {
    __ Ld4(v4.H(), v5.H(), v6.H(), v7.H(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 3; i >= 0; i--) {
    __ Ld4(v8_.S(), v9.S(), v10.S(), v11.S(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  __ Mov(x17, src_base);
  for (int i = 1; i >= 0; i--) {
    __ Ld4(v12.D(), v13.D(), v14.D(), v15.D(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }

  // Test loading a single element into an initialised register.
  __ Mov(x17, src_base);
  __ Mov(x4, x17);
  __ Ldr(q16, MemOperand(x4, 16, PostIndex));
  __ Ldr(q17, MemOperand(x4, 16, PostIndex));
  __ Ldr(q18, MemOperand(x4, 16, PostIndex));
  __ Ldr(q19, MemOperand(x4));
  __ Ld4(v16.B(), v17.B(), v18.B(), v19.B(), 4, MemOperand(x17));

  __ Mov(x5, x17);
  __ Ldr(q20, MemOperand(x5, 16, PostIndex));
  __ Ldr(q21, MemOperand(x5, 16, PostIndex));
  __ Ldr(q22, MemOperand(x5, 16, PostIndex));
  __ Ldr(q23, MemOperand(x5));
  __ Ld4(v20.H(), v21.H(), v22.H(), v23.H(), 3, MemOperand(x17));

  __ Mov(x6, x17);
  __ Ldr(q24, MemOperand(x6, 16, PostIndex));
  __ Ldr(q25, MemOperand(x6, 16, PostIndex));
  __ Ldr(q26, MemOperand(x6, 16, PostIndex));
  __ Ldr(q27, MemOperand(x6));
  __ Ld4(v24.S(), v25.S(), v26.S(), v27.S(), 2, MemOperand(x17));

  __ Mov(x7, x17);
  __ Ldr(q28, MemOperand(x7, 16, PostIndex));
  __ Ldr(q29, MemOperand(x7, 16, PostIndex));
  __ Ldr(q30, MemOperand(x7, 16, PostIndex));
  __ Ldr(q31, MemOperand(x7));
  __ Ld4(v28.D(), v29.D(), v30.D(), v31.D(), 1, MemOperand(x17));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q0);
  CHECK_EQUAL_128(0x0102030405060708, 0x090A0B0C0D0E0F10, q1);
  CHECK_EQUAL_128(0x0203040506070809, 0x0A0B0C0D0E0F1011, q2);
  CHECK_EQUAL_128(0x030405060708090A, 0x0B0C0D0E0F101112, q3);
  CHECK_EQUAL_128(0x0100020103020403, 0x0504060507060807, q4);
  CHECK_EQUAL_128(0x0302040305040605, 0x0706080709080A09, q5);
  CHECK_EQUAL_128(0x0504060507060807, 0x09080A090B0A0C0B, q6);
  CHECK_EQUAL_128(0x0706080709080A09, 0x0B0A0C0B0D0C0E0D, q7);
  CHECK_EQUAL_128(0x0302010004030201, 0x0504030206050403, q8);
  CHECK_EQUAL_128(0x0706050408070605, 0x090807060A090807, q9);
  CHECK_EQUAL_128(0x0B0A09080C0B0A09, 0x0D0C0B0A0E0D0C0B, q10);
  CHECK_EQUAL_128(0x0F0E0D0C100F0E0D, 0x11100F0E1211100F, q11);
  CHECK_EQUAL_128(0x0706050403020100, 0x0807060504030201, q12);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x100F0E0D0C0B0A09, q13);
  CHECK_EQUAL_128(0x1716151413121110, 0x1817161514131211, q14);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x201F1E1D1C1B1A19, q15);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q16);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716150113121110, q17);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726250223222120, q18);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736350333323130, q19);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q20);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0302151413121110, q21);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x0504252423222120, q22);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x0706353433323130, q23);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q24);
  CHECK_EQUAL_128(0x1F1E1D1C07060504, 0x1716151413121110, q25);
  CHECK_EQUAL_128(0x2F2E2D2C0B0A0908, 0x2726252423222120, q26);
  CHECK_EQUAL_128(0x3F3E3D3C0F0E0D0C, 0x3736353433323130, q27);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q28);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x1716151413121110, q29);
  CHECK_EQUAL_128(0x1716151413121110, 0x2726252423222120, q30);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x3736353433323130, q31);

  TEARDOWN();
}

TEST(neon_ld4_lane_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();

  // Test loading whole register by element.
  __ Mov(x17, src_base);
  for (int i = 15; i >= 0; i--) {
    __ Ld4(v0.B(), v1.B(), v2.B(), v3.B(), i, MemOperand(x17, 4, PostIndex));
  }

  __ Mov(x18, src_base);
  for (int i = 7; i >= 0; i--) {
    __ Ld4(v4.H(), v5.H(), v6.H(), v7.H(), i, MemOperand(x18, 8, PostIndex));
  }

  __ Mov(x19, src_base);
  for (int i = 3; i >= 0; i--) {
    __ Ld4(v8_.S(), v9.S(), v10.S(), v11.S(), i,
           MemOperand(x19, 16, PostIndex));
  }

  __ Mov(x20, src_base);
  for (int i = 1; i >= 0; i--) {
    __ Ld4(v12.D(), v13.D(), v14.D(), v15.D(), i,
           MemOperand(x20, 32, PostIndex));
  }

  // Test loading a single element into an initialised register.
  __ Mov(x25, 1);
  __ Mov(x21, src_base);
  __ Mov(x22, src_base);
  __ Mov(x23, src_base);
  __ Mov(x24, src_base);

  __ Mov(x4, x21);
  __ Ldr(q16, MemOperand(x4, 16, PostIndex));
  __ Ldr(q17, MemOperand(x4, 16, PostIndex));
  __ Ldr(q18, MemOperand(x4, 16, PostIndex));
  __ Ldr(q19, MemOperand(x4));
  __ Ld4(v16.B(), v17.B(), v18.B(), v19.B(), 4,
         MemOperand(x21, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x5, x22);
  __ Ldr(q20, MemOperand(x5, 16, PostIndex));
  __ Ldr(q21, MemOperand(x5, 16, PostIndex));
  __ Ldr(q22, MemOperand(x5, 16, PostIndex));
  __ Ldr(q23, MemOperand(x5));
  __ Ld4(v20.H(), v21.H(), v22.H(), v23.H(), 3,
         MemOperand(x22, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x6, x23);
  __ Ldr(q24, MemOperand(x6, 16, PostIndex));
  __ Ldr(q25, MemOperand(x6, 16, PostIndex));
  __ Ldr(q26, MemOperand(x6, 16, PostIndex));
  __ Ldr(q27, MemOperand(x6));
  __ Ld4(v24.S(), v25.S(), v26.S(), v27.S(), 2,
         MemOperand(x23, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Mov(x7, x24);
  __ Ldr(q28, MemOperand(x7, 16, PostIndex));
  __ Ldr(q29, MemOperand(x7, 16, PostIndex));
  __ Ldr(q30, MemOperand(x7, 16, PostIndex));
  __ Ldr(q31, MemOperand(x7));
  __ Ld4(v28.D(), v29.D(), v30.D(), v31.D(), 1,
         MemOperand(x24, x25, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x0004080C1014181C, 0x2024282C3034383C, q0);
  CHECK_EQUAL_128(0x0105090D1115191D, 0x2125292D3135393D, q1);
  CHECK_EQUAL_128(0x02060A0E12161A1E, 0x22262A2E32363A3E, q2);
  CHECK_EQUAL_128(0x03070B0F13171B1F, 0x23272B2F33373B3F, q3);
  CHECK_EQUAL_128(0x0100090811101918, 0x2120292831303938, q4);
  CHECK_EQUAL_128(0x03020B0A13121B1A, 0x23222B2A33323B3A, q5);
  CHECK_EQUAL_128(0x05040D0C15141D1C, 0x25242D2C35343D3C, q6);
  CHECK_EQUAL_128(0x07060F0E17161F1E, 0x27262F2E37363F3E, q7);
  CHECK_EQUAL_128(0x0302010013121110, 0x2322212033323130, q8);
  CHECK_EQUAL_128(0x0706050417161514, 0x2726252437363534, q9);
  CHECK_EQUAL_128(0x0B0A09081B1A1918, 0x2B2A29283B3A3938, q10);
  CHECK_EQUAL_128(0x0F0E0D0C1F1E1D1C, 0x2F2E2D2C3F3E3D3C, q11);
  CHECK_EQUAL_128(0x0706050403020100, 0x2726252423222120, q12);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x2F2E2D2C2B2A2928, q13);
  CHECK_EQUAL_128(0x1716151413121110, 0x3736353433323130, q14);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x3F3E3D3C3B3A3938, q15);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q16);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716150113121110, q17);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726250223222120, q18);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736350333323130, q19);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q20);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0302151413121110, q21);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x0504252423222120, q22);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x0706353433323130, q23);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q24);
  CHECK_EQUAL_128(0x1F1E1D1C07060504, 0x1716151413121110, q25);
  CHECK_EQUAL_128(0x2F2E2D2C0B0A0908, 0x2726252423222120, q26);
  CHECK_EQUAL_128(0x3F3E3D3C0F0E0D0C, 0x3736353433323130, q27);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q28);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x1716151413121110, q29);
  CHECK_EQUAL_128(0x1716151413121110, 0x2726252423222120, q30);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x3736353433323130, q31);

  CHECK_EQUAL_64(src_base + 64, x17);
  CHECK_EQUAL_64(src_base + 64, x18);
  CHECK_EQUAL_64(src_base + 64, x19);
  CHECK_EQUAL_64(src_base + 64, x20);
  CHECK_EQUAL_64(src_base + 1, x21);
  CHECK_EQUAL_64(src_base + 2, x22);
  CHECK_EQUAL_64(src_base + 3, x23);
  CHECK_EQUAL_64(src_base + 4, x24);

  TEARDOWN();
}

TEST(neon_ld4_alllanes) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld4r(v0.V8B(), v1.V8B(), v2.V8B(), v3.V8B(), MemOperand(x17));
  __ Add(x17, x17, 4);
  __ Ld4r(v4.V16B(), v5.V16B(), v6.V16B(), v7.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4r(v8_.V4H(), v9.V4H(), v10.V4H(), v11.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4r(v12.V8H(), v13.V8H(), v14.V8H(), v15.V8H(), MemOperand(x17));
  __ Add(x17, x17, 8);
  __ Ld4r(v16.V2S(), v17.V2S(), v18.V2S(), v19.V2S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld4r(v20.V4S(), v21.V4S(), v22.V4S(), v23.V4S(), MemOperand(x17));
  __ Add(x17, x17, 16);
  __ Ld4r(v24.V2D(), v25.V2D(), v26.V2D(), v27.V2D(), MemOperand(x17));

  END();

  RUN();

  CHECK_EQUAL_128(0x0000000000000000, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0000000000000000, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0x0000000000000000, 0x0303030303030303, q2);
  CHECK_EQUAL_128(0x0000000000000000, 0x0404040404040404, q3);
  CHECK_EQUAL_128(0x0505050505050505, 0x0505050505050505, q4);
  CHECK_EQUAL_128(0x0606060606060606, 0x0606060606060606, q5);
  CHECK_EQUAL_128(0x0707070707070707, 0x0707070707070707, q6);
  CHECK_EQUAL_128(0x0808080808080808, 0x0808080808080808, q7);
  CHECK_EQUAL_128(0x0000000000000000, 0x0706070607060706, q8);
  CHECK_EQUAL_128(0x0000000000000000, 0x0908090809080908, q9);
  CHECK_EQUAL_128(0x0000000000000000, 0x0B0A0B0A0B0A0B0A, q10);
  CHECK_EQUAL_128(0x0000000000000000, 0x0D0C0D0C0D0C0D0C, q11);
  CHECK_EQUAL_128(0x0807080708070807, 0x0807080708070807, q12);
  CHECK_EQUAL_128(0x0A090A090A090A09, 0x0A090A090A090A09, q13);
  CHECK_EQUAL_128(0x0C0B0C0B0C0B0C0B, 0x0C0B0C0B0C0B0C0B, q14);
  CHECK_EQUAL_128(0x0E0D0E0D0E0D0E0D, 0x0E0D0E0D0E0D0E0D, q15);
  CHECK_EQUAL_128(0x0000000000000000, 0x1211100F1211100F, q16);
  CHECK_EQUAL_128(0x0000000000000000, 0x1615141316151413, q17);
  CHECK_EQUAL_128(0x0000000000000000, 0x1A1918171A191817, q18);
  CHECK_EQUAL_128(0x0000000000000000, 0x1E1D1C1B1E1D1C1B, q19);
  CHECK_EQUAL_128(0x1312111013121110, 0x1312111013121110, q20);
  CHECK_EQUAL_128(0x1716151417161514, 0x1716151417161514, q21);
  CHECK_EQUAL_128(0x1B1A19181B1A1918, 0x1B1A19181B1A1918, q22);
  CHECK_EQUAL_128(0x1F1E1D1C1F1E1D1C, 0x1F1E1D1C1F1E1D1C, q23);
  CHECK_EQUAL_128(0x2726252423222120, 0x2726252423222120, q24);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2F2E2D2C2B2A2928, q25);
  CHECK_EQUAL_128(0x3736353433323130, 0x3736353433323130, q26);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3F3E3D3C3B3A3938, q27);

  TEARDOWN();
}

TEST(neon_ld4_alllanes_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld4r(v0.V8B(), v1.V8B(), v2.V8B(), v3.V8B(),
          MemOperand(x17, 4, PostIndex));
  __ Ld4r(v4.V16B(), v5.V16B(), v6.V16B(), v7.V16B(),
          MemOperand(x17, x18, PostIndex));
  __ Ld4r(v8_.V4H(), v9.V4H(), v10.V4H(), v11.V4H(),
          MemOperand(x17, x18, PostIndex));
  __ Ld4r(v12.V8H(), v13.V8H(), v14.V8H(), v15.V8H(),
          MemOperand(x17, 8, PostIndex));
  __ Ld4r(v16.V2S(), v17.V2S(), v18.V2S(), v19.V2S(),
          MemOperand(x17, x18, PostIndex));
  __ Ld4r(v20.V4S(), v21.V4S(), v22.V4S(), v23.V4S(),
          MemOperand(x17, 16, PostIndex));
  __ Ld4r(v24.V2D(), v25.V2D(), v26.V2D(), v27.V2D(),
          MemOperand(x17, 32, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0x0000000000000000, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0000000000000000, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0x0000000000000000, 0x0303030303030303, q2);
  CHECK_EQUAL_128(0x0000000000000000, 0x0404040404040404, q3);
  CHECK_EQUAL_128(0x0505050505050505, 0x0505050505050505, q4);
  CHECK_EQUAL_128(0x0606060606060606, 0x0606060606060606, q5);
  CHECK_EQUAL_128(0x0707070707070707, 0x0707070707070707, q6);
  CHECK_EQUAL_128(0x0808080808080808, 0x0808080808080808, q7);
  CHECK_EQUAL_128(0x0000000000000000, 0x0706070607060706, q8);
  CHECK_EQUAL_128(0x0000000000000000, 0x0908090809080908, q9);
  CHECK_EQUAL_128(0x0000000000000000, 0x0B0A0B0A0B0A0B0A, q10);
  CHECK_EQUAL_128(0x0000000000000000, 0x0D0C0D0C0D0C0D0C, q11);
  CHECK_EQUAL_128(0x0807080708070807, 0x0807080708070807, q12);
  CHECK_EQUAL_128(0x0A090A090A090A09, 0x0A090A090A090A09, q13);
  CHECK_EQUAL_128(0x0C0B0C0B0C0B0C0B, 0x0C0B0C0B0C0B0C0B, q14);
  CHECK_EQUAL_128(0x0E0D0E0D0E0D0E0D, 0x0E0D0E0D0E0D0E0D, q15);
  CHECK_EQUAL_128(0x0000000000000000, 0x1211100F1211100F, q16);
  CHECK_EQUAL_128(0x0000000000000000, 0x1615141316151413, q17);
  CHECK_EQUAL_128(0x0000000000000000, 0x1A1918171A191817, q18);
  CHECK_EQUAL_128(0x0000000000000000, 0x1E1D1C1B1E1D1C1B, q19);
  CHECK_EQUAL_128(0x1312111013121110, 0x1312111013121110, q20);
  CHECK_EQUAL_128(0x1716151417161514, 0x1716151417161514, q21);
  CHECK_EQUAL_128(0x1B1A19181B1A1918, 0x1B1A19181B1A1918, q22);
  CHECK_EQUAL_128(0x1F1E1D1C1F1E1D1C, 0x1F1E1D1C1F1E1D1C, q23);
  CHECK_EQUAL_128(0x2726252423222120, 0x2726252423222120, q24);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2F2E2D2C2B2A2928, q25);
  CHECK_EQUAL_128(0x3736353433323130, 0x3736353433323130, q26);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3F3E3D3C3B3A3938, q27);
  CHECK_EQUAL_64(src_base + 64, x17);

  TEARDOWN();
}

TEST(neon_st1_lane) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, -16);
  __ Ldr(q0, MemOperand(x17));

  for (int i = 15; i >= 0; i--) {
    __ St1(v0.B(), i, MemOperand(x17));
    __ Add(x17, x17, 1);
  }
  __ Ldr(q1, MemOperand(x17, x18));

  for (int i = 7; i >= 0; i--) {
    __ St1(v0.H(), i, MemOperand(x17));
    __ Add(x17, x17, 2);
  }
  __ Ldr(q2, MemOperand(x17, x18));

  for (int i = 3; i >= 0; i--) {
    __ St1(v0.S(), i, MemOperand(x17));
    __ Add(x17, x17, 4);
  }
  __ Ldr(q3, MemOperand(x17, x18));

  for (int i = 1; i >= 0; i--) {
    __ St1(v0.D(), i, MemOperand(x17));
    __ Add(x17, x17, 8);
  }
  __ Ldr(q4, MemOperand(x17, x18));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q1);
  CHECK_EQUAL_128(0x0100030205040706, 0x09080B0A0D0C0F0E, q2);
  CHECK_EQUAL_128(0x0302010007060504, 0x0B0A09080F0E0D0C, q3);
  CHECK_EQUAL_128(0x0706050403020100, 0x0F0E0D0C0B0A0908, q4);

  TEARDOWN();
}

TEST(neon_st2_lane) {
  INIT_V8();
  SETUP();

  // Struct size * addressing modes * element sizes * vector size.
  uint8_t dst[2 * 2 * 4 * 16];
  memset(dst, 0, sizeof(dst));
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, dst_base);
  __ Mov(x18, dst_base);
  __ Movi(v0.V2D(), 0x0001020304050607, 0x08090A0B0C0D0E0F);
  __ Movi(v1.V2D(), 0x1011121314151617, 0x18191A1B1C1D1E1F);

  // Test B stores with and without post index.
  for (int i = 15; i >= 0; i--) {
    __ St2(v0.B(), v1.B(), i, MemOperand(x18));
    __ Add(x18, x18, 2);
  }
  for (int i = 15; i >= 0; i--) {
    __ St2(v0.B(), v1.B(), i, MemOperand(x18, 2, PostIndex));
  }
  __ Ldr(q2, MemOperand(x17, 0 * 16));
  __ Ldr(q3, MemOperand(x17, 1 * 16));
  __ Ldr(q4, MemOperand(x17, 2 * 16));
  __ Ldr(q5, MemOperand(x17, 3 * 16));

  // Test H stores with and without post index.
  __ Mov(x0, 4);
  for (int i = 7; i >= 0; i--) {
    __ St2(v0.H(), v1.H(), i, MemOperand(x18));
    __ Add(x18, x18, 4);
  }
  for (int i = 7; i >= 0; i--) {
    __ St2(v0.H(), v1.H(), i, MemOperand(x18, x0, PostIndex));
  }
  __ Ldr(q6, MemOperand(x17, 4 * 16));
  __ Ldr(q7, MemOperand(x17, 5 * 16));
  __ Ldr(q16, MemOperand(x17, 6 * 16));
  __ Ldr(q17, MemOperand(x17, 7 * 16));

  // Test S stores with and without post index.
  for (int i = 3; i >= 0; i--) {
    __ St2(v0.S(), v1.S(), i, MemOperand(x18));
    __ Add(x18, x18, 8);
  }
  for (int i = 3; i >= 0; i--) {
    __ St2(v0.S(), v1.S(), i, MemOperand(x18, 8, PostIndex));
  }
  __ Ldr(q18, MemOperand(x17, 8 * 16));
  __ Ldr(q19, MemOperand(x17, 9 * 16));
  __ Ldr(q20, MemOperand(x17, 10 * 16));
  __ Ldr(q21, MemOperand(x17, 11 * 16));

  // Test D stores with and without post index.
  __ Mov(x0, 16);
  __ St2(v0.D(), v1.D(), 1, MemOperand(x18));
  __ Add(x18, x18, 16);
  __ St2(v0.D(), v1.D(), 0, MemOperand(x18, 16, PostIndex));
  __ St2(v0.D(), v1.D(), 1, MemOperand(x18, x0, PostIndex));
  __ St2(v0.D(), v1.D(), 0, MemOperand(x18, x0, PostIndex));
  __ Ldr(q22, MemOperand(x17, 12 * 16));
  __ Ldr(q23, MemOperand(x17, 13 * 16));
  __ Ldr(q24, MemOperand(x17, 14 * 16));
  __ Ldr(q25, MemOperand(x17, 15 * 16));
  END();

  RUN();

  CHECK_EQUAL_128(0x1707160615051404, 0x1303120211011000, q2);
  CHECK_EQUAL_128(0x1F0F1E0E1D0D1C0C, 0x1B0B1A0A19091808, q3);
  CHECK_EQUAL_128(0x1707160615051404, 0x1303120211011000, q4);
  CHECK_EQUAL_128(0x1F0F1E0E1D0D1C0C, 0x1B0B1A0A19091808, q5);

  CHECK_EQUAL_128(0x1617060714150405, 0x1213020310110001, q6);
  CHECK_EQUAL_128(0x1E1F0E0F1C1D0C0D, 0x1A1B0A0B18190809, q7);
  CHECK_EQUAL_128(0x1617060714150405, 0x1213020310110001, q16);
  CHECK_EQUAL_128(0x1E1F0E0F1C1D0C0D, 0x1A1B0A0B18190809, q17);

  CHECK_EQUAL_128(0x1415161704050607, 0x1011121300010203, q18);
  CHECK_EQUAL_128(0x1C1D1E1F0C0D0E0F, 0x18191A1B08090A0B, q19);
  CHECK_EQUAL_128(0x1415161704050607, 0x1011121300010203, q20);
  CHECK_EQUAL_128(0x1C1D1E1F0C0D0E0F, 0x18191A1B08090A0B, q21);

  CHECK_EQUAL_128(0x1011121314151617, 0x0001020304050607, q22);
  CHECK_EQUAL_128(0x18191A1B1C1D1E1F, 0x08090A0B0C0D0E0F, q23);
  CHECK_EQUAL_128(0x1011121314151617, 0x0001020304050607, q22);
  CHECK_EQUAL_128(0x18191A1B1C1D1E1F, 0x08090A0B0C0D0E0F, q23);

  TEARDOWN();
}

TEST(neon_st3_lane) {
  INIT_V8();
  SETUP();

  // Struct size * addressing modes * element sizes * vector size.
  uint8_t dst[3 * 2 * 4 * 16];
  memset(dst, 0, sizeof(dst));
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, dst_base);
  __ Mov(x18, dst_base);
  __ Movi(v0.V2D(), 0x0001020304050607, 0x08090A0B0C0D0E0F);
  __ Movi(v1.V2D(), 0x1011121314151617, 0x18191A1B1C1D1E1F);
  __ Movi(v2.V2D(), 0x2021222324252627, 0x28292A2B2C2D2E2F);

  // Test B stores with and without post index.
  for (int i = 15; i >= 0; i--) {
    __ St3(v0.B(), v1.B(), v2.B(), i, MemOperand(x18));
    __ Add(x18, x18, 3);
  }
  for (int i = 15; i >= 0; i--) {
    __ St3(v0.B(), v1.B(), v2.B(), i, MemOperand(x18, 3, PostIndex));
  }
  __ Ldr(q3, MemOperand(x17, 0 * 16));
  __ Ldr(q4, MemOperand(x17, 1 * 16));
  __ Ldr(q5, MemOperand(x17, 2 * 16));
  __ Ldr(q6, MemOperand(x17, 3 * 16));
  __ Ldr(q7, MemOperand(x17, 4 * 16));
  __ Ldr(q16, MemOperand(x17, 5 * 16));

  // Test H stores with and without post index.
  __ Mov(x0, 6);
  for (int i = 7; i >= 0; i--) {
    __ St3(v0.H(), v1.H(), v2.H(), i, MemOperand(x18));
    __ Add(x18, x18, 6);
  }
  for (int i = 7; i >= 0; i--) {
    __ St3(v0.H(), v1.H(), v2.H(), i, MemOperand(x18, x0, PostIndex));
  }
  __ Ldr(q17, MemOperand(x17, 6 * 16));
  __ Ldr(q18, MemOperand(x17, 7 * 16));
  __ Ldr(q19, MemOperand(x17, 8 * 16));
  __ Ldr(q20, MemOperand(x17, 9 * 16));
  __ Ldr(q21, MemOperand(x17, 10 * 16));
  __ Ldr(q22, MemOperand(x17, 11 * 16));

  // Test S stores with and without post index.
  for (int i = 3; i >= 0; i--) {
    __ St3(v0.S(), v1.S(), v2.S(), i, MemOperand(x18));
    __ Add(x18, x18, 12);
  }
  for (int i = 3; i >= 0; i--) {
    __ St3(v0.S(), v1.S(), v2.S(), i, MemOperand(x18, 12, PostIndex));
  }
  __ Ldr(q23, MemOperand(x17, 12 * 16));
  __ Ldr(q24, MemOperand(x17, 13 * 16));
  __ Ldr(q25, MemOperand(x17, 14 * 16));
  __ Ldr(q26, MemOperand(x17, 15 * 16));
  __ Ldr(q27, MemOperand(x17, 16 * 16));
  __ Ldr(q28, MemOperand(x17, 17 * 16));

  // Test D stores with and without post index.
  __ Mov(x0, 24);
  __ St3(v0.D(), v1.D(), v2.D(), 1, MemOperand(x18));
  __ Add(x18, x18, 24);
  __ St3(v0.D(), v1.D(), v2.D(), 0, MemOperand(x18, 24, PostIndex));
  __ St3(v0.D(), v1.D(), v2.D(), 1, MemOperand(x18, x0, PostIndex));
  __ Ldr(q29, MemOperand(x17, 18 * 16));
  __ Ldr(q30, MemOperand(x17, 19 * 16));
  __ Ldr(q31, MemOperand(x17, 20 * 16));
  END();

  RUN();

  CHECK_EQUAL_128(0x0524140423130322, 0x1202211101201000, q3);
  CHECK_EQUAL_128(0x1A0A291909281808, 0x2717072616062515, q4);
  CHECK_EQUAL_128(0x2F1F0F2E1E0E2D1D, 0x0D2C1C0C2B1B0B2A, q5);
  CHECK_EQUAL_128(0x0524140423130322, 0x1202211101201000, q6);
  CHECK_EQUAL_128(0x1A0A291909281808, 0x2717072616062515, q7);
  CHECK_EQUAL_128(0x2F1F0F2E1E0E2D1D, 0x0D2C1C0C2B1B0B2A, q16);

  CHECK_EQUAL_128(0x1415040522231213, 0x0203202110110001, q17);
  CHECK_EQUAL_128(0x0A0B282918190809, 0x2627161706072425, q18);
  CHECK_EQUAL_128(0x2E2F1E1F0E0F2C2D, 0x1C1D0C0D2A2B1A1B, q19);
  CHECK_EQUAL_128(0x1415040522231213, 0x0203202110110001, q20);
  CHECK_EQUAL_128(0x0A0B282918190809, 0x2627161706072425, q21);
  CHECK_EQUAL_128(0x2E2F1E1F0E0F2C2D, 0x1C1D0C0D2A2B1A1B, q22);

  CHECK_EQUAL_128(0x0405060720212223, 0x1011121300010203, q23);
  CHECK_EQUAL_128(0x18191A1B08090A0B, 0x2425262714151617, q24);
  CHECK_EQUAL_128(0x2C2D2E2F1C1D1E1F, 0x0C0D0E0F28292A2B, q25);
  CHECK_EQUAL_128(0x0405060720212223, 0x1011121300010203, q26);
  CHECK_EQUAL_128(0x18191A1B08090A0B, 0x2425262714151617, q27);
  CHECK_EQUAL_128(0x2C2D2E2F1C1D1E1F, 0x0C0D0E0F28292A2B, q28);

  TEARDOWN();
}

TEST(neon_st4_lane) {
  INIT_V8();
  SETUP();

  // Struct size * element sizes * vector size.
  uint8_t dst[4 * 4 * 16];
  memset(dst, 0, sizeof(dst));
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x17, dst_base);
  __ Mov(x18, dst_base);
  __ Movi(v0.V2D(), 0x0001020304050607, 0x08090A0B0C0D0E0F);
  __ Movi(v1.V2D(), 0x1011121314151617, 0x18191A1B1C1D1E1F);
  __ Movi(v2.V2D(), 0x2021222324252627, 0x28292A2B2C2D2E2F);
  __ Movi(v3.V2D(), 0x2021222324252627, 0x28292A2B2C2D2E2F);

  // Test B stores without post index.
  for (int i = 15; i >= 0; i--) {
    __ St4(v0.B(), v1.B(), v2.B(), v3.B(), i, MemOperand(x18));
    __ Add(x18, x18, 4);
  }
  __ Ldr(q4, MemOperand(x17, 0 * 16));
  __ Ldr(q5, MemOperand(x17, 1 * 16));
  __ Ldr(q6, MemOperand(x17, 2 * 16));
  __ Ldr(q7, MemOperand(x17, 3 * 16));

  // Test H stores with post index.
  __ Mov(x0, 8);
  for (int i = 7; i >= 0; i--) {
    __ St4(v0.H(), v1.H(), v2.H(), v3.H(), i, MemOperand(x18, x0, PostIndex));
  }
  __ Ldr(q16, MemOperand(x17, 4 * 16));
  __ Ldr(q17, MemOperand(x17, 5 * 16));
  __ Ldr(q18, MemOperand(x17, 6 * 16));
  __ Ldr(q19, MemOperand(x17, 7 * 16));

  // Test S stores without post index.
  for (int i = 3; i >= 0; i--) {
    __ St4(v0.S(), v1.S(), v2.S(), v3.S(), i, MemOperand(x18));
    __ Add(x18, x18, 16);
  }
  __ Ldr(q20, MemOperand(x17, 8 * 16));
  __ Ldr(q21, MemOperand(x17, 9 * 16));
  __ Ldr(q22, MemOperand(x17, 10 * 16));
  __ Ldr(q23, MemOperand(x17, 11 * 16));

  // Test D stores with post index.
  __ Mov(x0, 32);
  __ St4(v0.D(), v1.D(), v2.D(), v3.D(), 0, MemOperand(x18, 32, PostIndex));
  __ St4(v0.D(), v1.D(), v2.D(), v3.D(), 1, MemOperand(x18, x0, PostIndex));

  __ Ldr(q24, MemOperand(x17, 12 * 16));
  __ Ldr(q25, MemOperand(x17, 13 * 16));
  __ Ldr(q26, MemOperand(x17, 14 * 16));
  __ Ldr(q27, MemOperand(x17, 15 * 16));
  END();

  RUN();

  CHECK_EQUAL_128(0x2323130322221202, 0x2121110120201000, q4);
  CHECK_EQUAL_128(0x2727170726261606, 0x2525150524241404, q5);
  CHECK_EQUAL_128(0x2B2B1B0B2A2A1A0A, 0x2929190928281808, q6);
  CHECK_EQUAL_128(0x2F2F1F0F2E2E1E0E, 0x2D2D1D0D2C2C1C0C, q7);

  CHECK_EQUAL_128(0x2223222312130203, 0x2021202110110001, q16);
  CHECK_EQUAL_128(0x2627262716170607, 0x2425242514150405, q17);
  CHECK_EQUAL_128(0x2A2B2A2B1A1B0A0B, 0x2829282918190809, q18);
  CHECK_EQUAL_128(0x2E2F2E2F1E1F0E0F, 0x2C2D2C2D1C1D0C0D, q19);

  CHECK_EQUAL_128(0x2021222320212223, 0x1011121300010203, q20);
  CHECK_EQUAL_128(0x2425262724252627, 0x1415161704050607, q21);
  CHECK_EQUAL_128(0x28292A2B28292A2B, 0x18191A1B08090A0B, q22);
  CHECK_EQUAL_128(0x2C2D2E2F2C2D2E2F, 0x1C1D1E1F0C0D0E0F, q23);

  CHECK_EQUAL_128(0x18191A1B1C1D1E1F, 0x08090A0B0C0D0E0F, q24);
  CHECK_EQUAL_128(0x28292A2B2C2D2E2F, 0x28292A2B2C2D2E2F, q25);
  CHECK_EQUAL_128(0x1011121314151617, 0x0001020304050607, q26);
  CHECK_EQUAL_128(0x2021222324252627, 0x2021222324252627, q27);

  TEARDOWN();
}

TEST(neon_ld1_lane_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Mov(x19, src_base);
  __ Mov(x20, src_base);
  __ Mov(x21, src_base);
  __ Mov(x22, src_base);
  __ Mov(x23, src_base);
  __ Mov(x24, src_base);

  // Test loading whole register by element.
  for (int i = 15; i >= 0; i--) {
    __ Ld1(v0.B(), i, MemOperand(x17, 1, PostIndex));
  }

  for (int i = 7; i >= 0; i--) {
    __ Ld1(v1.H(), i, MemOperand(x18, 2, PostIndex));
  }

  for (int i = 3; i >= 0; i--) {
    __ Ld1(v2.S(), i, MemOperand(x19, 4, PostIndex));
  }

  for (int i = 1; i >= 0; i--) {
    __ Ld1(v3.D(), i, MemOperand(x20, 8, PostIndex));
  }

  // Test loading a single element into an initialised register.
  __ Mov(x25, 1);
  __ Ldr(q4, MemOperand(x21));
  __ Ld1(v4.B(), 4, MemOperand(x21, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Ldr(q5, MemOperand(x22));
  __ Ld1(v5.H(), 3, MemOperand(x22, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Ldr(q6, MemOperand(x23));
  __ Ld1(v6.S(), 2, MemOperand(x23, x25, PostIndex));
  __ Add(x25, x25, 1);

  __ Ldr(q7, MemOperand(x24));
  __ Ld1(v7.D(), 1, MemOperand(x24, x25, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q0);
  CHECK_EQUAL_128(0x0100030205040706, 0x09080B0A0D0C0F0E, q1);
  CHECK_EQUAL_128(0x0302010007060504, 0x0B0A09080F0E0D0C, q2);
  CHECK_EQUAL_128(0x0706050403020100, 0x0F0E0D0C0B0A0908, q3);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050003020100, q4);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0100050403020100, q5);
  CHECK_EQUAL_128(0x0F0E0D0C03020100, 0x0706050403020100, q6);
  CHECK_EQUAL_128(0x0706050403020100, 0x0706050403020100, q7);
  CHECK_EQUAL_64(src_base + 16, x17);
  CHECK_EQUAL_64(src_base + 16, x18);
  CHECK_EQUAL_64(src_base + 16, x19);
  CHECK_EQUAL_64(src_base + 16, x20);
  CHECK_EQUAL_64(src_base + 1, x21);
  CHECK_EQUAL_64(src_base + 2, x22);
  CHECK_EQUAL_64(src_base + 3, x23);
  CHECK_EQUAL_64(src_base + 4, x24);

  TEARDOWN();
}

TEST(neon_st1_lane_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, -16);
  __ Ldr(q0, MemOperand(x17));

  for (int i = 15; i >= 0; i--) {
    __ St1(v0.B(), i, MemOperand(x17, 1, PostIndex));
  }
  __ Ldr(q1, MemOperand(x17, x18));

  for (int i = 7; i >= 0; i--) {
    __ St1(v0.H(), i, MemOperand(x17, 2, PostIndex));
  }
  __ Ldr(q2, MemOperand(x17, x18));

  for (int i = 3; i >= 0; i--) {
    __ St1(v0.S(), i, MemOperand(x17, 4, PostIndex));
  }
  __ Ldr(q3, MemOperand(x17, x18));

  for (int i = 1; i >= 0; i--) {
    __ St1(v0.D(), i, MemOperand(x17, 8, PostIndex));
  }
  __ Ldr(q4, MemOperand(x17, x18));

  END();

  RUN();

  CHECK_EQUAL_128(0x0001020304050607, 0x08090A0B0C0D0E0F, q1);
  CHECK_EQUAL_128(0x0100030205040706, 0x09080B0A0D0C0F0E, q2);
  CHECK_EQUAL_128(0x0302010007060504, 0x0B0A09080F0E0D0C, q3);
  CHECK_EQUAL_128(0x0706050403020100, 0x0F0E0D0C0B0A0908, q4);

  TEARDOWN();
}

TEST(neon_ld1_alllanes) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base + 1);
  __ Ld1r(v0.V8B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v1.V16B(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v2.V4H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v3.V8H(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v4.V2S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v5.V4S(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v6.V1D(), MemOperand(x17));
  __ Add(x17, x17, 1);
  __ Ld1r(v7.V2D(), MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0202020202020202, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0, 0x0403040304030403, q2);
  CHECK_EQUAL_128(0x0504050405040504, 0x0504050405040504, q3);
  CHECK_EQUAL_128(0, 0x0807060508070605, q4);
  CHECK_EQUAL_128(0x0908070609080706, 0x0908070609080706, q5);
  CHECK_EQUAL_128(0, 0x0E0D0C0B0A090807, q6);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0F0E0D0C0B0A0908, q7);

  TEARDOWN();
}

TEST(neon_ld1_alllanes_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base + 1);
  __ Mov(x18, 1);
  __ Ld1r(v0.V8B(), MemOperand(x17, 1, PostIndex));
  __ Ld1r(v1.V16B(), MemOperand(x17, x18, PostIndex));
  __ Ld1r(v2.V4H(), MemOperand(x17, x18, PostIndex));
  __ Ld1r(v3.V8H(), MemOperand(x17, 2, PostIndex));
  __ Ld1r(v4.V2S(), MemOperand(x17, x18, PostIndex));
  __ Ld1r(v5.V4S(), MemOperand(x17, 4, PostIndex));
  __ Ld1r(v6.V2D(), MemOperand(x17, 8, PostIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0101010101010101, q0);
  CHECK_EQUAL_128(0x0202020202020202, 0x0202020202020202, q1);
  CHECK_EQUAL_128(0, 0x0403040304030403, q2);
  CHECK_EQUAL_128(0x0504050405040504, 0x0504050405040504, q3);
  CHECK_EQUAL_128(0, 0x0908070609080706, q4);
  CHECK_EQUAL_128(0x0A0908070A090807, 0x0A0908070A090807, q5);
  CHECK_EQUAL_128(0x1211100F0E0D0C0B, 0x1211100F0E0D0C0B, q6);
  CHECK_EQUAL_64(src_base + 19, x17);

  TEARDOWN();
}

TEST(neon_st1_d) {
  INIT_V8();
  SETUP();

  uint8_t src[14 * kDRegSize];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));
  __ Mov(x17, src_base);

  __ St1(v0.V8B(), MemOperand(x17));
  __ Ldr(d16, MemOperand(x17, 8, PostIndex));

  __ St1(v0.V8B(), v1.V8B(), MemOperand(x17));
  __ Ldr(q17, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V4H(), v1.V4H(), v2.V4H(), MemOperand(x17));
  __ Ldr(d18, MemOperand(x17, 8, PostIndex));
  __ Ldr(d19, MemOperand(x17, 8, PostIndex));
  __ Ldr(d20, MemOperand(x17, 8, PostIndex));

  __ St1(v0.V2S(), v1.V2S(), v2.V2S(), v3.V2S(), MemOperand(x17));
  __ Ldr(q21, MemOperand(x17, 16, PostIndex));
  __ Ldr(q22, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V1D(), v1.V1D(), v2.V1D(), v3.V1D(), MemOperand(x17));
  __ Ldr(q23, MemOperand(x17, 16, PostIndex));
  __ Ldr(q24, MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q0);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q1);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726252423222120, q2);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736353433323130, q3);
  CHECK_EQUAL_128(0, 0x0706050403020100, q16);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q17);
  CHECK_EQUAL_128(0, 0x0706050403020100, q18);
  CHECK_EQUAL_128(0, 0x1716151413121110, q19);
  CHECK_EQUAL_128(0, 0x2726252423222120, q20);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q21);
  CHECK_EQUAL_128(0x3736353433323130, 0x2726252423222120, q22);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q23);
  CHECK_EQUAL_128(0x3736353433323130, 0x2726252423222120, q24);

  TEARDOWN();
}

TEST(neon_st1_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 14 * kDRegSize];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, -8);
  __ Mov(x19, -16);
  __ Mov(x20, -24);
  __ Mov(x21, -32);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));
  __ Mov(x17, src_base);

  __ St1(v0.V8B(), MemOperand(x17, 8, PostIndex));
  __ Ldr(d16, MemOperand(x17, x18));

  __ St1(v0.V8B(), v1.V8B(), MemOperand(x17, 16, PostIndex));
  __ Ldr(q17, MemOperand(x17, x19));

  __ St1(v0.V4H(), v1.V4H(), v2.V4H(), MemOperand(x17, 24, PostIndex));
  __ Ldr(d18, MemOperand(x17, x20));
  __ Ldr(d19, MemOperand(x17, x19));
  __ Ldr(d20, MemOperand(x17, x18));

  __ St1(v0.V2S(), v1.V2S(), v2.V2S(), v3.V2S(),
         MemOperand(x17, 32, PostIndex));
  __ Ldr(q21, MemOperand(x17, x21));
  __ Ldr(q22, MemOperand(x17, x19));

  __ St1(v0.V1D(), v1.V1D(), v2.V1D(), v3.V1D(),
         MemOperand(x17, 32, PostIndex));
  __ Ldr(q23, MemOperand(x17, x21));
  __ Ldr(q24, MemOperand(x17, x19));
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0706050403020100, q16);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q17);
  CHECK_EQUAL_128(0, 0x0706050403020100, q18);
  CHECK_EQUAL_128(0, 0x1716151413121110, q19);
  CHECK_EQUAL_128(0, 0x2726252423222120, q20);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q21);
  CHECK_EQUAL_128(0x3736353433323130, 0x2726252423222120, q22);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q23);
  CHECK_EQUAL_128(0x3736353433323130, 0x2726252423222120, q24);

  TEARDOWN();
}

TEST(neon_st1_q) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 160];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V16B(), MemOperand(x17));
  __ Ldr(q16, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V8H(), v1.V8H(), MemOperand(x17));
  __ Ldr(q17, MemOperand(x17, 16, PostIndex));
  __ Ldr(q18, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V4S(), v1.V4S(), v2.V4S(), MemOperand(x17));
  __ Ldr(q19, MemOperand(x17, 16, PostIndex));
  __ Ldr(q20, MemOperand(x17, 16, PostIndex));
  __ Ldr(q21, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V2D(), v1.V2D(), v2.V2D(), v3.V2D(), MemOperand(x17));
  __ Ldr(q22, MemOperand(x17, 16, PostIndex));
  __ Ldr(q23, MemOperand(x17, 16, PostIndex));
  __ Ldr(q24, MemOperand(x17, 16, PostIndex));
  __ Ldr(q25, MemOperand(x17));
  END();

  RUN();

  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q16);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q17);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q18);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q19);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q20);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726252423222120, q21);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q22);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q23);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726252423222120, q24);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736353433323130, q25);

  TEARDOWN();
}

TEST(neon_st1_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[64 + 160];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, -16);
  __ Mov(x19, -32);
  __ Mov(x20, -48);
  __ Mov(x21, -64);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));

  __ St1(v0.V16B(), MemOperand(x17, 16, PostIndex));
  __ Ldr(q16, MemOperand(x17, x18));

  __ St1(v0.V8H(), v1.V8H(), MemOperand(x17, 32, PostIndex));
  __ Ldr(q17, MemOperand(x17, x19));
  __ Ldr(q18, MemOperand(x17, x18));

  __ St1(v0.V4S(), v1.V4S(), v2.V4S(), MemOperand(x17, 48, PostIndex));
  __ Ldr(q19, MemOperand(x17, x20));
  __ Ldr(q20, MemOperand(x17, x19));
  __ Ldr(q21, MemOperand(x17, x18));

  __ St1(v0.V2D(), v1.V2D(), v2.V2D(), v3.V2D(),
         MemOperand(x17, 64, PostIndex));
  __ Ldr(q22, MemOperand(x17, x21));
  __ Ldr(q23, MemOperand(x17, x20));
  __ Ldr(q24, MemOperand(x17, x19));
  __ Ldr(q25, MemOperand(x17, x18));

  END();

  RUN();

  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q16);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q17);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q18);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q19);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q20);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726252423222120, q21);
  CHECK_EQUAL_128(0x0F0E0D0C0B0A0908, 0x0706050403020100, q22);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x1716151413121110, q23);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726252423222120, q24);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736353433323130, q25);

  TEARDOWN();
}

TEST(neon_st2_d) {
  INIT_V8();
  SETUP();

  uint8_t src[4 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));

  __ St2(v0.V8B(), v1.V8B(), MemOperand(x18));
  __ Add(x18, x18, 22);
  __ St2(v0.V4H(), v1.V4H(), MemOperand(x18));
  __ Add(x18, x18, 11);
  __ St2(v0.V2S(), v1.V2S(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1707160615051404, 0x1303120211011000, q0);
  CHECK_EQUAL_128(0x0504131203021110, 0x0100151413121110, q1);
  CHECK_EQUAL_128(0x1615140706050413, 0x1211100302010014, q2);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736353433323117, q3);

  TEARDOWN();
}

TEST(neon_st2_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[4 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x22, 5);
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));

  __ St2(v0.V8B(), v1.V8B(), MemOperand(x18, x22, PostIndex));
  __ St2(v0.V4H(), v1.V4H(), MemOperand(x18, 16, PostIndex));
  __ St2(v0.V2S(), v1.V2S(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1405041312030211, 0x1001000211011000, q0);
  CHECK_EQUAL_128(0x0605041312111003, 0x0201001716070615, q1);
  CHECK_EQUAL_128(0x2F2E2D2C2B2A2928, 0x2726251716151407, q2);

  TEARDOWN();
}

TEST(neon_st2_q) {
  INIT_V8();
  SETUP();

  uint8_t src[5 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));

  __ St2(v0.V16B(), v1.V16B(), MemOperand(x18));
  __ Add(x18, x18, 8);
  __ St2(v0.V8H(), v1.V8H(), MemOperand(x18));
  __ Add(x18, x18, 22);
  __ St2(v0.V4S(), v1.V4S(), MemOperand(x18));
  __ Add(x18, x18, 2);
  __ St2(v0.V2D(), v1.V2D(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1312030211100100, 0x1303120211011000, q0);
  CHECK_EQUAL_128(0x01000B0A19180908, 0x1716070615140504, q1);
  CHECK_EQUAL_128(0x1716151413121110, 0x0706050403020100, q2);
  CHECK_EQUAL_128(0x1F1E1D1C1B1A1918, 0x0F0E0D0C0B0A0908, q3);
  TEARDOWN();
}

TEST(neon_st2_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[5 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x22, 5);
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));

  __ St2(v0.V16B(), v1.V16B(), MemOperand(x18, x22, PostIndex));
  __ St2(v0.V8H(), v1.V8H(), MemOperand(x18, 32, PostIndex));
  __ St2(v0.V4S(), v1.V4S(), MemOperand(x18, x22, PostIndex));
  __ St2(v0.V2D(), v1.V2D(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));
  __ Ldr(q4, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1405041312030211, 0x1001000211011000, q0);
  CHECK_EQUAL_128(0x1C0D0C1B1A0B0A19, 0x1809081716070615, q1);
  CHECK_EQUAL_128(0x0504030201001003, 0x0201001F1E0F0E1D, q2);
  CHECK_EQUAL_128(0x0D0C0B0A09081716, 0x1514131211100706, q3);
  CHECK_EQUAL_128(0x4F4E4D4C4B4A1F1E, 0x1D1C1B1A19180F0E, q4);

  TEARDOWN();
}

TEST(neon_st3_d) {
  INIT_V8();
  SETUP();

  uint8_t src[3 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));

  __ St3(v0.V8B(), v1.V8B(), v2.V8B(), MemOperand(x18));
  __ Add(x18, x18, 3);
  __ St3(v0.V4H(), v1.V4H(), v2.V4H(), MemOperand(x18));
  __ Add(x18, x18, 2);
  __ St3(v0.V2S(), v1.V2S(), v2.V2S(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x2221201312111003, 0x0201000100201000, q0);
  CHECK_EQUAL_128(0x1F1E1D2726252417, 0x1615140706050423, q1);

  TEARDOWN();
}

TEST(neon_st3_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[4 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x22, 5);
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));

  __ St3(v0.V8B(), v1.V8B(), v2.V8B(), MemOperand(x18, x22, PostIndex));
  __ St3(v0.V4H(), v1.V4H(), v2.V4H(), MemOperand(x18, 24, PostIndex));
  __ St3(v0.V2S(), v1.V2S(), v2.V2S(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x2213120302212011, 0x1001001101201000, q0);
  CHECK_EQUAL_128(0x0201002726171607, 0x0625241514050423, q1);
  CHECK_EQUAL_128(0x1615140706050423, 0x2221201312111003, q2);
  CHECK_EQUAL_128(0x3F3E3D3C3B3A3938, 0x3736352726252417, q3);

  TEARDOWN();
}

TEST(neon_st3_q) {
  INIT_V8();
  SETUP();

  uint8_t src[6 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));

  __ St3(v0.V16B(), v1.V16B(), v2.V16B(), MemOperand(x18));
  __ Add(x18, x18, 5);
  __ St3(v0.V8H(), v1.V8H(), v2.V8H(), MemOperand(x18));
  __ Add(x18, x18, 12);
  __ St3(v0.V4S(), v1.V4S(), v2.V4S(), MemOperand(x18));
  __ Add(x18, x18, 22);
  __ St3(v0.V2D(), v1.V2D(), v2.V2D(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));
  __ Ldr(q4, MemOperand(x19, 16, PostIndex));
  __ Ldr(q5, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x2213120302212011, 0x1001001101201000, q0);
  CHECK_EQUAL_128(0x0605042322212013, 0x1211100302010023, q1);
  CHECK_EQUAL_128(0x1007060504030201, 0x0025241716151407, q2);
  CHECK_EQUAL_128(0x0827262524232221, 0x2017161514131211, q3);
  CHECK_EQUAL_128(0x281F1E1D1C1B1A19, 0x180F0E0D0C0B0A09, q4);
  CHECK_EQUAL_128(0x5F5E5D5C5B5A5958, 0x572F2E2D2C2B2A29, q5);

  TEARDOWN();
}

TEST(neon_st3_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[7 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x22, 5);
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));

  __ St3(v0.V16B(), v1.V16B(), v2.V16B(), MemOperand(x18, x22, PostIndex));
  __ St3(v0.V8H(), v1.V8H(), v2.V8H(), MemOperand(x18, 48, PostIndex));
  __ St3(v0.V4S(), v1.V4S(), v2.V4S(), MemOperand(x18, x22, PostIndex));
  __ St3(v0.V2D(), v1.V2D(), v2.V2D(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));
  __ Ldr(q4, MemOperand(x19, 16, PostIndex));
  __ Ldr(q5, MemOperand(x19, 16, PostIndex));
  __ Ldr(q6, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x2213120302212011, 0x1001001101201000, q0);
  CHECK_EQUAL_128(0x1809082726171607, 0x0625241514050423, q1);
  CHECK_EQUAL_128(0x0E2D2C1D1C0D0C2B, 0x2A1B1A0B0A292819, q2);
  CHECK_EQUAL_128(0x0504030201001003, 0x0201002F2E1F1E0F, q3);
  CHECK_EQUAL_128(0x2524232221201716, 0x1514131211100706, q4);
  CHECK_EQUAL_128(0x1D1C1B1A19180F0E, 0x0D0C0B0A09082726, q5);
  CHECK_EQUAL_128(0x6F6E6D6C6B6A2F2E, 0x2D2C2B2A29281F1E, q6);

  TEARDOWN();
}

TEST(neon_st4_d) {
  INIT_V8();
  SETUP();

  uint8_t src[4 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));

  __ St4(v0.V8B(), v1.V8B(), v2.V8B(), v3.V8B(), MemOperand(x18));
  __ Add(x18, x18, 12);
  __ St4(v0.V4H(), v1.V4H(), v2.V4H(), v3.V4H(), MemOperand(x18));
  __ Add(x18, x18, 15);
  __ St4(v0.V2S(), v1.V2S(), v2.V2S(), v3.V2S(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1110010032221202, 0X3121110130201000, q0);
  CHECK_EQUAL_128(0x1003020100322322, 0X1312030231302120, q1);
  CHECK_EQUAL_128(0x1407060504333231, 0X3023222120131211, q2);
  CHECK_EQUAL_128(0x3F3E3D3C3B373635, 0x3427262524171615, q3);

  TEARDOWN();
}

TEST(neon_st4_d_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[5 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x22, 5);
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));

  __ St4(v0.V8B(), v1.V8B(), v2.V8B(), v3.V8B(),
         MemOperand(x18, x22, PostIndex));
  __ St4(v0.V4H(), v1.V4H(), v2.V4H(), v3.V4H(),
         MemOperand(x18, 32, PostIndex));
  __ St4(v0.V2S(), v1.V2S(), v2.V2S(), v3.V2S(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));
  __ Ldr(q4, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1203023130212011, 0x1001000130201000, q0);
  CHECK_EQUAL_128(0x1607063534252415, 0x1405043332232213, q1);
  CHECK_EQUAL_128(0x2221201312111003, 0x0201003736272617, q2);
  CHECK_EQUAL_128(0x2625241716151407, 0x0605043332313023, q3);
  CHECK_EQUAL_128(0x4F4E4D4C4B4A4948, 0x4746453736353427, q4);

  TEARDOWN();
}

TEST(neon_st4_q) {
  INIT_V8();
  SETUP();

  uint8_t src[7 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));

  __ St4(v0.V16B(), v1.V16B(), v2.V16B(), v3.V16B(), MemOperand(x18));
  __ Add(x18, x18, 5);
  __ St4(v0.V8H(), v1.V8H(), v2.V8H(), v3.V8H(), MemOperand(x18));
  __ Add(x18, x18, 12);
  __ St4(v0.V4S(), v1.V4S(), v2.V4S(), v3.V4S(), MemOperand(x18));
  __ Add(x18, x18, 22);
  __ St4(v0.V2D(), v1.V2D(), v2.V2D(), v3.V2D(), MemOperand(x18));
  __ Add(x18, x18, 10);

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));
  __ Ldr(q4, MemOperand(x19, 16, PostIndex));
  __ Ldr(q5, MemOperand(x19, 16, PostIndex));
  __ Ldr(q6, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1203023130212011, 0x1001000130201000, q0);
  CHECK_EQUAL_128(0x3231302322212013, 0x1211100302010013, q1);
  CHECK_EQUAL_128(0x1007060504030201, 0x0015140706050433, q2);
  CHECK_EQUAL_128(0x3027262524232221, 0x2017161514131211, q3);
  CHECK_EQUAL_128(0x180F0E0D0C0B0A09, 0x0837363534333231, q4);
  CHECK_EQUAL_128(0x382F2E2D2C2B2A29, 0x281F1E1D1C1B1A19, q5);
  CHECK_EQUAL_128(0x6F6E6D6C6B6A6968, 0x673F3E3D3C3B3A39, q6);

  TEARDOWN();
}

TEST(neon_st4_q_postindex) {
  INIT_V8();
  SETUP();

  uint8_t src[9 * 16];
  for (unsigned i = 0; i < sizeof(src); i++) {
    src[i] = i;
  }
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x22, 5);
  __ Mov(x17, src_base);
  __ Mov(x18, src_base);
  __ Ldr(q0, MemOperand(x17, 16, PostIndex));
  __ Ldr(q1, MemOperand(x17, 16, PostIndex));
  __ Ldr(q2, MemOperand(x17, 16, PostIndex));
  __ Ldr(q3, MemOperand(x17, 16, PostIndex));

  __ St4(v0.V16B(), v1.V16B(), v2.V16B(), v3.V16B(),
         MemOperand(x18, x22, PostIndex));
  __ St4(v0.V8H(), v1.V8H(), v2.V8H(), v3.V8H(),
         MemOperand(x18, 64, PostIndex));
  __ St4(v0.V4S(), v1.V4S(), v2.V4S(), v3.V4S(),
         MemOperand(x18, x22, PostIndex));
  __ St4(v0.V2D(), v1.V2D(), v2.V2D(), v3.V2D(), MemOperand(x18));

  __ Mov(x19, src_base);
  __ Ldr(q0, MemOperand(x19, 16, PostIndex));
  __ Ldr(q1, MemOperand(x19, 16, PostIndex));
  __ Ldr(q2, MemOperand(x19, 16, PostIndex));
  __ Ldr(q3, MemOperand(x19, 16, PostIndex));
  __ Ldr(q4, MemOperand(x19, 16, PostIndex));
  __ Ldr(q5, MemOperand(x19, 16, PostIndex));
  __ Ldr(q6, MemOperand(x19, 16, PostIndex));
  __ Ldr(q7, MemOperand(x19, 16, PostIndex));
  __ Ldr(q8, MemOperand(x19, 16, PostIndex));

  END();

  RUN();

  CHECK_EQUAL_128(0x1203023130212011, 0x1001000130201000, q0);
  CHECK_EQUAL_128(0x1607063534252415, 0x1405043332232213, q1);
  CHECK_EQUAL_128(0x1A0B0A3938292819, 0x1809083736272617, q2);
  CHECK_EQUAL_128(0x1E0F0E3D3C2D2C1D, 0x1C0D0C3B3A2B2A1B, q3);
  CHECK_EQUAL_128(0x0504030201001003, 0x0201003F3E2F2E1F, q4);
  CHECK_EQUAL_128(0x2524232221201716, 0x1514131211100706, q5);
  CHECK_EQUAL_128(0x0D0C0B0A09083736, 0x3534333231302726, q6);
  CHECK_EQUAL_128(0x2D2C2B2A29281F1E, 0x1D1C1B1A19180F0E, q7);
  CHECK_EQUAL_128(0x8F8E8D8C8B8A3F3E, 0x3D3C3B3A39382F2E, q8);

  TEARDOWN();
}

TEST(neon_destructive_minmaxp) {
  INIT_V8();
  SETUP();

  START();
  __ Movi(v0.V2D(), 0, 0x2222222233333333);
  __ Movi(v1.V2D(), 0, 0x0000000011111111);

  __ Sminp(v16.V2S(), v0.V2S(), v1.V2S());
  __ Mov(v17, v0);
  __ Sminp(v17.V2S(), v17.V2S(), v1.V2S());
  __ Mov(v18, v1);
  __ Sminp(v18.V2S(), v0.V2S(), v18.V2S());
  __ Mov(v19, v0);
  __ Sminp(v19.V2S(), v19.V2S(), v19.V2S());

  __ Smaxp(v20.V2S(), v0.V2S(), v1.V2S());
  __ Mov(v21, v0);
  __ Smaxp(v21.V2S(), v21.V2S(), v1.V2S());
  __ Mov(v22, v1);
  __ Smaxp(v22.V2S(), v0.V2S(), v22.V2S());
  __ Mov(v23, v0);
  __ Smaxp(v23.V2S(), v23.V2S(), v23.V2S());

  __ Uminp(v24.V2S(), v0.V2S(), v1.V2S());
  __ Mov(v25, v0);
  __ Uminp(v25.V2S(), v25.V2S(), v1.V2S());
  __ Mov(v26, v1);
  __ Uminp(v26.V2S(), v0.V2S(), v26.V2S());
  __ Mov(v27, v0);
  __ Uminp(v27.V2S(), v27.V2S(), v27.V2S());

  __ Umaxp(v28.V2S(), v0.V2S(), v1.V2S());
  __ Mov(v29, v0);
  __ Umaxp(v29.V2S(), v29.V2S(), v1.V2S());
  __ Mov(v30, v1);
  __ Umaxp(v30.V2S(), v0.V2S(), v30.V2S());
  __ Mov(v31, v0);
  __ Umaxp(v31.V2S(), v31.V2S(), v31.V2S());
  END();

  RUN();

  CHECK_EQUAL_128(0, 0x0000000022222222, q16);
  CHECK_EQUAL_128(0, 0x0000000022222222, q17);
  CHECK_EQUAL_128(0, 0x0000000022222222, q18);
  CHECK_EQUAL_128(0, 0x2222222222222222, q19);

  CHECK_EQUAL_128(0, 0x1111111133333333, q20);
  CHECK_EQUAL_128(0, 0x1111111133333333, q21);
  CHECK_EQUAL_128(0, 0x1111111133333333, q22);
  CHECK_EQUAL_128(0, 0x3333333333333333, q23);

  CHECK_EQUAL_128(0, 0x0000000022222222, q24);
  CHECK_EQUAL_128(0, 0x0000000022222222, q25);
  CHECK_EQUAL_128(0, 0x0000000022222222, q26);
  CHECK_EQUAL_128(0, 0x2222222222222222, q27);

  CHECK_EQUAL_128(0, 0x1111111133333333, q28);
  CHECK_EQUAL_128(0, 0x1111111133333333, q29);
  CHECK_EQUAL_128(0, 0x1111111133333333, q30);
  CHECK_EQUAL_128(0, 0x3333333333333333, q31);

  TEARDOWN();
}

TEST(neon_destructive_tbl) {
  INIT_V8();
  SETUP();

  START();
  __ Movi(v0.V2D(), 0x0041424334353627, 0x28291A1B1C0D0E0F);
  __ Movi(v1.V2D(), 0xAFAEADACABAAA9A8, 0xA7A6A5A4A3A2A1A0);
  __ Movi(v2.V2D(), 0xBFBEBDBCBBBAB9B8, 0xB7B6B5B4B3B2B1B0);
  __ Movi(v3.V2D(), 0xCFCECDCCCBCAC9C8, 0xC7C6C5C4C3C2C1C0);
  __ Movi(v4.V2D(), 0xDFDEDDDCDBDAD9D8, 0xD7D6D5D4D3D2D1D0);

  __ Movi(v16.V2D(), 0x5555555555555555, 0x5555555555555555);
  __ Tbl(v16.V16B(), v1.V16B(), v0.V16B());
  __ Mov(v17, v0);
  __ Tbl(v17.V16B(), v1.V16B(), v17.V16B());
  __ Mov(v18, v1);
  __ Tbl(v18.V16B(), v18.V16B(), v0.V16B());
  __ Mov(v19, v0);
  __ Tbl(v19.V16B(), v19.V16B(), v19.V16B());

  __ Movi(v20.V2D(), 0x5555555555555555, 0x5555555555555555);
  __ Tbl(v20.V16B(), v1.V16B(), v2.V16B(), v3.V16B(), v4.V16B(), v0.V16B());
  __ Mov(v21, v0);
  __ Tbl(v21.V16B(), v1.V16B(), v2.V16B(), v3.V16B(), v4.V16B(), v21.V16B());
  __ Mov(v22, v1);
  __ Mov(v23, v2);
  __ Mov(v24, v3);
  __ Mov(v25, v4);
  __ Tbl(v22.V16B(), v22.V16B(), v23.V16B(), v24.V16B(), v25.V16B(), v0.V16B());
  __ Mov(v26, v0);
  __ Mov(v27, v1);
  __ Mov(v28, v2);
  __ Mov(v29, v3);
  __ Tbl(v26.V16B(), v26.V16B(), v27.V16B(), v28.V16B(), v29.V16B(),
         v26.V16B());
  END();

  RUN();

  CHECK_EQUAL_128(0xA000000000000000, 0x0000000000ADAEAF, q16);
  CHECK_EQUAL_128(0xA000000000000000, 0x0000000000ADAEAF, q17);
  CHECK_EQUAL_128(0xA000000000000000, 0x0000000000ADAEAF, q18);
  CHECK_EQUAL_128(0x0F00000000000000, 0x0000000000424100, q19);

  CHECK_EQUAL_128(0xA0000000D4D5D6C7, 0xC8C9BABBBCADAEAF, q20);
  CHECK_EQUAL_128(0xA0000000D4D5D6C7, 0xC8C9BABBBCADAEAF, q21);
  CHECK_EQUAL_128(0xA0000000D4D5D6C7, 0xC8C9BABBBCADAEAF, q22);
  CHECK_EQUAL_128(0x0F000000C4C5C6B7, 0xB8B9AAABAC424100, q26);

  TEARDOWN();
}

TEST(neon_destructive_tbx) {
  INIT_V8();
  SETUP();

  START();
  __ Movi(v0.V2D(), 0x0041424334353627, 0x28291A1B1C0D0E0F);
  __ Movi(v1.V2D(), 0xAFAEADACABAAA9A8, 0xA7A6A5A4A3A2A1A0);
  __ Movi(v2.V2D(), 0xBFBEBDBCBBBAB9B8, 0xB7B6B5B4B3B2B1B0);
  __ Movi(v3.V2D(), 0xCFCECDCCCBCAC9C8, 0xC7C6C5C4C3C2C1C0);
  __ Movi(v4.V2D(), 0xDFDEDDDCDBDAD9D8, 0xD7D6D5D4D3D2D1D0);

  __ Movi(v16.V2D(), 0x5555555555555555, 0x5555555555555555);
  __ Tbx(v16.V16B(), v1.V16B(), v0.V16B());
  __ Mov(v17, v0);
  __ Tbx(v17.V16B(), v1.V16B(), v17.V16B());
  __ Mov(v18, v1);
  __ Tbx(v18.V16B(), v18.V16B(), v0.V16B());
  __ Mov(v19, v0);
  __ Tbx(v19.V16B(), v19.V16B(), v19.V16B());

  __ Movi(v20.V2D(), 0x5555555555555555, 0x5555555555555555);
  __ Tbx(v20.V16B(), v1.V16B(), v2.V16B(), v3.V16B(), v4.V16B(), v0.V16B());
  __ Mov(v21, v0);
  __ Tbx(v21.V16B(), v1.V16B(), v2.V16B(), v3.V16B(), v4.V16B(), v21.V16B());
  __ Mov(v22, v1);
  __ Mov(v23, v2);
  __ Mov(v24, v3);
  __ Mov(v25, v4);
  __ Tbx(v22.V16B(), v22.V16B(), v23.V16B(), v24.V16B(), v25.V16B(), v0.V16B());
  __ Mov(v26, v0);
  __ Mov(v27, v1);
  __ Mov(v28, v2);
  __ Mov(v29, v3);
  __ Tbx(v26.V16B(), v26.V16B(), v27.V16B(), v28.V16B(), v29.V16B(),
         v26.V16B());
  END();

  RUN();

  CHECK_EQUAL_128(0xA055555555555555, 0x5555555555ADAEAF, q16);
  CHECK_EQUAL_128(0xA041424334353627, 0x28291A1B1CADAEAF, q17);
  CHECK_EQUAL_128(0xA0AEADACABAAA9A8, 0xA7A6A5A4A3ADAEAF, q18);
  CHECK_EQUAL_128(0x0F41424334353627, 0x28291A1B1C424100, q19);

  CHECK_EQUAL_128(0xA0555555D4D5D6C7, 0xC8C9BABBBCADAEAF, q20);
  CHECK_EQUAL_128(0xA0414243D4D5D6C7, 0xC8C9BABBBCADAEAF, q21);
  CHECK_EQUAL_128(0xA0AEADACD4D5D6C7, 0xC8C9BABBBCADAEAF, q22);
  CHECK_EQUAL_128(0x0F414243C4C5C6B7, 0xB8B9AAABAC424100, q26);

  TEARDOWN();
}

TEST(neon_destructive_fcvtl) {
  INIT_V8();
  SETUP();

  START();
  __ Movi(v0.V2D(), 0x400000003F800000, 0xBF800000C0000000);
  __ Fcvtl(v16.V2D(), v0.V2S());
  __ Fcvtl2(v17.V2D(), v0.V4S());
  __ Mov(v18, v0);
  __ Mov(v19, v0);
  __ Fcvtl(v18.V2D(), v18.V2S());
  __ Fcvtl2(v19.V2D(), v19.V4S());

  __ Movi(v1.V2D(), 0x40003C003C004000, 0xC000BC00BC00C000);
  __ Fcvtl(v20.V4S(), v1.V4H());
  __ Fcvtl2(v21.V4S(), v1.V8H());
  __ Mov(v22, v1);
  __ Mov(v23, v1);
  __ Fcvtl(v22.V4S(), v22.V4H());
  __ Fcvtl2(v23.V4S(), v23.V8H());

  END();

  RUN();

  CHECK_EQUAL_128(0xBFF0000000000000, 0xC000000000000000, q16);
  CHECK_EQUAL_128(0x4000000000000000, 0x3FF0000000000000, q17);
  CHECK_EQUAL_128(0xBFF0000000000000, 0xC000000000000000, q18);
  CHECK_EQUAL_128(0x4000000000000000, 0x3FF0000000000000, q19);

  CHECK_EQUAL_128(0xC0000000BF800000, 0xBF800000C0000000, q20);
  CHECK_EQUAL_128(0x400000003F800000, 0x3F80000040000000, q21);
  CHECK_EQUAL_128(0xC0000000BF800000, 0xBF800000C0000000, q22);
  CHECK_EQUAL_128(0x400000003F800000, 0x3F80000040000000, q23);

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

TEST(ldp_stp_quad) {
  SETUP();

  uint64_t src[4] = {0x0123456789ABCDEF, 0xAAAAAAAA55555555, 0xFEDCBA9876543210,
                     0x55555555AAAAAAAA};
  uint64_t dst[6] = {0, 0, 0, 0, 0, 0};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);
  uintptr_t dst_base = reinterpret_cast<uintptr_t>(dst);

  START();
  __ Mov(x16, src_base);
  __ Mov(x17, dst_base);
  __ Ldp(q31, q0, MemOperand(x16, 4 * sizeof(src[0]), PostIndex));
  __ Stp(q0, q31, MemOperand(x17, 2 * sizeof(dst[1]), PreIndex));
  END();

  RUN();

  CHECK_EQUAL_128(0xAAAAAAAA55555555, 0x0123456789ABCDEF, q31);
  CHECK_EQUAL_128(0x55555555AAAAAAAA, 0xFEDCBA9876543210, q0);
  CHECK_EQUAL_64(0, dst[0]);
  CHECK_EQUAL_64(0, dst[1]);
  CHECK_EQUAL_64(0xFEDCBA9876543210, dst[2]);
  CHECK_EQUAL_64(0x55555555AAAAAAAA, dst[3]);
  CHECK_EQUAL_64(0x0123456789ABCDEF, dst[4]);
  CHECK_EQUAL_64(0xAAAAAAAA55555555, dst[5]);
  CHECK_EQUAL_64(src_base + 4 * sizeof(src[0]), x16);
  CHECK_EQUAL_64(dst_base + 2 * sizeof(dst[1]), x17);

  TEARDOWN();
}

TEST(ldp_stp_offset) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677UL, 0x8899AABBCCDDEEFFUL,
                     0xFFEEDDCCBBAA9988UL};
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
  CHECK_EQUAL_64(0xCCDDEEFF, x3);
  CHECK_EQUAL_64(0xCCDDEEFF00112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x4);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[2]);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x5);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[3]);
  CHECK_EQUAL_64(0x8899AABB, x6);
  CHECK_EQUAL_64(0xBBAA9988, x7);
  CHECK_EQUAL_64(0xBBAA99888899AABBUL, dst[4]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x8);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[5]);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x9);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[6]);
  CHECK_EQUAL_64(src_base, x16);
  CHECK_EQUAL_64(dst_base, x17);
  CHECK_EQUAL_64(src_base + 24, x18);
  CHECK_EQUAL_64(dst_base + 56, x19);

  TEARDOWN();
}


TEST(ldp_stp_offset_wide) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677, 0x8899AABBCCDDEEFF,
                     0xFFEEDDCCBBAA9988};
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
  CHECK_EQUAL_64(0xCCDDEEFF, x3);
  CHECK_EQUAL_64(0xCCDDEEFF00112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x4);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[2]);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x5);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[3]);
  CHECK_EQUAL_64(0x8899AABB, x6);
  CHECK_EQUAL_64(0xBBAA9988, x7);
  CHECK_EQUAL_64(0xBBAA99888899AABBUL, dst[4]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x8);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[5]);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x9);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[6]);
  CHECK_EQUAL_64(src_base - base_offset, x20);
  CHECK_EQUAL_64(dst_base - base_offset, x21);
  CHECK_EQUAL_64(src_base + base_offset + 24, x18);
  CHECK_EQUAL_64(dst_base + base_offset + 56, x19);

  TEARDOWN();
}


TEST(ldp_stp_preindex) {
  INIT_V8();
  SETUP();

  uint64_t src[3] = {0x0011223344556677UL, 0x8899AABBCCDDEEFFUL,
                     0xFFEEDDCCBBAA9988UL};
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
  CHECK_EQUAL_64(0xCCDDEEFF, x1);
  CHECK_EQUAL_64(0x44556677, x2);
  CHECK_EQUAL_64(0x00112233, x3);
  CHECK_EQUAL_64(0xCCDDEEFF00112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x4);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x5);
  CHECK_EQUAL_64(0x0011223344556677UL, x6);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x7);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[3]);
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

  uint64_t src[3] = {0x0011223344556677, 0x8899AABBCCDDEEFF,
                     0xFFEEDDCCBBAA9988};
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
  CHECK_EQUAL_64(0xCCDDEEFF, x1);
  CHECK_EQUAL_64(0x44556677, x2);
  CHECK_EQUAL_64(0x00112233, x3);
  CHECK_EQUAL_64(0xCCDDEEFF00112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x4);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x5);
  CHECK_EQUAL_64(0x0011223344556677UL, x6);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x7);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[3]);
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

  uint64_t src[4] = {0x0011223344556677UL, 0x8899AABBCCDDEEFFUL,
                     0xFFEEDDCCBBAA9988UL, 0x7766554433221100UL};
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
  CHECK_EQUAL_64(0xCCDDEEFF, x3);
  CHECK_EQUAL_64(0x4455667700112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x0011223344556677UL, x4);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x5);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x6);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x7);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[3]);
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

  uint64_t src[4] = {0x0011223344556677, 0x8899AABBCCDDEEFF, 0xFFEEDDCCBBAA9988,
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
  CHECK_EQUAL_64(0xCCDDEEFF, x3);
  CHECK_EQUAL_64(0x4455667700112233UL, dst[0]);
  CHECK_EQUAL_64(0x0000000000112233UL, dst[1]);
  CHECK_EQUAL_64(0x0011223344556677UL, x4);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x5);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, x6);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, x7);
  CHECK_EQUAL_64(0xFFEEDDCCBBAA9988UL, dst[2]);
  CHECK_EQUAL_64(0x8899AABBCCDDEEFFUL, dst[3]);
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

  uint32_t src[2] = {0x80000000, 0x7FFFFFFF};
  uintptr_t src_base = reinterpret_cast<uintptr_t>(src);

  START();
  __ Mov(x24, src_base);
  __ Ldpsw(x0, x1, MemOperand(x24));
  END();

  RUN();

  CHECK_EQUAL_64(0xFFFFFFFF80000000UL, x0);
  CHECK_EQUAL_64(0x000000007FFFFFFFUL, x1);

  TEARDOWN();
}


TEST(ldur_stur) {
  INIT_V8();
  SETUP();

  int64_t src[2] = {0x0123456789ABCDEFUL, 0x0123456789ABCDEFUL};
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

  CHECK_EQUAL_64(0x6789ABCD, x0);
  CHECK_EQUAL_64(0x6789ABCD0000L, dst[0]);
  CHECK_EQUAL_64(0xABCDEF0123456789L, x1);
  CHECK_EQUAL_64(0xCDEF012345678900L, dst[1]);
  CHECK_EQUAL_64(0x000000AB, dst[2]);
  CHECK_EQUAL_64(0xABCDEF01, x2);
  CHECK_EQUAL_64(0x00ABCDEF01000000L, dst[3]);
  CHECK_EQUAL_64(0x00000001, x3);
  CHECK_EQUAL_64(0x0100000000000000L, dst[4]);
  CHECK_EQUAL_64(src_base, x17);
  CHECK_EQUAL_64(dst_base, x18);
  CHECK_EQUAL_64(src_base + 16, x19);
  CHECK_EQUAL_64(dst_base + 32, x20);

  TEARDOWN();
}

namespace {

void LoadLiteral(MacroAssembler* masm, Register reg, uint64_t imm) {
  // Since we do not allow non-relocatable entries in the literal pool, we need
  // to fake a relocation mode that is not NONE here.
  masm->Ldr(reg, Immediate(imm, RelocInfo::EMBEDDED_OBJECT));
}

}  // namespace

TEST(ldr_pcrel_large_offset) {
  INIT_V8();
  SETUP_SIZE(1 * MB);

  START();

  LoadLiteral(&masm, x1, 0x1234567890ABCDEFUL);

  {
    v8::internal::PatchingAssembler::BlockPoolsScope scope(&masm);
    int start = __ pc_offset();
    while (__ pc_offset() - start < 600 * KB) {
      __ Nop();
    }
  }

  LoadLiteral(&masm, x2, 0x1234567890ABCDEFUL);

  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x1);
  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x2);

  TEARDOWN();
}

TEST(ldr_literal) {
  INIT_V8();
  SETUP();

  START();
  LoadLiteral(&masm, x2, 0x1234567890ABCDEFUL);

  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x2);

  TEARDOWN();
}

#ifdef DEBUG
// These tests rely on functions available in debug mode.
enum LiteralPoolEmitOutcome { EmitExpected, NoEmitExpected };

static void LdrLiteralRangeHelper(size_t range, LiteralPoolEmitOutcome outcome,
                                  size_t prepadding = 0) {
  SETUP_SIZE(static_cast<int>(range + 1024));

  size_t code_size = 0;
  const size_t pool_entries = 2;
  const size_t kEntrySize = 8;

  START();
  // Force a pool dump so the pool starts off empty.
  __ CheckConstPool(true, true);
  CHECK_CONSTANT_POOL_SIZE(0);

  // Emit prepadding to influence alignment of the pool; we don't count this
  // into code size.
  for (size_t i = 0; i < prepadding; ++i) __ Nop();

  LoadLiteral(&masm, x0, 0x1234567890ABCDEFUL);
  LoadLiteral(&masm, x1, 0xABCDEF1234567890UL);
  code_size += 2 * kInstrSize;
  CHECK_CONSTANT_POOL_SIZE(pool_entries * kEntrySize);

  // Check that the requested range (allowing space for a branch over the pool)
  // can be handled by this test.
  CHECK_LE(code_size, range);

  auto PoolSizeAt = [pool_entries](int pc_offset) {
    // To determine padding, consider the size of the prologue of the pool,
    // and the jump around the pool, which we always need.
    size_t prologue_size = 2 * kInstrSize + kInstrSize;
    size_t pc = pc_offset + prologue_size;
    const size_t padding = IsAligned(pc, 8) ? 0 : 4;
    return prologue_size + pool_entries * kEntrySize + padding;
  };

  int pc_offset_before_emission = -1;
  // Emit NOPs up to 'range'.
  while (code_size < range) {
    pc_offset_before_emission = __ pc_offset() + kInstrSize;
    __ Nop();
    code_size += kInstrSize;
  }
  CHECK_EQ(code_size, range);

  if (outcome == EmitExpected) {
    CHECK_CONSTANT_POOL_SIZE(0);
    // Check that the size of the emitted constant pool is as expected.
    size_t pool_size = PoolSizeAt(pc_offset_before_emission);
    CHECK_EQ(pc_offset_before_emission + pool_size, __ pc_offset());
    byte* pool_start = buf + pc_offset_before_emission;
    Instruction* branch = reinterpret_cast<Instruction*>(pool_start);
    CHECK(branch->IsImmBranch());
    CHECK_EQ(pool_size, branch->ImmPCOffset());
    Instruction* marker =
        reinterpret_cast<Instruction*>(pool_start + kInstrSize);
    CHECK(marker->IsLdrLiteralX());
    const size_t padding =
        IsAligned(pc_offset_before_emission + kInstrSize, kEntrySize) ? 0 : 1;
    CHECK_EQ(pool_entries * 2 + 1 + padding, marker->ImmLLiteral());

  } else {
    CHECK_EQ(outcome, NoEmitExpected);
    CHECK_CONSTANT_POOL_SIZE(pool_entries * kEntrySize);
    CHECK_EQ(pc_offset_before_emission, __ pc_offset());
  }

  // Force a pool flush to check that a second pool functions correctly.
  __ CheckConstPool(true, true);
  CHECK_CONSTANT_POOL_SIZE(0);

  // These loads should be after the pool (and will require a new one).
  LoadLiteral(&masm, x4, 0x34567890ABCDEF12UL);
  LoadLiteral(&masm, x5, 0xABCDEF0123456789UL);
  CHECK_CONSTANT_POOL_SIZE(pool_entries * kEntrySize);
  END();

  RUN();

  // Check that the literals loaded correctly.
  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x0);
  CHECK_EQUAL_64(0xABCDEF1234567890UL, x1);
  CHECK_EQUAL_64(0x34567890ABCDEF12UL, x4);
  CHECK_EQUAL_64(0xABCDEF0123456789UL, x5);

  TEARDOWN();
}

TEST(ldr_literal_range_max_dist_emission_1) {
  INIT_V8();
  LdrLiteralRangeHelper(MacroAssembler::GetApproxMaxDistToConstPoolForTesting(),
                        EmitExpected);
}

TEST(ldr_literal_range_max_dist_emission_2) {
  INIT_V8();
  LdrLiteralRangeHelper(MacroAssembler::GetApproxMaxDistToConstPoolForTesting(),
                        EmitExpected, 1);
}

TEST(ldr_literal_range_max_dist_no_emission_1) {
  INIT_V8();
  LdrLiteralRangeHelper(
      MacroAssembler::GetApproxMaxDistToConstPoolForTesting() - kInstrSize,
      NoEmitExpected);
}

TEST(ldr_literal_range_max_dist_no_emission_2) {
  INIT_V8();
  LdrLiteralRangeHelper(
      MacroAssembler::GetApproxMaxDistToConstPoolForTesting() - kInstrSize,
      NoEmitExpected, 1);
}

#endif

TEST(add_sub_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0x0);
  __ Mov(x1, 0x1111);
  __ Mov(x2, 0xFFFFFFFFFFFFFFFFL);
  __ Mov(x3, 0x8000000000000000L);

  __ Add(x10, x0, Operand(0x123));
  __ Add(x11, x1, Operand(0x122000));
  __ Add(x12, x0, Operand(0xABC << 12));
  __ Add(x13, x2, Operand(1));

  __ Add(w14, w0, Operand(0x123));
  __ Add(w15, w1, Operand(0x122000));
  __ Add(w16, w0, Operand(0xABC << 12));
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
  CHECK_EQUAL_64(0xABC000, x12);
  CHECK_EQUAL_64(0x0, x13);

  CHECK_EQUAL_32(0x123, w14);
  CHECK_EQUAL_32(0x123111, w15);
  CHECK_EQUAL_32(0xABC000, w16);
  CHECK_EQUAL_32(0x0, w17);

  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFL, x20);
  CHECK_EQUAL_64(0x1000, x21);
  CHECK_EQUAL_64(0x111, x22);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFL, x23);

  CHECK_EQUAL_32(0xFFFFFFFF, w24);
  CHECK_EQUAL_32(0x1000, w25);
  CHECK_EQUAL_32(0x111, w26);
  CHECK_EQUAL_32(0xFFFFFFFF, w27);

  TEARDOWN();
}


TEST(add_sub_wide_imm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0x0);
  __ Mov(x1, 0x1);

  __ Add(x10, x0, Operand(0x1234567890ABCDEFUL));
  __ Add(x11, x1, Operand(0xFFFFFFFF));

  __ Add(w12, w0, Operand(0x12345678));
  __ Add(w13, w1, Operand(0xFFFFFFFF));

  __ Add(w18, w0, Operand(kWMinInt));
  __ Sub(w19, w0, Operand(kWMinInt));

  __ Sub(x20, x0, Operand(0x1234567890ABCDEFUL));
  __ Sub(w21, w0, Operand(0x12345678));
  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x10);
  CHECK_EQUAL_64(0x100000000UL, x11);

  CHECK_EQUAL_32(0x12345678, w12);
  CHECK_EQUAL_64(0x0, x13);

  CHECK_EQUAL_32(kWMinInt, w18);
  CHECK_EQUAL_32(kWMinInt, w19);

  CHECK_EQUAL_64(-0x1234567890ABCDEFUL, x20);
  CHECK_EQUAL_32(-0x12345678, w21);

  TEARDOWN();
}


TEST(add_sub_shifted) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0x0123456789ABCDEFL);
  __ Mov(x2, 0xFEDCBA9876543210L);
  __ Mov(x3, 0xFFFFFFFFFFFFFFFFL);

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

  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFL, x10);
  CHECK_EQUAL_64(0x23456789ABCDEF00L, x11);
  CHECK_EQUAL_64(0x000123456789ABCDL, x12);
  CHECK_EQUAL_64(0x000123456789ABCDL, x13);
  CHECK_EQUAL_64(0xFFFEDCBA98765432L, x14);
  CHECK_EQUAL_64(0xFF89ABCD, x15);
  CHECK_EQUAL_64(0xEF89ABCC, x18);
  CHECK_EQUAL_64(0xEF0123456789ABCCL, x19);

  CHECK_EQUAL_64(0x0123456789ABCDEFL, x20);
  CHECK_EQUAL_64(0xDCBA9876543210FFL, x21);
  CHECK_EQUAL_64(0xFFFEDCBA98765432L, x22);
  CHECK_EQUAL_64(0xFFFEDCBA98765432L, x23);
  CHECK_EQUAL_64(0x000123456789ABCDL, x24);
  CHECK_EQUAL_64(0x00765432, x25);
  CHECK_EQUAL_64(0x10765432, x26);
  CHECK_EQUAL_64(0x10FEDCBA98765432L, x27);

  TEARDOWN();
}


TEST(add_sub_extended) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 0x0123456789ABCDEFL);
  __ Mov(x2, 0xFEDCBA9876543210L);
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

  CHECK_EQUAL_64(0xEFL, x10);
  CHECK_EQUAL_64(0x1DEL, x11);
  CHECK_EQUAL_64(0x337BCL, x12);
  CHECK_EQUAL_64(0x89ABCDEF0L, x13);

  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFEFL, x14);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFDEL, x15);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF37BCL, x16);
  CHECK_EQUAL_64(0xFFFFFFFC4D5E6F78L, x17);
  CHECK_EQUAL_64(0x10L, x18);
  CHECK_EQUAL_64(0x20L, x19);
  CHECK_EQUAL_64(0xC840L, x20);
  CHECK_EQUAL_64(0x3B2A19080L, x21);

  CHECK_EQUAL_64(0x0123456789ABCE0FL, x22);
  CHECK_EQUAL_64(0x0123456789ABCDCFL, x23);

  CHECK_EQUAL_32(0x89ABCE2F, w24);
  CHECK_EQUAL_32(0xFFFFFFEF, w25);
  CHECK_EQUAL_32(0xFFFFFFDE, w26);
  CHECK_EQUAL_32(0xC3B2A188, w27);

  CHECK_EQUAL_32(0x4D5E6F78, w28);
  CHECK_EQUAL_64(0xFFFFFFFC4D5E6F78L, x29);

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

  __ Sub(w21, w3, -0xBC);
  __ Sub(w22, w4, -2000);
  END();

  RUN();

  CHECK_EQUAL_64(-42, x10);
  CHECK_EQUAL_64(4000, x11);
  CHECK_EQUAL_64(0x1122334455667700, x12);

  CHECK_EQUAL_64(600, x13);
  CHECK_EQUAL_64(5000, x14);
  CHECK_EQUAL_64(0x1122334455667CDD, x15);

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
  CHECK_EQ(0u, __ SizeOfCodeGeneratedSince(&blob1));

  Label blob2;
  __ Bind(&blob2);
  __ Add(w3, w3, 0);
  CHECK_NE(0u, __ SizeOfCodeGeneratedSince(&blob2));

  Label blob3;
  __ Bind(&blob3);
  __ Sub(w3, w3, wzr);
  CHECK_NE(0u, __ SizeOfCodeGeneratedSince(&blob3));

  END();

  RUN();

  CHECK_EQUAL_64(0, x0);
  CHECK_EQUAL_64(0, x1);
  CHECK_EQUAL_64(0, x2);

  TEARDOWN();
}

TEST(preshift_immediates) {
  INIT_V8();
  SETUP();

  START();
  // Test operations involving immediates that could be generated using a
  // pre-shifted encodable immediate followed by a post-shift applied to
  // the arithmetic or logical operation.

  // Save sp.
  __ Mov(x29, sp);

  // Set the registers to known values.
  __ Mov(x0, 0x1000);
  __ Mov(sp, 0x1000);

  // Arithmetic ops.
  __ Add(x1, x0, 0x1F7DE);
  __ Add(w2, w0, 0xFFFFFF1);
  __ Adds(x3, x0, 0x18001);
  __ Adds(w4, w0, 0xFFFFFF1);
  __ Add(x5, x0, 0x10100);
  __ Sub(w6, w0, 0xFFFFFF1);
  __ Subs(x7, x0, 0x18001);
  __ Subs(w8, w0, 0xFFFFFF1);

  // Logical ops.
  __ And(x9, x0, 0x1F7DE);
  __ Orr(w10, w0, 0xFFFFFF1);
  __ Eor(x11, x0, 0x18001);

  // Ops using the stack pointer.
  __ Add(sp, sp, 0x1F7F0);
  __ Mov(x12, sp);
  __ Mov(sp, 0x1000);

  __ Adds(x13, sp, 0x1F7F0);

  __ Orr(sp, x0, 0x1F7F0);
  __ Mov(x14, sp);
  __ Mov(sp, 0x1000);

  __ Add(sp, sp, 0x10100);
  __ Mov(x15, sp);

  //  Restore sp.
  __ Mov(sp, x29);
  END();

  RUN();

  CHECK_EQUAL_64(0x1000, x0);
  CHECK_EQUAL_64(0x207DE, x1);
  CHECK_EQUAL_64(0x10000FF1, x2);
  CHECK_EQUAL_64(0x19001, x3);
  CHECK_EQUAL_64(0x10000FF1, x4);
  CHECK_EQUAL_64(0x11100, x5);
  CHECK_EQUAL_64(0xF000100F, x6);
  CHECK_EQUAL_64(0xFFFFFFFFFFFE8FFF, x7);
  CHECK_EQUAL_64(0xF000100F, x8);
  CHECK_EQUAL_64(0x1000, x9);
  CHECK_EQUAL_64(0xFFFFFF1, x10);
  CHECK_EQUAL_64(0x207F0, x12);
  CHECK_EQUAL_64(0x207F0, x13);
  CHECK_EQUAL_64(0x1F7F0, x14);
  CHECK_EQUAL_64(0x11100, x15);

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
  CHECK_EQ(0u, __ SizeOfCodeGeneratedSince(&start));

  END();

  RUN();

  TEARDOWN();
}


TEST(neg) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0xF123456789ABCDEFL);

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

  CHECK_EQUAL_64(0xFFFFFFFFFFFFFEDDUL, x1);
  CHECK_EQUAL_64(0xFFFFFEDD, x2);
  CHECK_EQUAL_64(0x1DB97530ECA86422UL, x3);
  CHECK_EQUAL_64(0xD950C844, x4);
  CHECK_EQUAL_64(0xE1DB97530ECA8643UL, x5);
  CHECK_EQUAL_64(0xF7654322, x6);
  CHECK_EQUAL_64(0x0076E5D4C3B2A191UL, x7);
  CHECK_EQUAL_64(0x01D950C9, x8);
  CHECK_EQUAL_64(0xFFFFFF11, x9);
  CHECK_EQUAL_64(0x0000000000000022UL, x10);
  CHECK_EQUAL_64(0xFFFCC844, x11);
  CHECK_EQUAL_64(0x0000000000019088UL, x12);
  CHECK_EQUAL_64(0x65432110, x13);
  CHECK_EQUAL_64(0x0000000765432110UL, x14);

  TEARDOWN();
}


template <typename T, typename Op>
static void AdcsSbcsHelper(Op op, T left, T right, int carry, T expected,
                           StatusFlags expected_flags) {
  int reg_size = sizeof(T) * 8;
  auto left_reg = Register::Create(0, reg_size);
  auto right_reg = Register::Create(1, reg_size);
  auto result_reg = Register::Create(2, reg_size);

  SETUP();
  START();

  __ Mov(left_reg, left);
  __ Mov(right_reg, right);
  __ Mov(x10, (carry ? CFlag : NoFlag));

  __ Msr(NZCV, x10);
  (masm.*op)(result_reg, left_reg, right_reg);

  END();
  RUN();

  CHECK_EQUAL_64(left, left_reg.X());
  CHECK_EQUAL_64(right, right_reg.X());
  CHECK_EQUAL_64(expected, result_reg.X());
  CHECK_EQUAL_NZCV(expected_flags);

  TEARDOWN();
}


TEST(adcs_sbcs_x) {
  INIT_V8();
  uint64_t inputs[] = {
      0x0000000000000000, 0x0000000000000001, 0x7FFFFFFFFFFFFFFE,
      0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0x8000000000000001,
      0xFFFFFFFFFFFFFFFE, 0xFFFFFFFFFFFFFFFF,
  };
  static const size_t input_count = sizeof(inputs) / sizeof(inputs[0]);

  struct Expected {
    uint64_t carry0_result;
    StatusFlags carry0_flags;
    uint64_t carry1_result;
    StatusFlags carry1_flags;
  };

  static const Expected expected_adcs_x[input_count][input_count] = {
      {{0x0000000000000000, ZFlag, 0x0000000000000001, NoFlag},
       {0x0000000000000001, NoFlag, 0x0000000000000002, NoFlag},
       {0x7FFFFFFFFFFFFFFE, NoFlag, 0x7FFFFFFFFFFFFFFF, NoFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0x8000000000000000, NFlag, 0x8000000000000001, NFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag}},
      {{0x0000000000000001, NoFlag, 0x0000000000000002, NoFlag},
       {0x0000000000000002, NoFlag, 0x0000000000000003, NoFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0x8000000000000000, NVFlag, 0x8000000000000001, NVFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0x8000000000000002, NFlag, 0x8000000000000003, NFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag}},
      {{0x7FFFFFFFFFFFFFFE, NoFlag, 0x7FFFFFFFFFFFFFFF, NoFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0xFFFFFFFFFFFFFFFC, NVFlag, 0xFFFFFFFFFFFFFFFD, NVFlag},
       {0xFFFFFFFFFFFFFFFD, NVFlag, 0xFFFFFFFFFFFFFFFE, NVFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x7FFFFFFFFFFFFFFC, CFlag, 0x7FFFFFFFFFFFFFFD, CFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag}},
      {{0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0x8000000000000000, NVFlag, 0x8000000000000001, NVFlag},
       {0xFFFFFFFFFFFFFFFD, NVFlag, 0xFFFFFFFFFFFFFFFE, NVFlag},
       {0xFFFFFFFFFFFFFFFE, NVFlag, 0xFFFFFFFFFFFFFFFF, NVFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x7FFFFFFFFFFFFFFE, CFlag, 0x7FFFFFFFFFFFFFFF, CFlag}},
      {{0x8000000000000000, NFlag, 0x8000000000000001, NFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x0000000000000000, ZCVFlag, 0x0000000000000001, CVFlag},
       {0x0000000000000001, CVFlag, 0x0000000000000002, CVFlag},
       {0x7FFFFFFFFFFFFFFE, CVFlag, 0x7FFFFFFFFFFFFFFF, CVFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag}},
      {{0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0x8000000000000002, NFlag, 0x8000000000000003, NFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0x0000000000000001, CVFlag, 0x0000000000000002, CVFlag},
       {0x0000000000000002, CVFlag, 0x0000000000000003, CVFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0x8000000000000000, NCFlag, 0x8000000000000001, NCFlag}},
      {{0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x7FFFFFFFFFFFFFFC, CFlag, 0x7FFFFFFFFFFFFFFD, CFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x7FFFFFFFFFFFFFFE, CVFlag, 0x7FFFFFFFFFFFFFFF, CVFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0xFFFFFFFFFFFFFFFC, NCFlag, 0xFFFFFFFFFFFFFFFD, NCFlag},
       {0xFFFFFFFFFFFFFFFD, NCFlag, 0xFFFFFFFFFFFFFFFE, NCFlag}},
      {{0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x7FFFFFFFFFFFFFFE, CFlag, 0x7FFFFFFFFFFFFFFF, CFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0x8000000000000000, NCFlag, 0x8000000000000001, NCFlag},
       {0xFFFFFFFFFFFFFFFD, NCFlag, 0xFFFFFFFFFFFFFFFE, NCFlag},
       {0xFFFFFFFFFFFFFFFE, NCFlag, 0xFFFFFFFFFFFFFFFF, NCFlag}}};

  static const Expected expected_sbcs_x[input_count][input_count] = {
      {{0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0x8000000000000000, NFlag, 0x8000000000000001, NFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0x7FFFFFFFFFFFFFFE, NoFlag, 0x7FFFFFFFFFFFFFFF, NoFlag},
       {0x0000000000000001, NoFlag, 0x0000000000000002, NoFlag},
       {0x0000000000000000, ZFlag, 0x0000000000000001, NoFlag}},
      {{0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x8000000000000002, NFlag, 0x8000000000000003, NFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0x8000000000000000, NVFlag, 0x8000000000000001, NVFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0x0000000000000002, NoFlag, 0x0000000000000003, NoFlag},
       {0x0000000000000001, NoFlag, 0x0000000000000002, NoFlag}},
      {{0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x7FFFFFFFFFFFFFFC, CFlag, 0x7FFFFFFFFFFFFFFD, CFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0xFFFFFFFFFFFFFFFD, NVFlag, 0xFFFFFFFFFFFFFFFE, NVFlag},
       {0xFFFFFFFFFFFFFFFC, NVFlag, 0xFFFFFFFFFFFFFFFD, NVFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag},
       {0x7FFFFFFFFFFFFFFE, NoFlag, 0x7FFFFFFFFFFFFFFF, NoFlag}},
      {{0x7FFFFFFFFFFFFFFE, CFlag, 0x7FFFFFFFFFFFFFFF, CFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0xFFFFFFFFFFFFFFFE, NVFlag, 0xFFFFFFFFFFFFFFFF, NVFlag},
       {0xFFFFFFFFFFFFFFFD, NVFlag, 0xFFFFFFFFFFFFFFFE, NVFlag},
       {0x8000000000000000, NVFlag, 0x8000000000000001, NVFlag},
       {0x7FFFFFFFFFFFFFFF, NoFlag, 0x8000000000000000, NVFlag}},
      {{0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0x7FFFFFFFFFFFFFFE, CVFlag, 0x7FFFFFFFFFFFFFFF, CVFlag},
       {0x0000000000000001, CVFlag, 0x0000000000000002, CVFlag},
       {0x0000000000000000, ZCVFlag, 0x0000000000000001, CVFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag},
       {0x8000000000000000, NFlag, 0x8000000000000001, NFlag}},
      {{0x8000000000000000, NCFlag, 0x8000000000000001, NCFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0x0000000000000002, CVFlag, 0x0000000000000003, CVFlag},
       {0x0000000000000001, CVFlag, 0x0000000000000002, CVFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0x8000000000000002, NFlag, 0x8000000000000003, NFlag},
       {0x8000000000000001, NFlag, 0x8000000000000002, NFlag}},
      {{0xFFFFFFFFFFFFFFFD, NCFlag, 0xFFFFFFFFFFFFFFFE, NCFlag},
       {0xFFFFFFFFFFFFFFFC, NCFlag, 0xFFFFFFFFFFFFFFFD, NCFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0x7FFFFFFFFFFFFFFE, CVFlag, 0x7FFFFFFFFFFFFFFF, CVFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x7FFFFFFFFFFFFFFC, CFlag, 0x7FFFFFFFFFFFFFFD, CFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag},
       {0xFFFFFFFFFFFFFFFE, NFlag, 0xFFFFFFFFFFFFFFFF, NFlag}},
      {{0xFFFFFFFFFFFFFFFE, NCFlag, 0xFFFFFFFFFFFFFFFF, NCFlag},
       {0xFFFFFFFFFFFFFFFD, NCFlag, 0xFFFFFFFFFFFFFFFE, NCFlag},
       {0x8000000000000000, NCFlag, 0x8000000000000001, NCFlag},
       {0x7FFFFFFFFFFFFFFF, CVFlag, 0x8000000000000000, NCFlag},
       {0x7FFFFFFFFFFFFFFE, CFlag, 0x7FFFFFFFFFFFFFFF, CFlag},
       {0x7FFFFFFFFFFFFFFD, CFlag, 0x7FFFFFFFFFFFFFFE, CFlag},
       {0x0000000000000000, ZCFlag, 0x0000000000000001, CFlag},
       {0xFFFFFFFFFFFFFFFF, NFlag, 0x0000000000000000, ZCFlag}}};

  for (size_t left = 0; left < input_count; left++) {
    for (size_t right = 0; right < input_count; right++) {
      const Expected& expected = expected_adcs_x[left][right];
      AdcsSbcsHelper(&MacroAssembler::Adcs, inputs[left], inputs[right], 0,
                     expected.carry0_result, expected.carry0_flags);
      AdcsSbcsHelper(&MacroAssembler::Adcs, inputs[left], inputs[right], 1,
                     expected.carry1_result, expected.carry1_flags);
    }
  }

  for (size_t left = 0; left < input_count; left++) {
    for (size_t right = 0; right < input_count; right++) {
      const Expected& expected = expected_sbcs_x[left][right];
      AdcsSbcsHelper(&MacroAssembler::Sbcs, inputs[left], inputs[right], 0,
                     expected.carry0_result, expected.carry0_flags);
      AdcsSbcsHelper(&MacroAssembler::Sbcs, inputs[left], inputs[right], 1,
                     expected.carry1_result, expected.carry1_flags);
    }
  }
}


TEST(adcs_sbcs_w) {
  INIT_V8();
  uint32_t inputs[] = {
      0x00000000, 0x00000001, 0x7FFFFFFE, 0x7FFFFFFF,
      0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF,
  };
  static const size_t input_count = sizeof(inputs) / sizeof(inputs[0]);

  struct Expected {
    uint32_t carry0_result;
    StatusFlags carry0_flags;
    uint32_t carry1_result;
    StatusFlags carry1_flags;
  };

  static const Expected expected_adcs_w[input_count][input_count] = {
      {{0x00000000, ZFlag, 0x00000001, NoFlag},
       {0x00000001, NoFlag, 0x00000002, NoFlag},
       {0x7FFFFFFE, NoFlag, 0x7FFFFFFF, NoFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0x80000000, NFlag, 0x80000001, NFlag},
       {0x80000001, NFlag, 0x80000002, NFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag}},
      {{0x00000001, NoFlag, 0x00000002, NoFlag},
       {0x00000002, NoFlag, 0x00000003, NoFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0x80000000, NVFlag, 0x80000001, NVFlag},
       {0x80000001, NFlag, 0x80000002, NFlag},
       {0x80000002, NFlag, 0x80000003, NFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag}},
      {{0x7FFFFFFE, NoFlag, 0x7FFFFFFF, NoFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0xFFFFFFFC, NVFlag, 0xFFFFFFFD, NVFlag},
       {0xFFFFFFFD, NVFlag, 0xFFFFFFFE, NVFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x7FFFFFFC, CFlag, 0x7FFFFFFD, CFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag}},
      {{0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0x80000000, NVFlag, 0x80000001, NVFlag},
       {0xFFFFFFFD, NVFlag, 0xFFFFFFFE, NVFlag},
       {0xFFFFFFFE, NVFlag, 0xFFFFFFFF, NVFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x7FFFFFFE, CFlag, 0x7FFFFFFF, CFlag}},
      {{0x80000000, NFlag, 0x80000001, NFlag},
       {0x80000001, NFlag, 0x80000002, NFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x00000000, ZCVFlag, 0x00000001, CVFlag},
       {0x00000001, CVFlag, 0x00000002, CVFlag},
       {0x7FFFFFFE, CVFlag, 0x7FFFFFFF, CVFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag}},
      {{0x80000001, NFlag, 0x80000002, NFlag},
       {0x80000002, NFlag, 0x80000003, NFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag},
       {0x00000001, CVFlag, 0x00000002, CVFlag},
       {0x00000002, CVFlag, 0x00000003, CVFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0x80000000, NCFlag, 0x80000001, NCFlag}},
      {{0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x7FFFFFFC, CFlag, 0x7FFFFFFD, CFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x7FFFFFFE, CVFlag, 0x7FFFFFFF, CVFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0xFFFFFFFC, NCFlag, 0xFFFFFFFD, NCFlag},
       {0xFFFFFFFD, NCFlag, 0xFFFFFFFE, NCFlag}},
      {{0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x7FFFFFFE, CFlag, 0x7FFFFFFF, CFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0x80000000, NCFlag, 0x80000001, NCFlag},
       {0xFFFFFFFD, NCFlag, 0xFFFFFFFE, NCFlag},
       {0xFFFFFFFE, NCFlag, 0xFFFFFFFF, NCFlag}}};

  static const Expected expected_sbcs_w[input_count][input_count] = {
      {{0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0x80000001, NFlag, 0x80000002, NFlag},
       {0x80000000, NFlag, 0x80000001, NFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0x7FFFFFFE, NoFlag, 0x7FFFFFFF, NoFlag},
       {0x00000001, NoFlag, 0x00000002, NoFlag},
       {0x00000000, ZFlag, 0x00000001, NoFlag}},
      {{0x00000000, ZCFlag, 0x00000001, CFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x80000002, NFlag, 0x80000003, NFlag},
       {0x80000001, NFlag, 0x80000002, NFlag},
       {0x80000000, NVFlag, 0x80000001, NVFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0x00000002, NoFlag, 0x00000003, NoFlag},
       {0x00000001, NoFlag, 0x00000002, NoFlag}},
      {{0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x7FFFFFFC, CFlag, 0x7FFFFFFD, CFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0xFFFFFFFD, NVFlag, 0xFFFFFFFE, NVFlag},
       {0xFFFFFFFC, NVFlag, 0xFFFFFFFD, NVFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag},
       {0x7FFFFFFE, NoFlag, 0x7FFFFFFF, NoFlag}},
      {{0x7FFFFFFE, CFlag, 0x7FFFFFFF, CFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0xFFFFFFFE, NVFlag, 0xFFFFFFFF, NVFlag},
       {0xFFFFFFFD, NVFlag, 0xFFFFFFFE, NVFlag},
       {0x80000000, NVFlag, 0x80000001, NVFlag},
       {0x7FFFFFFF, NoFlag, 0x80000000, NVFlag}},
      {{0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0x7FFFFFFE, CVFlag, 0x7FFFFFFF, CVFlag},
       {0x00000001, CVFlag, 0x00000002, CVFlag},
       {0x00000000, ZCVFlag, 0x00000001, CVFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag},
       {0x80000001, NFlag, 0x80000002, NFlag},
       {0x80000000, NFlag, 0x80000001, NFlag}},
      {{0x80000000, NCFlag, 0x80000001, NCFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0x00000002, CVFlag, 0x00000003, CVFlag},
       {0x00000001, CVFlag, 0x00000002, CVFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0x80000002, NFlag, 0x80000003, NFlag},
       {0x80000001, NFlag, 0x80000002, NFlag}},
      {{0xFFFFFFFD, NCFlag, 0xFFFFFFFE, NCFlag},
       {0xFFFFFFFC, NCFlag, 0xFFFFFFFD, NCFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0x7FFFFFFE, CVFlag, 0x7FFFFFFF, CVFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x7FFFFFFC, CFlag, 0x7FFFFFFD, CFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag},
       {0xFFFFFFFE, NFlag, 0xFFFFFFFF, NFlag}},
      {{0xFFFFFFFE, NCFlag, 0xFFFFFFFF, NCFlag},
       {0xFFFFFFFD, NCFlag, 0xFFFFFFFE, NCFlag},
       {0x80000000, NCFlag, 0x80000001, NCFlag},
       {0x7FFFFFFF, CVFlag, 0x80000000, NCFlag},
       {0x7FFFFFFE, CFlag, 0x7FFFFFFF, CFlag},
       {0x7FFFFFFD, CFlag, 0x7FFFFFFE, CFlag},
       {0x00000000, ZCFlag, 0x00000001, CFlag},
       {0xFFFFFFFF, NFlag, 0x00000000, ZCFlag}}};

  for (size_t left = 0; left < input_count; left++) {
    for (size_t right = 0; right < input_count; right++) {
      const Expected& expected = expected_adcs_w[left][right];
      AdcsSbcsHelper(&MacroAssembler::Adcs, inputs[left], inputs[right], 0,
                     expected.carry0_result, expected.carry0_flags);
      AdcsSbcsHelper(&MacroAssembler::Adcs, inputs[left], inputs[right], 1,
                     expected.carry1_result, expected.carry1_flags);
    }
  }

  for (size_t left = 0; left < input_count; left++) {
    for (size_t right = 0; right < input_count; right++) {
      const Expected& expected = expected_sbcs_w[left][right];
      AdcsSbcsHelper(&MacroAssembler::Sbcs, inputs[left], inputs[right], 0,
                     expected.carry0_result, expected.carry0_flags);
      AdcsSbcsHelper(&MacroAssembler::Sbcs, inputs[left], inputs[right], 1,
                     expected.carry1_result, expected.carry1_flags);
    }
  }
}


TEST(adc_sbc_shift) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x0, 0);
  __ Mov(x1, 1);
  __ Mov(x2, 0x0123456789ABCDEFL);
  __ Mov(x3, 0xFEDCBA9876543210L);
  __ Mov(x4, 0xFFFFFFFFFFFFFFFFL);

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

  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFL, x5);
  CHECK_EQUAL_64(1L << 60, x6);
  CHECK_EQUAL_64(0xF0123456789ABCDDL, x7);
  CHECK_EQUAL_64(0x0111111111111110L, x8);
  CHECK_EQUAL_64(0x1222222222222221L, x9);

  CHECK_EQUAL_32(0xFFFFFFFF, w10);
  CHECK_EQUAL_32(1 << 30, w11);
  CHECK_EQUAL_32(0xF89ABCDD, w12);
  CHECK_EQUAL_32(0x91111110, w13);
  CHECK_EQUAL_32(0x9A222221, w14);

  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFL + 1, x18);
  CHECK_EQUAL_64((1L << 60) + 1, x19);
  CHECK_EQUAL_64(0xF0123456789ABCDDL + 1, x20);
  CHECK_EQUAL_64(0x0111111111111110L + 1, x21);
  CHECK_EQUAL_64(0x1222222222222221L + 1, x22);

  CHECK_EQUAL_32(0xFFFFFFFF + 1, w23);
  CHECK_EQUAL_32((1 << 30) + 1, w24);
  CHECK_EQUAL_32(0xF89ABCDD + 1, w25);
  CHECK_EQUAL_32(0x91111110 + 1, w26);
  CHECK_EQUAL_32(0x9A222221 + 1, w27);

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
  __ Mov(x2, 0x0123456789ABCDEFL);

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

  CHECK_EQUAL_64(0x1DF, x10);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF37BDL, x11);
  CHECK_EQUAL_64(0xFFFFFFF765432110L, x12);
  CHECK_EQUAL_64(0x123456789ABCDEF1L, x13);

  CHECK_EQUAL_32(0x1DF, w14);
  CHECK_EQUAL_32(0xFFFF37BD, w15);
  CHECK_EQUAL_32(0x9ABCDEF1, w9);

  CHECK_EQUAL_64(0x1DF + 1, x20);
  CHECK_EQUAL_64(0xFFFFFFFFFFFF37BDL + 1, x21);
  CHECK_EQUAL_64(0xFFFFFFF765432110L + 1, x22);
  CHECK_EQUAL_64(0x123456789ABCDEF1L + 1, x23);

  CHECK_EQUAL_32(0x1DF + 1, w24);
  CHECK_EQUAL_32(0xFFFF37BD + 1, w25);
  CHECK_EQUAL_32(0x9ABCDEF1 + 1, w26);

  // Check that adc correctly sets the condition flags.
  START();
  __ Mov(x0, 0xFF);
  __ Mov(x1, 0xFFFFFFFFFFFFFFFFL);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1, SXTX, 1));
  END();

  RUN();

  CHECK_EQUAL_NZCV(CFlag);

  START();
  __ Mov(x0, 0x7FFFFFFFFFFFFFFFL);
  __ Mov(x1, 1);
  // Clear the C flag.
  __ Adds(x0, x0, Operand(0));
  __ Adcs(x10, x0, Operand(x1, UXTB, 2));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  START();
  __ Mov(x0, 0x7FFFFFFFFFFFFFFFL);
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

  __ Adc(x7, x0, Operand(0x1234567890ABCDEFUL));
  __ Adc(w8, w0, Operand(0xFFFFFFFF));
  __ Sbc(x9, x0, Operand(0x1234567890ABCDEFUL));
  __ Sbc(w10, w0, Operand(0xFFFFFFFF));
  __ Ngc(x11, Operand(0xFFFFFFFF00000000UL));
  __ Ngc(w12, Operand(0xFFFF0000));

  // Set the C flag.
  __ Cmp(w0, Operand(w0));

  __ Adc(x18, x0, Operand(0x1234567890ABCDEFUL));
  __ Adc(w19, w0, Operand(0xFFFFFFFF));
  __ Sbc(x20, x0, Operand(0x1234567890ABCDEFUL));
  __ Sbc(w21, w0, Operand(0xFFFFFFFF));
  __ Ngc(x22, Operand(0xFFFFFFFF00000000UL));
  __ Ngc(w23, Operand(0xFFFF0000));
  END();

  RUN();

  CHECK_EQUAL_64(0x1234567890ABCDEFUL, x7);
  CHECK_EQUAL_64(0xFFFFFFFF, x8);
  CHECK_EQUAL_64(0xEDCBA9876F543210UL, x9);
  CHECK_EQUAL_64(0, x10);
  CHECK_EQUAL_64(0xFFFFFFFF, x11);
  CHECK_EQUAL_64(0xFFFF, x12);

  CHECK_EQUAL_64(0x1234567890ABCDEFUL + 1, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0xEDCBA9876F543211UL, x20);
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
  __ Mov(x1, 0x7FFFFFFFFFFFFFFFL);
  __ Cmn(x1, Operand(x0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  START();
  __ Mov(w0, 1);
  __ Mov(w1, 0x7FFFFFFF);
  __ Cmn(w1, Operand(w0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(NVFlag);

  START();
  __ Mov(x0, 1);
  __ Mov(x1, 0xFFFFFFFFFFFFFFFFL);
  __ Cmn(x1, Operand(x0));
  END();

  RUN();

  CHECK_EQUAL_NZCV(ZCFlag);

  START();
  __ Mov(w0, 1);
  __ Mov(w1, 0xFFFFFFFF);
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
  __ Mov(x18, 0xF0000000);
  __ Mov(x19, 0xF000000010000000UL);
  __ Mov(x20, 0xF0000000F0000000UL);
  __ Mov(x21, 0x7800000078000000UL);
  __ Mov(x22, 0x3C0000003C000000UL);
  __ Mov(x23, 0x8000000780000000UL);
  __ Mov(x24, 0x0000000F00000000UL);
  __ Mov(x25, 0x00000003C0000000UL);
  __ Mov(x26, 0x8000000780000000UL);
  __ Mov(x27, 0xC0000003);

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
  __ Mov(x22, 0xFFFFFFFFFFFFFFFFUL);
  __ Mov(x23, 0xFF);
  __ Mov(x24, 0xFFFFFFFFFFFFFFFEUL);
  __ Mov(x25, 0xFFFF);
  __ Mov(x26, 0xFFFFFFFF);

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
  __ Ccmp(x20, Operand(0xFFFFFFFFFFFFFFFFUL), NZCVFlag, eq);
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
  __ Mov(x22, 0xFFFFFFFFFFFFFFFFUL);
  __ Mov(x23, 0xFF);
  __ Mov(x24, 0xFFFFFFFFFFFFFFFEUL);

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
  __ Mov(x24, 0x0000000F0000000FUL);
  __ Mov(x25, 0x0000001F0000001FUL);
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

  CHECK_EQUAL_64(0x0000000F, x0);
  CHECK_EQUAL_64(0x0000001F, x1);
  CHECK_EQUAL_64(0x00000020, x2);
  CHECK_EQUAL_64(0x0000000F, x3);
  CHECK_EQUAL_64(0xFFFFFFE0FFFFFFE0UL, x4);
  CHECK_EQUAL_64(0x0000000F0000000FUL, x5);
  CHECK_EQUAL_64(0xFFFFFFE0FFFFFFE1UL, x6);
  CHECK_EQUAL_64(0x0000000F0000000FUL, x7);
  CHECK_EQUAL_64(0x00000001, x8);
  CHECK_EQUAL_64(0xFFFFFFFF, x9);
  CHECK_EQUAL_64(0x0000001F00000020UL, x10);
  CHECK_EQUAL_64(0xFFFFFFF0FFFFFFF0UL, x11);
  CHECK_EQUAL_64(0xFFFFFFF0FFFFFFF1UL, x12);
  CHECK_EQUAL_64(0x0000000F, x13);
  CHECK_EQUAL_64(0x0000000F0000000FUL, x14);
  CHECK_EQUAL_64(0x0000000F, x15);
  CHECK_EQUAL_64(0x0000000F0000000FUL, x18);
  CHECK_EQUAL_64(0, x24);
  CHECK_EQUAL_64(0x0000001F0000001FUL, x25);
  CHECK_EQUAL_64(0x0000001F0000001FUL, x26);
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

  uint64_t value = 0x0123456789ABCDEFUL;
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

  uint64_t value = 0x0123456789ABCDEFUL;
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

  value &= 0xFFFFFFFFUL;
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

  int64_t value = 0xFEDCBA98FEDCBA98UL;
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

  int32_t value32 = static_cast<int32_t>(value & 0xFFFFFFFFUL);
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

  uint64_t value = 0x0123456789ABCDEFUL;
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
  CHECK_EQUAL_64(0xF0123456789ABCDEUL, x16);
  CHECK_EQUAL_64(0xEF0123456789ABCDUL, x17);
  CHECK_EQUAL_64(0xDEF0123456789ABCUL, x18);
  CHECK_EQUAL_64(0xCDEF0123456789ABUL, x19);
  CHECK_EQUAL_64(0xABCDEF0123456789UL, x20);
  CHECK_EQUAL_64(0x789ABCDEF0123456UL, x21);
  CHECK_EQUAL_32(0xF89ABCDE, w22);
  CHECK_EQUAL_32(0xEF89ABCD, w23);
  CHECK_EQUAL_32(0xDEF89ABC, w24);
  CHECK_EQUAL_32(0xCDEF89AB, w25);
  CHECK_EQUAL_32(0xABCDEF89, w26);
  CHECK_EQUAL_32(0xF89ABCDE, w27);

  TEARDOWN();
}


TEST(bfm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789ABCDEFL);

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

  CHECK_EQUAL_64(0x88888888888889ABL, x10);
  CHECK_EQUAL_64(0x8888CDEF88888888L, x11);

  CHECK_EQUAL_32(0x888888AB, w20);
  CHECK_EQUAL_32(0x88CDEF88, w21);

  CHECK_EQUAL_64(0x8888888888EF8888L, x12);
  CHECK_EQUAL_64(0x88888888888888ABL, x13);

  TEARDOWN();
}


TEST(sbfm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789ABCDEFL);
  __ Mov(x2, 0xFEDCBA9876543210L);

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

  CHECK_EQUAL_64(0xFFFFFFFFFFFF89ABL, x10);
  CHECK_EQUAL_64(0xFFFFCDEF00000000L, x11);
  CHECK_EQUAL_64(0x4567L, x12);
  CHECK_EQUAL_64(0x789ABCDEF0000L, x13);

  CHECK_EQUAL_32(0xFFFFFFAB, w14);
  CHECK_EQUAL_32(0xFFCDEF00, w15);
  CHECK_EQUAL_32(0x54, w16);
  CHECK_EQUAL_32(0x00321000, w17);

  CHECK_EQUAL_64(0x01234567L, x18);
  CHECK_EQUAL_64(0xFFFFFFFFFEDCBA98L, x19);
  CHECK_EQUAL_64(0xFFFFFFFFFFCDEF00L, x20);
  CHECK_EQUAL_64(0x321000L, x21);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFABCDL, x22);
  CHECK_EQUAL_64(0x5432L, x23);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFEFL, x24);
  CHECK_EQUAL_64(0x10, x25);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFCDEFL, x26);
  CHECK_EQUAL_64(0x3210, x27);
  CHECK_EQUAL_64(0xFFFFFFFF89ABCDEFL, x28);
  CHECK_EQUAL_64(0x76543210, x29);

  TEARDOWN();
}


TEST(ubfm) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789ABCDEFL);
  __ Mov(x2, 0xFEDCBA9876543210L);

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

  CHECK_EQUAL_64(0x00000000000089ABL, x10);
  CHECK_EQUAL_64(0x0000CDEF00000000L, x11);
  CHECK_EQUAL_64(0x4567L, x12);
  CHECK_EQUAL_64(0x789ABCDEF0000L, x13);

  CHECK_EQUAL_32(0x000000AB, w25);
  CHECK_EQUAL_32(0x00CDEF00, w26);
  CHECK_EQUAL_32(0x54, w27);
  CHECK_EQUAL_32(0x00321000, w28);

  CHECK_EQUAL_64(0x8000000000000000L, x15);
  CHECK_EQUAL_64(0x0123456789ABCDEFL, x16);
  CHECK_EQUAL_64(0x01234567L, x17);
  CHECK_EQUAL_64(0xCDEF00L, x18);
  CHECK_EQUAL_64(0xABCDL, x19);
  CHECK_EQUAL_64(0xEFL, x20);
  CHECK_EQUAL_64(0xCDEFL, x21);
  CHECK_EQUAL_64(0x89ABCDEFL, x22);

  TEARDOWN();
}


TEST(extr) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x1, 0x0123456789ABCDEFL);
  __ Mov(x2, 0xFEDCBA9876543210L);

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
  CHECK_EQUAL_64(0xFEDCBA9876543210L, x11);
  CHECK_EQUAL_64(0xBB2A1908, x12);
  CHECK_EQUAL_64(0x0048D159E26AF37BUL, x13);
  CHECK_EQUAL_64(0x89ABCDEF, x20);
  CHECK_EQUAL_64(0x0123456789ABCDEFL, x21);
  CHECK_EQUAL_64(0x19083B2A, x22);
  CHECK_EQUAL_64(0x13579BDF, x23);
  CHECK_EQUAL_64(0x7F6E5D4C3B2A1908UL, x24);
  CHECK_EQUAL_64(0x02468ACF13579BDEUL, x25);

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
  __ Fmov(d6, bit_cast<double>(0x0123456789ABCDEFL));
  __ Fmov(s6, s6);
  END();

  RUN();

  CHECK_EQUAL_32(bit_cast<uint32_t>(1.0f), w10);
  CHECK_EQUAL_FP32(1.0, s30);
  CHECK_EQUAL_FP32(1.0, s5);
  CHECK_EQUAL_64(bit_cast<uint64_t>(-13.0), x1);
  CHECK_EQUAL_FP64(-13.0, d2);
  CHECK_EQUAL_FP64(-13.0, d4);
  CHECK_EQUAL_FP32(bit_cast<float>(0x89ABCDEF), s6);

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
  double s1 = bit_cast<double>(0x7FF5555511111111);
  double s2 = bit_cast<double>(0x7FF5555522222222);
  double sa = bit_cast<double>(0x7FF55555AAAAAAAA);
  double q1 = bit_cast<double>(0x7FFAAAAA11111111);
  double q2 = bit_cast<double>(0x7FFAAAAA22222222);
  double qa = bit_cast<double>(0x7FFAAAAAAAAAAAAA);
  CHECK(IsSignallingNaN(s1));
  CHECK(IsSignallingNaN(s2));
  CHECK(IsSignallingNaN(sa));
  CHECK(IsQuietNaN(q1));
  CHECK(IsQuietNaN(q2));
  CHECK(IsQuietNaN(qa));

  // The input NaNs after passing through ProcessNaN.
  double s1_proc = bit_cast<double>(0x7FFD555511111111);
  double s2_proc = bit_cast<double>(0x7FFD555522222222);
  double sa_proc = bit_cast<double>(0x7FFD5555AAAAAAAA);
  double q1_proc = q1;
  double q2_proc = q2;
  double qa_proc = qa;
  CHECK(IsQuietNaN(s1_proc));
  CHECK(IsQuietNaN(s2_proc));
  CHECK(IsQuietNaN(sa_proc));
  CHECK(IsQuietNaN(q1_proc));
  CHECK(IsQuietNaN(q2_proc));
  CHECK(IsQuietNaN(qa_proc));

  // Negated NaNs as it would be done on ARMv8 hardware.
  double s1_proc_neg = bit_cast<double>(0xFFFD555511111111);
  double sa_proc_neg = bit_cast<double>(0xFFFD5555AAAAAAAA);
  double q1_proc_neg = bit_cast<double>(0xFFFAAAAA11111111);
  double qa_proc_neg = bit_cast<double>(0xFFFAAAAAAAAAAAAA);
  CHECK(IsQuietNaN(s1_proc_neg));
  CHECK(IsQuietNaN(sa_proc_neg));
  CHECK(IsQuietNaN(q1_proc_neg));
  CHECK(IsQuietNaN(qa_proc_neg));

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
  float s1 = bit_cast<float>(0x7F951111);
  float s2 = bit_cast<float>(0x7F952222);
  float sa = bit_cast<float>(0x7F95AAAA);
  float q1 = bit_cast<float>(0x7FEA1111);
  float q2 = bit_cast<float>(0x7FEA2222);
  float qa = bit_cast<float>(0x7FEAAAAA);
  CHECK(IsSignallingNaN(s1));
  CHECK(IsSignallingNaN(s2));
  CHECK(IsSignallingNaN(sa));
  CHECK(IsQuietNaN(q1));
  CHECK(IsQuietNaN(q2));
  CHECK(IsQuietNaN(qa));

  // The input NaNs after passing through ProcessNaN.
  float s1_proc = bit_cast<float>(0x7FD51111);
  float s2_proc = bit_cast<float>(0x7FD52222);
  float sa_proc = bit_cast<float>(0x7FD5AAAA);
  float q1_proc = q1;
  float q2_proc = q2;
  float qa_proc = qa;
  CHECK(IsQuietNaN(s1_proc));
  CHECK(IsQuietNaN(s2_proc));
  CHECK(IsQuietNaN(sa_proc));
  CHECK(IsQuietNaN(q1_proc));
  CHECK(IsQuietNaN(q2_proc));
  CHECK(IsQuietNaN(qa_proc));

  // Negated NaNs as it would be done on ARMv8 hardware.
  float s1_proc_neg = bit_cast<float>(0xFFD51111);
  float sa_proc_neg = bit_cast<float>(0xFFD5AAAA);
  float q1_proc_neg = bit_cast<float>(0xFFEA1111);
  float qa_proc_neg = bit_cast<float>(0xFFEAAAAA);
  CHECK(IsQuietNaN(s1_proc_neg));
  CHECK(IsQuietNaN(sa_proc_neg));
  CHECK(IsQuietNaN(q1_proc_neg));
  CHECK(IsQuietNaN(qa_proc_neg));

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
  uint32_t raw_n = bit_cast<uint32_t>(n);
  uint32_t raw_m = bit_cast<uint32_t>(m);

  if (std::isnan(n) && ((raw_n & kSQuietNanMask) == 0)) {
    // n is signalling NaN.
    return bit_cast<float>(raw_n | static_cast<uint32_t>(kSQuietNanMask));
  } else if (std::isnan(m) && ((raw_m & kSQuietNanMask) == 0)) {
    // m is signalling NaN.
    return bit_cast<float>(raw_m | static_cast<uint32_t>(kSQuietNanMask));
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
  uint64_t raw_n = bit_cast<uint64_t>(n);
  uint64_t raw_m = bit_cast<uint64_t>(m);

  if (std::isnan(n) && ((raw_n & kDQuietNanMask) == 0)) {
    // n is signalling NaN.
    return bit_cast<double>(raw_n | kDQuietNanMask);
  } else if (std::isnan(m) && ((raw_m & kDQuietNanMask) == 0)) {
    // m is signalling NaN.
    return bit_cast<double>(raw_m | kDQuietNanMask);
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
  double snan = bit_cast<double>(0x7FF5555512345678);
  double qnan = bit_cast<double>(0x7FFAAAAA87654321);

  double snan_processed = bit_cast<double>(0x7FFD555512345678);
  double qnan_processed = qnan;

  CHECK(IsSignallingNaN(snan));
  CHECK(IsQuietNaN(qnan));
  CHECK(IsQuietNaN(snan_processed));
  CHECK(IsQuietNaN(qnan_processed));

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
  float snan = bit_cast<float>(0x7F951234);
  float qnan = bit_cast<float>(0x7FEA8765);

  float snan_processed = bit_cast<float>(0x7FD51234);
  float qnan_processed = qnan;

  CHECK(IsSignallingNaN(snan));
  CHECK(IsQuietNaN(qnan));
  CHECK(IsQuietNaN(snan_processed));
  CHECK(IsQuietNaN(qnan_processed));

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
    __ Mov(w18, 0x7F800001);  // Single precision NaN.
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
    masm.FPTmpList()->set_list(d0.bit());
    __ Fcmp(s8, 255.0);
    masm.FPTmpList()->set_list(0);
    __ Mrs(x6, NZCV);

    __ Fmov(d19, 0.0);
    __ Fmov(d20, 0.5);
    __ Mov(x21, 0x7FF0000000000001UL);  // Double precision NaN.
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
    masm.FPTmpList()->set_list(d0.bit());
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
  __ Fmov(s29, bit_cast<float>(0x7FC12345));  // Quiet NaN.
  __ Fmov(s30, bit_cast<float>(0x7F812345));  // Signalling NaN.

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
  CHECK_EQUAL_FP64(bit_cast<double>(0x7FF82468A0000000), d13);
  CHECK_EQUAL_FP64(bit_cast<double>(0x7FF82468A0000000), d14);

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
  struct {
    double in;
    float expected;
  } test[] = {
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
      {bit_cast<double>(0x3FF0000000000000), bit_cast<float>(0x3F800000)},
      {bit_cast<double>(0x3FF0000000000001), bit_cast<float>(0x3F800000)},
      {bit_cast<double>(0x3FF0000010000000), bit_cast<float>(0x3F800000)},
      {bit_cast<double>(0x3FF0000010000001), bit_cast<float>(0x3F800001)},
      {bit_cast<double>(0x3FF0000020000000), bit_cast<float>(0x3F800001)},
      {bit_cast<double>(0x3FF0000020000001), bit_cast<float>(0x3F800001)},
      {bit_cast<double>(0x3FF0000030000000), bit_cast<float>(0x3F800002)},
      {bit_cast<double>(0x3FF0000030000001), bit_cast<float>(0x3F800002)},
      {bit_cast<double>(0x3FF0000040000000), bit_cast<float>(0x3F800002)},
      {bit_cast<double>(0x3FF0000040000001), bit_cast<float>(0x3F800002)},
      {bit_cast<double>(0x3FF0000050000000), bit_cast<float>(0x3F800002)},
      {bit_cast<double>(0x3FF0000050000001), bit_cast<float>(0x3F800003)},
      {bit_cast<double>(0x3FF0000060000000), bit_cast<float>(0x3F800003)},
      //  - A mantissa that overflows into the exponent during rounding.
      {bit_cast<double>(0x3FEFFFFFF0000000), bit_cast<float>(0x3F800000)},
      //  - The largest double that rounds to a normal float.
      {bit_cast<double>(0x47EFFFFFEFFFFFFF), bit_cast<float>(0x7F7FFFFF)},

      // Doubles that are too big for a float.
      {kFP64PositiveInfinity, kFP32PositiveInfinity},
      {DBL_MAX, kFP32PositiveInfinity},
      //  - The smallest exponent that's too big for a float.
      {pow(2.0, 128), kFP32PositiveInfinity},
      //  - This exponent is in range, but the value rounds to infinity.
      {bit_cast<double>(0x47EFFFFFF0000000), kFP32PositiveInfinity},

      // Doubles that are too small for a float.
      //  - The smallest (subnormal) double.
      {DBL_MIN, 0.0},
      //  - The largest double which is too small for a subnormal float.
      {bit_cast<double>(0x3690000000000000), bit_cast<float>(0x00000000)},

      // Normal doubles that become subnormal floats.
      //  - The largest subnormal float.
      {bit_cast<double>(0x380FFFFFC0000000), bit_cast<float>(0x007FFFFF)},
      //  - The smallest subnormal float.
      {bit_cast<double>(0x36A0000000000000), bit_cast<float>(0x00000001)},
      //  - Subnormal floats that need (ties-to-even) rounding.
      //    For these subnormals:
      //         bit 34 (0x0000000400000000) is the lowest-order bit which will
      //                                     fit in the float's mantissa.
      {bit_cast<double>(0x37C159E000000000), bit_cast<float>(0x00045678)},
      {bit_cast<double>(0x37C159E000000001), bit_cast<float>(0x00045678)},
      {bit_cast<double>(0x37C159E200000000), bit_cast<float>(0x00045678)},
      {bit_cast<double>(0x37C159E200000001), bit_cast<float>(0x00045679)},
      {bit_cast<double>(0x37C159E400000000), bit_cast<float>(0x00045679)},
      {bit_cast<double>(0x37C159E400000001), bit_cast<float>(0x00045679)},
      {bit_cast<double>(0x37C159E600000000), bit_cast<float>(0x0004567A)},
      {bit_cast<double>(0x37C159E600000001), bit_cast<float>(0x0004567A)},
      {bit_cast<double>(0x37C159E800000000), bit_cast<float>(0x0004567A)},
      {bit_cast<double>(0x37C159E800000001), bit_cast<float>(0x0004567A)},
      {bit_cast<double>(0x37C159EA00000000), bit_cast<float>(0x0004567A)},
      {bit_cast<double>(0x37C159EA00000001), bit_cast<float>(0x0004567B)},
      {bit_cast<double>(0x37C159EC00000000), bit_cast<float>(0x0004567B)},
      //  - The smallest double which rounds up to become a subnormal float.
      {bit_cast<double>(0x3690000000000001), bit_cast<float>(0x00000001)},

      // Check NaN payload preservation.
      {bit_cast<double>(0x7FF82468A0000000), bit_cast<float>(0x7FC12345)},
      {bit_cast<double>(0x7FF82468BFFFFFFF), bit_cast<float>(0x7FC12345)},
      //  - Signalling NaNs become quiet NaNs.
      {bit_cast<double>(0x7FF02468A0000000), bit_cast<float>(0x7FC12345)},
      {bit_cast<double>(0x7FF02468BFFFFFFF), bit_cast<float>(0x7FC12345)},
      {bit_cast<double>(0x7FF000001FFFFFFF), bit_cast<float>(0x7FC00000)},
  };
  int count = sizeof(test) / sizeof(test[0]);

  for (int i = 0; i < count; i++) {
    double in = test[i].in;
    float expected = test[i].expected;

    // We only expect positive input.
    CHECK_EQ(std::signbit(in), 0);
    CHECK_EQ(std::signbit(expected), 0);

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
  __ Fmov(s6, 0x7FFFFF80);  // Largest float < INT32_MAX.
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
  __ Fmov(s22, 0x7FFFFF8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 2.5);
  __ Fmov(d26, -2.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7FFFFFFFFFFFFC00UL);   // Largest double < INT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFD, x3);
  CHECK_EQUAL_64(0x7FFFFFFF, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7FFFFF80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(3, x10);
  CHECK_EQUAL_64(0xFFFFFFFD, x11);
  CHECK_EQUAL_64(0x7FFFFFFF, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7FFFFFFE, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(3, x18);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFDUL, x19);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7FFFFF8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(3, x25);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFDUL, x26);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x27);
  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFC00UL, x29);
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
  __ Fmov(s6, 0xFFFFFF00);  // Largest float < UINT32_MAX.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 2.5);
  __ Fmov(d11, -2.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, 0xFFFFFFFE);
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 2.5);
  __ Fmov(s19, -2.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0xFFFFFF0000000000UL);  // Largest float < UINT64_MAX.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 2.5);
  __ Fmov(d26, -2.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0xFFFFFFFFFFFFF800UL);  // Largest double < UINT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFF, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0xFFFFFF00, x6);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(3, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xFFFFFFFF, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0xFFFFFFFE, x14);
  CHECK_EQUAL_64(1, x16);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(3, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0, x21);
  CHECK_EQUAL_64(0xFFFFFF0000000000UL, x22);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(3, x25);
  CHECK_EQUAL_64(0, x26);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x27);
  CHECK_EQUAL_64(0, x28);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFF800UL, x29);
  CHECK_EQUAL_64(0xFFFFFFFF, x30);

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
  __ Fmov(s6, 0x7FFFFF80);  // Largest float < INT32_MAX.
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
  __ Fmov(s22, 0x7FFFFF8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7FFFFFFFFFFFFC00UL);   // Largest double < INT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFE, x3);
  CHECK_EQUAL_64(0x7FFFFFFF, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7FFFFF80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0xFFFFFFFE, x11);
  CHECK_EQUAL_64(0x7FFFFFFF, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7FFFFFFE, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFEUL, x19);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7FFFFF8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFEUL, x26);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x27);
  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFC00UL, x29);
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
  __ Fmov(s6, 0x7FFFFF80);  // Largest float < INT32_MAX.
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
  __ Fmov(s22, 0x7FFFFF8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7FFFFFFFFFFFFC00UL);   // Largest double < INT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFF, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0x7FFFFF80, x6);
  CHECK_EQUAL_64(0, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xFFFFFFFF, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0x7FFFFFFE, x14);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0x0UL, x19);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0x0UL, x21);
  CHECK_EQUAL_64(0x7FFFFF8000000000UL, x22);
  CHECK_EQUAL_64(0x0UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0x0UL, x26);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x27);
  CHECK_EQUAL_64(0x0UL, x28);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFC00UL, x29);
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
  __ Fmov(s6, 0x7FFFFF80);  // Largest float < INT32_MAX.
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
  __ Fmov(s22, 0x7FFFFF8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7FFFFFFFFFFFFC00UL);   // Largest double < INT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFE, x3);
  CHECK_EQUAL_64(0x7FFFFFFF, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7FFFFF80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(2, x10);
  CHECK_EQUAL_64(0xFFFFFFFE, x11);
  CHECK_EQUAL_64(0x7FFFFFFF, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7FFFFFFE, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(2, x18);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFEUL, x19);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7FFFFF8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(2, x25);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFEUL, x26);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x27);
  //  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFC00UL, x29);
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
  __ Fmov(s6, 0xFFFFFF00);  // Largest float < UINT32_MAX.
  __ Fmov(d8, 1.0);
  __ Fmov(d9, 1.1);
  __ Fmov(d10, 1.5);
  __ Fmov(d11, -1.5);
  __ Fmov(d12, kFP64PositiveInfinity);
  __ Fmov(d13, kFP64NegativeInfinity);
  __ Fmov(d14, 0xFFFFFFFE);
  __ Fmov(s16, 1.0);
  __ Fmov(s17, 1.1);
  __ Fmov(s18, 1.5);
  __ Fmov(s19, -1.5);
  __ Fmov(s20, kFP32PositiveInfinity);
  __ Fmov(s21, kFP32NegativeInfinity);
  __ Fmov(s22, 0xFFFFFF0000000000UL);  // Largest float < UINT64_MAX.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0xFFFFFFFFFFFFF800UL);  // Largest double < UINT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFF, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0xFFFFFF00, x6);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(2, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xFFFFFFFF, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0xFFFFFFFE, x14);
  CHECK_EQUAL_64(1, x16);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(2, x18);
  CHECK_EQUAL_64(0, x19);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0, x21);
  CHECK_EQUAL_64(0xFFFFFF0000000000UL, x22);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(2, x25);
  CHECK_EQUAL_64(0, x26);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x27);
  //  CHECK_EQUAL_64(0, x28);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFF800UL, x29);
  CHECK_EQUAL_64(0xFFFFFFFF, x30);

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
  __ Fmov(s6, 0x7FFFFF80);  // Largest float < INT32_MAX.
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
  __ Fmov(s22, 0x7FFFFF8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7FFFFFFFFFFFFC00UL);   // Largest double < INT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFF, x3);
  CHECK_EQUAL_64(0x7FFFFFFF, x4);
  CHECK_EQUAL_64(0x80000000, x5);
  CHECK_EQUAL_64(0x7FFFFF80, x6);
  CHECK_EQUAL_64(0x80000080, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0xFFFFFFFF, x11);
  CHECK_EQUAL_64(0x7FFFFFFF, x12);
  CHECK_EQUAL_64(0x80000000, x13);
  CHECK_EQUAL_64(0x7FFFFFFE, x14);
  CHECK_EQUAL_64(0x80000001, x15);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x19);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0x8000000000000000UL, x21);
  CHECK_EQUAL_64(0x7FFFFF8000000000UL, x22);
  CHECK_EQUAL_64(0x8000008000000000UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x26);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFFFFUL, x27);
  CHECK_EQUAL_64(0x8000000000000000UL, x28);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFC00UL, x29);
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
  __ Fmov(s6, 0x7FFFFF80);  // Largest float < INT32_MAX.
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
  __ Fmov(s22, 0x7FFFFF8000000000UL);   // Largest float < INT64_MAX.
  __ Fneg(s23, s22);                    // Smallest float > INT64_MIN.
  __ Fmov(d24, 1.1);
  __ Fmov(d25, 1.5);
  __ Fmov(d26, -1.5);
  __ Fmov(d27, kFP64PositiveInfinity);
  __ Fmov(d28, kFP64NegativeInfinity);
  __ Fmov(d29, 0x7FFFFFFFFFFFFC00UL);   // Largest double < INT64_MAX.
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
  CHECK_EQUAL_64(0xFFFFFFFF, x4);
  CHECK_EQUAL_64(0, x5);
  CHECK_EQUAL_64(0x7FFFFF80, x6);
  CHECK_EQUAL_64(0, x7);
  CHECK_EQUAL_64(1, x8);
  CHECK_EQUAL_64(1, x9);
  CHECK_EQUAL_64(1, x10);
  CHECK_EQUAL_64(0, x11);
  CHECK_EQUAL_64(0xFFFFFFFF, x12);
  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(0x7FFFFFFE, x14);
  CHECK_EQUAL_64(1, x17);
  CHECK_EQUAL_64(1, x18);
  CHECK_EQUAL_64(0x0UL, x19);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x20);
  CHECK_EQUAL_64(0x0UL, x21);
  CHECK_EQUAL_64(0x7FFFFF8000000000UL, x22);
  CHECK_EQUAL_64(0x0UL, x23);
  CHECK_EQUAL_64(1, x24);
  CHECK_EQUAL_64(1, x25);
  CHECK_EQUAL_64(0x0UL, x26);
  CHECK_EQUAL_64(0xFFFFFFFFFFFFFFFFUL, x27);
  CHECK_EQUAL_64(0x0UL, x28);
  CHECK_EQUAL_64(0x7FFFFFFFFFFFFC00UL, x29);
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
  uint32_t u32 = u64 & 0xFFFFFFFF;
  int64_t s64 = static_cast<int64_t>(in);
  int32_t s32 = s64 & 0x7FFFFFFF;

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
  double expected_scvtf_base = bit_cast<double>(expected_scvtf_bits);
  double expected_ucvtf_base = bit_cast<double>(expected_ucvtf_bits);

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
  TestUScvtfHelper(0x0000000000000001, 0x3FF0000000000000, 0x3FF0000000000000);
  TestUScvtfHelper(0x0000000040000000, 0x41D0000000000000, 0x41D0000000000000);
  TestUScvtfHelper(0x0000000100000000, 0x41F0000000000000, 0x41F0000000000000);
  TestUScvtfHelper(0x4000000000000000, 0x43D0000000000000, 0x43D0000000000000);
  // Test mantissa extremities.
  TestUScvtfHelper(0x4000000000000400, 0x43D0000000000001, 0x43D0000000000001);
  // The largest int32_t that fits in a double.
  TestUScvtfHelper(0x000000007FFFFFFF, 0x41DFFFFFFFC00000, 0x41DFFFFFFFC00000);
  // Values that would be negative if treated as an int32_t.
  TestUScvtfHelper(0x00000000FFFFFFFF, 0x41EFFFFFFFE00000, 0x41EFFFFFFFE00000);
  TestUScvtfHelper(0x0000000080000000, 0x41E0000000000000, 0x41E0000000000000);
  TestUScvtfHelper(0x0000000080000001, 0x41E0000000200000, 0x41E0000000200000);
  // The largest int64_t that fits in a double.
  TestUScvtfHelper(0x7FFFFFFFFFFFFC00, 0x43DFFFFFFFFFFFFF, 0x43DFFFFFFFFFFFFF);
  // Check for bit pattern reproduction.
  TestUScvtfHelper(0x0123456789ABCDE0, 0x43723456789ABCDE, 0x43723456789ABCDE);
  TestUScvtfHelper(0x0000000012345678, 0x41B2345678000000, 0x41B2345678000000);

  // Simple conversions of negative int64_t values. These require no rounding,
  // and the results should not depend on the rounding mode.
  TestUScvtfHelper(0xFFFFFFFFC0000000, 0xC1D0000000000000, 0x43EFFFFFFFF80000);
  TestUScvtfHelper(0xFFFFFFFF00000000, 0xC1F0000000000000, 0x43EFFFFFFFE00000);
  TestUScvtfHelper(0xC000000000000000, 0xC3D0000000000000, 0x43E8000000000000);

  // Conversions which require rounding.
  TestUScvtfHelper(0x1000000000000000, 0x43B0000000000000, 0x43B0000000000000);
  TestUScvtfHelper(0x1000000000000001, 0x43B0000000000000, 0x43B0000000000000);
  TestUScvtfHelper(0x1000000000000080, 0x43B0000000000000, 0x43B0000000000000);
  TestUScvtfHelper(0x1000000000000081, 0x43B0000000000001, 0x43B0000000000001);
  TestUScvtfHelper(0x1000000000000100, 0x43B0000000000001, 0x43B0000000000001);
  TestUScvtfHelper(0x1000000000000101, 0x43B0000000000001, 0x43B0000000000001);
  TestUScvtfHelper(0x1000000000000180, 0x43B0000000000002, 0x43B0000000000002);
  TestUScvtfHelper(0x1000000000000181, 0x43B0000000000002, 0x43B0000000000002);
  TestUScvtfHelper(0x1000000000000200, 0x43B0000000000002, 0x43B0000000000002);
  TestUScvtfHelper(0x1000000000000201, 0x43B0000000000002, 0x43B0000000000002);
  TestUScvtfHelper(0x1000000000000280, 0x43B0000000000002, 0x43B0000000000002);
  TestUScvtfHelper(0x1000000000000281, 0x43B0000000000003, 0x43B0000000000003);
  TestUScvtfHelper(0x1000000000000300, 0x43B0000000000003, 0x43B0000000000003);
  // Check rounding of negative int64_t values (and large uint64_t values).
  TestUScvtfHelper(0x8000000000000000, 0xC3E0000000000000, 0x43E0000000000000);
  TestUScvtfHelper(0x8000000000000001, 0xC3E0000000000000, 0x43E0000000000000);
  TestUScvtfHelper(0x8000000000000200, 0xC3E0000000000000, 0x43E0000000000000);
  TestUScvtfHelper(0x8000000000000201, 0xC3DFFFFFFFFFFFFF, 0x43E0000000000000);
  TestUScvtfHelper(0x8000000000000400, 0xC3DFFFFFFFFFFFFF, 0x43E0000000000000);
  TestUScvtfHelper(0x8000000000000401, 0xC3DFFFFFFFFFFFFF, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000600, 0xC3DFFFFFFFFFFFFE, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000601, 0xC3DFFFFFFFFFFFFE, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000800, 0xC3DFFFFFFFFFFFFE, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000801, 0xC3DFFFFFFFFFFFFE, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000A00, 0xC3DFFFFFFFFFFFFE, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000A01, 0xC3DFFFFFFFFFFFFD, 0x43E0000000000001);
  TestUScvtfHelper(0x8000000000000C00, 0xC3DFFFFFFFFFFFFD, 0x43E0000000000002);
  // Round up to produce a result that's too big for the input to represent.
  TestUScvtfHelper(0x7FFFFFFFFFFFFE00, 0x43E0000000000000, 0x43E0000000000000);
  TestUScvtfHelper(0x7FFFFFFFFFFFFFFF, 0x43E0000000000000, 0x43E0000000000000);
  TestUScvtfHelper(0xFFFFFFFFFFFFFC00, 0xC090000000000000, 0x43F0000000000000);
  TestUScvtfHelper(0xFFFFFFFFFFFFFFFF, 0xBFF0000000000000, 0x43F0000000000000);
}


// The same as TestUScvtfHelper, but convert to floats.
static void TestUScvtf32Helper(uint64_t in,
                               uint32_t expected_scvtf_bits,
                               uint32_t expected_ucvtf_bits) {
  uint64_t u64 = in;
  uint32_t u32 = u64 & 0xFFFFFFFF;
  int64_t s64 = static_cast<int64_t>(in);
  int32_t s32 = s64 & 0x7FFFFFFF;

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
  float expected_scvtf_base = bit_cast<float>(expected_scvtf_bits);
  float expected_ucvtf_base = bit_cast<float>(expected_ucvtf_bits);

  for (int fbits = 0; fbits <= 32; fbits++) {
    float expected_scvtf = expected_scvtf_base / powf(2, fbits);
    float expected_ucvtf = expected_ucvtf_base / powf(2, fbits);
    CHECK_EQUAL_FP32(expected_scvtf, results_scvtf_x[fbits]);
    CHECK_EQUAL_FP32(expected_ucvtf, results_ucvtf_x[fbits]);
    if (cvtf_s32) CHECK_EQUAL_FP32(expected_scvtf, results_scvtf_w[fbits]);
    if (cvtf_u32) CHECK_EQUAL_FP32(expected_ucvtf, results_ucvtf_w[fbits]);
  }
  for (int fbits = 33; fbits <= 64; fbits++) {
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
  TestUScvtf32Helper(0x0000000000000001, 0x3F800000, 0x3F800000);
  TestUScvtf32Helper(0x0000000040000000, 0x4E800000, 0x4E800000);
  TestUScvtf32Helper(0x0000000100000000, 0x4F800000, 0x4F800000);
  TestUScvtf32Helper(0x4000000000000000, 0x5E800000, 0x5E800000);
  // Test mantissa extremities.
  TestUScvtf32Helper(0x0000000000800001, 0x4B000001, 0x4B000001);
  TestUScvtf32Helper(0x4000008000000000, 0x5E800001, 0x5E800001);
  // The largest int32_t that fits in a float.
  TestUScvtf32Helper(0x000000007FFFFF80, 0x4EFFFFFF, 0x4EFFFFFF);
  // Values that would be negative if treated as an int32_t.
  TestUScvtf32Helper(0x00000000FFFFFF00, 0x4F7FFFFF, 0x4F7FFFFF);
  TestUScvtf32Helper(0x0000000080000000, 0x4F000000, 0x4F000000);
  TestUScvtf32Helper(0x0000000080000100, 0x4F000001, 0x4F000001);
  // The largest int64_t that fits in a float.
  TestUScvtf32Helper(0x7FFFFF8000000000, 0x5EFFFFFF, 0x5EFFFFFF);
  // Check for bit pattern reproduction.
  TestUScvtf32Helper(0x0000000000876543, 0x4B076543, 0x4B076543);

  // Simple conversions of negative int64_t values. These require no rounding,
  // and the results should not depend on the rounding mode.
  TestUScvtf32Helper(0xFFFFFC0000000000, 0xD4800000, 0x5F7FFFFC);
  TestUScvtf32Helper(0xC000000000000000, 0xDE800000, 0x5F400000);

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
  TestUScvtf32Helper(0x8000000000000000, 0xDF000000, 0x5F000000);
  TestUScvtf32Helper(0x8000000000000001, 0xDF000000, 0x5F000000);
  TestUScvtf32Helper(0x8000004000000000, 0xDF000000, 0x5F000000);
  TestUScvtf32Helper(0x8000004000000001, 0xDEFFFFFF, 0x5F000000);
  TestUScvtf32Helper(0x8000008000000000, 0xDEFFFFFF, 0x5F000000);
  TestUScvtf32Helper(0x8000008000000001, 0xDEFFFFFF, 0x5F000001);
  TestUScvtf32Helper(0x800000C000000000, 0xDEFFFFFE, 0x5F000001);
  TestUScvtf32Helper(0x800000C000000001, 0xDEFFFFFE, 0x5F000001);
  TestUScvtf32Helper(0x8000010000000000, 0xDEFFFFFE, 0x5F000001);
  TestUScvtf32Helper(0x8000010000000001, 0xDEFFFFFE, 0x5F000001);
  TestUScvtf32Helper(0x8000014000000000, 0xDEFFFFFE, 0x5F000001);
  TestUScvtf32Helper(0x8000014000000001, 0xDEFFFFFD, 0x5F000001);
  TestUScvtf32Helper(0x8000018000000000, 0xDEFFFFFD, 0x5F000002);
  // Round up to produce a result that's too big for the input to represent.
  TestUScvtf32Helper(0x000000007FFFFFC0, 0x4F000000, 0x4F000000);
  TestUScvtf32Helper(0x000000007FFFFFFF, 0x4F000000, 0x4F000000);
  TestUScvtf32Helper(0x00000000FFFFFF80, 0x4F800000, 0x4F800000);
  TestUScvtf32Helper(0x00000000FFFFFFFF, 0x4F800000, 0x4F800000);
  TestUScvtf32Helper(0x7FFFFFC000000000, 0x5F000000, 0x5F000000);
  TestUScvtf32Helper(0x7FFFFFFFFFFFFFFF, 0x5F000000, 0x5F000000);
  TestUScvtf32Helper(0xFFFFFF8000000000, 0xD3000000, 0x5F800000);
  TestUScvtf32Helper(0xFFFFFFFFFFFFFFFF, 0xBF800000, 0x5F800000);
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
  const uint64_t fpcr_core = 0x07C00000;

  // All FPCR fields (including fields which may be read-as-zero):
  //  Stride, Len
  //  IDE, IXE, UFE, OFE, DZE, IOE
  const uint64_t fpcr_all = fpcr_core | 0x00379F00;

  SETUP();

  START();
  __ Mov(w0, 0);
  __ Mov(w1, 0x7FFFFFFF);

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

TEST(system) {
  INIT_V8();
  SETUP();
  RegisterDump before;

  START();
  before.Dump(&masm);
  __ Nop();
  __ Csdb();
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
  __ Mov(x30, sp);
  // Initialize the other registers used in this test.
  uint64_t literal_base = 0x0100001000100101UL;
  __ Mov(x0, 0);
  __ Mov(x1, literal_base);
  for (int i = 2; i < x30.code(); i++) {
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

  // Swap the saved system stack pointer with the real one. If sp was written
  // during the test, it will show up in x30. This is done because the test
  // framework assumes that sp will be valid at the end of the test.
  __ Mov(x29, x30);
  __ Mov(x30, sp);
  __ Mov(sp, x29);
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
  __ Mov(x30, sp);
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

  __ ands(xzr, x2, ~0xF);
  __ ands(xzr, xzr, ~0xF);
  __ ands(xzr, x0, x2);
  __ ands(xzr, x2, xzr);
  __ ands(xzr, xzr, x2);

  __ bics(xzr, x3, ~0xF);
  __ bics(xzr, xzr, ~0xF);
  __ bics(xzr, x0, x3);
  __ bics(xzr, x3, xzr);
  __ bics(xzr, xzr, x3);

  __ subs(xzr, x0, Operand(x3, UXTX));
  __ subs(xzr, x3, Operand(xzr, UXTX));
  __ subs(xzr, x3, 1234);
  __ subs(xzr, x0, x3);
  __ subs(xzr, x3, xzr);
  __ subs(xzr, xzr, x3);

  // Swap the saved system stack pointer with the real one. If sp was written
  // during the test, it will show up in x30. This is done because the test
  // framework assumes that sp will be valid at the end of the test.
  __ Mov(x29, x30);
  __ Mov(x30, sp);
  __ Mov(sp, x29);
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
  CHECK(x0.bit() == (1UL << 0));
  CHECK(x1.bit() == (1UL << 1));
  CHECK(x10.bit() == (1UL << 10));

  // AAPCS64 definitions.
  CHECK(fp.bit() == (1UL << kFramePointerRegCode));
  CHECK(lr.bit() == (1UL << kLinkRegCode));

  // Fixed (hardware) definitions.
  CHECK(xzr.bit() == (1UL << kZeroRegCode));

  // Internal ABI definitions.
  CHECK(sp.bit() == (1UL << kSPRegInternalCode));
  CHECK(sp.bit() != xzr.bit());

  // xn.bit() == wn.bit() at all times, for the same n.
  CHECK(x0.bit() == w0.bit());
  CHECK(x1.bit() == w1.bit());
  CHECK(x10.bit() == w10.bit());
  CHECK(xzr.bit() == wzr.bit());
  CHECK(sp.bit() == wsp.bit());
}


TEST(peek_poke_simple) {
  INIT_V8();
  SETUP();
  START();

  static const RegList x0_to_x3 = x0.bit() | x1.bit() | x2.bit() | x3.bit();
  static const RegList x10_to_x13 =
      x10.bit() | x11.bit() | x12.bit() | x13.bit();

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

  CHECK_EQUAL_64((literal_base * 1) & 0xFFFFFFFF, x10);
  CHECK_EQUAL_64((literal_base * 2) & 0xFFFFFFFF, x11);
  CHECK_EQUAL_64((literal_base * 3) & 0xFFFFFFFF, x12);
  CHECK_EQUAL_64((literal_base * 4) & 0xFFFFFFFF, x13);

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
  Clobber(&masm, x0.bit());
  __ Peek(x0, 1);
  __ Poke(x1, 2);
  Clobber(&masm, x1.bit());
  __ Peek(x1, 2);
  __ Poke(x2, 3);
  Clobber(&masm, x2.bit());
  __ Peek(x2, 3);
  __ Poke(x3, 4);
  Clobber(&masm, x3.bit());
  __ Peek(x3, 4);
  __ Poke(x4, 5);
  Clobber(&masm, x4.bit());
  __ Peek(x4, 5);
  __ Poke(x5, 6);
  Clobber(&masm, x5.bit());
  __ Peek(x5, 6);
  __ Poke(x6, 7);
  Clobber(&masm, x6.bit());
  __ Peek(x6, 7);

  __ Poke(w0, 1);
  Clobber(&masm, w10.bit());
  __ Peek(w10, 1);
  __ Poke(w1, 2);
  Clobber(&masm, w11.bit());
  __ Peek(w11, 2);
  __ Poke(w2, 3);
  Clobber(&masm, w12.bit());
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

  CHECK_EQUAL_64((literal_base * 1) & 0xFFFFFFFF, x10);
  CHECK_EQUAL_64((literal_base * 2) & 0xFFFFFFFF, x11);
  CHECK_EQUAL_64((literal_base * 3) & 0xFFFFFFFF, x12);

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
  uint64_t x5_expected =
      ((x1_expected << 16) & 0xFFFF0000) | ((x1_expected >> 16) & 0x0000FFFF);

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
    __ Peek(x6, 4);
    __ Peek(w7, 6);
    __ Poke(xzr, 0);    // Clobber the space we're about to drop.
    __ Poke(xzr, 8);    // Clobber the space we're about to drop.
    __ Drop(2);
    __ Poke(x3, 8);
    __ Poke(x2, 0);
    __ Claim(2);
    __ Poke(x0, 0);
    __ Poke(x1, 8);
  }

  __ Pop(x0, x1, x2, x3);

  END();
  RUN();

  uint64_t x0_expected = literal_base * 1;
  uint64_t x1_expected = literal_base * 2;
  uint64_t x2_expected = literal_base * 3;
  uint64_t x3_expected = literal_base * 4;
  uint64_t x6_expected = (x1_expected << 32) | (x0_expected >> 32);
  uint64_t x7_expected =
      ((x1_expected << 16) & 0xFFFF0000) | ((x0_expected >> 48) & 0x0000FFFF);

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

// The maximum number of registers that can be used by the PushPop* tests,
// where a reg_count field is provided.
static int const kPushPopMaxRegCount = -1;

// Test a simple push-pop pattern:
//  * Push <reg_count> registers with size <reg_size>.
//  * Clobber the register contents.
//  * Pop <reg_count> registers to restore the original contents.
//
// Different push and pop methods can be specified independently to test for
// proper word-endian behaviour.
static void PushPopSimpleHelper(int reg_count, int reg_size,
                                PushPopMethod push_method,
                                PushPopMethod pop_method) {
  SETUP();

  START();

  // Registers in the TmpList can be used by the macro assembler for debug code
  // (for example in 'Pop'), so we can't use them here.
  static RegList const allowed = ~(masm.TmpList()->list());
  if (reg_count == kPushPopMaxRegCount) {
    reg_count = CountSetBits(allowed, kNumberOfRegisters);
  }
  // Work out which registers to use, based on reg_size.
  auto r = CreateRegisterArray<Register, kNumberOfRegisters>();
  auto x = CreateRegisterArray<Register, kNumberOfRegisters>();
  RegList list = PopulateRegisterArray(nullptr, x.data(), r.data(), reg_size,
                                       reg_count, allowed);

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  {
    int i;

    // Initialize the registers.
    for (i = 0; i < reg_count; i++) {
      // Always write into the X register, to ensure that the upper word is
      // properly ignored by Push when testing W registers.
      if (!x[i].IsZero()) {
        __ Mov(x[i], literal_base * i);
      }
    }

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
          default:
            CHECK_EQ(i, 0);
            break;
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
          default:
            CHECK(i == reg_count);
            break;
        }
        break;
      case PushPopRegList:
        __ PopSizeRegList(list, reg_size);
        break;
    }
  }

  END();

  RUN();

  // Check that the register contents were preserved.
  // Always use CHECK_EQUAL_64, even when testing W registers, so we can test
  // that the upper word was properly cleared by Pop.
  literal_base &= (0xFFFFFFFFFFFFFFFFUL >> (64 - reg_size));
  for (int i = 0; i < reg_count; i++) {
    if (x[i].IsZero()) {
      CHECK_EQUAL_64(0, x[i]);
    } else {
      CHECK_EQUAL_64(literal_base * i, x[i]);
    }
  }

  TEARDOWN();
}

TEST(push_pop_simple_32) {
  INIT_V8();

  for (int count = 0; count < kPushPopMaxRegCount; count += 4) {
    PushPopSimpleHelper(count, kWRegSizeInBits, PushPopByFour, PushPopByFour);
    PushPopSimpleHelper(count, kWRegSizeInBits, PushPopByFour, PushPopRegList);
    PushPopSimpleHelper(count, kWRegSizeInBits, PushPopRegList, PushPopByFour);
    PushPopSimpleHelper(count, kWRegSizeInBits, PushPopRegList, PushPopRegList);
  }
  // Skip testing kPushPopMaxRegCount, as we exclude the temporary registers
  // and we end up with a number of registers that is not a multiple of four and
  // is not supported for pushing.
}

TEST(push_pop_simple_64) {
  INIT_V8();
  for (int count = 0; count <= 8; count += 2) {
    PushPopSimpleHelper(count, kXRegSizeInBits, PushPopByFour, PushPopByFour);
    PushPopSimpleHelper(count, kXRegSizeInBits, PushPopByFour, PushPopRegList);
    PushPopSimpleHelper(count, kXRegSizeInBits, PushPopRegList, PushPopByFour);
    PushPopSimpleHelper(count, kXRegSizeInBits, PushPopRegList, PushPopRegList);
  }
  // Test with the maximum number of registers.
  PushPopSimpleHelper(kPushPopMaxRegCount, kXRegSizeInBits, PushPopByFour,
                      PushPopByFour);
  PushPopSimpleHelper(kPushPopMaxRegCount, kXRegSizeInBits, PushPopByFour,
                      PushPopRegList);
  PushPopSimpleHelper(kPushPopMaxRegCount, kXRegSizeInBits, PushPopRegList,
                      PushPopByFour);
  PushPopSimpleHelper(kPushPopMaxRegCount, kXRegSizeInBits, PushPopRegList,
                      PushPopRegList);
}

// The maximum number of registers that can be used by the PushPopFP* tests,
// where a reg_count field is provided.
static int const kPushPopFPMaxRegCount = -1;

// Test a simple push-pop pattern:
//  * Push <reg_count> FP registers with size <reg_size>.
//  * Clobber the register contents.
//  * Pop <reg_count> FP registers to restore the original contents.
//
// Different push and pop methods can be specified independently to test for
// proper word-endian behaviour.
static void PushPopFPSimpleHelper(int reg_count, int reg_size,
                                  PushPopMethod push_method,
                                  PushPopMethod pop_method) {
  SETUP();

  START();

  // We can use any floating-point register. None of them are reserved for
  // debug code, for example.
  static RegList const allowed = ~0;
  if (reg_count == kPushPopFPMaxRegCount) {
    reg_count = CountSetBits(allowed, kNumberOfVRegisters);
  }
  // Work out which registers to use, based on reg_size.
  auto v = CreateRegisterArray<VRegister, kNumberOfRegisters>();
  auto d = CreateRegisterArray<VRegister, kNumberOfRegisters>();
  RegList list = PopulateVRegisterArray(nullptr, d.data(), v.data(), reg_size,
                                        reg_count, allowed);

  // The literal base is chosen to have two useful properties:
  //  * When multiplied (using an integer) by small values (such as a register
  //    index), this value is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  //  * It is never a floating-point NaN, and will therefore always compare
  //    equal to itself.
  uint64_t literal_base = 0x0100001000100101UL;

  {
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
          default:
            CHECK_EQ(i, 0);
            break;
        }
        break;
      case PushPopRegList:
        __ PushSizeRegList(list, reg_size, CPURegister::kVRegister);
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
          default:
            CHECK(i == reg_count);
            break;
        }
        break;
      case PushPopRegList:
        __ PopSizeRegList(list, reg_size, CPURegister::kVRegister);
        break;
    }
  }

  END();

  RUN();

  // Check that the register contents were preserved.
  // Always use CHECK_EQUAL_FP64, even when testing S registers, so we can
  // test that the upper word was properly cleared by Pop.
  literal_base &= (0xFFFFFFFFFFFFFFFFUL >> (64 - reg_size));
  for (int i = 0; i < reg_count; i++) {
    uint64_t literal = literal_base * i;
    double expected;
    memcpy(&expected, &literal, sizeof(expected));
    CHECK_EQUAL_FP64(expected, d[i]);
  }

  TEARDOWN();
}

TEST(push_pop_fp_simple_32) {
  INIT_V8();
  for (int count = 0; count <= 8; count += 4) {
    PushPopFPSimpleHelper(count, kSRegSizeInBits, PushPopByFour, PushPopByFour);
    PushPopFPSimpleHelper(count, kSRegSizeInBits, PushPopByFour,
                          PushPopRegList);
    PushPopFPSimpleHelper(count, kSRegSizeInBits, PushPopRegList,
                          PushPopByFour);
    PushPopFPSimpleHelper(count, kSRegSizeInBits, PushPopRegList,
                          PushPopRegList);
  }
  // Test with the maximum number of registers.
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kSRegSizeInBits, PushPopByFour,
                        PushPopByFour);
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kSRegSizeInBits, PushPopByFour,
                        PushPopRegList);
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kSRegSizeInBits, PushPopRegList,
                        PushPopByFour);
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kSRegSizeInBits, PushPopRegList,
                        PushPopRegList);
}

TEST(push_pop_fp_simple_64) {
  INIT_V8();
  for (int count = 0; count <= 8; count += 2) {
    PushPopFPSimpleHelper(count, kDRegSizeInBits, PushPopByFour, PushPopByFour);
    PushPopFPSimpleHelper(count, kDRegSizeInBits, PushPopByFour,
                          PushPopRegList);
    PushPopFPSimpleHelper(count, kDRegSizeInBits, PushPopRegList,
                          PushPopByFour);
    PushPopFPSimpleHelper(count, kDRegSizeInBits, PushPopRegList,
                          PushPopRegList);
  }
  // Test with the maximum number of registers.
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kDRegSizeInBits, PushPopByFour,
                        PushPopByFour);
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kDRegSizeInBits, PushPopByFour,
                        PushPopRegList);
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kDRegSizeInBits, PushPopRegList,
                        PushPopByFour);
  PushPopFPSimpleHelper(kPushPopFPMaxRegCount, kDRegSizeInBits, PushPopRegList,
                        PushPopRegList);
}


// Push and pop data using an overlapping combination of Push/Pop and
// RegList-based methods.
static void PushPopMixedMethodsHelper(int reg_size) {
  SETUP();

  // Registers in the TmpList can be used by the macro assembler for debug code
  // (for example in 'Pop'), so we can't use them here.
  static RegList const allowed = ~(masm.TmpList()->list());
  // Work out which registers to use, based on reg_size.
  auto r = CreateRegisterArray<Register, 10>();
  auto x = CreateRegisterArray<Register, 10>();
  PopulateRegisterArray(nullptr, x.data(), r.data(), reg_size, 10, allowed);

  // Calculate some handy register lists.
  RegList r0_to_r3 = 0;
  for (int i = 0; i <= 3; i++) {
    r0_to_r3 |= x[i].bit();
  }
  RegList r4_to_r5 = 0;
  for (int i = 4; i <= 5; i++) {
    r4_to_r5 |= x[i].bit();
  }
  RegList r6_to_r9 = 0;
  for (int i = 6; i <= 9; i++) {
    r6_to_r9 |= x[i].bit();
  }

  // The literal base is chosen to have two useful properties:
  //  * When multiplied by small values (such as a register index), this value
  //    is clearly readable in the result.
  //  * The value is not formed from repeating fixed-size smaller values, so it
  //    can be used to detect endianness-related errors.
  uint64_t literal_base = 0x0100001000100101UL;

  START();
  {
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
  }

  END();

  RUN();

  // Always use CHECK_EQUAL_64, even when testing W registers, so we can test
  // that the upper word was properly cleared by Pop.
  literal_base &= (0xFFFFFFFFFFFFFFFFUL >> (64 - reg_size));

  CHECK_EQUAL_64(literal_base * 3, x[9]);
  CHECK_EQUAL_64(literal_base * 2, x[8]);
  CHECK_EQUAL_64(literal_base * 0, x[7]);
  CHECK_EQUAL_64(literal_base * 3, x[6]);
  CHECK_EQUAL_64(literal_base * 1, x[5]);
  CHECK_EQUAL_64(literal_base * 2, x[4]);

  TEARDOWN();
}

TEST(push_pop_mixed_methods_64) {
  INIT_V8();
  PushPopMixedMethodsHelper(kXRegSizeInBits);
}

TEST(push_pop) {
  INIT_V8();
  SETUP();

  START();
  __ Mov(x3, 0x3333333333333333UL);
  __ Mov(x2, 0x2222222222222222UL);
  __ Mov(x1, 0x1111111111111111UL);
  __ Mov(x0, 0x0000000000000000UL);
  __ Claim(2);
  __ PushXRegList(x0.bit() | x1.bit() | x2.bit() | x3.bit());
  __ Push(x3, x2);
  __ PopXRegList(x0.bit() | x1.bit() | x2.bit() | x3.bit());
  __ Push(x2, x1, x3, x0);
  __ Pop(x4, x5);
  __ Pop(x6, x7, x8, x9);

  __ Claim(2);
  __ PushWRegList(w0.bit() | w1.bit() | w2.bit() | w3.bit());
  __ Push(w3, w1, w2, w0);
  __ PopWRegList(w10.bit() | w11.bit() | w12.bit() | w13.bit());
  __ Pop(w14, w15, w16, w17);

  __ Claim(2);
  __ Push(w2, w2, w1, w1);
  __ Push(x3, x3);
  __ Pop(w18, w19, w20, w21);
  __ Pop(x22, x23);

  __ Claim(2);
  __ PushXRegList(x1.bit() | x22.bit());
  __ PopXRegList(x24.bit() | x26.bit());

  __ Claim(2);
  __ PushWRegList(w1.bit() | w2.bit() | w4.bit() | w22.bit());
  __ PopWRegList(w25.bit() | w27.bit() | w28.bit() | w29.bit());

  __ Claim(2);
  __ PushXRegList(0);
  __ PopXRegList(0);
  __ PushXRegList(0xFFFFFFFF);
  __ PopXRegList(0xFFFFFFFF);
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

  MacroAssembler::PushPopQueue queue(&masm);

  // Queue up registers.
  queue.Queue(x0);
  queue.Queue(x1);
  queue.Queue(x2);
  queue.Queue(x3);

  queue.Queue(w4);
  queue.Queue(w5);
  queue.Queue(w6);
  queue.Queue(w7);

  queue.Queue(d0);
  queue.Queue(d1);

  queue.Queue(s2);
  queue.Queue(s3);
  queue.Queue(s4);
  queue.Queue(s5);

  __ Mov(x0, 0x1234000000000000);
  __ Mov(x1, 0x1234000100010001);
  __ Mov(x2, 0x1234000200020002);
  __ Mov(x3, 0x1234000300030003);
  __ Mov(w4, 0x12340004);
  __ Mov(w5, 0x12340005);
  __ Mov(w6, 0x12340006);
  __ Mov(w7, 0x12340007);
  __ Fmov(d0, 123400.0);
  __ Fmov(d1, 123401.0);
  __ Fmov(s2, 123402.0);
  __ Fmov(s3, 123403.0);
  __ Fmov(s4, 123404.0);
  __ Fmov(s5, 123405.0);

  // Actually push them.
  queue.PushQueued();

  Clobber(&masm, CPURegList(CPURegister::kRegister, kXRegSizeInBits, 0, 8));
  Clobber(&masm, CPURegList(CPURegister::kVRegister, kDRegSizeInBits, 0, 6));

  // Pop them conventionally.
  __ Pop(s5, s4, s3, s2);
  __ Pop(d1, d0);
  __ Pop(w7, w6, w5, w4);
  __ Pop(x3, x2, x1, x0);

  END();

  RUN();

  CHECK_EQUAL_64(0x1234000000000000, x0);
  CHECK_EQUAL_64(0x1234000100010001, x1);
  CHECK_EQUAL_64(0x1234000200020002, x2);
  CHECK_EQUAL_64(0x1234000300030003, x3);

  CHECK_EQUAL_64(0x0000000012340004, x4);
  CHECK_EQUAL_64(0x0000000012340005, x5);
  CHECK_EQUAL_64(0x0000000012340006, x6);
  CHECK_EQUAL_64(0x0000000012340007, x7);

  CHECK_EQUAL_FP64(123400.0, d0);
  CHECK_EQUAL_FP64(123401.0, d1);

  CHECK_EQUAL_FP32(123402.0, s2);
  CHECK_EQUAL_FP32(123403.0, s3);
  CHECK_EQUAL_FP32(123404.0, s4);
  CHECK_EQUAL_FP32(123405.0, s5);

  TEARDOWN();
}


TEST(pop_queued) {
  INIT_V8();
  SETUP();

  START();

  MacroAssembler::PushPopQueue queue(&masm);

  __ Mov(x0, 0x1234000000000000);
  __ Mov(x1, 0x1234000100010001);
  __ Mov(x2, 0x1234000200020002);
  __ Mov(x3, 0x1234000300030003);
  __ Mov(w4, 0x12340004);
  __ Mov(w5, 0x12340005);
  __ Mov(w6, 0x12340006);
  __ Mov(w7, 0x12340007);
  __ Fmov(d0, 123400.0);
  __ Fmov(d1, 123401.0);
  __ Fmov(s2, 123402.0);
  __ Fmov(s3, 123403.0);
  __ Fmov(s4, 123404.0);
  __ Fmov(s5, 123405.0);

  // Push registers conventionally.
  __ Push(x0, x1, x2, x3);
  __ Push(w4, w5, w6, w7);
  __ Push(d0, d1);
  __ Push(s2, s3, s4, s5);

  // Queue up a pop.
  queue.Queue(s5);
  queue.Queue(s4);
  queue.Queue(s3);
  queue.Queue(s2);

  queue.Queue(d1);
  queue.Queue(d0);

  queue.Queue(w7);
  queue.Queue(w6);
  queue.Queue(w5);
  queue.Queue(w4);

  queue.Queue(x3);
  queue.Queue(x2);
  queue.Queue(x1);
  queue.Queue(x0);

  Clobber(&masm, CPURegList(CPURegister::kRegister, kXRegSizeInBits, 0, 8));
  Clobber(&masm, CPURegList(CPURegister::kVRegister, kDRegSizeInBits, 0, 6));

  // Actually pop them.
  queue.PopQueued();

  END();

  RUN();

  CHECK_EQUAL_64(0x1234000000000000, x0);
  CHECK_EQUAL_64(0x1234000100010001, x1);
  CHECK_EQUAL_64(0x1234000200020002, x2);
  CHECK_EQUAL_64(0x1234000300030003, x3);

  CHECK_EQUAL_64(0x0000000012340004, x4);
  CHECK_EQUAL_64(0x0000000012340005, x5);
  CHECK_EQUAL_64(0x0000000012340006, x6);
  CHECK_EQUAL_64(0x0000000012340007, x7);

  CHECK_EQUAL_FP64(123400.0, d0);
  CHECK_EQUAL_FP64(123401.0, d1);

  CHECK_EQUAL_FP32(123402.0, s2);
  CHECK_EQUAL_FP32(123403.0, s3);
  CHECK_EQUAL_FP32(123404.0, s4);
  CHECK_EQUAL_FP32(123405.0, s5);

  TEARDOWN();
}

TEST(copy_slots_down) {
  INIT_V8();
  SETUP();

  const uint64_t ones = 0x1111111111111111UL;
  const uint64_t twos = 0x2222222222222222UL;
  const uint64_t threes = 0x3333333333333333UL;
  const uint64_t fours = 0x4444444444444444UL;

  START();

  // Test copying 12 slots down one slot.
  __ Mov(x1, ones);
  __ Mov(x2, twos);
  __ Mov(x3, threes);
  __ Mov(x4, fours);

  __ Push(x1, x2, x3, x4);
  __ Push(x1, x2, x1, x2);
  __ Push(x3, x4, x3, x4);
  __ Push(xzr, xzr);

  __ Mov(x5, 1);
  __ Mov(x6, 2);
  __ Mov(x7, 12);
  __ CopySlots(x5, x6, x7);

  __ Pop(xzr, x4, x5, x6);
  __ Pop(x7, x8, x9, x10);
  __ Pop(x11, x12, x13, x14);
  __ Pop(x15, xzr);

  // Test copying one slot down one slot.
  __ Push(x1, xzr, xzr, xzr);

  __ Mov(x1, 2);
  __ Mov(x2, 3);
  __ Mov(x3, 1);
  __ CopySlots(x1, x2, x3);

  __ Drop(2);
  __ Pop(x0, xzr);

  END();

  RUN();

  CHECK_EQUAL_64(fours, x4);
  CHECK_EQUAL_64(threes, x5);
  CHECK_EQUAL_64(fours, x6);
  CHECK_EQUAL_64(threes, x7);

  CHECK_EQUAL_64(twos, x8);
  CHECK_EQUAL_64(ones, x9);
  CHECK_EQUAL_64(twos, x10);
  CHECK_EQUAL_64(ones, x11);

  CHECK_EQUAL_64(fours, x12);
  CHECK_EQUAL_64(threes, x13);
  CHECK_EQUAL_64(twos, x14);
  CHECK_EQUAL_64(ones, x15);

  CHECK_EQUAL_64(ones, x0);

  TEARDOWN();
}

TEST(copy_slots_up) {
  INIT_V8();
  SETUP();

  const uint64_t ones = 0x1111111111111111UL;
  const uint64_t twos = 0x2222222222222222UL;
  const uint64_t threes = 0x3333333333333333UL;

  START();

  __ Mov(x1, ones);
  __ Mov(x2, twos);
  __ Mov(x3, threes);

  // Test copying one slot to the next slot higher in memory.
  __ Push(xzr, x1);

  __ Mov(x5, 1);
  __ Mov(x6, 0);
  __ Mov(x7, 1);
  __ CopySlots(x5, x6, x7);

  __ Pop(xzr, x10);

  // Test copying two slots to the next two slots higher in memory.
  __ Push(xzr, xzr);
  __ Push(x1, x2);

  __ Mov(x5, 2);
  __ Mov(x6, 0);
  __ Mov(x7, 2);
  __ CopySlots(x5, x6, x7);

  __ Drop(2);
  __ Pop(x11, x12);

  // Test copying three slots to the next three slots higher in memory.
  __ Push(xzr, xzr, xzr, x1);
  __ Push(x2, x3);

  __ Mov(x5, 3);
  __ Mov(x6, 0);
  __ Mov(x7, 3);
  __ CopySlots(x5, x6, x7);

  __ Drop(2);
  __ Pop(xzr, x0, x1, x2);

  END();

  RUN();

  CHECK_EQUAL_64(ones, x10);
  CHECK_EQUAL_64(twos, x11);
  CHECK_EQUAL_64(ones, x12);
  CHECK_EQUAL_64(threes, x0);
  CHECK_EQUAL_64(twos, x1);
  CHECK_EQUAL_64(ones, x2);

  TEARDOWN();
}

TEST(copy_double_words_downwards_even) {
  INIT_V8();
  SETUP();

  const uint64_t ones = 0x1111111111111111UL;
  const uint64_t twos = 0x2222222222222222UL;
  const uint64_t threes = 0x3333333333333333UL;
  const uint64_t fours = 0x4444444444444444UL;

  START();

  // Test copying 12 slots up one slot.
  __ Mov(x1, ones);
  __ Mov(x2, twos);
  __ Mov(x3, threes);
  __ Mov(x4, fours);

  __ Push(xzr, xzr);
  __ Push(x1, x2, x3, x4);
  __ Push(x1, x2, x1, x2);
  __ Push(x3, x4, x3, x4);

  __ SlotAddress(x5, 12);
  __ SlotAddress(x6, 11);
  __ Mov(x7, 12);
  __ CopyDoubleWords(x5, x6, x7, TurboAssembler::kSrcLessThanDst);

  __ Pop(xzr, x4, x5, x6);
  __ Pop(x7, x8, x9, x10);
  __ Pop(x11, x12, x13, x14);
  __ Pop(x15, xzr);

  END();

  RUN();

  CHECK_EQUAL_64(ones, x15);
  CHECK_EQUAL_64(twos, x14);
  CHECK_EQUAL_64(threes, x13);
  CHECK_EQUAL_64(fours, x12);

  CHECK_EQUAL_64(ones, x11);
  CHECK_EQUAL_64(twos, x10);
  CHECK_EQUAL_64(ones, x9);
  CHECK_EQUAL_64(twos, x8);

  CHECK_EQUAL_64(threes, x7);
  CHECK_EQUAL_64(fours, x6);
  CHECK_EQUAL_64(threes, x5);
  CHECK_EQUAL_64(fours, x4);

  TEARDOWN();
}

TEST(copy_double_words_downwards_odd) {
  INIT_V8();
  SETUP();

  const uint64_t ones = 0x1111111111111111UL;
  const uint64_t twos = 0x2222222222222222UL;
  const uint64_t threes = 0x3333333333333333UL;
  const uint64_t fours = 0x4444444444444444UL;
  const uint64_t fives = 0x5555555555555555UL;

  START();

  // Test copying 13 slots up one slot.
  __ Mov(x1, ones);
  __ Mov(x2, twos);
  __ Mov(x3, threes);
  __ Mov(x4, fours);
  __ Mov(x5, fives);

  __ Push(xzr, x5);
  __ Push(x1, x2, x3, x4);
  __ Push(x1, x2, x1, x2);
  __ Push(x3, x4, x3, x4);

  __ SlotAddress(x5, 13);
  __ SlotAddress(x6, 12);
  __ Mov(x7, 13);
  __ CopyDoubleWords(x5, x6, x7, TurboAssembler::kSrcLessThanDst);

  __ Pop(xzr, x4);
  __ Pop(x5, x6, x7, x8);
  __ Pop(x9, x10, x11, x12);
  __ Pop(x13, x14, x15, x16);

  END();

  RUN();

  CHECK_EQUAL_64(fives, x16);

  CHECK_EQUAL_64(ones, x15);
  CHECK_EQUAL_64(twos, x14);
  CHECK_EQUAL_64(threes, x13);
  CHECK_EQUAL_64(fours, x12);

  CHECK_EQUAL_64(ones, x11);
  CHECK_EQUAL_64(twos, x10);
  CHECK_EQUAL_64(ones, x9);
  CHECK_EQUAL_64(twos, x8);

  CHECK_EQUAL_64(threes, x7);
  CHECK_EQUAL_64(fours, x6);
  CHECK_EQUAL_64(threes, x5);
  CHECK_EQUAL_64(fours, x4);

  TEARDOWN();
}

TEST(copy_noop) {
  INIT_V8();
  SETUP();

  const uint64_t ones = 0x1111111111111111UL;
  const uint64_t twos = 0x2222222222222222UL;
  const uint64_t threes = 0x3333333333333333UL;
  const uint64_t fours = 0x4444444444444444UL;
  const uint64_t fives = 0x5555555555555555UL;

  START();

  __ Mov(x1, ones);
  __ Mov(x2, twos);
  __ Mov(x3, threes);
  __ Mov(x4, fours);
  __ Mov(x5, fives);

  __ Push(xzr, x5, x5, xzr);
  __ Push(x3, x4, x3, x4);
  __ Push(x1, x2, x1, x2);
  __ Push(x1, x2, x3, x4);

  // src < dst, count == 0
  __ SlotAddress(x5, 3);
  __ SlotAddress(x6, 2);
  __ Mov(x7, 0);
  __ CopyDoubleWords(x5, x6, x7, TurboAssembler::kSrcLessThanDst);

  // dst < src, count == 0
  __ SlotAddress(x5, 2);
  __ SlotAddress(x6, 3);
  __ Mov(x7, 0);
  __ CopyDoubleWords(x5, x6, x7, TurboAssembler::kDstLessThanSrc);

  __ Pop(x1, x2, x3, x4);
  __ Pop(x5, x6, x7, x8);
  __ Pop(x9, x10, x11, x12);
  __ Pop(x13, x14, x15, x16);

  END();

  RUN();

  CHECK_EQUAL_64(fours, x1);
  CHECK_EQUAL_64(threes, x2);
  CHECK_EQUAL_64(twos, x3);
  CHECK_EQUAL_64(ones, x4);

  CHECK_EQUAL_64(twos, x5);
  CHECK_EQUAL_64(ones, x6);
  CHECK_EQUAL_64(twos, x7);
  CHECK_EQUAL_64(ones, x8);

  CHECK_EQUAL_64(fours, x9);
  CHECK_EQUAL_64(threes, x10);
  CHECK_EQUAL_64(fours, x11);
  CHECK_EQUAL_64(threes, x12);

  CHECK_EQUAL_64(0, x13);
  CHECK_EQUAL_64(fives, x14);
  CHECK_EQUAL_64(fives, x15);
  CHECK_EQUAL_64(0, x16);

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
  __ Mov(x1, 0xAAAAAAAA00000001UL);  // A pointer.
  __ Mov(x2, 0x1234567800000000UL);  // A smi.
  __ Mov(x3, 0x8765432100000000UL);  // A smi.
  __ Mov(x4, 0xDEAD);
  __ Mov(x5, 0xDEAD);
  __ Mov(x6, 0xDEAD);
  __ Mov(x7, 0xDEAD);

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
  CHECK_EQUAL_64(0xAAAAAAAA00000001UL, x1);
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
  __ Mov(x1, 0xAAAAAAAA00000001UL);  // A pointer.
  __ Mov(x2, 0x1234567800000000UL);  // A smi.
  __ Mov(x3, 0x8765432100000000UL);  // A smi.
  __ Mov(x4, 0xDEAD);
  __ Mov(x5, 0xDEAD);
  __ Mov(x6, 0xDEAD);
  __ Mov(x7, 0xDEAD);

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
  CHECK_EQUAL_64(0xAAAAAAAA00000001UL, x1);
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
  CHECK(NoReg.Is(NoVReg));
  CHECK(NoVReg.Is(NoReg));
  CHECK(NoReg.Is(NoCPUReg));
  CHECK(NoCPUReg.Is(NoReg));
  CHECK(NoVReg.Is(NoCPUReg));
  CHECK(NoCPUReg.Is(NoVReg));

  CHECK(NoReg.IsNone());
  CHECK(NoVReg.IsNone());
  CHECK(NoCPUReg.IsNone());
}

TEST(vreg) {
  // This test doesn't generate any code, but it verifies
  // Helper functions and methods pertaining to VRegister logic.

  CHECK_EQ(8U, RegisterSizeInBitsFromFormat(kFormatB));
  CHECK_EQ(16U, RegisterSizeInBitsFromFormat(kFormatH));
  CHECK_EQ(32U, RegisterSizeInBitsFromFormat(kFormatS));
  CHECK_EQ(64U, RegisterSizeInBitsFromFormat(kFormatD));
  CHECK_EQ(64U, RegisterSizeInBitsFromFormat(kFormat8B));
  CHECK_EQ(64U, RegisterSizeInBitsFromFormat(kFormat4H));
  CHECK_EQ(64U, RegisterSizeInBitsFromFormat(kFormat2S));
  CHECK_EQ(64U, RegisterSizeInBitsFromFormat(kFormat1D));
  CHECK_EQ(128U, RegisterSizeInBitsFromFormat(kFormat16B));
  CHECK_EQ(128U, RegisterSizeInBitsFromFormat(kFormat8H));
  CHECK_EQ(128U, RegisterSizeInBitsFromFormat(kFormat4S));
  CHECK_EQ(128U, RegisterSizeInBitsFromFormat(kFormat2D));

  CHECK_EQ(16, LaneCountFromFormat(kFormat16B));
  CHECK_EQ(8, LaneCountFromFormat(kFormat8B));
  CHECK_EQ(8, LaneCountFromFormat(kFormat8H));
  CHECK_EQ(4, LaneCountFromFormat(kFormat4H));
  CHECK_EQ(4, LaneCountFromFormat(kFormat4S));
  CHECK_EQ(2, LaneCountFromFormat(kFormat2S));
  CHECK_EQ(2, LaneCountFromFormat(kFormat2D));
  CHECK_EQ(1, LaneCountFromFormat(kFormat1D));
  CHECK_EQ(1, LaneCountFromFormat(kFormatB));
  CHECK_EQ(1, LaneCountFromFormat(kFormatH));
  CHECK_EQ(1, LaneCountFromFormat(kFormatS));
  CHECK_EQ(1, LaneCountFromFormat(kFormatD));

  CHECK(!IsVectorFormat(kFormatB));
  CHECK(!IsVectorFormat(kFormatH));
  CHECK(!IsVectorFormat(kFormatS));
  CHECK(!IsVectorFormat(kFormatD));
  CHECK(IsVectorFormat(kFormat16B));
  CHECK(IsVectorFormat(kFormat8B));
  CHECK(IsVectorFormat(kFormat8H));
  CHECK(IsVectorFormat(kFormat4H));
  CHECK(IsVectorFormat(kFormat4S));
  CHECK(IsVectorFormat(kFormat2S));
  CHECK(IsVectorFormat(kFormat2D));
  CHECK(IsVectorFormat(kFormat1D));

  CHECK(!d0.Is8B());
  CHECK(!d0.Is16B());
  CHECK(!d0.Is4H());
  CHECK(!d0.Is8H());
  CHECK(!d0.Is2S());
  CHECK(!d0.Is4S());
  CHECK(d0.Is1D());
  CHECK(!d0.Is1S());
  CHECK(!d0.Is1H());
  CHECK(!d0.Is1B());
  CHECK(!d0.IsVector());
  CHECK(d0.IsScalar());
  CHECK(d0.IsFPRegister());

  CHECK(!d0.IsW());
  CHECK(!d0.IsX());
  CHECK(d0.IsV());
  CHECK(!d0.IsB());
  CHECK(!d0.IsH());
  CHECK(!d0.IsS());
  CHECK(d0.IsD());
  CHECK(!d0.IsQ());

  CHECK(!s0.Is8B());
  CHECK(!s0.Is16B());
  CHECK(!s0.Is4H());
  CHECK(!s0.Is8H());
  CHECK(!s0.Is2S());
  CHECK(!s0.Is4S());
  CHECK(!s0.Is1D());
  CHECK(s0.Is1S());
  CHECK(!s0.Is1H());
  CHECK(!s0.Is1B());
  CHECK(!s0.IsVector());
  CHECK(s0.IsScalar());
  CHECK(s0.IsFPRegister());

  CHECK(!s0.IsW());
  CHECK(!s0.IsX());
  CHECK(s0.IsV());
  CHECK(!s0.IsB());
  CHECK(!s0.IsH());
  CHECK(s0.IsS());
  CHECK(!s0.IsD());
  CHECK(!s0.IsQ());

  CHECK(!h0.Is8B());
  CHECK(!h0.Is16B());
  CHECK(!h0.Is4H());
  CHECK(!h0.Is8H());
  CHECK(!h0.Is2S());
  CHECK(!h0.Is4S());
  CHECK(!h0.Is1D());
  CHECK(!h0.Is1S());
  CHECK(h0.Is1H());
  CHECK(!h0.Is1B());
  CHECK(!h0.IsVector());
  CHECK(h0.IsScalar());
  CHECK(!h0.IsFPRegister());

  CHECK(!h0.IsW());
  CHECK(!h0.IsX());
  CHECK(h0.IsV());
  CHECK(!h0.IsB());
  CHECK(h0.IsH());
  CHECK(!h0.IsS());
  CHECK(!h0.IsD());
  CHECK(!h0.IsQ());

  CHECK(!b0.Is8B());
  CHECK(!b0.Is16B());
  CHECK(!b0.Is4H());
  CHECK(!b0.Is8H());
  CHECK(!b0.Is2S());
  CHECK(!b0.Is4S());
  CHECK(!b0.Is1D());
  CHECK(!b0.Is1S());
  CHECK(!b0.Is1H());
  CHECK(b0.Is1B());
  CHECK(!b0.IsVector());
  CHECK(b0.IsScalar());
  CHECK(!b0.IsFPRegister());

  CHECK(!b0.IsW());
  CHECK(!b0.IsX());
  CHECK(b0.IsV());
  CHECK(b0.IsB());
  CHECK(!b0.IsH());
  CHECK(!b0.IsS());
  CHECK(!b0.IsD());
  CHECK(!b0.IsQ());

  CHECK(!q0.Is8B());
  CHECK(!q0.Is16B());
  CHECK(!q0.Is4H());
  CHECK(!q0.Is8H());
  CHECK(!q0.Is2S());
  CHECK(!q0.Is4S());
  CHECK(!q0.Is1D());
  CHECK(!q0.Is2D());
  CHECK(!q0.Is1S());
  CHECK(!q0.Is1H());
  CHECK(!q0.Is1B());
  CHECK(!q0.IsVector());
  CHECK(q0.IsScalar());
  CHECK(!q0.IsFPRegister());

  CHECK(!q0.IsW());
  CHECK(!q0.IsX());
  CHECK(q0.IsV());
  CHECK(!q0.IsB());
  CHECK(!q0.IsH());
  CHECK(!q0.IsS());
  CHECK(!q0.IsD());
  CHECK(q0.IsQ());

  CHECK(w0.IsW());
  CHECK(!w0.IsX());
  CHECK(!w0.IsV());
  CHECK(!w0.IsB());
  CHECK(!w0.IsH());
  CHECK(!w0.IsS());
  CHECK(!w0.IsD());
  CHECK(!w0.IsQ());

  CHECK(!x0.IsW());
  CHECK(x0.IsX());
  CHECK(!x0.IsV());
  CHECK(!x0.IsB());
  CHECK(!x0.IsH());
  CHECK(!x0.IsS());
  CHECK(!x0.IsD());
  CHECK(!x0.IsQ());

  CHECK(v0.V().IsV());
  CHECK(v0.B().IsB());
  CHECK(v0.H().IsH());
  CHECK(v0.D().IsD());
  CHECK(v0.S().IsS());
  CHECK(v0.Q().IsQ());

  VRegister test_8b(VRegister::Create(0, 64, 8));
  CHECK(test_8b.Is8B());
  CHECK(!test_8b.Is16B());
  CHECK(!test_8b.Is4H());
  CHECK(!test_8b.Is8H());
  CHECK(!test_8b.Is2S());
  CHECK(!test_8b.Is4S());
  CHECK(!test_8b.Is1D());
  CHECK(!test_8b.Is2D());
  CHECK(!test_8b.Is1H());
  CHECK(!test_8b.Is1B());
  CHECK(test_8b.IsVector());
  CHECK(!test_8b.IsScalar());
  CHECK(test_8b.IsFPRegister());

  VRegister test_16b(VRegister::Create(0, 128, 16));
  CHECK(!test_16b.Is8B());
  CHECK(test_16b.Is16B());
  CHECK(!test_16b.Is4H());
  CHECK(!test_16b.Is8H());
  CHECK(!test_16b.Is2S());
  CHECK(!test_16b.Is4S());
  CHECK(!test_16b.Is1D());
  CHECK(!test_16b.Is2D());
  CHECK(!test_16b.Is1H());
  CHECK(!test_16b.Is1B());
  CHECK(test_16b.IsVector());
  CHECK(!test_16b.IsScalar());
  CHECK(!test_16b.IsFPRegister());

  VRegister test_4h(VRegister::Create(0, 64, 4));
  CHECK(!test_4h.Is8B());
  CHECK(!test_4h.Is16B());
  CHECK(test_4h.Is4H());
  CHECK(!test_4h.Is8H());
  CHECK(!test_4h.Is2S());
  CHECK(!test_4h.Is4S());
  CHECK(!test_4h.Is1D());
  CHECK(!test_4h.Is2D());
  CHECK(!test_4h.Is1H());
  CHECK(!test_4h.Is1B());
  CHECK(test_4h.IsVector());
  CHECK(!test_4h.IsScalar());
  CHECK(test_4h.IsFPRegister());

  VRegister test_8h(VRegister::Create(0, 128, 8));
  CHECK(!test_8h.Is8B());
  CHECK(!test_8h.Is16B());
  CHECK(!test_8h.Is4H());
  CHECK(test_8h.Is8H());
  CHECK(!test_8h.Is2S());
  CHECK(!test_8h.Is4S());
  CHECK(!test_8h.Is1D());
  CHECK(!test_8h.Is2D());
  CHECK(!test_8h.Is1H());
  CHECK(!test_8h.Is1B());
  CHECK(test_8h.IsVector());
  CHECK(!test_8h.IsScalar());
  CHECK(!test_8h.IsFPRegister());

  VRegister test_2s(VRegister::Create(0, 64, 2));
  CHECK(!test_2s.Is8B());
  CHECK(!test_2s.Is16B());
  CHECK(!test_2s.Is4H());
  CHECK(!test_2s.Is8H());
  CHECK(test_2s.Is2S());
  CHECK(!test_2s.Is4S());
  CHECK(!test_2s.Is1D());
  CHECK(!test_2s.Is2D());
  CHECK(!test_2s.Is1H());
  CHECK(!test_2s.Is1B());
  CHECK(test_2s.IsVector());
  CHECK(!test_2s.IsScalar());
  CHECK(test_2s.IsFPRegister());

  VRegister test_4s(VRegister::Create(0, 128, 4));
  CHECK(!test_4s.Is8B());
  CHECK(!test_4s.Is16B());
  CHECK(!test_4s.Is4H());
  CHECK(!test_4s.Is8H());
  CHECK(!test_4s.Is2S());
  CHECK(test_4s.Is4S());
  CHECK(!test_4s.Is1D());
  CHECK(!test_4s.Is2D());
  CHECK(!test_4s.Is1S());
  CHECK(!test_4s.Is1H());
  CHECK(!test_4s.Is1B());
  CHECK(test_4s.IsVector());
  CHECK(!test_4s.IsScalar());
  CHECK(!test_4s.IsFPRegister());

  VRegister test_1d(VRegister::Create(0, 64, 1));
  CHECK(!test_1d.Is8B());
  CHECK(!test_1d.Is16B());
  CHECK(!test_1d.Is4H());
  CHECK(!test_1d.Is8H());
  CHECK(!test_1d.Is2S());
  CHECK(!test_1d.Is4S());
  CHECK(test_1d.Is1D());
  CHECK(!test_1d.Is2D());
  CHECK(!test_1d.Is1S());
  CHECK(!test_1d.Is1H());
  CHECK(!test_1d.Is1B());
  CHECK(!test_1d.IsVector());
  CHECK(test_1d.IsScalar());
  CHECK(test_1d.IsFPRegister());

  VRegister test_2d(VRegister::Create(0, 128, 2));
  CHECK(!test_2d.Is8B());
  CHECK(!test_2d.Is16B());
  CHECK(!test_2d.Is4H());
  CHECK(!test_2d.Is8H());
  CHECK(!test_2d.Is2S());
  CHECK(!test_2d.Is4S());
  CHECK(!test_2d.Is1D());
  CHECK(test_2d.Is2D());
  CHECK(!test_2d.Is1H());
  CHECK(!test_2d.Is1B());
  CHECK(test_2d.IsVector());
  CHECK(!test_2d.IsScalar());
  CHECK(!test_2d.IsFPRegister());

  VRegister test_1s(VRegister::Create(0, 32, 1));
  CHECK(!test_1s.Is8B());
  CHECK(!test_1s.Is16B());
  CHECK(!test_1s.Is4H());
  CHECK(!test_1s.Is8H());
  CHECK(!test_1s.Is2S());
  CHECK(!test_1s.Is4S());
  CHECK(!test_1s.Is1D());
  CHECK(!test_1s.Is2D());
  CHECK(test_1s.Is1S());
  CHECK(!test_1s.Is1H());
  CHECK(!test_1s.Is1B());
  CHECK(!test_1s.IsVector());
  CHECK(test_1s.IsScalar());
  CHECK(test_1s.IsFPRegister());

  VRegister test_1h(VRegister::Create(0, 16, 1));
  CHECK(!test_1h.Is8B());
  CHECK(!test_1h.Is16B());
  CHECK(!test_1h.Is4H());
  CHECK(!test_1h.Is8H());
  CHECK(!test_1h.Is2S());
  CHECK(!test_1h.Is4S());
  CHECK(!test_1h.Is1D());
  CHECK(!test_1h.Is2D());
  CHECK(!test_1h.Is1S());
  CHECK(test_1h.Is1H());
  CHECK(!test_1h.Is1B());
  CHECK(!test_1h.IsVector());
  CHECK(test_1h.IsScalar());
  CHECK(!test_1h.IsFPRegister());

  VRegister test_1b(VRegister::Create(0, 8, 1));
  CHECK(!test_1b.Is8B());
  CHECK(!test_1b.Is16B());
  CHECK(!test_1b.Is4H());
  CHECK(!test_1b.Is8H());
  CHECK(!test_1b.Is2S());
  CHECK(!test_1b.Is4S());
  CHECK(!test_1b.Is1D());
  CHECK(!test_1b.Is2D());
  CHECK(!test_1b.Is1S());
  CHECK(!test_1b.Is1H());
  CHECK(test_1b.Is1B());
  CHECK(!test_1b.IsVector());
  CHECK(test_1b.IsScalar());
  CHECK(!test_1b.IsFPRegister());

  VRegister test_breg_from_code(VRegister::BRegFromCode(0));
  CHECK_EQ(test_breg_from_code.SizeInBits(), kBRegSizeInBits);

  VRegister test_hreg_from_code(VRegister::HRegFromCode(0));
  CHECK_EQ(test_hreg_from_code.SizeInBits(), kHRegSizeInBits);

  VRegister test_sreg_from_code(VRegister::SRegFromCode(0));
  CHECK_EQ(test_sreg_from_code.SizeInBits(), kSRegSizeInBits);

  VRegister test_dreg_from_code(VRegister::DRegFromCode(0));
  CHECK_EQ(test_dreg_from_code.SizeInBits(), kDRegSizeInBits);

  VRegister test_qreg_from_code(VRegister::QRegFromCode(0));
  CHECK_EQ(test_qreg_from_code.SizeInBits(), kQRegSizeInBits);

  VRegister test_vreg_from_code(VRegister::VRegFromCode(0));
  CHECK_EQ(test_vreg_from_code.SizeInBits(), kVRegSizeInBits);

  VRegister test_v8b(VRegister::VRegFromCode(31).V8B());
  CHECK_EQ(test_v8b.code(), 31);
  CHECK_EQ(test_v8b.SizeInBits(), kDRegSizeInBits);
  CHECK(test_v8b.IsLaneSizeB());
  CHECK(!test_v8b.IsLaneSizeH());
  CHECK(!test_v8b.IsLaneSizeS());
  CHECK(!test_v8b.IsLaneSizeD());
  CHECK_EQ(test_v8b.LaneSizeInBits(), 8U);

  VRegister test_v16b(VRegister::VRegFromCode(31).V16B());
  CHECK_EQ(test_v16b.code(), 31);
  CHECK_EQ(test_v16b.SizeInBits(), kQRegSizeInBits);
  CHECK(test_v16b.IsLaneSizeB());
  CHECK(!test_v16b.IsLaneSizeH());
  CHECK(!test_v16b.IsLaneSizeS());
  CHECK(!test_v16b.IsLaneSizeD());
  CHECK_EQ(test_v16b.LaneSizeInBits(), 8U);

  VRegister test_v4h(VRegister::VRegFromCode(31).V4H());
  CHECK_EQ(test_v4h.code(), 31);
  CHECK_EQ(test_v4h.SizeInBits(), kDRegSizeInBits);
  CHECK(!test_v4h.IsLaneSizeB());
  CHECK(test_v4h.IsLaneSizeH());
  CHECK(!test_v4h.IsLaneSizeS());
  CHECK(!test_v4h.IsLaneSizeD());
  CHECK_EQ(test_v4h.LaneSizeInBits(), 16U);

  VRegister test_v8h(VRegister::VRegFromCode(31).V8H());
  CHECK_EQ(test_v8h.code(), 31);
  CHECK_EQ(test_v8h.SizeInBits(), kQRegSizeInBits);
  CHECK(!test_v8h.IsLaneSizeB());
  CHECK(test_v8h.IsLaneSizeH());
  CHECK(!test_v8h.IsLaneSizeS());
  CHECK(!test_v8h.IsLaneSizeD());
  CHECK_EQ(test_v8h.LaneSizeInBits(), 16U);

  VRegister test_v2s(VRegister::VRegFromCode(31).V2S());
  CHECK_EQ(test_v2s.code(), 31);
  CHECK_EQ(test_v2s.SizeInBits(), kDRegSizeInBits);
  CHECK(!test_v2s.IsLaneSizeB());
  CHECK(!test_v2s.IsLaneSizeH());
  CHECK(test_v2s.IsLaneSizeS());
  CHECK(!test_v2s.IsLaneSizeD());
  CHECK_EQ(test_v2s.LaneSizeInBits(), 32U);

  VRegister test_v4s(VRegister::VRegFromCode(31).V4S());
  CHECK_EQ(test_v4s.code(), 31);
  CHECK_EQ(test_v4s.SizeInBits(), kQRegSizeInBits);
  CHECK(!test_v4s.IsLaneSizeB());
  CHECK(!test_v4s.IsLaneSizeH());
  CHECK(test_v4s.IsLaneSizeS());
  CHECK(!test_v4s.IsLaneSizeD());
  CHECK_EQ(test_v4s.LaneSizeInBits(), 32U);

  VRegister test_v1d(VRegister::VRegFromCode(31).V1D());
  CHECK_EQ(test_v1d.code(), 31);
  CHECK_EQ(test_v1d.SizeInBits(), kDRegSizeInBits);
  CHECK(!test_v1d.IsLaneSizeB());
  CHECK(!test_v1d.IsLaneSizeH());
  CHECK(!test_v1d.IsLaneSizeS());
  CHECK(test_v1d.IsLaneSizeD());
  CHECK_EQ(test_v1d.LaneSizeInBits(), 64U);

  VRegister test_v2d(VRegister::VRegFromCode(31).V2D());
  CHECK_EQ(test_v2d.code(), 31);
  CHECK_EQ(test_v2d.SizeInBits(), kQRegSizeInBits);
  CHECK(!test_v2d.IsLaneSizeB());
  CHECK(!test_v2d.IsLaneSizeH());
  CHECK(!test_v2d.IsLaneSizeS());
  CHECK(test_v2d.IsLaneSizeD());
  CHECK_EQ(test_v2d.LaneSizeInBits(), 64U);

  CHECK(test_v1d.IsSameFormat(test_v1d));
  CHECK(test_v2d.IsSameFormat(test_v2d));
  CHECK(!test_v1d.IsSameFormat(test_v2d));
  CHECK(!test_v2s.IsSameFormat(test_v2d));
}

TEST(isvalid) {
  // This test doesn't generate any code, but it verifies some invariants
  // related to IsValid().
  CHECK(!NoReg.IsValid());
  CHECK(!NoVReg.IsValid());
  CHECK(!NoCPUReg.IsValid());

  CHECK(x0.IsValid());
  CHECK(w0.IsValid());
  CHECK(x30.IsValid());
  CHECK(w30.IsValid());
  CHECK(xzr.IsValid());
  CHECK(wzr.IsValid());

  CHECK(sp.IsValid());
  CHECK(wsp.IsValid());

  CHECK(d0.IsValid());
  CHECK(s0.IsValid());
  CHECK(d31.IsValid());
  CHECK(s31.IsValid());

  CHECK(x0.IsRegister());
  CHECK(w0.IsRegister());
  CHECK(xzr.IsRegister());
  CHECK(wzr.IsRegister());
  CHECK(sp.IsRegister());
  CHECK(wsp.IsRegister());
  CHECK(!x0.IsVRegister());
  CHECK(!w0.IsVRegister());
  CHECK(!xzr.IsVRegister());
  CHECK(!wzr.IsVRegister());
  CHECK(!sp.IsVRegister());
  CHECK(!wsp.IsVRegister());

  CHECK(d0.IsVRegister());
  CHECK(s0.IsVRegister());
  CHECK(!d0.IsRegister());
  CHECK(!s0.IsRegister());

  // Test the same as before, but using CPURegister types. This shouldn't make
  // any difference.
  CHECK(static_cast<CPURegister>(x0).IsValid());
  CHECK(static_cast<CPURegister>(w0).IsValid());
  CHECK(static_cast<CPURegister>(x30).IsValid());
  CHECK(static_cast<CPURegister>(w30).IsValid());
  CHECK(static_cast<CPURegister>(xzr).IsValid());
  CHECK(static_cast<CPURegister>(wzr).IsValid());

  CHECK(static_cast<CPURegister>(sp).IsValid());
  CHECK(static_cast<CPURegister>(wsp).IsValid());

  CHECK(static_cast<CPURegister>(d0).IsValid());
  CHECK(static_cast<CPURegister>(s0).IsValid());
  CHECK(static_cast<CPURegister>(d31).IsValid());
  CHECK(static_cast<CPURegister>(s31).IsValid());

  CHECK(static_cast<CPURegister>(x0).IsRegister());
  CHECK(static_cast<CPURegister>(w0).IsRegister());
  CHECK(static_cast<CPURegister>(xzr).IsRegister());
  CHECK(static_cast<CPURegister>(wzr).IsRegister());
  CHECK(static_cast<CPURegister>(sp).IsRegister());
  CHECK(static_cast<CPURegister>(wsp).IsRegister());
  CHECK(!static_cast<CPURegister>(x0).IsVRegister());
  CHECK(!static_cast<CPURegister>(w0).IsVRegister());
  CHECK(!static_cast<CPURegister>(xzr).IsVRegister());
  CHECK(!static_cast<CPURegister>(wzr).IsVRegister());
  CHECK(!static_cast<CPURegister>(sp).IsVRegister());
  CHECK(!static_cast<CPURegister>(wsp).IsVRegister());

  CHECK(static_cast<CPURegister>(d0).IsVRegister());
  CHECK(static_cast<CPURegister>(s0).IsVRegister());
  CHECK(!static_cast<CPURegister>(d0).IsRegister());
  CHECK(!static_cast<CPURegister>(s0).IsRegister());
}

TEST(areconsecutive) {
  // This test generates no code; it just checks that AreConsecutive works.
  CHECK(AreConsecutive(b0, NoVReg));
  CHECK(AreConsecutive(b1, b2));
  CHECK(AreConsecutive(b3, b4, b5));
  CHECK(AreConsecutive(b6, b7, b8, b9));
  CHECK(AreConsecutive(h10, NoVReg));
  CHECK(AreConsecutive(h11, h12));
  CHECK(AreConsecutive(h13, h14, h15));
  CHECK(AreConsecutive(h16, h17, h18, h19));
  CHECK(AreConsecutive(s20, NoVReg));
  CHECK(AreConsecutive(s21, s22));
  CHECK(AreConsecutive(s23, s24, s25));
  CHECK(AreConsecutive(s26, s27, s28, s29));
  CHECK(AreConsecutive(d30, NoVReg));
  CHECK(AreConsecutive(d31, d0));
  CHECK(AreConsecutive(d1, d2, d3));
  CHECK(AreConsecutive(d4, d5, d6, d7));
  CHECK(AreConsecutive(q8, NoVReg));
  CHECK(AreConsecutive(q9, q10));
  CHECK(AreConsecutive(q11, q12, q13));
  CHECK(AreConsecutive(q14, q15, q16, q17));
  CHECK(AreConsecutive(v18, NoVReg));
  CHECK(AreConsecutive(v19, v20));
  CHECK(AreConsecutive(v21, v22, v23));
  CHECK(AreConsecutive(v24, v25, v26, v27));
  CHECK(AreConsecutive(b29, h30));
  CHECK(AreConsecutive(s31, d0, q1));
  CHECK(AreConsecutive(v2, b3, h4, s5));

  CHECK(AreConsecutive(b26, b27, NoVReg, NoVReg));
  CHECK(AreConsecutive(h28, NoVReg, NoVReg, NoVReg));

  CHECK(!AreConsecutive(b0, b2));
  CHECK(!AreConsecutive(h1, h0));
  CHECK(!AreConsecutive(s31, s1));
  CHECK(!AreConsecutive(d12, d12));
  CHECK(!AreConsecutive(q31, q1));

  CHECK(!AreConsecutive(b5, b4, b3));
  CHECK(!AreConsecutive(h15, h16, h15, h14));
  CHECK(!AreConsecutive(s25, s24, s23, s22));
  CHECK(!AreConsecutive(d5, d6, d7, d6));
  CHECK(!AreConsecutive(q15, q16, q17, q6));

  CHECK(!AreConsecutive(b0, b1, b3));
  CHECK(!AreConsecutive(h4, h5, h6, h6));
  CHECK(!AreConsecutive(d15, d16, d18, NoVReg));
  CHECK(!AreConsecutive(s28, s30, NoVReg, NoVReg));
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
  CHECK(!test.IncludesAliasOf(sp));
  CHECK(!test.IncludesAliasOf(w4));
  CHECK(!test.IncludesAliasOf(w30));
  CHECK(!test.IncludesAliasOf(wzr));
  CHECK(!test.IncludesAliasOf(wsp));

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
  CHECK(!test.IncludesAliasOf(sp));
  CHECK(!test.IncludesAliasOf(w0));
  CHECK(!test.IncludesAliasOf(w9));
  CHECK(!test.IncludesAliasOf(w14));
  CHECK(!test.IncludesAliasOf(w30));
  CHECK(!test.IncludesAliasOf(wzr));
  CHECK(!test.IncludesAliasOf(wsp));

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
  CHECK(!test.IncludesAliasOf(sp));
  CHECK(!test.IncludesAliasOf(wsp));

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
  CPURegList fpreg32(CPURegister::kVRegister, kSRegSizeInBits, 0);
  CPURegList fpreg64(CPURegister::kVRegister, kDRegSizeInBits, 0);

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
  __ Mov(x29, sp);

  // Test simple integer arguments.
  __ Mov(x0, 1234);
  __ Mov(x1, 0x1234);

  // Test simple floating-point arguments.
  __ Fmov(d0, 1.234);

  // Test pointer (string) arguments.
  __ Mov(x2, reinterpret_cast<uintptr_t>(test_substring));

  // Test the maximum number of arguments, and sign extension.
  __ Mov(w3, 0xFFFFFFFF);
  __ Mov(w4, 0xFFFFFFFF);
  __ Mov(x5, 0xFFFFFFFFFFFFFFFF);
  __ Mov(x6, 0xFFFFFFFFFFFFFFFF);
  __ Fmov(s1, 1.234);
  __ Fmov(s2, 2.345);
  __ Fmov(d3, 3.456);
  __ Fmov(d4, 4.567);

  // Test printing callee-saved registers.
  __ Mov(x28, 0x123456789ABCDEF);
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

  // Print the stack pointer.
  __ Printf("StackPointer(sp): 0x%016" PRIx64 ", 0x%08" PRIx32 "\n", sp, wsp);

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
  __ Mov(w3, 0xFFFFFFFF);
  __ Mov(w4, 0xFFFFFFFF);
  __ Mov(x5, 0xFFFFFFFFFFFFFFFF);
  __ Mov(x6, 0xFFFFFFFFFFFFFFFF);
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
  __ Mov(x28, 0x123456789ABCDEF);
  __ PrintfNoPreserve("0x%" PRIx32 ", 0x%" PRIx64 "\n", w28, x28);
  __ Mov(x25, x0);

  __ Fmov(d10, 42.0);
  __ PrintfNoPreserve("%g\n", d10);
  __ Mov(x26, x0);

  // Test with three arguments.
  __ Mov(x3, 3);
  __ Mov(x4, 40);
  __ Mov(x5, 500);
  __ PrintfNoPreserve("3=%u, 4=%u, 5=%u\n", x3, x4, x5);
  __ Mov(x27, x0);

  // Mixed argument types.
  __ Mov(w3, 0xFFFFFFFF);
  __ Fmov(s1, 1.234);
  __ Mov(x5, 0xFFFFFFFFFFFFFFFF);
  __ Fmov(d3, 3.456);
  __ PrintfNoPreserve("w3: %" PRIu32 ", s1: %f, x5: %" PRIu64 ", d3: %f\n",
                      w3, s1, x5, d3);
  __ Mov(x28, x0);

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
  // 0x89ABCDEF, 0x123456789ABCDEF
  CHECK_EQUAL_64(30, x25);
  // 42
  CHECK_EQUAL_64(3, x26);
  // 3=3, 4=40, 5=500
  CHECK_EQUAL_64(17, x27);
  // w3: 4294967295, s1: 1.234000, x5: 18446744073709551615, d3: 3.456000
  CHECK_EQUAL_64(69, x28);

  TEARDOWN();
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
  __ Mov(x0, 0xDEADBEEF);
  __ B(&end);

  __ Bind(&target);
  __ Mov(x0, 0xC001C0DE);

  __ Bind(&end);
  END();

  RUN();

  CHECK_EQUAL_64(0xC001C0DE, x0);

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
  double sn = bit_cast<double>(0x7FF5555511111111);
  double qn = bit_cast<double>(0x7FFAAAAA11111111);
  CHECK(IsSignallingNaN(sn));
  CHECK(IsQuietNaN(qn));

  // The input NaNs after passing through ProcessNaN.
  double sn_proc = bit_cast<double>(0x7FFD555511111111);
  double qn_proc = qn;
  CHECK(IsQuietNaN(sn_proc));
  CHECK(IsQuietNaN(qn_proc));

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

  uint64_t qn_raw = bit_cast<uint64_t>(qn);
  uint64_t sn_raw = bit_cast<uint64_t>(sn);

  //   - Signalling NaN
  CHECK_EQUAL_FP64(sn, d1);
  CHECK_EQUAL_FP64(bit_cast<double>(sn_raw & ~kDSignMask), d2);
  CHECK_EQUAL_FP64(bit_cast<double>(sn_raw ^ kDSignMask), d3);
  //   - Quiet NaN
  CHECK_EQUAL_FP64(qn, d11);
  CHECK_EQUAL_FP64(bit_cast<double>(qn_raw & ~kDSignMask), d12);
  CHECK_EQUAL_FP64(bit_cast<double>(qn_raw ^ kDSignMask), d13);

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
  float sn = bit_cast<float>(0x7F951111);
  float qn = bit_cast<float>(0x7FEA1111);
  CHECK(IsSignallingNaN(sn));
  CHECK(IsQuietNaN(qn));

  // The input NaNs after passing through ProcessNaN.
  float sn_proc = bit_cast<float>(0x7FD51111);
  float qn_proc = qn;
  CHECK(IsQuietNaN(sn_proc));
  CHECK(IsQuietNaN(qn_proc));

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

  uint32_t qn_raw = bit_cast<uint32_t>(qn);
  uint32_t sn_raw = bit_cast<uint32_t>(sn);
  uint32_t sign_mask = static_cast<uint32_t>(kSSignMask);

  //   - Signalling NaN
  CHECK_EQUAL_FP32(sn, s1);
  CHECK_EQUAL_FP32(bit_cast<float>(sn_raw & ~sign_mask), s2);
  CHECK_EQUAL_FP32(bit_cast<float>(sn_raw ^ sign_mask), s3);
  //   - Quiet NaN
  CHECK_EQUAL_FP32(qn, s11);
  CHECK_EQUAL_FP32(bit_cast<float>(qn_raw & ~sign_mask), s12);
  CHECK_EQUAL_FP32(bit_cast<float>(qn_raw ^ sign_mask), s13);

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
  CHECK(std::isnan(n) || std::isnan(m));
  CHECK(std::isnan(expected));

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
  double sn = bit_cast<double>(0x7FF5555511111111);
  double sm = bit_cast<double>(0x7FF5555522222222);
  double qn = bit_cast<double>(0x7FFAAAAA11111111);
  double qm = bit_cast<double>(0x7FFAAAAA22222222);
  CHECK(IsSignallingNaN(sn));
  CHECK(IsSignallingNaN(sm));
  CHECK(IsQuietNaN(qn));
  CHECK(IsQuietNaN(qm));

  // The input NaNs after passing through ProcessNaN.
  double sn_proc = bit_cast<double>(0x7FFD555511111111);
  double sm_proc = bit_cast<double>(0x7FFD555522222222);
  double qn_proc = qn;
  double qm_proc = qm;
  CHECK(IsQuietNaN(sn_proc));
  CHECK(IsQuietNaN(sm_proc));
  CHECK(IsQuietNaN(qn_proc));
  CHECK(IsQuietNaN(qm_proc));

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
  CHECK(std::isnan(n) || std::isnan(m));
  CHECK(std::isnan(expected));

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
  float sn = bit_cast<float>(0x7F951111);
  float sm = bit_cast<float>(0x7F952222);
  float qn = bit_cast<float>(0x7FEA1111);
  float qm = bit_cast<float>(0x7FEA2222);
  CHECK(IsSignallingNaN(sn));
  CHECK(IsSignallingNaN(sm));
  CHECK(IsQuietNaN(qn));
  CHECK(IsQuietNaN(qm));

  // The input NaNs after passing through ProcessNaN.
  float sn_proc = bit_cast<float>(0x7FD51111);
  float sm_proc = bit_cast<float>(0x7FD52222);
  float qn_proc = qn;
  float qm_proc = qm;
  CHECK(IsQuietNaN(sn_proc));
  CHECK(IsQuietNaN(sm_proc));
  CHECK(IsQuietNaN(qn_proc));
  CHECK(IsQuietNaN(qm_proc));

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
  CHECK(std::isnan(n) || std::isnan(m) || std::isnan(a));

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
    uint32_t n_raw = bit_cast<uint32_t>(n);
    uint32_t sign_mask = static_cast<uint32_t>(kSSignMask);
    CHECK_EQUAL_FP32(n, s10);
    CHECK_EQUAL_FP32(bit_cast<float>(n_raw & ~sign_mask), s11);
    CHECK_EQUAL_FP32(bit_cast<float>(n_raw ^ sign_mask), s12);
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
  float sn = bit_cast<float>(0x7F951111);
  float sm = bit_cast<float>(0x7F952222);
  float sa = bit_cast<float>(0x7F95AAAA);
  float qn = bit_cast<float>(0x7FEA1111);
  float qm = bit_cast<float>(0x7FEA2222);
  float qa = bit_cast<float>(0x7FEAAAAA);
  CHECK(IsSignallingNaN(sn));
  CHECK(IsSignallingNaN(sm));
  CHECK(IsSignallingNaN(sa));
  CHECK(IsQuietNaN(qn));
  CHECK(IsQuietNaN(qm));
  CHECK(IsQuietNaN(qa));

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
  CHECK(std::isnan(n) || std::isnan(m) || std::isnan(a));

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
    uint64_t n_raw = bit_cast<uint64_t>(n);
    CHECK_EQUAL_FP64(n, d10);
    CHECK_EQUAL_FP64(bit_cast<double>(n_raw & ~kDSignMask), d11);
    CHECK_EQUAL_FP64(bit_cast<double>(n_raw ^ kDSignMask), d12);
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
  double sn = bit_cast<double>(0x7FF5555511111111);
  double sm = bit_cast<double>(0x7FF5555522222222);
  double sa = bit_cast<double>(0x7FF55555AAAAAAAA);
  double qn = bit_cast<double>(0x7FFAAAAA11111111);
  double qm = bit_cast<double>(0x7FFAAAAA22222222);
  double qa = bit_cast<double>(0x7FFAAAAAAAAAAAAA);
  CHECK(IsSignallingNaN(sn));
  CHECK(IsSignallingNaN(sm));
  CHECK(IsSignallingNaN(sa));
  CHECK(IsQuietNaN(qn));
  CHECK(IsQuietNaN(qm));
  CHECK(IsQuietNaN(qa));

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
  INIT_V8();
  SETUP();

  START();
  Address buf_addr = reinterpret_cast<Address>(buf);

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
    __ Call(buf_addr + function.pos(), RelocInfo::NONE);
  }
  __ Pop(xzr, lr);
  END();

  RUN();

  CHECK_EQUAL_64(1, x0);

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
    __ Abs(x13, x1, nullptr, &done);
  } else {
    // labs is undefined for kXMinInt but our implementation in the
    // MacroAssembler will return kXMinInt in such a case.
    expected = kXMinInt;

    Label next;
    // The result is not representable.
    __ Abs(x10, x1);
    __ Abs(x11, x1, nullptr, &fail);
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
    __ Abs(w13, w1, nullptr, &done);
  } else {
    // abs is undefined for kWMinInt but our implementation in the
    // MacroAssembler will return kWMinInt in such a case.
    expected = kWMinInt;

    Label next;
    // The result is not representable.
    __ Abs(w10, w1);
    __ Abs(w11, w1, nullptr, &fail);
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
  for (unsigned i = 0; i < veneer_pool_size / kInstrSize; ++i) {
    __ nop();
  }

  __ bind(&exit);

  HandleScope handle_scope(isolate);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, masm.CodeObject());

  unsigned pool_count = 0;
  int pool_mask = RelocInfo::ModeMask(RelocInfo::CONST_POOL) |
                  RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  for (RelocIterator it(*code, pool_mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (RelocInfo::IsConstPool(info->rmode())) {
      CHECK(info->data() == constant_pool_size);
      ++pool_count;
    }
    if (RelocInfo::IsVeneerPool(info->rmode())) {
      CHECK(info->data() == veneer_pool_size);
      ++pool_count;
    }
  }

  CHECK_EQ(pool_count, 2);

  TEARDOWN();
}


TEST(jump_tables_forward) {
  // Test jump tables with forward jumps.
  const int kNumCases = 512;

  INIT_V8();
  SETUP_SIZE(kNumCases * 5 * kInstrSize + 8192);
  START();

  int32_t values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  int32_t results[kNumCases];
  memset(results, 0, sizeof(results));
  uintptr_t results_ptr = reinterpret_cast<uintptr_t>(results);

  Label loop;
  Label labels[kNumCases];
  Label done;

  const Register& index = x0;
  STATIC_ASSERT(sizeof(results[0]) == 4);
  const Register& value = w1;
  const Register& target = x2;

  __ Mov(index, 0);
  __ Mov(target, results_ptr);
  __ Bind(&loop);

  {
    Assembler::BlockPoolsScope block_pools(&masm);
    Label base;

    __ Adr(x10, &base);
    __ Ldr(x11, MemOperand(x10, index, LSL, kPointerSizeLog2));
    __ Br(x11);
    __ Bind(&base);
    for (int i = 0; i < kNumCases; ++i) {
      __ dcptr(&labels[i]);
    }
  }

  for (int i = 0; i < kNumCases; ++i) {
    __ Bind(&labels[i]);
    __ Mov(value, values[i]);
    __ B(&done);
  }

  __ Bind(&done);
  __ Str(value, MemOperand(target, 4, PostIndex));
  __ Add(index, index, 1);
  __ Cmp(index, kNumCases);
  __ B(ne, &loop);

  END();

  RUN();

  for (int i = 0; i < kNumCases; ++i) {
    CHECK_EQ(values[i], results[i]);
  }

  TEARDOWN();
}


TEST(jump_tables_backward) {
  // Test jump tables with backward jumps.
  const int kNumCases = 512;

  INIT_V8();
  SETUP_SIZE(kNumCases * 5 * kInstrSize + 8192);
  START();

  int32_t values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  int32_t results[kNumCases];
  memset(results, 0, sizeof(results));
  uintptr_t results_ptr = reinterpret_cast<uintptr_t>(results);

  Label loop;
  Label labels[kNumCases];
  Label done;

  const Register& index = x0;
  STATIC_ASSERT(sizeof(results[0]) == 4);
  const Register& value = w1;
  const Register& target = x2;

  __ Mov(index, 0);
  __ Mov(target, results_ptr);
  __ B(&loop);

  for (int i = 0; i < kNumCases; ++i) {
    __ Bind(&labels[i]);
    __ Mov(value, values[i]);
    __ B(&done);
  }

  __ Bind(&loop);
  {
    Assembler::BlockPoolsScope block_pools(&masm);
    Label base;

    __ Adr(x10, &base);
    __ Ldr(x11, MemOperand(x10, index, LSL, kPointerSizeLog2));
    __ Br(x11);
    __ Bind(&base);
    for (int i = 0; i < kNumCases; ++i) {
      __ dcptr(&labels[i]);
    }
  }

  __ Bind(&done);
  __ Str(value, MemOperand(target, 4, PostIndex));
  __ Add(index, index, 1);
  __ Cmp(index, kNumCases);
  __ B(ne, &loop);

  END();

  RUN();

  for (int i = 0; i < kNumCases; ++i) {
    CHECK_EQ(values[i], results[i]);
  }

  TEARDOWN();
}


TEST(internal_reference_linked) {
  // Test internal reference when they are linked in a label chain.

  INIT_V8();
  SETUP();
  START();

  Label done;

  __ Mov(x0, 0);
  __ Cbnz(x0, &done);

  {
    Assembler::BlockPoolsScope block_pools(&masm);
    Label base;

    __ Adr(x10, &base);
    __ Ldr(x11, MemOperand(x10));
    __ Br(x11);
    __ Bind(&base);
    __ dcptr(&done);
  }

  // Dead code, just to extend the label chain.
  __ B(&done);
  __ dcptr(&done);
  __ Tbz(x0, 1, &done);

  __ Bind(&done);
  __ Mov(x0, 1);

  END();

  RUN();

  CHECK_EQUAL_64(0x1, x0);

  TEARDOWN();
}

}  // namespace internal
}  // namespace v8

#undef __
#undef BUF_SIZE
#undef SETUP
#undef INIT_V8
#undef SETUP_SIZE
#undef RESET
#undef START_AFTER_RESET
#undef START
#undef RUN
#undef END
#undef TEARDOWN
#undef CHECK_EQUAL_NZCV
#undef CHECK_EQUAL_REGISTERS
#undef CHECK_EQUAL_32
#undef CHECK_EQUAL_FP32
#undef CHECK_EQUAL_64
#undef CHECK_EQUAL_FP64
#undef CHECK_EQUAL_128
#undef CHECK_CONSTANT_POOL_SIZE
