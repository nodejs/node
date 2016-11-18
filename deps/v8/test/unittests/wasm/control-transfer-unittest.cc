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

struct ExpectedTarget {
  pc_t pc;
  ControlTransfer expected;
};

// For nicer error messages.
class ControlTransferMatcher : public MatcherInterface<const ControlTransfer&> {
 public:
  explicit ControlTransferMatcher(pc_t pc, const ControlTransfer& expected)
      : pc_(pc), expected_(expected) {}

  void DescribeTo(std::ostream* os) const override {
    *os << "@" << pc_ << " {pcdiff = " << expected_.pcdiff
        << ", spdiff = " << expected_.spdiff
        << ", action = " << expected_.action << "}";
  }

  bool MatchAndExplain(const ControlTransfer& input,
                       MatchResultListener* listener) const override {
    if (input.pcdiff != expected_.pcdiff || input.spdiff != expected_.spdiff ||
        input.action != expected_.action) {
      *listener << "@" << pc_ << " {pcdiff = " << input.pcdiff
                << ", spdiff = " << input.spdiff
                << ", action = " << input.action << "}";
      return false;
    }
    return true;
  }

 private:
  pc_t pc_;
  const ControlTransfer& expected_;
};

class ControlTransferTest : public TestWithZone {
 public:
  void CheckControlTransfers(const byte* start, const byte* end,
                             ExpectedTarget* expected_targets,
                             size_t num_targets) {
    ControlTransferMap map =
        WasmInterpreter::ComputeControlTransfersForTesting(zone(), start, end);
    // Check all control targets in the map.
    for (size_t i = 0; i < num_targets; i++) {
      pc_t pc = expected_targets[i].pc;
      auto it = map.find(pc);
      if (it == map.end()) {
        printf("expected control target @ +%zu\n", pc);
        EXPECT_TRUE(false);
      } else {
        ControlTransfer& expected = expected_targets[i].expected;
        ControlTransfer& target = it->second;
        EXPECT_THAT(target,
                    MakeMatcher(new ControlTransferMatcher(pc, expected)));
      }
    }

    // Check there are no other control targets.
    for (pc_t pc = 0; start + pc < end; pc++) {
      bool found = false;
      for (size_t i = 0; i < num_targets; i++) {
        if (expected_targets[i].pc == pc) {
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

// Macro for simplifying tests below.
#define EXPECT_TARGETS(...)                                                    \
  do {                                                                         \
    ExpectedTarget pairs[] = {__VA_ARGS__};                                    \
    CheckControlTransfers(code, code + sizeof(code), pairs, arraysize(pairs)); \
  } while (false)

TEST_F(ControlTransferTest, SimpleIf) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprEnd        // @3
  };
  EXPECT_TARGETS({2, {2, 0, ControlTransfer::kPushVoid}},  // --
                 {3, {1, 0, ControlTransfer::kPushVoid}});
}

TEST_F(ControlTransferTest, SimpleIf1) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprNop,       // @3
      kExprEnd        // @4
  };
  EXPECT_TARGETS({2, {3, 0, ControlTransfer::kPushVoid}},  // --
                 {4, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, SimpleIf2) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprNop,       // @3
      kExprNop,       // @4
      kExprEnd        // @5
  };
  EXPECT_TARGETS({2, {4, 0, ControlTransfer::kPushVoid}},  // --
                 {5, {1, 2, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, SimpleIfElse) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprElse,      // @3
      kExprEnd        // @4
  };
  EXPECT_TARGETS({2, {2, 0, ControlTransfer::kNoAction}},  // --
                 {3, {2, 0, ControlTransfer::kPushVoid}},  // --
                 {4, {1, 0, ControlTransfer::kPushVoid}});
}

TEST_F(ControlTransferTest, SimpleIfElse1) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprNop,       // @3
      kExprElse,      // @4
      kExprNop,       // @5
      kExprEnd        // @6
  };
  EXPECT_TARGETS({2, {3, 0, ControlTransfer::kNoAction}},      // --
                 {4, {3, 1, ControlTransfer::kPopAndRepush}},  // --
                 {6, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, IfBr) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprBr,        // @3
      ARITY_0,        //   +1
      0,              //   +1
      kExprEnd        // @6
  };
  EXPECT_TARGETS({2, {5, 0, ControlTransfer::kPushVoid}},  // --
                 {3, {4, 0, ControlTransfer::kPushVoid}},  // --
                 {6, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, IfBrElse) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprBr,        // @3
      ARITY_0,        //   +1
      0,              //   +1
      kExprElse,      // @6
      kExprEnd        // @7
  };
  EXPECT_TARGETS({2, {5, 0, ControlTransfer::kNoAction}},      // --
                 {3, {5, 0, ControlTransfer::kPushVoid}},      // --
                 {6, {2, 1, ControlTransfer::kPopAndRepush}},  // --
                 {7, {1, 0, ControlTransfer::kPushVoid}});
}

