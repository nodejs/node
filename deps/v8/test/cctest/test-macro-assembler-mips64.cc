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

#include <stdlib.h>
#include <iostream>  // NOLINT(readability/streams)

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/utils/random-number-generator.h"
#include "src/macro-assembler.h"
#include "src/mips64/macro-assembler-mips64.h"
#include "src/objects-inl.h"
#include "src/simulator.h"

namespace v8 {
namespace internal {

// TODO(mips64): Refine these signatures per test case.
using FV = void*(int64_t x, int64_t y, int p2, int p3, int p4);
using F1 = Object*(int x, int p1, int p2, int p3, int p4);
using F3 = Object*(void* p, int p1, int p2, int p3, int p4);
using F4 = Object*(void* p0, void* p1, int p2, int p3, int p4);

#define __ masm->

TEST(BYTESWAP) {
  DCHECK(kArchVariant == kMips64r6 || kArchVariant == kMips64r2);
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  struct T {
    int64_t r1;
    int64_t r2;
    int64_t r3;
    int64_t r4;
    int64_t r5;
    int64_t r6;
    int64_t r7;
  };
  T t;

  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;

  __ Ld(a4, MemOperand(a0, offsetof(T, r1)));
  __ nop();
  __ ByteSwapSigned(a4, a4, 8);
  __ Sd(a4, MemOperand(a0, offsetof(T, r1)));

  __ Ld(a4, MemOperand(a0, offsetof(T, r2)));
  __ nop();
  __ ByteSwapSigned(a4, a4, 4);
  __ Sd(a4, MemOperand(a0, offsetof(T, r2)));

  __ Ld(a4, MemOperand(a0, offsetof(T, r3)));
  __ nop();
  __ ByteSwapSigned(a4, a4, 2);
  __ Sd(a4, MemOperand(a0, offsetof(T, r3)));

  __ Ld(a4, MemOperand(a0, offsetof(T, r4)));
  __ nop();
  __ ByteSwapSigned(a4, a4, 1);
  __ Sd(a4, MemOperand(a0, offsetof(T, r4)));

  __ Ld(a4, MemOperand(a0, offsetof(T, r5)));
  __ nop();
  __ ByteSwapUnsigned(a4, a4, 1);
  __ Sd(a4, MemOperand(a0, offsetof(T, r5)));

  __ Ld(a4, MemOperand(a0, offsetof(T, r6)));
  __ nop();
  __ ByteSwapUnsigned(a4, a4, 2);
  __ Sd(a4, MemOperand(a0, offsetof(T, r6)));

  __ Ld(a4, MemOperand(a0, offsetof(T, r7)));
  __ nop();
  __ ByteSwapUnsigned(a4, a4, 4);
  __ Sd(a4, MemOperand(a0, offsetof(T, r7)));

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
  auto f = GeneratedCode<F3>::FromCode(*code);
  t.r1 = 0x5612FFCD9D327ACC;
  t.r2 = 0x781A15C3;
  t.r3 = 0xFCDE;
  t.r4 = 0x9F;
  t.r5 = 0x9F;
  t.r6 = 0xFCDE;
  t.r7 = 0xC81A15C3;
  f.Call(&t, 0, 0, 0, 0);

  CHECK_EQ(static_cast<int64_t>(0xCC7A329DCDFF1256), t.r1);
  CHECK_EQ(static_cast<int64_t>(0xC3151A7800000000), t.r2);
  CHECK_EQ(static_cast<int64_t>(0xDEFCFFFFFFFFFFFF), t.r3);
  CHECK_EQ(static_cast<int64_t>(0x9FFFFFFFFFFFFFFF), t.r4);
  CHECK_EQ(static_cast<int64_t>(0x9F00000000000000), t.r5);
  CHECK_EQ(static_cast<int64_t>(0xDEFC000000000000), t.r6);
  CHECK_EQ(static_cast<int64_t>(0xC3151AC800000000), t.r7);
}

TEST(LoadConstants) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  int64_t refConstants[64];
  int64_t result[64];

  int64_t mask = 1;
  for (int i = 0; i < 64; i++) {
    refConstants[i] = ~(mask << i);
  }

  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  __ mov(a4, a0);
  for (int i = 0; i < 64; i++) {
    // Load constant.
    __ li(a5, Operand(refConstants[i]));
    __ Sd(a5, MemOperand(a4));
    __ Daddu(a4, a4, Operand(kPointerSize));
  }

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<FV>::FromCode(*code);
  (void)f.Call(reinterpret_cast<int64_t>(result), 0, 0, 0, 0);
  // Check results.
  for (int i = 0; i < 64; i++) {
    CHECK(refConstants[i] == result[i]);
  }
}


TEST(LoadAddress) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;
  Label to_jump, skip;
  __ mov(a4, a0);

  __ Branch(&skip);
  __ bind(&to_jump);
  __ nop();
  __ nop();
  __ jr(ra);
  __ nop();
  __ bind(&skip);
  __ li(a4, Operand(masm->jump_address(&to_jump)), ADDRESS_LOAD);
  int check_size = masm->InstructionsGeneratedSince(&skip);
  CHECK_EQ(4, check_size);
  __ jr(a4);
  __ nop();
  __ stop("invalid");
  __ stop("invalid");
  __ stop("invalid");
  __ stop("invalid");
  __ stop("invalid");


  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<FV>::FromCode(*code);
  (void)f.Call(0, 0, 0, 0, 0);
  // Check results.
}


TEST(jump_tables4) {
  // Similar to test-assembler-mips jump_tables1, with extra test for branch
  // trampoline required before emission of the dd table (where trampolines are
  // blocked), and proper transition to long-branch mode.
  // Regression test for v8:4294.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  const int kNumCases = 512;
  int values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kNumCases];
  Label near_start, end, done;

  __ Push(ra);
  __ mov(v0, zero_reg);

  __ Branch(&end);
  __ bind(&near_start);

  // Generate slightly less than 32K instructions, which will soon require
  // trampoline for branch distance fixup.
  for (int i = 0; i < 32768 - 256; ++i) {
    __ addiu(v0, v0, 1);
  }

  __ GenerateSwitchTable(a0, kNumCases,
                         [&labels](size_t i) { return labels + i; });

  for (int i = 0; i < kNumCases; ++i) {
    __ bind(&labels[i]);
    __ li(v0, values[i]);
    __ Branch(&done);
  }

  __ bind(&done);
  __ Pop(ra);
  __ jr(ra);
  __ nop();

  __ bind(&end);
  __ Branch(&near_start);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  for (int i = 0; i < kNumCases; ++i) {
    int64_t res = reinterpret_cast<int64_t>(f.Call(i, 0, 0, 0, 0));
    ::printf("f(%d) = %" PRId64 "\n", i, res);
    CHECK_EQ(values[i], res);
  }
}


TEST(jump_tables5) {
  if (kArchVariant != kMips64r6) return;

  // Similar to test-assembler-mips jump_tables1, with extra test for emitting a
  // compact branch instruction before emission of the dd table.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  const int kNumCases = 512;
  int values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kNumCases];
  Label done;

  __ Push(ra);

  // Opposite of Align(8) as we have unaligned number of instructions in the
  // following block before the first dd().
  if ((masm->pc_offset() & 7) == 0) {
    __ nop();
  }

  {
    __ BlockTrampolinePoolFor(kNumCases * 2 + 6 + 1);
    PredictableCodeSizeScope predictable(
        masm, kNumCases * kPointerSize + ((6 + 1) * Assembler::kInstrSize));

    __ addiupc(at, 6 + 1);
    __ Dlsa(at, at, a0, 3);
    __ Ld(at, MemOperand(at));
    __ jalr(at);
    __ nop();  // Branch delay slot nop.
    __ bc(&done);
    // A nop instruction must be generated by the forbidden slot guard
    // (Assembler::dd(Label*)) so the first label goes to an 8 bytes aligned
    // location.
    for (int i = 0; i < kNumCases; ++i) {
      __ dd(&labels[i]);
    }
  }

  for (int i = 0; i < kNumCases; ++i) {
    __ bind(&labels[i]);
    __ li(v0, values[i]);
    __ jr(ra);
    __ nop();
  }

  __ bind(&done);
  __ Pop(ra);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  for (int i = 0; i < kNumCases; ++i) {
    int64_t res = reinterpret_cast<int64_t>(f.Call(i, 0, 0, 0, 0));
    ::printf("f(%d) = %" PRId64 "\n", i, res);
    CHECK_EQ(values[i], res);
  }
}

