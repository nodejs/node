// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/int64-lowering.h"

#include "src/codegen/interface-descriptors.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/signature.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/wasm-compiler.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-module.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::AllOf;
using testing::Capture;
using testing::CaptureEq;

namespace v8 {
namespace internal {
namespace compiler {

class Int64LoweringTest : public GraphTest {
 public:
  Int64LoweringTest()
      : GraphTest(),
        machine_(zone(), MachineRepresentation::kWord32,
                 MachineOperatorBuilder::Flag::kAllOptionalOps) {
    value_[0] = 0x1234567890ABCDEF;
    value_[1] = 0x1EDCBA098765432F;
    value_[2] = 0x1133557799886644;
  }

  MachineOperatorBuilder* machine() { return &machine_; }

  void LowerGraph(Node* node, Signature<MachineRepresentation>* signature) {
    Node* zero = graph()->NewNode(common()->Int32Constant(0));
    Node* ret = graph()->NewNode(common()->Return(), zero, node,
                                 graph()->start(), graph()->start());
    NodeProperties::MergeControlToEnd(graph(), common(), ret);

    Int64Lowering lowering(graph(), machine(), common(), zone(), signature);
    lowering.LowerGraph();
  }

  void LowerGraphWithSpecialCase(
      Node* node, std::unique_ptr<Int64LoweringSpecialCase> special_case,
      MachineRepresentation rep) {
    Node* zero = graph()->NewNode(common()->Int32Constant(0));
    Node* ret = graph()->NewNode(common()->Return(), zero, node,
                                 graph()->start(), graph()->start());
    NodeProperties::MergeControlToEnd(graph(), common(), ret);

    // Create a signature for the outer wasm<>js call; for these tests we focus
    // on lowering the special cases rather than the wrapper node at the
    // JavaScript boundaries.
    Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 0);
    sig_builder.AddReturn(rep);

    Int64Lowering lowering(graph(), machine(), common(), zone(),
                           sig_builder.Build(), std::move(special_case));
    lowering.LowerGraph();
  }

  void LowerGraph(Node* node, MachineRepresentation return_type,
                  MachineRepresentation rep = MachineRepresentation::kWord32,
                  int num_params = 0) {
    Signature<MachineRepresentation>::Builder sig_builder(zone(), 1,
                                                          num_params);
    sig_builder.AddReturn(return_type);
    for (int i = 0; i < num_params; i++) {
      sig_builder.AddParam(rep);
    }
    LowerGraph(node, sig_builder.Build());
  }

  void CompareCallDescriptors(const CallDescriptor* lhs,
                              const CallDescriptor* rhs) {
    EXPECT_THAT(lhs->CalleeSavedFPRegisters(), rhs->CalleeSavedFPRegisters());
    EXPECT_THAT(lhs->CalleeSavedRegisters(), rhs->CalleeSavedRegisters());
    EXPECT_THAT(lhs->FrameStateCount(), rhs->FrameStateCount());
    EXPECT_THAT(lhs->InputCount(), rhs->InputCount());
    for (size_t i = 0; i < lhs->InputCount(); i++) {
      EXPECT_THAT(lhs->GetInputLocation(i), rhs->GetInputLocation(i));
      EXPECT_THAT(lhs->GetInputType(i), rhs->GetInputType(i));
    }
    EXPECT_THAT(lhs->ReturnCount(), rhs->ReturnCount());
    for (size_t i = 0; i < lhs->ReturnCount(); i++) {
      EXPECT_THAT(lhs->GetReturnLocation(i), rhs->GetReturnLocation(i));
      EXPECT_THAT(lhs->GetReturnType(i), rhs->GetReturnType(i));
    }
    EXPECT_THAT(lhs->flags(), rhs->flags());
    EXPECT_THAT(lhs->kind(), rhs->kind());
  }

  int64_t value(int i) { return value_[i]; }

  int32_t low_word_value(int i) {
    return static_cast<int32_t>(value_[i] & 0xFFFFFFFF);
  }

  int32_t high_word_value(int i) {
    return static_cast<int32_t>(value_[i] >> 32);
  }

  void TestComparison(
      const Operator* op,
      Matcher<Node*> (*high_word_matcher)(const Matcher<Node*>& lhs_matcher,
                                          const Matcher<Node*>& rhs_matcher),
      Matcher<Node*> (*low_word_matcher)(const Matcher<Node*>& lhs_matcher,
                                         const Matcher<Node*>& rhs_matcher)) {
    LowerGraph(
        graph()->NewNode(op, Int64Constant(value(0)), Int64Constant(value(1))),
        MachineRepresentation::kWord32);
    EXPECT_THAT(
        graph()->end()->InputAt(1),
        IsReturn(IsWord32Or(
                     high_word_matcher(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(high_word_value(1))),
                     IsWord32And(
                         IsWord32Equal(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(high_word_value(1))),
                         low_word_matcher(IsInt32Constant(low_word_value(0)),
                                          IsInt32Constant(low_word_value(1))))),
                 start(), start()));
  }

 private:
  MachineOperatorBuilder machine_;
  int64_t value_[3];
};

TEST_F(Int64LoweringTest, Int64Constant) {
  LowerGraph(Int64Constant(value(0)), MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)),
                        IsInt32Constant(high_word_value(0)), start(), start()));
}

