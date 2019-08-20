// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/compiler/backend/instruction-selector-unittest.h"

#include "src/codegen/code-factory.h"
#include "src/codegen/tick-counter.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph.h"
#include "src/compiler/schedule.h"
#include "src/flags/flags.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

InstructionSelectorTest::InstructionSelectorTest() : rng_(FLAG_random_seed) {}

InstructionSelectorTest::~InstructionSelectorTest() = default;

InstructionSelectorTest::Stream InstructionSelectorTest::StreamBuilder::Build(
    InstructionSelector::Features features,
    InstructionSelectorTest::StreamBuilderMode mode,
    InstructionSelector::SourcePositionMode source_position_mode) {
  Schedule* schedule = Export();
  if (FLAG_trace_turbo) {
    StdoutStream{} << "=== Schedule before instruction selection ==="
                   << std::endl
                   << *schedule;
  }
  size_t const node_count = graph()->NodeCount();
  EXPECT_NE(0u, node_count);
  Linkage linkage(call_descriptor());
  InstructionBlocks* instruction_blocks =
      InstructionSequence::InstructionBlocksFor(test_->zone(), schedule);
  InstructionSequence sequence(test_->isolate(), test_->zone(),
                               instruction_blocks);
  SourcePositionTable source_position_table(graph());
  TickCounter tick_counter;
  InstructionSelector selector(
      test_->zone(), node_count, &linkage, &sequence, schedule,
      &source_position_table, nullptr,
      InstructionSelector::kEnableSwitchJumpTable, &tick_counter,
      source_position_mode, features, InstructionSelector::kDisableScheduling,
      InstructionSelector::kEnableRootsRelativeAddressing,
      PoisoningMitigationLevel::kPoisonAll);
  selector.SelectInstructions();
  if (FLAG_trace_turbo) {
    StdoutStream{} << "=== Code sequence after instruction selection ==="
                   << std::endl
                   << sequence;
  }
  Stream s;
  s.virtual_registers_ = selector.GetVirtualRegistersForTesting();
  // Map virtual registers.
  for (Instruction* const instr : sequence) {
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
        int vreg = ConstantOperand::cast(output)->virtual_register();
        s.constants_.insert(std::make_pair(vreg, sequence.GetConstant(vreg)));
      }
    }
    for (size_t i = 0; i < instr->InputCount(); ++i) {
      InstructionOperand* input = instr->InputAt(i);
      EXPECT_NE(InstructionOperand::CONSTANT, input->kind());
      if (input->IsImmediate()) {
        auto imm = ImmediateOperand::cast(input);
        if (imm->type() == ImmediateOperand::INDEXED) {
          int index = imm->indexed_value();
          s.immediates_.insert(
              std::make_pair(index, sequence.GetImmediate(imm)));
        }
      }
    }
    s.instructions_.push_back(instr);
  }
  for (auto i : s.virtual_registers_) {
    int const virtual_register = i.second;
    if (sequence.IsFP(virtual_register)) {
      EXPECT_FALSE(sequence.IsReference(virtual_register));
      s.doubles_.insert(virtual_register);
    }
    if (sequence.IsReference(virtual_register)) {
      EXPECT_FALSE(sequence.IsFP(virtual_register));
      s.references_.insert(virtual_register);
    }
  }
  for (int i = 0; i < sequence.GetDeoptimizationEntryCount(); i++) {
    s.deoptimization_entries_.push_back(
        sequence.GetDeoptimizationEntry(i).descriptor());
  }
  return s;
}

int InstructionSelectorTest::Stream::ToVreg(const Node* node) const {
  VirtualRegisters::const_iterator i = virtual_registers_.find(node->id());
  CHECK(i != virtual_registers_.end());
  return i->second;
}

bool InstructionSelectorTest::Stream::IsFixed(const InstructionOperand* operand,
                                              Register reg) const {
  if (!operand->IsUnallocated()) return false;
  const UnallocatedOperand* unallocated = UnallocatedOperand::cast(operand);
  if (!unallocated->HasFixedRegisterPolicy()) return false;
  return unallocated->fixed_register_index() == reg.code();
}

