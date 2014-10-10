// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_SELECTOR_UNITTEST_H_
#define V8_COMPILER_INSTRUCTION_SELECTOR_UNITTEST_H_

#include <deque>
#include <set>

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/test/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class InstructionSelectorTest : public TestWithContext, public TestWithZone {
 public:
  InstructionSelectorTest();
  virtual ~InstructionSelectorTest();

  base::RandomNumberGenerator* rng() { return &rng_; }

  class Stream;

  enum StreamBuilderMode {
    kAllInstructions,
    kTargetInstructions,
    kAllExceptNopInstructions
  };

  class StreamBuilder FINAL : public RawMachineAssembler {
   public:
    StreamBuilder(InstructionSelectorTest* test, MachineType return_type)
        : RawMachineAssembler(new (test->zone()) Graph(test->zone()),
                              MakeMachineSignature(test->zone(), return_type)),
          test_(test) {}
    StreamBuilder(InstructionSelectorTest* test, MachineType return_type,
                  MachineType parameter0_type)
        : RawMachineAssembler(
              new (test->zone()) Graph(test->zone()),
              MakeMachineSignature(test->zone(), return_type, parameter0_type)),
          test_(test) {}
    StreamBuilder(InstructionSelectorTest* test, MachineType return_type,
                  MachineType parameter0_type, MachineType parameter1_type)
        : RawMachineAssembler(
              new (test->zone()) Graph(test->zone()),
              MakeMachineSignature(test->zone(), return_type, parameter0_type,
                                   parameter1_type)),
          test_(test) {}
    StreamBuilder(InstructionSelectorTest* test, MachineType return_type,
                  MachineType parameter0_type, MachineType parameter1_type,
                  MachineType parameter2_type)
        : RawMachineAssembler(
              new (test->zone()) Graph(test->zone()),
              MakeMachineSignature(test->zone(), return_type, parameter0_type,
                                   parameter1_type, parameter2_type)),
          test_(test) {}

    Stream Build(CpuFeature feature) {
      return Build(InstructionSelector::Features(feature));
    }
    Stream Build(CpuFeature feature1, CpuFeature feature2) {
      return Build(InstructionSelector::Features(feature1, feature2));
    }
    Stream Build(StreamBuilderMode mode = kTargetInstructions) {
      return Build(InstructionSelector::Features(), mode);
    }
    Stream Build(InstructionSelector::Features features,
                 StreamBuilderMode mode = kTargetInstructions);

   private:
    MachineSignature* MakeMachineSignature(Zone* zone,
                                           MachineType return_type) {
      MachineSignature::Builder builder(zone, 1, 0);
      builder.AddReturn(return_type);
      return builder.Build();
    }

    MachineSignature* MakeMachineSignature(Zone* zone, MachineType return_type,
                                           MachineType parameter0_type) {
      MachineSignature::Builder builder(zone, 1, 1);
      builder.AddReturn(return_type);
      builder.AddParam(parameter0_type);
      return builder.Build();
    }

    MachineSignature* MakeMachineSignature(Zone* zone, MachineType return_type,
                                           MachineType parameter0_type,
                                           MachineType parameter1_type) {
      MachineSignature::Builder builder(zone, 1, 2);
      builder.AddReturn(return_type);
      builder.AddParam(parameter0_type);
      builder.AddParam(parameter1_type);
      return builder.Build();
    }

    MachineSignature* MakeMachineSignature(Zone* zone, MachineType return_type,
                                           MachineType parameter0_type,
                                           MachineType parameter1_type,
                                           MachineType parameter2_type) {
      MachineSignature::Builder builder(zone, 1, 3);
      builder.AddReturn(return_type);
      builder.AddParam(parameter0_type);
      builder.AddParam(parameter1_type);
      builder.AddParam(parameter2_type);
      return builder.Build();
    }

   private:
    InstructionSelectorTest* test_;
  };

  class Stream FINAL {
   public:
    size_t size() const { return instructions_.size(); }
    const Instruction* operator[](size_t index) const {
      EXPECT_LT(index, size());
      return instructions_[index];
    }

    bool IsDouble(const InstructionOperand* operand) const {
      return IsDouble(ToVreg(operand));
    }
    bool IsDouble(int virtual_register) const {
      return doubles_.find(virtual_register) != doubles_.end();
    }

    bool IsInteger(const InstructionOperand* operand) const {
      return IsInteger(ToVreg(operand));
    }
    bool IsInteger(int virtual_register) const {
      return !IsDouble(virtual_register) && !IsReference(virtual_register);
    }

    bool IsReference(const InstructionOperand* operand) const {
      return IsReference(ToVreg(operand));
    }
    bool IsReference(int virtual_register) const {
      return references_.find(virtual_register) != references_.end();
    }

    float ToFloat32(const InstructionOperand* operand) const {
      return ToConstant(operand).ToFloat32();
    }

    int32_t ToInt32(const InstructionOperand* operand) const {
      return ToConstant(operand).ToInt32();
    }

    int64_t ToInt64(const InstructionOperand* operand) const {
      return ToConstant(operand).ToInt64();
    }

    int ToVreg(const InstructionOperand* operand) const {
      if (operand->IsConstant()) return operand->index();
      EXPECT_EQ(InstructionOperand::UNALLOCATED, operand->kind());
      return UnallocatedOperand::cast(operand)->virtual_register();
    }

    FrameStateDescriptor* GetFrameStateDescriptor(int deoptimization_id) {
      EXPECT_LT(deoptimization_id, GetFrameStateDescriptorCount());
      return deoptimization_entries_[deoptimization_id];
    }

    int GetFrameStateDescriptorCount() {
      return static_cast<int>(deoptimization_entries_.size());
    }

   private:
    Constant ToConstant(const InstructionOperand* operand) const {
      ConstantMap::const_iterator i;
      if (operand->IsConstant()) {
        i = constants_.find(operand->index());
        EXPECT_FALSE(constants_.end() == i);
      } else {
        EXPECT_EQ(InstructionOperand::IMMEDIATE, operand->kind());
        i = immediates_.find(operand->index());
        EXPECT_FALSE(immediates_.end() == i);
      }
      EXPECT_EQ(operand->index(), i->first);
      return i->second;
    }

    friend class StreamBuilder;

    typedef std::map<int, Constant> ConstantMap;

    ConstantMap constants_;
    ConstantMap immediates_;
    std::deque<Instruction*> instructions_;
    std::set<int> doubles_;
    std::set<int> references_;
    std::deque<FrameStateDescriptor*> deoptimization_entries_;
  };

  base::RandomNumberGenerator rng_;
};


template <typename T>
class InstructionSelectorTestWithParam
    : public InstructionSelectorTest,
      public ::testing::WithParamInterface<T> {};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_SELECTOR_UNITTEST_H_