#if defined(V8_TARGET_LITTLE_ENDIAN)
#define LOAD_VERIFY(kLoad)                                                     \
  Matcher<Node*> high_word_load_matcher =                                      \
      Is##kLoad(MachineType::Int32(), IsInt32Constant(base),                   \
                IsInt32Add(IsInt32Constant(index), IsInt32Constant(0x4)),      \
                start(), start());                                             \
                                                                               \
  EXPECT_THAT(                                                                 \
      graph()->end()->InputAt(1),                                              \
      IsReturn2(                                                               \
          Is##kLoad(MachineType::Int32(), IsInt32Constant(base),               \
                    IsInt32Constant(index),                                    \
                    AllOf(CaptureEq(&high_word_load), high_word_load_matcher), \
                    start()),                                                  \
          AllOf(CaptureEq(&high_word_load), high_word_load_matcher), start(),  \
          start()));
#elif defined(V8_TARGET_BIG_ENDIAN)
#define LOAD_VERIFY(kLoad)                                                     \
  Matcher<Node*> high_word_load_matcher =                                      \
      Is##kLoad(MachineType::Int32(), IsInt32Constant(base),                   \
                IsInt32Constant(index), start(), start());                     \
                                                                               \
  EXPECT_THAT(                                                                 \
      graph()->end()->InputAt(1),                                              \
      IsReturn2(                                                               \
          Is##kLoad(MachineType::Int32(), IsInt32Constant(base),               \
                    IsInt32Add(IsInt32Constant(index), IsInt32Constant(0x4)),  \
                    AllOf(CaptureEq(&high_word_load), high_word_load_matcher), \
                    start()),                                                  \
          AllOf(CaptureEq(&high_word_load), high_word_load_matcher), start(),  \
          start()));
#endif

#define INT64_LOAD_LOWERING(kLoad)                                       \
  int32_t base = 0x1234;                                                 \
  int32_t index = 0x5678;                                                \
                                                                         \
  LowerGraph(graph()->NewNode(machine()->kLoad(MachineType::Int64()),    \
                              Int32Constant(base), Int32Constant(index), \
                              start(), start()),                         \
             MachineRepresentation::kWord64);                            \
                                                                         \
  Capture<Node*> high_word_load;                                         \
  LOAD_VERIFY(kLoad)

TEST_F(Int64LoweringTest, Int64Load) { INT64_LOAD_LOWERING(Load); }

TEST_F(Int64LoweringTest, UnalignedInt64Load) {
  INT64_LOAD_LOWERING(UnalignedLoad);
}

#if defined(V8_TARGET_LITTLE_ENDIAN)
#define STORE_VERIFY(kStore, kRep)                                             \
  EXPECT_THAT(                                                                 \
      graph()->end()->InputAt(1),                                              \
      IsReturn(IsInt32Constant(return_value),                                  \
               Is##kStore(                                                     \
                   kRep, IsInt32Constant(base), IsInt32Constant(index),        \
                   IsInt32Constant(low_word_value(0)),                         \
                   Is##kStore(                                                 \
                       kRep, IsInt32Constant(base),                            \
                       IsInt32Add(IsInt32Constant(index), IsInt32Constant(4)), \
                       IsInt32Constant(high_word_value(0)), start(), start()), \
                   start()),                                                   \
               start()));
#elif defined(V8_TARGET_BIG_ENDIAN)
#define STORE_VERIFY(kStore, kRep)                                             \
  EXPECT_THAT(                                                                 \
      graph()->end()->InputAt(1),                                              \
      IsReturn(IsInt32Constant(return_value),                                  \
               Is##kStore(                                                     \
                   kRep, IsInt32Constant(base),                                \
                   IsInt32Add(IsInt32Constant(index), IsInt32Constant(4)),     \
                   IsInt32Constant(low_word_value(0)),                         \
                   Is##kStore(                                                 \
                       kRep, IsInt32Constant(base), IsInt32Constant(index),    \
                       IsInt32Constant(high_word_value(0)), start(), start()), \
                   start()),                                                   \
               start()));
#endif

#define INT64_STORE_LOWERING(kStore, kRep32, kRep64)                         \
  int32_t base = 1111;                                                       \
  int32_t index = 2222;                                                      \
  int32_t return_value = 0x5555;                                             \
                                                                             \
  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 0);       \
  sig_builder.AddReturn(MachineRepresentation::kWord32);                     \
                                                                             \
  Node* store = graph()->NewNode(machine()->kStore(kRep64),                  \
                                 Int32Constant(base), Int32Constant(index),  \
                                 Int64Constant(value(0)), start(), start()); \
                                                                             \
  Node* zero = graph()->NewNode(common()->Int32Constant(0));                 \
  Node* ret = graph()->NewNode(common()->Return(), zero,                     \
                               Int32Constant(return_value), store, start()); \
                                                                             \
  NodeProperties::MergeControlToEnd(graph(), common(), ret);                 \
                                                                             \
  Int64Lowering lowering(graph(), machine(), common(), zone(),               \
                         sig_builder.Build());                               \
  lowering.LowerGraph();                                                     \
                                                                             \
  STORE_VERIFY(kStore, kRep32)

