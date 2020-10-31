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

// Author: kenton@google.com (Kenton Varda)

#include <iostream>
#include <string>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/arena_test_util.h>
#include <google/protobuf/map_lite_test_util.h>
#include <google/protobuf/map_lite_unittest.pb.h>
#include <google/protobuf/test_util_lite.h>
#include <google/protobuf/unittest_lite.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/wire_format_lite.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {

// Helper methods to test parsing merge behavior.
void ExpectMessageMerged(const unittest::TestAllTypesLite& message) {
  EXPECT_EQ(message.optional_int32(), 3);
  EXPECT_EQ(message.optional_int64(), 2);
  EXPECT_EQ(message.optional_string(), "hello");
}

void AssignParsingMergeMessages(unittest::TestAllTypesLite* msg1,
                                unittest::TestAllTypesLite* msg2,
                                unittest::TestAllTypesLite* msg3) {
  msg1->set_optional_int32(1);
  msg2->set_optional_int64(2);
  msg3->set_optional_int32(3);
  msg3->set_optional_string("hello");
}

void SetAllTypesInEmptyMessageUnknownFields(
    unittest::TestEmptyMessageLite* empty_message) {
  protobuf_unittest::TestAllTypesLite message;
  TestUtilLite::ExpectClear(message);
  TestUtilLite::SetAllFields(&message);
  std::string data = message.SerializeAsString();
  empty_message->ParseFromString(data);
}

void SetSomeTypesInEmptyMessageUnknownFields(
    unittest::TestEmptyMessageLite* empty_message) {
  protobuf_unittest::TestAllTypesLite message;
  TestUtilLite::ExpectClear(message);
  message.set_optional_int32(101);
  message.set_optional_int64(102);
  message.set_optional_uint32(103);
  message.set_optional_uint64(104);
  std::string data = message.SerializeAsString();
  empty_message->ParseFromString(data);
}

TEST(Lite, AllLite1) {
  std::string data;

  {
    protobuf_unittest::TestAllTypesLite message, message2, message3;
    TestUtilLite::ExpectClear(message);
    TestUtilLite::SetAllFields(&message);
    message2.CopyFrom(message);
    data = message.SerializeAsString();
    message3.ParseFromString(data);
    TestUtilLite::ExpectAllFieldsSet(message);
    TestUtilLite::ExpectAllFieldsSet(message2);
    TestUtilLite::ExpectAllFieldsSet(message3);
    TestUtilLite::ModifyRepeatedFields(&message);
    TestUtilLite::ExpectRepeatedFieldsModified(message);
    message.Clear();
    TestUtilLite::ExpectClear(message);
  }
}

TEST(Lite, AllLite2) {
  std::string data;
  {
    protobuf_unittest::TestAllExtensionsLite message, message2, message3;
    TestUtilLite::ExpectExtensionsClear(message);
    TestUtilLite::SetAllExtensions(&message);
    message2.CopyFrom(message);
    std::string extensions_data = message.SerializeAsString();
    message3.ParseFromString(extensions_data);
    TestUtilLite::ExpectAllExtensionsSet(message);
    TestUtilLite::ExpectAllExtensionsSet(message2);
    TestUtilLite::ExpectAllExtensionsSet(message3);
    TestUtilLite::ModifyRepeatedExtensions(&message);
    TestUtilLite::ExpectRepeatedExtensionsModified(message);
    message.Clear();
    TestUtilLite::ExpectExtensionsClear(message);
  }
}

TEST(Lite, AllLite3) {
  std::string data, packed_data;

  {
    protobuf_unittest::TestPackedTypesLite message, message2, message3;
    TestUtilLite::ExpectPackedClear(message);
    TestUtilLite::SetPackedFields(&message);
    message2.CopyFrom(message);
    packed_data = message.SerializeAsString();
    message3.ParseFromString(packed_data);
    TestUtilLite::ExpectPackedFieldsSet(message);
    TestUtilLite::ExpectPackedFieldsSet(message2);
    TestUtilLite::ExpectPackedFieldsSet(message3);
    TestUtilLite::ModifyPackedFields(&message);
    TestUtilLite::ExpectPackedFieldsModified(message);
    message.Clear();
    TestUtilLite::ExpectPackedClear(message);
  }

  {
    protobuf_unittest::TestPackedExtensionsLite message, message2, message3;
    TestUtilLite::ExpectPackedExtensionsClear(message);
    TestUtilLite::SetPackedExtensions(&message);
    message2.CopyFrom(message);
    std::string packed_extensions_data = message.SerializeAsString();
    EXPECT_EQ(packed_extensions_data, packed_data);
    message3.ParseFromString(packed_extensions_data);
    TestUtilLite::ExpectPackedExtensionsSet(message);
    TestUtilLite::ExpectPackedExtensionsSet(message2);
    TestUtilLite::ExpectPackedExtensionsSet(message3);
    TestUtilLite::ModifyPackedExtensions(&message);
    TestUtilLite::ExpectPackedExtensionsModified(message);
    message.Clear();
    TestUtilLite::ExpectPackedExtensionsClear(message);
  }
}