bool InstructionSelectorTest::Stream::IsSameAsFirst(
    const InstructionOperand* operand) const {
  if (!operand->IsUnallocated()) return false;
  const UnallocatedOperand* unallocated = UnallocatedOperand::cast(operand);
  return unallocated->HasSameAsInputPolicy();
}

bool InstructionSelectorTest::Stream::IsUsedAtStart(
    const InstructionOperand* operand) const {
  if (!operand->IsUnallocated()) return false;
  const UnallocatedOperand* unallocated = UnallocatedOperand::cast(operand);
  return unallocated->IsUsedAtStart();
}

const FrameStateFunctionInfo*
InstructionSelectorTest::StreamBuilder::GetFrameStateFunctionInfo(
    int parameter_count, int local_count) {
  return common()->CreateFrameStateFunctionInfo(
      FrameStateType::kInterpretedFunction, parameter_count, local_count,
      Handle<SharedFunctionInfo>());
}

// -----------------------------------------------------------------------------
// Return.

TARGET_TEST_F(InstructionSelectorTest, ReturnFloat32Constant) {
  const float kValue = 4.2f;
  StreamBuilder m(this, MachineType::Float32());
  m.Return(m.Float32Constant(kValue));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(InstructionOperand::CONSTANT, s[0]->OutputAt(0)->kind());
  EXPECT_FLOAT_EQ(kValue, s.ToFloat32(s[0]->OutputAt(0)));
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(2U, s[1]->InputCount());
}

TARGET_TEST_F(InstructionSelectorTest, ReturnParameter) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Int32());
  m.Return(m.Parameter(0));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(2U, s[1]->InputCount());
}

TARGET_TEST_F(InstructionSelectorTest, ReturnZero) {
  StreamBuilder m(this, MachineType::Int32());
  m.Return(m.Int32Constant(0));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  EXPECT_EQ(InstructionOperand::CONSTANT, s[0]->OutputAt(0)->kind());
  EXPECT_EQ(0, s.ToInt32(s[0]->OutputAt(0)));
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(2U, s[1]->InputCount());
}

// -----------------------------------------------------------------------------
// Conversions.