TEST_F(Int64LoweringTest, Int64Store) {
  const StoreRepresentation rep64(MachineRepresentation::kWord64,
                                  WriteBarrierKind::kNoWriteBarrier);
  const StoreRepresentation rep32(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier);
  INT64_STORE_LOWERING(Store, rep32, rep64);
}

TEST_F(Int64LoweringTest, Int32Store) {
  const StoreRepresentation rep32(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier);
  int32_t base = 1111;
  int32_t index = 2222;
  int32_t return_value = 0x5555;

  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 0);
  sig_builder.AddReturn(MachineRepresentation::kWord32);

  Node* store = graph()->NewNode(machine()->Store(rep32), Int32Constant(base),
                                 Int32Constant(index), Int64Constant(value(0)),
                                 start(), start());

  Node* zero = graph()->NewNode(common()->Int32Constant(0));
  Node* ret = graph()->NewNode(common()->Return(), zero,
                               Int32Constant(return_value), store, start());

  NodeProperties::MergeControlToEnd(graph(), common(), ret);

  Int64Lowering lowering(graph(), machine(), common(), zone(),
                         sig_builder.Build());
  lowering.LowerGraph();

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsInt32Constant(return_value),
               IsStore(rep32, IsInt32Constant(base), IsInt32Constant(index),
                       IsInt32Constant(low_word_value(0)), start(), start()),
               start()));
}

TEST_F(Int64LoweringTest, Int64UnalignedStore) {
  const UnalignedStoreRepresentation rep64(MachineRepresentation::kWord64);
  const UnalignedStoreRepresentation rep32(MachineRepresentation::kWord32);
  INT64_STORE_LOWERING(UnalignedStore, rep32, rep64);
}

TEST_F(Int64LoweringTest, Int64And) {
  LowerGraph(graph()->NewNode(machine()->Word64And(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsWord32And(IsInt32Constant(low_word_value(0)),
                                    IsInt32Constant(low_word_value(1))),
                        IsWord32And(IsInt32Constant(high_word_value(0)),
                                    IsInt32Constant(high_word_value(1))),
                        start(), start()));
}

TEST_F(Int64LoweringTest, TruncateInt64ToInt32) {
  LowerGraph(graph()->NewNode(machine()->TruncateInt64ToInt32(),
                              Int64Constant(value(0))),
             MachineRepresentation::kWord32);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(IsInt32Constant(low_word_value(0)), start(), start()));
}

TEST_F(Int64LoweringTest, Parameter) {
  LowerGraph(Parameter(1), MachineRepresentation::kWord64,
             MachineRepresentation::kWord64, 1);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsParameter(1), IsParameter(2), start(), start()));
}

TEST_F(Int64LoweringTest, Parameter2) {
  Signature<MachineRepresentation>::Builder sig_builder(zone(), 1, 5);
  sig_builder.AddReturn(MachineRepresentation::kWord32);

  sig_builder.AddParam(MachineRepresentation::kWord32);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kFloat64);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kWord32);

  int start_parameter = start()->op()->ValueOutputCount();
  LowerGraph(Parameter(5), sig_builder.Build());

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(IsParameter(7), start(), start()));
  // The parameter of the start node should increase by 2, because we lowered
  // two parameter nodes.
  EXPECT_THAT(start()->op()->ValueOutputCount(), start_parameter + 2);
}

TEST_F(Int64LoweringTest, ParameterWithJSContextParam) {
  Signature<MachineRepresentation>::Builder sig_builder(zone(), 0, 2);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kWord64);

  auto sig = sig_builder.Build();

  Node* js_context = graph()->NewNode(
      common()->Parameter(Linkage::GetJSCallContextParamIndex(
                              static_cast<int>(sig->parameter_count()) + 1),
                          "%context"),
      start());
  LowerGraph(js_context, sig);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(js_context, start(), start()));
}

TEST_F(Int64LoweringTest, ParameterWithJSClosureParam) {
  Signature<MachineRepresentation>::Builder sig_builder(zone(), 0, 2);
  sig_builder.AddParam(MachineRepresentation::kWord64);
  sig_builder.AddParam(MachineRepresentation::kWord64);

  auto sig = sig_builder.Build();

  Node* js_closure = graph()->NewNode(
      common()->Parameter(Linkage::kJSCallClosureParamIndex, "%closure"),
      start());
  LowerGraph(js_closure, sig);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(js_closure, start(), start()));
}

// The following tests assume that pointers are 32 bit and therefore pointers do
// not get lowered. This assumption does not hold on 64 bit platforms, which
// invalidates these tests.
// TODO(wasm): We can find an alternative to re-activate these tests.
#if V8_TARGET_ARCH_32_BIT
TEST_F(Int64LoweringTest, CallI64Return) {
  int32_t function = 0x9999;
  Node* context_address = Int32Constant(0);

  wasm::FunctionSig::Builder sig_builder(zone(), 1, 0);
  sig_builder.AddReturn(wasm::kWasmI64);

  auto call_descriptor =
      compiler::GetWasmCallDescriptor(zone(), sig_builder.Build());

  LowerGraph(
      graph()->NewNode(common()->Call(call_descriptor), Int32Constant(function),
                       context_address, start(), start()),
      MachineRepresentation::kWord64);

  Capture<Node*> call;
  Matcher<Node*> call_matcher =
      IsCall(testing::_, IsInt32Constant(function), start(), start());

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&call), call_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&call), call_matcher)),
                        start(), start()));

  CompareCallDescriptors(
      CallDescriptorOf(
          graph()->end()->InputAt(1)->InputAt(1)->InputAt(0)->op()),
      compiler::GetI32WasmCallDescriptor(zone(), call_descriptor));
}

