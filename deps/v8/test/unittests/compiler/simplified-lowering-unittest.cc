// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-lowering.h"

#include "src/codegen/tick-counter.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "test/unittests/fuzztest.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedLoweringTest : public GraphTest {
 public:
  explicit SimplifiedLoweringTest(int num_parameters = 1)
      : GraphTest(num_parameters),
        num_parameters_(num_parameters),
        machine_(zone()),
        javascript_(zone()),
        simplified_(zone()),
        jsgraph_(isolate(), graph(), common(), &javascript_, &simplified_,
                 &machine_) {}
  ~SimplifiedLoweringTest() override = default;

  void LowerGraph(Node* node) {
    // Make sure we always start with an empty graph.
    graph()->SetStart(graph()->NewNode(common()->Start(num_parameters())));
    graph()->SetEnd(graph()->NewNode(common()->End(1), graph()->start()));

    // Return {node} directly, so that we can match it with
    // "IsReturn(expected)".
    Node* zero = graph()->NewNode(common()->NumberConstant(0));
    Node* ret = graph()->NewNode(common()->Return(), zero, node,
                                 graph()->start(), graph()->start());
    NodeProperties::MergeControlToEnd(graph(), common(), ret);

    RunLowering();
  }

  void RunLowering() {
    {
      // Simplified lowering needs to run w/o the typer decorator so make sure
      // the object is not live at the same time.
      Typer typer(broker(), Typer::kNoFlags, graph(), tick_counter());
      typer.Run();
    }

    Linkage* linkage = zone()->New<Linkage>(Linkage::GetJSCallDescriptor(
        zone(), false, num_parameters_ + 1, CallDescriptor::kNoFlags));
    SimplifiedLowering lowering(jsgraph(), broker(), zone(), source_positions(),
                                node_origins(), tick_counter(), linkage,
                                nullptr);
    lowering.LowerAllNodes();
  }

  void SmiConstantToIntPtrConstantP(int x);

  int num_parameters() const { return num_parameters_; }
  JSGraph* jsgraph() { return &jsgraph_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  const int num_parameters_;
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
  SimplifiedOperatorBuilder simplified_;
  JSGraph jsgraph_;
};

V8_FUZZ_SUITE(SimplifiedLoweringFuzzTest, SimplifiedLoweringTest);

const int kSmiValues[] = {Smi::kMinValue,
                          Smi::kMinValue + 1,
                          Smi::kMinValue + 2,
                          -3,
                          -2,
                          -1,
                          0,
                          1,
                          2,
                          3,
                          Smi::kMaxValue - 2,
                          Smi::kMaxValue - 1,
                          Smi::kMaxValue};

void SimplifiedLoweringTest::SmiConstantToIntPtrConstantP(int x) {
  LowerGraph(jsgraph()->ConstantNoHole(x));
  intptr_t smi = base::bit_cast<intptr_t>(Smi::FromInt(x));
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(IsIntPtrConstant(smi), start(), start()));
}

TEST_F(SimplifiedLoweringTest, SmiConstantToIntPtrConstant) {
  TRACED_FOREACH(int, x, kSmiValues) { SmiConstantToIntPtrConstantP(x); }
}

V8_FUZZ_TEST_F(SimplifiedLoweringFuzzTest, SmiConstantToIntPtrConstantP)
    .WithDomains(fuzztest::InRange(Smi::kMinValue, Smi::kMaxValue));

TEST_F(SimplifiedLoweringTest,
       StoreTaggedPointerFieldConvertsTaggedAndPreservesPointerWriteBarrier) {
  Node* p0 = Parameter(0);
  Node* cond = graph()->NewNode(common()->TypeGuard(Type::Boolean()), p0,
                                start(), start());
  Node* num =
      graph()->NewNode(common()->TypeGuard(Type::Number()), p0, cond, start());
  Node* base_obj =
      graph()->NewNode(common()->TypeGuard(Type::Receiver()), p0, num, start());

  Node* branch = graph()->NewNode(common()->Branch(), cond, start());
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);

  Node* mul = graph()->NewNode(simplified()->NumberMultiply(), num,
                               NumberConstant(0.5));
  Node* checked =
      graph()->NewNode(simplified()->CheckHeapObject(), mul, base_obj, if_true);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* ephi =
      graph()->NewNode(common()->EffectPhi(2), checked, base_obj, merge);
  Node* phi = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               checked, jsgraph()->NullConstant(), merge);

  FieldAccess access0 = AccessBuilder::ForPropertyArraySlot(
      0, Representation::HeapObject(), false);
  Node* store_phi = graph()->NewNode(simplified()->StoreField(access0),
                                     base_obj, phi, ephi, merge);

  FieldAccess access1 = AccessBuilder::ForPropertyArraySlot(
      1, Representation::HeapObject(), false);
  Node* smi_const = NumberConstant(7);
  Node* store_smi_const = graph()->NewNode(
      simplified()->StoreField(access1), base_obj, smi_const, store_phi, merge);

  Node* checked_smi = graph()->NewNode(simplified()->CheckSmi(FeedbackSource()),
                                       p0, store_smi_const, merge);
  ElementAccess elem_access = {kTaggedBase, FixedArray::OffsetOfElementAt(0),
                               Type::Any(), MachineType::TaggedPointer(),
                               kFullWriteBarrier};
  Node* index = NumberConstant(0);
  Node* store_elem =
      graph()->NewNode(simplified()->StoreElement(elem_access), base_obj, index,
                       checked_smi, checked_smi, merge);

  Node* zero = graph()->NewNode(common()->NumberConstant(0));
  Node* ret =
      graph()->NewNode(common()->Return(), zero, base_obj, store_elem, merge);
  NodeProperties::MergeControlToEnd(graph(), common(), ret);

  RunLowering();

  EXPECT_EQ(IrOpcode::kChangeTaggedToTaggedPointer,
            store_phi->InputAt(1)->opcode());
  EXPECT_EQ(kPointerWriteBarrier,
            FieldAccessOf(store_phi->op()).write_barrier_kind);

  EXPECT_EQ(IrOpcode::kChangeTaggedToTaggedPointer,
            store_smi_const->InputAt(1)->opcode());
  EXPECT_EQ(kPointerWriteBarrier,
            FieldAccessOf(store_smi_const->op()).write_barrier_kind);

  EXPECT_EQ(IrOpcode::kChangeTaggedToTaggedPointer,
            store_elem->InputAt(2)->opcode());
  EXPECT_EQ(kPointerWriteBarrier,
            ElementAccessOf(store_elem->op()).write_barrier_kind);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
