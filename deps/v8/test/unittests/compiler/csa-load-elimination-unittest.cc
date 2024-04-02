// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/csa-load-elimination.h"

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {

class CsaLoadEliminationTest : public GraphTest {
 public:
  CsaLoadEliminationTest()
      : GraphTest(3),
        simplified_(zone()),
        machine_(zone()),
        jsgraph_(isolate(), graph(), common(), nullptr, simplified(),
                 machine()),
        reducer_(zone(), graph(), tick_counter(), broker()),
        csa_(reducer(), jsgraph(), zone()),
        mcr_(reducer(), jsgraph()) {
    reducer()->AddReducer(&csa_);
    reducer()->AddReducer(&mcr_);
  }

  ~CsaLoadEliminationTest() override = default;

 protected:
  JSGraph* jsgraph() { return &jsgraph_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  MachineOperatorBuilder* machine() { return &machine_; }
  GraphReducer* reducer() { return &reducer_; }
  Node* param1() {
    return graph()->NewNode(common()->Parameter(1), graph()->start());
  }
  Node* constant(int32_t value) {
    return graph()->NewNode(common()->Int32Constant(value));
  }

 private:
  SimplifiedOperatorBuilder simplified_;
  MachineOperatorBuilder machine_;
  JSGraph jsgraph_;
  GraphReducer reducer_;
  CsaLoadElimination csa_;
  MachineOperatorReducer mcr_;
};

#define SETUP_SIMPLE_TEST(store_type, load_type, value_)                     \
  Node* object = graph()->NewNode(common()->Parameter(0), graph()->start()); \
  Node* offset = graph()->NewNode(common()->Int32Constant(5));               \
  Node* value = value_;                                                      \
  Node* control = graph()->start();                                          \
                                                                             \
  ObjectAccess store_access(MachineType::store_type(), kNoWriteBarrier);     \
  ObjectAccess load_access(MachineType::load_type(), kNoWriteBarrier);       \
                                                                             \
  Node* store =                                                              \
      graph()->NewNode(simplified()->StoreToObject(store_access), object,    \
                       offset, value, graph()->start(), control);            \
                                                                             \
  Node* load = graph()->NewNode(simplified()->LoadFromObject(load_access),   \
                                object, offset, store, control);             \
                                                                             \
  Node* ret = graph()->NewNode(common()->Return(0), load, load, control);    \
                                                                             \
  graph()->end()->InsertInput(zone(), 0, ret);                               \
                                                                             \
  reducer()->ReduceGraph();

TEST_F(CsaLoadEliminationTest, Int32) {
  SETUP_SIMPLE_TEST(Int32, Int32, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kParameter);
}

TEST_F(CsaLoadEliminationTest, Int64) {
  SETUP_SIMPLE_TEST(Int64, Int64, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kParameter);
}

TEST_F(CsaLoadEliminationTest, Int64_to_Int32) {
  SETUP_SIMPLE_TEST(Int64, Int32, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kTruncateInt64ToInt32);
}

TEST_F(CsaLoadEliminationTest, Int16_to_Int16) {
  SETUP_SIMPLE_TEST(Int16, Int16, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kWord32Sar);
}

TEST_F(CsaLoadEliminationTest, Int16_to_Uint8) {
  SETUP_SIMPLE_TEST(Int16, Uint8, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kWord32And);
}

TEST_F(CsaLoadEliminationTest, Int8_to_Uint16) {
  SETUP_SIMPLE_TEST(Int8, Uint16, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kLoadFromObject);
}

TEST_F(CsaLoadEliminationTest, Int8_to_Uint64) {
  SETUP_SIMPLE_TEST(Int8, Uint64, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kLoadFromObject);
}

TEST_F(CsaLoadEliminationTest, Int32_to_Int64) {
  SETUP_SIMPLE_TEST(Int32, Int64, param1())

  EXPECT_EQ(ret->InputAt(0)->opcode(), IrOpcode::kLoadFromObject);
}

TEST_F(CsaLoadEliminationTest, Int16_constant) {
  SETUP_SIMPLE_TEST(Int32, Int16, constant(0xfedcba98))

  Int32Matcher m(ret->InputAt(0));

  EXPECT_TRUE(m.HasResolvedValue());
  EXPECT_EQ(m.ResolvedValue(), int32_t(0xffffba98));
}

TEST_F(CsaLoadEliminationTest, Uint8_constant) {
  SETUP_SIMPLE_TEST(Int32, Uint8, constant(0xfedcba98))

  Uint32Matcher m(ret->InputAt(0));

  EXPECT_TRUE(m.HasResolvedValue());
  EXPECT_EQ(m.ResolvedValue(), uint32_t(0x98));
}

#undef SETUP_SIMPLE_TEST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