TEST(Lite, AllLite5) {
  std::string data;

  {
    // Test that if an optional or required message/group field appears multiple
    // times in the input, they need to be merged.
    unittest::TestParsingMergeLite::RepeatedFieldsGenerator generator;
    unittest::TestAllTypesLite* msg1;
    unittest::TestAllTypesLite* msg2;
    unittest::TestAllTypesLite* msg3;

#define ASSIGN_REPEATED_FIELD(FIELD) \
  msg1 = generator.add_##FIELD();    \
  msg2 = generator.add_##FIELD();    \
  msg3 = generator.add_##FIELD();    \
  AssignParsingMergeMessages(msg1, msg2, msg3)

    ASSIGN_REPEATED_FIELD(field1);
    ASSIGN_REPEATED_FIELD(field2);
    ASSIGN_REPEATED_FIELD(field3);
    ASSIGN_REPEATED_FIELD(ext1);
    ASSIGN_REPEATED_FIELD(ext2);

#undef ASSIGN_REPEATED_FIELD
#define ASSIGN_REPEATED_GROUP(FIELD)                \
  msg1 = generator.add_##FIELD()->mutable_field1(); \
  msg2 = generator.add_##FIELD()->mutable_field1(); \
  msg3 = generator.add_##FIELD()->mutable_field1(); \
  AssignParsingMergeMessages(msg1, msg2, msg3)

    ASSIGN_REPEATED_GROUP(group1);
    ASSIGN_REPEATED_GROUP(group2);

#undef ASSIGN_REPEATED_GROUP

    std::string buffer;
    generator.SerializeToString(&buffer);
    unittest::TestParsingMergeLite parsing_merge;
    parsing_merge.ParseFromString(buffer);

    // Required and optional fields should be merged.
    ExpectMessageMerged(parsing_merge.required_all_types());
    ExpectMessageMerged(parsing_merge.optional_all_types());
    ExpectMessageMerged(
        parsing_merge.optionalgroup().optional_group_all_types());
    ExpectMessageMerged(parsing_merge.GetExtension(
        unittest::TestParsingMergeLite::optional_ext));

    // Repeated fields should not be merged.
    EXPECT_EQ(parsing_merge.repeated_all_types_size(), 3);
    EXPECT_EQ(parsing_merge.repeatedgroup_size(), 3);
    EXPECT_EQ(parsing_merge.ExtensionSize(
                  unittest::TestParsingMergeLite::repeated_ext),
              3);
  }
}

TEST(Lite, AllLite6) {
  std::string data;

  // Test unknown fields support for lite messages.
  {
    protobuf_unittest::TestAllTypesLite message, message2;
    protobuf_unittest::TestEmptyMessageLite empty_message;
    TestUtilLite::ExpectClear(message);
    TestUtilLite::SetAllFields(&message);
    data = message.SerializeAsString();
    ASSERT_TRUE(empty_message.ParseFromString(data));
    data.clear();
    data = empty_message.SerializeAsString();
    EXPECT_TRUE(message2.ParseFromString(data));
    data = message2.SerializeAsString();
    TestUtilLite::ExpectAllFieldsSet(message2);
    message.Clear();
    TestUtilLite::ExpectClear(message);
  }
}

TEST(Lite, AllLite7) {
  std::string data;

  {
    protobuf_unittest::TestAllExtensionsLite message, message2;
    protobuf_unittest::TestEmptyMessageLite empty_message;
    TestUtilLite::ExpectExtensionsClear(message);
    TestUtilLite::SetAllExtensions(&message);
    data = message.SerializeAsString();
    empty_message.ParseFromString(data);
    data.clear();
    data = empty_message.SerializeAsString();
    message2.ParseFromString(data);
    data = message2.SerializeAsString();
    TestUtilLite::ExpectAllExtensionsSet(message2);
    message.Clear();
    TestUtilLite::ExpectExtensionsClear(message);
  }
}

TEST(Lite, AllLite8) {
  std::string data;

  {
    protobuf_unittest::TestPackedTypesLite message, message2;
    protobuf_unittest::TestEmptyMessageLite empty_message;
    TestUtilLite::ExpectPackedClear(message);
    TestUtilLite::SetPackedFields(&message);
    data = message.SerializeAsString();
    empty_message.ParseFromString(data);
    data.clear();
    data = empty_message.SerializeAsString();
    message2.ParseFromString(data);
    data = message2.SerializeAsString();
    TestUtilLite::ExpectPackedFieldsSet(message2);
    message.Clear();
    TestUtilLite::ExpectPackedClear(message);
  }
}

TEST(Lite, AllLite9) {
  std::string data;

  {
    protobuf_unittest::TestPackedExtensionsLite message, message2;
    protobuf_unittest::TestEmptyMessageLite empty_message;
    TestUtilLite::ExpectPackedExtensionsClear(message);
    TestUtilLite::SetPackedExtensions(&message);
    data = message.SerializeAsString();
    empty_message.ParseFromString(data);
    data.clear();
    data = empty_message.SerializeAsString();
    message2.ParseFromString(data);
    data = message2.SerializeAsString();
    TestUtilLite::ExpectPackedExtensionsSet(message2);
    message.Clear();
    TestUtilLite::ExpectPackedExtensionsClear(message);
  }
}

