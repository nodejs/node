// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/graph-builder-tester.h"

#include "src/compiler/node-matchers.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/typer.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

namespace v8 {  // for friendiness.
namespace internal {
namespace compiler {

class RepresentationChangerTester : public HandleAndZoneScope,
                                    public GraphAndBuilders {
 public:
  explicit RepresentationChangerTester(int num_parameters = 0)
      : GraphAndBuilders(main_zone()),
        typer_(main_zone()),
        jsgraph_(main_graph_, &main_common_, &typer_),
        changer_(&jsgraph_, &main_simplified_, &main_machine_, main_isolate()) {
    Node* s = graph()->NewNode(common()->Start(num_parameters));
    graph()->SetStart(s);
  }

  Typer typer_;
  JSGraph jsgraph_;
  RepresentationChanger changer_;

  Isolate* isolate() { return main_isolate(); }
  Graph* graph() { return main_graph_; }
  CommonOperatorBuilder* common() { return &main_common_; }
  JSGraph* jsgraph() { return &jsgraph_; }
  RepresentationChanger* changer() { return &changer_; }

  // TODO(titzer): use ValueChecker / ValueUtil
  void CheckInt32Constant(Node* n, int32_t expected) {
    ValueMatcher<int32_t> m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, m.Value());
  }

  void CheckHeapConstant(Node* n, Object* expected) {
    ValueMatcher<Handle<Object> > m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, *m.Value());
  }

  void CheckNumberConstant(Node* n, double expected) {
    ValueMatcher<double> m(n);
    CHECK_EQ(IrOpcode::kNumberConstant, n->opcode());
    CHECK(m.HasValue());
    CHECK_EQ(expected, m.Value());
  }

  Node* Parameter(int index = 0) {
    return graph()->NewNode(common()->Parameter(index), graph()->start());
  }

  void CheckTypeError(RepTypeUnion from, RepTypeUnion to) {
    changer()->testing_type_errors_ = true;
    changer()->type_error_ = false;
    Node* n = Parameter(0);
    Node* c = changer()->GetRepresentationFor(n, from, to);
    CHECK_EQ(n, c);
    CHECK(changer()->type_error_);
  }

  void CheckNop(RepTypeUnion from, RepTypeUnion to) {
    Node* n = Parameter(0);
    Node* c = changer()->GetRepresentationFor(n, from, to);
    CHECK_EQ(n, c);
  }
};
}
}
}  // namespace v8::internal::compiler


static const RepType all_reps[] = {rBit, rWord32, rWord64, rFloat64, rTagged};


// TODO(titzer): lift this to ValueHelper
static const double double_inputs[] = {
    0.0,   -0.0,    1.0,    -1.0,        0.1,         1.4,    -1.7,
    2,     5,       6,      982983,      888,         -999.8, 3.1e7,
    -2e66, 2.3e124, -12e73, V8_INFINITY, -V8_INFINITY};


static const int32_t int32_inputs[] = {
    0,      1,                                -1,
    2,      5,                                6,
    982983, 888,                              -999,
    65535,  static_cast<int32_t>(0xFFFFFFFF), static_cast<int32_t>(0x80000000)};


static const uint32_t uint32_inputs[] = {
    0,      1,   static_cast<uint32_t>(-1),   2,     5,          6,
    982983, 888, static_cast<uint32_t>(-999), 65535, 0xFFFFFFFF, 0x80000000};


TEST(BoolToBit_constant) {
  RepresentationChangerTester r;

  Node* true_node = r.jsgraph()->TrueConstant();
  Node* true_bit = r.changer()->GetRepresentationFor(true_node, rTagged, rBit);
  r.CheckInt32Constant(true_bit, 1);

  Node* false_node = r.jsgraph()->FalseConstant();
  Node* false_bit =
      r.changer()->GetRepresentationFor(false_node, rTagged, rBit);
  r.CheckInt32Constant(false_bit, 0);
}


TEST(BitToBool_constant) {
  RepresentationChangerTester r;

  for (int i = -5; i < 5; i++) {
    Node* node = r.jsgraph()->Int32Constant(i);
    Node* val = r.changer()->GetRepresentationFor(node, rBit, rTagged);
    r.CheckHeapConstant(val, i == 0 ? r.isolate()->heap()->false_value()
                                    : r.isolate()->heap()->true_value());
  }
}


TEST(ToTagged_constant) {
  RepresentationChangerTester r;

  for (size_t i = 0; i < ARRAY_SIZE(double_inputs); i++) {
    Node* n = r.jsgraph()->Float64Constant(double_inputs[i]);
    Node* c = r.changer()->GetRepresentationFor(n, rFloat64, rTagged);
    r.CheckNumberConstant(c, double_inputs[i]);
  }

  for (size_t i = 0; i < ARRAY_SIZE(int32_inputs); i++) {
    Node* n = r.jsgraph()->Int32Constant(int32_inputs[i]);
    Node* c = r.changer()->GetRepresentationFor(n, rWord32 | tInt32, rTagged);
    r.CheckNumberConstant(c, static_cast<double>(int32_inputs[i]));
  }

  for (size_t i = 0; i < ARRAY_SIZE(uint32_inputs); i++) {
    Node* n = r.jsgraph()->Int32Constant(uint32_inputs[i]);
    Node* c = r.changer()->GetRepresentationFor(n, rWord32 | tUint32, rTagged);
    r.CheckNumberConstant(c, static_cast<double>(uint32_inputs[i]));
  }
}


