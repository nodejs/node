// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/node-properties.h"
#include "src/heap/factory-inl.h"
#include "test/common/value-helper.h"
#include "test/unittests/compiler/js-heap-broker-base.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace js_constant_cache_unittest {

using JSConstantCacheTest = TestWithNativeContextAndZone;

class JSCacheTesterHelper {
 public:
  Isolate* main_isolate() { return main_isolate_; }
  Zone* main_zone() { return main_zone_; }

 protected:
  JSCacheTesterHelper(Isolate* isolate, Zone* zone)
      : scope_(isolate),
        main_isolate_(isolate),
        main_zone_(zone),
        main_graph_(zone),
        main_common_(zone),
        main_javascript_(zone),
        main_machine_(zone) {}

  HandleScope scope_;
  Isolate* main_isolate_;
  Zone* main_zone_;
  TFGraph main_graph_;
  CommonOperatorBuilder main_common_;
  JSOperatorBuilder main_javascript_;
  MachineOperatorBuilder main_machine_;
};

// TODO(dcarney): JSConstantCacheTester inherits from JSGraph???
class JSConstantCacheTester : public JSCacheTesterHelper,
                              public JSGraph,
                              public JSHeapBrokerTestBase {
 public:
  JSConstantCacheTester(Isolate* isolate, Zone* zone)
      : JSCacheTesterHelper(isolate, zone),
        JSGraph(main_isolate(), &main_graph_, &main_common_, &main_javascript_,
                nullptr, &main_machine_),
        JSHeapBrokerTestBase(main_isolate(), main_zone()) {
    main_graph_.SetStart(main_graph_.NewNode(common()->Start(0)));
    main_graph_.SetEnd(
        main_graph_.NewNode(common()->End(1), main_graph_.start()));
  }

  DirectHandle<HeapObject> handle(Node* node) {
    CHECK_EQ(IrOpcode::kHeapConstant, node->opcode());
    return HeapConstantOf(node->op());
  }

  Factory* factory() { return main_isolate()->factory(); }
};

TEST_F(JSConstantCacheTest, ZeroConstant1) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* zero = T.ZeroConstant();

  CHECK_EQ(IrOpcode::kNumberConstant, zero->opcode());
  CHECK_EQ(zero, T.ConstantNoHole(0));
  CHECK_NE(zero, T.ConstantNoHole(-0.0));
  CHECK_NE(zero, T.ConstantNoHole(1.0));
  CHECK_NE(zero, T.ConstantNoHole(std::numeric_limits<double>::quiet_NaN()));
  CHECK_NE(zero, T.Float64Constant(0));
  CHECK_NE(zero, T.Int32Constant(0));
}

TEST_F(JSConstantCacheTest, MinusZeroConstant) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* minus_zero = T.ConstantNoHole(-0.0);
  Node* zero = T.ZeroConstant();

  CHECK_EQ(IrOpcode::kNumberConstant, minus_zero->opcode());
  CHECK_EQ(minus_zero, T.ConstantNoHole(-0.0));
  CHECK_NE(zero, minus_zero);

  double zero_value = OpParameter<double>(zero->op());
  double minus_zero_value = OpParameter<double>(minus_zero->op());

  CHECK(base::bit_cast<uint64_t>(0.0) == base::bit_cast<uint64_t>(zero_value));
  CHECK(base::bit_cast<uint64_t>(-0.0) != base::bit_cast<uint64_t>(zero_value));
  CHECK(base::bit_cast<uint64_t>(0.0) !=
        base::bit_cast<uint64_t>(minus_zero_value));
  CHECK(base::bit_cast<uint64_t>(-0.0) ==
        base::bit_cast<uint64_t>(minus_zero_value));
}

TEST_F(JSConstantCacheTest, ZeroConstant2) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* zero = T.ConstantNoHole(0);

  CHECK_EQ(IrOpcode::kNumberConstant, zero->opcode());
  CHECK_EQ(zero, T.ZeroConstant());
  CHECK_NE(zero, T.ConstantNoHole(-0.0));
  CHECK_NE(zero, T.ConstantNoHole(1.0));
  CHECK_NE(zero, T.ConstantNoHole(std::numeric_limits<double>::quiet_NaN()));
  CHECK_NE(zero, T.Float64Constant(0));
  CHECK_NE(zero, T.Int32Constant(0));
}