TEST(Lite, AllLite10) {
  std::string data;

  {
    // Test Unknown fields swap
    protobuf_unittest::TestEmptyMessageLite empty_message, empty_message2;
    SetAllTypesInEmptyMessageUnknownFields(&empty_message);
    SetSomeTypesInEmptyMessageUnknownFields(&empty_message2);
    data = empty_message.SerializeAsString();
    std::string data2 = empty_message2.SerializeAsString();
    empty_message.Swap(&empty_message2);
    EXPECT_EQ(data, empty_message2.SerializeAsString());
    EXPECT_EQ(data2, empty_message.SerializeAsString());
  }
}

TEST(Lite, AllLite11) {
  std::string data;

  {
    // Test unknown fields swap with self
    protobuf_unittest::TestEmptyMessageLite empty_message;
    SetAllTypesInEmptyMessageUnknownFields(&empty_message);
    data = empty_message.SerializeAsString();
    empty_message.Swap(&empty_message);
    EXPECT_EQ(data, empty_message.SerializeAsString());
  }
}

TEST(Lite, AllLite12) {
  std::string data;

  {
    // Test MergeFrom with unknown fields
    protobuf_unittest::TestAllTypesLite message, message2;
    protobuf_unittest::TestEmptyMessageLite empty_message, empty_message2;
    message.set_optional_int32(101);
    message.add_repeated_int32(201);
    message.set_optional_nested_enum(unittest::TestAllTypesLite::BAZ);
    message2.set_optional_int64(102);
    message2.add_repeated_int64(202);
    message2.set_optional_foreign_enum(unittest::FOREIGN_LITE_BAZ);

    data = message.SerializeAsString();
    empty_message.ParseFromString(data);
    data = message2.SerializeAsString();
    empty_message2.ParseFromString(data);
    message.MergeFrom(message2);
    empty_message.MergeFrom(empty_message2);

    data = empty_message.SerializeAsString();
    message2.ParseFromString(data);
    // We do not compare the serialized output of a normal message and a lite
    // message because the order of fields do not match. We convert lite message
    // back into normal message, then compare.
    EXPECT_EQ(message.SerializeAsString(), message2.SerializeAsString());
  }
}

TEST(Lite, AllLite13) {
  std::string data;

  {
    // Test unknown enum value
    protobuf_unittest::TestAllTypesLite message;
    std::string buffer;
    {
      io::StringOutputStream output_stream(&buffer);
      io::CodedOutputStream coded_output(&output_stream);
      internal::WireFormatLite::WriteTag(
          protobuf_unittest::TestAllTypesLite::kOptionalNestedEnumFieldNumber,
          internal::WireFormatLite::WIRETYPE_VARINT, &coded_output);
      coded_output.WriteVarint32(10);
      internal::WireFormatLite::WriteTag(
          protobuf_unittest::TestAllTypesLite::kRepeatedNestedEnumFieldNumber,
          internal::WireFormatLite::WIRETYPE_VARINT, &coded_output);
      coded_output.WriteVarint32(20);
    }
    message.ParseFromString(buffer);
    data = message.SerializeAsString();
    EXPECT_EQ(data, buffer);
  }
}

TEST(Lite, AllLite14) {
  std::string data;

  {
    // Test Clear with unknown fields
    protobuf_unittest::TestEmptyMessageLite empty_message;
    SetAllTypesInEmptyMessageUnknownFields(&empty_message);
    empty_message.Clear();
    EXPECT_EQ(0, empty_message.unknown_fields().size());
  }
}

// Tests for map lite =============================================

TEST(Lite, AllLite15) {
  std::string data;

  {
    // Accessors
    protobuf_unittest::TestMapLite message;

    MapLiteTestUtil::SetMapFields(&message);
    MapLiteTestUtil::ExpectMapFieldsSet(message);

    MapLiteTestUtil::ModifyMapFields(&message);
    MapLiteTestUtil::ExpectMapFieldsModified(message);
  }
}

TEST(Lite, AllLite16) {
  std::string data;

  {
    // SetMapFieldsInitialized
    protobuf_unittest::TestMapLite message;

    MapLiteTestUtil::SetMapFieldsInitialized(&message);
    MapLiteTestUtil::ExpectMapFieldsSetInitialized(message);
  }
}

TEST(Lite, AllLite17) {
  std::string data;

  {
    // Clear
    protobuf_unittest::TestMapLite message;

    MapLiteTestUtil::SetMapFields(&message);
    message.Clear();
    MapLiteTestUtil::ExpectClear(message);
  }
}

TEST(Lite, AllLite18) {
  std::string data;

  {
    // ClearMessageMap
    protobuf_unittest::TestMessageMapLite message;

    // Creates a TestAllTypes with default value
    TestUtilLite::ExpectClear((*message.mutable_map_int32_message())[0]);
  }
}

TEST(Lite, AllLite19) {
  std::string data;

  {
    // CopyFrom
    protobuf_unittest::TestMapLite message1, message2;

    MapLiteTestUtil::SetMapFields(&message1);
    message2.CopyFrom(message1);
    MapLiteTestUtil::ExpectMapFieldsSet(message2);

    // Copying from self should be a no-op.
    message2.CopyFrom(message2);
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
  }
}

TEST(Lite, AllLite20) {
  std::string data;

  {
    // CopyFromMessageMap
    protobuf_unittest::TestMessageMapLite message1, message2;

    (*message1.mutable_map_int32_message())[0].add_repeated_int32(100);
    (*message2.mutable_map_int32_message())[0].add_repeated_int32(101);

    message1.CopyFrom(message2);

    // Checks repeated field is overwritten.
    EXPECT_EQ(1, message1.map_int32_message().at(0).repeated_int32_size());
    EXPECT_EQ(101, message1.map_int32_message().at(0).repeated_int32(0));
  }
}

