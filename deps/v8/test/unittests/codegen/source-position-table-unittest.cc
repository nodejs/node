// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/source-position-table.h"

#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

class SourcePositionTableTest : public TestWithIsolate {
 public:
  SourcePositionTableTest() : zone_(isolate()->allocator(), ZONE_NAME) {}
  ~SourcePositionTableTest() override = default;

  SourcePosition toPos(int offset) {
    return SourcePosition(offset, offset % 10 - 1);
  }

  SourcePositionTableBuilder* builder() { return &builder_; }

 private:
  Zone zone_;
  SourcePositionTableBuilder builder_{&zone_};
};

// Some random offsets, mostly at 'suspicious' bit boundaries.
static int offsets[] = {0,   1,   2,    3,    4,     30,      31,  32,
                        33,  62,  63,   64,   65,    126,     127, 128,
                        129, 250, 1000, 9999, 12000, 31415926};

TEST_F(SourcePositionTableTest, EncodeStatement) {
  for (size_t i = 0; i < arraysize(offsets); i++) {
    builder()->AddPosition(offsets[i], toPos(offsets[i]), true);
  }

  // To test correctness, we rely on the assertions in ToSourcePositionTable().
  // (Also below.)
  CHECK(!builder()->ToSourcePositionTable(isolate()).is_null());
}

TEST_F(SourcePositionTableTest, EncodeStatementDuplicates) {
  for (size_t i = 0; i < arraysize(offsets); i++) {
    builder()->AddPosition(offsets[i], toPos(offsets[i]), true);
    builder()->AddPosition(offsets[i], toPos(offsets[i] + 1), true);
  }

  // To test correctness, we rely on the assertions in ToSourcePositionTable().
  // (Also below.)
  CHECK(!builder()->ToSourcePositionTable(isolate()).is_null());
}

TEST_F(SourcePositionTableTest, EncodeExpression) {
  for (size_t i = 0; i < arraysize(offsets); i++) {
    builder()->AddPosition(offsets[i], toPos(offsets[i]), false);
  }
  CHECK(!builder()->ToSourcePositionTable(isolate()).is_null());
}

TEST_F(SourcePositionTableTest, EncodeAscendingPositive) {
  int code_offset = 0;
  int source_position = 0;
  for (size_t i = 0; i < arraysize(offsets); i++) {
    code_offset += offsets[i];
    source_position += offsets[i];
    if (i % 2) {
      builder()->AddPosition(code_offset, toPos(source_position), true);
    } else {
      builder()->AddPosition(code_offset, toPos(source_position), false);
    }
  }

  CHECK(!builder()->ToSourcePositionTable(isolate()).is_null());
}

TEST_F(SourcePositionTableTest, EncodeAscendingNegative) {
  int code_offset = 0;
  // Start with a big source position, then decrement it.
  int source_position = 1 << 26;
  for (size_t i = 0; i < arraysize(offsets); i++) {
    code_offset += offsets[i];
    source_position -= offsets[i];
    if (i % 2) {
      builder()->AddPosition(code_offset, toPos(source_position), true);
    } else {
      builder()->AddPosition(code_offset, toPos(source_position), false);
    }
  }

  CHECK(!builder()->ToSourcePositionTable(isolate()).is_null());
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
