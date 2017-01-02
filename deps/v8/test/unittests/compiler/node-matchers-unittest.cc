// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/opcodes.h"

#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

class NodeMatcherTest : public GraphTest {
 public:
  NodeMatcherTest() : machine_(zone()) {}
  ~NodeMatcherTest() override {}

  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
};

namespace {

template <class Matcher>
void CheckBaseWithIndexAndDisplacement(
    Matcher* matcher, Node* index, int scale, Node* base, Node* displacement,
    DisplacementMode displacement_mode = kPositiveDisplacement) {
  EXPECT_TRUE(matcher->matches());
  EXPECT_EQ(index, matcher->index());
  EXPECT_EQ(scale, matcher->scale());
  EXPECT_EQ(base, matcher->base());
  EXPECT_EQ(displacement, matcher->displacement());
  EXPECT_EQ(displacement_mode, matcher->displacement_mode());
}

}  // namespace


TEST_F(NodeMatcherTest, ScaledWithOffset32Matcher) {
  graph()->SetStart(graph()->NewNode(common()->Start(0)));

  const Operator* d0_op = common()->Int32Constant(0);
  Node* d0 = graph()->NewNode(d0_op);
  USE(d0);
  const Operator* d1_op = common()->Int32Constant(1);
  Node* d1 = graph()->NewNode(d1_op);
  USE(d1);
  const Operator* d2_op = common()->Int32Constant(2);
  Node* d2 = graph()->NewNode(d2_op);
  USE(d2);
  const Operator* d3_op = common()->Int32Constant(3);
  Node* d3 = graph()->NewNode(d3_op);
  USE(d3);
  const Operator* d4_op = common()->Int32Constant(4);
  Node* d4 = graph()->NewNode(d4_op);
  USE(d4);
  const Operator* d5_op = common()->Int32Constant(5);
  Node* d5 = graph()->NewNode(d5_op);
  USE(d5);
  const Operator* d7_op = common()->Int32Constant(7);
  Node* d7 = graph()->NewNode(d7_op);
  USE(d4);
  const Operator* d8_op = common()->Int32Constant(8);
  Node* d8 = graph()->NewNode(d8_op);
  USE(d8);
  const Operator* d9_op = common()->Int32Constant(9);
  Node* d9 = graph()->NewNode(d9_op);
  USE(d9);
  const Operator* d15_op = common()->Int32Constant(15);
  Node* d15 = graph()->NewNode(d15_op);
  USE(d15);

  const Operator* b0_op = common()->Parameter(0);
  Node* b0 = graph()->NewNode(b0_op, graph()->start());
  USE(b0);
  const Operator* b1_op = common()->Parameter(1);
  Node* b1 = graph()->NewNode(b1_op, graph()->start());
  USE(b0);

  const Operator* p1_op = common()->Parameter(3);
  Node* p1 = graph()->NewNode(p1_op, graph()->start());
  USE(p1);

  const Operator* a_op = machine()->Int32Add();
  USE(a_op);

  const Operator* sub_op = machine()->Int32Sub();
  USE(sub_op);

  const Operator* m_op = machine()->Int32Mul();
  Node* m1 = graph()->NewNode(m_op, p1, d1);
  Node* m2 = graph()->NewNode(m_op, p1, d2);
  Node* m3 = graph()->NewNode(m_op, p1, d3);
  Node* m4 = graph()->NewNode(m_op, p1, d4);
  Node* m5 = graph()->NewNode(m_op, p1, d5);
  Node* m7 = graph()->NewNode(m_op, p1, d7);
  Node* m8 = graph()->NewNode(m_op, p1, d8);
  Node* m9 = graph()->NewNode(m_op, p1, d9);
  USE(m1);
  USE(m2);
  USE(m3);
  USE(m4);
  USE(m5);
  USE(m7);
  USE(m8);
  USE(m9);

  const Operator* s_op = machine()->Word32Shl();
  Node* s0 = graph()->NewNode(s_op, p1, d0);
  Node* s1 = graph()->NewNode(s_op, p1, d1);
  Node* s2 = graph()->NewNode(s_op, p1, d2);
  Node* s3 = graph()->NewNode(s_op, p1, d3);
  Node* s4 = graph()->NewNode(s_op, p1, d4);
  USE(s0);
  USE(s1);
  USE(s2);
  USE(s3);
  USE(s4);

  // 1 INPUT

  // Only relevant test dases is Checking for non-match.
  BaseWithIndexAndDisplacement32Matcher match0(d15);
  EXPECT_FALSE(match0.matches());

  // 2 INPUT

  // (B0 + B1) -> [B0, 0, B1, NULL]
  BaseWithIndexAndDisplacement32Matcher match1(graph()->NewNode(a_op, b0, b1));
  CheckBaseWithIndexAndDisplacement(&match1, b1, 0, b0, NULL);

  // (B0 + D15) -> [NULL, 0, B0, D15]
  BaseWithIndexAndDisplacement32Matcher match2(graph()->NewNode(a_op, b0, d15));
  CheckBaseWithIndexAndDisplacement(&match2, NULL, 0, b0, d15);

  // (D15 + B0) -> [NULL, 0, B0, D15]
  BaseWithIndexAndDisplacement32Matcher match3(graph()->NewNode(a_op, d15, b0));
  CheckBaseWithIndexAndDisplacement(&match3, NULL, 0, b0, d15);

  // (B0 + M1) -> [p1, 0, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match4(graph()->NewNode(a_op, b0, m1));
  CheckBaseWithIndexAndDisplacement(&match4, p1, 0, b0, NULL);

  // (M1 + B0) -> [p1, 0, B0, NULL]
  m1 = graph()->NewNode(m_op, p1, d1);
  BaseWithIndexAndDisplacement32Matcher match5(graph()->NewNode(a_op, m1, b0));
  CheckBaseWithIndexAndDisplacement(&match5, p1, 0, b0, NULL);

  // (D15 + M1) -> [P1, 0, NULL, D15]
  m1 = graph()->NewNode(m_op, p1, d1);
  BaseWithIndexAndDisplacement32Matcher match6(graph()->NewNode(a_op, d15, m1));
  CheckBaseWithIndexAndDisplacement(&match6, p1, 0, NULL, d15);

  // (M1 + D15) -> [P1, 0, NULL, D15]
  m1 = graph()->NewNode(m_op, p1, d1);
  BaseWithIndexAndDisplacement32Matcher match7(graph()->NewNode(a_op, m1, d15));
  CheckBaseWithIndexAndDisplacement(&match7, p1, 0, NULL, d15);

  // (B0 + S0) -> [p1, 0, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match8(graph()->NewNode(a_op, b0, s0));
  CheckBaseWithIndexAndDisplacement(&match8, p1, 0, b0, NULL);

  // (S0 + B0) -> [p1, 0, B0, NULL]
  s0 = graph()->NewNode(s_op, p1, d0);
  BaseWithIndexAndDisplacement32Matcher match9(graph()->NewNode(a_op, s0, b0));
  CheckBaseWithIndexAndDisplacement(&match9, p1, 0, b0, NULL);

  // (D15 + S0) -> [P1, 0, NULL, D15]
  s0 = graph()->NewNode(s_op, p1, d0);
  BaseWithIndexAndDisplacement32Matcher match10(
      graph()->NewNode(a_op, d15, s0));
  CheckBaseWithIndexAndDisplacement(&match10, p1, 0, NULL, d15);

  // (S0 + D15) -> [P1, 0, NULL, D15]
  s0 = graph()->NewNode(s_op, p1, d0);
  BaseWithIndexAndDisplacement32Matcher match11(
      graph()->NewNode(a_op, s0, d15));
  CheckBaseWithIndexAndDisplacement(&match11, p1, 0, NULL, d15);

  // (B0 + M2) -> [p1, 1, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match12(graph()->NewNode(a_op, b0, m2));
  CheckBaseWithIndexAndDisplacement(&match12, p1, 1, b0, NULL);

  // (M2 + B0) -> [p1, 1, B0, NULL]
  m2 = graph()->NewNode(m_op, p1, d2);
  BaseWithIndexAndDisplacement32Matcher match13(graph()->NewNode(a_op, m2, b0));
  CheckBaseWithIndexAndDisplacement(&match13, p1, 1, b0, NULL);

  // (D15 + M2) -> [P1, 1, NULL, D15]
  m2 = graph()->NewNode(m_op, p1, d2);
  BaseWithIndexAndDisplacement32Matcher match14(
      graph()->NewNode(a_op, d15, m2));
  CheckBaseWithIndexAndDisplacement(&match14, p1, 1, NULL, d15);

  // (M2 + D15) -> [P1, 1, NULL, D15]
  m2 = graph()->NewNode(m_op, p1, d2);
  BaseWithIndexAndDisplacement32Matcher match15(
      graph()->NewNode(a_op, m2, d15));
  CheckBaseWithIndexAndDisplacement(&match15, p1, 1, NULL, d15);

  // (B0 + S1) -> [p1, 1, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match16(graph()->NewNode(a_op, b0, s1));
  CheckBaseWithIndexAndDisplacement(&match16, p1, 1, b0, NULL);

  // (S1 + B0) -> [p1, 1, B0, NULL]
  s1 = graph()->NewNode(s_op, p1, d1);
  BaseWithIndexAndDisplacement32Matcher match17(graph()->NewNode(a_op, s1, b0));
  CheckBaseWithIndexAndDisplacement(&match17, p1, 1, b0, NULL);

  // (D15 + S1) -> [P1, 1, NULL, D15]
  s1 = graph()->NewNode(s_op, p1, d1);
  BaseWithIndexAndDisplacement32Matcher match18(
      graph()->NewNode(a_op, d15, s1));
  CheckBaseWithIndexAndDisplacement(&match18, p1, 1, NULL, d15);

  // (S1 + D15) -> [P1, 1, NULL, D15]
  s1 = graph()->NewNode(s_op, p1, d1);
  BaseWithIndexAndDisplacement32Matcher match19(
      graph()->NewNode(a_op, s1, d15));
  CheckBaseWithIndexAndDisplacement(&match19, p1, 1, NULL, d15);

  // (B0 + M4) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match20(graph()->NewNode(a_op, b0, m4));
  CheckBaseWithIndexAndDisplacement(&match20, p1, 2, b0, NULL);

  // (M4 + B0) -> [p1, 2, B0, NULL]
  m4 = graph()->NewNode(m_op, p1, d4);
  BaseWithIndexAndDisplacement32Matcher match21(graph()->NewNode(a_op, m4, b0));
  CheckBaseWithIndexAndDisplacement(&match21, p1, 2, b0, NULL);

  // (D15 + M4) -> [p1, 2, NULL, D15]
  m4 = graph()->NewNode(m_op, p1, d4);
  BaseWithIndexAndDisplacement32Matcher match22(
      graph()->NewNode(a_op, d15, m4));
  CheckBaseWithIndexAndDisplacement(&match22, p1, 2, NULL, d15);

  // (M4 + D15) -> [p1, 2, NULL, D15]
  m4 = graph()->NewNode(m_op, p1, d4);
  BaseWithIndexAndDisplacement32Matcher match23(
      graph()->NewNode(a_op, m4, d15));
  CheckBaseWithIndexAndDisplacement(&match23, p1, 2, NULL, d15);

  // (B0 + S2) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match24(graph()->NewNode(a_op, b0, s2));
  CheckBaseWithIndexAndDisplacement(&match24, p1, 2, b0, NULL);

  // (S2 + B0) -> [p1, 2, B0, NULL]
  s2 = graph()->NewNode(s_op, p1, d2);
  BaseWithIndexAndDisplacement32Matcher match25(graph()->NewNode(a_op, s2, b0));
  CheckBaseWithIndexAndDisplacement(&match25, p1, 2, b0, NULL);

  // (D15 + S2) -> [p1, 2, NULL, D15]
  s2 = graph()->NewNode(s_op, p1, d2);
  BaseWithIndexAndDisplacement32Matcher match26(
      graph()->NewNode(a_op, d15, s2));
  CheckBaseWithIndexAndDisplacement(&match26, p1, 2, NULL, d15);

  // (S2 + D15) -> [p1, 2, NULL, D15]
  s2 = graph()->NewNode(s_op, p1, d2);
  BaseWithIndexAndDisplacement32Matcher match27(
      graph()->NewNode(a_op, s2, d15));
  CheckBaseWithIndexAndDisplacement(&match27, p1, 2, NULL, d15);

  // (B0 + M8) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match28(graph()->NewNode(a_op, b0, m8));
  CheckBaseWithIndexAndDisplacement(&match28, p1, 3, b0, NULL);

  // (M8 + B0) -> [p1, 2, B0, NULL]
  m8 = graph()->NewNode(m_op, p1, d8);
  BaseWithIndexAndDisplacement32Matcher match29(graph()->NewNode(a_op, m8, b0));
  CheckBaseWithIndexAndDisplacement(&match29, p1, 3, b0, NULL);

  // (D15 + M8) -> [p1, 2, NULL, D15]
  m8 = graph()->NewNode(m_op, p1, d8);
  BaseWithIndexAndDisplacement32Matcher match30(
      graph()->NewNode(a_op, d15, m8));
  CheckBaseWithIndexAndDisplacement(&match30, p1, 3, NULL, d15);

  // (M8 + D15) -> [p1, 2, NULL, D15]
  m8 = graph()->NewNode(m_op, p1, d8);
  BaseWithIndexAndDisplacement32Matcher match31(
      graph()->NewNode(a_op, m8, d15));
  CheckBaseWithIndexAndDisplacement(&match31, p1, 3, NULL, d15);

  // (B0 + S3) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement32Matcher match32(graph()->NewNode(a_op, b0, s3));
  CheckBaseWithIndexAndDisplacement(&match32, p1, 3, b0, NULL);

  // (S3 + B0) -> [p1, 2, B0, NULL]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match33(graph()->NewNode(a_op, s3, b0));
  CheckBaseWithIndexAndDisplacement(&match33, p1, 3, b0, NULL);

  // (D15 + S3) -> [p1, 2, NULL, D15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match34(
      graph()->NewNode(a_op, d15, s3));
  CheckBaseWithIndexAndDisplacement(&match34, p1, 3, NULL, d15);

  // (S3 + D15) -> [p1, 2, NULL, D15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match35(
      graph()->NewNode(a_op, s3, d15));
  CheckBaseWithIndexAndDisplacement(&match35, p1, 3, NULL, d15);

  // 2 INPUT - NEGATIVE CASES

  // (M3 + B1) -> [B0, 0, M3, NULL]
  BaseWithIndexAndDisplacement32Matcher match36(graph()->NewNode(a_op, b1, m3));
  CheckBaseWithIndexAndDisplacement(&match36, m3, 0, b1, NULL);

  // (S4 + B1) -> [B0, 0, S4, NULL]
  BaseWithIndexAndDisplacement32Matcher match37(graph()->NewNode(a_op, b1, s4));
  CheckBaseWithIndexAndDisplacement(&match37, s4, 0, b1, NULL);

  // 3 INPUT

  // (D15 + S3) + B0 -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match38(
      graph()->NewNode(a_op, graph()->NewNode(a_op, d15, s3), b0));
  CheckBaseWithIndexAndDisplacement(&match38, p1, 3, b0, d15);

  // (B0 + D15) + S3 -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match39(
      graph()->NewNode(a_op, graph()->NewNode(a_op, b0, d15), s3));
  CheckBaseWithIndexAndDisplacement(&match39, p1, 3, b0, d15);

  // (S3 + B0) + D15 -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match40(
      graph()->NewNode(a_op, graph()->NewNode(a_op, s3, b0), d15));
  CheckBaseWithIndexAndDisplacement(&match40, p1, 3, b0, d15);

  // D15 + (S3 + B0) -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match41(
      graph()->NewNode(a_op, d15, graph()->NewNode(a_op, s3, b0)));
  CheckBaseWithIndexAndDisplacement(&match41, p1, 3, b0, d15);

  // B0 + (D15 + S3) -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match42(
      graph()->NewNode(a_op, b0, graph()->NewNode(a_op, d15, s3)));
  CheckBaseWithIndexAndDisplacement(&match42, p1, 3, b0, d15);

  // S3 + (B0 + D15) -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match43(
      graph()->NewNode(a_op, s3, graph()->NewNode(a_op, b0, d15)));
  CheckBaseWithIndexAndDisplacement(&match43, p1, 3, b0, d15);

  // S3 + (B0 - D15) -> [p1, 2, b0, d15, true]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match44(
      graph()->NewNode(a_op, s3, graph()->NewNode(sub_op, b0, d15)));
  CheckBaseWithIndexAndDisplacement(&match44, p1, 3, b0, d15,
                                    kNegativeDisplacement);

  // B0 + (B1 - D15) -> [p1, 2, b0, d15, true]
  BaseWithIndexAndDisplacement32Matcher match45(
      graph()->NewNode(a_op, b0, graph()->NewNode(sub_op, b1, d15)));
  CheckBaseWithIndexAndDisplacement(&match45, b1, 0, b0, d15,
                                    kNegativeDisplacement);

  // (B0 - D15) + S3 -> [p1, 2, b0, d15, true]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement32Matcher match46(
      graph()->NewNode(a_op, graph()->NewNode(sub_op, b0, d15), s3));
  CheckBaseWithIndexAndDisplacement(&match46, p1, 3, b0, d15,
                                    kNegativeDisplacement);
}


