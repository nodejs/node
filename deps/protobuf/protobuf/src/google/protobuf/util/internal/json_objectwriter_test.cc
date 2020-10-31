// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/util/internal/json_objectwriter.h>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/util/internal/utility.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

using io::CodedOutputStream;
using io::StringOutputStream;

class JsonObjectWriterTest : public ::testing::Test {
 protected:
  JsonObjectWriterTest()
      : str_stream_(new StringOutputStream(&output_)),
        out_stream_(new CodedOutputStream(str_stream_)),
        ow_(nullptr) {}

  ~JsonObjectWriterTest() override { delete ow_; }

  std::string CloseStreamAndGetString() {
    delete out_stream_;
    delete str_stream_;
    return output_;
  }

  std::string output_;
  StringOutputStream* const str_stream_;
  CodedOutputStream* const out_stream_;
  JsonObjectWriter* ow_;
};

TEST_F(JsonObjectWriterTest, EmptyRootObject) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")->EndObject();
  EXPECT_EQ("{}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, EmptyObject) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->RenderString("test", "value")
      ->StartObject("empty")
      ->EndObject()
      ->EndObject();
  EXPECT_EQ("{\"test\":\"value\",\"empty\":{}}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, EmptyRootList) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartList("")->EndList();
  EXPECT_EQ("[]", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, EmptyList) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->RenderString("test", "value")
      ->StartList("empty")
      ->EndList()
      ->EndObject();
  EXPECT_EQ("{\"test\":\"value\",\"empty\":[]}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, EmptyObjectKey) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")->RenderString("", "value")->EndObject();
  EXPECT_EQ("{\"\":\"value\"}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, ObjectInObject) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->StartObject("nested")
      ->RenderString("field", "value")
      ->EndObject()
      ->EndObject();
  EXPECT_EQ("{\"nested\":{\"field\":\"value\"}}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, ListInObject) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->StartList("nested")
      ->RenderString("", "value")
      ->EndList()
      ->EndObject();
  EXPECT_EQ("{\"nested\":[\"value\"]}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, ObjectInList) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartList("")
      ->StartObject("")
      ->RenderString("field", "value")
      ->EndObject()
      ->EndList();
  EXPECT_EQ("[{\"field\":\"value\"}]", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, ListInList) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartList("")
      ->StartList("")
      ->RenderString("", "value")
      ->EndList()
      ->EndList();
  EXPECT_EQ("[[\"value\"]]", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, RenderPrimitives) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->RenderBool("bool", true)
      ->RenderDouble("double", std::numeric_limits<double>::max())
      ->RenderFloat("float", std::numeric_limits<float>::max())
      ->RenderInt32("int", std::numeric_limits<int32>::min())
      ->RenderInt64("long", std::numeric_limits<int64>::min())
      ->RenderBytes("bytes", "abracadabra")
      ->RenderString("string", "string")
      ->RenderBytes("emptybytes", "")
      ->RenderString("emptystring", string())
      ->EndObject();
  EXPECT_EQ(
      "{\"bool\":true,"
      "\"double\":" +
          ValueAsString<double>(std::numeric_limits<double>::max()) +
          ","
          "\"float\":" +
          ValueAsString<float>(std::numeric_limits<float>::max()) +
          ","
          "\"int\":-2147483648,"
          "\"long\":\"-9223372036854775808\","
          "\"bytes\":\"YWJyYWNhZGFicmE=\","
          "\"string\":\"string\","
          "\"emptybytes\":\"\","
          "\"emptystring\":\"\"}",
      CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, BytesEncodesAsNonWebSafeBase64) {
  std::string s;
  s.push_back('\377');
  s.push_back('\357');
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")->RenderBytes("bytes", s)->EndObject();
  // Non-web-safe would encode this as "/+8="
  EXPECT_EQ("{\"bytes\":\"/+8=\"}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, PrettyPrintList) {
  ow_ = new JsonObjectWriter(" ", out_stream_);
  ow_->StartObject("")
      ->StartList("items")
      ->RenderString("", "item1")
      ->RenderString("", "item2")
      ->RenderString("", "item3")
      ->EndList()
      ->StartList("empty")
      ->EndList()
      ->EndObject();
  EXPECT_EQ(
      "{\n"
      " \"items\": [\n"
      "  \"item1\",\n"
      "  \"item2\",\n"
      "  \"item3\"\n"
      " ],\n"
      " \"empty\": []\n"
      "}\n",
      CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, PrettyPrintObject) {
  ow_ = new JsonObjectWriter(" ", out_stream_);
  ow_->StartObject("")
      ->StartObject("items")
      ->RenderString("key1", "item1")
      ->RenderString("key2", "item2")
      ->RenderString("key3", "item3")
      ->EndObject()
      ->StartObject("empty")
      ->EndObject()
      ->EndObject();
  EXPECT_EQ(
      "{\n"
      " \"items\": {\n"
      "  \"key1\": \"item1\",\n"
      "  \"key2\": \"item2\",\n"
      "  \"key3\": \"item3\"\n"
      " },\n"
      " \"empty\": {}\n"
      "}\n",
      CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, PrettyPrintEmptyObjectInEmptyList) {
  ow_ = new JsonObjectWriter(" ", out_stream_);
  ow_->StartObject("")
      ->StartList("list")
      ->StartObject("")
      ->EndObject()
      ->EndList()
      ->EndObject();
  EXPECT_EQ(
      "{\n"
      " \"list\": [\n"
      "  {}\n"
      " ]\n"
      "}\n",
      CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, PrettyPrintDoubleIndent) {
  ow_ = new JsonObjectWriter("  ", out_stream_);
  ow_->StartObject("")
      ->RenderBool("bool", true)
      ->RenderInt32("int", 42)
      ->EndObject();
  EXPECT_EQ(
      "{\n"
      "  \"bool\": true,\n"
      "  \"int\": 42\n"
      "}\n",
      CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, StringsEscapedAndEnclosedInDoubleQuotes) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")->RenderString("string", "'<>&amp;\\\"\r\n")->EndObject();
  EXPECT_EQ("{\"string\":\"'\\u003c\\u003e&amp;\\\\\\\"\\r\\n\"}",
            CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, Stringification) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->RenderDouble("double_nan", std::numeric_limits<double>::quiet_NaN())
      ->RenderFloat("float_nan", std::numeric_limits<float>::quiet_NaN())
      ->RenderDouble("double_pos", std::numeric_limits<double>::infinity())
      ->RenderFloat("float_pos", std::numeric_limits<float>::infinity())
      ->RenderDouble("double_neg", -std::numeric_limits<double>::infinity())
      ->RenderFloat("float_neg", -std::numeric_limits<float>::infinity())
      ->EndObject();
  EXPECT_EQ(
      "{\"double_nan\":\"NaN\","
      "\"float_nan\":\"NaN\","
      "\"double_pos\":\"Infinity\","
      "\"float_pos\":\"Infinity\","
      "\"double_neg\":\"-Infinity\","
      "\"float_neg\":\"-Infinity\"}",
      CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, TestRegularByteEncoding) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->StartObject("")
      ->RenderBytes("bytes", "\x03\xef\xc0")
      ->EndObject();

  // Test that we get regular (non websafe) base64 encoding on byte fields by
  // default.
  EXPECT_EQ("{\"bytes\":\"A+/A\"}", CloseStreamAndGetString());
}

TEST_F(JsonObjectWriterTest, TestWebsafeByteEncoding) {
  ow_ = new JsonObjectWriter("", out_stream_);
  ow_->set_use_websafe_base64_for_bytes(true);
  ow_->StartObject("")
      ->RenderBytes("bytes", "\x03\xef\xc0\x10")
      ->EndObject();

  // Test that we get websafe base64 encoding when explicitly asked.
  EXPECT_EQ("{\"bytes\":\"A-_AEA==\"}", CloseStreamAndGetString());
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