TEST_F(Int64LoweringTest, CallI64Parameter) {
  int32_t function = 0x9999;
  Node* context_address = Int32Constant(0);

  wasm::FunctionSig::Builder sig_builder(zone(), 1, 3);
  sig_builder.AddReturn(wasm::kWasmI32);
  sig_builder.AddParam(wasm::kWasmI64);
  sig_builder.AddParam(wasm::kWasmI32);
  sig_builder.AddParam(wasm::kWasmI64);

  auto call_descriptor =
      compiler::GetWasmCallDescriptor(zone(), sig_builder.Build());

  LowerGraph(
      graph()->NewNode(common()->Call(call_descriptor), Int32Constant(function),
                       context_address, Int64Constant(value(0)),
                       Int32Constant(low_word_value(1)),
                       Int64Constant(value(2)), start(), start()),
      MachineRepresentation::kWord32);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsCall(testing::_, IsInt32Constant(function), context_address,
                      IsInt32Constant(low_word_value(0)),
                      IsInt32Constant(high_word_value(0)),
                      IsInt32Constant(low_word_value(1)),
                      IsInt32Constant(low_word_value(2)),
                      IsInt32Constant(high_word_value(2)), start(), start()),
               start(), start()));

  CompareCallDescriptors(
      CallDescriptorOf(graph()->end()->InputAt(1)->InputAt(1)->op()),
      compiler::GetI32WasmCallDescriptor(zone(), call_descriptor));
}

TEST_F(Int64LoweringTest, Int64Add) {
  LowerGraph(graph()->NewNode(machine()->Int64Add(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);

  Capture<Node*> add;
  Matcher<Node*> add_matcher = IsInt32PairAdd(
      IsInt32Constant(low_word_value(0)), IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(1)), IsInt32Constant(high_word_value(1)));

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&add), add_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&add), add_matcher)),
                        start(), start()));
}
#endif

TEST_F(Int64LoweringTest, Int64Sub) {
  LowerGraph(graph()->NewNode(machine()->Int64Sub(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);

  Capture<Node*> sub;
  Matcher<Node*> sub_matcher = IsInt32PairSub(
      IsInt32Constant(low_word_value(0)), IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(1)), IsInt32Constant(high_word_value(1)));

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&sub), sub_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&sub), sub_matcher)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, Int64Mul) {
  LowerGraph(graph()->NewNode(machine()->Int64Mul(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);

  Capture<Node*> mul_capture;
  Matcher<Node*> mul_matcher = IsInt32PairMul(
      IsInt32Constant(low_word_value(0)), IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(1)), IsInt32Constant(high_word_value(1)));

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsProjection(0, AllOf(CaptureEq(&mul_capture), mul_matcher)),
                IsProjection(1, AllOf(CaptureEq(&mul_capture), mul_matcher)),
                start(), start()));
}

TEST_F(Int64LoweringTest, Int64Ior) {
  LowerGraph(graph()->NewNode(machine()->Word64Or(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsWord32Or(IsInt32Constant(low_word_value(0)),
                                   IsInt32Constant(low_word_value(1))),
                        IsWord32Or(IsInt32Constant(high_word_value(0)),
                                   IsInt32Constant(high_word_value(1))),
                        start(), start()));
}

TEST_F(Int64LoweringTest, Int64Xor) {
  LowerGraph(graph()->NewNode(machine()->Word64Xor(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsWord32Xor(IsInt32Constant(low_word_value(0)),
                                    IsInt32Constant(low_word_value(1))),
                        IsWord32Xor(IsInt32Constant(high_word_value(0)),
                                    IsInt32Constant(high_word_value(1))),
                        start(), start()));
}

TEST_F(Int64LoweringTest, Int64Shl) {
  LowerGraph(graph()->NewNode(machine()->Word64Shl(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);

  Capture<Node*> shl;
  Matcher<Node*> shl_matcher = IsWord32PairShl(
      IsInt32Constant(low_word_value(0)), IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(1)));

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&shl), shl_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&shl), shl_matcher)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, Int64ShrU) {
  LowerGraph(graph()->NewNode(machine()->Word64Shr(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);

  Capture<Node*> shr;
  Matcher<Node*> shr_matcher = IsWord32PairShr(
      IsInt32Constant(low_word_value(0)), IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(1)));

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&shr), shr_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&shr), shr_matcher)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, Int64ShrS) {
  LowerGraph(graph()->NewNode(machine()->Word64Sar(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord64);

  Capture<Node*> sar;
  Matcher<Node*> sar_matcher = IsWord32PairSar(
      IsInt32Constant(low_word_value(0)), IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(1)));

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&sar), sar_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&sar), sar_matcher)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, Int64Eq) {
  LowerGraph(graph()->NewNode(machine()->Word64Equal(), Int64Constant(value(0)),
                              Int64Constant(value(1))),
             MachineRepresentation::kWord32);
  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsWord32Equal(
                   IsWord32Or(IsWord32Xor(IsInt32Constant(low_word_value(0)),
                                          IsInt32Constant(low_word_value(1))),
                              IsWord32Xor(IsInt32Constant(high_word_value(0)),
                                          IsInt32Constant(high_word_value(1)))),
                   IsInt32Constant(0)),
               start(), start()));
}