TEST_F(NodeMatcherTest, ScaledWithOffset64Matcher) {
  graph()->SetStart(graph()->NewNode(common()->Start(0)));

  const Operator* d0_op = common()->Int64Constant(0);
  Node* d0 = graph()->NewNode(d0_op);
  USE(d0);
  const Operator* d1_op = common()->Int64Constant(1);
  Node* d1 = graph()->NewNode(d1_op);
  USE(d1);
  const Operator* d2_op = common()->Int64Constant(2);
  Node* d2 = graph()->NewNode(d2_op);
  USE(d2);
  const Operator* d3_op = common()->Int64Constant(3);
  Node* d3 = graph()->NewNode(d3_op);
  USE(d3);
  const Operator* d4_op = common()->Int64Constant(4);
  Node* d4 = graph()->NewNode(d4_op);
  USE(d4);
  const Operator* d5_op = common()->Int64Constant(5);
  Node* d5 = graph()->NewNode(d5_op);
  USE(d5);
  const Operator* d7_op = common()->Int64Constant(7);
  Node* d7 = graph()->NewNode(d7_op);
  USE(d7);
  const Operator* d8_op = common()->Int64Constant(8);
  Node* d8 = graph()->NewNode(d8_op);
  USE(d8);
  const Operator* d9_op = common()->Int64Constant(9);
  Node* d9 = graph()->NewNode(d9_op);
  USE(d8);
  const Operator* d15_op = common()->Int64Constant(15);
  Node* d15 = graph()->NewNode(d15_op);
  USE(d15);
  const Operator* d15_32_op = common()->Int32Constant(15);
  Node* d15_32 = graph()->NewNode(d15_32_op);
  USE(d15_32);

  const Operator* b0_op = common()->Parameter(0);
  Node* b0 = graph()->NewNode(b0_op, graph()->start());
  USE(b0);
  const Operator* b1_op = common()->Parameter(1);
  Node* b1 = graph()->NewNode(b1_op, graph()->start());
  USE(b0);

  const Operator* p1_op = common()->Parameter(3);
  Node* p1 = graph()->NewNode(p1_op, graph()->start());
  USE(p1);

  const Operator* a_op = machine()->Int64Add();
  USE(a_op);

  const Operator* sub_op = machine()->Int64Sub();
  USE(sub_op);

  const Operator* m_op = machine()->Int64Mul();
  Node* m1 = graph()->NewNode(m_op, p1, d1);
  Node* m2 = graph()->NewNode(m_op, p1, d2);
  Node* m3 = graph()->NewNode(m_op, p1, d3);
  Node* m4 = graph()->NewNode(m_op, p1, d4);
  Node* m5 = graph()->NewNode(m_op, p1, d5);
  Node* m7 = graph()->NewNode(m_op, p1, d7);
  Node* m8 = graph()->NewNode(m_op, p1, d8);
  Node* m9 = graph()->NewNode(m_op, p1, d9);
  USE(m1);
  USE(m2);
  USE(m3);
  USE(m4);
  USE(m5);
  USE(m7);
  USE(m8);
  USE(m9);

  const Operator* s_op = machine()->Word64Shl();
  Node* s0 = graph()->NewNode(s_op, p1, d0);
  Node* s1 = graph()->NewNode(s_op, p1, d1);
  Node* s2 = graph()->NewNode(s_op, p1, d2);
  Node* s3 = graph()->NewNode(s_op, p1, d3);
  Node* s4 = graph()->NewNode(s_op, p1, d4);
  USE(s0);
  USE(s1);
  USE(s2);
  USE(s3);
  USE(s4);

  // 1 INPUT

  // Only relevant test dases is Checking for non-match.
  BaseWithIndexAndDisplacement64Matcher match0(d15);
  EXPECT_FALSE(match0.matches());

  // 2 INPUT

  // (B0 + B1) -> [B0, 0, B1, NULL]
  BaseWithIndexAndDisplacement64Matcher match1(graph()->NewNode(a_op, b0, b1));
  CheckBaseWithIndexAndDisplacement(&match1, b1, 0, b0, NULL);

  // (B0 + D15) -> [NULL, 0, B0, D15]
  BaseWithIndexAndDisplacement64Matcher match2(graph()->NewNode(a_op, b0, d15));
  CheckBaseWithIndexAndDisplacement(&match2, NULL, 0, b0, d15);

  BaseWithIndexAndDisplacement64Matcher match2_32(
      graph()->NewNode(a_op, b0, d15_32));
  CheckBaseWithIndexAndDisplacement(&match2_32, NULL, 0, b0, d15_32);

  // (D15 + B0) -> [NULL, 0, B0, D15]
  BaseWithIndexAndDisplacement64Matcher match3(graph()->NewNode(a_op, d15, b0));
  CheckBaseWithIndexAndDisplacement(&match3, NULL, 0, b0, d15);

  // (B0 + M1) -> [p1, 0, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match4(graph()->NewNode(a_op, b0, m1));
  CheckBaseWithIndexAndDisplacement(&match4, p1, 0, b0, NULL);

  // (M1 + B0) -> [p1, 0, B0, NULL]
  m1 = graph()->NewNode(m_op, p1, d1);
  BaseWithIndexAndDisplacement64Matcher match5(graph()->NewNode(a_op, m1, b0));
  CheckBaseWithIndexAndDisplacement(&match5, p1, 0, b0, NULL);

  // (D15 + M1) -> [P1, 0, NULL, D15]
  m1 = graph()->NewNode(m_op, p1, d1);
  BaseWithIndexAndDisplacement64Matcher match6(graph()->NewNode(a_op, d15, m1));
  CheckBaseWithIndexAndDisplacement(&match6, p1, 0, NULL, d15);

  // (M1 + D15) -> [P1, 0, NULL, D15]
  m1 = graph()->NewNode(m_op, p1, d1);
  BaseWithIndexAndDisplacement64Matcher match7(graph()->NewNode(a_op, m1, d15));
  CheckBaseWithIndexAndDisplacement(&match7, p1, 0, NULL, d15);

  // (B0 + S0) -> [p1, 0, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match8(graph()->NewNode(a_op, b0, s0));
  CheckBaseWithIndexAndDisplacement(&match8, p1, 0, b0, NULL);

  // (S0 + B0) -> [p1, 0, B0, NULL]
  s0 = graph()->NewNode(s_op, p1, d0);
  BaseWithIndexAndDisplacement64Matcher match9(graph()->NewNode(a_op, s0, b0));
  CheckBaseWithIndexAndDisplacement(&match9, p1, 0, b0, NULL);

  // (D15 + S0) -> [P1, 0, NULL, D15]
  s0 = graph()->NewNode(s_op, p1, d0);
  BaseWithIndexAndDisplacement64Matcher match10(
      graph()->NewNode(a_op, d15, s0));
  CheckBaseWithIndexAndDisplacement(&match10, p1, 0, NULL, d15);

  // (S0 + D15) -> [P1, 0, NULL, D15]
  s0 = graph()->NewNode(s_op, p1, d0);
  BaseWithIndexAndDisplacement64Matcher match11(
      graph()->NewNode(a_op, s0, d15));
  CheckBaseWithIndexAndDisplacement(&match11, p1, 0, NULL, d15);

  // (B0 + M2) -> [p1, 1, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match12(graph()->NewNode(a_op, b0, m2));
  CheckBaseWithIndexAndDisplacement(&match12, p1, 1, b0, NULL);

  // (M2 + B0) -> [p1, 1, B0, NULL]
  m2 = graph()->NewNode(m_op, p1, d2);
  BaseWithIndexAndDisplacement64Matcher match13(graph()->NewNode(a_op, m2, b0));
  CheckBaseWithIndexAndDisplacement(&match13, p1, 1, b0, NULL);

  // (D15 + M2) -> [P1, 1, NULL, D15]
  m2 = graph()->NewNode(m_op, p1, d2);
  BaseWithIndexAndDisplacement64Matcher match14(
      graph()->NewNode(a_op, d15, m2));
  CheckBaseWithIndexAndDisplacement(&match14, p1, 1, NULL, d15);

  // (M2 + D15) -> [P1, 1, NULL, D15]
  m2 = graph()->NewNode(m_op, p1, d2);
  BaseWithIndexAndDisplacement64Matcher match15(
      graph()->NewNode(a_op, m2, d15));
  CheckBaseWithIndexAndDisplacement(&match15, p1, 1, NULL, d15);

  // (B0 + S1) -> [p1, 1, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match16(graph()->NewNode(a_op, b0, s1));
  CheckBaseWithIndexAndDisplacement(&match16, p1, 1, b0, NULL);

  // (S1 + B0) -> [p1, 1, B0, NULL]
  s1 = graph()->NewNode(s_op, p1, d1);
  BaseWithIndexAndDisplacement64Matcher match17(graph()->NewNode(a_op, s1, b0));
  CheckBaseWithIndexAndDisplacement(&match17, p1, 1, b0, NULL);

  // (D15 + S1) -> [P1, 1, NULL, D15]
  s1 = graph()->NewNode(s_op, p1, d1);
  BaseWithIndexAndDisplacement64Matcher match18(
      graph()->NewNode(a_op, d15, s1));
  CheckBaseWithIndexAndDisplacement(&match18, p1, 1, NULL, d15);

  // (S1 + D15) -> [P1, 1, NULL, D15]
  s1 = graph()->NewNode(s_op, p1, d1);
  BaseWithIndexAndDisplacement64Matcher match19(
      graph()->NewNode(a_op, s1, d15));
  CheckBaseWithIndexAndDisplacement(&match19, p1, 1, NULL, d15);

  // (B0 + M4) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match20(graph()->NewNode(a_op, b0, m4));
  CheckBaseWithIndexAndDisplacement(&match20, p1, 2, b0, NULL);

  // (M4 + B0) -> [p1, 2, B0, NULL]
  m4 = graph()->NewNode(m_op, p1, d4);
  BaseWithIndexAndDisplacement64Matcher match21(graph()->NewNode(a_op, m4, b0));
  CheckBaseWithIndexAndDisplacement(&match21, p1, 2, b0, NULL);

  // (D15 + M4) -> [p1, 2, NULL, D15]
  m4 = graph()->NewNode(m_op, p1, d4);
  BaseWithIndexAndDisplacement64Matcher match22(
      graph()->NewNode(a_op, d15, m4));
  CheckBaseWithIndexAndDisplacement(&match22, p1, 2, NULL, d15);

  // (M4 + D15) -> [p1, 2, NULL, D15]
  m4 = graph()->NewNode(m_op, p1, d4);
  BaseWithIndexAndDisplacement64Matcher match23(
      graph()->NewNode(a_op, m4, d15));
  CheckBaseWithIndexAndDisplacement(&match23, p1, 2, NULL, d15);

  // (B0 + S2) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match24(graph()->NewNode(a_op, b0, s2));
  CheckBaseWithIndexAndDisplacement(&match24, p1, 2, b0, NULL);

  // (S2 + B0) -> [p1, 2, B0, NULL]
  s2 = graph()->NewNode(s_op, p1, d2);
  BaseWithIndexAndDisplacement64Matcher match25(graph()->NewNode(a_op, s2, b0));
  CheckBaseWithIndexAndDisplacement(&match25, p1, 2, b0, NULL);

  // (D15 + S2) -> [p1, 2, NULL, D15]
  s2 = graph()->NewNode(s_op, p1, d2);
  BaseWithIndexAndDisplacement64Matcher match26(
      graph()->NewNode(a_op, d15, s2));
  CheckBaseWithIndexAndDisplacement(&match26, p1, 2, NULL, d15);

  // (S2 + D15) -> [p1, 2, NULL, D15]
  s2 = graph()->NewNode(s_op, p1, d2);
  BaseWithIndexAndDisplacement64Matcher match27(
      graph()->NewNode(a_op, s2, d15));
  CheckBaseWithIndexAndDisplacement(&match27, p1, 2, NULL, d15);

  // (B0 + M8) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match28(graph()->NewNode(a_op, b0, m8));
  CheckBaseWithIndexAndDisplacement(&match28, p1, 3, b0, NULL);

  // (M8 + B0) -> [p1, 2, B0, NULL]
  m8 = graph()->NewNode(m_op, p1, d8);
  BaseWithIndexAndDisplacement64Matcher match29(graph()->NewNode(a_op, m8, b0));
  CheckBaseWithIndexAndDisplacement(&match29, p1, 3, b0, NULL);

  // (D15 + M8) -> [p1, 2, NULL, D15]
  m8 = graph()->NewNode(m_op, p1, d8);
  BaseWithIndexAndDisplacement64Matcher match30(
      graph()->NewNode(a_op, d15, m8));
  CheckBaseWithIndexAndDisplacement(&match30, p1, 3, NULL, d15);

  // (M8 + D15) -> [p1, 2, NULL, D15]
  m8 = graph()->NewNode(m_op, p1, d8);
  BaseWithIndexAndDisplacement64Matcher match31(
      graph()->NewNode(a_op, m8, d15));
  CheckBaseWithIndexAndDisplacement(&match31, p1, 3, NULL, d15);

  // (B0 + S3) -> [p1, 2, B0, NULL]
  BaseWithIndexAndDisplacement64Matcher match64(graph()->NewNode(a_op, b0, s3));
  CheckBaseWithIndexAndDisplacement(&match64, p1, 3, b0, NULL);

  // (S3 + B0) -> [p1, 2, B0, NULL]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match33(graph()->NewNode(a_op, s3, b0));
  CheckBaseWithIndexAndDisplacement(&match33, p1, 3, b0, NULL);

  // (D15 + S3) -> [p1, 2, NULL, D15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match34(
      graph()->NewNode(a_op, d15, s3));
  CheckBaseWithIndexAndDisplacement(&match34, p1, 3, NULL, d15);

  // (S3 + D15) -> [p1, 2, NULL, D15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match35(
      graph()->NewNode(a_op, s3, d15));
  CheckBaseWithIndexAndDisplacement(&match35, p1, 3, NULL, d15);

  // 2 INPUT - NEGATIVE CASES

  // (M3 + B1) -> [B0, 0, M3, NULL]
  BaseWithIndexAndDisplacement64Matcher match36(graph()->NewNode(a_op, b1, m3));
  CheckBaseWithIndexAndDisplacement(&match36, m3, 0, b1, NULL);

  // (S4 + B1) -> [B0, 0, S4, NULL]
  BaseWithIndexAndDisplacement64Matcher match37(graph()->NewNode(a_op, b1, s4));
  CheckBaseWithIndexAndDisplacement(&match37, s4, 0, b1, NULL);

  // 3 INPUT

  // (D15 + S3) + B0 -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match38(
      graph()->NewNode(a_op, graph()->NewNode(a_op, d15, s3), b0));
  CheckBaseWithIndexAndDisplacement(&match38, p1, 3, b0, d15);

  // (B0 + D15) + S3 -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match39(
      graph()->NewNode(a_op, graph()->NewNode(a_op, b0, d15), s3));
  CheckBaseWithIndexAndDisplacement(&match39, p1, 3, b0, d15);

  // (S3 + B0) + D15 -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match40(
      graph()->NewNode(a_op, graph()->NewNode(a_op, s3, b0), d15));
  CheckBaseWithIndexAndDisplacement(&match40, p1, 3, b0, d15);

  // D15 + (S3 + B0) -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match41(
      graph()->NewNode(a_op, d15, graph()->NewNode(a_op, s3, b0)));
  CheckBaseWithIndexAndDisplacement(&match41, p1, 3, b0, d15);

  // B0 + (D15 + S3) -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match42(
      graph()->NewNode(a_op, b0, graph()->NewNode(a_op, d15, s3)));
  CheckBaseWithIndexAndDisplacement(&match42, p1, 3, b0, d15);

  // S3 + (B0 + D15) -> [p1, 2, b0, d15]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match43(
      graph()->NewNode(a_op, s3, graph()->NewNode(a_op, b0, d15)));
  CheckBaseWithIndexAndDisplacement(&match43, p1, 3, b0, d15);

  // 2 INPUT with non-power of 2 scale

  // (M3 + D15) -> [p1, 1, p1, D15]
  m3 = graph()->NewNode(m_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match44(
      graph()->NewNode(a_op, m3, d15));
  CheckBaseWithIndexAndDisplacement(&match44, p1, 1, p1, d15);

  // (M5 + D15) -> [p1, 2, p1, D15]
  m5 = graph()->NewNode(m_op, p1, d5);
  BaseWithIndexAndDisplacement64Matcher match45(
      graph()->NewNode(a_op, m5, d15));
  CheckBaseWithIndexAndDisplacement(&match45, p1, 2, p1, d15);

  // (M9 + D15) -> [p1, 3, p1, D15]
  m9 = graph()->NewNode(m_op, p1, d9);
  BaseWithIndexAndDisplacement64Matcher match46(
      graph()->NewNode(a_op, m9, d15));
  CheckBaseWithIndexAndDisplacement(&match46, p1, 3, p1, d15);

  // 3 INPUT negative cases: non-power of 2 scale but with a base

  // ((M3 + B0) + D15) -> [m3, 0, b0, D15]
  m3 = graph()->NewNode(m_op, p1, d3);
  Node* temp = graph()->NewNode(a_op, m3, b0);
  BaseWithIndexAndDisplacement64Matcher match47(
      graph()->NewNode(a_op, temp, d15));
  CheckBaseWithIndexAndDisplacement(&match47, m3, 0, b0, d15);

  // (M3 + (B0 + D15)) -> [m3, 0, b0, D15]
  m3 = graph()->NewNode(m_op, p1, d3);
  temp = graph()->NewNode(a_op, d15, b0);
  BaseWithIndexAndDisplacement64Matcher match48(
      graph()->NewNode(a_op, m3, temp));
  CheckBaseWithIndexAndDisplacement(&match48, m3, 0, b0, d15);

  // ((B0 + M3) + D15) -> [m3, 0, b0, D15]
  m3 = graph()->NewNode(m_op, p1, d3);
  temp = graph()->NewNode(a_op, b0, m3);
  BaseWithIndexAndDisplacement64Matcher match49(
      graph()->NewNode(a_op, temp, d15));
  CheckBaseWithIndexAndDisplacement(&match49, m3, 0, b0, d15);

  // (M3 + (D15 + B0)) -> [m3, 0, b0, D15]
  m3 = graph()->NewNode(m_op, p1, d3);
  temp = graph()->NewNode(a_op, b0, d15);
  BaseWithIndexAndDisplacement64Matcher match50(
      graph()->NewNode(a_op, m3, temp));
  CheckBaseWithIndexAndDisplacement(&match50, m3, 0, b0, d15);

  // S3 + (B0 - D15) -> [p1, 2, b0, d15, true]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match51(
      graph()->NewNode(a_op, s3, graph()->NewNode(sub_op, b0, d15)));
  CheckBaseWithIndexAndDisplacement(&match51, p1, 3, b0, d15,
                                    kNegativeDisplacement);

  // B0 + (B1 - D15) -> [p1, 2, b0, d15, true]
  BaseWithIndexAndDisplacement64Matcher match52(
      graph()->NewNode(a_op, b0, graph()->NewNode(sub_op, b1, d15)));
  CheckBaseWithIndexAndDisplacement(&match52, b1, 0, b0, d15,
                                    kNegativeDisplacement);

  // (B0 - D15) + S3 -> [p1, 2, b0, d15, true]
  s3 = graph()->NewNode(s_op, p1, d3);
  BaseWithIndexAndDisplacement64Matcher match53(
      graph()->NewNode(a_op, graph()->NewNode(sub_op, b0, d15), s3));
  CheckBaseWithIndexAndDisplacement(&match53, p1, 3, b0, d15,
                                    kNegativeDisplacement);
}

