// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-address-reassociation.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/node.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace compiler {

class WasmAddressReassociationTest : public GraphTest {
 public:
  WasmAddressReassociationTest()
      : GraphTest(3),
        machine_(zone()),
        javascript_(zone()),
        jsgraph_(isolate(), graph(), common(), &javascript_, nullptr,
                 &machine_),
        ar_(&jsgraph_, zone()),
        protected_load_op_(machine()->ProtectedLoad(MachineType::Int32())),
        protected_store_op_(
            machine()->ProtectedStore(MachineRepresentation::kWord32)),
        base_(graph()->NewNode(common()->Parameter(0), graph()->start())),
        offset_(graph()->NewNode(common()->Parameter(1), graph()->start())) {}

  ~WasmAddressReassociationTest() override = default;

 protected:
  MachineOperatorBuilder* machine() { return &machine_; }
  WasmAddressReassociation* ar() { return &ar_; }
  Node* Int32Constant(int32_t value) {
    return graph()->NewNode(common()->Int32Constant(value));
  }
  Node* Int64Constant(int64_t value) {
    return graph()->NewNode(common()->Int64Constant(value));
  }
  Node* Int32Add(Node* lhs, Node* rhs) {
    return graph()->NewNode(machine()->Int32Add(), lhs, rhs);
  }
  Node* Int64Add(Node* lhs, Node* rhs) {
    return graph()->NewNode(machine()->Int64Add(), lhs, rhs);
  }
  Node* ExtendAdd(Node* lhs, Node* rhs) {
    Node* add = Int32Add(lhs, rhs);
    return graph()->NewNode(machine()->ChangeUint32ToUint64(), add);
  }
  Node* ProtectedLoad(Node* base, Node* offset, Node* effect, Node* control) {
    return graph()->NewNode(protected_load_op_, base, offset, effect, control);
  }
  Node* ProtectedStore(Node* base, Node* offset, Node* val, Node* effect,
                       Node* control) {
    return graph()->NewNode(protected_store_op_, base, offset, val, effect,
                            control);
  }
  void CheckEffectChain(Node* effect_op, Node* end,
                        std::vector<int64_t>& expected_offsets) {
    std::vector<NodeId> base_addrs;
    std::vector<int64_t> offsets;
    std::set<NodeId> all_visited;
    std::vector<Node*> effect_nodes = {effect_op};
    while (!effect_nodes.empty()) {
      effect_op = effect_nodes.back();
      effect_nodes.pop_back();
      if (all_visited.count(effect_op->id())) continue;
      if (effect_op == end) continue;

      all_visited.insert(effect_op->id());
      for (int i = 0; i < effect_op->op()->EffectInputCount(); ++i) {
        effect_nodes.push_back(NodeProperties::GetEffectInput(effect_op, i));
      }

      if (effect_op->opcode() == IrOpcode::kProtectedLoad ||
          effect_op->opcode() == IrOpcode::kProtectedStore) {
        Node* add = effect_op->InputAt(0);
        EXPECT_EQ(add->opcode(), IrOpcode::kInt64Add);
        EXPECT_TRUE(add->InputAt(0) == base_);
        EXPECT_TRUE(add->InputAt(1) == offset_);
        Node* offset = effect_op->InputAt(1);
        EXPECT_TRUE(NodeProperties::IsConstant(offset));
        base_addrs.push_back(add->id());
        offsets.push_back(OpParameter<int64_t>(offset->op()));
      }
    }
    EXPECT_EQ(base_addrs.size(), expected_offsets.size());
    EXPECT_TRUE(std::adjacent_find(base_addrs.begin(), base_addrs.end(),
                                   std::not_equal_to<>()) == base_addrs.end());
    EXPECT_EQ(offsets, expected_offsets);
  }
  Node* base() const { return base_; }
  Node* offset() const { return offset_; }

 private:
  MachineOperatorBuilder machine_;
  JSOperatorBuilder javascript_;
  JSGraph jsgraph_;
  WasmAddressReassociation ar_;
  const Operator* protected_load_op_;
  const Operator* protected_store_op_;
  Node* base_;
  Node* offset_;
};

TEST_F(WasmAddressReassociationTest, ProtectedBase) {
  if (machine()->Is32()) return;

  Node* control = graph()->start();
  Node* effect = graph()->start();
  NodeId effect_id = effect->id();
  for (unsigned i = 0; i < 3; ++i) {
    Node* index = Int64Constant((i + 1) * 8);
    Node* object = Int64Add(base(), index);
    Node* load = ProtectedLoad(object, offset(), effect, control);
    Node* store = ProtectedStore(object, offset(), load, load, control);
    ar()->VisitProtectedMemOp(load, effect_id);
    ar()->VisitProtectedMemOp(store, effect_id);
    effect = store;
  }
  graph()->end()->InsertInput(zone(), 0, effect);
  ar()->Optimize();
  std::vector<int64_t> offsets = {24, 24, 16, 16, 8, 8};
  CheckEffectChain(effect, graph()->start(), offsets);
}

