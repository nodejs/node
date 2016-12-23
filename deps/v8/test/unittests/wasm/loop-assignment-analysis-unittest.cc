// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/v8.h"

#include "test/common/wasm/test-signatures.h"

#include "src/bit-vector.h"
#include "src/objects.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"

#define WASM_SET_ZERO(i) WASM_SET_LOCAL(i, WASM_ZERO)

namespace v8 {
namespace internal {
namespace wasm {

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
  CHECK_NULL(assigned);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Empty1) {
  byte code[] = {kExprLoop, kLocalVoid, 0};
  for (int i = 0; i < 5; i++) {
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      CHECK_EQ(false, assigned->Contains(j));
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
      CHECK_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, TeeOne) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    byte code[] = {WASM_LOOP(WASM_TEE_LOCAL(i, WASM_ZERO))};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      CHECK_EQ(j == i, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, OneBeyond) {
  num_locals = 5;
  for (int i = 0; i < 5; i++) {
    byte code[] = {WASM_LOOP(WASM_SET_ZERO(i)), WASM_SET_ZERO(1)};
    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      CHECK_EQ(j == i, assigned->Contains(j));
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
        CHECK_EQ(expected, assigned->Contains(k));
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
      CHECK_EQ(expected, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, BigLocal) {
  num_locals = 65000;
  for (int i = 13; i < 65000; i = static_cast<int>(i * 1.5)) {
    byte code[] = {WASM_LOOP(WASM_I8(11), kExprSetLocal, U32V_3(i))};

    BitVector* assigned = Analyze(code, code + arraysize(code));
    for (int j = 0; j < assigned->length(); j++) {
      bool expected = i == j;
      CHECK_EQ(expected, assigned->Contains(j));
    }
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Break) {
  num_locals = 3;
  byte code[] = {
      WASM_LOOP(WASM_IF(WASM_GET_LOCAL(0), WASM_BRV(1, WASM_SET_ZERO(1)))),
      WASM_SET_ZERO(0)};

  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (int j = 0; j < assigned->length(); j++) {
    bool expected = j == 1;
    CHECK_EQ(expected, assigned->Contains(j));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Loop1) {
  num_locals = 5;
  byte code[] = {
      WASM_LOOP(WASM_IF(
          WASM_GET_LOCAL(0),
          WASM_BRV(0, WASM_SET_LOCAL(
                          3, WASM_I32_SUB(WASM_GET_LOCAL(0), WASM_I8(1)))))),
      WASM_GET_LOCAL(0)};

  BitVector* assigned = Analyze(code, code + arraysize(code));
  for (int j = 0; j < assigned->length(); j++) {
    bool expected = j == 3;
    CHECK_EQ(expected, assigned->Contains(j));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Loop2) {
  num_locals = 6;
  const byte kIter = 0;
  const byte kSum = 3;

  byte code[] = {WASM_BLOCK(
      WASM_WHILE(
          WASM_GET_LOCAL(kIter),
          WASM_BLOCK(
              WASM_SET_LOCAL(
                  kSum, WASM_F32_ADD(WASM_GET_LOCAL(kSum),
                                     WASM_LOAD_MEM(MachineType::Float32(),
                                                   WASM_GET_LOCAL(kIter)))),
              WASM_SET_LOCAL(kIter,
                             WASM_I32_SUB(WASM_GET_LOCAL(kIter), WASM_I8(4))))),
      WASM_STORE_MEM(MachineType::Float32(), WASM_ZERO, WASM_GET_LOCAL(kSum)),
      WASM_GET_LOCAL(kIter))};

  BitVector* assigned = Analyze(code + 2, code + arraysize(code));
  for (int j = 0; j < assigned->length(); j++) {
    bool expected = j == kIter || j == kSum;
    CHECK_EQ(expected, assigned->Contains(j));
  }
}

TEST_F(WasmLoopAssignmentAnalyzerTest, Malformed) {
  byte code[] = {kExprLoop, kLocalVoid, kExprF32Neg, kExprBrTable, 0x0e, 'h',
                 'e',       'l',        'l',         'o',          ',',  ' ',
                 'w',       'o',        'r',         'l',          'd',  '!'};
  BitVector* assigned = Analyze(code, code + arraysize(code));
  CHECK_NULL(assigned);
}

TEST_F(WasmLoopAssignmentAnalyzerTest, regress_642867) {
  static const byte code[] = {
      WASM_LOOP(WASM_ZERO, kExprSetLocal, 0xfa, 0xff, 0xff, 0xff,
                0x0f)};  // local index LEB128 0xfffffffa
  // Just make sure that the analysis does not crash.
  Analyze(code, code + arraysize(code));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
