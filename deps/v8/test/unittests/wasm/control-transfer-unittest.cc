// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "src/v8.h"

#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-macro-gen.h"

using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::StringMatchResultListener;

namespace v8 {
namespace internal {
namespace wasm {

#define B1(a) kExprBlock, a, kExprEnd
#define B2(a, b) kExprBlock, a, b, kExprEnd
#define B3(a, b, c) kExprBlock, a, b, c, kExprEnd

#define TRANSFER_VOID 0
#define TRANSFER_ONE 1

struct ExpectedPcDelta {
  pc_t pc;
  pcdiff_t expected;
};

// For nicer error messages.
class ControlTransferMatcher : public MatcherInterface<const pcdiff_t&> {
 public:
  explicit ControlTransferMatcher(pc_t pc, const pcdiff_t& expected)
      : pc_(pc), expected_(expected) {}

  void DescribeTo(std::ostream* os) const override {
    *os << "@" << pc_ << " pcdiff = " << expected_;
  }

  bool MatchAndExplain(const pcdiff_t& input,
                       MatchResultListener* listener) const override {
    if (input != expected_) {
      *listener << "@" << pc_ << " pcdiff = " << input;
      return false;
    }
    return true;
  }

 private:
  pc_t pc_;
  const pcdiff_t& expected_;
};

class ControlTransferTest : public TestWithZone {
 public:
  void CheckPcDeltas(const byte* start, const byte* end,
                     ExpectedPcDelta* expected_deltas, size_t num_targets) {
    ControlTransferMap map =
        WasmInterpreter::ComputeControlTransfersForTesting(zone(), start, end);
    // Check all control targets in the map.
    for (size_t i = 0; i < num_targets; i++) {
      pc_t pc = expected_deltas[i].pc;
      auto it = map.find(pc);
      if (it == map.end()) {
        EXPECT_TRUE(false) << "expected control target @ " << pc;
      } else {
        pcdiff_t expected = expected_deltas[i].expected;
        pcdiff_t& target = it->second;
        EXPECT_THAT(target,
                    MakeMatcher(new ControlTransferMatcher(pc, expected)));
      }
    }

    // Check there are no other control targets.
    CheckNoOtherTargets<ExpectedPcDelta>(start, end, map, expected_deltas,
                                         num_targets);
  }

  template <typename T>
  void CheckNoOtherTargets(const byte* start, const byte* end,
                           ControlTransferMap& map, T* targets,
                           size_t num_targets) {
    // Check there are no other control targets.
    for (pc_t pc = 0; start + pc < end; pc++) {
      bool found = false;
      for (size_t i = 0; i < num_targets; i++) {
        if (targets[i].pc == pc) {
          found = true;
          break;
        }
      }
      if (found) continue;
      if (map.find(pc) != map.end()) {
        printf("expected no control @ +%zu\n", pc);
        EXPECT_TRUE(false);
      }
    }
  }
};

#define EXPECT_PC_DELTAS(...)                                          \
  do {                                                                 \
    ExpectedPcDelta pairs[] = {__VA_ARGS__};                           \
    CheckPcDeltas(code, code + sizeof(code), pairs, arraysize(pairs)); \
  } while (false)

TEST_F(ControlTransferTest, SimpleIf) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprEnd        // @4
  };
  EXPECT_PC_DELTAS({2, 2});
}

TEST_F(ControlTransferTest, SimpleIf1) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprNop,       // @4
      kExprEnd        // @5
  };
  EXPECT_PC_DELTAS({2, 3});
}

TEST_F(ControlTransferTest, SimpleIf2) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprNop,       // @4
      kExprNop,       // @5
      kExprEnd        // @6
  };
  EXPECT_PC_DELTAS({2, 4});
}

TEST_F(ControlTransferTest, SimpleIfElse) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprElse,      // @4
      kExprEnd        // @5
  };
  EXPECT_PC_DELTAS({2, 3}, {4, 2});
}

TEST_F(ControlTransferTest, SimpleIfElse_v1) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprI8Const,   // @4
      0,              // @5
      kExprElse,      // @6
      kExprI8Const,   // @7
      0,              // @8
      kExprEnd        // @9
  };
  EXPECT_PC_DELTAS({2, 5}, {6, 4});
}

TEST_F(ControlTransferTest, SimpleIfElse1) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprElse,      // @4
      kExprNop,       // @5
      kExprEnd        // @6
  };
  EXPECT_PC_DELTAS({2, 3}, {4, 3});
}

TEST_F(ControlTransferTest, IfBr) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprBr,        // @4
      0,              // @5
      kExprEnd        // @6
  };
  EXPECT_PC_DELTAS({2, 4}, {4, 3});
}

TEST_F(ControlTransferTest, IfBrElse) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprBr,        // @4
      0,              // @5
      kExprElse,      // @6
      kExprEnd        // @7
  };
  EXPECT_PC_DELTAS({2, 5}, {4, 4}, {6, 2});
}

TEST_F(ControlTransferTest, IfElseBr) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              // @1
      kExprIf,        // @2
      kLocalVoid,     // @3
      kExprElse,      // @4
      kExprBr,        // @5
      0,              // @6
      kExprEnd        // @7
  };
  EXPECT_PC_DELTAS({2, 3}, {4, 4}, {5, 3});
}

TEST_F(ControlTransferTest, BlockEmpty) {
  byte code[] = {
      kExprBlock,  // @0
      kExprEnd     // @1
  };
  CheckPcDeltas(code, code + sizeof(code), nullptr, 0);
}