TEST(jump_tables6) {
  // Similar to test-assembler-mips jump_tables1, with extra test for branch
  // trampoline required after emission of the dd table (where trampolines are
  // blocked). This test checks if number of really generated instructions is
  // greater than number of counted instructions from code, as we are expecting
  // generation of trampoline in this case (when number of kFillInstr
  // instructions is close to 32K)
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  const int kSwitchTableCases = 40;

  const int kInstrSize = Assembler::kInstrSize;
  const int kMaxBranchOffset = Assembler::kMaxBranchOffset;
  const int kTrampolineSlotsSize = Assembler::kTrampolineSlotsSize;
  const int kSwitchTablePrologueSize = MacroAssembler::kSwitchTablePrologueSize;

  const int kMaxOffsetForTrampolineStart =
      kMaxBranchOffset - 16 * kTrampolineSlotsSize;
  const int kFillInstr = (kMaxOffsetForTrampolineStart / kInstrSize) -
                         (kSwitchTablePrologueSize + 2 * kSwitchTableCases) -
                         20;

  int values[kSwitchTableCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kSwitchTableCases];
  Label near_start, end, done;

  __ Push(ra);
  __ mov(v0, zero_reg);

  int offs1 = masm->pc_offset();
  int gen_insn = 0;

  __ Branch(&end);
  gen_insn += Assembler::IsCompactBranchSupported() ? 1 : 2;
  __ bind(&near_start);

  // Generate slightly less than 32K instructions, which will soon require
  // trampoline for branch distance fixup.
  for (int i = 0; i < kFillInstr; ++i) {
    __ addiu(v0, v0, 1);
  }
  gen_insn += kFillInstr;

  __ GenerateSwitchTable(a0, kSwitchTableCases,
                         [&labels](size_t i) { return labels + i; });
  gen_insn += (kSwitchTablePrologueSize + 2 * kSwitchTableCases);

  for (int i = 0; i < kSwitchTableCases; ++i) {
    __ bind(&labels[i]);
    __ li(v0, values[i]);
    __ Branch(&done);
  }
  gen_insn +=
      ((Assembler::IsCompactBranchSupported() ? 3 : 4) * kSwitchTableCases);

  // If offset from here to first branch instr is greater than max allowed
  // offset for trampoline ...
  CHECK_LT(kMaxOffsetForTrampolineStart, masm->pc_offset() - offs1);
  // ... number of generated instructions must be greater then "gen_insn",
  // as we are expecting trampoline generation
  CHECK_LT(gen_insn, (masm->pc_offset() - offs1) / kInstrSize);

  __ bind(&done);
  __ Pop(ra);
  __ jr(ra);
  __ nop();

  __ bind(&end);
  __ Branch(&near_start);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  for (int i = 0; i < kSwitchTableCases; ++i) {
    int64_t res = reinterpret_cast<int64_t>(f.Call(i, 0, 0, 0, 0));
    ::printf("f(%d) = %" PRId64 "\n", i, res);
    CHECK_EQ(values[i], res);
  }
}

static uint64_t run_lsa(uint32_t rt, uint32_t rs, int8_t sa) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  __ Lsa(v0, a0, a1, sa);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assembler.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<F1>::FromCode(*code);

  uint64_t res = reinterpret_cast<uint64_t>(f.Call(rt, rs, 0, 0, 0));

  return res;
}


TEST(Lsa) {
  CcTest::InitializeVM();
  struct TestCaseLsa {
    int32_t rt;
    int32_t rs;
    uint8_t sa;
    uint64_t expected_res;
  };

  struct TestCaseLsa tc[] = {// rt, rs, sa, expected_res
                             {0x4, 0x1, 1, 0x6},
                             {0x4, 0x1, 2, 0x8},
                             {0x4, 0x1, 3, 0xC},
                             {0x4, 0x1, 4, 0x14},
                             {0x4, 0x1, 5, 0x24},
                             {0x0, 0x1, 1, 0x2},
                             {0x0, 0x1, 2, 0x4},
                             {0x0, 0x1, 3, 0x8},
                             {0x0, 0x1, 4, 0x10},
                             {0x0, 0x1, 5, 0x20},
                             {0x4, 0x0, 1, 0x4},
                             {0x4, 0x0, 2, 0x4},
                             {0x4, 0x0, 3, 0x4},
                             {0x4, 0x0, 4, 0x4},
                             {0x4, 0x0, 5, 0x4},

                             // Shift overflow.
                             {0x4, INT32_MAX, 1, 0x2},
                             {0x4, INT32_MAX >> 1, 2, 0x0},
                             {0x4, INT32_MAX >> 2, 3, 0xFFFFFFFFFFFFFFFC},
                             {0x4, INT32_MAX >> 3, 4, 0xFFFFFFFFFFFFFFF4},
                             {0x4, INT32_MAX >> 4, 5, 0xFFFFFFFFFFFFFFE4},

                             // Signed addition overflow.
                             {INT32_MAX - 1, 0x1, 1, 0xFFFFFFFF80000000},
                             {INT32_MAX - 3, 0x1, 2, 0xFFFFFFFF80000000},
                             {INT32_MAX - 7, 0x1, 3, 0xFFFFFFFF80000000},
                             {INT32_MAX - 15, 0x1, 4, 0xFFFFFFFF80000000},
                             {INT32_MAX - 31, 0x1, 5, 0xFFFFFFFF80000000},

                             // Addition overflow.
                             {-2, 0x1, 1, 0x0},
                             {-4, 0x1, 2, 0x0},
                             {-8, 0x1, 3, 0x0},
                             {-16, 0x1, 4, 0x0},
                             {-32, 0x1, 5, 0x0}};

  size_t nr_test_cases = sizeof(tc) / sizeof(TestCaseLsa);
  for (size_t i = 0; i < nr_test_cases; ++i) {
    uint64_t res = run_lsa(tc[i].rt, tc[i].rs, tc[i].sa);
    PrintF("0x%" PRIx64 " =? 0x%" PRIx64 " == Lsa(v0, %x, %x, %hhu)\n",
           tc[i].expected_res, res, tc[i].rt, tc[i].rs, tc[i].sa);
    CHECK_EQ(tc[i].expected_res, res);
  }
}


static uint64_t run_dlsa(uint64_t rt, uint64_t rs, int8_t sa) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  __ Dlsa(v0, a0, a1, sa);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assembler.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<FV>::FromCode(*code);

  uint64_t res = reinterpret_cast<uint64_t>(f.Call(rt, rs, 0, 0, 0));

  return res;
}


