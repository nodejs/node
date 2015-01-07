// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/pipeline.h"
#include "test/unittests/compiler/instruction-sequence-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class RegisterAllocatorTest : public InstructionSequenceTest {
 public:
  void Allocate() {
    WireBlocks();
    Pipeline::AllocateRegistersForTesting(config(), sequence(), true);
  }
};


TEST_F(RegisterAllocatorTest, CanAllocateThreeRegisters) {
  // return p0 + p1;
  StartBlock();
  auto a_reg = Parameter();
  auto b_reg = Parameter();
  auto c_reg = EmitOI(Reg(1), Reg(a_reg, 1), Reg(b_reg, 0));
  Return(c_reg);
  EndBlock(Last());

  Allocate();
}


TEST_F(RegisterAllocatorTest, SimpleLoop) {
  // i = K;
  // while(true) { i++ }
  StartBlock();
  auto i_reg = DefineConstant();
  EndBlock();

  {
    StartLoop(1);

    StartBlock();
    auto phi = Phi(i_reg);
    auto ipp = EmitOI(Same(), Reg(phi), Use(DefineConstant()));
    Extend(phi, ipp);
    EndBlock(Jump(0));

    EndLoop();
  }

  Allocate();
}


TEST_F(RegisterAllocatorTest, SimpleBranch) {
  // return i ? K1 : K2
  StartBlock();
  auto i = DefineConstant();
  EndBlock(Branch(Reg(i), 1, 2));

  StartBlock();
  Return(DefineConstant());
  EndBlock(Last());

  StartBlock();
  Return(DefineConstant());
  EndBlock(Last());

  Allocate();
}


TEST_F(RegisterAllocatorTest, SimpleDiamond) {
  // return p0 ? p0 : p0
  StartBlock();
  auto param = Parameter();
  EndBlock(Branch(Reg(param), 1, 2));

  StartBlock();
  EndBlock(Jump(2));

  StartBlock();
  EndBlock(Jump(1));

  StartBlock();
  Return(param);
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, SimpleDiamondPhi) {
  // return i ? K1 : K2
  StartBlock();
  EndBlock(Branch(Reg(DefineConstant()), 1, 2));

  StartBlock();
  auto t_val = DefineConstant();
  EndBlock(Jump(2));

  StartBlock();
  auto f_val = DefineConstant();
  EndBlock(Jump(1));

  StartBlock();
  Return(Reg(Phi(t_val, f_val)));
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, DiamondManyPhis) {
  const int kPhis = kDefaultNRegs * 2;

  StartBlock();
  EndBlock(Branch(Reg(DefineConstant()), 1, 2));

  StartBlock();
  VReg t_vals[kPhis];
  for (int i = 0; i < kPhis; ++i) {
    t_vals[i] = DefineConstant();
  }
  EndBlock(Jump(2));

  StartBlock();
  VReg f_vals[kPhis];
  for (int i = 0; i < kPhis; ++i) {
    f_vals[i] = DefineConstant();
  }
  EndBlock(Jump(1));

  StartBlock();
  TestOperand merged[kPhis];
  for (int i = 0; i < kPhis; ++i) {
    merged[i] = Use(Phi(t_vals[i], f_vals[i]));
  }
  Return(EmitCall(Slot(-1), kPhis, merged));
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, DoubleDiamondManyRedundantPhis) {
  const int kPhis = kDefaultNRegs * 2;

  // First diamond.
  StartBlock();
  VReg vals[kPhis];
  for (int i = 0; i < kPhis; ++i) {
    vals[i] = Parameter(Slot(-1 - i));
  }
  EndBlock(Branch(Reg(DefineConstant()), 1, 2));

  StartBlock();
  EndBlock(Jump(2));

  StartBlock();
  EndBlock(Jump(1));

  // Second diamond.
  StartBlock();
  EndBlock(Branch(Reg(DefineConstant()), 1, 2));

  StartBlock();
  EndBlock(Jump(2));

  StartBlock();
  EndBlock(Jump(1));

  StartBlock();
  TestOperand merged[kPhis];
  for (int i = 0; i < kPhis; ++i) {
    merged[i] = Use(Phi(vals[i], vals[i]));
  }
  Return(EmitCall(Reg(0), kPhis, merged));
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, RegressionPhisNeedTooManyRegisters) {
  const size_t kNumRegs = 3;
  const size_t kParams = kNumRegs + 1;
  // Override number of registers.
  SetNumRegs(kNumRegs, kNumRegs);

  StartBlock();
  auto constant = DefineConstant();
  VReg parameters[kParams];
  for (size_t i = 0; i < arraysize(parameters); ++i) {
    parameters[i] = DefineConstant();
  }
  EndBlock();

  PhiInstruction* phis[kParams];
  {
    StartLoop(2);

    // Loop header.
    StartBlock();

    for (size_t i = 0; i < arraysize(parameters); ++i) {
      phis[i] = Phi(parameters[i]);
    }

    // Perform some computations.
    // something like phi[i] += const
    for (size_t i = 0; i < arraysize(parameters); ++i) {
      auto result = EmitOI(Same(), Reg(phis[i]), Use(constant));
      Extend(phis[i], result);
    }

    EndBlock(Branch(Reg(DefineConstant()), 1, 2));

    // Jump back to loop header.
    StartBlock();
    EndBlock(Jump(-1));

    EndLoop();
  }

  StartBlock();
  Return(DefineConstant());
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, SpillPhi) {
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  auto left = Define(Reg(0));
  EndBlock(Jump(2));

  StartBlock();
  auto right = Define(Reg(0));
  EndBlock();

  StartBlock();
  auto phi = Phi(left, right);
  EmitCall(Slot(-1));
  Return(Reg(phi));
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, MoveLotsOfConstants) {
  StartBlock();
  VReg constants[kDefaultNRegs];
  for (size_t i = 0; i < arraysize(constants); ++i) {
    constants[i] = DefineConstant();
  }
  TestOperand call_ops[kDefaultNRegs * 2];
  for (int i = 0; i < kDefaultNRegs; ++i) {
    call_ops[i] = Reg(constants[i], i);
  }
  for (int i = 0; i < kDefaultNRegs; ++i) {
    call_ops[i + kDefaultNRegs] = Slot(constants[i], i);
  }
  EmitCall(Slot(-1), arraysize(call_ops), call_ops);
  EndBlock(Last());

  Allocate();
}


