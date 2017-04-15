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

TEST_F(BytecodeRegisterOptimizerTest, TemporaryMaterializedForFlush) {
  Initialize(1, 1);
  Register temp = NewTemporary();
  optimizer()->DoStar(temp, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  optimizer()->Flush();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0), static_cast<uint32_t>(temp.ToOperand()));
}

TEST_F(BytecodeRegisterOptimizerTest, TemporaryMaterializedForJump) {
  Initialize(1, 1);
  Register temp = NewTemporary();
  optimizer()->DoStar(temp, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  optimizer()->PrepareForBytecode<Bytecode::kJump, AccumulatorUse::kNone>();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0), static_cast<uint32_t>(temp.ToOperand()));
}

// Basic Register Optimizations

TEST_F(BytecodeRegisterOptimizerTest, TemporaryNotEmitted) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  optimizer()->DoLdar(parameter, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  Register temp = NewTemporary();
  optimizer()->DoStar(temp, BytecodeSourceInfo());
  BytecodeNode node1(Bytecode::kStar, NewTemporary().ToOperand());
  ReleaseTemporaries(temp);
  CHECK_EQ(write_count(), 0u);
  optimizer()->PrepareForBytecode<Bytecode::kReturn, AccumulatorUse::kRead>();
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kLdar);
  CHECK_EQ(output()->at(0).operand(0),
           static_cast<uint32_t>(parameter.ToOperand()));
}

TEST_F(BytecodeRegisterOptimizerTest, ReleasedRegisterUsed) {
  Initialize(3, 1);
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->DoStar(temp1, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0),
           static_cast<uint32_t>(temp1.ToOperand()));
  optimizer()->DoMov(temp1, temp0, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 1u);
  ReleaseTemporaries(temp1);
  CHECK_EQ(write_count(), 1u);
  optimizer()->DoLdar(temp0, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 1u);
  optimizer()->PrepareForBytecode<Bytecode::kReturn, AccumulatorUse::kRead>();
  CHECK_EQ(write_count(), 2u);
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kLdar);
  CHECK_EQ(output()->at(1).operand(0),
           static_cast<uint32_t>(temp1.ToOperand()));
}

TEST_F(BytecodeRegisterOptimizerTest, ReleasedRegisterNotFlushed) {
  Initialize(3, 1);
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->DoStar(temp0, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  optimizer()->DoStar(temp1, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  ReleaseTemporaries(temp1);
  optimizer()->Flush();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0),
           static_cast<uint32_t>(temp0.ToOperand()));
}

TEST_F(BytecodeRegisterOptimizerTest, StoresToLocalsImmediate) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  optimizer()->DoLdar(parameter, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);
  Register local = Register(0);
  optimizer()->DoStar(local, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kMov);
  CHECK_EQ(output()->at(0).operand(0),
           static_cast<uint32_t>(parameter.ToOperand()));
  CHECK_EQ(output()->at(0).operand(1),
           static_cast<uint32_t>(local.ToOperand()));

  optimizer()->PrepareForBytecode<Bytecode::kReturn, AccumulatorUse::kRead>();
  CHECK_EQ(write_count(), 2u);
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kLdar);
  CHECK_EQ(output()->at(1).operand(0),
           static_cast<uint32_t>(local.ToOperand()));
}

TEST_F(BytecodeRegisterOptimizerTest, SingleTemporaryNotMaterializedForInput) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->DoMov(parameter, temp0, BytecodeSourceInfo());
  optimizer()->DoMov(parameter, temp1, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);

  Register reg = optimizer()->GetInputRegister(temp0);
  RegisterList reg_list =
      optimizer()->GetInputRegisterList(RegisterList(temp0.index(), 1));
  CHECK_EQ(write_count(), 0u);
  CHECK_EQ(parameter.index(), reg.index());
  CHECK_EQ(parameter.index(), reg_list.first_register().index());
  CHECK_EQ(1, reg_list.register_count());
}

TEST_F(BytecodeRegisterOptimizerTest, RangeOfTemporariesMaterializedForInput) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  optimizer()->DoStar(temp0, BytecodeSourceInfo());
  optimizer()->DoMov(parameter, temp1, BytecodeSourceInfo());
  CHECK_EQ(write_count(), 0u);

  optimizer()
      ->PrepareForBytecode<Bytecode::kCallJSRuntime, AccumulatorUse::kWrite>();
  RegisterList reg_list =
      optimizer()->GetInputRegisterList(RegisterList(temp0.index(), 2));
  CHECK_EQ(temp0.index(), reg_list.first_register().index());
  CHECK_EQ(2, reg_list.register_count());
  CHECK_EQ(write_count(), 2u);
  CHECK_EQ(output()->at(0).bytecode(), Bytecode::kStar);
  CHECK_EQ(output()->at(0).operand(0),
           static_cast<uint32_t>(temp0.ToOperand()));
  CHECK_EQ(output()->at(1).bytecode(), Bytecode::kMov);
  CHECK_EQ(output()->at(1).operand(0),
           static_cast<uint32_t>(parameter.ToOperand()));
  CHECK_EQ(output()->at(1).operand(1),
           static_cast<uint32_t>(temp1.ToOperand()));
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