TARGET_TEST_F(InstructionSelectorTest, TruncateFloat64ToWord32WithParameter) {
  StreamBuilder m(this, MachineType::Int32(), MachineType::Float64());
  m.Return(m.TruncateFloat64ToWord32(m.Parameter(0)));
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(4U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  EXPECT_EQ(kArchTruncateDoubleToI, s[1]->arch_opcode());
  EXPECT_EQ(1U, s[1]->InputCount());
  EXPECT_EQ(1U, s[1]->OutputCount());
  EXPECT_EQ(kArchRet, s[2]->arch_opcode());
}

// -----------------------------------------------------------------------------
// Parameters.

TARGET_TEST_F(InstructionSelectorTest, DoubleParameter) {
  StreamBuilder m(this, MachineType::Float64(), MachineType::Float64());
  Node* param = m.Parameter(0);
  m.Return(param);
  Stream s = m.Build(kAllInstructions);
  EXPECT_TRUE(s.IsDouble(param));
}

TARGET_TEST_F(InstructionSelectorTest, ReferenceParameter) {
  StreamBuilder m(this, MachineType::AnyTagged(), MachineType::AnyTagged());
  Node* param = m.Parameter(0);
  m.Return(param);
  Stream s = m.Build(kAllInstructions);
  EXPECT_TRUE(s.IsReference(param));
}

// -----------------------------------------------------------------------------
// FinishRegion.

TARGET_TEST_F(InstructionSelectorTest, FinishRegion) {
  StreamBuilder m(this, MachineType::AnyTagged(), MachineType::AnyTagged());
  Node* param = m.Parameter(0);
  Node* finish =
      m.AddNode(m.common()->FinishRegion(), param, m.graph()->start());
  m.Return(finish);
  Stream s = m.Build(kAllInstructions);
  ASSERT_EQ(3U, s.size());
  EXPECT_EQ(kArchNop, s[0]->arch_opcode());
  ASSERT_EQ(1U, s[0]->OutputCount());
  ASSERT_TRUE(s[0]->Output()->IsUnallocated());
  EXPECT_EQ(kArchRet, s[1]->arch_opcode());
  EXPECT_EQ(s.ToVreg(param), s.ToVreg(s[0]->Output()));
  EXPECT_EQ(s.ToVreg(param), s.ToVreg(s[1]->InputAt(1)));
  EXPECT_TRUE(s.IsReference(finish));
}

// -----------------------------------------------------------------------------
// Phi.

using InstructionSelectorPhiTest =
    InstructionSelectorTestWithParam<MachineType>;

TARGET_TEST_P(InstructionSelectorPhiTest, Doubleness) {
  const MachineType type = GetParam();
  StreamBuilder m(this, type, type, type);
  Node* param0 = m.Parameter(0);
  Node* param1 = m.Parameter(1);
  RawMachineLabel a, b, c;
  m.Branch(m.Int32Constant(0), &a, &b);
  m.Bind(&a);
  m.Goto(&c);
  m.Bind(&b);
  m.Goto(&c);
  m.Bind(&c);
  Node* phi = m.Phi(type.representation(), param0, param1);
  m.Return(phi);
  Stream s = m.Build(kAllInstructions);
  EXPECT_EQ(s.IsDouble(phi), s.IsDouble(param0));
  EXPECT_EQ(s.IsDouble(phi), s.IsDouble(param1));
}

TARGET_TEST_P(InstructionSelectorPhiTest, Referenceness) {
  const MachineType type = GetParam();
  StreamBuilder m(this, type, type, type);
  Node* param0 = m.Parameter(0);
  Node* param1 = m.Parameter(1);
  RawMachineLabel a, b, c;
  m.Branch(m.Int32Constant(1), &a, &b);
  m.Bind(&a);
  m.Goto(&c);
  m.Bind(&b);
  m.Goto(&c);
  m.Bind(&c);
  Node* phi = m.Phi(type.representation(), param0, param1);
  m.Return(phi);
  Stream s = m.Build(kAllInstructions);
  EXPECT_EQ(s.IsReference(phi), s.IsReference(param0));
  EXPECT_EQ(s.IsReference(phi), s.IsReference(param1));
}

INSTANTIATE_TEST_SUITE_P(
    InstructionSelectorTest, InstructionSelectorPhiTest,
    ::testing::Values(MachineType::Float64(), MachineType::Int8(),
                      MachineType::Uint8(), MachineType::Int16(),
                      MachineType::Uint16(), MachineType::Int32(),
                      MachineType::Uint32(), MachineType::Int64(),
                      MachineType::Uint64(), MachineType::Pointer(),
                      MachineType::AnyTagged()));

// -----------------------------------------------------------------------------
// ValueEffect.

TARGET_TEST_F(InstructionSelectorTest, ValueEffect) {
  StreamBuilder m1(this, MachineType::Int32(), MachineType::Pointer());
  Node* p1 = m1.Parameter(0);
  m1.Return(m1.Load(MachineType::Int32(), p1, m1.Int32Constant(0)));
  Stream s1 = m1.Build(kAllInstructions);
  StreamBuilder m2(this, MachineType::Int32(), MachineType::Pointer());
  Node* p2 = m2.Parameter(0);
  m2.Return(m2.AddNode(
      m2.machine()->Load(MachineType::Int32()), p2, m2.Int32Constant(0),
      m2.AddNode(m2.common()->BeginRegion(RegionObservability::kObservable),
                 m2.graph()->start())));
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
  StreamBuilder m(this, MachineType::AnyTagged(), MachineType::AnyTagged(),
                  MachineType::AnyTagged(), MachineType::AnyTagged());

  BailoutId bailout_id(42);

  Node* function_node = m.Parameter(0);
  Node* receiver = m.Parameter(1);
  Node* context = m.Parameter(2);

  ZoneVector<MachineType> int32_type(1, MachineType::Int32(), zone());
  ZoneVector<MachineType> empty_types(zone());

  auto call_descriptor = Linkage::GetJSCallDescriptor(
      zone(), false, 1,
      CallDescriptor::kNeedsFrameState | CallDescriptor::kCanUseRoots);

  // Build frame state for the state before the call.
  Node* parameters = m.AddNode(
      m.common()->TypedStateValues(&int32_type, SparseInputMask::Dense()),
      m.Int32Constant(1));
  Node* locals = m.AddNode(
      m.common()->TypedStateValues(&empty_types, SparseInputMask::Dense()));
  Node* stack = m.AddNode(
      m.common()->TypedStateValues(&empty_types, SparseInputMask::Dense()));
  Node* context_sentinel = m.Int32Constant(0);
  Node* state_node = m.AddNode(
      m.common()->FrameState(bailout_id, OutputFrameStateCombine::PokeAt(0),
                             m.GetFrameStateFunctionInfo(1, 0)),
      parameters, locals, stack, context_sentinel, function_node,
      m.UndefinedConstant());

  // Build the call.
  Node* nodes[] = {function_node,      receiver, m.UndefinedConstant(),
                   m.Int32Constant(1), context,  state_node};
  Node* call = m.CallNWithFrameState(call_descriptor, arraysize(nodes), nodes);
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

TARGET_TEST_F(InstructionSelectorTest, CallStubWithDeopt) {
  StreamBuilder m(this, MachineType::AnyTagged(), MachineType::AnyTagged(),
                  MachineType::AnyTagged(), MachineType::AnyTagged());

  BailoutId bailout_id_before(42);

  // Some arguments for the call node.
  Node* function_node = m.Parameter(0);
  Node* receiver = m.Parameter(1);
  Node* context = m.Int32Constant(1);  // Context is ignored.

  ZoneVector<MachineType> int32_type(1, MachineType::Int32(), zone());
  ZoneVector<MachineType> float64_type(1, MachineType::Float64(), zone());
  ZoneVector<MachineType> tagged_type(1, MachineType::AnyTagged(), zone());

  Callable callable = Builtins::CallableFor(isolate(), Builtins::kToObject);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), callable.descriptor(), 1, CallDescriptor::kNeedsFrameState,
      Operator::kNoProperties);

  // Build frame state for the state before the call.
  Node* parameters = m.AddNode(
      m.common()->TypedStateValues(&int32_type, SparseInputMask::Dense()),
      m.Int32Constant(43));
  Node* locals = m.AddNode(
      m.common()->TypedStateValues(&float64_type, SparseInputMask::Dense()),
      m.Float64Constant(0.5));
  Node* stack = m.AddNode(
      m.common()->TypedStateValues(&tagged_type, SparseInputMask::Dense()),
      m.UndefinedConstant());
  Node* context_sentinel = m.Int32Constant(0);
  Node* state_node =
      m.AddNode(m.common()->FrameState(bailout_id_before,
                                       OutputFrameStateCombine::PokeAt(0),
                                       m.GetFrameStateFunctionInfo(1, 1)),
                parameters, locals, stack, context_sentinel, function_node,
                m.UndefinedConstant());

  // Build the call.
  Node* stub_code = m.HeapConstant(callable.code());
  Node* nodes[] = {stub_code, function_node, receiver, context, state_node};
  Node* call = m.CallNWithFrameState(call_descriptor, arraysize(nodes), nodes);
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
      1 +  // Poison index
      6 +  // Frame state deopt id + one input for each value in frame state.
      1 +  // Function.
      1;   // Context.
  ASSERT_EQ(num_operands, call_instr->InputCount());

  // Code object.
  EXPECT_TRUE(call_instr->InputAt(0)->IsImmediate());

  // Deoptimization id.
  int32_t deopt_id_before = s.ToInt32(call_instr->InputAt(2));
  FrameStateDescriptor* desc_before =
      s.GetFrameStateDescriptor(deopt_id_before);
  EXPECT_EQ(bailout_id_before, desc_before->bailout_id());
  EXPECT_EQ(1u, desc_before->parameters_count());
  EXPECT_EQ(1u, desc_before->locals_count());
  EXPECT_EQ(1u, desc_before->stack_count());
  EXPECT_EQ(43, s.ToInt32(call_instr->InputAt(4)));
  EXPECT_EQ(0, s.ToInt32(call_instr->InputAt(5)));  // This should be a context.
                                                    // We inserted 0 here.
  EXPECT_EQ(0.5, s.ToFloat64(call_instr->InputAt(6)));
  EXPECT_TRUE(s.ToHeapObject(call_instr->InputAt(7))->IsUndefined(isolate()));

  // Function.
  EXPECT_EQ(s.ToVreg(function_node), s.ToVreg(call_instr->InputAt(8)));
  // Context.
  EXPECT_EQ(s.ToVreg(context), s.ToVreg(call_instr->InputAt(9)));

  EXPECT_EQ(kArchRet, s[index++]->arch_opcode());

  EXPECT_EQ(index, s.size());
}

