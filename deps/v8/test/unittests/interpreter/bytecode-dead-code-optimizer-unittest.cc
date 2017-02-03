// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/bytecode-dead-code-optimizer.h"
#include "src/interpreter/bytecode-label.h"
#include "src/objects.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeDeadCodeOptimizerTest : public BytecodePipelineStage,
                                      public TestWithIsolateAndZone {
 public:
  BytecodeDeadCodeOptimizerTest()
      : dead_code_optimizer_(this), last_written_(Bytecode::kIllegal) {}
  ~BytecodeDeadCodeOptimizerTest() override {}

  void Write(BytecodeNode* node) override {
    write_count_++;
    last_written_.Clone(node);
  }

  void WriteJump(BytecodeNode* node, BytecodeLabel* label) override {
    write_count_++;
    last_written_.Clone(node);
  }

  void BindLabel(BytecodeLabel* label) override {}
  void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) override {}
  Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int fixed_register_count, int parameter_count,
      Handle<FixedArray> handle_table) override {
    return Handle<BytecodeArray>();
  }

  BytecodeDeadCodeOptimizer* optimizer() { return &dead_code_optimizer_; }

  int write_count() const { return write_count_; }
  const BytecodeNode& last_written() const { return last_written_; }

 private:
  BytecodeDeadCodeOptimizer dead_code_optimizer_;

  int write_count_ = 0;
  BytecodeNode last_written_;
};

TEST_F(BytecodeDeadCodeOptimizerTest, LiveCodeKept) {
  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(add, last_written());

  BytecodeLabel target;
  BytecodeNode jump(Bytecode::kJump, 0, nullptr);
  optimizer()->WriteJump(&jump, &target);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(jump, last_written());
}

TEST_F(BytecodeDeadCodeOptimizerTest, DeadCodeAfterReturnEliminated) {
  BytecodeNode ret(Bytecode::kReturn);
  optimizer()->Write(&ret);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(ret, last_written());

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(ret, last_written());
}

TEST_F(BytecodeDeadCodeOptimizerTest, DeadCodeAfterThrowEliminated) {
  BytecodeNode thrw(Bytecode::kThrow);
  optimizer()->Write(&thrw);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(thrw, last_written());

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(thrw, last_written());
}

TEST_F(BytecodeDeadCodeOptimizerTest, DeadCodeAfterReThrowEliminated) {
  BytecodeNode rethrow(Bytecode::kReThrow);
  optimizer()->Write(&rethrow);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(rethrow, last_written());

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(rethrow, last_written());
}

TEST_F(BytecodeDeadCodeOptimizerTest, DeadCodeAfterJumpEliminated) {
  BytecodeLabel target;
  BytecodeNode jump(Bytecode::kJump, 0, nullptr);
  optimizer()->WriteJump(&jump, &target);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(jump, last_written());

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(jump, last_written());
}

TEST_F(BytecodeDeadCodeOptimizerTest, DeadCodeStillDeadAfterConditinalJump) {
  BytecodeNode ret(Bytecode::kReturn);
  optimizer()->Write(&ret);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(ret, last_written());

  BytecodeLabel target;
  BytecodeNode jump(Bytecode::kJumpIfTrue, 0, nullptr);
  optimizer()->WriteJump(&jump, &target);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(ret, last_written());

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(ret, last_written());
}

TEST_F(BytecodeDeadCodeOptimizerTest, CodeLiveAfterLabelBind) {
  BytecodeNode ret(Bytecode::kReturn);
  optimizer()->Write(&ret);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(ret, last_written());

  BytecodeLabel target;
  optimizer()->BindLabel(&target);

  BytecodeNode add(Bytecode::kAdd, Register(0).ToOperand(), 1);
  optimizer()->Write(&add);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(add, last_written());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