TEST(Lite, AllLite21) {
  std::string data;

  {
    // SwapWithEmpty
    protobuf_unittest::TestMapLite message1, message2;

    MapLiteTestUtil::SetMapFields(&message1);
    MapLiteTestUtil::ExpectMapFieldsSet(message1);
    MapLiteTestUtil::ExpectClear(message2);

    message1.Swap(&message2);
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
    MapLiteTestUtil::ExpectClear(message1);
  }
}

TEST(Lite, AllLite22) {
  std::string data;

  {
    // SwapWithSelf
    protobuf_unittest::TestMapLite message;

    MapLiteTestUtil::SetMapFields(&message);
    MapLiteTestUtil::ExpectMapFieldsSet(message);

    message.Swap(&message);
    MapLiteTestUtil::ExpectMapFieldsSet(message);
  }
}

TEST(Lite, AllLite23) {
  std::string data;

  {
    // SwapWithOther
    protobuf_unittest::TestMapLite message1, message2;

    MapLiteTestUtil::SetMapFields(&message1);
    MapLiteTestUtil::SetMapFields(&message2);
    MapLiteTestUtil::ModifyMapFields(&message2);

    message1.Swap(&message2);
    MapLiteTestUtil::ExpectMapFieldsModified(message1);
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
  }
}

TEST(Lite, AllLite24) {
  std::string data;

  {
    // CopyConstructor
    protobuf_unittest::TestMapLite message1;
    MapLiteTestUtil::SetMapFields(&message1);

    protobuf_unittest::TestMapLite message2(message1);
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
  }
}

TEST(Lite, AllLite25) {
  std::string data;

  {
    // CopyAssignmentOperator
    protobuf_unittest::TestMapLite message1;
    MapLiteTestUtil::SetMapFields(&message1);

    protobuf_unittest::TestMapLite message2;
    message2 = message1;
    MapLiteTestUtil::ExpectMapFieldsSet(message2);

    // Make sure that self-assignment does something sane.
    message2.operator=(message2);
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
  }
}

TEST(Lite, AllLite26) {
  std::string data;

  {
    // NonEmptyMergeFrom
    protobuf_unittest::TestMapLite message1, message2;

    MapLiteTestUtil::SetMapFields(&message1);

    // This field will test merging into an empty spot.
    (*message2.mutable_map_int32_int32())[1] = 1;
    message1.mutable_map_int32_int32()->erase(1);

    // This tests overwriting.
    (*message2.mutable_map_int32_double())[1] = 1;
    (*message1.mutable_map_int32_double())[1] = 2;

    message1.MergeFrom(message2);
    MapLiteTestUtil::ExpectMapFieldsSet(message1);
  }
}

TEST(Lite, AllLite27) {
  std::string data;

  {
    // MergeFromMessageMap
    protobuf_unittest::TestMessageMapLite message1, message2;

    (*message1.mutable_map_int32_message())[0].add_repeated_int32(100);
    (*message2.mutable_map_int32_message())[0].add_repeated_int32(101);

    message1.MergeFrom(message2);

    // Checks repeated field is overwritten.
    EXPECT_EQ(1, message1.map_int32_message().at(0).repeated_int32_size());
    EXPECT_EQ(101, message1.map_int32_message().at(0).repeated_int32(0));
  }
}

TEST(Lite, AllLite28) {
  std::string data;

  {
    // Test the generated SerializeWithCachedSizesToArray()
    protobuf_unittest::TestMapLite message1, message2;
    std::string data;
    MapLiteTestUtil::SetMapFields(&message1);
    int size = message1.ByteSize();
    data.resize(size);
    ::google::protobuf::uint8* start = reinterpret_cast<::google::protobuf::uint8*>(::google::protobuf::string_as_array(&data));
    ::google::protobuf::uint8* end = message1.SerializeWithCachedSizesToArray(start);
    EXPECT_EQ(size, end - start);
    EXPECT_TRUE(message2.ParseFromString(data));
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
  }
}

TEST(Lite, AllLite29) {
  std::string data;

  {
    // Test the generated SerializeWithCachedSizes()
    protobuf_unittest::TestMapLite message1, message2;
    MapLiteTestUtil::SetMapFields(&message1);
    int size = message1.ByteSize();
    std::string data;
    data.resize(size);
    {
      // Allow the output stream to buffer only one byte at a time.
      io::ArrayOutputStream array_stream(::google::protobuf::string_as_array(&data), size, 1);
      io::CodedOutputStream output_stream(&array_stream);
      message1.SerializeWithCachedSizes(&output_stream);
      EXPECT_FALSE(output_stream.HadError());
      EXPECT_EQ(size, output_stream.ByteCount());
    }
    EXPECT_TRUE(message2.ParseFromString(data));
    MapLiteTestUtil::ExpectMapFieldsSet(message2);
  }
}