TARGET_TEST_F(InstructionSelectorTest, CallStubWithDeoptRecursiveFrameState) {
  StreamBuilder m(this, MachineType::AnyTagged(), MachineType::AnyTagged(),
                  MachineType::AnyTagged(), MachineType::AnyTagged());

  BailoutId bailout_id_before(42);
  BailoutId bailout_id_parent(62);

  // Some arguments for the call node.
  Node* function_node = m.Parameter(0);
  Node* receiver = m.Parameter(1);
  Node* context = m.Int32Constant(66);
  Node* context2 = m.Int32Constant(46);

  ZoneVector<MachineType> int32_type(1, MachineType::Int32(), zone());
  ZoneVector<MachineType> int32x2_type(2, MachineType::Int32(), zone());
  ZoneVector<MachineType> float64_type(1, MachineType::Float64(), zone());

  Callable callable = Builtins::CallableFor(isolate(), Builtins::kToObject);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), callable.descriptor(), 1, CallDescriptor::kNeedsFrameState,
      Operator::kNoProperties);

  // Build frame state for the state before the call.
  Node* parameters = m.AddNode(
      m.common()->TypedStateValues(&int32_type, SparseInputMask::Dense()),
      m.Int32Constant(63));
  Node* locals = m.AddNode(
      m.common()->TypedStateValues(&int32_type, SparseInputMask::Dense()),
      m.Int32Constant(64));
  Node* stack = m.AddNode(
      m.common()->TypedStateValues(&int32_type, SparseInputMask::Dense()),
      m.Int32Constant(65));
  Node* frame_state_parent = m.AddNode(
      m.common()->FrameState(bailout_id_parent,
                             OutputFrameStateCombine::Ignore(),
                             m.GetFrameStateFunctionInfo(1, 1)),
      parameters, locals, stack, context, function_node, m.UndefinedConstant());

  Node* parameters2 = m.AddNode(
      m.common()->TypedStateValues(&int32_type, SparseInputMask::Dense()),
      m.Int32Constant(43));
  Node* locals2 = m.AddNode(
      m.common()->TypedStateValues(&float64_type, SparseInputMask::Dense()),
      m.Float64Constant(0.25));
  Node* stack2 = m.AddNode(
      m.common()->TypedStateValues(&int32x2_type, SparseInputMask::Dense()),
      m.Int32Constant(44), m.Int32Constant(45));
  Node* state_node =
      m.AddNode(m.common()->FrameState(bailout_id_before,
                                       OutputFrameStateCombine::PokeAt(0),
                                       m.GetFrameStateFunctionInfo(1, 1)),
                parameters2, locals2, stack2, context2, function_node,
                frame_state_parent);

  // Build the call.
  Node* stub_code = m.HeapConstant(callable.code());
  Node* nodes[] = {stub_code, function_node, receiver, context2, state_node};
  Node* call = m.CallNWithFrameState(call_descriptor, arraysize(nodes), nodes);
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
      1 +  // Poison index.
      1 +  // Frame state deopt id
      6 +  // One input for each value in frame state + context.
      5 +  // One input for each value in the parent frame state + context.
      1 +  // Function.
      1;   // Context.
  EXPECT_EQ(num_operands, call_instr->InputCount());
  // Code object.
  EXPECT_TRUE(call_instr->InputAt(0)->IsImmediate());

  // Deoptimization id.
  int32_t deopt_id_before = s.ToInt32(call_instr->InputAt(2));
  FrameStateDescriptor* desc_before =
      s.GetFrameStateDescriptor(deopt_id_before);
  FrameStateDescriptor* desc_before_outer = desc_before->outer_state();
  EXPECT_EQ(bailout_id_before, desc_before->bailout_id());
  EXPECT_EQ(1u, desc_before_outer->parameters_count());
  EXPECT_EQ(1u, desc_before_outer->locals_count());
  EXPECT_EQ(1u, desc_before_outer->stack_count());
  // Values from parent environment.
  EXPECT_EQ(63, s.ToInt32(call_instr->InputAt(4)));
  // Context:
  EXPECT_EQ(66, s.ToInt32(call_instr->InputAt(5)));
  EXPECT_EQ(64, s.ToInt32(call_instr->InputAt(6)));
  EXPECT_EQ(65, s.ToInt32(call_instr->InputAt(7)));
  // Values from the nested frame.
  EXPECT_EQ(1u, desc_before->parameters_count());
  EXPECT_EQ(1u, desc_before->locals_count());
  EXPECT_EQ(2u, desc_before->stack_count());
  EXPECT_EQ(43, s.ToInt32(call_instr->InputAt(9)));
  EXPECT_EQ(46, s.ToInt32(call_instr->InputAt(10)));
  EXPECT_EQ(0.25, s.ToFloat64(call_instr->InputAt(11)));
  EXPECT_EQ(44, s.ToInt32(call_instr->InputAt(12)));
  EXPECT_EQ(45, s.ToInt32(call_instr->InputAt(13)));

  // Function.
  EXPECT_EQ(s.ToVreg(function_node), s.ToVreg(call_instr->InputAt(14)));
  // Context.
  EXPECT_EQ(s.ToVreg(context2), s.ToVreg(call_instr->InputAt(15)));
  // Continuation.

  EXPECT_EQ(kArchRet, s[index++]->arch_opcode());
  EXPECT_EQ(index, s.size());
}