TEST(Dlsa) {
  CcTest::InitializeVM();
  struct TestCaseLsa {
    int64_t rt;
    int64_t rs;
    uint8_t sa;
    uint64_t expected_res;
  };

  struct TestCaseLsa tc[] = {// rt, rs, sa, expected_res
                             {0x4, 0x1, 1, 0x6},
                             {0x4, 0x1, 2, 0x8},
                             {0x4, 0x1, 3, 0xC},
                             {0x4, 0x1, 4, 0x14},
                             {0x4, 0x1, 5, 0x24},
                             {0x0, 0x1, 1, 0x2},
                             {0x0, 0x1, 2, 0x4},
                             {0x0, 0x1, 3, 0x8},
                             {0x0, 0x1, 4, 0x10},
                             {0x0, 0x1, 5, 0x20},
                             {0x4, 0x0, 1, 0x4},
                             {0x4, 0x0, 2, 0x4},
                             {0x4, 0x0, 3, 0x4},
                             {0x4, 0x0, 4, 0x4},
                             {0x4, 0x0, 5, 0x4},

                             // Shift overflow.
                             {0x4, INT64_MAX, 1, 0x2},
                             {0x4, INT64_MAX >> 1, 2, 0x0},
                             {0x4, INT64_MAX >> 2, 3, 0xFFFFFFFFFFFFFFFC},
                             {0x4, INT64_MAX >> 3, 4, 0xFFFFFFFFFFFFFFF4},
                             {0x4, INT64_MAX >> 4, 5, 0xFFFFFFFFFFFFFFE4},

                             // Signed addition overflow.
                             {INT64_MAX - 1, 0x1, 1, 0x8000000000000000},
                             {INT64_MAX - 3, 0x1, 2, 0x8000000000000000},
                             {INT64_MAX - 7, 0x1, 3, 0x8000000000000000},
                             {INT64_MAX - 15, 0x1, 4, 0x8000000000000000},
                             {INT64_MAX - 31, 0x1, 5, 0x8000000000000000},

                             // Addition overflow.
                             {-2, 0x1, 1, 0x0},
                             {-4, 0x1, 2, 0x0},
                             {-8, 0x1, 3, 0x0},
                             {-16, 0x1, 4, 0x0},
                             {-32, 0x1, 5, 0x0}};

  size_t nr_test_cases = sizeof(tc) / sizeof(TestCaseLsa);
  for (size_t i = 0; i < nr_test_cases; ++i) {
    uint64_t res = run_dlsa(tc[i].rt, tc[i].rs, tc[i].sa);
    PrintF("0x%" PRIx64 " =? 0x%" PRIx64 " == Dlsa(v0, %" PRIx64 ", %" PRIx64
           ", %hhu)\n",
           tc[i].expected_res, res, tc[i].rt, tc[i].rs, tc[i].sa);
    CHECK_EQ(tc[i].expected_res, res);
  }
}

