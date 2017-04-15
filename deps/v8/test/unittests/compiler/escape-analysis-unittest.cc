// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/escape-analysis.h"
#include "src/bit-vector.h"
#include "src/compiler/escape-analysis-reducer.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/types.h"
#include "src/zone/zone-containers.h"
#include "test/unittests/compiler/graph-unittest.h"

namespace v8 {
namespace internal {
namespace compiler {

class EscapeAnalysisTest : public TypedGraphTest {
 public:
  EscapeAnalysisTest()
      : simplified_(zone()),
        jsgraph_(isolate(), graph(), common(), nullptr, nullptr, nullptr),
        escape_analysis_(graph(), common(), zone()),
        effect_(graph()->start()),
        control_(graph()->start()) {}

  ~EscapeAnalysisTest() {}

  EscapeAnalysis* escape_analysis() { return &escape_analysis_; }

 protected:
  void Analysis() { escape_analysis_.Run(); }

  void Transformation() {
    GraphReducer graph_reducer(zone(), graph());
    EscapeAnalysisReducer escape_reducer(&graph_reducer, &jsgraph_,
                                         &escape_analysis_, zone());
    graph_reducer.AddReducer(&escape_reducer);
    graph_reducer.ReduceGraph();
  }

  // ---------------------------------Node Creation Helper----------------------

  Node* BeginRegion(Node* effect = nullptr) {
    if (!effect) {
      effect = effect_;
    }

    return effect_ = graph()->NewNode(
               common()->BeginRegion(RegionObservability::kObservable), effect);
  }

  Node* FinishRegion(Node* value, Node* effect = nullptr) {
    if (!effect) {
      effect = effect_;
    }
    return effect_ = graph()->NewNode(common()->FinishRegion(), value, effect);
  }

  Node* Allocate(Node* size, Node* effect = nullptr, Node* control = nullptr) {
    if (!effect) {
      effect = effect_;
    }
    if (!control) {
      control = control_;
    }
    return effect_ = graph()->NewNode(simplified()->Allocate(), size, effect,
                                      control);
  }

  Node* Constant(int num) {
    return graph()->NewNode(common()->NumberConstant(num));
  }

  Node* Store(const FieldAccess& access, Node* allocation, Node* value,
              Node* effect = nullptr, Node* control = nullptr) {
    if (!effect) {
      effect = effect_;
    }
    if (!control) {
      control = control_;
    }
    return effect_ = graph()->NewNode(simplified()->StoreField(access),
                                      allocation, value, effect, control);
  }

  Node* StoreElement(const ElementAccess& access, Node* allocation, Node* index,
                     Node* value, Node* effect = nullptr,
                     Node* control = nullptr) {
    if (!effect) {
      effect = effect_;
    }
    if (!control) {
      control = control_;
    }
    return effect_ =
               graph()->NewNode(simplified()->StoreElement(access), allocation,
                                index, value, effect, control);
  }

  Node* Load(const FieldAccess& access, Node* from, Node* effect = nullptr,
             Node* control = nullptr) {
    if (!effect) {
      effect = effect_;
    }
    if (!control) {
      control = control_;
    }
    return graph()->NewNode(simplified()->LoadField(access), from, effect,
                            control);
  }

  Node* Return(Node* value, Node* effect = nullptr, Node* control = nullptr) {
    if (!effect) {
      effect = effect_;
    }
    if (!control) {
      control = control_;
    }
    Node* zero = graph()->NewNode(common()->NumberConstant(0));
    return control_ = graph()->NewNode(common()->Return(), zero, value, effect,
                                       control);
  }

  void EndGraph() {
    for (Edge edge : graph()->end()->input_edges()) {
      if (NodeProperties::IsControlEdge(edge)) {
        edge.UpdateTo(control_);
      }
    }
  }

  Node* Branch() {
    return control_ =
               graph()->NewNode(common()->Branch(), Constant(0), control_);
  }

  Node* IfTrue() {
    return control_ = graph()->NewNode(common()->IfTrue(), control_);
  }

  Node* IfFalse() { return graph()->NewNode(common()->IfFalse(), control_); }

  Node* Merge2(Node* control1, Node* control2) {
    return control_ = graph()->NewNode(common()->Merge(2), control1, control2);
  }