static void CheckChange(IrOpcode::Value expected, RepTypeUnion from,
                        RepTypeUnion to) {
  RepresentationChangerTester r;

  Node* n = r.Parameter();
  Node* c = r.changer()->GetRepresentationFor(n, from, to);

  CHECK_NE(c, n);
  CHECK_EQ(expected, c->opcode());
  CHECK_EQ(n, c->InputAt(0));
}


TEST(SingleChanges) {
  CheckChange(IrOpcode::kChangeBoolToBit, rTagged, rBit);
  CheckChange(IrOpcode::kChangeBitToBool, rBit, rTagged);

  CheckChange(IrOpcode::kChangeInt32ToTagged, rWord32 | tInt32, rTagged);
  CheckChange(IrOpcode::kChangeUint32ToTagged, rWord32 | tUint32, rTagged);
  CheckChange(IrOpcode::kChangeFloat64ToTagged, rFloat64, rTagged);

  CheckChange(IrOpcode::kChangeTaggedToInt32, rTagged | tInt32, rWord32);
  CheckChange(IrOpcode::kChangeTaggedToUint32, rTagged | tUint32, rWord32);
  CheckChange(IrOpcode::kChangeTaggedToFloat64, rTagged, rFloat64);

  // Int32,Uint32 <-> Float64 are actually machine conversions.
  CheckChange(IrOpcode::kChangeInt32ToFloat64, rWord32 | tInt32, rFloat64);
  CheckChange(IrOpcode::kChangeUint32ToFloat64, rWord32 | tUint32, rFloat64);
  CheckChange(IrOpcode::kChangeFloat64ToInt32, rFloat64 | tInt32, rWord32);
  CheckChange(IrOpcode::kChangeFloat64ToUint32, rFloat64 | tUint32, rWord32);
}


TEST(SignednessInWord32) {
  RepresentationChangerTester r;

  // TODO(titzer): these are currently type errors because the output type is
  // not specified. Maybe the RepresentationChanger should assume anything to or
  // from {rWord32} is {tInt32}, i.e. signed, if not it is explicitly otherwise?
  r.CheckTypeError(rTagged, rWord32 | tInt32);
  r.CheckTypeError(rTagged, rWord32 | tUint32);
  r.CheckTypeError(rWord32, rFloat64);
  r.CheckTypeError(rFloat64, rWord32);

  //  CheckChange(IrOpcode::kChangeTaggedToInt32, rTagged, rWord32 | tInt32);
  //  CheckChange(IrOpcode::kChangeTaggedToUint32, rTagged, rWord32 | tUint32);
  //  CheckChange(IrOpcode::kChangeInt32ToFloat64, rWord32, rFloat64);
  //  CheckChange(IrOpcode::kChangeFloat64ToInt32, rFloat64, rWord32);
}


TEST(Nops) {
  RepresentationChangerTester r;

  // X -> X is always a nop for any single representation X.
  for (size_t i = 0; i < ARRAY_SIZE(all_reps); i++) {
    r.CheckNop(all_reps[i], all_reps[i]);
  }

  // 32-bit or 64-bit words can be used as branch conditions (rBit).
  r.CheckNop(rWord32, rBit);
  r.CheckNop(rWord32, rBit | tBool);
  r.CheckNop(rWord64, rBit);
  r.CheckNop(rWord64, rBit | tBool);

  // rBit (result of comparison) is implicitly a wordish thing.
  r.CheckNop(rBit, rWord32);
  r.CheckNop(rBit | tBool, rWord32);
  r.CheckNop(rBit, rWord64);
  r.CheckNop(rBit | tBool, rWord64);
}


TEST(TypeErrors) {
  RepresentationChangerTester r;

  // Floats cannot be implicitly converted to/from comparison conditions.
  r.CheckTypeError(rFloat64, rBit);
  r.CheckTypeError(rFloat64, rBit | tBool);
  r.CheckTypeError(rBit, rFloat64);
  r.CheckTypeError(rBit | tBool, rFloat64);

  // Word64 is internal and shouldn't be implicitly converted.
  r.CheckTypeError(rWord64, rTagged | tBool);
  r.CheckTypeError(rWord64, rTagged);
  r.CheckTypeError(rWord64, rTagged | tBool);
  r.CheckTypeError(rTagged, rWord64);
  r.CheckTypeError(rTagged | tBool, rWord64);

  // Word64 / Word32 shouldn't be implicitly converted.
  r.CheckTypeError(rWord64, rWord32);
  r.CheckTypeError(rWord32, rWord64);
  r.CheckTypeError(rWord64, rWord32 | tInt32);
  r.CheckTypeError(rWord32 | tInt32, rWord64);
  r.CheckTypeError(rWord64, rWord32 | tUint32);
  r.CheckTypeError(rWord32 | tUint32, rWord64);

  for (size_t i = 0; i < ARRAY_SIZE(all_reps); i++) {
    for (size_t j = 0; j < ARRAY_SIZE(all_reps); j++) {
      if (i == j) continue;
      // Only a single from representation is allowed.
      r.CheckTypeError(all_reps[i] | all_reps[j], rTagged);
    }
  }
}


TEST(CompleteMatrix) {
  // TODO(titzer): test all variants in the matrix.
  // rB
  // tBrB
  // tBrT
  // rW32
  // tIrW32
  // tUrW32
  // rW64
  // tIrW64
  // tUrW64
  // rF64
  // tIrF64
  // tUrF64
  // tArF64
  // rT
  // tArT
}