static const std::vector<uint32_t> cvt_trunc_uint32_test_values() {
  static const uint32_t kValues[] = {0x00000000, 0x00000001, 0x00FFFF00,
                                     0x7FFFFFFF, 0x80000000, 0x80000001,
                                     0x80FFFF00, 0x8FFFFFFF, 0xFFFFFFFF};
  return std::vector<uint32_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

static const std::vector<int32_t> cvt_trunc_int32_test_values() {
  static const int32_t kValues[] = {
      static_cast<int32_t>(0x00000000), static_cast<int32_t>(0x00000001),
      static_cast<int32_t>(0x00FFFF00), static_cast<int32_t>(0x7FFFFFFF),
      static_cast<int32_t>(0x80000000), static_cast<int32_t>(0x80000001),
      static_cast<int32_t>(0x80FFFF00), static_cast<int32_t>(0x8FFFFFFF),
      static_cast<int32_t>(0xFFFFFFFF)};
  return std::vector<int32_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

static const std::vector<uint64_t> cvt_trunc_uint64_test_values() {
  static const uint64_t kValues[] = {
      0x0000000000000000, 0x0000000000000001, 0x0000FFFFFFFF0000,
      0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0x8000000000000001,
      0x8000FFFFFFFF0000, 0x8FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF};
  return std::vector<uint64_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

static const std::vector<int64_t> cvt_trunc_int64_test_values() {
  static const int64_t kValues[] = {static_cast<int64_t>(0x0000000000000000),
                                    static_cast<int64_t>(0x0000000000000001),
                                    static_cast<int64_t>(0x0000FFFFFFFF0000),
                                    static_cast<int64_t>(0x7FFFFFFFFFFFFFFF),
                                    static_cast<int64_t>(0x8000000000000000),
                                    static_cast<int64_t>(0x8000000000000001),
                                    static_cast<int64_t>(0x8000FFFFFFFF0000),
                                    static_cast<int64_t>(0x8FFFFFFFFFFFFFFF),
                                    static_cast<int64_t>(0xFFFFFFFFFFFFFFFF)};
  return std::vector<int64_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

// Helper macros that can be used in FOR_INT32_INPUTS(i) { ... *i ... }
#define FOR_INPUTS(ctype, itype, var, test_vector)           \
  std::vector<ctype> var##_vec = test_vector();              \
  for (std::vector<ctype>::iterator var = var##_vec.begin(); \
       var != var##_vec.end(); ++var)

#define FOR_INPUTS2(ctype, itype, var, var2, test_vector)  \
  std::vector<ctype> var##_vec = test_vector();            \
  std::vector<ctype>::iterator var;                        \
  std::vector<ctype>::reverse_iterator var2;               \
  for (var = var##_vec.begin(), var2 = var##_vec.rbegin(); \
       var != var##_vec.end(); ++var, ++var2)

#define FOR_ENUM_INPUTS(var, type, test_vector) \
  FOR_INPUTS(enum type, type, var, test_vector)
#define FOR_STRUCT_INPUTS(var, type, test_vector) \
  FOR_INPUTS(struct type, type, var, test_vector)
#define FOR_INT32_INPUTS(var, test_vector) \
  FOR_INPUTS(int32_t, int32, var, test_vector)
#define FOR_INT32_INPUTS2(var, var2, test_vector) \
  FOR_INPUTS2(int32_t, int32, var, var2, test_vector)
#define FOR_INT64_INPUTS(var, test_vector) \
  FOR_INPUTS(int64_t, int64, var, test_vector)
#define FOR_UINT32_INPUTS(var, test_vector) \
  FOR_INPUTS(uint32_t, uint32, var, test_vector)
#define FOR_UINT64_INPUTS(var, test_vector) \
  FOR_INPUTS(uint64_t, uint64, var, test_vector)

template <typename RET_TYPE, typename IN_TYPE, typename Func>
RET_TYPE run_Cvt(IN_TYPE x, Func GenerateConvertInstructionFunc) {
  typedef RET_TYPE(F_CVT)(IN_TYPE x0, int x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;

  GenerateConvertInstructionFunc(masm);
  __ dmfc1(v0, f2);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<F_CVT>::FromCode(*code);

  return reinterpret_cast<RET_TYPE>(f.Call(x, 0, 0, 0, 0));
}

TEST(Cvt_s_uw_Trunc_uw_s) {
  CcTest::InitializeVM();
  FOR_UINT32_INPUTS(i, cvt_trunc_uint32_test_values) {
    uint32_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ Cvt_s_uw(f0, a0);
      __ mthc1(zero_reg, f2);
      __ Trunc_uw_s(f2, f0, f1);
    };
    CHECK_EQ(static_cast<float>(input), run_Cvt<uint64_t>(input, fn));
  }
}

TEST(Cvt_s_ul_Trunc_ul_s) {
  CcTest::InitializeVM();
  FOR_UINT64_INPUTS(i, cvt_trunc_uint64_test_values) {
    uint64_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ Cvt_s_ul(f0, a0);
      __ Trunc_ul_s(f2, f0, f1, v0);
    };
    CHECK_EQ(static_cast<float>(input), run_Cvt<uint64_t>(input, fn));
  }
}

TEST(Cvt_d_ul_Trunc_ul_d) {
  CcTest::InitializeVM();
  FOR_UINT64_INPUTS(i, cvt_trunc_uint64_test_values) {
    uint64_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ Cvt_d_ul(f0, a0);
      __ Trunc_ul_d(f2, f0, f1, v0);
    };
    CHECK_EQ(static_cast<double>(input), run_Cvt<uint64_t>(input, fn));
  }
}

TEST(cvt_d_l_Trunc_l_d) {
  CcTest::InitializeVM();
  FOR_INT64_INPUTS(i, cvt_trunc_int64_test_values) {
    int64_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ dmtc1(a0, f4);
      __ cvt_d_l(f0, f4);
      __ Trunc_l_d(f2, f0);
    };
    CHECK_EQ(static_cast<double>(input), run_Cvt<int64_t>(input, fn));
  }
}

TEST(cvt_d_l_Trunc_l_ud) {
  CcTest::InitializeVM();
  FOR_INT64_INPUTS(i, cvt_trunc_int64_test_values) {
    int64_t input = *i;
    uint64_t abs_input = (input < 0) ? -input : input;
    auto fn = [](MacroAssembler* masm) {
      __ dmtc1(a0, f4);
      __ cvt_d_l(f0, f4);
      __ Trunc_l_ud(f2, f0, f6);
    };
    CHECK_EQ(static_cast<double>(abs_input), run_Cvt<uint64_t>(input, fn));
  }
}

TEST(cvt_d_w_Trunc_w_d) {
  CcTest::InitializeVM();
  FOR_INT32_INPUTS(i, cvt_trunc_int32_test_values) {
    int32_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ mtc1(a0, f4);
      __ cvt_d_w(f0, f4);
      __ Trunc_w_d(f2, f0);
      __ mfc1(v1, f2);
      __ dmtc1(v1, f2);
    };
    CHECK_EQ(static_cast<double>(input), run_Cvt<int64_t>(input, fn));
  }
}

static const std::vector<int64_t> overflow_int64_test_values() {
  static const int64_t kValues[] = {static_cast<int64_t>(0xF000000000000000),
                                    static_cast<int64_t>(0x0000000000000001),
                                    static_cast<int64_t>(0xFF00000000000000),
                                    static_cast<int64_t>(0x0000F00111111110),
                                    static_cast<int64_t>(0x0F00001000000000),
                                    static_cast<int64_t>(0x991234AB12A96731),
                                    static_cast<int64_t>(0xB0FFFF0F0F0F0F01),
                                    static_cast<int64_t>(0x00006FFFFFFFFFFF),
                                    static_cast<int64_t>(0xFFFFFFFFFFFFFFFF)};
  return std::vector<int64_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

enum OverflowBranchType {
  kAddBranchOverflow,
  kSubBranchOverflow,
};

struct OverflowRegisterCombination {
  Register dst;
  Register left;
  Register right;
  Register scratch;
};

static const std::vector<enum OverflowBranchType> overflow_branch_type() {
  static const enum OverflowBranchType kValues[] = {kAddBranchOverflow,
                                                    kSubBranchOverflow};
  return std::vector<enum OverflowBranchType>(&kValues[0],
                                              &kValues[arraysize(kValues)]);
}

static const std::vector<struct OverflowRegisterCombination>
overflow_register_combination() {
  static const struct OverflowRegisterCombination kValues[] = {
      {t0, t1, t2, t3}, {t0, t0, t2, t3}, {t0, t1, t0, t3}, {t0, t1, t1, t3}};
  return std::vector<struct OverflowRegisterCombination>(
      &kValues[0], &kValues[arraysize(kValues)]);
}

template <typename T>
static bool IsAddOverflow(T x, T y) {
  DCHECK(std::numeric_limits<T>::is_integer);
  T max = std::numeric_limits<T>::max();
  T min = std::numeric_limits<T>::min();

  return (x > 0 && y > (max - x)) || (x < 0 && y < (min - x));
}

template <typename T>
static bool IsSubOverflow(T x, T y) {
  DCHECK(std::numeric_limits<T>::is_integer);
  T max = std::numeric_limits<T>::max();
  T min = std::numeric_limits<T>::min();

  return (y > 0 && x < (min + y)) || (y < 0 && x > (max + y));
}

template <typename IN_TYPE, typename Func>
static bool runOverflow(IN_TYPE valLeft, IN_TYPE valRight,
                        Func GenerateOverflowInstructions) {
  typedef int64_t(F_CVT)(char* x0, int x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;

  GenerateOverflowInstructions(masm, valLeft, valRight);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<F_CVT>::FromCode(*code);

  int64_t r = reinterpret_cast<int64_t>(f.Call(0, 0, 0, 0, 0));

  DCHECK(r == 0 || r == 1);
  return r;
}

TEST(BranchOverflowInt64BothLabels) {
  FOR_INT64_INPUTS(i, overflow_int64_test_values) {
    FOR_INT64_INPUTS(j, overflow_int64_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int64_t ii = *i;
          int64_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int64_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int64_t valLeft,
                                       int64_t valRight) {
                Label overflow, no_overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ DaddBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                     &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ DsubBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                     &no_overflow, rc.scratch);
                    break;
                }
                __ li(v0, 2);
                __ Branch(&end);
                __ bind(&overflow);
                __ li(v0, 1);
                __ Branch(&end);
                __ bind(&no_overflow);
                __ li(v0, 0);
                __ bind(&end);
              });

          bool res2 = runOverflow<int64_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int64_t valLeft,
                                       int64_t valRight) {
                Label overflow, no_overflow, end;
                __ li(rc.left, valLeft);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ DaddBranchOvf(rc.dst, rc.left, Operand(valRight),
                                     &overflow, &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ DsubBranchOvf(rc.dst, rc.left, Operand(valRight),
                                     &overflow, &no_overflow, rc.scratch);
                    break;
                }
                __ li(v0, 2);
                __ Branch(&end);
                __ bind(&overflow);
                __ li(v0, 1);
                __ Branch(&end);
                __ bind(&no_overflow);
                __ li(v0, 0);
                __ bind(&end);
              });

          switch (branchType) {
            case kAddBranchOverflow:
              CHECK_EQ(IsAddOverflow<int64_t>(ii, jj), res1);
              CHECK_EQ(IsAddOverflow<int64_t>(ii, jj), res2);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int64_t>(ii, jj), res1);
              CHECK_EQ(IsSubOverflow<int64_t>(ii, jj), res2);
              break;
            default:
              UNREACHABLE();
          }
        }
      }
    }
  }
}

TEST(BranchOverflowInt64LeftLabel) {
  FOR_INT64_INPUTS(i, overflow_int64_test_values) {
    FOR_INT64_INPUTS(j, overflow_int64_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int64_t ii = *i;
          int64_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int64_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int64_t valLeft,
                                       int64_t valRight) {
                Label overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ DaddBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                     nullptr, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ DsubBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                     nullptr, rc.scratch);
                    break;
                }
                __ li(v0, 0);
                __ Branch(&end);
                __ bind(&overflow);
                __ li(v0, 1);
                __ bind(&end);
              });

          bool res2 = runOverflow<int64_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int64_t valLeft,
                                       int64_t valRight) {
                Label overflow, end;
                __ li(rc.left, valLeft);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ DaddBranchOvf(rc.dst, rc.left, Operand(valRight),
                                     &overflow, nullptr, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ DsubBranchOvf(rc.dst, rc.left, Operand(valRight),
                                     &overflow, nullptr, rc.scratch);
                    break;
                }
                __ li(v0, 0);
                __ Branch(&end);
                __ bind(&overflow);
                __ li(v0, 1);
                __ bind(&end);
              });

          switch (branchType) {
            case kAddBranchOverflow:
              CHECK_EQ(IsAddOverflow<int64_t>(ii, jj), res1);
              CHECK_EQ(IsAddOverflow<int64_t>(ii, jj), res2);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int64_t>(ii, jj), res1);
              CHECK_EQ(IsSubOverflow<int64_t>(ii, jj), res2);
              break;
            default:
              UNREACHABLE();
          }
        }
      }
    }
  }
}

