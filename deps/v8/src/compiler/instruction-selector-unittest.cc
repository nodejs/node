// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/instruction-selector-unittest.h"

#include "src/compiler/compiler-test-utils.h"
#include "src/flags.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

typedef RawMachineAssembler::Label MLabel;

}  // namespace


InstructionSelectorTest::InstructionSelectorTest() : rng_(FLAG_random_seed) {}


InstructionSelectorTest::~InstructionSelectorTest() {}


InstructionSelectorTest::Stream InstructionSelectorTest::StreamBuilder::Build(
    InstructionSelector::Features features,
    InstructionSelectorTest::StreamBuilderMode mode) {
  Schedule* schedule = Export();
  if (FLAG_trace_turbo) {
    OFStream out(stdout);
    out << "=== Schedule before instruction selection ===" << endl << *schedule;
  }
  EXPECT_NE(0, graph()->NodeCount());
  CompilationInfo info(test_->isolate(), test_->zone());
  Linkage linkage(&info, call_descriptor());
  InstructionSequence sequence(&linkage, graph(), schedule);
  SourcePositionTable source_position_table(graph());
  InstructionSelector selector(&sequence, &source_position_table, features);
  selector.SelectInstructions();
  if (FLAG_trace_turbo) {
    OFStream out(stdout);
    out << "=== Code sequence after instruction selection ===" << endl
        << sequence;
  }
  Stream s;
  std::set<int> virtual_registers;
  for (InstructionSequence::const_iterator i = sequence.begin();
       i != sequence.end(); ++i) {
    Instruction* instr = *i;
    if (instr->opcode() < 0) continue;
    if (mode == kTargetInstructions) {
      switch (instr->arch_opcode()) {
#define CASE(Name) \
  case k##Name:    \
    break;
        TARGET_ARCH_OPCODE_LIST(CASE)
#undef CASE
        default:
          continue;
      }
    }
    if (mode == kAllExceptNopInstructions && instr->arch_opcode() == kArchNop) {
      continue;
    }
    for (size_t i = 0; i < instr->OutputCount(); ++i) {
      InstructionOperand* output = instr->OutputAt(i);
      EXPECT_NE(InstructionOperand::IMMEDIATE, output->kind());
      if (output->IsConstant()) {
        s.constants_.insert(std::make_pair(
            output->index(), sequence.GetConstant(output->index())));
        virtual_registers.insert(output->index());
      } else if (output->IsUnallocated()) {
        virtual_registers.insert(
            UnallocatedOperand::cast(output)->virtual_register());
      }
    }
    for (size_t i = 0; i < instr->InputCount(); ++i) {
      InstructionOperand* input = instr->InputAt(i);
      EXPECT_NE(InstructionOperand::CONSTANT, input->kind());
      if (input->IsImmediate()) {
        s.immediates_.insert(std::make_pair(
            input->index(), sequence.GetImmediate(input->index())));
      } else if (input->IsUnallocated()) {
        virtual_registers.insert(
            UnallocatedOperand::cast(input)->virtual_register());
      }
    }
    s.instructions_.push_back(instr);
  }
  for (std::set<int>::const_iterator i = virtual_registers.begin();
       i != virtual_registers.end(); ++i) {
    int virtual_register = *i;
    if (sequence.IsDouble(virtual_register)) {
      EXPECT_FALSE(sequence.IsReference(virtual_register));
      s.doubles_.insert(virtual_register);
    }
    if (sequence.IsReference(virtual_register)) {
      EXPECT_FALSE(sequence.IsDouble(virtual_register));
      s.references_.insert(virtual_register);
    }
  }
  for (int i = 0; i < sequence.GetFrameStateDescriptorCount(); i++) {
    s.deoptimization_entries_.push_back(sequence.GetFrameStateDescriptor(
        InstructionSequence::StateId::FromInt(i)));
  }
  return s;
}


// -----------------------------------------------------------------------------
// Return.


