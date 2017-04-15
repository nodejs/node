// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/objects.h"
#include "src/source-position-table.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class SourcePositionTableTest : public TestWithIsolateAndZone {
 public:
  SourcePositionTableTest() {}
  ~SourcePositionTableTest() override {}

  SourcePosition toPos(int offset) {
    return SourcePosition(offset, offset % 10 - 1);
  }
};

// Some random offsets, mostly at 'suspicious' bit boundaries.
static int offsets[] = {0,   1,   2,    3,    4,     30,      31,  32,
                        33,  62,  63,   64,   65,    126,     127, 128,
                        129, 250, 1000, 9999, 12000, 31415926};

TEST_F(SourcePositionTableTest, EncodeStatement) {
  SourcePositionTableBuilder builder(zone());
  for (size_t i = 0; i < arraysize(offsets); i++) {
    builder.AddPosition(offsets[i], toPos(offsets[i]), true);
  }

  // To test correctness, we rely on the assertions in ToSourcePositionTable().
  // (Also below.)
  CHECK(!builder.ToSourcePositionTable(isolate(), Handle<AbstractCode>())
             .is_null());
}

TEST_F(SourcePositionTableTest, EncodeStatementDuplicates) {
  SourcePositionTableBuilder builder(zone());
  for (size_t i = 0; i < arraysize(offsets); i++) {
    builder.AddPosition(offsets[i], toPos(offsets[i]), true);
    builder.AddPosition(offsets[i], toPos(offsets[i] + 1), true);
  }

  // To test correctness, we rely on the assertions in ToSourcePositionTable().
  // (Also below.)
  CHECK(!builder.ToSourcePositionTable(isolate(), Handle<AbstractCode>())
             .is_null());
}

TEST_F(SourcePositionTableTest, EncodeExpression) {
  SourcePositionTableBuilder builder(zone());
  for (size_t i = 0; i < arraysize(offsets); i++) {
    builder.AddPosition(offsets[i], toPos(offsets[i]), false);
  }
  CHECK(!builder.ToSourcePositionTable(isolate(), Handle<AbstractCode>())
             .is_null());
}

TEST_F(SourcePositionTableTest, EncodeAscending) {
  SourcePositionTableBuilder builder(zone());

  int code_offset = 0;
  int source_position = 0;
  for (size_t i = 0; i < arraysize(offsets); i++) {
    code_offset += offsets[i];
    source_position += offsets[i];
    if (i % 2) {
      builder.AddPosition(code_offset, toPos(source_position), true);
    } else {
      builder.AddPosition(code_offset, toPos(source_position), false);
    }
  }

  // Also test negative offsets for source positions:
  for (size_t i = 0; i < arraysize(offsets); i++) {
    code_offset += offsets[i];
    source_position -= offsets[i];
    if (i % 2) {
      builder.AddPosition(code_offset, toPos(source_position), true);
    } else {
      builder.AddPosition(code_offset, toPos(source_position), false);
    }
  }

  CHECK(!builder.ToSourcePositionTable(isolate(), Handle<AbstractCode>())
             .is_null());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