TEST_F(Int64LoweringTest, Int64LtS) {
  TestComparison(machine()->Int64LessThan(), IsInt32LessThan, IsUint32LessThan);
}

TEST_F(Int64LoweringTest, Int64LeS) {
  TestComparison(machine()->Int64LessThanOrEqual(), IsInt32LessThan,
                 IsUint32LessThanOrEqual);
}

TEST_F(Int64LoweringTest, Int64LtU) {
  TestComparison(machine()->Uint64LessThan(), IsUint32LessThan,
                 IsUint32LessThan);
}

TEST_F(Int64LoweringTest, Int64LeU) {
  TestComparison(machine()->Uint64LessThanOrEqual(), IsUint32LessThan,
                 IsUint32LessThanOrEqual);
}

TEST_F(Int64LoweringTest, I32ConvertI64) {
  LowerGraph(graph()->NewNode(machine()->TruncateInt64ToInt32(),
                              Int64Constant(value(0))),
             MachineRepresentation::kWord32);
  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn(IsInt32Constant(low_word_value(0)), start(), start()));
}

TEST_F(Int64LoweringTest, I64SConvertI32) {
  LowerGraph(graph()->NewNode(machine()->ChangeInt32ToInt64(),
                              Int32Constant(low_word_value(0))),
             MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)),
                        IsWord32Sar(IsInt32Constant(low_word_value(0)),
                                    IsInt32Constant(31)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, I64SConvertI32_2) {
  LowerGraph(
      graph()->NewNode(machine()->ChangeInt32ToInt64(),
                       graph()->NewNode(machine()->TruncateInt64ToInt32(),
                                        Int64Constant(value(0)))),
      MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)),
                        IsWord32Sar(IsInt32Constant(low_word_value(0)),
                                    IsInt32Constant(31)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, I64UConvertI32) {
  LowerGraph(graph()->NewNode(machine()->ChangeUint32ToUint64(),
                              Int32Constant(low_word_value(0))),
             MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)), IsInt32Constant(0),
                        start(), start()));
}

TEST_F(Int64LoweringTest, I64UConvertI32_2) {
  LowerGraph(
      graph()->NewNode(machine()->ChangeUint32ToUint64(),
                       graph()->NewNode(machine()->TruncateInt64ToInt32(),
                                        Int64Constant(value(0)))),
      MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)), IsInt32Constant(0),
                        start(), start()));
}

TEST_F(Int64LoweringTest, F64ReinterpretI64) {
  LowerGraph(graph()->NewNode(machine()->BitcastInt64ToFloat64(),
                              Int64Constant(value(0))),
             MachineRepresentation::kFloat64);

  Capture<Node*> stack_slot_capture;
  Matcher<Node*> stack_slot_matcher =
      IsStackSlot(StackSlotRepresentation(sizeof(int64_t), 0));

  Capture<Node*> store_capture;
  Matcher<Node*> store_matcher =
      IsStore(StoreRepresentation(MachineRepresentation::kWord32,
                                  WriteBarrierKind::kNoWriteBarrier),
              AllOf(CaptureEq(&stack_slot_capture), stack_slot_matcher),
              IsInt32Constant(kInt64LowerHalfMemoryOffset),
              IsInt32Constant(low_word_value(0)),
              IsStore(StoreRepresentation(MachineRepresentation::kWord32,
                                          WriteBarrierKind::kNoWriteBarrier),
                      AllOf(CaptureEq(&stack_slot_capture), stack_slot_matcher),
                      IsInt32Constant(kInt64UpperHalfMemoryOffset),
                      IsInt32Constant(high_word_value(0)), start(), start()),
              start());

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsLoad(MachineType::Float64(),
                      AllOf(CaptureEq(&stack_slot_capture), stack_slot_matcher),
                      IsInt32Constant(0),
                      AllOf(CaptureEq(&store_capture), store_matcher), start()),
               start(), start()));
}

TEST_F(Int64LoweringTest, I64ReinterpretF64) {
  LowerGraph(graph()->NewNode(machine()->BitcastFloat64ToInt64(),
                              Float64Constant(bit_cast<double>(value(0)))),
             MachineRepresentation::kWord64);

  Capture<Node*> stack_slot;
  Matcher<Node*> stack_slot_matcher =
      IsStackSlot(StackSlotRepresentation(sizeof(int64_t), 0));

  Capture<Node*> store;
  Matcher<Node*> store_matcher = IsStore(
      StoreRepresentation(MachineRepresentation::kFloat64,
                          WriteBarrierKind::kNoWriteBarrier),
      AllOf(CaptureEq(&stack_slot), stack_slot_matcher), IsInt32Constant(0),
      IsFloat64Constant(bit_cast<double>(value(0))), start(), start());

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsLoad(MachineType::Int32(),
                       AllOf(CaptureEq(&stack_slot), stack_slot_matcher),
                       IsInt32Constant(kInt64LowerHalfMemoryOffset),
                       AllOf(CaptureEq(&store), store_matcher), start()),
                IsLoad(MachineType::Int32(),
                       AllOf(CaptureEq(&stack_slot), stack_slot_matcher),
                       IsInt32Constant(kInt64UpperHalfMemoryOffset),
                       AllOf(CaptureEq(&store), store_matcher), start()),
                start(), start()));
}

