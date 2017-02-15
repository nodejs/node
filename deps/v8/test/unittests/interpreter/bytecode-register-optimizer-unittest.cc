// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/factory.h"
#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register-optimizer.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeRegisterOptimizerTest : public BytecodePipelineStage,
                                      public TestWithIsolateAndZone {
 public:
  BytecodeRegisterOptimizerTest() {}
  ~BytecodeRegisterOptimizerTest() override { delete register_allocator_; }

  void Initialize(int number_of_parameters, int number_of_locals) {
    register_allocator_ = new BytecodeRegisterAllocator(number_of_locals);
    register_optimizer_ = new (zone())
        BytecodeRegisterOptimizer(zone(), register_allocator_, number_of_locals,
                                  number_of_parameters, this);
  }

  void Write(BytecodeNode* node) override { output_.push_back(*node); }
  void WriteJump(BytecodeNode* node, BytecodeLabel* label) override {
    output_.push_back(*node);
  }
  void BindLabel(BytecodeLabel* label) override {}
  void BindLabel(const BytecodeLabel& target, BytecodeLabel* label) override {}
  Handle<BytecodeArray> ToBytecodeArray(
      Isolate* isolate, int fixed_register_count, int parameter_count,
      Handle<FixedArray> handle_table) override {
    return Handle<BytecodeArray>();
  }

  BytecodeRegisterAllocator* allocator() { return register_allocator_; }
  BytecodeRegisterOptimizer* optimizer() { return register_optimizer_; }

  Register NewTemporary() { return allocator()->NewRegister(); }

  void ReleaseTemporaries(Register reg) {
    allocator()->ReleaseRegisters(reg.index());
  }

  size_t write_count() const { return output_.size(); }
  const BytecodeNode& last_written() const { return output_.back(); }
  const std::vector<BytecodeNode>* output() { return &output_; }

 private:
  BytecodeRegisterAllocator* register_allocator_;
  BytecodeRegisterOptimizer* register_optimizer_;

  std::vector<BytecodeNode> output_;
};

// Sanity tests.

TEST_F(BytecodeRegisterOptimizerTest, WriteNop) {
  Initialize(1, 1);
  BytecodeNode node(Bytecode::kNop);
  optimizer()->Write(&node);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(node, last_written());
}

TEST_F(BytecodeRegisterOptimizerTest, WriteNopExpression) {
  Initialize(1, 1);
  BytecodeSourceInfo source_info(3, false);
  BytecodeNode node(Bytecode::kNop, &source_info);
  optimizer()->Write(&node);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(node, last_written());
}

TEST_F(BytecodeRegisterOptimizerTest, WriteNopStatement) {
  Initialize(1, 1);
  BytecodeSourceInfo source_info(3, true);
  BytecodeNode node(Bytecode::kNop);
  optimizer()->Write(&node);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(node, last_written());
}

TEST_F(BytecodeRegisterOptimizerTest, TemporaryMaterializedForJump) {
  Initialize(1, 1);
  Register temp = NewTemporary();
  BytecodeNode node(Bytecode::kStar, temp.ToOperand());
  optimizer()->Write(&node);
  CHECK_EQ(write_count(), 0);
  BytecodeLabel label;
  BytecodeNode jump(Bytecode::kJump, 0, nullptr);
  optimizer()->WriteJump(&jump, &label);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0), temp.ToOperand());
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kJump);
}

TEST_F(BytecodeRegisterOptimizerTest, TemporaryMaterializedForBind) {
  Initialize(1, 1);
  Register temp = NewTemporary();
  BytecodeNode node(Bytecode::kStar, temp.ToOperand());
  optimizer()->Write(&node);
  CHECK_EQ(write_count(), 0);
  BytecodeLabel label;
  optimizer()->BindLabel(&label);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0), temp.ToOperand());
}

// Basic Register Optimizations

TEST_F(BytecodeRegisterOptimizerTest, TemporaryNotEmitted) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  BytecodeNode node0(Bytecode::kLdar, parameter.ToOperand());
  optimizer()->Write(&node0);
  CHECK_EQ(write_count(), 0);
  Register temp = NewTemporary();
  BytecodeNode node1(Bytecode::kStar, NewTemporary().ToOperand());
  optimizer()->Write(&node1);
  CHECK_EQ(write_count(), 0);
  ReleaseTemporaries(temp);
  CHECK_EQ(write_count(), 0);
  BytecodeNode node2(Bytecode::kReturn);
  optimizer()->Write(&node2);
  CHECK_EQ(write_count(), 2);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kLdar);
  CHECK_EQ(output()->at(0).operand(0), parameter.ToOperand());
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kReturn);
}

