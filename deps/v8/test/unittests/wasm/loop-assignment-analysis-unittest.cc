// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/utils/bit-vector.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module.h"

#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

#define WASM_SET_ZERO(i) WASM_LOCAL_SET(i, WASM_ZERO)

class WasmLoopAssignmentAnalyzerTest : public TestWithZone {
 public:
  WasmLoopAssignmentAnalyzerTest() : num_locals(0) {}
  TestSignatures sigs;
  uint32_t num_locals;

  BitVector* Analyze(const uint8_t* start, const uint8_t* end,
                     bool* loop_is_innermost = nullptr) {
    return AnalyzeLoopAssignmentForTesting(zone(), num_locals, start, end,
                                           loop_is_innermost);
  }
};

TEST_F(WasmLoopAssignmentAnalyzerTest, Empty0) {
  uint8_t code[] = {0};
  BitVector* assigned = Analyze(code, code);
  EXPECT_EQ(assigned, nullptr);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Empty1) {
  uint8_t code[] = {kExprLoop, kVoidCode, 0};
  for (int i = 0; i < 5; i++) {
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      EXPECT_FALSE(assigned->Contains(j));
    }
    num_locals++;
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, One) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    uint8_t code[] = {WASM_LOOP(WASM_SET_ZERO(i))};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      EXPECT_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, TeeOne) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    uint8_t code[] = {WASM_LOOP(WASM_LOCAL_TEE(i, WASM_ZERO))};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      EXPECT_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, OneBeyond) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    uint8_t code[] = {WASM_LOOP(WASM_SET_ZERO(i)), WASM_SET_ZERO(1)};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      EXPECT_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Two) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      uint8_t code[] = {WASM_LOOP(WASM_SET_ZERO(i), WASM_SET_ZERO(j))};
      BitVector* assigned = Analyze(code, code + arraysize(code));
      for (int k = 0; k < assigned->length(); k++) {
        bool expected = k == i || k == j;
        EXPECT_EQ(expected, assigned->Contains(k));
      }
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, NestedIf) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    uint8_t code[] = {WASM_LOOP(
        WASM_IF_ELSE(WASM_SET_ZERO(0), WASM_SET_ZERO(i), WASM_SET_ZERO(1)))};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      bool expected = i == j || j == 0 || j == 1;
      EXPECT_EQ(expected, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, BigLocal) {
  num_locals = 65000;
  for (int i = 13; i < 65000; i = static_cast<int>(i * 1.5)) {
    uint8_t code[] = {WASM_LOOP(WASM_I32V_1(11), kExprLocalSet, U32V_3(i))};

    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      bool expected = i == j;
      EXPECT_EQ(expected, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Break) {
  num_locals = 3;
  uint8_t code[] = {
      WASM_LOOP(WASM_IF(WASM_LOCAL_GET(0), WASM_BRV(1, WASM_SET_ZERO(1)))),
      WASM_SET_ZERO(0)};

  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (int j = 0; j < assigned->length(); j++) {
    bool expected = j == 1;
    EXPECT_EQ(expected, assigned->Contains(j));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Loop1) {
  num_locals = 5;
  uint8_t code[] = {
      WASM_LOOP(WASM_IF(
          WASM_LOCAL_GET(0),
          WASM_BRV(0, WASM_LOCAL_SET(3, WASM_I32_SUB(WASM_LOCAL_GET(0),
                                                     WASM_I32V_1(1)))))),
      WASM_LOCAL_GET(0)};

  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (int j = 0; j < assigned->length(); j++) {
    bool expected = j == 3;
    EXPECT_EQ(expected, assigned->Contains(j));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Loop2) {
  num_locals = 6;
  const uint8_t kIter = 0;
  const uint8_t kSum = 3;

  uint8_t code[] = {WASM_BLOCK(
      WASM_WHILE(
          WASM_LOCAL_GET(kIter),
          WASM_BLOCK(
              WASM_LOCAL_SET(
                  kSum, WASM_F32_ADD(WASM_LOCAL_GET(kSum),
                                     WASM_LOAD_MEM(MachineType::Float32(),
                                                   WASM_LOCAL_GET(kIter)))),
              WASM_LOCAL_SET(
                  kIter, WASM_I32_SUB(WASM_LOCAL_GET(kIter), WASM_I32V_1(4))))),
      WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO, WASM_LOCAL_GET(kSum)),
      WASM_LOCAL_GET(kIter))};

  BitVector* assigned = Analyze(code + 2, code + arraysize(code));
  for (int j = 0; j < assigned->length(); j++) {
    bool expected = j == kIter || j == kSum;
    EXPECT_EQ(expected, assigned->Contains(j));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, NestedLoop) {
  num_locals = 5;
  uint8_t code[] = {WASM_LOOP(WASM_LOOP(WASM_LOCAL_SET(0, 1)))};

  bool outer_is_innermost = false;
  BitVector* outer_assigned =
      Analyze(code, code + arraysize(code), &outer_is_innermost);
  for (int j = 0; j < outer_assigned->length(); j++) {
    bool expected = j == 0;
    EXPECT_EQ(expected, outer_assigned->Contains(j));
  }
  EXPECT_FALSE(outer_is_innermost);

  bool inner_is_innermost = false;
  BitVector* inner_assigned =
      Analyze(code + 2, code + arraysize(code), &inner_is_innermost);
  for (int j = 0; j < inner_assigned->length(); j++) {
    bool expected = j == 0;
    EXPECT_EQ(expected, inner_assigned->Contains(j));
  }
  EXPECT_TRUE(inner_is_innermost);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Malformed) {
  uint8_t code[] = {kExprLoop, kVoidCode, kExprF32Neg, kExprBrTable, 0x0E, 'h',
                    'e',       'l',       'l',         'o',          ',',  ' ',
                    'w',       'o',       'r',         'l',          'd',  '!'};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  EXPECT_EQ(assigned, nullptr);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, InvalidOpcode) {
  uint8_t code[] = {WASM_LOOP(0xFF)};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  EXPECT_EQ(assigned, nullptr);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, regress_642867) {
  static const uint8_t code[] = {
      WASM_LOOP(WASM_ZERO, kExprLocalSet, 0xFA, 0xFF, 0xFF, 0xFF,
                0x0F)};  // local index LEB128 0xFFFFFFFA
  // Just make sure that the analysis does not crash.
  Analyze(code, code + arraysize(code));
}

#undef WASM_SET_ZERO

}  // namespace wasm
}  // namespace internal
}  // namespace v8