TEST(Lite, AllLite32) {
  std::string data;

  {
    // Proto2UnknownEnum
    protobuf_unittest::TestEnumMapPlusExtraLite from;
    (*from.mutable_known_map_field())[0] =
        protobuf_unittest::E_PROTO2_MAP_ENUM_FOO_LITE;
    (*from.mutable_unknown_map_field())[0] =
        protobuf_unittest::E_PROTO2_MAP_ENUM_EXTRA_LITE;
    std::string data;
    from.SerializeToString(&data);

    protobuf_unittest::TestEnumMapLite to;
    EXPECT_TRUE(to.ParseFromString(data));
    EXPECT_EQ(0, to.unknown_map_field().size());
    EXPECT_FALSE(to.mutable_unknown_fields()->empty());
    ASSERT_EQ(1, to.known_map_field().size());
    EXPECT_EQ(protobuf_unittest::PROTO2_MAP_ENUM_FOO_LITE,
              to.known_map_field().at(0));

    data.clear();
    from.Clear();
    to.SerializeToString(&data);
    EXPECT_TRUE(from.ParseFromString(data));
    ASSERT_EQ(1, from.known_map_field().size());
    EXPECT_EQ(protobuf_unittest::E_PROTO2_MAP_ENUM_FOO_LITE,
              from.known_map_field().at(0));
    ASSERT_EQ(1, from.unknown_map_field().size());
    EXPECT_EQ(protobuf_unittest::E_PROTO2_MAP_ENUM_EXTRA_LITE,
              from.unknown_map_field().at(0));
  }
}

TEST(Lite, AllLite33) {
  std::string data;

  {
    // StandardWireFormat
    protobuf_unittest::TestMapLite message;
    std::string data = "\x0A\x04\x08\x01\x10\x01";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    EXPECT_EQ(1, message.map_int32_int32().at(1));
  }
}

TEST(Lite, AllLite34) {
  std::string data;

  {
    // UnorderedWireFormat
    protobuf_unittest::TestMapLite message;

    // put value before key in wire format
    std::string data = "\x0A\x04\x10\x01\x08\x02";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    ASSERT_NE(message.map_int32_int32().find(2),
              message.map_int32_int32().end());
    EXPECT_EQ(1, message.map_int32_int32().at(2));
  }
}

TEST(Lite, AllLite35) {
  std::string data;

  {
    // DuplicatedKeyWireFormat
    protobuf_unittest::TestMapLite message;

    // Two key fields in wire format
    std::string data = "\x0A\x06\x08\x01\x08\x02\x10\x01";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    EXPECT_EQ(1, message.map_int32_int32().at(2));
  }
}

TEST(Lite, AllLite36) {
  std::string data;

  {
    // DuplicatedValueWireFormat
    protobuf_unittest::TestMapLite message;

    // Two value fields in wire format
    std::string data = "\x0A\x06\x08\x01\x10\x01\x10\x02";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    EXPECT_EQ(2, message.map_int32_int32().at(1));
  }
}

TEST(Lite, AllLite37) {
  std::string data;

  {
    // MissedKeyWireFormat
    protobuf_unittest::TestMapLite message;

    // No key field in wire format
    std::string data = "\x0A\x02\x10\x01";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    ASSERT_NE(message.map_int32_int32().find(0),
              message.map_int32_int32().end());
    EXPECT_EQ(1, message.map_int32_int32().at(0));
  }
}

TEST(Lite, AllLite38) {
  std::string data;

  {
    // MissedValueWireFormat
    protobuf_unittest::TestMapLite message;

    // No value field in wire format
    std::string data = "\x0A\x02\x08\x01";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    ASSERT_NE(message.map_int32_int32().find(1),
              message.map_int32_int32().end());
    EXPECT_EQ(0, message.map_int32_int32().at(1));
  }
}

TEST(Lite, AllLite39) {
  std::string data;

  {
    // UnknownFieldWireFormat
    protobuf_unittest::TestMapLite message;

    // Unknown field in wire format
    std::string data = "\x0A\x06\x08\x02\x10\x03\x18\x01";

    EXPECT_TRUE(message.ParseFromString(data));
    ASSERT_EQ(1, message.map_int32_int32().size());
    EXPECT_EQ(3, message.map_int32_int32().at(2));
  }
}

TEST(Lite, AllLite40) {
  std::string data;

  {
    // CorruptedWireFormat
    protobuf_unittest::TestMapLite message;

    // corrupted data in wire format
    std::string data = "\x0A\x06\x08\x02\x11\x03";

    EXPECT_FALSE(message.ParseFromString(data));
  }
}

TEST(Lite, AllLite41) {
  std::string data;

  {
    // IsInitialized
    protobuf_unittest::TestRequiredMessageMapLite map_message;

    // Add an uninitialized message.
    (*map_message.mutable_map_field())[0];
    EXPECT_FALSE(map_message.IsInitialized());

    // Initialize uninitialized message
    (*map_message.mutable_map_field())[0].set_a(0);
    (*map_message.mutable_map_field())[0].set_b(0);
    (*map_message.mutable_map_field())[0].set_c(0);
    EXPECT_TRUE(map_message.IsInitialized());
  }
}