  FieldAccess FieldAccessAtIndex(int offset) {
    FieldAccess access = {kTaggedBase,         offset,
                          MaybeHandle<Name>(), MaybeHandle<Map>(),
                          Type::Any(),         MachineType::AnyTagged(),
                          kFullWriteBarrier};
    return access;
  }

  ElementAccess MakeElementAccess(int header_size) {
    ElementAccess access = {kTaggedBase, header_size, Type::Any(),
                            MachineType::AnyTagged(), kFullWriteBarrier};
    return access;
  }

  // ---------------------------------Assertion Helper--------------------------

  void ExpectReplacement(Node* node, Node* rep) {
    EXPECT_EQ(rep, escape_analysis()->GetReplacement(node));
  }

  void ExpectReplacementPhi(Node* node, Node* left, Node* right) {
    Node* rep = escape_analysis()->GetReplacement(node);
    ASSERT_NE(nullptr, rep);
    ASSERT_EQ(IrOpcode::kPhi, rep->opcode());
    EXPECT_EQ(left, NodeProperties::GetValueInput(rep, 0));
    EXPECT_EQ(right, NodeProperties::GetValueInput(rep, 1));
  }

  void ExpectVirtual(Node* node) {
    EXPECT_TRUE(node->opcode() == IrOpcode::kAllocate ||
                node->opcode() == IrOpcode::kFinishRegion);
    EXPECT_TRUE(escape_analysis()->IsVirtual(node));
  }