TEST(BranchOverflowInt64RightLabel) {
  FOR_INT64_INPUTS(i, overflow_int64_test_values) {
    FOR_INT64_INPUTS(j, overflow_int64_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int64_t ii = *i;
          int64_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int64_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int64_t valLeft,
                                       int64_t valRight) {
                Label no_overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ DaddBranchOvf(rc.dst, rc.left, rc.right, nullptr,
                                     &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ DsubBranchOvf(rc.dst, rc.left, rc.right, nullptr,
                                     &no_overflow, rc.scratch);
                    break;
                }
                __ li(v0, 1);
                __ Branch(&end);
                __ bind(&no_overflow);
                __ li(v0, 0);
                __ bind(&end);
              });

          bool res2 = runOverflow<int64_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int64_t valLeft,
                                       int64_t valRight) {
                Label no_overflow, end;
                __ li(rc.left, valLeft);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ DaddBranchOvf(rc.dst, rc.left, Operand(valRight),
                                     nullptr, &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ DsubBranchOvf(rc.dst, rc.left, Operand(valRight),
                                     nullptr, &no_overflow, rc.scratch);
                    break;
                }
                __ li(v0, 1);
                __ Branch(&end);
                __ bind(&no_overflow);
                __ li(v0, 0);
                __ bind(&end);
              });

          switch (branchType) {
            case kAddBranchOverflow:
              CHECK_EQ(IsAddOverflow<int64_t>(ii, jj), res1);
              CHECK_EQ(IsAddOverflow<int64_t>(ii, jj), res2);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int64_t>(ii, jj), res1);
              CHECK_EQ(IsSubOverflow<int64_t>(ii, jj), res2);
              break;
            default:
              UNREACHABLE();
          }
        }
      }
    }
  }
}

TEST(min_max_nan) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  struct TestFloat {
    double a;
    double b;
    double c;
    double d;
    float e;
    float f;
    float g;
    float h;
  };

  TestFloat test;
  const double dnan = std::numeric_limits<double>::quiet_NaN();
  const double dinf = std::numeric_limits<double>::infinity();
  const double dminf = -std::numeric_limits<double>::infinity();
  const float fnan = std::numeric_limits<float>::quiet_NaN();
  const float finf = std::numeric_limits<float>::infinity();
  const float fminf = std::numeric_limits<float>::infinity();
  const int kTableLength = 13;

  double inputsa[kTableLength] = {2.0,  3.0,  -0.0, 0.0,  42.0, dinf, dminf,
                                  dinf, dnan, 3.0,  dinf, dnan, dnan};
  double inputsb[kTableLength] = {3.0,   2.0, 0.0,  -0.0, dinf, 42.0, dinf,
                                  dminf, 3.0, dnan, dnan, dinf, dnan};
  double outputsdmin[kTableLength] = {2.0,  2.0,   -0.0,  -0.0, 42.0,
                                      42.0, dminf, dminf, dnan, dnan,
                                      dnan, dnan,  dnan};
  double outputsdmax[kTableLength] = {3.0,  3.0,  0.0,  0.0,  dinf, dinf, dinf,
                                      dinf, dnan, dnan, dnan, dnan, dnan};

  float inputse[kTableLength] = {2.0,  3.0,  -0.0, 0.0,  42.0, finf, fminf,
                                 finf, fnan, 3.0,  finf, fnan, fnan};
  float inputsf[kTableLength] = {3.0,   2.0, 0.0,  -0.0, finf, 42.0, finf,
                                 fminf, 3.0, fnan, fnan, finf, fnan};
  float outputsfmin[kTableLength] = {2.0,   2.0,  -0.0, -0.0, 42.0, 42.0, fminf,
                                     fminf, fnan, fnan, fnan, fnan, fnan};
  float outputsfmax[kTableLength] = {3.0,  3.0,  0.0,  0.0,  finf, finf, finf,
                                     finf, fnan, fnan, fnan, fnan, fnan};

  auto handle_dnan = [masm](FPURegister dst, Label* nan, Label* back) {
    __ bind(nan);
    __ LoadRoot(t8, Heap::kNanValueRootIndex);
    __ Ldc1(dst, FieldMemOperand(t8, HeapNumber::kValueOffset));
    __ Branch(back);
  };

  auto handle_snan = [masm, fnan](FPURegister dst, Label* nan, Label* back) {
    __ bind(nan);
    __ Move(dst, fnan);
    __ Branch(back);
  };

  Label handle_mind_nan, handle_maxd_nan, handle_mins_nan, handle_maxs_nan;
  Label back_mind_nan, back_maxd_nan, back_mins_nan, back_maxs_nan;

  __ push(s6);
  __ InitializeRootRegister();
  __ Ldc1(f4, MemOperand(a0, offsetof(TestFloat, a)));
  __ Ldc1(f8, MemOperand(a0, offsetof(TestFloat, b)));
  __ Lwc1(f2, MemOperand(a0, offsetof(TestFloat, e)));
  __ Lwc1(f6, MemOperand(a0, offsetof(TestFloat, f)));
  __ Float64Min(f10, f4, f8, &handle_mind_nan);
  __ bind(&back_mind_nan);
  __ Float64Max(f12, f4, f8, &handle_maxd_nan);
  __ bind(&back_maxd_nan);
  __ Float32Min(f14, f2, f6, &handle_mins_nan);
  __ bind(&back_mins_nan);
  __ Float32Max(f16, f2, f6, &handle_maxs_nan);
  __ bind(&back_maxs_nan);
  __ Sdc1(f10, MemOperand(a0, offsetof(TestFloat, c)));
  __ Sdc1(f12, MemOperand(a0, offsetof(TestFloat, d)));
  __ Swc1(f14, MemOperand(a0, offsetof(TestFloat, g)));
  __ Swc1(f16, MemOperand(a0, offsetof(TestFloat, h)));
  __ pop(s6);
  __ jr(ra);
  __ nop();

  handle_dnan(f10, &handle_mind_nan, &back_mind_nan);
  handle_dnan(f12, &handle_maxd_nan, &back_maxd_nan);
  handle_snan(f14, &handle_mins_nan, &back_mins_nan);
  handle_snan(f16, &handle_maxs_nan, &back_maxs_nan);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
  auto f = GeneratedCode<F3>::FromCode(*code);
  for (int i = 0; i < kTableLength; i++) {
    test.a = inputsa[i];
    test.b = inputsb[i];
    test.e = inputse[i];
    test.f = inputsf[i];

    f.Call(&test, 0, 0, 0, 0);

    CHECK_EQ(0, memcmp(&test.c, &outputsdmin[i], sizeof(test.c)));
    CHECK_EQ(0, memcmp(&test.d, &outputsdmax[i], sizeof(test.d)));
    CHECK_EQ(0, memcmp(&test.g, &outputsfmin[i], sizeof(test.g)));
    CHECK_EQ(0, memcmp(&test.h, &outputsfmax[i], sizeof(test.h)));
  }
}

