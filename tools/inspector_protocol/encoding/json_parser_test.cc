// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_parser.h"

#include <iostream>
#include <sstream>
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "linux_dev_platform.h"

namespace inspector_protocol {
class Log : public JSONParserHandler {
 public:
  void HandleObjectBegin() override { log_ << "object begin\n"; }

  void HandleObjectEnd() override { log_ << "object end\n"; }

  void HandleArrayBegin() override { log_ << "array begin\n"; }

  void HandleArrayEnd() override { log_ << "array end\n"; }

  void HandleString16(std::vector<uint16_t> chars) override {
    base::StringPiece16 foo(reinterpret_cast<const base::char16*>(chars.data()),
                            chars.size());
    log_ << "string: " << base::UTF16ToUTF8(foo) << "\n";
  }

  void HandleBinary(std::vector<uint8_t> bytes) override {
    // JSON doesn't have native support for arbitrary bytes, so our parser will
    // never call this.
    assert(false);
  }

  void HandleDouble(double value) override {
    log_ << "double: " << value << "\n";
  }

  void HandleInt32(int32_t value) override { log_ << "int: " << value << "\n"; }

  void HandleBool(bool value) override { log_ << "bool: " << value << "\n"; }

  void HandleNull() override { log_ << "null\n"; }

  void HandleError(Status status) override { status_ = status; }

  std::string str() const { return status_.ok() ? log_.str() : ""; }

  Status status() const { return status_; }

 private:
  std::ostringstream log_;
  Status status_;
};

class JsonParserTest : public ::testing::Test {
 protected:
  Log log_;
};

TEST_F(JsonParserTest, SimpleDictionary) {
  std::string json = "{\"foo\": 42}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "int: 42\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, NestedDictionary) {
  std::string json = "{\"foo\": {\"bar\": {\"baz\": 1}, \"bar2\": 2}}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "object begin\n"
      "string: bar\n"
      "object begin\n"
      "string: baz\n"
      "int: 1\n"
      "object end\n"
      "string: bar2\n"
      "int: 2\n"
      "object end\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Doubles) {
  std::string json = "{\"foo\": 3.1415, \"bar\": 31415e-4}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "double: 3.1415\n"
      "string: bar\n"
      "double: 3.1415\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Unicode) {
  // Globe character. 0xF0 0x9F 0x8C 0x8E in utf8, 0xD83C 0xDF0E in utf16.
  std::string json = "{\"msg\": \"Hello, \\uD83C\\uDF0E.\"}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: msg\n"
      "string: Hello, 🌎.\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Unicode_ParseUtf16) {
  // Globe character. utf8: 0xF0 0x9F 0x8C 0x8E; utf16: 0xD83C 0xDF0E.
  // Crescent moon character. utf8: 0xF0 0x9F 0x8C 0x99; utf16: 0xD83C 0xDF19.

  // We provide the moon with json escape, but the earth as utf16 input.
  // Either way they arrive as utf8 (after decoding in log_.str()).
  base::string16 json = base::UTF8ToUTF16("{\"space\": \"🌎 \\uD83C\\uDF19.\"}");
  ParseJSONChars(GetLinuxDevPlatform(),
                 span<uint16_t>(reinterpret_cast<const uint16_t*>(json.data()),
                                json.size()),
                 &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: space\n"
      "string: 🌎 🌙.\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, Unicode_ParseUtf8) {
  // Used below:
  // гласность - example for 2 byte utf8, Russian word "glasnost"
  // 屋 - example for 3 byte utf8, Chinese word for "house"
  // 🌎 - example for 4 byte utf8: 0xF0 0x9F 0x8C 0x8E; utf16: 0xD83C 0xDF0E.
  // 🌙 - example for escapes: utf8: 0xF0 0x9F 0x8C 0x99; utf16: 0xD83C 0xDF19.

  // We provide the moon with json escape, but the earth as utf8 input.
  // Either way they arrive as utf8 (after decoding in log_.str()).
  std::string json =
      "{"
      "\"escapes\": \"\\uD83C\\uDF19\","
      "\"2 byte\":\"гласность\","
      "\"3 byte\":\"屋\","
      "\"4 byte\":\"🌎\""
      "}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: escapes\n"
      "string: 🌙\n"
      "string: 2 byte\n"
      "string: гласность\n"
      "string: 3 byte\n"
      "string: 屋\n"
      "string: 4 byte\n"
      "string: 🌎\n"
      "object end\n",
      log_.str());
}

TEST_F(JsonParserTest, UnprocessedInputRemainsError) {
  // Trailing junk after the valid JSON.
  std::string json = "{\"foo\": 3.1415} junk";
  int64_t junk_idx = json.find("junk");
  EXPECT_GT(junk_idx, 0);
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_UNPROCESSED_INPUT_REMAINS, log_.status().error);
  EXPECT_EQ(junk_idx, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

std::string MakeNestedJson(int depth) {
  std::string json;
  for (int ii = 0; ii < depth; ++ii) json += "{\"foo\":";
  json += "42";
  for (int ii = 0; ii < depth; ++ii) json += "}";
  return json;
}

TEST_F(JsonParserTest, StackLimitExceededError) {
  // kStackLimit is 1000 (see json_parser.cc). First let's
  // try with a small nested example.
  std::string json_3 = MakeNestedJson(3);
  ParseJSONChars(GetLinuxDevPlatform(),
                 span<uint8_t>(reinterpret_cast<const uint8_t*>(json_3.data()),
                               json_3.size()),
                 &log_);
  EXPECT_TRUE(log_.status().ok());
  EXPECT_EQ(
      "object begin\n"
      "string: foo\n"
      "object begin\n"
      "string: foo\n"
      "object begin\n"
      "string: foo\n"
      "int: 42\n"
      "object end\n"
      "object end\n"
      "object end\n",
      log_.str());

  // Now with kStackLimit (1000).
  log_ = Log();
  std::string json_limit = MakeNestedJson(1000);
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json_limit.data()),
                    json_limit.size()),
      &log_);
  EXPECT_TRUE(log_.status().ok());
  // Now with kStackLimit + 1 (1001) - it exceeds in the innermost instance.
  log_ = Log();
  std::string exceeded = MakeNestedJson(1001);
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(exceeded.data()),
                    exceeded.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_STACK_LIMIT_EXCEEDED, log_.status().error);
  EXPECT_EQ(static_cast<std::ptrdiff_t>(strlen("{\"foo\":") * 1001),
            log_.status().pos);
  // Now way past the limit. Still, the point of exceeding is 1001.
  log_ = Log();
  std::string far_out = MakeNestedJson(10000);
  ParseJSONChars(GetLinuxDevPlatform(),
                 span<uint8_t>(reinterpret_cast<const uint8_t*>(far_out.data()),
                               far_out.size()),
                 &log_);
  EXPECT_EQ(Error::JSON_PARSER_STACK_LIMIT_EXCEEDED, log_.status().error);
  EXPECT_EQ(static_cast<std::ptrdiff_t>(strlen("{\"foo\":") * 1001),
            log_.status().pos);
}