  void ExpectEscaped(Node* node) {
    EXPECT_TRUE(node->opcode() == IrOpcode::kAllocate ||
                node->opcode() == IrOpcode::kFinishRegion);
    EXPECT_TRUE(escape_analysis()->IsEscaped(node));
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

  Node* effect() { return effect_; }
  Node* control() { return control_; }

 private:
  SimplifiedOperatorBuilder simplified_;
  JSGraph jsgraph_;
  EscapeAnalysis escape_analysis_;

  Node* effect_;
  Node* control_;
};


// -----------------------------------------------------------------------------
// Test cases.


TEST_F(EscapeAnalysisTest, StraightNonEscape) {
  Node* object1 = Constant(1);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Node* finish = FinishRegion(allocation);
  Node* load = Load(FieldAccessAtIndex(0), finish);
  Node* result = Return(load);
  EndGraph();

  Analysis();

  ExpectVirtual(allocation);
  ExpectReplacement(load, object1);

  Transformation();

  ASSERT_EQ(object1, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, StraightNonEscapeNonConstStore) {
  Node* object1 = Constant(1);
  Node* object2 = Constant(2);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Node* index =
      graph()->NewNode(common()->Select(MachineRepresentation::kTagged),
                       object1, object2, control());
  StoreElement(MakeElementAccess(0), allocation, index, object1);
  Node* finish = FinishRegion(allocation);
  Node* load = Load(FieldAccessAtIndex(0), finish);
  Node* result = Return(load);
  EndGraph();

  Analysis();

  ExpectEscaped(allocation);
  ExpectReplacement(load, nullptr);

  Transformation();

  ASSERT_EQ(load, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, StraightEscape) {
  Node* object1 = Constant(1);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Node* finish = FinishRegion(allocation);
  Node* load = Load(FieldAccessAtIndex(0), finish);
  Node* result = Return(allocation);
  EndGraph();
  graph()->end()->AppendInput(zone(), load);

  Analysis();

  ExpectEscaped(allocation);
  ExpectReplacement(load, object1);

  Transformation();

  ASSERT_EQ(allocation, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, StoreLoadEscape) {
  Node* object1 = Constant(1);

  BeginRegion();
  Node* allocation1 = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation1, object1);
  Node* finish1 = FinishRegion(allocation1);

  BeginRegion();
  Node* allocation2 = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation2, finish1);
  Node* finish2 = FinishRegion(allocation2);

  Node* load = Load(FieldAccessAtIndex(0), finish2);
  Node* result = Return(load);
  EndGraph();
  Analysis();

  ExpectEscaped(allocation1);
  ExpectVirtual(allocation2);
  ExpectReplacement(load, finish1);

  Transformation();

  ASSERT_EQ(finish1, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, BranchNonEscape) {
  Node* object1 = Constant(1);
  Node* object2 = Constant(2);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Node* finish = FinishRegion(allocation);
  Branch();
  Node* ifFalse = IfFalse();
  Node* ifTrue = IfTrue();
  Node* effect1 =
      Store(FieldAccessAtIndex(0), allocation, object1, finish, ifFalse);
  Node* effect2 =
      Store(FieldAccessAtIndex(0), allocation, object2, finish, ifTrue);
  Node* merge = Merge2(ifFalse, ifTrue);
  Node* phi = graph()->NewNode(common()->EffectPhi(2), effect1, effect2, merge);
  Node* load = Load(FieldAccessAtIndex(0), finish, phi, merge);
  Node* result = Return(load, phi);
  EndGraph();
  graph()->end()->AppendInput(zone(), result);

  Analysis();

  ExpectVirtual(allocation);
  ExpectReplacementPhi(load, object1, object2);
  Node* replacement_phi = escape_analysis()->GetReplacement(load);

  Transformation();

  ASSERT_EQ(replacement_phi, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, BranchEscapeOne) {
  Node* object1 = Constant(1);
  Node* object2 = Constant(2);
  Node* index = graph()->NewNode(common()->Parameter(0), start());
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Node* finish = FinishRegion(allocation);
  Branch();
  Node* ifFalse = IfFalse();
  Node* ifTrue = IfTrue();
  Node* effect1 =
      Store(FieldAccessAtIndex(0), allocation, object1, finish, ifFalse);
  Node* effect2 = StoreElement(MakeElementAccess(0), allocation, index, object2,
                               finish, ifTrue);
  Node* merge = Merge2(ifFalse, ifTrue);
  Node* phi = graph()->NewNode(common()->EffectPhi(2), effect1, effect2, merge);
  Node* load = Load(FieldAccessAtIndex(0), finish, phi, merge);
  Node* result = Return(load, phi);
  EndGraph();

  Analysis();

  ExpectEscaped(allocation);
  ExpectReplacement(load, nullptr);

  Transformation();

  ASSERT_EQ(load, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, BranchEscapeThroughStore) {
  Node* object1 = Constant(1);
  Node* object2 = Constant(2);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  FinishRegion(allocation);
  BeginRegion();
  Node* allocation2 = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object2);
  Node* finish2 = FinishRegion(allocation2);
  Branch();
  Node* ifFalse = IfFalse();
  Node* ifTrue = IfTrue();
  Node* effect1 =
      Store(FieldAccessAtIndex(0), allocation, allocation2, finish2, ifFalse);
  Node* merge = Merge2(ifFalse, ifTrue);
  Node* phi = graph()->NewNode(common()->EffectPhi(2), effect1, finish2, merge);
  Node* load = Load(FieldAccessAtIndex(0), finish2, phi, merge);
  Node* result = Return(allocation, phi);
  EndGraph();
  graph()->end()->AppendInput(zone(), load);

  Analysis();

  ExpectEscaped(allocation);
  ExpectEscaped(allocation2);
  ExpectReplacement(load, nullptr);

  Transformation();

  ASSERT_EQ(allocation, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, DanglingLoadOrder) {
  Node* object1 = Constant(1);
  Node* object2 = Constant(2);
  Node* allocation = Allocate(Constant(kPointerSize));
  Node* store1 = Store(FieldAccessAtIndex(0), allocation, object1);
  Node* load1 = Load(FieldAccessAtIndex(0), allocation);
  Node* store2 = Store(FieldAccessAtIndex(0), allocation, object2);
  Node* load2 = Load(FieldAccessAtIndex(0), allocation, store1);
  Node* result = Return(load2);
  EndGraph();
  graph()->end()->AppendInput(zone(), store2);
  graph()->end()->AppendInput(zone(), load1);

  Analysis();

  ExpectVirtual(allocation);
  ExpectReplacement(load1, object1);
  ExpectReplacement(load2, object1);

  Transformation();

  ASSERT_EQ(object1, NodeProperties::GetValueInput(result, 1));
}


TEST_F(EscapeAnalysisTest, DeoptReplacement) {
  Node* object1 = Constant(1);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Node* finish = FinishRegion(allocation);
  Node* effect1 = Store(FieldAccessAtIndex(0), allocation, object1, finish);
  Branch();
  Node* ifFalse = IfFalse();
  Node* state_values1 = graph()->NewNode(
      common()->StateValues(1, SparseInputMask::Dense()), finish);
  Node* state_values2 =
      graph()->NewNode(common()->StateValues(0, SparseInputMask::Dense()));
  Node* state_values3 =
      graph()->NewNode(common()->StateValues(0, SparseInputMask::Dense()));
  Node* frame_state = graph()->NewNode(
      common()->FrameState(BailoutId::None(), OutputFrameStateCombine::Ignore(),
                           nullptr),
      state_values1, state_values2, state_values3, UndefinedConstant(),
      graph()->start(), graph()->start());
  Node* deopt = graph()->NewNode(
      common()->Deoptimize(DeoptimizeKind::kEager, DeoptimizeReason::kNoReason),
      frame_state, effect1, ifFalse);
  Node* ifTrue = IfTrue();
  Node* load = Load(FieldAccessAtIndex(0), finish, effect1, ifTrue);
  Node* result = Return(load, effect1, ifTrue);
  EndGraph();
  graph()->end()->AppendInput(zone(), deopt);
  Analysis();

  ExpectVirtual(allocation);
  ExpectReplacement(load, object1);

  Transformation();

  ASSERT_EQ(object1, NodeProperties::GetValueInput(result, 1));
  Node* object_state = NodeProperties::GetValueInput(state_values1, 0);
  ASSERT_EQ(object_state->opcode(), IrOpcode::kObjectState);
  ASSERT_EQ(1, object_state->op()->ValueInputCount());
  ASSERT_EQ(object1, NodeProperties::GetValueInput(object_state, 0));
}

TEST_F(EscapeAnalysisTest, DISABLED_DeoptReplacementIdentity) {
  Node* object1 = Constant(1);
  BeginRegion();
  Node* allocation = Allocate(Constant(kPointerSize * 2));
  Store(FieldAccessAtIndex(0), allocation, object1);
  Store(FieldAccessAtIndex(kPointerSize), allocation, allocation);
  Node* finish = FinishRegion(allocation);
  Node* effect1 = Store(FieldAccessAtIndex(0), allocation, object1, finish);
  Branch();
  Node* ifFalse = IfFalse();
  Node* state_values1 = graph()->NewNode(
      common()->StateValues(1, SparseInputMask::Dense()), finish);
  Node* state_values2 = graph()->NewNode(
      common()->StateValues(1, SparseInputMask::Dense()), finish);
  Node* state_values3 =
      graph()->NewNode(common()->StateValues(0, SparseInputMask::Dense()));
  Node* frame_state = graph()->NewNode(
      common()->FrameState(BailoutId::None(), OutputFrameStateCombine::Ignore(),
                           nullptr),
      state_values1, state_values2, state_values3, UndefinedConstant(),
      graph()->start(), graph()->start());
  Node* deopt = graph()->NewNode(
      common()->Deoptimize(DeoptimizeKind::kEager, DeoptimizeReason::kNoReason),
      frame_state, effect1, ifFalse);
  Node* ifTrue = IfTrue();
  Node* load = Load(FieldAccessAtIndex(0), finish, effect1, ifTrue);
  Node* result = Return(load, effect1, ifTrue);
  EndGraph();
  graph()->end()->AppendInput(zone(), deopt);
  Analysis();

  ExpectVirtual(allocation);
  ExpectReplacement(load, object1);

  Transformation();

  ASSERT_EQ(object1, NodeProperties::GetValueInput(result, 1));

  Node* object_state = NodeProperties::GetValueInput(state_values1, 0);
  ASSERT_EQ(object_state->opcode(), IrOpcode::kObjectState);
  ASSERT_EQ(2, object_state->op()->ValueInputCount());
  ASSERT_EQ(object1, NodeProperties::GetValueInput(object_state, 0));
  ASSERT_EQ(object_state, NodeProperties::GetValueInput(object_state, 1));

  Node* object_state2 = NodeProperties::GetValueInput(state_values1, 0);
  ASSERT_EQ(object_state, object_state2);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
