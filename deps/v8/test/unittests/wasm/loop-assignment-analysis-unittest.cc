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

  BitVector* Analyze(const byte* start, const byte* end) {
    return AnalyzeLoopAssignmentForTesting(zone(), num_locals, start, end);
  }
};

TEST_F(WasmLoopAssignmentAnalyzerTest, Empty0) {
  byte code[] = { 0 };
  BitVector* assigned = Analyze(code, code);
  EXPECT_EQ(assigned, nullptr);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Empty1) {
  byte code[] = {kExprLoop, kVoidCode, 0};
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
    byte code[] = {WASM_LOOP(WASM_SET_ZERO(i))};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      EXPECT_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, TeeOne) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    byte code[] = {WASM_LOOP(WASM_LOCAL_TEE(i, WASM_ZERO))};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      EXPECT_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, OneBeyond) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    byte code[] = {WASM_LOOP(WASM_SET_ZERO(i)), WASM_SET_ZERO(1)};
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
      byte code[] = {WASM_LOOP(WASM_SET_ZERO(i), WASM_SET_ZERO(j))};
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
    byte code[] = {WASM_LOOP(
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
    byte code[] = {WASM_LOOP(WASM_I32V_1(11), kExprLocalSet, U32V_3(i))};

    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      bool expected = i == j;
      EXPECT_EQ(expected, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Break) {
  num_locals = 3;
  byte code[] = {
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
  byte code[] = {
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
  const byte kIter = 0;
  const byte kSum = 3;

  byte code[] = {WASM_BLOCK(
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

TEST_F(WasmLoopAssignmentAnalyzerTest, Malformed) {
  byte code[] = {kExprLoop, kVoidCode, kExprF32Neg, kExprBrTable, 0x0E, 'h',
                 'e',       'l',       'l',         'o',          ',',  ' ',
                 'w',       'o',       'r',         'l',          'd',  '!'};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  EXPECT_EQ(assigned, nullptr);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, InvalidOpcode) {
  byte code[] = {WASM_LOOP(0xFF)};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  EXPECT_EQ(assigned, nullptr);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, regress_642867) {
  static const byte code[] = {
      WASM_LOOP(WASM_ZERO, kExprLocalSet, 0xFA, 0xFF, 0xFF, 0xFF,
                0x0F)};  // local index LEB128 0xFFFFFFFA
  // Just make sure that the analysis does not crash.
  Analyze(code, code + arraysize(code));
}

TEST_F(WasmLoopAssignmentAnalyzerTest, LetInLoopAssigned) {
  num_locals = 5;
  static const byte code[] = {
      WASM_LOOP(WASM_LET_1_V(kI32Code, WASM_I32V_1(42), WASM_SET_ZERO(3)))};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (uint32_t i = 0; i <= num_locals; i++) {
    EXPECT_EQ(assigned->Contains(i), i == 2);
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, LetInLoopNotAssigned) {
  num_locals = 2;
  static const byte code[] = {WASM_LOOP(
      WASM_LET_1_V(kI32Code, WASM_I32V_1(42),
                   WASM_LET_1_V(kI32Code, WASM_I32V_1(42), WASM_SET_ZERO(0),
                                WASM_SET_ZERO(1))))};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (uint32_t i = 0; i <= num_locals; i++) {
    EXPECT_FALSE(assigned->Contains(i));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, AssignmentOutsideOfLet) {
  num_locals = 5;
  static const byte code[] = {
      WASM_LOOP(WASM_LET_1_V(kI32Code, WASM_I32V_1(42), WASM_SET_ZERO(3)),
                WASM_SET_ZERO(4))};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (uint32_t i = 0; i <= num_locals; i++) {
    EXPECT_EQ(assigned->Contains(i), i == 2 || i == 4);
  }
}

#undef WASM_SET_ZERO

}  // namespace wasm
}  // namespace internal
}  // namespace v8