TEST_F(RegisterAllocatorTest, SplitBeforeInstruction) {
  const int kNumRegs = 6;
  SetNumRegs(kNumRegs, kNumRegs);

  StartBlock();

  // Stack parameters/spilled values.
  auto p_0 = Define(Slot(-1));
  auto p_1 = Define(Slot(-2));

  // Fill registers.
  VReg values[kNumRegs];
  for (size_t i = 0; i < arraysize(values); ++i) {
    values[i] = Define(Reg(static_cast<int>(i)));
  }

  // values[0] will be split in the second half of this instruction.
  // Models Intel mod instructions.
  EmitOI(Reg(0), Reg(p_0, 1), UniqueReg(p_1));
  EmitI(Reg(values[0], 0));
  EndBlock(Last());

  Allocate();
}


TEST_F(RegisterAllocatorTest, NestedDiamondPhiMerge) {
  // Outer diamond.
  StartBlock();
  EndBlock(Branch(Imm(), 1, 5));

  // Diamond 1
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  auto ll = Define(Reg());
  EndBlock(Jump(2));

  StartBlock();
  auto lr = Define(Reg());
  EndBlock();

  StartBlock();
  auto l_phi = Phi(ll, lr);
  EndBlock(Jump(5));

  // Diamond 2
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  auto rl = Define(Reg());
  EndBlock(Jump(2));

  StartBlock();
  auto rr = Define(Reg());
  EndBlock();

  StartBlock();
  auto r_phi = Phi(rl, rr);
  EndBlock();

  // Outer diamond merge.
  StartBlock();
  auto phi = Phi(l_phi, r_phi);
  Return(Reg(phi));
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, NestedDiamondPhiMergeDifferent) {
  // Outer diamond.
  StartBlock();
  EndBlock(Branch(Imm(), 1, 5));

  // Diamond 1
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  auto ll = Define(Reg(0));
  EndBlock(Jump(2));

  StartBlock();
  auto lr = Define(Reg(1));
  EndBlock();

  StartBlock();
  auto l_phi = Phi(ll, lr);
  EndBlock(Jump(5));

  // Diamond 2
  StartBlock();
  EndBlock(Branch(Imm(), 1, 2));

  StartBlock();
  auto rl = Define(Reg(2));
  EndBlock(Jump(2));

  StartBlock();
  auto rr = Define(Reg(3));
  EndBlock();

  StartBlock();
  auto r_phi = Phi(rl, rr);
  EndBlock();

  // Outer diamond merge.
  StartBlock();
  auto phi = Phi(l_phi, r_phi);
  Return(Reg(phi));
  EndBlock();

  Allocate();
}


TEST_F(RegisterAllocatorTest, RegressionSplitBeforeAndMove) {
  StartBlock();

  // Fill registers.
  VReg values[kDefaultNRegs];
  for (size_t i = 0; i < arraysize(values); ++i) {
    if (i == 0 || i == 1) continue;  // Leave a hole for c_1 to take.
    values[i] = Define(Reg(static_cast<int>(i)));
  }

  auto c_0 = DefineConstant();
  auto c_1 = DefineConstant();

  EmitOI(Reg(1), Reg(c_0, 0), UniqueReg(c_1));

  // Use previous values to force c_1 to split before the previous instruction.
  for (size_t i = 0; i < arraysize(values); ++i) {
    if (i == 0 || i == 1) continue;
    EmitI(Reg(values[i], static_cast<int>(i)));
  }

  EndBlock(Last());

  Allocate();
}


TEST_F(RegisterAllocatorTest, RegressionSpillTwice) {
  StartBlock();
  auto p_0 = Parameter(Reg(1));
  EmitCall(Slot(-2), Unique(p_0), Reg(p_0, 1));
  EndBlock(Last());

  Allocate();
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