TEST_F(Int64LoweringTest, I64Clz) {
  LowerGraph(graph()->NewNode(machine()->Word64Clz(), Int64Constant(value(0))),
             MachineRepresentation::kWord64);

  Capture<Node*> branch_capture;
  Matcher<Node*> branch_matcher = IsBranch(
      IsWord32Equal(IsInt32Constant(high_word_value(0)), IsInt32Constant(0)),
      start());

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(
          IsPhi(MachineRepresentation::kWord32,
                IsInt32Add(IsWord32Clz(IsInt32Constant(low_word_value(0))),
                           IsInt32Constant(32)),
                IsWord32Clz(IsInt32Constant(high_word_value(0))),
                IsMerge(
                    IsIfTrue(AllOf(CaptureEq(&branch_capture), branch_matcher)),
                    IsIfFalse(
                        AllOf(CaptureEq(&branch_capture), branch_matcher)))),
          IsInt32Constant(0), start(), start()));
}

TEST_F(Int64LoweringTest, I64Ctz) {
  LowerGraph(graph()->NewNode(machine()->Word64Ctz().placeholder(),
                              Int64Constant(value(0))),
             MachineRepresentation::kWord64);
  Capture<Node*> branch_capture;
  Matcher<Node*> branch_matcher = IsBranch(
      IsWord32Equal(IsInt32Constant(low_word_value(0)), IsInt32Constant(0)),
      start());
  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(
          IsPhi(MachineRepresentation::kWord32,
                IsInt32Add(IsWord32Ctz(IsInt32Constant(high_word_value(0))),
                           IsInt32Constant(32)),
                IsWord32Ctz(IsInt32Constant(low_word_value(0))),
                IsMerge(
                    IsIfTrue(AllOf(CaptureEq(&branch_capture), branch_matcher)),
                    IsIfFalse(
                        AllOf(CaptureEq(&branch_capture), branch_matcher)))),
          IsInt32Constant(0), start(), start()));
}

TEST_F(Int64LoweringTest, Dfs) {
  Node* common = Int64Constant(value(0));
  LowerGraph(graph()->NewNode(machine()->Word64And(), common,
                              graph()->NewNode(machine()->Word64And(), common,
                                               Int64Constant(value(1)))),
             MachineRepresentation::kWord64);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsWord32And(IsInt32Constant(low_word_value(0)),
                            IsWord32And(IsInt32Constant(low_word_value(0)),
                                        IsInt32Constant(low_word_value(1)))),
                IsWord32And(IsInt32Constant(high_word_value(0)),
                            IsWord32And(IsInt32Constant(high_word_value(0)),
                                        IsInt32Constant(high_word_value(1)))),
                start(), start()));
}

TEST_F(Int64LoweringTest, I64Popcnt) {
  LowerGraph(graph()->NewNode(machine()->Word64Popcnt().placeholder(),
                              Int64Constant(value(0))),
             MachineRepresentation::kWord64);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsInt32Add(IsWord32Popcnt(IsInt32Constant(low_word_value(0))),
                           IsWord32Popcnt(IsInt32Constant(high_word_value(0)))),
                IsInt32Constant(0), start(), start()));
}

TEST_F(Int64LoweringTest, I64Ror) {
  LowerGraph(graph()->NewNode(machine()->Word64Ror(), Int64Constant(value(0)),
                              Parameter(0)),
             MachineRepresentation::kWord64, MachineRepresentation::kWord64, 1);

  Matcher<Node*> branch_lt32_matcher =
      IsBranch(IsInt32LessThan(IsParameter(0), IsInt32Constant(32)), start());

  Matcher<Node*> low_input_matcher = IsPhi(
      MachineRepresentation::kWord32, IsInt32Constant(low_word_value(0)),
      IsInt32Constant(high_word_value(0)),
      IsMerge(IsIfTrue(branch_lt32_matcher), IsIfFalse(branch_lt32_matcher)));

  Matcher<Node*> high_input_matcher = IsPhi(
      MachineRepresentation::kWord32, IsInt32Constant(high_word_value(0)),
      IsInt32Constant(low_word_value(0)),
      IsMerge(IsIfTrue(branch_lt32_matcher), IsIfFalse(branch_lt32_matcher)));

  Matcher<Node*> shift_matcher =
      IsWord32And(IsParameter(0), IsInt32Constant(0x1F));

  Matcher<Node*> bit_mask_matcher = IsWord32Shl(
      IsWord32Sar(IsInt32Constant(std::numeric_limits<int32_t>::min()),
                  shift_matcher),
      IsInt32Constant(1));

  Matcher<Node*> inv_mask_matcher =
      IsWord32Xor(bit_mask_matcher, IsInt32Constant(-1));

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(
          IsWord32Or(IsWord32And(IsWord32Ror(low_input_matcher, shift_matcher),
                                 inv_mask_matcher),
                     IsWord32And(IsWord32Ror(high_input_matcher, shift_matcher),
                                 bit_mask_matcher)),
          IsWord32Or(IsWord32And(IsWord32Ror(high_input_matcher, shift_matcher),
                                 inv_mask_matcher),
                     IsWord32And(IsWord32Ror(low_input_matcher, shift_matcher),
                                 bit_mask_matcher)),
          start(), start()));
}

