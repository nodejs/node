// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler/bytecode-analysis.h"
#include "src/interpreter/bytecode-array-builder.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/control-flow-builders.h"
#include "src/objects-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class BytecodeAnalysisTest : public TestWithIsolateAndZone {
 public:
  BytecodeAnalysisTest() {}
  ~BytecodeAnalysisTest() override {}

  static void SetUpTestCase() {
    old_FLAG_ignition_peephole_ = i::FLAG_ignition_peephole;
    i::FLAG_ignition_peephole = false;

    old_FLAG_ignition_reo_ = i::FLAG_ignition_reo;
    i::FLAG_ignition_reo = false;

    TestWithIsolateAndZone::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithIsolateAndZone::TearDownTestCase();
    i::FLAG_ignition_peephole = old_FLAG_ignition_peephole_;
    i::FLAG_ignition_reo = old_FLAG_ignition_reo_;
  }

  std::string ToLivenessString(const BytecodeLivenessState* liveness) const {
    const BitVector& bit_vector = liveness->bit_vector();

    std::string out;
    out.resize(bit_vector.length());
    for (int i = 0; i < bit_vector.length(); ++i) {
      if (bit_vector.Contains(i)) {
        out[i] = 'L';
      } else {
        out[i] = '.';
      }
    }
    return out;
  }

  void EnsureLivenessMatches(
      Handle<BytecodeArray> bytecode,
      const std::vector<std::pair<std::string, std::string>>&
          expected_liveness) {
    BytecodeAnalysis analysis(bytecode, zone(), true);
    analysis.Analyze(BailoutId::None());

    interpreter::BytecodeArrayIterator iterator(bytecode);
    for (auto liveness : expected_liveness) {
      std::stringstream ss;
      ss << std::setw(4) << iterator.current_offset() << " : ";
      iterator.PrintTo(ss);

      EXPECT_EQ(liveness.first, ToLivenessString(analysis.GetInLivenessFor(
                                    iterator.current_offset())))
          << " at bytecode " << ss.str();

      EXPECT_EQ(liveness.second, ToLivenessString(analysis.GetOutLivenessFor(
                                     iterator.current_offset())))
          << " at bytecode " << ss.str();

      iterator.Advance();
    }

    EXPECT_TRUE(iterator.done());
  }

 private:
  static bool old_FLAG_ignition_peephole_;
  static bool old_FLAG_ignition_reo_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeAnalysisTest);
};

bool BytecodeAnalysisTest::old_FLAG_ignition_peephole_;
bool BytecodeAnalysisTest::old_FLAG_ignition_reo_;