TEST_F(JsonParserTest, NoInputError) {
  std::string json = "";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_NO_INPUT, log_.status().error);
  EXPECT_EQ(0, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, InvalidTokenError) {
  std::string json = "|";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_INVALID_TOKEN, log_.status().error);
  EXPECT_EQ(0, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, InvalidNumberError) {
  // Mantissa exceeds max (the constant used here is int64_t max).
  std::string json = "1E9223372036854775807";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_INVALID_NUMBER, log_.status().error);
  EXPECT_EQ(0, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, InvalidStringError) {
  // \x22 is an unsupported escape sequence
  std::string json = "\"foo\\x22\"";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_INVALID_STRING, log_.status().error);
  EXPECT_EQ(0, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, UnexpectedArrayEndError) {
  std::string json = "[1,2,]";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_UNEXPECTED_ARRAY_END, log_.status().error);
  EXPECT_EQ(5, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, CommaOrArrayEndExpectedError) {
  std::string json = "[1,2 2";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_COMMA_OR_ARRAY_END_EXPECTED,
            log_.status().error);
  EXPECT_EQ(5, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, StringLiteralExpectedError) {
  // There's an error because the key bar, a string, is not terminated.
  std::string json = "{\"foo\": 3.1415, \"bar: 31415e-4}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_STRING_LITERAL_EXPECTED, log_.status().error);
  EXPECT_EQ(16, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, ColonExpectedError) {
  std::string json = "{\"foo\", 42}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_COLON_EXPECTED, log_.status().error);
  EXPECT_EQ(6, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, UnexpectedObjectEndError) {
  std::string json = "{\"foo\": 42, }";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_UNEXPECTED_OBJECT_END, log_.status().error);
  EXPECT_EQ(12, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, CommaOrObjectEndExpectedError) {
  // The second separator should be a comma.
  std::string json = "{\"foo\": 3.1415: \"bar\": 0}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_COMMA_OR_OBJECT_END_EXPECTED,
            log_.status().error);
  EXPECT_EQ(14, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

TEST_F(JsonParserTest, ValueExpectedError) {
  std::string json = "}";
  ParseJSONChars(
      GetLinuxDevPlatform(),
      span<uint8_t>(reinterpret_cast<const uint8_t*>(json.data()), json.size()),
      &log_);
  EXPECT_EQ(Error::JSON_PARSER_VALUE_EXPECTED, log_.status().error);
  EXPECT_EQ(0, log_.status().pos);
  EXPECT_EQ("", log_.str());
}

}  // namespace inspector_protocol
