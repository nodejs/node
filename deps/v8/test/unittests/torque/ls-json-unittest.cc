// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/ls/json-parser.h"
#include "src/torque/ls/json.h"
#include "src/torque/source-positions.h"
#include "src/torque/utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace internal {
namespace torque {
namespace ls {

TEST(LanguageServerJson, TestJsonPrimitives) {
  const JsonValue true_result = ParseJson("true").value;
  ASSERT_EQ(true_result.tag, JsonValue::BOOL);
  EXPECT_EQ(true_result.ToBool(), true);

  const JsonValue false_result = ParseJson("false").value;
  ASSERT_EQ(false_result.tag, JsonValue::BOOL);
  EXPECT_EQ(false_result.ToBool(), false);

  const JsonValue null_result = ParseJson("null").value;
  ASSERT_EQ(null_result.tag, JsonValue::IS_NULL);

  const JsonValue number = ParseJson("42").value;
  ASSERT_EQ(number.tag, JsonValue::NUMBER);
  EXPECT_EQ(number.ToNumber(), 42);
}

TEST(LanguageServerJson, TestJsonStrings) {
  const JsonValue basic = ParseJson("\"basic\"").value;
  ASSERT_EQ(basic.tag, JsonValue::STRING);
  EXPECT_EQ(basic.ToString(), "basic");

  const JsonValue singleQuote = ParseJson("\"'\"").value;
  ASSERT_EQ(singleQuote.tag, JsonValue::STRING);
  EXPECT_EQ(singleQuote.ToString(), "'");
}

TEST(LanguageServerJson, TestJsonArrays) {
  const JsonValue empty_array = ParseJson("[]").value;
  ASSERT_EQ(empty_array.tag, JsonValue::ARRAY);
  EXPECT_EQ(empty_array.ToArray().size(), (size_t)0);

  const JsonValue number_array = ParseJson("[1, 2, 3, 4]").value;
  ASSERT_EQ(number_array.tag, JsonValue::ARRAY);

  const JsonArray& array = number_array.ToArray();
  ASSERT_EQ(array.size(), (size_t)4);
  ASSERT_EQ(array[1].tag, JsonValue::NUMBER);
  EXPECT_EQ(array[1].ToNumber(), 2);

  const JsonValue string_array_object = ParseJson("[\"a\", \"b\"]").value;
  ASSERT_EQ(string_array_object.tag, JsonValue::ARRAY);

  const JsonArray& string_array = string_array_object.ToArray();
  ASSERT_EQ(string_array.size(), (size_t)2);
  ASSERT_EQ(string_array[1].tag, JsonValue::STRING);
  EXPECT_EQ(string_array[1].ToString(), "b");
}

TEST(LanguageServerJson, TestJsonObjects) {
  const JsonValue empty_object = ParseJson("{}").value;
  ASSERT_EQ(empty_object.tag, JsonValue::OBJECT);
  EXPECT_EQ(empty_object.ToObject().size(), (size_t)0);

  const JsonValue primitive_fields =
      ParseJson("{ \"flag\": true, \"id\": 5}").value;
  EXPECT_EQ(primitive_fields.tag, JsonValue::OBJECT);

  const JsonValue& flag = primitive_fields.ToObject().at("flag");
  ASSERT_EQ(flag.tag, JsonValue::BOOL);
  EXPECT_TRUE(flag.ToBool());

  const JsonValue& id = primitive_fields.ToObject().at("id");
  ASSERT_EQ(id.tag, JsonValue::NUMBER);
  EXPECT_EQ(id.ToNumber(), 5);

  const JsonValue& complex_fields =
      ParseJson("{ \"array\": [], \"object\": { \"name\": \"torque\" } }")
          .value;
  ASSERT_EQ(complex_fields.tag, JsonValue::OBJECT);

  const JsonValue& array = complex_fields.ToObject().at("array");
  ASSERT_EQ(array.tag, JsonValue::ARRAY);
  EXPECT_EQ(array.ToArray().size(), (size_t)0);

  const JsonValue& object = complex_fields.ToObject().at("object");
  ASSERT_EQ(object.tag, JsonValue::OBJECT);
  ASSERT_EQ(object.ToObject().at("name").tag, JsonValue::STRING);
  EXPECT_EQ(object.ToObject().at("name").ToString(), "torque");
}

// These tests currently fail on Windows as there seems to be a linking
// issue with exceptions enabled for Torque.
// TODO(szuend): Remove the OS check when errors are reported differently,
//               or the issue is resolved.
#if !defined(V8_OS_WIN)
using ::testing::HasSubstr;
TEST(LanguageServerJson, ParserError) {
  JsonParserResult result = ParseJson("{]");
  ASSERT_TRUE(result.error.has_value());
  EXPECT_THAT(result.error->message,
              HasSubstr("Parser Error: unexpected token"));
}

TEST(LanguageServerJson, LexerError) {
  JsonParserResult result = ParseJson("{ noquoteskey: null }");
  ASSERT_TRUE(result.error.has_value());
  EXPECT_THAT(result.error->message, HasSubstr("Lexer Error: unknown token"));
}
#endif

}  // namespace ls
}  // namespace torque
}  // namespace internal
}  // namespace v8