TEST_F(JSConstantCacheTest, OneConstant1) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* one = T.OneConstant();

  CHECK_EQ(IrOpcode::kNumberConstant, one->opcode());
  CHECK_EQ(one, T.ConstantNoHole(1));
  CHECK_EQ(one, T.ConstantNoHole(1.0));
  CHECK_NE(one, T.ConstantNoHole(1.01));
  CHECK_NE(one, T.ConstantNoHole(-1.01));
  CHECK_NE(one, T.ConstantNoHole(std::numeric_limits<double>::quiet_NaN()));
  CHECK_NE(one, T.Float64Constant(1.0));
  CHECK_NE(one, T.Int32Constant(1));
}

TEST_F(JSConstantCacheTest, OneConstant2) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* one = T.ConstantNoHole(1);

  CHECK_EQ(IrOpcode::kNumberConstant, one->opcode());
  CHECK_EQ(one, T.OneConstant());
  CHECK_EQ(one, T.ConstantNoHole(1.0));
  CHECK_NE(one, T.ConstantNoHole(1.01));
  CHECK_NE(one, T.ConstantNoHole(-1.01));
  CHECK_NE(one, T.ConstantNoHole(std::numeric_limits<double>::quiet_NaN()));
  CHECK_NE(one, T.Float64Constant(1.0));
  CHECK_NE(one, T.Int32Constant(1));
}

TEST_F(JSConstantCacheTest, Canonicalizations) {
  JSConstantCacheTester T(i_isolate(), zone());

  CHECK_EQ(T.ZeroConstant(), T.ZeroConstant());
  CHECK_EQ(T.UndefinedConstant(), T.UndefinedConstant());
  CHECK_EQ(T.TheHoleConstant(), T.TheHoleConstant());
  CHECK_EQ(T.TrueConstant(), T.TrueConstant());
  CHECK_EQ(T.FalseConstant(), T.FalseConstant());
  CHECK_EQ(T.NullConstant(), T.NullConstant());
  CHECK_EQ(T.ZeroConstant(), T.ZeroConstant());
  CHECK_EQ(T.OneConstant(), T.OneConstant());
  CHECK_EQ(T.NaNConstant(), T.NaNConstant());
}

TEST_F(JSConstantCacheTest, NoAliasing) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* nodes[] = {T.UndefinedConstant(), T.TheHoleConstant(),
                   T.TrueConstant(),      T.FalseConstant(),
                   T.NullConstant(),      T.ZeroConstant(),
                   T.OneConstant(),       T.NaNConstant(),
                   T.ConstantNoHole(21),  T.ConstantNoHole(22.2)};

  for (size_t i = 0; i < arraysize(nodes); i++) {
    for (size_t j = 0; j < arraysize(nodes); j++) {
      if (i != j) CHECK_NE(nodes[i], nodes[j]);
    }
  }
}

TEST_F(JSConstantCacheTest, CanonicalizingNumbers) {
  JSConstantCacheTester T(i_isolate(), zone());

  FOR_FLOAT64_INPUTS(i) {
    Node* node = T.ConstantNoHole(i);
    for (int j = 0; j < 5; j++) {
      CHECK_EQ(node, T.ConstantNoHole(i));
    }
  }
}

TEST_F(JSConstantCacheTest, HeapNumbers) {
  JSConstantCacheTester T(i_isolate(), zone());

  FOR_FLOAT64_INPUTS(value) {
    Handle<Object> num = T.CanonicalHandle(*T.factory()->NewNumber(value));
    Handle<HeapNumber> heap =
        T.CanonicalHandle(*T.factory()->NewHeapNumber(value));
    Node* node1 = T.ConstantNoHole(value);
    Node* node2 = T.ConstantNoHole(MakeRef(T.broker(), num), T.broker());
    Node* node3 = T.ConstantNoHole(MakeRef(T.broker(), heap), T.broker());
    CHECK_EQ(node1, node2);
    CHECK_EQ(node1, node3);
  }
}