TEST_F(Int64LoweringTest, I64Ror_0) {
  LowerGraph(graph()->NewNode(machine()->Word64Ror(), Int64Constant(value(0)),
                              Int32Constant(0)),
             MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(low_word_value(0)),
                        IsInt32Constant(high_word_value(0)), start(), start()));
}

TEST_F(Int64LoweringTest, I64Ror_32) {
  LowerGraph(graph()->NewNode(machine()->Word64Ror(), Int64Constant(value(0)),
                              Int32Constant(32)),
             MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsInt32Constant(high_word_value(0)),
                        IsInt32Constant(low_word_value(0)), start(), start()));
}

TEST_F(Int64LoweringTest, I64Ror_11) {
  LowerGraph(graph()->NewNode(machine()->Word64Ror(), Int64Constant(value(0)),
                              Int32Constant(11)),
             MachineRepresentation::kWord64);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsWord32Or(IsWord32Shr(IsInt32Constant(low_word_value(0)),
                                       IsInt32Constant(11)),
                           IsWord32Shl(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(21))),
                IsWord32Or(IsWord32Shr(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(11)),
                           IsWord32Shl(IsInt32Constant(low_word_value(0)),
                                       IsInt32Constant(21))),
                start(), start()));
}

TEST_F(Int64LoweringTest, I64Ror_43) {
  LowerGraph(graph()->NewNode(machine()->Word64Ror(), Int64Constant(value(0)),
                              Int32Constant(43)),
             MachineRepresentation::kWord64);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsWord32Or(IsWord32Shr(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(11)),
                           IsWord32Shl(IsInt32Constant(low_word_value(0)),
                                       IsInt32Constant(21))),
                IsWord32Or(IsWord32Shr(IsInt32Constant(low_word_value(0)),
                                       IsInt32Constant(11)),
                           IsWord32Shl(IsInt32Constant(high_word_value(0)),
                                       IsInt32Constant(21))),
                start(), start()));
}

TEST_F(Int64LoweringTest, I64PhiWord64) {
  LowerGraph(graph()->NewNode(common()->Phi(MachineRepresentation::kWord64, 2),
                              Int64Constant(value(0)), Int64Constant(value(1)),
                              start()),
             MachineRepresentation::kWord64);

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsPhi(MachineRepresentation::kWord32,
                              IsInt32Constant(low_word_value(0)),
                              IsInt32Constant(low_word_value(1)), start()),
                        IsPhi(MachineRepresentation::kWord32,
                              IsInt32Constant(high_word_value(0)),
                              IsInt32Constant(high_word_value(1)), start()),
                        start(), start()));
}

void TestPhi(Int64LoweringTest* test, MachineRepresentation rep, Node* v1,
             Node* v2) {
  test->LowerGraph(test->graph()->NewNode(test->common()->Phi(rep, 2), v1, v2,
                                          test->start()),
                   rep);

  EXPECT_THAT(test->graph()->end()->InputAt(1),
              IsReturn(IsPhi(rep, v1, v2, test->start()), test->start(),
                       test->start()));
}

TEST_F(Int64LoweringTest, I64PhiFloat32) {
  TestPhi(this, MachineRepresentation::kFloat32, Float32Constant(1.5),
          Float32Constant(2.5));
}

TEST_F(Int64LoweringTest, I64PhiFloat64) {
  TestPhi(this, MachineRepresentation::kFloat64, Float32Constant(1.5),
          Float32Constant(2.5));
}

TEST_F(Int64LoweringTest, I64PhiWord32) {
  TestPhi(this, MachineRepresentation::kWord32, Float32Constant(1),
          Float32Constant(2));
}

TEST_F(Int64LoweringTest, I64ReverseBytes) {
  LowerGraph(graph()->NewNode(machine()->Word64ReverseBytes(),
                              Int64Constant(value(0))),
             MachineRepresentation::kWord64);
  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn2(IsWord32ReverseBytes(IsInt32Constant(high_word_value(0))),
                IsWord32ReverseBytes(IsInt32Constant(low_word_value(0))),
                start(), start()));
}

TEST_F(Int64LoweringTest, EffectPhiLoop) {
  // Construct a cycle consisting of an EffectPhi, a Store, and a Load.
  Node* eff_phi = graph()->NewNode(common()->EffectPhi(1), graph()->start(),
                                   graph()->start());

  StoreRepresentation store_rep(MachineRepresentation::kWord64,
                                WriteBarrierKind::kNoWriteBarrier);
  LoadRepresentation load_rep(MachineType::Int64());

  Node* load =
      graph()->NewNode(machine()->Load(load_rep), Int64Constant(value(0)),
                       Int64Constant(value(1)), eff_phi, graph()->start());

  Node* store =
      graph()->NewNode(machine()->Store(store_rep), Int64Constant(value(0)),
                       Int64Constant(value(1)), load, load, graph()->start());

  eff_phi->InsertInput(zone(), 1, store);
  NodeProperties::ChangeOp(eff_phi,
                           common()->ResizeMergeOrPhi(eff_phi->op(), 2));

  LowerGraph(load, MachineRepresentation::kWord64);
}