TEST_F(NodeMatcherTest, BranchMatcher_match) {
  Node* zero = graph()->NewNode(common()->Int32Constant(0));

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    BranchMatcher matcher(branch);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    BranchMatcher matcher(branch);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* other = graph()->NewNode(common()->IfValue(33), branch);
    BranchMatcher matcher(branch);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
    USE(other);
  }
}


TEST_F(NodeMatcherTest, BranchMatcher_fail) {
  Node* zero = graph()->NewNode(common()->Int32Constant(0));

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    BranchMatcher matcher(branch);
    EXPECT_FALSE(matcher.Matched());
    USE(if_true);
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    BranchMatcher matcher(branch);
    EXPECT_FALSE(matcher.Matched());
    USE(if_false);
  }

  {
    BranchMatcher matcher(zero);
    EXPECT_FALSE(matcher.Matched());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    EXPECT_TRUE(BranchMatcher(branch).Matched());
    EXPECT_FALSE(BranchMatcher(if_true).Matched());
    EXPECT_FALSE(BranchMatcher(if_false).Matched());
  }

  {
    Node* sw = graph()->NewNode(common()->Switch(5), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), sw);
    Node* if_false = graph()->NewNode(common()->IfFalse(), sw);
    EXPECT_FALSE(BranchMatcher(sw).Matched());
    EXPECT_FALSE(BranchMatcher(if_true).Matched());
    EXPECT_FALSE(BranchMatcher(if_false).Matched());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_value = graph()->NewNode(common()->IfValue(2), branch);
    BranchMatcher matcher(branch);
    EXPECT_FALSE(matcher.Matched());
    EXPECT_FALSE(BranchMatcher(if_true).Matched());
    EXPECT_FALSE(BranchMatcher(if_value).Matched());
  }
}