TARGET_TEST_F(InstructionSelectorTest, ReturnFloat32Constant) {
  const float kValue = 4.2f;
  StreamBuilder m(this, kMachFloat32);
  m.Return(m.Float32Constant(kValue));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(InstructionOperand::CONSTANT, s[0]->OutputAt(0)->kind());
  EXPECT_FLOAT_EQ(kValue, s.ToFloat32(s[0]->OutputAt(0)));
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
}


TARGET_TEST_F(InstructionSelectorTest, ReturnParameter) {
  StreamBuilder m(this, kMachInt32, kMachInt32);
  m.Return(m.Parameter(0));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
}


TARGET_TEST_F(InstructionSelectorTest, ReturnZero) {
  StreamBuilder m(this, kMachInt32);
  m.Return(m.Int32Constant(0));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(2U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(InstructionOperand::CONSTANT, s[0]->OutputAt(0)->kind());
  EXPECT_EQ(0, s.ToInt32(s[0]->OutputAt(0)));
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
}


// -----------------------------------------------------------------------------
// Conversions.


TARGET_TEST_F(InstructionSelectorTest, TruncateFloat64ToInt32WithParameter) {
  StreamBuilder m(this, kMachInt32, kMachFloat64);
  m.Return(m.TruncateFloat64ToInt32(m.Parameter(0)));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  EXPECT_EQ(kArchTruncateDoubleToI, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
  EXPECT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArchRet, s[2]->arch_opcode());
}


// -----------------------------------------------------------------------------
// Parameters.


TARGET_TEST_F(InstructionSelectorTest, DoubleParameter) {
  StreamBuilder m(this, kMachFloat64, kMachFloat64);
  Node* param = m.Parameter(0);
  m.Return(param);
  Stream s = m.Build(kAllInstructions);
  EXPECT_TRUE(s.IsDouble(param->id()));
}


TARGET_TEST_F(InstructionSelectorTest, ReferenceParameter) {
  StreamBuilder m(this, kMachAnyTagged, kMachAnyTagged);
  Node* param = m.Parameter(0);
  m.Return(param);
  Stream s = m.Build(kAllInstructions);
  EXPECT_TRUE(s.IsReference(param->id()));
}


// -----------------------------------------------------------------------------
// Finish.


TARGET_TEST_F(InstructionSelectorTest, Finish) {
  StreamBuilder m(this, kMachAnyTagged, kMachAnyTagged);
  Node* param = m.Parameter(0);
  Node* finish = m.NewNode(m.common()->Finish(1), param, m.graph()->start());
  m.Return(finish);
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  ASSERT_TRUE(s[0]->Output()->IsUnallocated());
  EXPECT_EQ(param->id(), s.ToVreg(s[0]->Output()));
  EXPECT_EQ(kArchNop, s[1]->arch_opcode());
  ASSERT_EQ(1U, s[1]->InputCount());
  ASSERT_TRUE(s[1]->InputAt(0)->IsUnallocated());
  EXPECT_EQ(param->id(), s.ToVreg(s[1]->InputAt(0)));
  ASSERT_EQ(1U, s[1]->OutputCount());
  ASSERT_TRUE(s[1]->Output()->IsUnallocated());
  EXPECT_TRUE(UnallocatedOperand::cast(s[1]->Output())->HasSameAsInputPolicy());
  EXPECT_EQ(finish->id(), s.ToVreg(s[1]->Output()));
  EXPECT_TRUE(s.IsReference(finish->id()));
}


// -----------------------------------------------------------------------------
// Phi.


typedef InstructionSelectorTestWithParam<MachineType>
    InstructionSelectorPhiTest;


TARGET_TEST_P(InstructionSelectorPhiTest, Doubleness) {
  const MachineType type = GetParam();
  StreamBuilder m(this, type, type, type);
  Node* param0 = m.Parameter(0);
  Node* param1 = m.Parameter(1);
  MLabel a, b, c;
  m.Branch(m.Int32Constant(0), &a, &b);
  m.Bind(&a);
  m.Goto(&c);
  m.Bind(&b);
  m.Goto(&c);
  m.Bind(&c);
  Node* phi = m.Phi(type, param0, param1);
  m.Return(phi);
  Stream s = m.Build(kAllInstructions);
  EXPECT_EQ(s.IsDouble(phi->id()), s.IsDouble(param0->id()));
  EXPECT_EQ(s.IsDouble(phi->id()), s.IsDouble(param1->id()));
}


TARGET_TEST_P(InstructionSelectorPhiTest, Referenceness) {
  const MachineType type = GetParam();
  StreamBuilder m(this, type, type, type);
  Node* param0 = m.Parameter(0);
  Node* param1 = m.Parameter(1);
  MLabel a, b, c;
  m.Branch(m.Int32Constant(1), &a, &b);
  m.Bind(&a);
  m.Goto(&c);
  m.Bind(&b);
  m.Goto(&c);
  m.Bind(&c);
  Node* phi = m.Phi(type, param0, param1);
  m.Return(phi);
  Stream s = m.Build(kAllInstructions);
  EXPECT_EQ(s.IsReference(phi->id()), s.IsReference(param0->id()));
  EXPECT_EQ(s.IsReference(phi->id()), s.IsReference(param1->id()));
}


INSTANTIATE_TEST_CASE_P(InstructionSelectorTest, InstructionSelectorPhiTest,
                        ::testing::Values(kMachFloat64, kMachInt8, kMachUint8,
                                          kMachInt16, kMachUint16, kMachInt32,
                                          kMachUint32, kMachInt64, kMachUint64,
                                          kMachPtr, kMachAnyTagged));


// -----------------------------------------------------------------------------
// ValueEffect.


TARGET_TEST_F(InstructionSelectorTest, ValueEffect) {
  StreamBuilder m1(this, kMachInt32, kMachPtr);
  Node* p1 = m1.Parameter(0);
  m1.Return(m1.Load(kMachInt32, p1, m1.Int32Constant(0)));
  Stream s1 = m1.Build(kAllInstructions);
  StreamBuilder m2(this, kMachInt32, kMachPtr);
  Node* p2 = m2.Parameter(0);
  m2.Return(m2.NewNode(m2.machine()->Load(kMachInt32), p2, m2.Int32Constant(0),
                       m2.NewNode(m2.common()->ValueEffect(1), p2)));
  Stream s2 = m2.Build(kAllInstructions);
  EXPECT_LE(3U, s1.size());
  ASSERT_EQ(s1.size(), s2.size());
  TRACED_FORRANGE(size_t, i, 0, s1.size() - 1) {
    const Instruction* i1 = s1[i];
    const Instruction* i2 = s2[i];
    EXPECT_EQ(i1->arch_opcode(), i2->arch_opcode());
    EXPECT_EQ(i1->InputCount(), i2->InputCount());
    EXPECT_EQ(i1->OutputCount(), i2->OutputCount());
  }
}


// -----------------------------------------------------------------------------
// Calls with deoptimization.


TARGET_TEST_F(InstructionSelectorTest, CallJSFunctionWithDeopt) {
  StreamBuilder m(this, kMachAnyTagged, kMachAnyTagged, kMachAnyTagged,
                  kMachAnyTagged);

  BailoutId bailout_id(42);

  Node* function_node = m.Parameter(0);
  Node* receiver = m.Parameter(1);
  Node* context = m.Parameter(2);

  Node* parameters = m.NewNode(m.common()->StateValues(1), m.Int32Constant(1));
  Node* locals = m.NewNode(m.common()->StateValues(0));
  Node* stack = m.NewNode(m.common()->StateValues(0));
  Node* context_dummy = m.Int32Constant(0);

  Node* state_node = m.NewNode(
      m.common()->FrameState(JS_FRAME, bailout_id, kPushOutput), parameters,
      locals, stack, context_dummy, m.UndefinedConstant());
  Node* call = m.CallJS0(function_node, receiver, context, state_node);
  m.Return(call);

  Stream s = m.Build(kAllExceptNopInstructions);

  // Skip until kArchCallJSFunction.
  size_t index = 0;
  for (; index < s.size() && s[index]->arch_opcode() != kArchCallJSFunction;
       index++) {
  }
  // Now we should have two instructions: call and return.
  ASSERT_EQ(index + 2, s.size());

  EXPECT_EQ(kArchCallJSFunction, s[index++]->arch_opcode());
  EXPECT_EQ(kArchRet, s[index++]->arch_opcode());

  // TODO(jarin) Check deoptimization table.
}


TARGET_TEST_F(InstructionSelectorTest, CallFunctionStubWithDeopt) {
  StreamBuilder m(this, kMachAnyTagged, kMachAnyTagged, kMachAnyTagged,
                  kMachAnyTagged);

  BailoutId bailout_id_before(42);

  // Some arguments for the call node.
  Node* function_node = m.Parameter(0);
  Node* receiver = m.Parameter(1);
  Node* context = m.Int32Constant(1);  // Context is ignored.

  // Build frame state for the state before the call.
  Node* parameters = m.NewNode(m.common()->StateValues(1), m.Int32Constant(43));
  Node* locals = m.NewNode(m.common()->StateValues(1), m.Int32Constant(44));
  Node* stack = m.NewNode(m.common()->StateValues(1), m.Int32Constant(45));

  Node* context_sentinel = m.Int32Constant(0);
  Node* frame_state_before = m.NewNode(
      m.common()->FrameState(JS_FRAME, bailout_id_before, kPushOutput),
      parameters, locals, stack, context_sentinel, m.UndefinedConstant());

  // Build the call.
  Node* call = m.CallFunctionStub0(function_node, receiver, context,
                                   frame_state_before, CALL_AS_METHOD);

  m.Return(call);

  Stream s = m.Build(kAllExceptNopInstructions);

  // Skip until kArchCallJSFunction.
  size_t index = 0;
  for (; index < s.size() && s[index]->arch_opcode() != kArchCallCodeObject;
       index++) {
  }
  // Now we should have two instructions: call, return.
  ASSERT_EQ(index + 2, s.size());

  // Check the call instruction
  const Instruction* call_instr = s[index++];
  EXPECT_EQ(kArchCallCodeObject, call_instr->arch_opcode());
  size_t num_operands =
      1 +  // Code object.
      1 +
      4 +  // Frame state deopt id + one input for each value in frame state.
      1 +  // Function.
      1;   // Context.
  ASSERT_EQ(num_operands, call_instr->InputCount());

  // Code object.
  EXPECT_TRUE(call_instr->InputAt(0)->IsImmediate());

  // Deoptimization id.
  int32_t deopt_id_before = s.ToInt32(call_instr->InputAt(1));
  FrameStateDescriptor* desc_before =
      s.GetFrameStateDescriptor(deopt_id_before);
  EXPECT_EQ(bailout_id_before, desc_before->bailout_id());
  EXPECT_EQ(kPushOutput, desc_before->state_combine());
  EXPECT_EQ(1u, desc_before->parameters_count());
  EXPECT_EQ(1u, desc_before->locals_count());
  EXPECT_EQ(1u, desc_before->stack_count());
  EXPECT_EQ(43, s.ToInt32(call_instr->InputAt(2)));
  EXPECT_EQ(0, s.ToInt32(call_instr->InputAt(3)));
  EXPECT_EQ(44, s.ToInt32(call_instr->InputAt(4)));
  EXPECT_EQ(45, s.ToInt32(call_instr->InputAt(5)));

  // Function.
  EXPECT_EQ(function_node->id(), s.ToVreg(call_instr->InputAt(6)));
  // Context.
  EXPECT_EQ(context->id(), s.ToVreg(call_instr->InputAt(7)));

  EXPECT_EQ(kArchRet, s[index++]->arch_opcode());

  EXPECT_EQ(index, s.size());
}


TARGET_TEST_F(InstructionSelectorTest,
              CallFunctionStubDeoptRecursiveFrameState) {
  StreamBuilder m(this, kMachAnyTagged, kMachAnyTagged, kMachAnyTagged,
                  kMachAnyTagged);

  BailoutId bailout_id_before(42);
  BailoutId bailout_id_parent(62);

  // Some arguments for the call node.
  Node* function_node = m.Parameter(0);
  Node* receiver = m.Parameter(1);
  Node* context = m.Int32Constant(66);

  // Build frame state for the state before the call.
  Node* parameters = m.NewNode(m.common()->StateValues(1), m.Int32Constant(63));
  Node* locals = m.NewNode(m.common()->StateValues(1), m.Int32Constant(64));
  Node* stack = m.NewNode(m.common()->StateValues(1), m.Int32Constant(65));
  Node* frame_state_parent = m.NewNode(
      m.common()->FrameState(JS_FRAME, bailout_id_parent, kIgnoreOutput),
      parameters, locals, stack, context, m.UndefinedConstant());

  Node* context2 = m.Int32Constant(46);
  Node* parameters2 =
      m.NewNode(m.common()->StateValues(1), m.Int32Constant(43));
  Node* locals2 = m.NewNode(m.common()->StateValues(1), m.Int32Constant(44));
  Node* stack2 = m.NewNode(m.common()->StateValues(1), m.Int32Constant(45));
  Node* frame_state_before = m.NewNode(
      m.common()->FrameState(JS_FRAME, bailout_id_before, kPushOutput),
      parameters2, locals2, stack2, context2, frame_state_parent);

  // Build the call.
  Node* call = m.CallFunctionStub0(function_node, receiver, context2,
                                   frame_state_before, CALL_AS_METHOD);

  m.Return(call);

  Stream s = m.Build(kAllExceptNopInstructions);

  // Skip until kArchCallJSFunction.
  size_t index = 0;
  for (; index < s.size() && s[index]->arch_opcode() != kArchCallCodeObject;
       index++) {
  }
  // Now we should have three instructions: call, return.
  EXPECT_EQ(index + 2, s.size());

  // Check the call instruction
  const Instruction* call_instr = s[index++];
  EXPECT_EQ(kArchCallCodeObject, call_instr->arch_opcode());
  size_t num_operands =
      1 +  // Code object.
      1 +  // Frame state deopt id
      4 +  // One input for each value in frame state + context.
      4 +  // One input for each value in the parent frame state + context.
      1 +  // Function.
      1;   // Context.
  EXPECT_EQ(num_operands, call_instr->InputCount());
  // Code object.
  EXPECT_TRUE(call_instr->InputAt(0)->IsImmediate());

  // Deoptimization id.
  int32_t deopt_id_before = s.ToInt32(call_instr->InputAt(1));
  FrameStateDescriptor* desc_before =
      s.GetFrameStateDescriptor(deopt_id_before);
  EXPECT_EQ(bailout_id_before, desc_before->bailout_id());
  EXPECT_EQ(1u, desc_before->parameters_count());
  EXPECT_EQ(1u, desc_before->locals_count());
  EXPECT_EQ(1u, desc_before->stack_count());
  EXPECT_EQ(63, s.ToInt32(call_instr->InputAt(2)));
  // Context:
  EXPECT_EQ(66, s.ToInt32(call_instr->InputAt(3)));
  EXPECT_EQ(64, s.ToInt32(call_instr->InputAt(4)));
  EXPECT_EQ(65, s.ToInt32(call_instr->InputAt(5)));
  // Values from parent environment should follow.
  EXPECT_EQ(43, s.ToInt32(call_instr->InputAt(6)));
  EXPECT_EQ(46, s.ToInt32(call_instr->InputAt(7)));
  EXPECT_EQ(44, s.ToInt32(call_instr->InputAt(8)));
  EXPECT_EQ(45, s.ToInt32(call_instr->InputAt(9)));

  // Function.
  EXPECT_EQ(function_node->id(), s.ToVreg(call_instr->InputAt(10)));
  // Context.
  EXPECT_EQ(context2->id(), s.ToVreg(call_instr->InputAt(11)));
  // Continuation.

  EXPECT_EQ(kArchRet, s[index++]->arch_opcode());
  EXPECT_EQ(index, s.size());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