TEST_F(Int64LoweringTest, LoopCycle) {
  // New node with two placeholders.
  Node* compare = graph()->NewNode(machine()->Word64Equal(), Int64Constant(0),
                                   Int64Constant(value(0)));

  Node* load = graph()->NewNode(
      machine()->Load(MachineType::Int64()), Int64Constant(value(1)),
      Int64Constant(value(2)), graph()->start(),
      graph()->NewNode(
          common()->Loop(2), graph()->start(),
          graph()->NewNode(common()->IfFalse(),
                           graph()->NewNode(common()->Branch(), compare,
                                            graph()->start()))));

  NodeProperties::ReplaceValueInput(compare, load, 0);

  LowerGraph(load, MachineRepresentation::kWord64);
}

TEST_F(Int64LoweringTest, WasmBigIntSpecialCaseBigIntToI64) {
  Node* target = Int32Constant(1);
  Node* context = Int32Constant(2);
  Node* bigint = Int32Constant(4);

  CallDescriptor* bigint_to_i64_call_descriptor =
      Linkage::GetStubCallDescriptor(
          zone(),                   // zone
          BigIntToI64Descriptor(),  // descriptor
          BigIntToI64Descriptor()
              .GetStackParameterCount(),   // stack parameter count
          CallDescriptor::kNoFlags,        // flags
          Operator::kNoProperties,         // properties
          StubCallMode::kCallCodeObject);  // stub call mode

  CallDescriptor* bigint_to_i32_pair_call_descriptor =
      Linkage::GetStubCallDescriptor(
          zone(),                       // zone
          BigIntToI32PairDescriptor(),  // descriptor
          BigIntToI32PairDescriptor()
              .GetStackParameterCount(),   // stack parameter count
          CallDescriptor::kNoFlags,        // flags
          Operator::kNoProperties,         // properties
          StubCallMode::kCallCodeObject);  // stub call mode

  auto lowering_special_case = base::make_unique<Int64LoweringSpecialCase>();
  lowering_special_case->bigint_to_i64_call_descriptor =
      bigint_to_i64_call_descriptor;
  lowering_special_case->bigint_to_i32_pair_call_descriptor =
      bigint_to_i32_pair_call_descriptor;

  Node* call_node =
      graph()->NewNode(common()->Call(bigint_to_i64_call_descriptor), target,
                       bigint, context, start(), start());

  LowerGraphWithSpecialCase(call_node, std::move(lowering_special_case),
                            MachineRepresentation::kWord64);

  Capture<Node*> call;
  Matcher<Node*> call_matcher =
      IsCall(bigint_to_i32_pair_call_descriptor, target, bigint, context,
             start(), start());

  EXPECT_THAT(graph()->end()->InputAt(1),
              IsReturn2(IsProjection(0, AllOf(CaptureEq(&call), call_matcher)),
                        IsProjection(1, AllOf(CaptureEq(&call), call_matcher)),
                        start(), start()));
}

TEST_F(Int64LoweringTest, WasmBigIntSpecialCaseI64ToBigInt) {
  Node* target = Int32Constant(1);
  Node* i64 = Int64Constant(value(0));

  CallDescriptor* i64_to_bigint_call_descriptor =
      Linkage::GetStubCallDescriptor(
          zone(),                   // zone
          I64ToBigIntDescriptor(),  // descriptor
          I64ToBigIntDescriptor()
              .GetStackParameterCount(),   // stack parameter count
          CallDescriptor::kNoFlags,        // flags
          Operator::kNoProperties,         // properties
          StubCallMode::kCallCodeObject);  // stub call mode

  CallDescriptor* i32_pair_to_bigint_call_descriptor =
      Linkage::GetStubCallDescriptor(
          zone(),                       // zone
          I32PairToBigIntDescriptor(),  // descriptor
          I32PairToBigIntDescriptor()
              .GetStackParameterCount(),   // stack parameter count
          CallDescriptor::kNoFlags,        // flags
          Operator::kNoProperties,         // properties
          StubCallMode::kCallCodeObject);  // stub call mode

  auto lowering_special_case = base::make_unique<Int64LoweringSpecialCase>();
  lowering_special_case->i64_to_bigint_call_descriptor =
      i64_to_bigint_call_descriptor;
  lowering_special_case->i32_pair_to_bigint_call_descriptor =
      i32_pair_to_bigint_call_descriptor;

  Node* call = graph()->NewNode(common()->Call(i64_to_bigint_call_descriptor),
                                target, i64, start(), start());

  LowerGraphWithSpecialCase(call, std::move(lowering_special_case),
                            MachineRepresentation::kTaggedPointer);

  EXPECT_THAT(
      graph()->end()->InputAt(1),
      IsReturn(IsCall(i32_pair_to_bigint_call_descriptor, target,
                      IsInt32Constant(low_word_value(0)),
                      IsInt32Constant(high_word_value(0)), start(), start()),
               start(), start()));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
