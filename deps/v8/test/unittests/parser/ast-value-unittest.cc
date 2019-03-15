// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/hash-seed-inl.h"
#include "src/heap/heap-inl.h"
#include "src/isolate-inl.h"
#include "src/zone/zone.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class AstValueTest : public TestWithIsolateAndZone {
 protected:
  AstValueTest()
      : ast_value_factory_(zone(), i_isolate()->ast_string_constants(),
                           HashSeed(i_isolate())),
        ast_node_factory_(&ast_value_factory_, zone()) {}

  Literal* NewBigInt(const char* str) {
    return ast_node_factory_.NewBigIntLiteral(AstBigInt(str),
                                              kNoSourcePosition);
  }

  AstValueFactory ast_value_factory_;
  AstNodeFactory ast_node_factory_;
};

TEST_F(AstValueTest, BigIntToBooleanIsTrue) {
  EXPECT_FALSE(NewBigInt("0")->ToBooleanIsTrue());
  EXPECT_FALSE(NewBigInt("0b0")->ToBooleanIsTrue());
  EXPECT_FALSE(NewBigInt("0o0")->ToBooleanIsTrue());
  EXPECT_FALSE(NewBigInt("0x0")->ToBooleanIsTrue());
  EXPECT_FALSE(NewBigInt("0b000")->ToBooleanIsTrue());
  EXPECT_FALSE(NewBigInt("0o00000")->ToBooleanIsTrue());
  EXPECT_FALSE(NewBigInt("0x000000000")->ToBooleanIsTrue());

  EXPECT_TRUE(NewBigInt("3")->ToBooleanIsTrue());
  EXPECT_TRUE(NewBigInt("0b1")->ToBooleanIsTrue());
  EXPECT_TRUE(NewBigInt("0o6")->ToBooleanIsTrue());
  EXPECT_TRUE(NewBigInt("0xA")->ToBooleanIsTrue());
  EXPECT_TRUE(NewBigInt("0b0000001")->ToBooleanIsTrue());
  EXPECT_TRUE(NewBigInt("0o00005000")->ToBooleanIsTrue());
  EXPECT_TRUE(NewBigInt("0x0000D00C0")->ToBooleanIsTrue());
}

}  // namespace internal
}  // namespace v8