TEST_F(BytecodeAnalysisTest, EmptyBlock) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, SimpleLoad) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);

  builder.LoadAccumulatorWithRegister(reg_0);
  expected_liveness.emplace_back("L...", "...L");

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, StoreThenLoad) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);

  builder.StoreAccumulatorInRegister(reg_0);
  expected_liveness.emplace_back("...L", "L...");

  builder.LoadNull();
  expected_liveness.emplace_back("L...", "L...");

  builder.LoadAccumulatorWithRegister(reg_0);
  expected_liveness.emplace_back("L...", "...L");

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, DiamondLoad) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);
  interpreter::Register reg_1(1);
  interpreter::Register reg_2(2);

  interpreter::BytecodeLabel ld1_label;
  interpreter::BytecodeLabel end_label;

  builder.JumpIfTrue(&ld1_label);
  expected_liveness.emplace_back("LLLL", "LLL.");

  builder.LoadAccumulatorWithRegister(reg_0);
  expected_liveness.emplace_back("L.L.", "..L.");

  builder.Jump(&end_label);
  expected_liveness.emplace_back("..L.", "..L.");

  builder.Bind(&ld1_label);
  builder.LoadAccumulatorWithRegister(reg_1);
  expected_liveness.emplace_back(".LL.", "..L.");

  builder.Bind(&end_label);

  builder.LoadAccumulatorWithRegister(reg_2);
  expected_liveness.emplace_back("..L.", "...L");

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, DiamondLookupsAndBinds) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);
  interpreter::Register reg_1(1);
  interpreter::Register reg_2(2);

  interpreter::BytecodeLabel ld1_label;
  interpreter::BytecodeLabel end_label;

  builder.StoreAccumulatorInRegister(reg_0);
  expected_liveness.emplace_back(".LLL", "LLLL");

  builder.JumpIfTrue(&ld1_label);
  expected_liveness.emplace_back("LLLL", "LLL.");

  {
    builder.LoadAccumulatorWithRegister(reg_0);
    expected_liveness.emplace_back("L...", "...L");

    builder.StoreAccumulatorInRegister(reg_2);
    expected_liveness.emplace_back("...L", "..L.");

    builder.Jump(&end_label);
    expected_liveness.emplace_back("..L.", "..L.");
  }

  builder.Bind(&ld1_label);
  {
    builder.LoadAccumulatorWithRegister(reg_1);
    expected_liveness.emplace_back(".LL.", "..L.");
  }

  builder.Bind(&end_label);

  builder.LoadAccumulatorWithRegister(reg_2);
  expected_liveness.emplace_back("..L.", "...L");

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, SimpleLoop) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);
  interpreter::Register reg_1(1);
  interpreter::Register reg_2(2);

  builder.StoreAccumulatorInRegister(reg_0);
  expected_liveness.emplace_back("..LL", "L.LL");

  interpreter::LoopBuilder loop_builder(&builder);
  loop_builder.LoopHeader();
  {
    builder.JumpIfTrue(loop_builder.break_labels()->New());
    expected_liveness.emplace_back("L.LL", "L.L.");

    builder.LoadAccumulatorWithRegister(reg_0);
    expected_liveness.emplace_back("L...", "L..L");

    builder.StoreAccumulatorInRegister(reg_2);
    expected_liveness.emplace_back("L..L", "L.LL");

    loop_builder.BindContinueTarget();
    loop_builder.JumpToHeader(0);
    expected_liveness.emplace_back("L.LL", "L.LL");
  }
  loop_builder.EndLoop();

  builder.LoadAccumulatorWithRegister(reg_2);
  expected_liveness.emplace_back("..L.", "...L");

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, TryCatch) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);
  interpreter::Register reg_1(1);
  interpreter::Register reg_context(2);

  builder.StoreAccumulatorInRegister(reg_0);
  expected_liveness.emplace_back(".LLL", "LLL.");

  interpreter::TryCatchBuilder try_builder(&builder, HandlerTable::CAUGHT);
  try_builder.BeginTry(reg_context);
  {
    builder.LoadAccumulatorWithRegister(reg_0);
    expected_liveness.emplace_back("LLL.", ".LLL");

    builder.StoreAccumulatorInRegister(reg_0);
    expected_liveness.emplace_back(".LLL", ".LL.");

    builder.CallRuntime(Runtime::kThrow);
    expected_liveness.emplace_back(".LL.", ".LLL");

    builder.StoreAccumulatorInRegister(reg_0);
    // Star can't throw, so doesn't take handler liveness
    expected_liveness.emplace_back("...L", "...L");
  }
  try_builder.EndTry();
  expected_liveness.emplace_back("...L", "...L");

  // Catch
  {
    builder.LoadAccumulatorWithRegister(reg_1);
    expected_liveness.emplace_back(".L..", "...L");
  }
  try_builder.EndCatch();

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, DiamondInLoop) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);
  interpreter::Register reg_1(1);
  interpreter::Register reg_2(2);

  builder.StoreAccumulatorInRegister(reg_0);
  expected_liveness.emplace_back("...L", "L..L");

  interpreter::LoopBuilder loop_builder(&builder);
  loop_builder.LoopHeader();
  {
    builder.JumpIfTrue(loop_builder.break_labels()->New());
    expected_liveness.emplace_back("L..L", "L..L");

    interpreter::BytecodeLabel ld1_label;
    interpreter::BytecodeLabel end_label;
    builder.JumpIfTrue(&ld1_label);
    expected_liveness.emplace_back("L..L", "L..L");

    {
      builder.Jump(&end_label);
      expected_liveness.emplace_back("L..L", "L..L");
    }

    builder.Bind(&ld1_label);
    {
      builder.LoadAccumulatorWithRegister(reg_0);
      expected_liveness.emplace_back("L...", "L..L");
    }

    builder.Bind(&end_label);

    loop_builder.BindContinueTarget();
    loop_builder.JumpToHeader(0);
    expected_liveness.emplace_back("L..L", "L..L");
  }
  loop_builder.EndLoop();

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

TEST_F(BytecodeAnalysisTest, KillingLoopInsideLoop) {
  interpreter::BytecodeArrayBuilder builder(isolate(), zone(), 3, 0, 3);
  std::vector<std::pair<std::string, std::string>> expected_liveness;

  interpreter::Register reg_0(0);
  interpreter::Register reg_1(1);

  builder.StoreAccumulatorInRegister(reg_0);
  expected_liveness.emplace_back(".L.L", "LL..");

  interpreter::LoopBuilder loop_builder(&builder);
  loop_builder.LoopHeader();
  {
    builder.LoadAccumulatorWithRegister(reg_0);
    expected_liveness.emplace_back("LL..", ".L..");

    builder.LoadAccumulatorWithRegister(reg_1);
    expected_liveness.emplace_back(".L..", ".L.L");

    builder.JumpIfTrue(loop_builder.break_labels()->New());
    expected_liveness.emplace_back(".L.L", ".L.L");

    interpreter::LoopBuilder inner_loop_builder(&builder);
    inner_loop_builder.LoopHeader();
    {
      builder.StoreAccumulatorInRegister(reg_0);
      expected_liveness.emplace_back(".L.L", "LL.L");

      builder.JumpIfTrue(inner_loop_builder.break_labels()->New());
      expected_liveness.emplace_back("LL.L", "LL.L");

      inner_loop_builder.BindContinueTarget();
      inner_loop_builder.JumpToHeader(1);
      expected_liveness.emplace_back(".L.L", ".L.L");
    }
    inner_loop_builder.EndLoop();

    loop_builder.BindContinueTarget();
    loop_builder.JumpToHeader(0);
    expected_liveness.emplace_back("LL..", "LL..");
  }
  loop_builder.EndLoop();

  builder.Return();
  expected_liveness.emplace_back("...L", "....");

  Handle<BytecodeArray> bytecode = builder.ToBytecodeArray(isolate());

  EnsureLivenessMatches(bytecode, expected_liveness);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
