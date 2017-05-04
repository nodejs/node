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

#include "src/base/utils/random-number-generator.h"
#include "src/macro-assembler.h"
#include "src/mips/macro-assembler-mips.h"
#include "src/mips/simulator-mips.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"


using namespace v8::internal;

typedef void* (*F)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F3)(void* p, int p1, int p2, int p3, int p4);
typedef Object* (*F4)(void* p0, void* p1, int p2, int p3, int p4);

#define __ masm->

TEST(BYTESWAP) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  struct T {
    int32_t r1;
    int32_t r2;
    int32_t r3;
    int32_t r4;
    int32_t r5;
  };
  T t;

  MacroAssembler assembler(isolate, NULL, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  __ lw(a2, MemOperand(a0, offsetof(T, r1)));
  __ nop();
  __ ByteSwapSigned(a2, a2, 4);
  __ sw(a2, MemOperand(a0, offsetof(T, r1)));

  __ lw(a2, MemOperand(a0, offsetof(T, r2)));
  __ nop();
  __ ByteSwapSigned(a2, a2, 2);
  __ sw(a2, MemOperand(a0, offsetof(T, r2)));

  __ lw(a2, MemOperand(a0, offsetof(T, r3)));
  __ nop();
  __ ByteSwapSigned(a2, a2, 1);
  __ sw(a2, MemOperand(a0, offsetof(T, r3)));

  __ lw(a2, MemOperand(a0, offsetof(T, r4)));
  __ nop();
  __ ByteSwapUnsigned(a2, a2, 1);
  __ sw(a2, MemOperand(a0, offsetof(T, r4)));

  __ lw(a2, MemOperand(a0, offsetof(T, r5)));
  __ nop();
  __ ByteSwapUnsigned(a2, a2, 2);
  __ sw(a2, MemOperand(a0, offsetof(T, r5)));

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  masm->GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  ::F3 f = FUNCTION_CAST<::F3>(code->entry());
  t.r1 = 0x781A15C3;
  t.r2 = 0x2CDE;
  t.r3 = 0x9F;
  t.r4 = 0x9F;
  t.r5 = 0x2CDE;
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(static_cast<int32_t>(0xC3151A78), t.r1);
  CHECK_EQ(static_cast<int32_t>(0xDE2C0000), t.r2);
  CHECK_EQ(static_cast<int32_t>(0x9FFFFFFF), t.r3);
  CHECK_EQ(static_cast<int32_t>(0x9F000000), t.r4);
  CHECK_EQ(static_cast<int32_t>(0xDE2C0000), t.r5);
}

static void TestNaN(const char *code) {
  // NaN value is different on MIPS and x86 architectures, and TEST(NaNx)
  // tests checks the case where a x86 NaN value is serialized into the
  // snapshot on the simulator during cross compilation.
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext(PRINT_EXTENSION);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Script> script =
      v8::Script::Compile(context, v8_str(code)).ToLocalChecked();
  v8::Local<v8::Object> result =
      v8::Local<v8::Object>::Cast(script->Run(context).ToLocalChecked());
  i::Handle<i::JSReceiver> o = v8::Utils::OpenHandle(*result);
  i::Handle<i::JSArray> array1(reinterpret_cast<i::JSArray*>(*o));
  i::FixedDoubleArray* a = i::FixedDoubleArray::cast(array1->elements());
  double value = a->get_scalar(0);
  CHECK(std::isnan(value) &&
        bit_cast<uint64_t>(value) ==
            bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN()));
}


TEST(NaN0) {
  TestNaN(
          "var result;"
          "for (var i = 0; i < 2; i++) {"
          "  result = new Array(Number.NaN, Number.POSITIVE_INFINITY);"
          "}"
          "result;");
}


TEST(NaN1) {
  TestNaN(
          "var result;"
          "for (var i = 0; i < 2; i++) {"
          "  result = [NaN];"
          "}"
          "result;");
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
  masm->GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  for (int i = 0; i < kNumCases; ++i) {
    int res =
        reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, i, 0, 0, 0, 0));
    ::printf("f(%d) = %d\n", i, res);
    CHECK_EQ(values[i], res);
  }
}