TEST_F(NodeMatcherTest, DiamondMatcher_match) {
  Node* zero = graph()->NewNode(common()->Int32Constant(0));

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    DiamondMatcher matcher(merge);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
    EXPECT_EQ(merge, matcher.Merge());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    DiamondMatcher matcher(merge);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
    EXPECT_EQ(merge, matcher.Merge());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_false, if_true);
    DiamondMatcher matcher(merge);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
    EXPECT_EQ(merge, matcher.Merge());
  }
}


TEST_F(NodeMatcherTest, DiamondMatcher_fail) {
  Node* zero = graph()->NewNode(common()->Int32Constant(0));

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_value = graph()->NewNode(common()->IfValue(1), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_value);
    DiamondMatcher matcher(merge);
    EXPECT_FALSE(matcher.Matched());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* if_value = graph()->NewNode(common()->IfValue(1), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_false, if_value);
    DiamondMatcher matcher(merge);
    EXPECT_FALSE(matcher.Matched());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    DiamondMatcher matcher(merge);
    EXPECT_TRUE(matcher.Matched());
    EXPECT_EQ(branch, matcher.Branch());
    EXPECT_EQ(if_true, matcher.IfTrue());
    EXPECT_EQ(if_false, matcher.IfFalse());
    EXPECT_EQ(merge, matcher.Merge());

    EXPECT_FALSE(DiamondMatcher(branch).Matched());  // Must be the merge.
    EXPECT_FALSE(DiamondMatcher(if_true).Matched());
    EXPECT_FALSE(DiamondMatcher(if_false).Matched());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* merge = graph()->NewNode(common()->Merge(3), if_true, if_false,
                                   graph()->start());
    DiamondMatcher matcher(merge);
    EXPECT_FALSE(matcher.Matched());  // Too many inputs to merge.
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->start();
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    DiamondMatcher matcher(merge);
    EXPECT_FALSE(matcher.Matched());
  }

  {
    Node* branch = graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->start();
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    DiamondMatcher matcher(merge);
    EXPECT_FALSE(matcher.Matched());
  }

  {
    Node* branch1 =
        graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* branch2 =
        graph()->NewNode(common()->Branch(), zero, graph()->start());
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch1);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch2);
    Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
    DiamondMatcher matcher(merge);
    EXPECT_FALSE(matcher.Matched());
  }
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
