// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/compiler/value-helper.h"

#include "src/compiler/node-matchers.h"
#include "src/compiler/representation-change.h"

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
        javascript_(main_zone()),
        jsgraph_(main_graph_, &main_common_, &javascript_, &main_machine_),
        changer_(&jsgraph_, &main_simplified_, main_isolate()) {
    Node* s = graph()->NewNode(common()->Start(num_parameters));
    graph()->SetStart(s);
  }

  JSOperatorBuilder javascript_;
  JSGraph jsgraph_;
  RepresentationChanger changer_;

  Isolate* isolate() { return main_isolate(); }
  Graph* graph() { return main_graph_; }
  CommonOperatorBuilder* common() { return &main_common_; }
  JSGraph* jsgraph() { return &jsgraph_; }
  RepresentationChanger* changer() { return &changer_; }

  // TODO(titzer): use ValueChecker / ValueUtil
  void CheckInt32Constant(Node* n, int32_t expected) {
    Int32Matcher m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, m.Value());
  }

  void CheckUint32Constant(Node* n, uint32_t expected) {
    Uint32Matcher m(n);
    CHECK(m.HasValue());
    CHECK_EQ(static_cast<int>(expected), static_cast<int>(m.Value()));
  }

  void CheckFloat64Constant(Node* n, double expected) {
    Float64Matcher m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, m.Value());
  }

  void CheckFloat32Constant(Node* n, float expected) {
    CHECK_EQ(IrOpcode::kFloat32Constant, n->opcode());
    float fval = OpParameter<float>(n->op());
    CHECK_EQ(expected, fval);
  }

  void CheckHeapConstant(Node* n, HeapObject* expected) {
    HeapObjectMatcher<HeapObject> m(n);
    CHECK(m.HasValue());
    CHECK_EQ(expected, *m.Value().handle());
  }

  void CheckNumberConstant(Node* n, double expected) {
    NumberMatcher m(n);
    CHECK_EQ(IrOpcode::kNumberConstant, n->opcode());
    CHECK(m.HasValue());
    CHECK_EQ(expected, m.Value());
  }

  Node* Parameter(int index = 0) {
    return graph()->NewNode(common()->Parameter(index), graph()->start());
  }

  void CheckTypeError(MachineTypeUnion from, MachineTypeUnion to) {
    changer()->testing_type_errors_ = true;
    changer()->type_error_ = false;
    Node* n = Parameter(0);
    Node* c = changer()->GetRepresentationFor(n, from, to);
    CHECK(changer()->type_error_);
    CHECK_EQ(n, c);
  }

  void CheckNop(MachineTypeUnion from, MachineTypeUnion to) {
    Node* n = Parameter(0);
    Node* c = changer()->GetRepresentationFor(n, from, to);
    CHECK_EQ(n, c);
  }
};
}
}
}  // namespace v8::internal::compiler


static const MachineType all_reps[] = {kRepBit,     kRepWord32,  kRepWord64,
                                       kRepFloat32, kRepFloat64, kRepTagged};


TEST(BoolToBit_constant) {
  RepresentationChangerTester r;

  Node* true_node = r.jsgraph()->TrueConstant();
  Node* true_bit =
      r.changer()->GetRepresentationFor(true_node, kRepTagged, kRepBit);
  r.CheckInt32Constant(true_bit, 1);

  Node* false_node = r.jsgraph()->FalseConstant();
  Node* false_bit =
      r.changer()->GetRepresentationFor(false_node, kRepTagged, kRepBit);
  r.CheckInt32Constant(false_bit, 0);
}


TEST(BitToBool_constant) {
  RepresentationChangerTester r;

  for (int i = -5; i < 5; i++) {
    Node* node = r.jsgraph()->Int32Constant(i);
    Node* val = r.changer()->GetRepresentationFor(node, kRepBit, kRepTagged);
    r.CheckHeapConstant(val, i == 0 ? r.isolate()->heap()->false_value()
                                    : r.isolate()->heap()->true_value());
  }
}


TEST(ToTagged_constant) {
  RepresentationChangerTester r;

  {
    FOR_FLOAT64_INPUTS(i) {
      Node* n = r.jsgraph()->Float64Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat64, kRepTagged);
      r.CheckNumberConstant(c, *i);
    }
  }

  {
    FOR_FLOAT64_INPUTS(i) {
      Node* n = r.jsgraph()->Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat64, kRepTagged);
      r.CheckNumberConstant(c, *i);
    }
  }

  {
    FOR_FLOAT32_INPUTS(i) {
      Node* n = r.jsgraph()->Float32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat32, kRepTagged);
      r.CheckNumberConstant(c, *i);
    }
  }

  {
    FOR_INT32_INPUTS(i) {
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeInt32,
                                                  kRepTagged);
      r.CheckNumberConstant(c, *i);
    }
  }

  {
    FOR_UINT32_INPUTS(i) {
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeUint32,
                                                  kRepTagged);
      r.CheckNumberConstant(c, *i);
    }
  }
}