TEST_F(ControlTransferTest, Br0) {
  byte code[] = {
      kExprBlock,  // @0
      kLocalVoid,  // @1
      kExprBr,     // @2
      0,           // @3
      kExprEnd     // @4
  };
  EXPECT_PC_DELTAS({2, 3});
}

TEST_F(ControlTransferTest, Br1) {
  byte code[] = {
      kExprBlock,  // @0
      kLocalVoid,  // @1
      kExprNop,    // @2
      kExprBr,     // @3
      0,           // @4
      kExprEnd     // @5
  };
  EXPECT_PC_DELTAS({3, 3});
}

TEST_F(ControlTransferTest, Br_v1a) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprI8Const,  // @2
      0,             // @3
      kExprBr,       // @4
      0,             // @5
      kExprEnd       // @6
  };
  EXPECT_PC_DELTAS({4, 3});
}

TEST_F(ControlTransferTest, Br_v1b) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprI8Const,  // @2
      0,             // @3
      kExprBr,       // @4
      0,             // @5
      kExprEnd       // @6
  };
  EXPECT_PC_DELTAS({4, 3});
}

TEST_F(ControlTransferTest, Br_v1c) {
  byte code[] = {
      kExprI8Const,  // @0
      0,             // @1
      kExprBlock,    // @2
      kLocalVoid,    // @3
      kExprBr,       // @4
      0,             // @5
      kExprEnd       // @6
  };
  EXPECT_PC_DELTAS({4, 3});
}

TEST_F(ControlTransferTest, Br2) {
  byte code[] = {
      kExprBlock,  // @0
      kLocalVoid,  // @1
      kExprNop,    // @2
      kExprNop,    // @3
      kExprBr,     // @4
      0,           // @5
      kExprEnd     // @6
  };
  EXPECT_PC_DELTAS({4, 3});
}

TEST_F(ControlTransferTest, Br0b) {
  byte code[] = {
      kExprBlock,  // @0
      kLocalVoid,  // @1
      kExprBr,     // @2
      0,           // @3
      kExprNop,    // @4
      kExprEnd     // @5
  };
  EXPECT_PC_DELTAS({2, 4});
}

TEST_F(ControlTransferTest, Br0c) {
  byte code[] = {
      kExprBlock,  // @0
      kLocalVoid,  // @1
      kExprBr,     // @2
      0,           // @3
      kExprNop,    // @4
      kExprNop,    // @5
      kExprEnd     // @6
  };
  EXPECT_PC_DELTAS({2, 5});
}

TEST_F(ControlTransferTest, SimpleLoop1) {
  byte code[] = {
      kExprLoop,   // @0
      kLocalVoid,  // @1
      kExprBr,     // @2
      0,           // @3
      kExprEnd     // @4
  };
  EXPECT_PC_DELTAS({2, -2});
}

TEST_F(ControlTransferTest, SimpleLoop2) {
  byte code[] = {
      kExprLoop,   // @0
      kLocalVoid,  // @1
      kExprNop,    // @2
      kExprBr,     // @3
      0,           // @4
      kExprEnd     // @5
  };
  EXPECT_PC_DELTAS({3, -3});
}

TEST_F(ControlTransferTest, SimpleLoopExit1) {
  byte code[] = {
      kExprLoop,   // @0
      kLocalVoid,  // @1
      kExprBr,     // @2
      1,           // @3
      kExprEnd     // @4
  };
  EXPECT_PC_DELTAS({2, 3});
}

TEST_F(ControlTransferTest, SimpleLoopExit2) {
  byte code[] = {
      kExprLoop,   // @0
      kLocalVoid,  // @1
      kExprNop,    // @2
      kExprBr,     // @3
      1,           // @4
      kExprEnd     // @5
  };
  EXPECT_PC_DELTAS({3, 3});
}

TEST_F(ControlTransferTest, BrTable0) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprI8Const,  // @2
      0,             // @3
      kExprBrTable,  // @4
      0,             // @5
      U32V_1(0),     // @6
      kExprEnd       // @7
  };
  EXPECT_PC_DELTAS({4, 4});
}

TEST_F(ControlTransferTest, BrTable0_v1a) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprI8Const,  // @2
      0,             // @3
      kExprI8Const,  // @4
      0,             // @5
      kExprBrTable,  // @6
      0,             // @7
      U32V_1(0),     // @8
      kExprEnd       // @9
  };
  EXPECT_PC_DELTAS({6, 4});
}

TEST_F(ControlTransferTest, BrTable0_v1b) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprI8Const,  // @2
      0,             // @3
      kExprI8Const,  // @4
      0,             // @5
      kExprBrTable,  // @6
      0,             // @7
      U32V_1(0),     // @8
      kExprEnd       // @9
  };
  EXPECT_PC_DELTAS({6, 4});
}

TEST_F(ControlTransferTest, BrTable1) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprI8Const,  // @2
      0,             // @3
      kExprBrTable,  // @4
      1,             // @5
      U32V_1(0),     // @6
      U32V_1(0),     // @7
      kExprEnd       // @8
  };
  EXPECT_PC_DELTAS({4, 5}, {5, 4});
}

TEST_F(ControlTransferTest, BrTable2) {
  byte code[] = {
      kExprBlock,    // @0
      kLocalVoid,    // @1
      kExprBlock,    // @2
      kLocalVoid,    // @3
      kExprI8Const,  // @4
      0,             // @5
      kExprBrTable,  // @6
      2,             // @7
      U32V_1(0),     // @8
      U32V_1(0),     // @9
      U32V_1(1),     // @10
      kExprEnd,      // @11
      kExprEnd       // @12
  };
  EXPECT_PC_DELTAS({6, 6}, {7, 5}, {8, 5});
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