TEST(Lite, AllLite42) {
  std::string data;

  {
    // Check that adding more values to enum does not corrupt message
    // when passed through an old client.
    protobuf_unittest::V2MessageLite v2_message;
    v2_message.set_int_field(800);
    // Set enum field to the value not understood by the old client.
    v2_message.set_enum_field(protobuf_unittest::V2_SECOND);
    std::string v2_bytes = v2_message.SerializeAsString();

    protobuf_unittest::V1MessageLite v1_message;
    v1_message.ParseFromString(v2_bytes);
    EXPECT_TRUE(v1_message.IsInitialized());
    EXPECT_EQ(v1_message.int_field(), v2_message.int_field());
    // V1 client does not understand V2_SECOND value, so it discards it and
    // uses default value instead.
    EXPECT_EQ(v1_message.enum_field(), protobuf_unittest::V1_FIRST);

    // However, when re-serialized, it should preserve enum value.
    std::string v1_bytes = v1_message.SerializeAsString();

    protobuf_unittest::V2MessageLite same_v2_message;
    same_v2_message.ParseFromString(v1_bytes);

    EXPECT_EQ(v2_message.int_field(), same_v2_message.int_field());
    EXPECT_EQ(v2_message.enum_field(), same_v2_message.enum_field());
  }
}

// Test that when parsing a oneof, we can successfully clear whatever already
// happened to be stored in the oneof.
TEST(Lite, AllLite43) {
  protobuf_unittest::TestOneofParsingLite message1;

  message1.set_oneof_int32(17);
  std::string serialized;
  EXPECT_TRUE(message1.SerializeToString(&serialized));

  // Submessage
  {
    protobuf_unittest::TestOneofParsingLite message2;
    message2.mutable_oneof_submessage();
    io::CodedInputStream input_stream(
        reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()), serialized.size());
    EXPECT_TRUE(message2.MergeFromCodedStream(&input_stream));
    EXPECT_EQ(17, message2.oneof_int32());
  }

  // String
  {
    protobuf_unittest::TestOneofParsingLite message2;
    message2.set_oneof_string("string");
    io::CodedInputStream input_stream(
        reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()), serialized.size());
    EXPECT_TRUE(message2.MergeFromCodedStream(&input_stream));
    EXPECT_EQ(17, message2.oneof_int32());
  }

  // Bytes
  {
    protobuf_unittest::TestOneofParsingLite message2;
    message2.set_oneof_bytes("bytes");
    io::CodedInputStream input_stream(
        reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()), serialized.size());
    EXPECT_TRUE(message2.MergeFromCodedStream(&input_stream));
    EXPECT_EQ(17, message2.oneof_int32());
  }
}

// Verify that we can successfully parse fields of various types within oneof
// fields. We also verify that we can parse the same data twice into the same
// message.
TEST(Lite, AllLite44) {
  // Int32
  {
    protobuf_unittest::TestOneofParsingLite original;
    original.set_oneof_int32(17);
    std::string serialized;
    EXPECT_TRUE(original.SerializeToString(&serialized));
    protobuf_unittest::TestOneofParsingLite parsed;
    for (int i = 0; i < 2; ++i) {
      io::CodedInputStream input_stream(
          reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()),
          serialized.size());
      EXPECT_TRUE(parsed.MergeFromCodedStream(&input_stream));
      EXPECT_EQ(17, parsed.oneof_int32());
    }
  }

  // Submessage
  {
    protobuf_unittest::TestOneofParsingLite original;
    original.mutable_oneof_submessage()->set_optional_int32(5);
    std::string serialized;
    EXPECT_TRUE(original.SerializeToString(&serialized));
    protobuf_unittest::TestOneofParsingLite parsed;
    for (int i = 0; i < 2; ++i) {
      io::CodedInputStream input_stream(
          reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()),
          serialized.size());
      EXPECT_TRUE(parsed.MergeFromCodedStream(&input_stream));
      EXPECT_EQ(5, parsed.oneof_submessage().optional_int32());
    }
  }

  // String
  {
    protobuf_unittest::TestOneofParsingLite original;
    original.set_oneof_string("string");
    std::string serialized;
    EXPECT_TRUE(original.SerializeToString(&serialized));
    protobuf_unittest::TestOneofParsingLite parsed;
    for (int i = 0; i < 2; ++i) {
      io::CodedInputStream input_stream(
          reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()),
          serialized.size());
      EXPECT_TRUE(parsed.MergeFromCodedStream(&input_stream));
      EXPECT_EQ("string", parsed.oneof_string());
    }
  }

  // Bytes
  {
    protobuf_unittest::TestOneofParsingLite original;
    original.set_oneof_bytes("bytes");
    std::string serialized;
    EXPECT_TRUE(original.SerializeToString(&serialized));
    protobuf_unittest::TestOneofParsingLite parsed;
    for (int i = 0; i < 2; ++i) {
      io::CodedInputStream input_stream(
          reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()),
          serialized.size());
      EXPECT_TRUE(parsed.MergeFromCodedStream(&input_stream));
      EXPECT_EQ("bytes", parsed.oneof_bytes());
    }
  }

  // Enum
  {
    protobuf_unittest::TestOneofParsingLite original;
    original.set_oneof_enum(protobuf_unittest::V2_SECOND);
    std::string serialized;
    EXPECT_TRUE(original.SerializeToString(&serialized));
    protobuf_unittest::TestOneofParsingLite parsed;
    for (int i = 0; i < 2; ++i) {
      io::CodedInputStream input_stream(
          reinterpret_cast<const ::google::protobuf::uint8*>(serialized.data()),
          serialized.size());
      EXPECT_TRUE(parsed.MergeFromCodedStream(&input_stream));
      EXPECT_EQ(protobuf_unittest::V2_SECOND, parsed.oneof_enum());
    }
  }

  std::cout << "PASS" << std::endl;
}