// Helper to make calls to private InstructionSelector shuffle functions.
class InstructionSelectorShuffleTest : public ::testing::Test {
 public:
  using Shuffle = std::array<uint8_t, kSimd128Size>;

  struct TestShuffle {
    Shuffle non_canonical;
    Shuffle canonical;
    bool needs_swap;
    bool is_swizzle;
  };

  // Call testing members in InstructionSelector.
  static void CanonicalizeShuffle(bool inputs_equal, Shuffle* shuffle,
                                  bool* needs_swap, bool* is_swizzle) {
    InstructionSelector::CanonicalizeShuffleForTesting(
        inputs_equal, &(*shuffle)[0], needs_swap, is_swizzle);
  }

  static bool TryMatchIdentity(const Shuffle& shuffle) {
    return InstructionSelector::TryMatchIdentityForTesting(&shuffle[0]);
  }
  template <int LANES>
  static bool TryMatchDup(const Shuffle& shuffle, int* index) {
    return InstructionSelector::TryMatchDupForTesting<LANES>(&shuffle[0],
                                                             index);
  }
  static bool TryMatch32x4Shuffle(const Shuffle& shuffle,
                                  uint8_t* shuffle32x4) {
    return InstructionSelector::TryMatch32x4ShuffleForTesting(&shuffle[0],
                                                              shuffle32x4);
  }
  static bool TryMatch16x8Shuffle(const Shuffle& shuffle,
                                  uint8_t* shuffle16x8) {
    return InstructionSelector::TryMatch16x8ShuffleForTesting(&shuffle[0],
                                                              shuffle16x8);
  }
  static bool TryMatchConcat(const Shuffle& shuffle, uint8_t* offset) {
    return InstructionSelector::TryMatchConcatForTesting(&shuffle[0], offset);
  }
  static bool TryMatchBlend(const Shuffle& shuffle) {
    return InstructionSelector::TryMatchBlendForTesting(&shuffle[0]);
  }
};