TEST(jump_tables5) {
  if (!IsMipsArchVariant(kMips32r6)) return;

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

  {
    __ BlockTrampolinePoolFor(kNumCases + 6 + 1);
    PredictableCodeSizeScope predictable(
        masm, kNumCases * kPointerSize + ((6 + 1) * Assembler::kInstrSize));

    __ addiupc(at, 6 + 1);
    __ Lsa(at, at, a0, 2);
    __ lw(at, MemOperand(at));
    __ jalr(at);
    __ nop();  // Branch delay slot nop.
    __ bc(&done);
    // A nop instruction must be generated by the forbidden slot guard
    // (Assembler::dd(Label*)).
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
  masm->GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  for (int i = 0; i < kNumCases; ++i) {
    int32_t res = reinterpret_cast<int32_t>(
        CALL_GENERATED_CODE(isolate, f, i, 0, 0, 0, 0));
    ::printf("f(%d) = %d\n", i, res);
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
                         (kSwitchTablePrologueSize + kSwitchTableCases) - 20;

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
  gen_insn += (kSwitchTablePrologueSize + kSwitchTableCases);

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
  masm->GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  for (int i = 0; i < kSwitchTableCases; ++i) {
    int res =
        reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, i, 0, 0, 0, 0));
    ::printf("f(%d) = %d\n", i, res);
    CHECK_EQ(values[i], res);
  }
}

static uint32_t run_lsa(uint32_t rt, uint32_t rs, int8_t sa) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assembler(isolate, nullptr, 0,
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;

  __ Lsa(v0, a0, a1, sa);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assembler.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  F1 f = FUNCTION_CAST<F1>(code->entry());

  uint32_t res = reinterpret_cast<uint32_t>(
      CALL_GENERATED_CODE(isolate, f, rt, rs, 0, 0, 0));

  return res;
}


TEST(Lsa) {
  CcTest::InitializeVM();
  struct TestCaseLsa {
    int32_t rt;
    int32_t rs;
    uint8_t sa;
    uint32_t expected_res;
  };

  struct TestCaseLsa tc[] = {// rt, rs, sa, expected_res
                             {0x4, 0x1, 1, 0x6},
                             {0x4, 0x1, 2, 0x8},
                             {0x4, 0x1, 3, 0xc},
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
                             {0x4, INT32_MAX >> 2, 3, 0xfffffffc},
                             {0x4, INT32_MAX >> 3, 4, 0xfffffff4},
                             {0x4, INT32_MAX >> 4, 5, 0xffffffe4},

                             // Signed addition overflow.
                             {INT32_MAX - 1, 0x1, 1, 0x80000000},
                             {INT32_MAX - 3, 0x1, 2, 0x80000000},
                             {INT32_MAX - 7, 0x1, 3, 0x80000000},
                             {INT32_MAX - 15, 0x1, 4, 0x80000000},
                             {INT32_MAX - 31, 0x1, 5, 0x80000000},

                             // Addition overflow.
                             {-2, 0x1, 1, 0x0},
                             {-4, 0x1, 2, 0x0},
                             {-8, 0x1, 3, 0x0},
                             {-16, 0x1, 4, 0x0},
                             {-32, 0x1, 5, 0x0}};

  size_t nr_test_cases = sizeof(tc) / sizeof(TestCaseLsa);
  for (size_t i = 0; i < nr_test_cases; ++i) {
    uint32_t res = run_lsa(tc[i].rt, tc[i].rs, tc[i].sa);
    PrintF("0x%x =? 0x%x == lsa(v0, %x, %x, %hhu)\n", tc[i].expected_res, res,
           tc[i].rt, tc[i].rs, tc[i].sa);
    CHECK_EQ(tc[i].expected_res, res);
  }
}