TEST_F(ControlTransferTest, IfElseBr) {
  byte code[] = {
      kExprI32Const,  // @0
      0,              //   +1
      kExprIf,        // @2
      kExprNop,       // @3
      kExprElse,      // @4
      kExprBr,        // @5
      ARITY_0,        //   +1
      0,              //   +1
      kExprEnd        // @8
  };
  EXPECT_TARGETS({2, {3, 0, ControlTransfer::kNoAction}},      // --
                 {4, {5, 1, ControlTransfer::kPopAndRepush}},  // --
                 {5, {4, 0, ControlTransfer::kPushVoid}},      // --
                 {8, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, BlockEmpty) {
  byte code[] = {
      kExprBlock,  // @0
      kExprEnd     // @1
  };
  EXPECT_TARGETS({1, {1, 0, ControlTransfer::kPushVoid}});
}

TEST_F(ControlTransferTest, Br0) {
  byte code[] = {
      kExprBlock,  // @0
      kExprBr,     // @1
      ARITY_0,     //   +1
      0,           //   +1
      kExprEnd     // @4
  };
  EXPECT_TARGETS({1, {4, 0, ControlTransfer::kPushVoid}},
                 {4, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, Br1) {
  byte code[] = {
      kExprBlock,  // @0
      kExprNop,    // @1
      kExprBr,     // @2
      ARITY_0,     //   +1
      0,           //   +1
      kExprEnd     // @5
  };
  EXPECT_TARGETS({2, {4, 1, ControlTransfer::kPopAndRepush}},  // --
                 {5, {1, 2, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, Br2) {
  byte code[] = {
      kExprBlock,  // @0
      kExprNop,    // @1
      kExprNop,    // @2
      kExprBr,     // @3
      ARITY_0,     //   +1
      0,           //   +1
      kExprEnd     // @6
  };
  EXPECT_TARGETS({3, {4, 2, ControlTransfer::kPopAndRepush}},  // --
                 {6, {1, 3, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, Br0b) {
  byte code[] = {
      kExprBlock,  // @0
      kExprBr,     // @1
      ARITY_0,     //   +1
      0,           //   +1
      kExprNop,    // @4
      kExprEnd     // @5
  };
  EXPECT_TARGETS({1, {5, 0, ControlTransfer::kPushVoid}},  // --
                 {5, {1, 2, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, Br0c) {
  byte code[] = {
      kExprBlock,  // @0
      kExprBr,     // @1
      ARITY_0,     //   +1
      0,           //   +1
      kExprNop,    // @4
      kExprNop,    // @5
      kExprEnd     // @6
  };
  EXPECT_TARGETS({1, {6, 0, ControlTransfer::kPushVoid}},  // --
                 {6, {1, 3, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, SimpleLoop1) {
  byte code[] = {
      kExprLoop,  // @0
      kExprBr,    // @1
      ARITY_0,    //   +1
      0,          //   +1
      kExprEnd    // @4
  };
  EXPECT_TARGETS({1, {-1, 0, ControlTransfer::kNoAction}},  // --
                 {4, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, SimpleLoop2) {
  byte code[] = {
      kExprLoop,  // @0
      kExprNop,   // @1
      kExprBr,    // @2
      ARITY_0,    //   +1
      0,          //   +1
      kExprEnd    // @5
  };
  EXPECT_TARGETS({2, {-2, 1, ControlTransfer::kNoAction}},  // --
                 {5, {1, 2, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, SimpleLoopExit1) {
  byte code[] = {
      kExprLoop,  // @0
      kExprBr,    // @1
      ARITY_0,    //   +1
      1,          //   +1
      kExprEnd    // @4
  };
  EXPECT_TARGETS({1, {4, 0, ControlTransfer::kPushVoid}},  // --
                 {4, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, SimpleLoopExit2) {
  byte code[] = {
      kExprLoop,  // @0
      kExprNop,   // @1
      kExprBr,    // @2
      ARITY_0,    //   +1
      1,          //   +1
      kExprEnd    // @5
  };
  EXPECT_TARGETS({2, {4, 1, ControlTransfer::kPopAndRepush}},  // --
                 {5, {1, 2, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, BrTable0) {
  byte code[] = {
      kExprBlock,    // @0
      kExprI8Const,  // @1
      0,             //   +1
      kExprBrTable,  // @3
      ARITY_0,       //   +1
      0,             //   +1
      U32_LE(0),     //   +4
      kExprEnd       // @10
  };
  EXPECT_TARGETS({3, {8, 0, ControlTransfer::kPushVoid}},  // --
                 {10, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, BrTable1) {
  byte code[] = {
      kExprBlock,    // @0
      kExprI8Const,  // @1
      0,             //   +1
      kExprBrTable,  // @3
      ARITY_0,       //   +1
      1,             //   +1
      U32_LE(0),     //   +4
      U32_LE(0),     //   +4
      kExprEnd       // @14
  };
  EXPECT_TARGETS({3, {12, 0, ControlTransfer::kPushVoid}},  // --
                 {4, {11, 0, ControlTransfer::kPushVoid}},  // --
                 {14, {1, 1, ControlTransfer::kPopAndRepush}});
}

TEST_F(ControlTransferTest, BrTable2) {
  byte code[] = {
      kExprBlock,    // @0
      kExprBlock,    // @1
      kExprI8Const,  // @2
      0,             //   +1
      kExprBrTable,  // @4
      ARITY_0,       //   +1
      2,             //   +1
      U32_LE(0),     //   +4
      U32_LE(0),     //   +4
      U32_LE(1),     //   +4
      kExprEnd,      // @19
      kExprEnd       // @19
  };
  EXPECT_TARGETS({4, {16, 0, ControlTransfer::kPushVoid}},      // --
                 {5, {15, 0, ControlTransfer::kPushVoid}},      // --
                 {6, {15, 0, ControlTransfer::kPushVoid}},      // --
                 {19, {1, 1, ControlTransfer::kPopAndRepush}},  // --
                 {20, {1, 1, ControlTransfer::kPopAndRepush}});
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