bool operator==(const InstructionSelectorShuffleTest::Shuffle& a,
                const InstructionSelectorShuffleTest::Shuffle& b) {
  for (int i = 0; i < kSimd128Size; ++i) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

TEST_F(InstructionSelectorShuffleTest, CanonicalizeShuffle) {
  const bool kInputsEqual = true;
  const bool kNeedsSwap = true;
  const bool kIsSwizzle = true;

  bool needs_swap;
  bool is_swizzle;

  // Test canonicalization driven by input shuffle.
  TestShuffle test_shuffles[] = {
      // Identity is canonical.
      {{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
       {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
       !kNeedsSwap,
       kIsSwizzle},
      // Non-canonical identity requires a swap.
      {{{16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}},
       {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
       kNeedsSwap,
       kIsSwizzle},
      // General shuffle, canonical is unchanged.
      {{{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}},
       {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}},
       !kNeedsSwap,
       !kIsSwizzle},
      // Non-canonical shuffle requires a swap.
      {{{16, 0, 17, 1, 18, 2, 19, 3, 20, 4, 21, 5, 22, 6, 23, 7}},
       {{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}},
       kNeedsSwap,
       !kIsSwizzle},
  };
  for (size_t i = 0; i < arraysize(test_shuffles); ++i) {
    Shuffle shuffle = test_shuffles[i].non_canonical;
    CanonicalizeShuffle(!kInputsEqual, &shuffle, &needs_swap, &is_swizzle);
    EXPECT_EQ(shuffle, test_shuffles[i].canonical);
    EXPECT_EQ(needs_swap, test_shuffles[i].needs_swap);
    EXPECT_EQ(is_swizzle, test_shuffles[i].is_swizzle);
  }

  // Test canonicalization when inputs are equal (explicit swizzle).
  TestShuffle test_swizzles[] = {
      // Identity is canonical.
      {{{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
       {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
       !kNeedsSwap,
       kIsSwizzle},
      // Non-canonical identity requires a swap.
      {{{16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}},
       {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
       !kNeedsSwap,
       kIsSwizzle},
      // Canonicalized to swizzle.
      {{{0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23}},
       {{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}},
       !kNeedsSwap,
       kIsSwizzle},
      // Canonicalized to swizzle.
      {{{16, 0, 17, 1, 18, 2, 19, 3, 20, 4, 21, 5, 22, 6, 23, 7}},
       {{0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7}},
       !kNeedsSwap,
       kIsSwizzle},
  };
  for (size_t i = 0; i < arraysize(test_swizzles); ++i) {
    Shuffle shuffle = test_swizzles[i].non_canonical;
    CanonicalizeShuffle(kInputsEqual, &shuffle, &needs_swap, &is_swizzle);
    EXPECT_EQ(shuffle, test_swizzles[i].canonical);
    EXPECT_EQ(needs_swap, test_swizzles[i].needs_swap);
    EXPECT_EQ(is_swizzle, test_swizzles[i].is_swizzle);
  }
}

TEST_F(InstructionSelectorShuffleTest, TryMatchIdentity) {
  // Match shuffle that returns first source operand.
  EXPECT_TRUE(TryMatchIdentity(
      {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}}));
  // The non-canonicalized identity shuffle doesn't match.
  EXPECT_FALSE(TryMatchIdentity(
      {{16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31}}));
  // Even one lane out of place is not an identity shuffle.
  EXPECT_FALSE(TryMatchIdentity(
      {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 31}}));
}

TEST_F(InstructionSelectorShuffleTest, TryMatchDup) {
  int index;
  // All lanes from the same 32 bit source lane.
  EXPECT_TRUE(TryMatchDup<4>({{4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7}},
                             &index));
  EXPECT_EQ(1, index);
  // It shouldn't match for other vector shapes.
  EXPECT_FALSE(TryMatchDup<8>(
      {{4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7}}, &index));
  EXPECT_FALSE(TryMatchDup<16>(
      {{4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7}}, &index));
  // All lanes from the same 16 bit source lane.
  EXPECT_TRUE(TryMatchDup<8>(
      {{16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17}},
      &index));
  EXPECT_EQ(8, index);
  // It shouldn't match for other vector shapes.
  EXPECT_FALSE(TryMatchDup<4>(
      {{16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17}},
      &index));
  EXPECT_FALSE(TryMatchDup<16>(
      {{16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17, 16, 17}},
      &index));
  // All lanes from the same 8 bit source lane.
  EXPECT_TRUE(TryMatchDup<16>(
      {{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7}}, &index));
  EXPECT_EQ(7, index);
  // It shouldn't match for other vector shapes.
  EXPECT_FALSE(TryMatchDup<4>(
      {{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7}}, &index));
  EXPECT_FALSE(TryMatchDup<8>(
      {{7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7}}, &index));
}

TEST_F(InstructionSelectorShuffleTest, TryMatchConcat) {
  uint8_t offset;
  // Ascending indices, jump at end to same input (concatenating swizzle).
  EXPECT_TRUE(TryMatchConcat(
      {{3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2}}, &offset));
  EXPECT_EQ(3, offset);
  // Ascending indices, jump at end to other input (concatenating shuffle).
  EXPECT_TRUE(TryMatchConcat(
      {{4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}}, &offset));
  EXPECT_EQ(4, offset);

  // Shuffles that should not match:
  // Ascending indices, but jump isn't at end/beginning.
  EXPECT_FALSE(TryMatchConcat(
      {{3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6}}, &offset));
  // Ascending indices, but multiple jumps.
  EXPECT_FALSE(TryMatchConcat(
      {{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}}, &offset));
}

TEST_F(InstructionSelectorShuffleTest, TryMatch32x4Shuffle) {
  uint8_t shuffle32x4[4];
  // Match if each group of 4 bytes is from the same 32 bit lane.
  EXPECT_TRUE(TryMatch32x4Shuffle(
      {{12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 16, 17, 18, 19}},
      shuffle32x4));
  EXPECT_EQ(3, shuffle32x4[0]);
  EXPECT_EQ(2, shuffle32x4[1]);
  EXPECT_EQ(1, shuffle32x4[2]);
  EXPECT_EQ(4, shuffle32x4[3]);
  // Bytes must be in order in the 32 bit lane.
  EXPECT_FALSE(TryMatch32x4Shuffle(
      {{12, 13, 14, 14, 8, 9, 10, 11, 4, 5, 6, 7, 16, 17, 18, 19}},
      shuffle32x4));
  // Each group must start with the first byte in the 32 bit lane.
  EXPECT_FALSE(TryMatch32x4Shuffle(
      {{13, 14, 15, 12, 8, 9, 10, 11, 4, 5, 6, 7, 16, 17, 18, 19}},
      shuffle32x4));
}

TEST_F(InstructionSelectorShuffleTest, TryMatch16x8Shuffle) {
  uint8_t shuffle16x8[8];
  // Match if each group of 2 bytes is from the same 16 bit lane.
  EXPECT_TRUE(TryMatch16x8Shuffle(
      {{12, 13, 30, 31, 8, 9, 26, 27, 4, 5, 22, 23, 16, 17, 2, 3}},
      shuffle16x8));
  EXPECT_EQ(6, shuffle16x8[0]);
  EXPECT_EQ(15, shuffle16x8[1]);
  EXPECT_EQ(4, shuffle16x8[2]);
  EXPECT_EQ(13, shuffle16x8[3]);
  EXPECT_EQ(2, shuffle16x8[4]);
  EXPECT_EQ(11, shuffle16x8[5]);
  EXPECT_EQ(8, shuffle16x8[6]);
  EXPECT_EQ(1, shuffle16x8[7]);
  // Bytes must be in order in the 16 bit lane.
  EXPECT_FALSE(TryMatch16x8Shuffle(
      {{12, 13, 30, 30, 8, 9, 26, 27, 4, 5, 22, 23, 16, 17, 2, 3}},
      shuffle16x8));
  // Each group must start with the first byte in the 16 bit lane.
  EXPECT_FALSE(TryMatch16x8Shuffle(
      {{12, 13, 31, 30, 8, 9, 26, 27, 4, 5, 22, 23, 16, 17, 2, 3}},
      shuffle16x8));
}

TEST_F(InstructionSelectorShuffleTest, TryMatchBlend) {
  // Match if each byte remains in place.
  EXPECT_TRUE(TryMatchBlend(
      {{0, 17, 2, 19, 4, 21, 6, 23, 8, 25, 10, 27, 12, 29, 14, 31}}));
  // Identity is a blend.
  EXPECT_TRUE(
      TryMatchBlend({{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}}));
  // Even one lane out of place is not a blend.
  EXPECT_FALSE(TryMatchBlend(
      {{1, 17, 2, 19, 4, 21, 6, 23, 8, 25, 10, 27, 12, 29, 14, 31}}));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