static const std::vector<uint32_t> cvt_trunc_uint32_test_values() {
  static const uint32_t kValues[] = {0x00000000, 0x00000001, 0x00ffff00,
                                     0x7fffffff, 0x80000000, 0x80000001,
                                     0x80ffff00, 0x8fffffff, 0xffffffff};
  return std::vector<uint32_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

static const std::vector<int32_t> cvt_trunc_int32_test_values() {
  static const int32_t kValues[] = {
      static_cast<int32_t>(0x00000000), static_cast<int32_t>(0x00000001),
      static_cast<int32_t>(0x00ffff00), static_cast<int32_t>(0x7fffffff),
      static_cast<int32_t>(0x80000000), static_cast<int32_t>(0x80000001),
      static_cast<int32_t>(0x80ffff00), static_cast<int32_t>(0x8fffffff),
      static_cast<int32_t>(0xffffffff)};
  return std::vector<int32_t>(&kValues[0], &kValues[arraysize(kValues)]);
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
#define FOR_UINT32_INPUTS(var, test_vector) \
  FOR_INPUTS(uint32_t, uint32, var, test_vector)
#define FOR_INT32_INPUTS(var, test_vector) \
  FOR_INPUTS(int32_t, int32, var, test_vector)
#define FOR_INT32_INPUTS2(var, var2, test_vector) \
  FOR_INPUTS2(int32_t, int32, var, var2, test_vector)

#define FOR_UINT64_INPUTS(var, test_vector) \
  FOR_INPUTS(uint64_t, uint32, var, test_vector)

template <typename RET_TYPE, typename IN_TYPE, typename Func>
RET_TYPE run_Cvt(IN_TYPE x, Func GenerateConvertInstructionFunc) {
  typedef RET_TYPE (*F_CVT)(IN_TYPE x0, int x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;

  __ mtc1(a0, f4);
  GenerateConvertInstructionFunc(masm);
  __ mfc1(v0, f2);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  F_CVT f = FUNCTION_CAST<F_CVT>(code->entry());

  return reinterpret_cast<RET_TYPE>(
      CALL_GENERATED_CODE(isolate, f, x, 0, 0, 0, 0));
}

TEST(cvt_s_w_Trunc_uw_s) {
  CcTest::InitializeVM();
  FOR_UINT32_INPUTS(i, cvt_trunc_uint32_test_values) {
    uint32_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ cvt_s_w(f0, f4);
      __ Trunc_uw_s(f2, f0, f6);
    };
    CHECK_EQ(static_cast<float>(input), run_Cvt<uint32_t>(input, fn));
  }
}

TEST(cvt_d_w_Trunc_w_d) {
  CcTest::InitializeVM();
  FOR_INT32_INPUTS(i, cvt_trunc_int32_test_values) {
    int32_t input = *i;
    auto fn = [](MacroAssembler* masm) {
      __ cvt_d_w(f0, f4);
      __ Trunc_w_d(f2, f0);
    };
    CHECK_EQ(static_cast<double>(input), run_Cvt<int32_t>(input, fn));
  }
}

static const std::vector<int32_t> overflow_int32_test_values() {
  static const int32_t kValues[] = {
      static_cast<int32_t>(0xf0000000), static_cast<int32_t>(0x00000001),
      static_cast<int32_t>(0xff000000), static_cast<int32_t>(0x0000f000),
      static_cast<int32_t>(0x0f000000), static_cast<int32_t>(0x991234ab),
      static_cast<int32_t>(0xb0ffff01), static_cast<int32_t>(0x00006fff),
      static_cast<int32_t>(0xffffffff)};
  return std::vector<int32_t>(&kValues[0], &kValues[arraysize(kValues)]);
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
  typedef int32_t (*F_CVT)(char* x0, int x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;

  GenerateOverflowInstructions(masm, valLeft, valRight);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  F_CVT f = FUNCTION_CAST<F_CVT>(code->entry());

  int32_t r =
      reinterpret_cast<int32_t>(CALL_GENERATED_CODE(isolate, f, 0, 0, 0, 0, 0));

  DCHECK(r == 0 || r == 1);
  return r;
}

TEST(BranchOverflowInt32BothLabelsTrampoline) {
  if (!IsMipsArchVariant(kMips32r6)) return;
  static const int kMaxBranchOffset = (1 << (18 - 1)) - 1;

  FOR_INT32_INPUTS(i, overflow_int32_test_values) {
    FOR_INT32_INPUTS(j, overflow_int32_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int32_t ii = *i;
          int32_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label overflow, no_overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                    &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                    &no_overflow, rc.scratch);
                    break;
                }

                Label done;
                size_t nr_calls =
                    kMaxBranchOffset / (2 * Instruction::kInstrSize) + 2;
                for (size_t i = 0; i < nr_calls; ++i) {
                  __ BranchShort(&done, eq, a0, Operand(a1));
                }
                __ bind(&done);

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
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res1);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res1);
              break;
            default:
              UNREACHABLE();
          }
        }
      }
    }
  }
}