TEST_F(JSConstantCacheTest, OddballHandle) {
  JSConstantCacheTester T(i_isolate(), zone());

  CHECK_EQ(T.UndefinedConstant(),
           T.ConstantNoHole(T.broker()->undefined_value(), T.broker()));
  CHECK_EQ(T.TrueConstant(),
           T.ConstantNoHole(T.broker()->true_value(), T.broker()));
  CHECK_EQ(T.FalseConstant(),
           T.ConstantNoHole(T.broker()->false_value(), T.broker()));
  CHECK_EQ(T.NullConstant(),
           T.ConstantNoHole(T.broker()->null_value(), T.broker()));
  CHECK_EQ(T.NaNConstant(),
           T.ConstantNoHole(T.broker()->nan_value(), T.broker()));
}

TEST_F(JSConstantCacheTest, OddballValues) {
  JSConstantCacheTester T(i_isolate(), zone());

  CHECK_EQ(*T.factory()->undefined_value(), *T.handle(T.UndefinedConstant()));
  CHECK_EQ(*T.factory()->the_hole_value(), *T.handle(T.TheHoleConstant()));
  CHECK_EQ(*T.factory()->true_value(), *T.handle(T.TrueConstant()));
  CHECK_EQ(*T.factory()->false_value(), *T.handle(T.FalseConstant()));
  CHECK_EQ(*T.factory()->null_value(), *T.handle(T.NullConstant()));
}

TEST_F(JSConstantCacheTest, ExternalReferences) {
  // TODO(titzer): test canonicalization of external references.
}

static bool Contains(NodeVector* nodes, Node* n) {
  for (size_t i = 0; i < nodes->size(); i++) {
    if (nodes->at(i) == n) return true;
  }
  return false;
}