TEST(ToFloat64_constant) {
  RepresentationChangerTester r;

  {
    FOR_FLOAT64_INPUTS(i) {
      Node* n = r.jsgraph()->Float64Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat64, kRepFloat64);
      CHECK_EQ(n, c);
    }
  }

  {
    FOR_FLOAT64_INPUTS(i) {
      Node* n = r.jsgraph()->Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepTagged, kRepFloat64);
      r.CheckFloat64Constant(c, *i);
    }
  }

  {
    FOR_FLOAT32_INPUTS(i) {
      Node* n = r.jsgraph()->Float32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat32, kRepFloat64);
      r.CheckFloat64Constant(c, *i);
    }
  }

  {
    FOR_INT32_INPUTS(i) {
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeInt32,
                                                  kRepFloat64);
      r.CheckFloat64Constant(c, *i);
    }
  }

  {
    FOR_UINT32_INPUTS(i) {
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeUint32,
                                                  kRepFloat64);
      r.CheckFloat64Constant(c, *i);
    }
  }
}


static bool IsFloat32Int32(int32_t val) {
  return val >= -(1 << 23) && val <= (1 << 23);
}


static bool IsFloat32Uint32(uint32_t val) { return val <= (1 << 23); }


TEST(ToFloat32_constant) {
  RepresentationChangerTester r;

  {
    FOR_FLOAT32_INPUTS(i) {
      Node* n = r.jsgraph()->Float32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat32, kRepFloat32);
      CHECK_EQ(n, c);
    }
  }

  {
    FOR_FLOAT32_INPUTS(i) {
      Node* n = r.jsgraph()->Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepTagged, kRepFloat32);
      r.CheckFloat32Constant(c, *i);
    }
  }

  {
    FOR_FLOAT32_INPUTS(i) {
      Node* n = r.jsgraph()->Float64Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat64, kRepFloat32);
      r.CheckFloat32Constant(c, *i);
    }
  }

  {
    FOR_INT32_INPUTS(i) {
      if (!IsFloat32Int32(*i)) continue;
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeInt32,
                                                  kRepFloat32);
      r.CheckFloat32Constant(c, static_cast<float>(*i));
    }
  }

  {
    FOR_UINT32_INPUTS(i) {
      if (!IsFloat32Uint32(*i)) continue;
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeUint32,
                                                  kRepFloat32);
      r.CheckFloat32Constant(c, static_cast<float>(*i));
    }
  }
}


TEST(ToInt32_constant) {
  RepresentationChangerTester r;

  {
    FOR_INT32_INPUTS(i) {
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeInt32,
                                                  kRepWord32);
      r.CheckInt32Constant(c, *i);
    }
  }

  {
    FOR_INT32_INPUTS(i) {
      if (!IsFloat32Int32(*i)) continue;
      Node* n = r.jsgraph()->Float32Constant(static_cast<float>(*i));
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat32 | kTypeInt32,
                                                  kRepWord32);
      r.CheckInt32Constant(c, *i);
    }
  }

  {
    FOR_INT32_INPUTS(i) {
      Node* n = r.jsgraph()->Float64Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat64 | kTypeInt32,
                                                  kRepWord32);
      r.CheckInt32Constant(c, *i);
    }
  }

  {
    FOR_INT32_INPUTS(i) {
      Node* n = r.jsgraph()->Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepTagged | kTypeInt32,
                                                  kRepWord32);
      r.CheckInt32Constant(c, *i);
    }
  }
}


TEST(ToUint32_constant) {
  RepresentationChangerTester r;

  {
    FOR_UINT32_INPUTS(i) {
      Node* n = r.jsgraph()->Int32Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepWord32 | kTypeUint32,
                                                  kRepWord32);
      r.CheckUint32Constant(c, *i);
    }
  }

  {
    FOR_UINT32_INPUTS(i) {
      if (!IsFloat32Uint32(*i)) continue;
      Node* n = r.jsgraph()->Float32Constant(static_cast<float>(*i));
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat32 | kTypeUint32,
                                                  kRepWord32);
      r.CheckUint32Constant(c, *i);
    }
  }

  {
    FOR_UINT32_INPUTS(i) {
      Node* n = r.jsgraph()->Float64Constant(*i);
      Node* c = r.changer()->GetRepresentationFor(n, kRepFloat64 | kTypeUint32,
                                                  kRepWord32);
      r.CheckUint32Constant(c, *i);
    }
  }

  {
    FOR_UINT32_INPUTS(i) {
      Node* n = r.jsgraph()->Constant(static_cast<double>(*i));
      Node* c = r.changer()->GetRepresentationFor(n, kRepTagged | kTypeUint32,
                                                  kRepWord32);
      r.CheckUint32Constant(c, *i);
    }
  }
}


