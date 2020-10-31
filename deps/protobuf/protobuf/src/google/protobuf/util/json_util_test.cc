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

#include <google/protobuf/util/json_util.h>

#include <list>
#include <string>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/util/internal/testdata/maps.pb.h>
#include <google/protobuf/util/json_format.pb.h>
#include <google/protobuf/util/json_format_proto3.pb.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/util/type_resolver_util.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace util {
namespace {

using proto3::BAR;
using proto3::FOO;
using proto3::TestAny;
using proto3::TestEnumValue;
using proto3::TestMap;
using proto3::TestMessage;
using proto3::TestOneof;
using proto_util_converter::testing::MapIn;

// As functions defined in json_util.h are just thin wrappers around the
// JSON conversion code in //net/proto2/util/converter, in this test we
// only cover some very basic cases to make sure the wrappers have forwarded
// parameters to the underlying implementation correctly. More detailed
// tests are contained in the //net/proto2/util/converter directory.
class JsonUtilTest : public ::testing::Test {
 protected:
  JsonUtilTest() {}

  std::string ToJson(const Message& message, const JsonPrintOptions& options) {
    std::string result;
    GOOGLE_CHECK_OK(MessageToJsonString(message, &result, options));
    return result;
  }

  bool FromJson(const std::string& json, Message* message,
                const JsonParseOptions& options) {
    return JsonStringToMessage(json, message, options).ok();
  }

  bool FromJson(const std::string& json, Message* message) {
    return FromJson(json, message, JsonParseOptions());
  }