template <typename IN_TYPE, typename Func>
bool run_Unaligned(char* memory_buffer, int32_t in_offset, int32_t out_offset,
                   IN_TYPE value, Func GenerateUnalignedInstructionFunc) {
  typedef int32_t(F_CVT)(char* x0, int x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;
  IN_TYPE res;

  GenerateUnalignedInstructionFunc(masm, in_offset, out_offset);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<F_CVT>::FromCode(*code);

  MemCopy(memory_buffer + in_offset, &value, sizeof(IN_TYPE));
  f.Call(memory_buffer, 0, 0, 0, 0);
  MemCopy(&res, memory_buffer + out_offset, sizeof(IN_TYPE));

  return res == value;
}

static const std::vector<uint64_t> unsigned_test_values() {
  static const uint64_t kValues[] = {
      0x2180F18A06384414, 0x000A714532102277, 0xBC1ACCCF180649F0,
      0x8000000080008000, 0x0000000000000001, 0xFFFFFFFFFFFFFFFF,
  };
  return std::vector<uint64_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

static const std::vector<int32_t> unsigned_test_offset() {
  static const int32_t kValues[] = {// value, offset
                                    -132 * KB, -21 * KB, 0, 19 * KB, 135 * KB};
  return std::vector<int32_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

static const std::vector<int32_t> unsigned_test_offset_increment() {
  static const int32_t kValues[] = {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5};
  return std::vector<int32_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

TEST(Ulh) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        uint16_t value = static_cast<uint64_t>(*i & 0xFFFF);
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn_1 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ Ulh(v0, MemOperand(a0, in_offset));
          __ Ush(v0, MemOperand(a0, out_offset), v0);
        };
        CHECK_EQ(true, run_Unaligned<uint16_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_1));

        auto fn_2 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ mov(t0, a0);
          __ Ulh(a0, MemOperand(a0, in_offset));
          __ Ush(a0, MemOperand(t0, out_offset), v0);
        };
        CHECK_EQ(true, run_Unaligned<uint16_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_2));

        auto fn_3 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ mov(t0, a0);
          __ Ulhu(a0, MemOperand(a0, in_offset));
          __ Ush(a0, MemOperand(t0, out_offset), t1);
        };
        CHECK_EQ(true, run_Unaligned<uint16_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_3));

        auto fn_4 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ Ulhu(v0, MemOperand(a0, in_offset));
          __ Ush(v0, MemOperand(a0, out_offset), t1);
        };
        CHECK_EQ(true, run_Unaligned<uint16_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_4));
      }
    }
  }
}

TEST(Ulh_bitextension) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        uint16_t value = static_cast<uint64_t>(*i & 0xFFFF);
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn = [](MacroAssembler* masm, int32_t in_offset,
                     int32_t out_offset) {
          Label success, fail, end, different;
          __ Ulh(t0, MemOperand(a0, in_offset));
          __ Ulhu(t1, MemOperand(a0, in_offset));
          __ Branch(&different, ne, t0, Operand(t1));

          // If signed and unsigned values are same, check
          // the upper bits to see if they are zero
          __ sra(t0, t0, 15);
          __ Branch(&success, eq, t0, Operand(zero_reg));
          __ Branch(&fail);

          // If signed and unsigned values are different,
          // check that the upper bits are complementary
          __ bind(&different);
          __ sra(t1, t1, 15);
          __ Branch(&fail, ne, t1, Operand(1));
          __ sra(t0, t0, 15);
          __ addiu(t0, t0, 1);
          __ Branch(&fail, ne, t0, Operand(zero_reg));
          // Fall through to success

          __ bind(&success);
          __ Ulh(t0, MemOperand(a0, in_offset));
          __ Ush(t0, MemOperand(a0, out_offset), v0);
          __ Branch(&end);
          __ bind(&fail);
          __ Ush(zero_reg, MemOperand(a0, out_offset), v0);
          __ bind(&end);
        };
        CHECK_EQ(true, run_Unaligned<uint16_t>(buffer_middle, in_offset,
                                               out_offset, value, fn));
      }
    }
  }
}

TEST(Ulw) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        uint32_t value = static_cast<uint32_t>(*i & 0xFFFFFFFF);
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn_1 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ Ulw(v0, MemOperand(a0, in_offset));
          __ Usw(v0, MemOperand(a0, out_offset));
        };
        CHECK_EQ(true, run_Unaligned<uint32_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_1));

        auto fn_2 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ mov(t0, a0);
          __ Ulw(a0, MemOperand(a0, in_offset));
          __ Usw(a0, MemOperand(t0, out_offset));
        };
        CHECK_EQ(true,
                 run_Unaligned<uint32_t>(buffer_middle, in_offset, out_offset,
                                         (uint32_t)value, fn_2));

        auto fn_3 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ Ulwu(v0, MemOperand(a0, in_offset));
          __ Usw(v0, MemOperand(a0, out_offset));
        };
        CHECK_EQ(true, run_Unaligned<uint32_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_3));

        auto fn_4 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ mov(t0, a0);
          __ Ulwu(a0, MemOperand(a0, in_offset));
          __ Usw(a0, MemOperand(t0, out_offset));
        };
        CHECK_EQ(true,
                 run_Unaligned<uint32_t>(buffer_middle, in_offset, out_offset,
                                         (uint32_t)value, fn_4));
      }
    }
  }
}

TEST(Ulw_extension) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        uint32_t value = static_cast<uint32_t>(*i & 0xFFFFFFFF);
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn = [](MacroAssembler* masm, int32_t in_offset,
                     int32_t out_offset) {
          Label success, fail, end, different;
          __ Ulw(t0, MemOperand(a0, in_offset));
          __ Ulwu(t1, MemOperand(a0, in_offset));
          __ Branch(&different, ne, t0, Operand(t1));

          // If signed and unsigned values are same, check
          // the upper bits to see if they are zero
          __ dsra(t0, t0, 31);
          __ Branch(&success, eq, t0, Operand(zero_reg));
          __ Branch(&fail);

          // If signed and unsigned values are different,
          // check that the upper bits are complementary
          __ bind(&different);
          __ dsra(t1, t1, 31);
          __ Branch(&fail, ne, t1, Operand(1));
          __ dsra(t0, t0, 31);
          __ daddiu(t0, t0, 1);
          __ Branch(&fail, ne, t0, Operand(zero_reg));
          // Fall through to success

          __ bind(&success);
          __ Ulw(t0, MemOperand(a0, in_offset));
          __ Usw(t0, MemOperand(a0, out_offset));
          __ Branch(&end);
          __ bind(&fail);
          __ Usw(zero_reg, MemOperand(a0, out_offset));
          __ bind(&end);
        };
        CHECK_EQ(true, run_Unaligned<uint32_t>(buffer_middle, in_offset,
                                               out_offset, value, fn));
      }
    }
  }
}

TEST(Uld) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        uint64_t value = *i;
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn_1 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ Uld(v0, MemOperand(a0, in_offset));
          __ Usd(v0, MemOperand(a0, out_offset));
        };
        CHECK_EQ(true, run_Unaligned<uint64_t>(buffer_middle, in_offset,
                                               out_offset, value, fn_1));

        auto fn_2 = [](MacroAssembler* masm, int32_t in_offset,
                       int32_t out_offset) {
          __ mov(t0, a0);
          __ Uld(a0, MemOperand(a0, in_offset));
          __ Usd(a0, MemOperand(t0, out_offset));
        };
        CHECK_EQ(true,
                 run_Unaligned<uint64_t>(buffer_middle, in_offset, out_offset,
                                         (uint32_t)value, fn_2));
      }
    }
  }
}

TEST(Ulwc1) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        float value = static_cast<float>(*i & 0xFFFFFFFF);
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn = [](MacroAssembler* masm, int32_t in_offset,
                     int32_t out_offset) {
          __ Ulwc1(f0, MemOperand(a0, in_offset), t0);
          __ Uswc1(f0, MemOperand(a0, out_offset), t0);
        };
        CHECK_EQ(true, run_Unaligned<float>(buffer_middle, in_offset,
                                            out_offset, value, fn));
      }
    }
  }
}