static void CheckGetCachedNodesContains(JSConstantCacheTester* T, Node* n) {
  NodeVector nodes(T->main_zone());
  T->GetCachedNodes(&nodes);
  CHECK(Contains(&nodes, n));
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes1) {
  JSConstantCacheTester T(i_isolate(), zone());
  CheckGetCachedNodesContains(&T, T.TrueConstant());
  CheckGetCachedNodesContains(&T, T.UndefinedConstant());
  CheckGetCachedNodesContains(&T, T.TheHoleConstant());
  CheckGetCachedNodesContains(&T, T.TrueConstant());
  CheckGetCachedNodesContains(&T, T.FalseConstant());
  CheckGetCachedNodesContains(&T, T.NullConstant());
  CheckGetCachedNodesContains(&T, T.ZeroConstant());
  CheckGetCachedNodesContains(&T, T.OneConstant());
  CheckGetCachedNodesContains(&T, T.NaNConstant());
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes_int32) {
  JSConstantCacheTester T(i_isolate(), zone());

  int32_t constants[] = {0,  1,  1,   1,   1,   2,   3,   4,  11, 12, 13,
                         14, 55, -55, -44, -33, -22, -11, 16, 16, 17, 17,
                         18, 18, 19,  19,  20,  20,  21,  21, 22, 23, 24,
                         25, 15, 30,  31,  45,  46,  47,  48};

  for (size_t i = 0; i < arraysize(constants); i++) {
    size_t count_before = T.graph()->NodeCount();
    NodeVector nodes_before(T.main_zone());
    T.GetCachedNodes(&nodes_before);
    Node* n = T.Int32Constant(constants[i]);
    if (n->id() < count_before) {
      // An old ID indicates a cached node. It should have been in the set.
      CHECK(Contains(&nodes_before, n));
    }
    // Old or new, it should be in the cached set afterwards.
    CheckGetCachedNodesContains(&T, n);
  }
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes_float64) {
  JSConstantCacheTester T(i_isolate(), zone());

  double constants[] = {0,   11.1, 12.2,  13,    14,   55.5, -55.5, -44.4,
                        -33, -22,  -11,   0,     11.1, 11.1, 12.3,  12.3,
                        11,  11,   -33.3, -33.3, -22,  -11};

  for (size_t i = 0; i < arraysize(constants); i++) {
    size_t count_before = T.graph()->NodeCount();
    NodeVector nodes_before(T.main_zone());
    T.GetCachedNodes(&nodes_before);
    Node* n = T.Float64Constant(constants[i]);
    if (n->id() < count_before) {
      // An old ID indicates a cached node. It should have been in the set.
      CHECK(Contains(&nodes_before, n));
    }
    // Old or new, it should be in the cached set afterwards.
    CheckGetCachedNodesContains(&T, n);
  }
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes_int64) {
  JSConstantCacheTester T(i_isolate(), zone());

  int32_t constants[] = {0,   11,  12, 13, 14, 55, -55, -44, -33,
                         -22, -11, 16, 16, 17, 17, 18,  18,  19,
                         19,  20,  20, 21, 21, 22, 23,  24,  25};

  for (size_t i = 0; i < arraysize(constants); i++) {
    size_t count_before = T.graph()->NodeCount();
    NodeVector nodes_before(T.main_zone());
    T.GetCachedNodes(&nodes_before);
    Node* n = T.Int64Constant(constants[i]);
    if (n->id() < count_before) {
      // An old ID indicates a cached node. It should have been in the set.
      CHECK(Contains(&nodes_before, n));
    }
    // Old or new, it should be in the cached set afterwards.
    CheckGetCachedNodesContains(&T, n);
  }
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes_number) {
  JSConstantCacheTester T(i_isolate(), zone());

  double constants[] = {0,   11.1, 12.2,  13,    14,   55.5, -55.5, -44.4,
                        -33, -22,  -11,   0,     11.1, 11.1, 12.3,  12.3,
                        11,  11,   -33.3, -33.3, -22,  -11};

  for (size_t i = 0; i < arraysize(constants); i++) {
    size_t count_before = T.graph()->NodeCount();
    NodeVector nodes_before(T.main_zone());
    T.GetCachedNodes(&nodes_before);
    Node* n = T.ConstantNoHole(constants[i]);
    if (n->id() < count_before) {
      // An old ID indicates a cached node. It should have been in the set.
      CHECK(Contains(&nodes_before, n));
    }
    // Old or new, it should be in the cached set afterwards.
    CheckGetCachedNodesContains(&T, n);
  }
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes_external) {
  JSConstantCacheTester T(i_isolate(), zone());

  ExternalReference constants[] = {ExternalReference::address_of_min_int(),
                                   ExternalReference::address_of_min_int(),
                                   ExternalReference::address_of_min_int(),
                                   ExternalReference::address_of_one_half(),
                                   ExternalReference::address_of_one_half(),
                                   ExternalReference::address_of_min_int(),
                                   ExternalReference::address_of_the_hole_nan(),
                                   ExternalReference::address_of_one_half()};

  for (size_t i = 0; i < arraysize(constants); i++) {
    size_t count_before = T.graph()->NodeCount();
    NodeVector nodes_before(T.main_zone());
    T.GetCachedNodes(&nodes_before);
    Node* n = T.ExternalConstant(constants[i]);
    if (n->id() < count_before) {
      // An old ID indicates a cached node. It should have been in the set.
      CHECK(Contains(&nodes_before, n));
    }
    // Old or new, it should be in the cached set afterwards.
    CheckGetCachedNodesContains(&T, n);
  }
}

TEST_F(JSConstantCacheTest, JSGraph_GetCachedNodes_together) {
  JSConstantCacheTester T(i_isolate(), zone());

  Node* constants[] = {
      T.TrueConstant(),
      T.UndefinedConstant(),
      T.TheHoleConstant(),
      T.TrueConstant(),
      T.FalseConstant(),
      T.NullConstant(),
      T.ZeroConstant(),
      T.OneConstant(),
      T.NaNConstant(),
      T.Int32Constant(0),
      T.Int32Constant(1),
      T.Int64Constant(-2),
      T.Int64Constant(-4),
      T.Float64Constant(0.9),
      T.Float64Constant(V8_INFINITY),
      T.ConstantNoHole(0.99),
      T.ConstantNoHole(1.11),
      T.ExternalConstant(ExternalReference::address_of_one_half())};

  NodeVector nodes(T.main_zone());
  T.GetCachedNodes(&nodes);

  for (size_t i = 0; i < arraysize(constants); i++) {
    CHECK(Contains(&nodes, constants[i]));
  }
}

}  // namespace js_constant_cache_unittest
}  // namespace compiler
}  // namespace internal
}  // namespace v8