TEST(BranchOverflowInt32BothLabels) {
  FOR_INT32_INPUTS(i, overflow_int32_test_values) {
    FOR_INT32_INPUTS(j, overflow_int32_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int32_t ii = *i;
          int32_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label overflow, no_overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, rc.right, &overflow,
                                    &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, rc.right, &overflow,
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

          bool res2 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label overflow, no_overflow, end;
                __ li(rc.left, valLeft);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, Operand(valRight),
                                    &overflow, &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, Operand(valRight),
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
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res1);
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res2);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res1);
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res2);
              break;
            default:
              UNREACHABLE();
          }
        }
      }
    }
  }
}

TEST(BranchOverflowInt32LeftLabel) {
  FOR_INT32_INPUTS(i, overflow_int32_test_values) {
    FOR_INT32_INPUTS(j, overflow_int32_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int32_t ii = *i;
          int32_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, rc.right, &overflow, NULL,
                                    rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, rc.right, &overflow, NULL,
                                    rc.scratch);
                    break;
                }
                __ li(v0, 0);
                __ Branch(&end);
                __ bind(&overflow);
                __ li(v0, 1);
                __ bind(&end);
              });

          bool res2 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label overflow, end;
                __ li(rc.left, valLeft);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, Operand(valRight),
                                    &overflow, NULL, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, Operand(valRight),
                                    &overflow, NULL, rc.scratch);
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
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res1);
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res2);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res1);
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res2);
              break;
            default:
              UNREACHABLE();
          }
        }
      }
    }
  }
}