TEST_F(BytecodeRegisterOptimizerTest, ReleasedRegisterUsed) {
  Initialize(3, 1);
  BytecodeNode node0(Bytecode::kLdaSmi, 3);
  optimizer()->Write(&node0);
  CHECK_EQ(write_count(), 1);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  BytecodeNode node1(Bytecode::kStar, temp1.ToOperand());
  optimizer()->Write(&node1);
  CHECK_EQ(write_count(), 1);
  BytecodeNode node2(Bytecode::kLdaSmi, 1);
  optimizer()->Write(&node2);
  CHECK_EQ(write_count(), 3);
  BytecodeNode node3(Bytecode::kMov, temp1.ToOperand(), temp0.ToOperand());
  optimizer()->Write(&node3);
  CHECK_EQ(write_count(), 3);
  ReleaseTemporaries(temp1);
  CHECK_EQ(write_count(), 3);
  BytecodeNode node4(Bytecode::kLdar, temp0.ToOperand());
  optimizer()->Write(&node4);
  CHECK_EQ(write_count(), 3);
  BytecodeNode node5(Bytecode::kReturn);
  optimizer()->Write(&node5);
  CHECK_EQ(write_count(), 5);
  CHECK_EQ(output()->at(3).bytecode(), Bytecode::kLdar);
  CHECK_EQ(output()->at(3).operand(0), temp1.ToOperand());
  CHECK_EQ(output()->at(4).bytecode(), Bytecode::kReturn);
}

TEST_F(BytecodeRegisterOptimizerTest, ReleasedRegisterNotFlushed) {
  Initialize(3, 1);
  BytecodeNode node0(Bytecode::kLdaSmi, 3);
  optimizer()->Write(&node0);
  CHECK_EQ(write_count(), 1);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  BytecodeNode node1(Bytecode::kStar, temp0.ToOperand());
  optimizer()->Write(&node1);
  CHECK_EQ(write_count(), 1);
  BytecodeNode node2(Bytecode::kStar, temp1.ToOperand());
  optimizer()->Write(&node2);
  CHECK_EQ(write_count(), 1);
  ReleaseTemporaries(temp1);
  BytecodeLabel label;
  BytecodeNode jump(Bytecode::kJump, 0, nullptr);
  optimizer()->WriteJump(&jump, &label);
  BytecodeNode node3(Bytecode::kReturn);
  optimizer()->Write(&node3);
  CHECK_EQ(write_count(), 4);
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(1).operand(0), temp0.ToOperand());
  CHECK_EQ(output()->at(2).bytecode(), Bytecode::kJump);
  CHECK_EQ(output()->at(3).bytecode(), Bytecode::kReturn);
}

TEST_F(BytecodeRegisterOptimizerTest, StoresToLocalsImmediate) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  BytecodeNode node0(Bytecode::kLdar, parameter.ToOperand());
  optimizer()->Write(&node0);
  CHECK_EQ(write_count(), 0);
  Register local = Register(0);
  BytecodeNode node1(Bytecode::kStar, local.ToOperand());
  optimizer()->Write(&node1);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kMov);
  CHECK_EQ(output()->at(0).operand(0), parameter.ToOperand());
  CHECK_EQ(output()->at(0).operand(1), local.ToOperand());

  BytecodeNode node2(Bytecode::kReturn);
  optimizer()->Write(&node2);
  CHECK_EQ(write_count(), 3);
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kLdar);
  CHECK_EQ(output()->at(1).operand(0), local.ToOperand());
  CHECK_EQ(output()->at(2).bytecode(), Bytecode::kReturn);
}

TEST_F(BytecodeRegisterOptimizerTest, TemporaryNotMaterializedForInput) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  BytecodeNode node0(Bytecode::kMov, parameter.ToOperand(), temp0.ToOperand());
  optimizer()->Write(&node0);
  BytecodeNode node1(Bytecode::kMov, parameter.ToOperand(), temp1.ToOperand());
  optimizer()->Write(&node1);
  CHECK_EQ(write_count(), 0);
  BytecodeNode node2(Bytecode::kCallJSRuntime, 0, temp0.ToOperand(), 1);
  optimizer()->Write(&node2);
  CHECK_EQ(write_count(), 1);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kCallJSRuntime);
  CHECK_EQ(output()->at(0).operand(0), 0);
  CHECK_EQ(output()->at(0).operand(1), parameter.ToOperand());
  CHECK_EQ(output()->at(0).operand(2), 1);
}

TEST_F(BytecodeRegisterOptimizerTest, RangeOfTemporariesMaterializedForInput) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  BytecodeNode node0(Bytecode::kLdaSmi, 3);
  optimizer()->Write(&node0);
  CHECK_EQ(write_count(), 1);
  BytecodeNode node1(Bytecode::kStar, temp0.ToOperand());
  optimizer()->Write(&node1);
  BytecodeNode node2(Bytecode::kMov, parameter.ToOperand(), temp1.ToOperand());
  optimizer()->Write(&node2);
  CHECK_EQ(write_count(), 1);
  BytecodeNode node3(Bytecode::kCallJSRuntime, 0, temp0.ToOperand(), 2);
  optimizer()->Write(&node3);
  CHECK_EQ(write_count(), 4);

  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kLdaSmi);
  CHECK_EQ(output()->at(0).operand(0), 3);

  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(1).operand(0), temp0.ToOperand());

  CHECK_EQ(output()->at(2).bytecode(), Bytecode::kMov);
  CHECK_EQ(output()->at(2).operand(0), parameter.ToOperand());
  CHECK_EQ(output()->at(2).operand(1), temp1.ToOperand());

  CHECK_EQ(output()->at(3).bytecode(), Bytecode::kCallJSRuntime);
  CHECK_EQ(output()->at(3).operand(0), 0);
  CHECK_EQ(output()->at(3).operand(1), temp0.ToOperand());
  CHECK_EQ(output()->at(3).operand(2), 2);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
