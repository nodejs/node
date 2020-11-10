// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"

#include "src/interpreter/bytecode-label.h"
#include "src/interpreter/bytecode-register-optimizer.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class BytecodeRegisterOptimizerTest
    : public BytecodeRegisterOptimizer::BytecodeWriter,
      public TestWithIsolateAndZone {
 public:
  struct RegisterTransfer {
    Bytecode bytecode;
    Register input;
    Register output;
  };

  BytecodeRegisterOptimizerTest() = default;
  ~BytecodeRegisterOptimizerTest() override { delete register_allocator_; }

  void Initialize(int number_of_parameters, int number_of_locals) {
    register_allocator_ = new BytecodeRegisterAllocator(number_of_locals);
    register_optimizer_ = new (zone())
        BytecodeRegisterOptimizer(zone(), register_allocator_, number_of_locals,
                                  number_of_parameters, this);
  }

  void EmitLdar(Register input) override {
    output_.push_back({Bytecode::kLdar, input, Register()});
  }
  void EmitStar(Register output) override {
    output_.push_back({Bytecode::kStar, Register(), output});
  }
  void EmitMov(Register input, Register output) override {
    output_.push_back({Bytecode::kMov, input, output});
  }

  BytecodeRegisterAllocator* allocator() { return register_allocator_; }
  BytecodeRegisterOptimizer* optimizer() { return register_optimizer_; }

  Register NewTemporary() { return allocator()->NewRegister(); }

  void ReleaseTemporaries(Register reg) {
    allocator()->ReleaseRegisters(reg.index());
  }

  size_t write_count() const { return output_.size(); }
  const RegisterTransfer& last_written() const { return output_.back(); }
  const std::vector<RegisterTransfer>* output() { return &output_; }

 private:
  BytecodeRegisterAllocator* register_allocator_;
  BytecodeRegisterOptimizer* register_optimizer_;

  std::vector<RegisterTransfer> output_;
};

// Sanity tests.

TEST_F(BytecodeRegisterOptimizerTest, TemporaryMaterializedForFlush) {
  Initialize(1, 1);
  Register temp = NewTemporary();
  optimizer()->DoStar(temp);
  CHECK_EQ(write_count(), 0u);
  optimizer()->Flush();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kStar);
  CHECK_EQ(output()->at(0).output.index(), temp.index());
}

TEST_F(BytecodeRegisterOptimizerTest, TemporaryMaterializedForJump) {
  Initialize(1, 1);
  Register temp = NewTemporary();
  optimizer()->DoStar(temp);
  CHECK_EQ(write_count(), 0u);
  optimizer()->PrepareForBytecode<Bytecode::kJump, AccumulatorUse::kNone>();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kStar);
  CHECK_EQ(output()->at(0).output.index(), temp.index());
}

// Basic Register Optimizations

TEST_F(BytecodeRegisterOptimizerTest, TemporaryNotEmitted) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  optimizer()->DoLdar(parameter);
  CHECK_EQ(write_count(), 0u);
  Register temp = NewTemporary();
  optimizer()->DoStar(temp);
  ReleaseTemporaries(temp);
  CHECK_EQ(write_count(), 0u);
  optimizer()->PrepareForBytecode<Bytecode::kReturn, AccumulatorUse::kRead>();
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kLdar);
  CHECK_EQ(output()->at(0).input.index(), parameter.index());
}

TEST_F(BytecodeRegisterOptimizerTest, ReleasedRegisterUsed) {
  Initialize(3, 1);
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->DoStar(temp1);
  CHECK_EQ(write_count(), 0u);
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kStar);
  CHECK_EQ(output()->at(0).output.index(), temp1.index());
  optimizer()->DoMov(temp1, temp0);
  CHECK_EQ(write_count(), 1u);
  ReleaseTemporaries(temp1);
  CHECK_EQ(write_count(), 1u);
  optimizer()->DoLdar(temp0);
  CHECK_EQ(write_count(), 1u);
  optimizer()->PrepareForBytecode<Bytecode::kReturn, AccumulatorUse::kRead>();
  CHECK_EQ(write_count(), 2u);
  CHECK_EQ(output()->at(1).bytecode, Bytecode::kLdar);
  CHECK_EQ(output()->at(1).input.index(), temp1.index());
}

TEST_F(BytecodeRegisterOptimizerTest, ReleasedRegisterNotFlushed) {
  Initialize(3, 1);
  optimizer()->PrepareForBytecode<Bytecode::kLdaSmi, AccumulatorUse::kWrite>();
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->DoStar(temp0);
  CHECK_EQ(write_count(), 0u);
  optimizer()->DoStar(temp1);
  CHECK_EQ(write_count(), 0u);
  ReleaseTemporaries(temp1);
  optimizer()->Flush();
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kStar);
  CHECK_EQ(output()->at(0).output.index(), temp0.index());
}

TEST_F(BytecodeRegisterOptimizerTest, StoresToLocalsImmediate) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  optimizer()->DoLdar(parameter);
  CHECK_EQ(write_count(), 0u);
  Register local = Register(0);
  optimizer()->DoStar(local);
  CHECK_EQ(write_count(), 1u);
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kMov);
  CHECK_EQ(output()->at(0).input.index(), parameter.index());
  CHECK_EQ(output()->at(0).output.index(), local.index());

  optimizer()->PrepareForBytecode<Bytecode::kReturn, AccumulatorUse::kRead>();
  CHECK_EQ(write_count(), 2u);
  CHECK_EQ(output()->at(1).bytecode, Bytecode::kLdar);
  CHECK_EQ(output()->at(1).input.index(), local.index());
}

TEST_F(BytecodeRegisterOptimizerTest, SingleTemporaryNotMaterializedForInput) {
  Initialize(3, 1);
  Register parameter = Register::FromParameterIndex(1, 3);
  Register temp0 = NewTemporary();
  Register temp1 = NewTemporary();
  optimizer()->DoMov(parameter, temp0);
  optimizer()->DoMov(parameter, temp1);
  CHECK_EQ(write_count(), 0u);

  Register reg = optimizer()->GetInputRegister(temp0);
  RegisterList reg_list = optimizer()->GetInputRegisterList(
      BytecodeUtils::NewRegisterList(temp0.index(), 1));
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
  optimizer()->DoStar(temp0);
  optimizer()->DoMov(parameter, temp1);
  CHECK_EQ(write_count(), 0u);

  optimizer()
      ->PrepareForBytecode<Bytecode::kCallJSRuntime, AccumulatorUse::kWrite>();
  RegisterList reg_list = optimizer()->GetInputRegisterList(
      BytecodeUtils::NewRegisterList(temp0.index(), 2));
  CHECK_EQ(temp0.index(), reg_list.first_register().index());
  CHECK_EQ(2, reg_list.register_count());
  CHECK_EQ(write_count(), 2u);
  CHECK_EQ(output()->at(0).bytecode, Bytecode::kStar);
  CHECK_EQ(output()->at(0).output.index(), temp0.index());
  CHECK_EQ(output()->at(1).bytecode, Bytecode::kMov);
  CHECK_EQ(output()->at(1).input.index(), parameter.index());
  CHECK_EQ(output()->at(1).output.index(), temp1.index());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