static void CheckChange(IrOpcode::Value expected, MachineTypeUnion from,
                        MachineTypeUnion to) {
  RepresentationChangerTester r;

  Node* n = r.Parameter();
  Node* c = r.changer()->GetRepresentationFor(n, from, to);

  CHECK_NE(c, n);
  CHECK_EQ(expected, c->opcode());
  CHECK_EQ(n, c->InputAt(0));
}


static void CheckTwoChanges(IrOpcode::Value expected2,
                            IrOpcode::Value expected1, MachineTypeUnion from,
                            MachineTypeUnion to) {
  RepresentationChangerTester r;

  Node* n = r.Parameter();
  Node* c1 = r.changer()->GetRepresentationFor(n, from, to);

  CHECK_NE(c1, n);
  CHECK_EQ(expected1, c1->opcode());
  Node* c2 = c1->InputAt(0);
  CHECK_NE(c2, n);
  CHECK_EQ(expected2, c2->opcode());
  CHECK_EQ(n, c2->InputAt(0));
}


TEST(SingleChanges) {
  CheckChange(IrOpcode::kChangeBoolToBit, kRepTagged, kRepBit);
  CheckChange(IrOpcode::kChangeBitToBool, kRepBit, kRepTagged);

  CheckChange(IrOpcode::kChangeInt32ToTagged, kRepWord32 | kTypeInt32,
              kRepTagged);
  CheckChange(IrOpcode::kChangeUint32ToTagged, kRepWord32 | kTypeUint32,
              kRepTagged);
  CheckChange(IrOpcode::kChangeFloat64ToTagged, kRepFloat64, kRepTagged);

  CheckChange(IrOpcode::kChangeTaggedToInt32, kRepTagged | kTypeInt32,
              kRepWord32);
  CheckChange(IrOpcode::kChangeTaggedToUint32, kRepTagged | kTypeUint32,
              kRepWord32);
  CheckChange(IrOpcode::kChangeTaggedToFloat64, kRepTagged, kRepFloat64);

  // Int32,Uint32 <-> Float64 are actually machine conversions.
  CheckChange(IrOpcode::kChangeInt32ToFloat64, kRepWord32 | kTypeInt32,
              kRepFloat64);
  CheckChange(IrOpcode::kChangeUint32ToFloat64, kRepWord32 | kTypeUint32,
              kRepFloat64);
  CheckChange(IrOpcode::kChangeFloat64ToInt32, kRepFloat64 | kTypeInt32,
              kRepWord32);
  CheckChange(IrOpcode::kChangeFloat64ToUint32, kRepFloat64 | kTypeUint32,
              kRepWord32);

  CheckChange(IrOpcode::kTruncateFloat64ToFloat32, kRepFloat64, kRepFloat32);

  // Int32,Uint32 <-> Float32 require two changes.
  CheckTwoChanges(IrOpcode::kChangeInt32ToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32, kRepWord32 | kTypeInt32,
                  kRepFloat32);
  CheckTwoChanges(IrOpcode::kChangeUint32ToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32, kRepWord32 | kTypeUint32,
                  kRepFloat32);
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToInt32, kRepFloat32 | kTypeInt32,
                  kRepWord32);
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToUint32, kRepFloat32 | kTypeUint32,
                  kRepWord32);

  // Float32 <-> Tagged require two changes.
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToTagged, kRepFloat32, kRepTagged);
  CheckTwoChanges(IrOpcode::kChangeTaggedToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32, kRepTagged, kRepFloat32);
}