TEST(Lite, AllLite45) {
  // Test unknown fields are not discarded upon parsing.
  std::string data = "\20\1";  // varint 1 with field number 2

  protobuf_unittest::ForeignMessageLite a;
  EXPECT_TRUE(a.ParseFromString(data));
  io::CodedInputStream input_stream(
      reinterpret_cast<const ::google::protobuf::uint8*>(data.data()), data.size());
  EXPECT_TRUE(a.MergePartialFromCodedStream(&input_stream));

  std::string serialized = a.SerializeAsString();
  EXPECT_EQ(serialized.substr(0, 2), data);
  EXPECT_EQ(serialized.substr(2), data);
}

// The following two tests check for wire compatibility between packed and
// unpacked repeated fields. There used to be a bug in the generated parsing
// code that caused us to calculate the highest possible tag number without
// taking into account that a repeated field might not be in the packed (or
// unpacked) state we expect. These tests specifically check for that issue by
// making sure we can parse repeated fields when the tag is higher than we would
// expect.
TEST(Lite, AllLite46) {
  protobuf_unittest::PackedInt32 packed;
  packed.add_repeated_int32(42);
  std::string serialized;
  ASSERT_TRUE(packed.SerializeToString(&serialized));

  protobuf_unittest::NonPackedInt32 non_packed;
  ASSERT_TRUE(non_packed.ParseFromString(serialized));
  ASSERT_EQ(1, non_packed.repeated_int32_size());
  EXPECT_EQ(42, non_packed.repeated_int32(0));
}

TEST(Lite, AllLite47) {
  protobuf_unittest::NonPackedFixed32 non_packed;
  non_packed.add_repeated_fixed32(42);
  std::string serialized;
  ASSERT_TRUE(non_packed.SerializeToString(&serialized));

  protobuf_unittest::PackedFixed32 packed;
  ASSERT_TRUE(packed.ParseFromString(serialized));
  ASSERT_EQ(1, packed.repeated_fixed32_size());
  EXPECT_EQ(42, packed.repeated_fixed32(0));
}

TEST(Lite, MapCrash) {
  // See b/113635730
  Arena arena;
  auto msg = Arena::CreateMessage<protobuf_unittest::TestMapLite>(&arena);
  // Payload for the map<string, Enum> with a enum varint that's longer >
  // 10 bytes. This causes a parse fail and a subsequent delete. field 16
  // (map<int32, MapEnumLite>) tag = 128+2 = \202 \1
  //   13 long \15
  //   int32 key = 1  (\10 \1)
  //   MapEnumLite value = too long varint (parse error)
  EXPECT_FALSE(msg->ParseFromString(
      "\202\1\15\10\1\200\200\200\200\200\200\200\200\200\200\1"));
}

TEST(Lite, CorrectEnding) {
  protobuf_unittest::TestAllTypesLite msg;
  {
    // All proto wireformat parsers should act the same on parsing data in as
    // much as it concerns the parsing, ie. not the interpretation of the data.
    // TestAllTypesLite is not a group inside another message. So in practice
    // will not encounter an end-group tag. However the parser should behave
    // like any wire format parser should.
    static const char kWireFormat[] = "\204\1";
    io::CodedInputStream cis(reinterpret_cast<const uint8*>(kWireFormat), 2);
    // The old CodedInputStream parser got an optimization (ReadTagNoLastTag)
    // for non-group messages (like TestAllTypesLite) which made it not accept
    // end-group. This is not a real big deal, but I think going forward its
    // good to have all parse loops behave 'exactly' the same.
#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
    EXPECT_TRUE(msg.MergePartialFromCodedStream(&cis));
    EXPECT_FALSE(cis.ConsumedEntireMessage());
    EXPECT_TRUE(cis.LastTagWas(132));
#endif
  }
  {
    // This is an incomplete end-group tag. This should be a genuine parse
    // failure.
    static const char kWireFormat[] = "\214";
    io::CodedInputStream cis(reinterpret_cast<const uint8*>(kWireFormat), 1);
    // Unfortunately the old parser detects a parse error in ReadTag and returns
    // 0 (as it states 0 is an invalid tag). However 0 is not an invalid tag
    // as it can be used to terminate the stream, so this returns true.
#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
    EXPECT_FALSE(msg.MergePartialFromCodedStream(&cis));
#endif
  }
}

TEST(Lite, DebugString) {
  protobuf_unittest::TestAllTypesLite message1, message2;
  EXPECT_TRUE(HasPrefixString(message1.DebugString(), "MessageLite at 0x"));
  EXPECT_TRUE(HasPrefixString(message2.DebugString(), "MessageLite at 0x"));

  // DebugString() and ShortDebugString() are the same for now.
  EXPECT_EQ(message1.DebugString(), message1.ShortDebugString());

  // Even identical lite protos should have different DebugString() output. Part
  // of the reason for including the memory address is so that we get some
  // non-determinism, which should make it easier for us to change the output
  // later without breaking any code.
  EXPECT_NE(message1.DebugString(), message2.DebugString());
}

