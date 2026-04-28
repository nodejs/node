// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"

#include "src/interpreter/bytecode-source-info.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

TEST(BytecodeSourceInfo, Operations) {
  BytecodeSourceInfo x(0, true);
  CHECK_EQ(x.source_position(), 0);
  CHECK_EQ(x.is_statement(), true);
  CHECK_EQ(x.is_valid(), true);
  x.set_invalid();
  CHECK_EQ(x.is_statement(), false);
  CHECK_EQ(x.is_valid(), false);

  x.MakeStatementPosition(1);
  BytecodeSourceInfo y(1, true);
  CHECK(x == y);
  CHECK(!(x != y));

  x.set_invalid();
  CHECK(!(x == y));
  CHECK(x != y);

  y.MakeStatementPosition(1);
  CHECK_EQ(y.source_position(), 1);
  CHECK_EQ(y.is_statement(), true);

  y.MakeStatementPosition(2);
  CHECK_EQ(y.source_position(), 2);
  CHECK_EQ(y.is_statement(), true);

  y.set_invalid();
  y.MakeExpressionPosition(3);
  CHECK_EQ(y.source_position(), 3);
  CHECK_EQ(y.is_statement(), false);

  y.MakeStatementPosition(3);
  CHECK_EQ(y.source_position(), 3);
  CHECK_EQ(y.is_statement(), true);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