TEST(SignednessInWord32) {
  RepresentationChangerTester r;

  // TODO(titzer): assume that uses of a word32 without a sign mean kTypeInt32.
  CheckChange(IrOpcode::kChangeTaggedToInt32, kRepTagged,
              kRepWord32 | kTypeInt32);
  CheckChange(IrOpcode::kChangeTaggedToUint32, kRepTagged,
              kRepWord32 | kTypeUint32);
  CheckChange(IrOpcode::kChangeInt32ToFloat64, kRepWord32, kRepFloat64);
  CheckChange(IrOpcode::kChangeFloat64ToInt32, kRepFloat64, kRepWord32);

  CheckTwoChanges(IrOpcode::kChangeInt32ToFloat64,
                  IrOpcode::kTruncateFloat64ToFloat32, kRepWord32, kRepFloat32);
  CheckTwoChanges(IrOpcode::kChangeFloat32ToFloat64,
                  IrOpcode::kChangeFloat64ToInt32, kRepFloat32, kRepWord32);
}


TEST(Nops) {
  RepresentationChangerTester r;

  // X -> X is always a nop for any single representation X.
  for (size_t i = 0; i < arraysize(all_reps); i++) {
    r.CheckNop(all_reps[i], all_reps[i]);
  }

  // 32-bit floats.
  r.CheckNop(kRepFloat32, kRepFloat32);
  r.CheckNop(kRepFloat32 | kTypeNumber, kRepFloat32);
  r.CheckNop(kRepFloat32, kRepFloat32 | kTypeNumber);

  // 32-bit or 64-bit words can be used as branch conditions (kRepBit).
  r.CheckNop(kRepWord32, kRepBit);
  r.CheckNop(kRepWord32, kRepBit | kTypeBool);
  r.CheckNop(kRepWord64, kRepBit);
  r.CheckNop(kRepWord64, kRepBit | kTypeBool);

  // 32-bit words can be used as smaller word sizes and vice versa, because
  // loads from memory implicitly sign or zero extend the value to the
  // full machine word size, and stores implicitly truncate.
  r.CheckNop(kRepWord32, kRepWord8);
  r.CheckNop(kRepWord32, kRepWord16);
  r.CheckNop(kRepWord32, kRepWord32);
  r.CheckNop(kRepWord8, kRepWord32);
  r.CheckNop(kRepWord16, kRepWord32);

  // kRepBit (result of comparison) is implicitly a wordish thing.
  r.CheckNop(kRepBit, kRepWord8);
  r.CheckNop(kRepBit | kTypeBool, kRepWord8);
  r.CheckNop(kRepBit, kRepWord16);
  r.CheckNop(kRepBit | kTypeBool, kRepWord16);
  r.CheckNop(kRepBit, kRepWord32);
  r.CheckNop(kRepBit | kTypeBool, kRepWord32);
  r.CheckNop(kRepBit, kRepWord64);
  r.CheckNop(kRepBit | kTypeBool, kRepWord64);
}


TEST(TypeErrors) {
  RepresentationChangerTester r;

  // Floats cannot be implicitly converted to/from comparison conditions.
  r.CheckTypeError(kRepFloat64, kRepBit);
  r.CheckTypeError(kRepFloat64, kRepBit | kTypeBool);
  r.CheckTypeError(kRepBit, kRepFloat64);
  r.CheckTypeError(kRepBit | kTypeBool, kRepFloat64);

  // Floats cannot be implicitly converted to/from comparison conditions.
  r.CheckTypeError(kRepFloat32, kRepBit);
  r.CheckTypeError(kRepFloat32, kRepBit | kTypeBool);
  r.CheckTypeError(kRepBit, kRepFloat32);
  r.CheckTypeError(kRepBit | kTypeBool, kRepFloat32);

  // Word64 is internal and shouldn't be implicitly converted.
  r.CheckTypeError(kRepWord64, kRepTagged | kTypeBool);
  r.CheckTypeError(kRepWord64, kRepTagged);
  r.CheckTypeError(kRepWord64, kRepTagged | kTypeBool);
  r.CheckTypeError(kRepTagged, kRepWord64);
  r.CheckTypeError(kRepTagged | kTypeBool, kRepWord64);

  // Word64 / Word32 shouldn't be implicitly converted.
  r.CheckTypeError(kRepWord64, kRepWord32);
  r.CheckTypeError(kRepWord32, kRepWord64);
  r.CheckTypeError(kRepWord64, kRepWord32 | kTypeInt32);
  r.CheckTypeError(kRepWord32 | kTypeInt32, kRepWord64);
  r.CheckTypeError(kRepWord64, kRepWord32 | kTypeUint32);
  r.CheckTypeError(kRepWord32 | kTypeUint32, kRepWord64);

  for (size_t i = 0; i < arraysize(all_reps); i++) {
    for (size_t j = 0; j < arraysize(all_reps); j++) {
      if (i == j) continue;
      // Only a single from representation is allowed.
      r.CheckTypeError(all_reps[i] | all_reps[j], kRepTagged);
    }
  }
}