  std::unique_ptr<TypeResolver> resolver_;
};

TEST_F(JsonUtilTest, TestWhitespaces) {
  TestMessage m;
  m.mutable_message_value();

  JsonPrintOptions options;
  EXPECT_EQ("{\"messageValue\":{}}", ToJson(m, options));
  options.add_whitespace = true;
  EXPECT_EQ(
      "{\n"
      " \"messageValue\": {}\n"
      "}\n",
      ToJson(m, options));
}

TEST_F(JsonUtilTest, TestDefaultValues) {
  TestMessage m;
  JsonPrintOptions options;
  EXPECT_EQ("{}", ToJson(m, options));
  options.always_print_primitive_fields = true;
  EXPECT_EQ(
      "{\"boolValue\":false,"
      "\"int32Value\":0,"
      "\"int64Value\":\"0\","
      "\"uint32Value\":0,"
      "\"uint64Value\":\"0\","
      "\"floatValue\":0,"
      "\"doubleValue\":0,"
      "\"stringValue\":\"\","
      "\"bytesValue\":\"\","
      "\"enumValue\":\"FOO\","
      "\"repeatedBoolValue\":[],"
      "\"repeatedInt32Value\":[],"
      "\"repeatedInt64Value\":[],"
      "\"repeatedUint32Value\":[],"
      "\"repeatedUint64Value\":[],"
      "\"repeatedFloatValue\":[],"
      "\"repeatedDoubleValue\":[],"
      "\"repeatedStringValue\":[],"
      "\"repeatedBytesValue\":[],"
      "\"repeatedEnumValue\":[],"
      "\"repeatedMessageValue\":[]"
      "}",
      ToJson(m, options));

  options.always_print_primitive_fields = true;
  m.set_string_value("i am a test string value");
  m.set_bytes_value("i am a test bytes value");
  EXPECT_EQ(
      "{\"boolValue\":false,"
      "\"int32Value\":0,"
      "\"int64Value\":\"0\","
      "\"uint32Value\":0,"
      "\"uint64Value\":\"0\","
      "\"floatValue\":0,"
      "\"doubleValue\":0,"
      "\"stringValue\":\"i am a test string value\","
      "\"bytesValue\":\"aSBhbSBhIHRlc3QgYnl0ZXMgdmFsdWU=\","
      "\"enumValue\":\"FOO\","
      "\"repeatedBoolValue\":[],"
      "\"repeatedInt32Value\":[],"
      "\"repeatedInt64Value\":[],"
      "\"repeatedUint32Value\":[],"
      "\"repeatedUint64Value\":[],"
      "\"repeatedFloatValue\":[],"
      "\"repeatedDoubleValue\":[],"
      "\"repeatedStringValue\":[],"
      "\"repeatedBytesValue\":[],"
      "\"repeatedEnumValue\":[],"
      "\"repeatedMessageValue\":[]"
      "}",
      ToJson(m, options));

  options.preserve_proto_field_names = true;
  m.set_string_value("i am a test string value");
  m.set_bytes_value("i am a test bytes value");
  EXPECT_EQ(
      "{\"bool_value\":false,"
      "\"int32_value\":0,"
      "\"int64_value\":\"0\","
      "\"uint32_value\":0,"
      "\"uint64_value\":\"0\","
      "\"float_value\":0,"
      "\"double_value\":0,"
      "\"string_value\":\"i am a test string value\","
      "\"bytes_value\":\"aSBhbSBhIHRlc3QgYnl0ZXMgdmFsdWU=\","
      "\"enum_value\":\"FOO\","
      "\"repeated_bool_value\":[],"
      "\"repeated_int32_value\":[],"
      "\"repeated_int64_value\":[],"
      "\"repeated_uint32_value\":[],"
      "\"repeated_uint64_value\":[],"
      "\"repeated_float_value\":[],"
      "\"repeated_double_value\":[],"
      "\"repeated_string_value\":[],"
      "\"repeated_bytes_value\":[],"
      "\"repeated_enum_value\":[],"
      "\"repeated_message_value\":[]"
      "}",
      ToJson(m, options));
}

TEST_F(JsonUtilTest, TestPreserveProtoFieldNames) {
  TestMessage m;
  m.mutable_message_value();

  JsonPrintOptions options;
  options.preserve_proto_field_names = true;
  EXPECT_EQ("{\"message_value\":{}}", ToJson(m, options));
}

TEST_F(JsonUtilTest, TestAlwaysPrintEnumsAsInts) {
  TestMessage orig;
  orig.set_enum_value(proto3::BAR);
  orig.add_repeated_enum_value(proto3::FOO);
  orig.add_repeated_enum_value(proto3::BAR);

  JsonPrintOptions print_options;
  print_options.always_print_enums_as_ints = true;

  std::string expected_json = "{\"enumValue\":1,\"repeatedEnumValue\":[0,1]}";
  EXPECT_EQ(expected_json, ToJson(orig, print_options));

  TestMessage parsed;
  JsonParseOptions parse_options;
  ASSERT_TRUE(FromJson(expected_json, &parsed, parse_options));

  EXPECT_EQ(proto3::BAR, parsed.enum_value());
  EXPECT_EQ(2, parsed.repeated_enum_value_size());
  EXPECT_EQ(proto3::FOO, parsed.repeated_enum_value(0));
  EXPECT_EQ(proto3::BAR, parsed.repeated_enum_value(1));
}

TEST_F(JsonUtilTest, TestPrintEnumsAsIntsWithDefaultValue) {
  TestEnumValue orig;
  // orig.set_enum_value1(proto3::FOO)
  orig.set_enum_value2(proto3::FOO);
  orig.set_enum_value3(proto3::BAR);

  JsonPrintOptions print_options;
  print_options.always_print_enums_as_ints = true;
  print_options.always_print_primitive_fields = true;

  std::string expected_json =
      "{\"enumValue1\":0,\"enumValue2\":0,\"enumValue3\":1}";
  EXPECT_EQ(expected_json, ToJson(orig, print_options));

  TestEnumValue parsed;
  JsonParseOptions parse_options;
  ASSERT_TRUE(FromJson(expected_json, &parsed, parse_options));

  EXPECT_EQ(proto3::FOO, parsed.enum_value1());
  EXPECT_EQ(proto3::FOO, parsed.enum_value2());
  EXPECT_EQ(proto3::BAR, parsed.enum_value3());
}

TEST_F(JsonUtilTest, ParseMessage) {
  // Some random message but good enough to verify that the parsing warpper
  // functions are working properly.
  std::string input =
      "{\n"
      "  \"int32Value\": 1024,\n"
      "  \"repeatedInt32Value\": [1, 2],\n"
      "  \"messageValue\": {\n"
      "    \"value\": 2048\n"
      "  },\n"
      "  \"repeatedMessageValue\": [\n"
      "    {\"value\": 40}, {\"value\": 96}\n"
      "  ]\n"
      "}\n";
  JsonParseOptions options;
  TestMessage m;
  ASSERT_TRUE(FromJson(input, &m, options));
  EXPECT_EQ(1024, m.int32_value());
  ASSERT_EQ(2, m.repeated_int32_value_size());
  EXPECT_EQ(1, m.repeated_int32_value(0));
  EXPECT_EQ(2, m.repeated_int32_value(1));
  EXPECT_EQ(2048, m.message_value().value());
  ASSERT_EQ(2, m.repeated_message_value_size());
  EXPECT_EQ(40, m.repeated_message_value(0).value());
  EXPECT_EQ(96, m.repeated_message_value(1).value());
}

TEST_F(JsonUtilTest, ParseMap) {
  TestMap message;
  (*message.mutable_string_map())["hello"] = 1234;
  JsonPrintOptions print_options;
  JsonParseOptions parse_options;
  EXPECT_EQ("{\"stringMap\":{\"hello\":1234}}", ToJson(message, print_options));
  TestMap other;
  ASSERT_TRUE(FromJson(ToJson(message, print_options), &other, parse_options));
  EXPECT_EQ(message.DebugString(), other.DebugString());
}

TEST_F(JsonUtilTest, ParsePrimitiveMapIn) {
  MapIn message;
  JsonPrintOptions print_options;
  print_options.always_print_primitive_fields = true;
  JsonParseOptions parse_options;
  EXPECT_EQ("{\"other\":\"\",\"things\":[],\"mapInput\":{},\"mapAny\":{}}",
            ToJson(message, print_options));
  MapIn other;
  ASSERT_TRUE(FromJson(ToJson(message, print_options), &other, parse_options));
  EXPECT_EQ(message.DebugString(), other.DebugString());
}

TEST_F(JsonUtilTest, PrintPrimitiveOneof) {
  TestOneof message;
  JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  message.mutable_oneof_message_value();
  EXPECT_EQ("{\"oneofMessageValue\":{\"value\":0}}", ToJson(message, options));

  message.set_oneof_int32_value(1);
  EXPECT_EQ("{\"oneofInt32Value\":1}", ToJson(message, options));
}

TEST_F(JsonUtilTest, TestParseIgnoreUnknownFields) {
  TestMessage m;
  JsonParseOptions options;
  options.ignore_unknown_fields = true;
  EXPECT_TRUE(FromJson("{\"unknownName\":0}", &m, options));
}

TEST_F(JsonUtilTest, TestParseErrors) {
  TestMessage m;
  JsonParseOptions options;
  // Parsing should fail if the field name can not be recognized.
  EXPECT_FALSE(FromJson("{\"unknownName\":0}", &m, options));
  // Parsing should fail if the value is invalid.
  EXPECT_FALSE(FromJson("{\"int32Value\":2147483648}", &m, options));
}

TEST_F(JsonUtilTest, TestDynamicMessage) {
  // Some random message but good enough to test the wrapper functions.
  std::string input =
      "{\n"
      "  \"int32Value\": 1024,\n"
      "  \"repeatedInt32Value\": [1, 2],\n"
      "  \"messageValue\": {\n"
      "    \"value\": 2048\n"
      "  },\n"
      "  \"repeatedMessageValue\": [\n"
      "    {\"value\": 40}, {\"value\": 96}\n"
      "  ]\n"
      "}\n";

  // Create a new DescriptorPool with the same protos as the generated one.
  DescriptorPoolDatabase database(*DescriptorPool::generated_pool());
  DescriptorPool pool(&database);
  // A dynamic version of the test proto.
  DynamicMessageFactory factory;
  std::unique_ptr<Message> message(
      factory.GetPrototype(pool.FindMessageTypeByName("proto3.TestMessage"))
          ->New());
  EXPECT_TRUE(FromJson(input, message.get()));

  // Convert to generated message for easy inspection.
  TestMessage generated;
  EXPECT_TRUE(generated.ParseFromString(message->SerializeAsString()));
  EXPECT_EQ(1024, generated.int32_value());
  ASSERT_EQ(2, generated.repeated_int32_value_size());
  EXPECT_EQ(1, generated.repeated_int32_value(0));
  EXPECT_EQ(2, generated.repeated_int32_value(1));
  EXPECT_EQ(2048, generated.message_value().value());
  ASSERT_EQ(2, generated.repeated_message_value_size());
  EXPECT_EQ(40, generated.repeated_message_value(0).value());
  EXPECT_EQ(96, generated.repeated_message_value(1).value());

  JsonOptions options;
  EXPECT_EQ(ToJson(generated, options), ToJson(*message, options));
}

TEST_F(JsonUtilTest, TestParsingUnknownAnyFields) {
  std::string input =
      "{\n"
      "  \"value\": {\n"
      "    \"@type\": \"type.googleapis.com/proto3.TestMessage\",\n"
      "    \"unknown_field\": \"UNKNOWN_VALUE\",\n"
      "    \"string_value\": \"expected_value\"\n"
      "  }\n"
      "}";

  TestAny m;
  JsonParseOptions options;
  EXPECT_FALSE(FromJson(input, &m, options));

  options.ignore_unknown_fields = true;
  EXPECT_TRUE(FromJson(input, &m, options));

  TestMessage t;
  EXPECT_TRUE(m.value().UnpackTo(&t));
  EXPECT_EQ("expected_value", t.string_value());
}

TEST_F(JsonUtilTest, TestParsingUnknownEnumsProto2) {
  std::string input =
      "{\n"
      "  \"a\": \"UNKNOWN_VALUE\"\n"
      "}";
  protobuf_unittest::TestNumbers m;
  JsonParseOptions options;
  EXPECT_FALSE(FromJson(input, &m, options));

  options.ignore_unknown_fields = true;
  EXPECT_TRUE(FromJson(input, &m, options));
  EXPECT_FALSE(m.has_a());
}

TEST_F(JsonUtilTest, TestParsingUnknownEnumsProto3) {
  TestMessage m;
  {
    JsonParseOptions options;
    ASSERT_FALSE(options.ignore_unknown_fields);
    std::string input =
        "{\n"
        "  \"enum_value\":\"UNKNOWN_VALUE\"\n"
        "}";
    m.set_enum_value(proto3::BAR);
    EXPECT_FALSE(FromJson(input, &m, options));
    ASSERT_EQ(proto3::BAR, m.enum_value());  // Keep previous value

    options.ignore_unknown_fields = true;
    EXPECT_TRUE(FromJson(input, &m, options));
    EXPECT_EQ(0, m.enum_value());  // Unknown enum value must be decoded as 0
  }
  // Integer values are read as usual
  {
    JsonParseOptions options;
    std::string input =
        "{\n"
        "  \"enum_value\":12345\n"
        "}";
    m.set_enum_value(proto3::BAR);
    EXPECT_TRUE(FromJson(input, &m, options));
    ASSERT_EQ(12345, m.enum_value());

    options.ignore_unknown_fields = true;
    EXPECT_TRUE(FromJson(input, &m, options));
    EXPECT_EQ(12345, m.enum_value());
  }

  // Trying to pass an object as an enum field value is always treated as an
  // error
  {
    JsonParseOptions options;
    std::string input =
        "{\n"
        "  \"enum_value\":{}\n"
        "}";
    options.ignore_unknown_fields = true;
    EXPECT_FALSE(FromJson(input, &m, options));
    options.ignore_unknown_fields = false;
    EXPECT_FALSE(FromJson(input, &m, options));
  }
  // Trying to pass an array as an enum field value is always treated as an
  // error
  {
    JsonParseOptions options;
    std::string input =
        "{\n"
        "  \"enum_value\":[]\n"
        "}";
    EXPECT_FALSE(FromJson(input, &m, options));
    options.ignore_unknown_fields = true;
    EXPECT_FALSE(FromJson(input, &m, options));
  }
}

TEST_F(JsonUtilTest, TestParsingEnumIgnoreCase) {
  TestMessage m;
  {
    JsonParseOptions options;
    std::string input =
        "{\n"
        "  \"enum_value\":\"bar\"\n"
        "}";
    m.set_enum_value(proto3::FOO);
    EXPECT_FALSE(FromJson(input, &m, options));
    // Default behavior is case-sensitive, so keep previous value.
    ASSERT_EQ(proto3::FOO, m.enum_value());
  }
  {
    JsonParseOptions options;
    options.case_insensitive_enum_parsing = false;
    std::string input =
        "{\n"
        "  \"enum_value\":\"bar\"\n"
        "}";
    m.set_enum_value(proto3::FOO);
    EXPECT_FALSE(FromJson(input, &m, options));
    ASSERT_EQ(proto3::FOO, m.enum_value());  // Keep previous value
  }
  {
    JsonParseOptions options;
    options.case_insensitive_enum_parsing = true;
    std::string input =
        "{\n"
        "  \"enum_value\":\"bar\"\n"
        "}";
    m.set_enum_value(proto3::FOO);
    EXPECT_TRUE(FromJson(input, &m, options));
    ASSERT_EQ(proto3::BAR, m.enum_value());
  }
}

typedef std::pair<char*, int> Segment;
// A ZeroCopyOutputStream that writes to multiple buffers.
class SegmentedZeroCopyOutputStream : public io::ZeroCopyOutputStream {
 public:
  explicit SegmentedZeroCopyOutputStream(std::list<Segment> segments)
      : segments_(segments),
        last_segment_(static_cast<char*>(NULL), 0),
        byte_count_(0) {}