TEST(Uldc1) {
  CcTest::InitializeVM();

  static const int kBufferSize = 300 * KB;
  char memory_buffer[kBufferSize];
  char* buffer_middle = memory_buffer + (kBufferSize / 2);

  FOR_UINT64_INPUTS(i, unsigned_test_values) {
    FOR_INT32_INPUTS2(j1, j2, unsigned_test_offset) {
      FOR_INT32_INPUTS2(k1, k2, unsigned_test_offset_increment) {
        double value = static_cast<double>(*i);
        int32_t in_offset = *j1 + *k1;
        int32_t out_offset = *j2 + *k2;

        auto fn = [](MacroAssembler* masm, int32_t in_offset,
                     int32_t out_offset) {
          __ Uldc1(f0, MemOperand(a0, in_offset), t0);
          __ Usdc1(f0, MemOperand(a0, out_offset), t0);
        };
        CHECK_EQ(true, run_Unaligned<double>(buffer_middle, in_offset,
                                             out_offset, value, fn));
      }
    }
  }
}

static const std::vector<uint64_t> sltu_test_values() {
  static const uint64_t kValues[] = {
      0,
      1,
      0x7FFE,
      0x7FFF,
      0x8000,
      0x8001,
      0xFFFE,
      0xFFFF,
      0xFFFFFFFFFFFF7FFE,
      0xFFFFFFFFFFFF7FFF,
      0xFFFFFFFFFFFF8000,
      0xFFFFFFFFFFFF8001,
      0xFFFFFFFFFFFFFFFE,
      0xFFFFFFFFFFFFFFFF,
  };
  return std::vector<uint64_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

template <typename Func>
bool run_Sltu(uint64_t rs, uint64_t rd, Func GenerateSltuInstructionFunc) {
  typedef int64_t(F_CVT)(uint64_t x0, uint64_t x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;

  GenerateSltuInstructionFunc(masm, rd);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  auto f = GeneratedCode<F_CVT>::FromCode(*code);
  int64_t res = reinterpret_cast<int64_t>(f.Call(rs, rd, 0, 0, 0));
  return res == 1;
}

TEST(Sltu) {
  CcTest::InitializeVM();

  FOR_UINT64_INPUTS(i, sltu_test_values) {
    FOR_UINT64_INPUTS(j, sltu_test_values) {
      uint64_t rs = *i;
      uint64_t rd = *j;

      auto fn_1 = [](MacroAssembler* masm, uint64_t imm) {
        __ Sltu(v0, a0, Operand(imm));
      };
      CHECK_EQ(rs < rd, run_Sltu(rs, rd, fn_1));

      auto fn_2 = [](MacroAssembler* masm, uint64_t imm) {
        __ Sltu(v0, a0, a1);
      };
      CHECK_EQ(rs < rd, run_Sltu(rs, rd, fn_2));
    }
  }
}

template <typename T, typename Inputs, typename Results>
static GeneratedCode<F4> GenerateMacroFloat32MinMax(MacroAssembler* masm) {
  T a = T::from_code(4);  // f4
  T b = T::from_code(6);  // f6
  T c = T::from_code(8);  // f8

  Label ool_min_abc, ool_min_aab, ool_min_aba;
  Label ool_max_abc, ool_max_aab, ool_max_aba;

  Label done_min_abc, done_min_aab, done_min_aba;
  Label done_max_abc, done_max_aab, done_max_aba;

#define FLOAT_MIN_MAX(fminmax, res, x, y, done, ool, res_field) \
  __ Lwc1(x, MemOperand(a0, offsetof(Inputs, src1_)));          \
  __ Lwc1(y, MemOperand(a0, offsetof(Inputs, src2_)));          \
  __ fminmax(res, x, y, &ool);                                  \
  __ bind(&done);                                               \
  __ Swc1(a, MemOperand(a1, offsetof(Results, res_field)))

  // a = min(b, c);
  FLOAT_MIN_MAX(Float32Min, a, b, c, done_min_abc, ool_min_abc, min_abc_);
  // a = min(a, b);
  FLOAT_MIN_MAX(Float32Min, a, a, b, done_min_aab, ool_min_aab, min_aab_);
  // a = min(b, a);
  FLOAT_MIN_MAX(Float32Min, a, b, a, done_min_aba, ool_min_aba, min_aba_);

  // a = max(b, c);
  FLOAT_MIN_MAX(Float32Max, a, b, c, done_max_abc, ool_max_abc, max_abc_);
  // a = max(a, b);
  FLOAT_MIN_MAX(Float32Max, a, a, b, done_max_aab, ool_max_aab, max_aab_);
  // a = max(b, a);
  FLOAT_MIN_MAX(Float32Max, a, b, a, done_max_aba, ool_max_aba, max_aba_);

#undef FLOAT_MIN_MAX

  __ jr(ra);
  __ nop();

  // Generate out-of-line cases.
  __ bind(&ool_min_abc);
  __ Float32MinOutOfLine(a, b, c);
  __ Branch(&done_min_abc);

  __ bind(&ool_min_aab);
  __ Float32MinOutOfLine(a, a, b);
  __ Branch(&done_min_aab);

  __ bind(&ool_min_aba);
  __ Float32MinOutOfLine(a, b, a);
  __ Branch(&done_min_aba);

  __ bind(&ool_max_abc);
  __ Float32MaxOutOfLine(a, b, c);
  __ Branch(&done_max_abc);

  __ bind(&ool_max_aab);
  __ Float32MaxOutOfLine(a, a, b);
  __ Branch(&done_max_aab);

  __ bind(&ool_max_aba);
  __ Float32MaxOutOfLine(a, b, a);
  __ Branch(&done_max_aba);

  CodeDesc desc;
  masm->GetCode(masm->isolate(), &desc);
  Handle<Code> code =
      masm->isolate()->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  return GeneratedCode<F4>::FromCode(*code);
}

TEST(macro_float_minmax_f32) {
  // Test the Float32Min and Float32Max macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  struct Inputs {
    float src1_;
    float src2_;
  };

  struct Results {
    // Check all register aliasing possibilities in order to exercise all
    // code-paths in the macro assembler.
    float min_abc_;
    float min_aab_;
    float min_aba_;
    float max_abc_;
    float max_aab_;
    float max_aba_;
  };

  GeneratedCode<F4> f =
      GenerateMacroFloat32MinMax<FPURegister, Inputs, Results>(masm);

#define CHECK_MINMAX(src1, src2, min, max)                                   \
  do {                                                                       \
    Inputs inputs = {src1, src2};                                            \
    Results results;                                                         \
    f.Call(&inputs, &results, 0, 0, 0);                                      \
    CHECK_EQ(bit_cast<uint32_t>(min), bit_cast<uint32_t>(results.min_abc_)); \
    CHECK_EQ(bit_cast<uint32_t>(min), bit_cast<uint32_t>(results.min_aab_)); \
    CHECK_EQ(bit_cast<uint32_t>(min), bit_cast<uint32_t>(results.min_aba_)); \
    CHECK_EQ(bit_cast<uint32_t>(max), bit_cast<uint32_t>(results.max_abc_)); \
    CHECK_EQ(bit_cast<uint32_t>(max), bit_cast<uint32_t>(results.max_aab_)); \
    CHECK_EQ(bit_cast<uint32_t>(max), bit_cast<uint32_t>(results.max_aba_)); \
    /* Use a bit_cast to correctly identify -0.0 and NaNs. */                \
  } while (0)

  float nan_a = std::numeric_limits<float>::quiet_NaN();
  float nan_b = std::numeric_limits<float>::quiet_NaN();

  CHECK_MINMAX(1.0f, -1.0f, -1.0f, 1.0f);
  CHECK_MINMAX(-1.0f, 1.0f, -1.0f, 1.0f);
  CHECK_MINMAX(0.0f, -1.0f, -1.0f, 0.0f);
  CHECK_MINMAX(-1.0f, 0.0f, -1.0f, 0.0f);
  CHECK_MINMAX(-0.0f, -1.0f, -1.0f, -0.0f);
  CHECK_MINMAX(-1.0f, -0.0f, -1.0f, -0.0f);
  CHECK_MINMAX(0.0f, 1.0f, 0.0f, 1.0f);
  CHECK_MINMAX(1.0f, 0.0f, 0.0f, 1.0f);

  CHECK_MINMAX(0.0f, 0.0f, 0.0f, 0.0f);
  CHECK_MINMAX(-0.0f, -0.0f, -0.0f, -0.0f);
  CHECK_MINMAX(-0.0f, 0.0f, -0.0f, 0.0f);
  CHECK_MINMAX(0.0f, -0.0f, -0.0f, 0.0f);

  CHECK_MINMAX(0.0f, nan_a, nan_a, nan_a);
  CHECK_MINMAX(nan_a, 0.0f, nan_a, nan_a);
  CHECK_MINMAX(nan_a, nan_b, nan_a, nan_a);
  CHECK_MINMAX(nan_b, nan_a, nan_b, nan_b);

#undef CHECK_MINMAX
}

template <typename T, typename Inputs, typename Results>
static GeneratedCode<F4> GenerateMacroFloat64MinMax(MacroAssembler* masm) {
  T a = T::from_code(4);  // f4
  T b = T::from_code(6);  // f6
  T c = T::from_code(8);  // f8

  Label ool_min_abc, ool_min_aab, ool_min_aba;
  Label ool_max_abc, ool_max_aab, ool_max_aba;

  Label done_min_abc, done_min_aab, done_min_aba;
  Label done_max_abc, done_max_aab, done_max_aba;

#define FLOAT_MIN_MAX(fminmax, res, x, y, done, ool, res_field) \
  __ Ldc1(x, MemOperand(a0, offsetof(Inputs, src1_)));          \
  __ Ldc1(y, MemOperand(a0, offsetof(Inputs, src2_)));          \
  __ fminmax(res, x, y, &ool);                                  \
  __ bind(&done);                                               \
  __ Sdc1(a, MemOperand(a1, offsetof(Results, res_field)))

  // a = min(b, c);
  FLOAT_MIN_MAX(Float64Min, a, b, c, done_min_abc, ool_min_abc, min_abc_);
  // a = min(a, b);
  FLOAT_MIN_MAX(Float64Min, a, a, b, done_min_aab, ool_min_aab, min_aab_);
  // a = min(b, a);
  FLOAT_MIN_MAX(Float64Min, a, b, a, done_min_aba, ool_min_aba, min_aba_);

  // a = max(b, c);
  FLOAT_MIN_MAX(Float64Max, a, b, c, done_max_abc, ool_max_abc, max_abc_);
  // a = max(a, b);
  FLOAT_MIN_MAX(Float64Max, a, a, b, done_max_aab, ool_max_aab, max_aab_);
  // a = max(b, a);
  FLOAT_MIN_MAX(Float64Max, a, b, a, done_max_aba, ool_max_aba, max_aba_);

#undef FLOAT_MIN_MAX

  __ jr(ra);
  __ nop();

  // Generate out-of-line cases.
  __ bind(&ool_min_abc);
  __ Float64MinOutOfLine(a, b, c);
  __ Branch(&done_min_abc);

  __ bind(&ool_min_aab);
  __ Float64MinOutOfLine(a, a, b);
  __ Branch(&done_min_aab);

  __ bind(&ool_min_aba);
  __ Float64MinOutOfLine(a, b, a);
  __ Branch(&done_min_aba);

  __ bind(&ool_max_abc);
  __ Float64MaxOutOfLine(a, b, c);
  __ Branch(&done_max_abc);

  __ bind(&ool_max_aab);
  __ Float64MaxOutOfLine(a, a, b);
  __ Branch(&done_max_aab);

  __ bind(&ool_max_aba);
  __ Float64MaxOutOfLine(a, b, a);
  __ Branch(&done_max_aba);

  CodeDesc desc;
  masm->GetCode(masm->isolate(), &desc);
  Handle<Code> code =
      masm->isolate()->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  return GeneratedCode<F4>::FromCode(*code);
}

TEST(macro_float_minmax_f64) {
  // Test the Float64Min and Float64Max macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  struct Inputs {
    double src1_;
    double src2_;
  };

  struct Results {
    // Check all register aliasing possibilities in order to exercise all
    // code-paths in the macro assembler.
    double min_abc_;
    double min_aab_;
    double min_aba_;
    double max_abc_;
    double max_aab_;
    double max_aba_;
  };

  GeneratedCode<F4> f =
      GenerateMacroFloat64MinMax<DoubleRegister, Inputs, Results>(masm);

#define CHECK_MINMAX(src1, src2, min, max)                                   \
  do {                                                                       \
    Inputs inputs = {src1, src2};                                            \
    Results results;                                                         \
    f.Call(&inputs, &results, 0, 0, 0);                                      \
    CHECK_EQ(bit_cast<uint64_t>(min), bit_cast<uint64_t>(results.min_abc_)); \
    CHECK_EQ(bit_cast<uint64_t>(min), bit_cast<uint64_t>(results.min_aab_)); \
    CHECK_EQ(bit_cast<uint64_t>(min), bit_cast<uint64_t>(results.min_aba_)); \
    CHECK_EQ(bit_cast<uint64_t>(max), bit_cast<uint64_t>(results.max_abc_)); \
    CHECK_EQ(bit_cast<uint64_t>(max), bit_cast<uint64_t>(results.max_aab_)); \
    CHECK_EQ(bit_cast<uint64_t>(max), bit_cast<uint64_t>(results.max_aba_)); \
    /* Use a bit_cast to correctly identify -0.0 and NaNs. */                \
  } while (0)

  double nan_a = std::numeric_limits<double>::quiet_NaN();
  double nan_b = std::numeric_limits<double>::quiet_NaN();

  CHECK_MINMAX(1.0, -1.0, -1.0, 1.0);
  CHECK_MINMAX(-1.0, 1.0, -1.0, 1.0);
  CHECK_MINMAX(0.0, -1.0, -1.0, 0.0);
  CHECK_MINMAX(-1.0, 0.0, -1.0, 0.0);
  CHECK_MINMAX(-0.0, -1.0, -1.0, -0.0);
  CHECK_MINMAX(-1.0, -0.0, -1.0, -0.0);
  CHECK_MINMAX(0.0, 1.0, 0.0, 1.0);
  CHECK_MINMAX(1.0, 0.0, 0.0, 1.0);

  CHECK_MINMAX(0.0, 0.0, 0.0, 0.0);
  CHECK_MINMAX(-0.0, -0.0, -0.0, -0.0);
  CHECK_MINMAX(-0.0, 0.0, -0.0, 0.0);
  CHECK_MINMAX(0.0, -0.0, -0.0, 0.0);

  CHECK_MINMAX(0.0, nan_a, nan_a, nan_a);
  CHECK_MINMAX(nan_a, 0.0, nan_a, nan_a);
  CHECK_MINMAX(nan_a, nan_b, nan_a, nan_a);
  CHECK_MINMAX(nan_b, nan_a, nan_b, nan_b);

#undef CHECK_MINMAX
}

#undef __

}  // namespace internal
}  // namespace v8
