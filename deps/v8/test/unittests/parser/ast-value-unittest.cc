// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/heap-inl.h"
#include "src/numbers/hash-seed-inl.h"
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

  const AstRawString* GetOneByteString(const char* str) {
    return ast_value_factory_.GetOneByteString(str);
  }

  DirectHandle<String> CreateTwoByteString(const char* str) {
    int length = static_cast<int>(strlen(str));
    DirectHandle<SeqTwoByteString> result =
        i_isolate()->factory()->NewRawTwoByteString(length).ToHandleChecked();
    DisallowGarbageCollection no_gc;
    CopyChars(result->GetChars(no_gc), str, length);
    return result;
  }

  DirectHandle<String> CreateTwoByteStringWithNonOneByte() {
    DirectHandle<SeqTwoByteString> result =
        i_isolate()->factory()->NewRawTwoByteString(1).ToHandleChecked();
    DisallowGarbageCollection no_gc;
    result->GetChars(no_gc)[0] = 0x1234;
    return result;
  }

  AstValueFactory ast_value_factory_;
  AstNodeFactory ast_node_factory_;
};

TEST_F(AstValueTest, AstRawStringAsArrayIndex) {
  auto test_as_array_index = [&](const char* str, bool expected_success,
                                 uint32_t expected_index) {
    const AstRawString* one_byte = GetOneByteString(str);
    EXPECT_TRUE(one_byte->is_one_byte());
    uint32_t index = 0;
    EXPECT_EQ(one_byte->AsArrayIndex(&index), expected_success);
    if (expected_success) {
      EXPECT_EQ(index, expected_index);
    }
  };

  // Cached array index (length <= Name::kMaxCachedArrayIndexLength).
  test_as_array_index("0", true, 0);
  test_as_array_index("1", true, 1);
  test_as_array_index("1234567", true, 1234567);

  // Uncached array index (length > 7 and <= Name::kMaxArrayIndexSize).
  test_as_array_index("12345678", true, 12345678);
  test_as_array_index("1000000000", true, 1000000000);

  // Max valid array index (4294967294).
  test_as_array_index("4294967294", true, 4294967294u);

  // Overflow (length 10, > 4294967294).
  test_as_array_index("4294967295", false, 0);
  test_as_array_index("5000000000", false, 0);

  // Overflow (length > 10, > Name::kMaxArrayIndexSize).
  test_as_array_index("10000000000", false, 0);

  // Non-index strings.
  test_as_array_index("01", false, 0);
  test_as_array_index("foo", false, 0);
}

TEST_F(AstValueTest, GetStringNormalizesOneByteContent) {
  DirectHandle<String> two_byte_str = CreateTwoByteString("1000000000");
  EXPECT_TRUE(two_byte_str->IsTwoByteRepresentation());
  const AstRawString* raw = ast_value_factory_.GetString(
      *two_byte_str, SharedStringAccessGuardIfNeeded::NotNeeded());
  EXPECT_TRUE(raw->is_one_byte());
  uint32_t index = 0;
  EXPECT_TRUE(raw->AsArrayIndex(&index));
  EXPECT_EQ(index, 1000000000u);

  // Also check that internalizing a two-byte integer-index string (unhashed or
  // pre-hashed) canonicalizes it to one-byte in the StringTable.
  DirectHandle<String> internalized_unhashed =
      i_isolate()->factory()->InternalizeString(two_byte_str);
  EXPECT_TRUE(internalized_unhashed->IsOneByteRepresentation());

  DirectHandle<String> prehashed_two_byte_str =
      CreateTwoByteString("1000000001");
  prehashed_two_byte_str->EnsureRawHash();
  DirectHandle<String> internalized_prehashed =
      i_isolate()->factory()->InternalizeString(prehashed_two_byte_str);
  EXPECT_TRUE(internalized_prehashed->IsOneByteRepresentation());

  DirectHandle<String> non_one_byte_str = CreateTwoByteStringWithNonOneByte();
  EXPECT_TRUE(non_one_byte_str->IsTwoByteRepresentation());
  const AstRawString* raw_non_one_byte = ast_value_factory_.GetString(
      *non_one_byte_str, SharedStringAccessGuardIfNeeded::NotNeeded());
  EXPECT_FALSE(raw_non_one_byte->is_one_byte());
  EXPECT_FALSE(raw_non_one_byte->AsArrayIndex(&index));
}

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