TEST(Lite, EnumValueToName) {
  EXPECT_EQ("FOREIGN_LITE_FOO", protobuf_unittest::ForeignEnumLite_Name(
                                    protobuf_unittest::FOREIGN_LITE_FOO));
  EXPECT_EQ("FOREIGN_LITE_BAR", protobuf_unittest::ForeignEnumLite_Name(
                                    protobuf_unittest::FOREIGN_LITE_BAR));
  EXPECT_EQ("FOREIGN_LITE_BAZ", protobuf_unittest::ForeignEnumLite_Name(
                                    protobuf_unittest::FOREIGN_LITE_BAZ));
  EXPECT_EQ("", protobuf_unittest::ForeignEnumLite_Name(0));
  EXPECT_EQ("", protobuf_unittest::ForeignEnumLite_Name(999));
}

TEST(Lite, NestedEnumValueToName) {
  EXPECT_EQ("FOO", protobuf_unittest::TestAllTypesLite::NestedEnum_Name(
                       protobuf_unittest::TestAllTypesLite::FOO));
  EXPECT_EQ("BAR", protobuf_unittest::TestAllTypesLite::NestedEnum_Name(
                       protobuf_unittest::TestAllTypesLite::BAR));
  EXPECT_EQ("BAZ", protobuf_unittest::TestAllTypesLite::NestedEnum_Name(
                       protobuf_unittest::TestAllTypesLite::BAZ));
  EXPECT_EQ("", protobuf_unittest::TestAllTypesLite::NestedEnum_Name(0));
  EXPECT_EQ("", protobuf_unittest::TestAllTypesLite::NestedEnum_Name(999));
}

TEST(Lite, EnumNameToValue) {
  protobuf_unittest::ForeignEnumLite value;

  ASSERT_TRUE(
      protobuf_unittest::ForeignEnumLite_Parse("FOREIGN_LITE_FOO", &value));
  EXPECT_EQ(protobuf_unittest::FOREIGN_LITE_FOO, value);

  ASSERT_TRUE(
      protobuf_unittest::ForeignEnumLite_Parse("FOREIGN_LITE_BAR", &value));
  EXPECT_EQ(protobuf_unittest::FOREIGN_LITE_BAR, value);

  ASSERT_TRUE(
      protobuf_unittest::ForeignEnumLite_Parse("FOREIGN_LITE_BAZ", &value));
  EXPECT_EQ(protobuf_unittest::FOREIGN_LITE_BAZ, value);

  // Non-existent values
  EXPECT_FALSE(protobuf_unittest::ForeignEnumLite_Parse("E", &value));
  EXPECT_FALSE(
      protobuf_unittest::ForeignEnumLite_Parse("FOREIGN_LITE_C", &value));
  EXPECT_FALSE(protobuf_unittest::ForeignEnumLite_Parse("G", &value));
}

TEST(Lite, NestedEnumNameToValue) {
  protobuf_unittest::TestAllTypesLite::NestedEnum value;

  ASSERT_TRUE(
      protobuf_unittest::TestAllTypesLite::NestedEnum_Parse("FOO", &value));
  EXPECT_EQ(protobuf_unittest::TestAllTypesLite::FOO, value);

  ASSERT_TRUE(
      protobuf_unittest::TestAllTypesLite::NestedEnum_Parse("BAR", &value));
  EXPECT_EQ(protobuf_unittest::TestAllTypesLite::BAR, value);

  ASSERT_TRUE(
      protobuf_unittest::TestAllTypesLite::NestedEnum_Parse("BAZ", &value));
  EXPECT_EQ(protobuf_unittest::TestAllTypesLite::BAZ, value);

  // Non-existent values
  EXPECT_FALSE(
      protobuf_unittest::TestAllTypesLite::NestedEnum_Parse("A", &value));
  EXPECT_FALSE(
      protobuf_unittest::TestAllTypesLite::NestedEnum_Parse("C", &value));
  EXPECT_FALSE(
      protobuf_unittest::TestAllTypesLite::NestedEnum_Parse("G", &value));
}

TEST(Lite, AliasedEnum) {
  // Enums with allow_alias = true can have multiple entries with the same
  // value.
  EXPECT_EQ("FOO1", protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Name(
                        protobuf_unittest::DupEnum::FOO1));
  EXPECT_EQ("FOO1", protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Name(
                        protobuf_unittest::DupEnum::FOO2));
  EXPECT_EQ("BAR1", protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Name(
                        protobuf_unittest::DupEnum::BAR1));
  EXPECT_EQ("BAR1", protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Name(
                        protobuf_unittest::DupEnum::BAR2));
  EXPECT_EQ("BAZ", protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Name(
                       protobuf_unittest::DupEnum::BAZ));
  EXPECT_EQ("", protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Name(999));

  protobuf_unittest::DupEnum::TestEnumWithDupValueLite value;
  ASSERT_TRUE(
      protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Parse("FOO1", &value));
  EXPECT_EQ(protobuf_unittest::DupEnum::FOO1, value);

  value = static_cast<protobuf_unittest::DupEnum::TestEnumWithDupValueLite>(0);
  ASSERT_TRUE(
      protobuf_unittest::DupEnum::TestEnumWithDupValueLite_Parse("FOO2", &value));
  EXPECT_EQ(protobuf_unittest::DupEnum::FOO2, value);
}

}  // namespace protobuf
}  // namespace google
