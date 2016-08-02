// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interpreter/source-position-table.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class SourcePositionTableTest : public TestWithIsolateAndZone {
 public:
  SourcePositionTableTest() {}
  ~SourcePositionTableTest() override {}
};

// Some random offsets, mostly at 'suspicious' bit boundaries.
static int offsets[] = {0,   1,   2,    3,    4,     30,      31,  32,
                        33,  62,  63,   64,   65,    126,     127, 128,
                        129, 250, 1000, 9999, 12000, 31415926};

TEST_F(SourcePositionTableTest, EncodeStatement) {
  SourcePositionTableBuilder builder(isolate(), zone());
  for (int i = 0; i < arraysize(offsets); i++) {
    builder.AddPosition(offsets[i], offsets[i], true);
  }

  // To test correctness, we rely on the assertions in ToSourcePositionTable().
  // (Also below.)
  CHECK(!builder.ToSourcePositionTable().is_null());
}

TEST_F(SourcePositionTableTest, EncodeStatementDuplicates) {
  SourcePositionTableBuilder builder(isolate(), zone());
  for (int i = 0; i < arraysize(offsets); i++) {
    builder.AddPosition(offsets[i], offsets[i], true);
    builder.AddPosition(offsets[i], offsets[i] + 1, true);
  }

  // To test correctness, we rely on the assertions in ToSourcePositionTable().
  // (Also below.)
  CHECK(!builder.ToSourcePositionTable().is_null());
}

TEST_F(SourcePositionTableTest, EncodeExpression) {
  SourcePositionTableBuilder builder(isolate(), zone());
  for (int i = 0; i < arraysize(offsets); i++) {
    builder.AddPosition(offsets[i], offsets[i], false);
  }
  CHECK(!builder.ToSourcePositionTable().is_null());
}

TEST_F(SourcePositionTableTest, EncodeAscending) {
  SourcePositionTableBuilder builder(isolate(), zone());

  int accumulator = 0;
  for (int i = 0; i < arraysize(offsets); i++) {
    accumulator += offsets[i];
    if (i % 2) {
      builder.AddPosition(accumulator, accumulator, true);
    } else {
      builder.AddPosition(accumulator, accumulator, false);
    }
  }

  // Also test negative offsets:
  for (int i = 0; i < arraysize(offsets); i++) {
    accumulator -= offsets[i];
    if (i % 2) {
      builder.AddPosition(accumulator, accumulator, true);
    } else {
      builder.AddPosition(accumulator, accumulator, false);
    }
  }

  CHECK(!builder.ToSourcePositionTable().is_null());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