TEST_F(WasmAddressReassociationTest, ProtectedIndex) {
  if (machine()->Is32()) return;

  Node* control = graph()->start();
  Node* effect = graph()->start();
  NodeId effect_id = effect->id();
  for (int64_t i = 0; i < 3; ++i) {
    Node* index = Int64Constant((i + 1) * -8);
    Node* add = Int64Add(offset(), index);
    Node* load = ProtectedLoad(base(), add, effect, control);
    Node* store = ProtectedStore(base(), add, load, load, control);
    ar()->VisitProtectedMemOp(load, effect_id);
    ar()->VisitProtectedMemOp(store, effect_id);
    effect = store;
  }
  graph()->end()->InsertInput(zone(), 0, effect);
  ar()->Optimize();
  std::vector<int64_t> offsets = {-24, -24, -16, -16, -8, -8};
  CheckEffectChain(effect, graph()->start(), offsets);
}

TEST_F(WasmAddressReassociationTest, ProtectedBaseIndex) {
  if (machine()->Is32()) return;

  Node* control = graph()->start();
  Node* effect = graph()->start();
  NodeId effect_id = effect->id();
  for (unsigned i = 0; i < 3; ++i) {
    Node* base_add = Int64Add(base(), Int64Constant(i * 4));
    Node* index_add = Int64Add(offset(), Int64Constant((i + 1) * 8));
    Node* load = ProtectedLoad(base_add, index_add, effect, control);
    Node* store = ProtectedStore(base_add, index_add, load, load, control);
    ar()->VisitProtectedMemOp(load, effect_id);
    ar()->VisitProtectedMemOp(store, effect_id);
    effect = store;
  }
  graph()->end()->InsertInput(zone(), 0, effect);
  ar()->Optimize();
  std::vector<int64_t> offsets = {32, 32, 20, 20, 8, 8};
  CheckEffectChain(effect, graph()->start(), offsets);
}

TEST_F(WasmAddressReassociationTest, ProtectedExtendIndex) {
  if (machine()->Is32()) return;

  Node* control = graph()->start();
  Node* effect = graph()->start();
  NodeId effect_id = effect->id();
  for (unsigned i = 0; i < 3; ++i) {
    Node* index = Int32Constant(8);
    Node* add = ExtendAdd(offset(), index);
    Node* load = ProtectedLoad(base(), add, effect, control);
    Node* store = ProtectedStore(base(), add, load, load, control);
    ar()->VisitProtectedMemOp(load, effect_id);
    ar()->VisitProtectedMemOp(store, effect_id);
    effect = store;
  }
  graph()->end()->InsertInput(zone(), 0, effect);
  ar()->Optimize();

  while (effect && effect != graph()->start()) {
    EXPECT_FALSE(NodeProperties::IsConstant(effect->InputAt(1)));
    effect = NodeProperties::GetEffectInput(effect, 0);
  }
}

TEST_F(WasmAddressReassociationTest, Diamond) {
  if (machine()->Is32()) return;

  // start
  //   3 loads
  //   branch
  // if_true
  //   3 loads
  // if_false
  //   3 stores
  // merge
  //   3 loads
  auto SequentialLoads = [this](size_t N, Node* effect_chain, Node* control_in,
                                Node* effect_region) {
    NodeId effect_region_id = effect_region->id();
    for (unsigned i = 0; i < N; ++i) {
      size_t current_offset = 8 * (i + 1);
      Node* add = Int64Add(base(), Int64Constant(current_offset));
      Node* load = ProtectedLoad(add, offset(), effect_chain, control_in);
      ar()->VisitProtectedMemOp(load, effect_region_id);
      effect_chain = load;
    }
    return effect_chain;
  };
  auto SequentialStores = [this](size_t N, Node* effect_chain, Node* control_in,
                                 Node* effect_region) {
    NodeId effect_region_id = effect_region->id();
    for (unsigned i = 0; i < N; ++i) {
      size_t current_offset = 8 * (i + 1);
      Node* add = Int64Add(offset(), Int64Constant(current_offset));
      Node* store = ProtectedStore(base(), add, add, effect_chain, control_in);
      ar()->VisitProtectedMemOp(store, effect_region_id);
      effect_chain = store;
    }
    return effect_chain;
  };
  Node* start = graph()->start();
  Node* effect = SequentialLoads(3, start, start, start);
  Node* check = Parameter(Type::Boolean(), 1);
  Node* branch = graph()->NewNode(common()->Branch(), check, start);
  Node* etrue = effect;
  Node* efalse = effect;

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  etrue = SequentialLoads(3, etrue, if_true, start);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  efalse = SequentialStores(3, efalse, if_false, start);

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* effect_phi =
      graph()->NewNode(common()->EffectPhi(2), etrue, efalse, merge);
  Node* end = SequentialLoads(3, effect_phi, merge, effect_phi);

  ar()->Optimize();

  // end to effect phi: 3 loads sharing base address.
  std::vector<int64_t> offsets = {24, 16, 8};
  CheckEffectChain(end, effect_phi, offsets);

  // effect phi to start: 9 operations sharing base address.
  offsets = {24, 16, 8, 24, 16, 8, 24, 16, 8};
  CheckEffectChain(effect_phi, start, offsets);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