TEST(BranchOverflowInt32RightLabel) {
  FOR_INT32_INPUTS(i, overflow_int32_test_values) {
    FOR_INT32_INPUTS(j, overflow_int32_test_values) {
      FOR_ENUM_INPUTS(br, OverflowBranchType, overflow_branch_type) {
        FOR_STRUCT_INPUTS(regComb, OverflowRegisterCombination,
                          overflow_register_combination) {
          int32_t ii = *i;
          int32_t jj = *j;
          enum OverflowBranchType branchType = *br;
          struct OverflowRegisterCombination rc = *regComb;

          // If left and right register are same then left and right
          // test values must also be same, otherwise we skip the test
          if (rc.left.code() == rc.right.code()) {
            if (ii != jj) {
              continue;
            }
          }

          bool res1 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label no_overflow, end;
                __ li(rc.left, valLeft);
                __ li(rc.right, valRight);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, rc.right, NULL,
                                    &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, rc.right, NULL,
                                    &no_overflow, rc.scratch);
                    break;
                }
                __ li(v0, 1);
                __ Branch(&end);
                __ bind(&no_overflow);
                __ li(v0, 0);
                __ bind(&end);
              });

          bool res2 = runOverflow<int32_t>(
              ii, jj, [branchType, rc](MacroAssembler* masm, int32_t valLeft,
                                       int32_t valRight) {
                Label no_overflow, end;
                __ li(rc.left, valLeft);
                switch (branchType) {
                  case kAddBranchOverflow:
                    __ AddBranchOvf(rc.dst, rc.left, Operand(valRight), NULL,
                                    &no_overflow, rc.scratch);
                    break;
                  case kSubBranchOverflow:
                    __ SubBranchOvf(rc.dst, rc.left, Operand(valRight), NULL,
                                    &no_overflow, rc.scratch);
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
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res1);
              CHECK_EQ(IsAddOverflow<int32_t>(ii, jj), res2);
              break;
            case kSubBranchOverflow:
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res1);
              CHECK_EQ(IsSubOverflow<int32_t>(ii, jj), res2);
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
    __ LoadRoot(at, Heap::kNanValueRootIndex);
    __ Ldc1(dst, FieldMemOperand(at, HeapNumber::kValueOffset));
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
  __ lwc1(f2, MemOperand(a0, offsetof(TestFloat, e)));
  __ lwc1(f6, MemOperand(a0, offsetof(TestFloat, f)));
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
  __ swc1(f14, MemOperand(a0, offsetof(TestFloat, g)));
  __ swc1(f16, MemOperand(a0, offsetof(TestFloat, h)));
  __ pop(s6);
  __ jr(ra);
  __ nop();

  handle_dnan(f10, &handle_mind_nan, &back_mind_nan);
  handle_dnan(f12, &handle_maxd_nan, &back_maxd_nan);
  handle_snan(f14, &handle_mins_nan, &back_mins_nan);
  handle_snan(f16, &handle_maxs_nan, &back_maxs_nan);

  CodeDesc desc;
  masm->GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  ::F3 f = FUNCTION_CAST<::F3>(code->entry());
  for (int i = 0; i < kTableLength; i++) {
    test.a = inputsa[i];
    test.b = inputsb[i];
    test.e = inputse[i];
    test.f = inputsf[i];

    CALL_GENERATED_CODE(isolate, f, &test, 0, 0, 0, 0);

    CHECK_EQ(0, memcmp(&test.c, &outputsdmin[i], sizeof(test.c)));
    CHECK_EQ(0, memcmp(&test.d, &outputsdmax[i], sizeof(test.d)));
    CHECK_EQ(0, memcmp(&test.g, &outputsfmin[i], sizeof(test.g)));
    CHECK_EQ(0, memcmp(&test.h, &outputsfmax[i], sizeof(test.h)));
  }
}

template <typename IN_TYPE, typename Func>
bool run_Unaligned(char* memory_buffer, int32_t in_offset, int32_t out_offset,
                   IN_TYPE value, Func GenerateUnalignedInstructionFunc) {
  typedef int32_t (*F_CVT)(char* x0, int x1, int x2, int x3, int x4);

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
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  F_CVT f = FUNCTION_CAST<F_CVT>(code->entry());

  MemCopy(memory_buffer + in_offset, &value, sizeof(IN_TYPE));
  CALL_GENERATED_CODE(isolate, f, memory_buffer, 0, 0, 0, 0);
  MemCopy(&res, memory_buffer + out_offset, sizeof(IN_TYPE));

  return res == value;
}

static const std::vector<uint64_t> unsigned_test_values() {
  static const uint64_t kValues[] = {
      0x2180f18a06384414, 0x000a714532102277, 0xbc1acccf180649f0,
      0x8000000080008000, 0x0000000000000001, 0xffffffffffffffff,
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

static const std::vector<uint32_t> sltu_test_values() {
  static const uint32_t kValues[] = {
      0,          1,          0x7ffe,     0x7fff,     0x8000,
      0x8001,     0xfffe,     0xffff,     0xffff7ffe, 0xffff7fff,
      0xffff8000, 0xffff8001, 0xfffffffe, 0xffffffff,
  };
  return std::vector<uint32_t>(&kValues[0], &kValues[arraysize(kValues)]);
}

template <typename Func>
bool run_Sltu(uint32_t rs, uint32_t rd, Func GenerateSltuInstructionFunc) {
  typedef int32_t (*F_CVT)(uint32_t x0, uint32_t x1, int x2, int x3, int x4);

  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  MacroAssembler assm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assm;

  GenerateSltuInstructionFunc(masm, rd);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  F_CVT f = FUNCTION_CAST<F_CVT>(code->entry());
  int32_t res = reinterpret_cast<int32_t>(
      CALL_GENERATED_CODE(isolate, f, rs, rd, 0, 0, 0));
  return res == 1;
}

TEST(Sltu) {
  CcTest::InitializeVM();

  FOR_UINT32_INPUTS(i, sltu_test_values) {
    FOR_UINT32_INPUTS(j, sltu_test_values) {
      uint32_t rs = *i;
      uint32_t rd = *j;

      auto fn_1 = [](MacroAssembler* masm, uint32_t imm) {
        __ Sltu(v0, a0, Operand(imm));
      };
      CHECK_EQ(rs < rd, run_Sltu(rs, rd, fn_1));

      auto fn_2 = [](MacroAssembler* masm, uint32_t imm) {
        __ Sltu(v0, a0, a1);
      };
      CHECK_EQ(rs < rd, run_Sltu(rs, rd, fn_2));
    }
  }
}

template <typename T, typename Inputs, typename Results>
static ::F4 GenerateMacroFloat32MinMax(MacroAssembler* masm) {
  T a = T::from_code(4);  // f4
  T b = T::from_code(6);  // f6
  T c = T::from_code(8);  // f8

  Label ool_min_abc, ool_min_aab, ool_min_aba;
  Label ool_max_abc, ool_max_aab, ool_max_aba;

  Label done_min_abc, done_min_aab, done_min_aba;
  Label done_max_abc, done_max_aab, done_max_aba;

#define FLOAT_MIN_MAX(fminmax, res, x, y, done, ool, res_field) \
  __ lwc1(x, MemOperand(a0, offsetof(Inputs, src1_)));          \
  __ lwc1(y, MemOperand(a0, offsetof(Inputs, src2_)));          \
  __ fminmax(res, x, y, &ool);                                  \
  __ bind(&done);                                               \
  __ swc1(a, MemOperand(a1, offsetof(Results, res_field)))

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
  masm->GetCode(&desc);
  Handle<Code> code = masm->isolate()->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  return FUNCTION_CAST<::F4>(code->entry());
}

TEST(macro_float_minmax_f32) {
  // Test the Float32Min and Float32Max macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assembler(isolate, NULL, 0,
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

  ::F4 f = GenerateMacroFloat32MinMax<FPURegister, Inputs, Results>(masm);
  Object* dummy = nullptr;
  USE(dummy);

#define CHECK_MINMAX(src1, src2, min, max)                                   \
  do {                                                                       \
    Inputs inputs = {src1, src2};                                            \
    Results results;                                                         \
    dummy = CALL_GENERATED_CODE(isolate, f, &inputs, &results, 0, 0, 0);     \
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
static ::F4 GenerateMacroFloat64MinMax(MacroAssembler* masm) {
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
  masm->GetCode(&desc);
  Handle<Code> code = masm->isolate()->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  return FUNCTION_CAST<::F4>(code->entry());
}

TEST(macro_float_minmax_f64) {
  // Test the Float64Min and Float64Max macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assembler(isolate, NULL, 0,
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

  ::F4 f = GenerateMacroFloat64MinMax<DoubleRegister, Inputs, Results>(masm);
  Object* dummy = nullptr;
  USE(dummy);

#define CHECK_MINMAX(src1, src2, min, max)                                   \
  do {                                                                       \
    Inputs inputs = {src1, src2};                                            \
    Results results;                                                         \
    dummy = CALL_GENERATED_CODE(isolate, f, &inputs, &results, 0, 0, 0);     \
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