  virtual bool Next(void** buffer, int* length) {
    if (segments_.empty()) {
      return false;
    }
    last_segment_ = segments_.front();
    segments_.pop_front();
    *buffer = last_segment_.first;
    *length = last_segment_.second;
    byte_count_ += *length;
    return true;
  }

  virtual void BackUp(int length) {
    GOOGLE_CHECK(length <= last_segment_.second);
    segments_.push_front(
        Segment(last_segment_.first + last_segment_.second - length, length));
    last_segment_ = Segment(last_segment_.first, last_segment_.second - length);
    byte_count_ -= length;
  }

  virtual int64 ByteCount() const { return byte_count_; }

 private:
  std::list<Segment> segments_;
  Segment last_segment_;
  int64 byte_count_;
};

// This test splits the output buffer and also the input data into multiple
// segments and checks that the implementation of ZeroCopyStreamByteSink
// handles all possible cases correctly.
TEST(ZeroCopyStreamByteSinkTest, TestAllInputOutputPatterns) {
  static const int kOutputBufferLength = 10;
  // An exhaustive test takes too long, skip some combinations to make the test
  // run faster.
  static const int kSkippedPatternCount = 7;

  char buffer[kOutputBufferLength];
  for (int split_pattern = 0; split_pattern < (1 << (kOutputBufferLength - 1));
       split_pattern += kSkippedPatternCount) {
    // Split the buffer into small segments according to the split_pattern.
    std::list<Segment> segments;
    int segment_start = 0;
    for (int i = 0; i < kOutputBufferLength - 1; ++i) {
      if (split_pattern & (1 << i)) {
        segments.push_back(
            Segment(buffer + segment_start, i - segment_start + 1));
        segment_start = i + 1;
      }
    }
    segments.push_back(
        Segment(buffer + segment_start, kOutputBufferLength - segment_start));

    // Write exactly 10 bytes through the ByteSink.
    std::string input_data = "0123456789";
    for (int input_pattern = 0; input_pattern < (1 << (input_data.size() - 1));
         input_pattern += kSkippedPatternCount) {
      memset(buffer, 0, sizeof(buffer));
      {
        SegmentedZeroCopyOutputStream output_stream(segments);
        internal::ZeroCopyStreamByteSink byte_sink(&output_stream);
        int start = 0;
        for (int j = 0; j < input_data.length() - 1; ++j) {
          if (input_pattern & (1 << j)) {
            byte_sink.Append(&input_data[start], j - start + 1);
            start = j + 1;
          }
        }
        byte_sink.Append(&input_data[start], input_data.length() - start);
      }
      EXPECT_EQ(input_data, std::string(buffer, input_data.length()));
    }

    // Write only 9 bytes through the ByteSink.
    input_data = "012345678";
    for (int input_pattern = 0; input_pattern < (1 << (input_data.size() - 1));
         input_pattern += kSkippedPatternCount) {
      memset(buffer, 0, sizeof(buffer));
      {
        SegmentedZeroCopyOutputStream output_stream(segments);
        internal::ZeroCopyStreamByteSink byte_sink(&output_stream);
        int start = 0;
        for (int j = 0; j < input_data.length() - 1; ++j) {
          if (input_pattern & (1 << j)) {
            byte_sink.Append(&input_data[start], j - start + 1);
            start = j + 1;
          }
        }
        byte_sink.Append(&input_data[start], input_data.length() - start);
      }
      EXPECT_EQ(input_data, std::string(buffer, input_data.length()));
      EXPECT_EQ(0, buffer[input_data.length()]);
    }

    // Write 11 bytes through the ByteSink. The extra byte will just
    // be ignored.
    input_data = "0123456789A";
    for (int input_pattern = 0; input_pattern < (1 << (input_data.size() - 1));
         input_pattern += kSkippedPatternCount) {
      memset(buffer, 0, sizeof(buffer));
      {
        SegmentedZeroCopyOutputStream output_stream(segments);
        internal::ZeroCopyStreamByteSink byte_sink(&output_stream);
        int start = 0;
        for (int j = 0; j < input_data.length() - 1; ++j) {
          if (input_pattern & (1 << j)) {
            byte_sink.Append(&input_data[start], j - start + 1);
            start = j + 1;
          }
        }
        byte_sink.Append(&input_data[start], input_data.length() - start);
      }
      EXPECT_EQ(input_data.substr(0, kOutputBufferLength),
                std::string(buffer, kOutputBufferLength));
    }
  }
}

TEST_F(JsonUtilTest, TestWrongJsonInput) {
  const char json[] = "{\"unknown_field\":\"some_value\"}";
  io::ArrayInputStream input_stream(json, strlen(json));
  char proto_buffer[10000];
  io::ArrayOutputStream output_stream(proto_buffer, sizeof(proto_buffer));
  std::string message_type = "type.googleapis.com/proto3.TestMessage";
  TypeResolver* resolver = NewTypeResolverForDescriptorPool(
      "type.googleapis.com", DescriptorPool::generated_pool());

  auto result_status = util::JsonToBinaryStream(resolver, message_type,
                                                &input_stream, &output_stream);

  delete resolver;

  EXPECT_FALSE(result_status.ok());
  EXPECT_EQ(result_status.code(),
            util::error::INVALID_ARGUMENT);
}

TEST_F(JsonUtilTest, HtmlEscape) {
  TestMessage m;
  m.set_string_value("</script>");
  JsonPrintOptions options;
  EXPECT_EQ("{\"stringValue\":\"\\u003c/script\\u003e\"}", ToJson(m, options));
}

}  // namespace
}  // namespace util
}  // namespace protobuf
}  // namespace google
