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
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/wire_format.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_mset.pb.h>
#include <google/protobuf/unittest_mset_wire_format.pb.h>
#include <google/protobuf/unittest_proto3_arena.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/stl_util.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {
namespace {

TEST(WireFormatTest, EnumsInSync) {
  // Verify that WireFormatLite::FieldType and WireFormatLite::CppType match
  // FieldDescriptor::Type and FieldDescriptor::CppType.

  EXPECT_EQ(implicit_cast<int>(FieldDescriptor::MAX_TYPE),
            implicit_cast<int>(WireFormatLite::MAX_FIELD_TYPE));
  EXPECT_EQ(implicit_cast<int>(FieldDescriptor::MAX_CPPTYPE),
            implicit_cast<int>(WireFormatLite::MAX_CPPTYPE));

  for (int i = 1; i <= WireFormatLite::MAX_FIELD_TYPE; i++) {
    EXPECT_EQ(implicit_cast<int>(FieldDescriptor::TypeToCppType(
                  static_cast<FieldDescriptor::Type>(i))),
              implicit_cast<int>(WireFormatLite::FieldTypeToCppType(
                  static_cast<WireFormatLite::FieldType>(i))));
  }
}

TEST(WireFormatTest, MaxFieldNumber) {
  // Make sure the max field number constant is accurate.
  EXPECT_EQ((1 << (32 - WireFormatLite::kTagTypeBits)) - 1,
            FieldDescriptor::kMaxNumber);
}

TEST(WireFormatTest, Parse) {
  unittest::TestAllTypes source, dest;
  std::string data;

  // Serialize using the generated code.
  TestUtil::SetAllFields(&source);
  source.SerializeToString(&data);

  // Parse using WireFormat.
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectAllFieldsSet(dest);
}

TEST(WireFormatTest, ParseExtensions) {
  unittest::TestAllExtensions source, dest;
  std::string data;

  // Serialize using the generated code.
  TestUtil::SetAllExtensions(&source);
  source.SerializeToString(&data);

  // Parse using WireFormat.
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectAllExtensionsSet(dest);
}

TEST(WireFormatTest, ParsePacked) {
  unittest::TestPackedTypes source, dest;
  std::string data;

  // Serialize using the generated code.
  TestUtil::SetPackedFields(&source);
  source.SerializeToString(&data);

  // Parse using WireFormat.
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectPackedFieldsSet(dest);
}

TEST(WireFormatTest, ParsePackedFromUnpacked) {
  // Serialize using the generated code.
  unittest::TestUnpackedTypes source;
  TestUtil::SetUnpackedFields(&source);
  std::string data = source.SerializeAsString();

  // Parse using WireFormat.
  unittest::TestPackedTypes dest;
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectPackedFieldsSet(dest);
}

TEST(WireFormatTest, ParseUnpackedFromPacked) {
  // Serialize using the generated code.
  unittest::TestPackedTypes source;
  TestUtil::SetPackedFields(&source);
  std::string data = source.SerializeAsString();

  // Parse using WireFormat.
  unittest::TestUnpackedTypes dest;
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectUnpackedFieldsSet(dest);
}

TEST(WireFormatTest, ParsePackedExtensions) {
  unittest::TestPackedExtensions source, dest;
  std::string data;

  // Serialize using the generated code.
  TestUtil::SetPackedExtensions(&source);
  source.SerializeToString(&data);

  // Parse using WireFormat.
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectPackedExtensionsSet(dest);
}

TEST(WireFormatTest, ParseOneof) {
  unittest::TestOneof2 source, dest;
  std::string data;

  // Serialize using the generated code.
  TestUtil::SetOneof1(&source);
  source.SerializeToString(&data);

  // Parse using WireFormat.
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &dest);

  // Check.
  TestUtil::ExpectOneofSet1(dest);
}

TEST(WireFormatTest, OneofOnlySetLast) {
  unittest::TestOneofBackwardsCompatible source;
  unittest::TestOneof oneof_dest;
  std::string data;

  // Set two fields
  source.set_foo_int(100);
  source.set_foo_string("101");

  // Serialize and parse to oneof message.
  source.SerializeToString(&data);
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream input(&raw_input);
  WireFormat::ParseAndMergePartial(&input, &oneof_dest);

  // Only the last field is set.
  EXPECT_FALSE(oneof_dest.has_foo_int());
  EXPECT_TRUE(oneof_dest.has_foo_string());
}

TEST(WireFormatTest, ByteSize) {
  unittest::TestAllTypes message;
  TestUtil::SetAllFields(&message);

  EXPECT_EQ(message.ByteSize(), WireFormat::ByteSize(message));
  message.Clear();
  EXPECT_EQ(0, message.ByteSize());
  EXPECT_EQ(0, WireFormat::ByteSize(message));
}

TEST(WireFormatTest, ByteSizeExtensions) {
  unittest::TestAllExtensions message;
  TestUtil::SetAllExtensions(&message);

  EXPECT_EQ(message.ByteSize(), WireFormat::ByteSize(message));
  message.Clear();
  EXPECT_EQ(0, message.ByteSize());
  EXPECT_EQ(0, WireFormat::ByteSize(message));
}

TEST(WireFormatTest, ByteSizePacked) {
  unittest::TestPackedTypes message;
  TestUtil::SetPackedFields(&message);

  EXPECT_EQ(message.ByteSize(), WireFormat::ByteSize(message));
  message.Clear();
  EXPECT_EQ(0, message.ByteSize());
  EXPECT_EQ(0, WireFormat::ByteSize(message));
}

TEST(WireFormatTest, ByteSizePackedExtensions) {
  unittest::TestPackedExtensions message;
  TestUtil::SetPackedExtensions(&message);

  EXPECT_EQ(message.ByteSize(), WireFormat::ByteSize(message));
  message.Clear();
  EXPECT_EQ(0, message.ByteSize());
  EXPECT_EQ(0, WireFormat::ByteSize(message));
}

TEST(WireFormatTest, ByteSizeOneof) {
  unittest::TestOneof2 message;
  TestUtil::SetOneof1(&message);

  EXPECT_EQ(message.ByteSize(), WireFormat::ByteSize(message));
  message.Clear();

  EXPECT_EQ(0, message.ByteSize());
  EXPECT_EQ(0, WireFormat::ByteSize(message));
}

TEST(WireFormatTest, Serialize) {
  unittest::TestAllTypes message;
  std::string generated_data;
  std::string dynamic_data;

  TestUtil::SetAllFields(&message);
  int size = message.ByteSize();

  // Serialize using the generated code.
  {
    io::StringOutputStream raw_output(&generated_data);
    io::CodedOutputStream output(&raw_output);
    message.SerializeWithCachedSizes(&output);
    ASSERT_FALSE(output.HadError());
  }

  // Serialize using WireFormat.
  {
    io::StringOutputStream raw_output(&dynamic_data);
    io::CodedOutputStream output(&raw_output);
    WireFormat::SerializeWithCachedSizes(message, size, &output);
    ASSERT_FALSE(output.HadError());
  }

  // Should be the same.
  // Don't use EXPECT_EQ here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  EXPECT_TRUE(dynamic_data == generated_data);
}

TEST(WireFormatTest, SerializeExtensions) {
  unittest::TestAllExtensions message;
  std::string generated_data;
  std::string dynamic_data;

  TestUtil::SetAllExtensions(&message);
  int size = message.ByteSize();

  // Serialize using the generated code.
  {
    io::StringOutputStream raw_output(&generated_data);
    io::CodedOutputStream output(&raw_output);
    message.SerializeWithCachedSizes(&output);
    ASSERT_FALSE(output.HadError());
  }

  // Serialize using WireFormat.
  {
    io::StringOutputStream raw_output(&dynamic_data);
    io::CodedOutputStream output(&raw_output);
    WireFormat::SerializeWithCachedSizes(message, size, &output);
    ASSERT_FALSE(output.HadError());
  }

  // Should be the same.
  // Don't use EXPECT_EQ here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  EXPECT_TRUE(dynamic_data == generated_data);
}

TEST(WireFormatTest, SerializeFieldsAndExtensions) {
  unittest::TestFieldOrderings message;
  std::string generated_data;
  std::string dynamic_data;

  TestUtil::SetAllFieldsAndExtensions(&message);
  int size = message.ByteSize();

  // Serialize using the generated code.
  {
    io::StringOutputStream raw_output(&generated_data);
    io::CodedOutputStream output(&raw_output);
    message.SerializeWithCachedSizes(&output);
    ASSERT_FALSE(output.HadError());
  }

  // Serialize using WireFormat.
  {
    io::StringOutputStream raw_output(&dynamic_data);
    io::CodedOutputStream output(&raw_output);
    WireFormat::SerializeWithCachedSizes(message, size, &output);
    ASSERT_FALSE(output.HadError());
  }

  // Should be the same.
  // Don't use EXPECT_EQ here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  EXPECT_TRUE(dynamic_data == generated_data);

  // Should output in canonical order.
  TestUtil::ExpectAllFieldsAndExtensionsInOrder(dynamic_data);
  TestUtil::ExpectAllFieldsAndExtensionsInOrder(generated_data);
}

TEST(WireFormatTest, SerializeOneof) {
  unittest::TestOneof2 message;
  std::string generated_data;
  std::string dynamic_data;

  TestUtil::SetOneof1(&message);
  int size = message.ByteSize();

  // Serialize using the generated code.
  {
    io::StringOutputStream raw_output(&generated_data);
    io::CodedOutputStream output(&raw_output);
    message.SerializeWithCachedSizes(&output);
    ASSERT_FALSE(output.HadError());
  }

  // Serialize using WireFormat.
  {
    io::StringOutputStream raw_output(&dynamic_data);
    io::CodedOutputStream output(&raw_output);
    WireFormat::SerializeWithCachedSizes(message, size, &output);
    ASSERT_FALSE(output.HadError());
  }

  // Should be the same.
  // Don't use EXPECT_EQ here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  EXPECT_TRUE(dynamic_data == generated_data);
}

TEST(WireFormatTest, ParseMultipleExtensionRanges) {
  // Make sure we can parse a message that contains multiple extensions ranges.
  unittest::TestFieldOrderings source;
  std::string data;

  TestUtil::SetAllFieldsAndExtensions(&source);
  source.SerializeToString(&data);

  {
    unittest::TestFieldOrderings dest;
    EXPECT_TRUE(dest.ParseFromString(data));
    EXPECT_EQ(source.DebugString(), dest.DebugString());
  }

  // Also test using reflection-based parsing.
  {
    unittest::TestFieldOrderings dest;
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream coded_input(&raw_input);
    EXPECT_TRUE(WireFormat::ParseAndMergePartial(&coded_input, &dest));
    EXPECT_EQ(source.DebugString(), dest.DebugString());
  }
}

const int kUnknownTypeId = 1550055;

TEST(WireFormatTest, SerializeMessageSet) {
  // Set up a TestMessageSet with two known messages and an unknown one.
  proto2_wireformat_unittest::TestMessageSet message_set;
  message_set
      .MutableExtension(
          unittest::TestMessageSetExtension1::message_set_extension)
      ->set_i(123);
  message_set
      .MutableExtension(
          unittest::TestMessageSetExtension2::message_set_extension)
      ->set_str("foo");
  message_set.mutable_unknown_fields()->AddLengthDelimited(kUnknownTypeId,
                                                           "bar");

  std::string data;
  ASSERT_TRUE(message_set.SerializeToString(&data));

  // Parse back using RawMessageSet and check the contents.
  unittest::RawMessageSet raw;
  ASSERT_TRUE(raw.ParseFromString(data));

  EXPECT_EQ(0, raw.unknown_fields().field_count());

  ASSERT_EQ(3, raw.item_size());
  EXPECT_EQ(
      unittest::TestMessageSetExtension1::descriptor()->extension(0)->number(),
      raw.item(0).type_id());
  EXPECT_EQ(
      unittest::TestMessageSetExtension2::descriptor()->extension(0)->number(),
      raw.item(1).type_id());
  EXPECT_EQ(kUnknownTypeId, raw.item(2).type_id());

  unittest::TestMessageSetExtension1 message1;
  EXPECT_TRUE(message1.ParseFromString(raw.item(0).message()));
  EXPECT_EQ(123, message1.i());

  unittest::TestMessageSetExtension2 message2;
  EXPECT_TRUE(message2.ParseFromString(raw.item(1).message()));
  EXPECT_EQ("foo", message2.str());

  EXPECT_EQ("bar", raw.item(2).message());
}

TEST(WireFormatTest, SerializeMessageSetVariousWaysAreEqual) {
  // Serialize a MessageSet to a stream and to a flat array using generated
  // code, and also using WireFormat, and check that the results are equal.
  // Set up a TestMessageSet with two known messages and an unknown one, as
  // above.

  proto2_wireformat_unittest::TestMessageSet message_set;
  message_set
      .MutableExtension(
          unittest::TestMessageSetExtension1::message_set_extension)
      ->set_i(123);
  message_set
      .MutableExtension(
          unittest::TestMessageSetExtension2::message_set_extension)
      ->set_str("foo");
  message_set.mutable_unknown_fields()->AddLengthDelimited(kUnknownTypeId,
                                                           "bar");

  int size = message_set.ByteSize();
  EXPECT_EQ(size, message_set.GetCachedSize());
  ASSERT_EQ(size, WireFormat::ByteSize(message_set));

  std::string flat_data;
  std::string stream_data;
  std::string dynamic_data;
  flat_data.resize(size);
  stream_data.resize(size);

  // Serialize to flat array
  {
    uint8* target = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&flat_data));
    uint8* end = message_set.SerializeWithCachedSizesToArray(target);
    EXPECT_EQ(size, end - target);
  }

  // Serialize to buffer
  {
    io::ArrayOutputStream array_stream(::google::protobuf::string_as_array(&stream_data), size,
                                       1);
    io::CodedOutputStream output_stream(&array_stream);
    message_set.SerializeWithCachedSizes(&output_stream);
    ASSERT_FALSE(output_stream.HadError());
  }

  // Serialize to buffer with WireFormat.
  {
    io::StringOutputStream string_stream(&dynamic_data);
    io::CodedOutputStream output_stream(&string_stream);
    WireFormat::SerializeWithCachedSizes(message_set, size, &output_stream);
    ASSERT_FALSE(output_stream.HadError());
  }

  EXPECT_TRUE(flat_data == stream_data);
  EXPECT_TRUE(flat_data == dynamic_data);
}

TEST(WireFormatTest, ParseMessageSet) {
  // Set up a RawMessageSet with two known messages and an unknown one.
  unittest::RawMessageSet raw;

  {
    unittest::RawMessageSet::Item* item = raw.add_item();
    item->set_type_id(unittest::TestMessageSetExtension1::descriptor()
                          ->extension(0)
                          ->number());
    unittest::TestMessageSetExtension1 message;
    message.set_i(123);
    message.SerializeToString(item->mutable_message());
  }

  {
    unittest::RawMessageSet::Item* item = raw.add_item();
    item->set_type_id(unittest::TestMessageSetExtension2::descriptor()
                          ->extension(0)
                          ->number());
    unittest::TestMessageSetExtension2 message;
    message.set_str("foo");
    message.SerializeToString(item->mutable_message());
  }

  {
    unittest::RawMessageSet::Item* item = raw.add_item();
    item->set_type_id(kUnknownTypeId);
    item->set_message("bar");
  }

  std::string data;
  ASSERT_TRUE(raw.SerializeToString(&data));

  // Parse as a TestMessageSet and check the contents.
  proto2_wireformat_unittest::TestMessageSet message_set;
  ASSERT_TRUE(message_set.ParseFromString(data));

  EXPECT_EQ(123,
            message_set
                .GetExtension(
                    unittest::TestMessageSetExtension1::message_set_extension)
                .i());
  EXPECT_EQ("foo",
            message_set
                .GetExtension(
                    unittest::TestMessageSetExtension2::message_set_extension)
                .str());

  ASSERT_EQ(1, message_set.unknown_fields().field_count());
  ASSERT_EQ(UnknownField::TYPE_LENGTH_DELIMITED,
            message_set.unknown_fields().field(0).type());
  EXPECT_EQ("bar", message_set.unknown_fields().field(0).length_delimited());

  // Also parse using WireFormat.
  proto2_wireformat_unittest::TestMessageSet dynamic_message_set;
  io::CodedInputStream input(reinterpret_cast<const uint8*>(data.data()),
                             data.size());
  ASSERT_TRUE(WireFormat::ParseAndMergePartial(&input, &dynamic_message_set));
  EXPECT_EQ(message_set.DebugString(), dynamic_message_set.DebugString());
}

TEST(WireFormatTest, ParseMessageSetWithReverseTagOrder) {
  std::string data;
  {
    unittest::TestMessageSetExtension1 message;
    message.set_i(123);
    // Build a MessageSet manually with its message content put before its
    // type_id.
    io::StringOutputStream output_stream(&data);
    io::CodedOutputStream coded_output(&output_stream);
    coded_output.WriteTag(WireFormatLite::kMessageSetItemStartTag);
    // Write the message content first.
    WireFormatLite::WriteTag(WireFormatLite::kMessageSetMessageNumber,
                             WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                             &coded_output);
    coded_output.WriteVarint32(message.ByteSize());
    message.SerializeWithCachedSizes(&coded_output);
    // Write the type id.
    uint32 type_id = message.GetDescriptor()->extension(0)->number();
    WireFormatLite::WriteUInt32(WireFormatLite::kMessageSetTypeIdNumber,
                                type_id, &coded_output);
    coded_output.WriteTag(WireFormatLite::kMessageSetItemEndTag);
  }
  {
    proto2_wireformat_unittest::TestMessageSet message_set;
    ASSERT_TRUE(message_set.ParseFromString(data));

    EXPECT_EQ(123,
              message_set
                  .GetExtension(
                      unittest::TestMessageSetExtension1::message_set_extension)
                  .i());
  }
  {
    // Test parse the message via Reflection.
    proto2_wireformat_unittest::TestMessageSet message_set;
    io::CodedInputStream input(reinterpret_cast<const uint8*>(data.data()),
                               data.size());
    EXPECT_TRUE(WireFormat::ParseAndMergePartial(&input, &message_set));
    EXPECT_TRUE(input.ConsumedEntireMessage());

    EXPECT_EQ(123,
              message_set
                  .GetExtension(
                      unittest::TestMessageSetExtension1::message_set_extension)
                  .i());
  }
}

void SerializeReverseOrder(
    const proto2_wireformat_unittest::TestMessageSet& mset,
    io::CodedOutputStream* coded_output);

void SerializeReverseOrder(const unittest::TestMessageSetExtension1& message,
                           io::CodedOutputStream* coded_output) {
  WireFormatLite::WriteTag(15,  // i
                           WireFormatLite::WIRETYPE_VARINT, coded_output);
  coded_output->WriteVarint32(message.i());
  WireFormatLite::WriteTag(16,  // recursive
                           WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                           coded_output);
  coded_output->WriteVarint32(message.recursive().GetCachedSize());
  SerializeReverseOrder(message.recursive(), coded_output);
}

void SerializeReverseOrder(
    const proto2_wireformat_unittest::TestMessageSet& mset,
    io::CodedOutputStream* coded_output) {
  if (!mset.HasExtension(
          unittest::TestMessageSetExtension1::message_set_extension))
    return;
  coded_output->WriteTag(WireFormatLite::kMessageSetItemStartTag);
  // Write the message content first.
  WireFormatLite::WriteTag(WireFormatLite::kMessageSetMessageNumber,
                           WireFormatLite::WIRETYPE_LENGTH_DELIMITED,
                           coded_output);
  auto& message = mset.GetExtension(
      unittest::TestMessageSetExtension1::message_set_extension);
  coded_output->WriteVarint32(message.GetCachedSize());
  SerializeReverseOrder(message, coded_output);
  // Write the type id.
  uint32 type_id = message.GetDescriptor()->extension(0)->number();
  WireFormatLite::WriteUInt32(WireFormatLite::kMessageSetTypeIdNumber, type_id,
                              coded_output);
  coded_output->WriteTag(WireFormatLite::kMessageSetItemEndTag);
}

TEST(WireFormatTest, ParseMessageSetWithDeepRecReverseOrder) {
  std::string data;
  {
    proto2_wireformat_unittest::TestMessageSet message_set;
    proto2_wireformat_unittest::TestMessageSet* mset = &message_set;
    for (int i = 0; i < 200; i++) {
      auto m = mset->MutableExtension(
          unittest::TestMessageSetExtension1::message_set_extension);
      m->set_i(i);
      mset = m->mutable_recursive();
    }
    message_set.ByteSizeLong();
    // Serialize with reverse payload tag order
    io::StringOutputStream output_stream(&data);
    io::CodedOutputStream coded_output(&output_stream);
    SerializeReverseOrder(message_set, &coded_output);
  }
  proto2_wireformat_unittest::TestMessageSet message_set;
  EXPECT_FALSE(message_set.ParseFromString(data));
}

TEST(WireFormatTest, ParseBrokenMessageSet) {
  proto2_wireformat_unittest::TestMessageSet message_set;
  std::string input("goodbye");  // Invalid wire format data.
  EXPECT_FALSE(message_set.ParseFromString(input));
}

TEST(WireFormatTest, RecursionLimit) {
  unittest::TestRecursiveMessage message;
  message.mutable_a()->mutable_a()->mutable_a()->mutable_a()->set_i(1);
  std::string data;
  message.SerializeToString(&data);

  {
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream input(&raw_input);
    input.SetRecursionLimit(4);
    unittest::TestRecursiveMessage message2;
    EXPECT_TRUE(message2.ParseFromCodedStream(&input));
  }

  {
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream input(&raw_input);
    input.SetRecursionLimit(3);
    unittest::TestRecursiveMessage message2;
    EXPECT_FALSE(message2.ParseFromCodedStream(&input));
  }
}

TEST(WireFormatTest, UnknownFieldRecursionLimit) {
  unittest::TestEmptyMessage message;
  message.mutable_unknown_fields()
      ->AddGroup(1234)
      ->AddGroup(1234)
      ->AddGroup(1234)
      ->AddGroup(1234)
      ->AddVarint(1234, 123);
  std::string data;
  message.SerializeToString(&data);

  {
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream input(&raw_input);
    input.SetRecursionLimit(4);
    unittest::TestEmptyMessage message2;
    EXPECT_TRUE(message2.ParseFromCodedStream(&input));
  }

  {
    io::ArrayInputStream raw_input(data.data(), data.size());
    io::CodedInputStream input(&raw_input);
    input.SetRecursionLimit(3);
    unittest::TestEmptyMessage message2;
    EXPECT_FALSE(message2.ParseFromCodedStream(&input));
  }
}

TEST(WireFormatTest, ZigZag) {
// avoid line-wrapping
#define LL(x) PROTOBUF_LONGLONG(x)
#define ULL(x) PROTOBUF_ULONGLONG(x)
#define ZigZagEncode32(x) WireFormatLite::ZigZagEncode32(x)
#define ZigZagDecode32(x) WireFormatLite::ZigZagDecode32(x)
#define ZigZagEncode64(x) WireFormatLite::ZigZagEncode64(x)
#define ZigZagDecode64(x) WireFormatLite::ZigZagDecode64(x)

  EXPECT_EQ(0u, ZigZagEncode32(0));
  EXPECT_EQ(1u, ZigZagEncode32(-1));
  EXPECT_EQ(2u, ZigZagEncode32(1));
  EXPECT_EQ(3u, ZigZagEncode32(-2));
  EXPECT_EQ(0x7FFFFFFEu, ZigZagEncode32(0x3FFFFFFF));
  EXPECT_EQ(0x7FFFFFFFu, ZigZagEncode32(0xC0000000));
  EXPECT_EQ(0xFFFFFFFEu, ZigZagEncode32(0x7FFFFFFF));
  EXPECT_EQ(0xFFFFFFFFu, ZigZagEncode32(0x80000000));

  EXPECT_EQ(0, ZigZagDecode32(0u));
  EXPECT_EQ(-1, ZigZagDecode32(1u));
  EXPECT_EQ(1, ZigZagDecode32(2u));
  EXPECT_EQ(-2, ZigZagDecode32(3u));
  EXPECT_EQ(0x3FFFFFFF, ZigZagDecode32(0x7FFFFFFEu));
  EXPECT_EQ(0xC0000000, ZigZagDecode32(0x7FFFFFFFu));
  EXPECT_EQ(0x7FFFFFFF, ZigZagDecode32(0xFFFFFFFEu));
  EXPECT_EQ(0x80000000, ZigZagDecode32(0xFFFFFFFFu));

  EXPECT_EQ(0u, ZigZagEncode64(0));
  EXPECT_EQ(1u, ZigZagEncode64(-1));
  EXPECT_EQ(2u, ZigZagEncode64(1));
  EXPECT_EQ(3u, ZigZagEncode64(-2));
  EXPECT_EQ(ULL(0x000000007FFFFFFE), ZigZagEncode64(LL(0x000000003FFFFFFF)));
  EXPECT_EQ(ULL(0x000000007FFFFFFF), ZigZagEncode64(LL(0xFFFFFFFFC0000000)));
  EXPECT_EQ(ULL(0x00000000FFFFFFFE), ZigZagEncode64(LL(0x000000007FFFFFFF)));
  EXPECT_EQ(ULL(0x00000000FFFFFFFF), ZigZagEncode64(LL(0xFFFFFFFF80000000)));
  EXPECT_EQ(ULL(0xFFFFFFFFFFFFFFFE), ZigZagEncode64(LL(0x7FFFFFFFFFFFFFFF)));
  EXPECT_EQ(ULL(0xFFFFFFFFFFFFFFFF), ZigZagEncode64(LL(0x8000000000000000)));

  EXPECT_EQ(0, ZigZagDecode64(0u));
  EXPECT_EQ(-1, ZigZagDecode64(1u));
  EXPECT_EQ(1, ZigZagDecode64(2u));
  EXPECT_EQ(-2, ZigZagDecode64(3u));
  EXPECT_EQ(LL(0x000000003FFFFFFF), ZigZagDecode64(ULL(0x000000007FFFFFFE)));
  EXPECT_EQ(LL(0xFFFFFFFFC0000000), ZigZagDecode64(ULL(0x000000007FFFFFFF)));
  EXPECT_EQ(LL(0x000000007FFFFFFF), ZigZagDecode64(ULL(0x00000000FFFFFFFE)));
  EXPECT_EQ(LL(0xFFFFFFFF80000000), ZigZagDecode64(ULL(0x00000000FFFFFFFF)));
  EXPECT_EQ(LL(0x7FFFFFFFFFFFFFFF), ZigZagDecode64(ULL(0xFFFFFFFFFFFFFFFE)));
  EXPECT_EQ(LL(0x8000000000000000), ZigZagDecode64(ULL(0xFFFFFFFFFFFFFFFF)));

  // Some easier-to-verify round-trip tests.  The inputs (other than 0, 1, -1)
  // were chosen semi-randomly via keyboard bashing.
  EXPECT_EQ(0, ZigZagDecode32(ZigZagEncode32(0)));
  EXPECT_EQ(1, ZigZagDecode32(ZigZagEncode32(1)));
  EXPECT_EQ(-1, ZigZagDecode32(ZigZagEncode32(-1)));
  EXPECT_EQ(14927, ZigZagDecode32(ZigZagEncode32(14927)));
  EXPECT_EQ(-3612, ZigZagDecode32(ZigZagEncode32(-3612)));

  EXPECT_EQ(0, ZigZagDecode64(ZigZagEncode64(0)));
  EXPECT_EQ(1, ZigZagDecode64(ZigZagEncode64(1)));
  EXPECT_EQ(-1, ZigZagDecode64(ZigZagEncode64(-1)));
  EXPECT_EQ(14927, ZigZagDecode64(ZigZagEncode64(14927)));
  EXPECT_EQ(-3612, ZigZagDecode64(ZigZagEncode64(-3612)));

  EXPECT_EQ(LL(856912304801416),
            ZigZagDecode64(ZigZagEncode64(LL(856912304801416))));
  EXPECT_EQ(LL(-75123905439571256),
            ZigZagDecode64(ZigZagEncode64(LL(-75123905439571256))));
}

TEST(WireFormatTest, RepeatedScalarsDifferentTagSizes) {
  // At one point checks would trigger when parsing repeated fixed scalar
  // fields.
  protobuf_unittest::TestRepeatedScalarDifferentTagSizes msg1, msg2;
  for (int i = 0; i < 100; ++i) {
    msg1.add_repeated_fixed32(i);
    msg1.add_repeated_int32(i);
    msg1.add_repeated_fixed64(i);
    msg1.add_repeated_int64(i);
    msg1.add_repeated_float(i);
    msg1.add_repeated_uint64(i);
  }

  // Make sure that we have a variety of tag sizes.
  const Descriptor* desc = msg1.GetDescriptor();
  const FieldDescriptor* field;
  field = desc->FindFieldByName("repeated_fixed32");
  ASSERT_TRUE(field != NULL);
  ASSERT_EQ(1, WireFormat::TagSize(field->number(), field->type()));
  field = desc->FindFieldByName("repeated_int32");
  ASSERT_TRUE(field != NULL);
  ASSERT_EQ(1, WireFormat::TagSize(field->number(), field->type()));
  field = desc->FindFieldByName("repeated_fixed64");
  ASSERT_TRUE(field != NULL);
  ASSERT_EQ(2, WireFormat::TagSize(field->number(), field->type()));
  field = desc->FindFieldByName("repeated_int64");
  ASSERT_TRUE(field != NULL);
  ASSERT_EQ(2, WireFormat::TagSize(field->number(), field->type()));
  field = desc->FindFieldByName("repeated_float");
  ASSERT_TRUE(field != NULL);
  ASSERT_EQ(3, WireFormat::TagSize(field->number(), field->type()));
  field = desc->FindFieldByName("repeated_uint64");
  ASSERT_TRUE(field != NULL);
  ASSERT_EQ(3, WireFormat::TagSize(field->number(), field->type()));

  EXPECT_TRUE(msg2.ParseFromString(msg1.SerializeAsString()));
  EXPECT_EQ(msg1.DebugString(), msg2.DebugString());
}

TEST(WireFormatTest, CompatibleTypes) {
  const int64 data = 0x100000000LL;
  unittest::Int64Message msg1;
  msg1.set_data(data);
  std::string serialized;
  msg1.SerializeToString(&serialized);

  // Test int64 is compatible with bool
  unittest::BoolMessage msg2;
  ASSERT_TRUE(msg2.ParseFromString(serialized));
  ASSERT_EQ(static_cast<bool>(data), msg2.data());

  // Test int64 is compatible with uint64
  unittest::Uint64Message msg3;
  ASSERT_TRUE(msg3.ParseFromString(serialized));
  ASSERT_EQ(static_cast<uint64>(data), msg3.data());

  // Test int64 is compatible with int32
  unittest::Int32Message msg4;
  ASSERT_TRUE(msg4.ParseFromString(serialized));
  ASSERT_EQ(static_cast<int32>(data), msg4.data());

  // Test int64 is compatible with uint32
  unittest::Uint32Message msg5;
  ASSERT_TRUE(msg5.ParseFromString(serialized));
  ASSERT_EQ(static_cast<uint32>(data), msg5.data());
}

class Proto3PrimitiveRepeatedWireFormatTest : public ::testing::Test {
 protected:
  Proto3PrimitiveRepeatedWireFormatTest()
      : packedTestAllTypes_(
            "\xFA\x01\x01\x01"
            "\x82\x02\x01\x01"
            "\x8A\x02\x01\x01"
            "\x92\x02\x01\x01"
            "\x9A\x02\x01\x02"
            "\xA2\x02\x01\x02"
            "\xAA\x02\x04\x01\x00\x00\x00"
            "\xB2\x02\x08\x01\x00\x00\x00\x00\x00\x00\x00"
            "\xBA\x02\x04\x01\x00\x00\x00"
            "\xC2\x02\x08\x01\x00\x00\x00\x00\x00\x00\x00"
            "\xCA\x02\x04\x00\x00\x80\x3f"
            "\xD2\x02\x08\x00\x00\x00\x00\x00\x00\xf0\x3f"
            "\xDA\x02\x01\x01"
            "\x9A\x03\x01\x01",
            86),
        packedTestUnpackedTypes_(
            "\x0A\x01\x01"
            "\x12\x01\x01"
            "\x1A\x01\x01"
            "\x22\x01\x01"
            "\x2A\x01\x02"
            "\x32\x01\x02"
            "\x3A\x04\x01\x00\x00\x00"
            "\x42\x08\x01\x00\x00\x00\x00\x00\x00\x00"
            "\x4A\x04\x01\x00\x00\x00"
            "\x52\x08\x01\x00\x00\x00\x00\x00\x00\x00"
            "\x5A\x04\x00\x00\x80\x3f"
            "\x62\x08\x00\x00\x00\x00\x00\x00\xf0\x3f"
            "\x6A\x01\x01"
            "\x72\x01\x01",
            72),
        unpackedTestAllTypes_(
            "\xF8\x01\x01"
            "\x80\x02\x01"
            "\x88\x02\x01"
            "\x90\x02\x01"
            "\x98\x02\x02"
            "\xA0\x02\x02"
            "\xAD\x02\x01\x00\x00\x00"
            "\xB1\x02\x01\x00\x00\x00\x00\x00\x00\x00"
            "\xBD\x02\x01\x00\x00\x00"
            "\xC1\x02\x01\x00\x00\x00\x00\x00\x00\x00"
            "\xCD\x02\x00\x00\x80\x3f"
            "\xD1\x02\x00\x00\x00\x00\x00\x00\xf0\x3f"
            "\xD8\x02\x01"
            "\x98\x03\x01",
            72),
        unpackedTestUnpackedTypes_(
            "\x08\x01"
            "\x10\x01"
            "\x18\x01"
            "\x20\x01"
            "\x28\x02"
            "\x30\x02"
            "\x3D\x01\x00\x00\x00"
            "\x41\x01\x00\x00\x00\x00\x00\x00\x00"
            "\x4D\x01\x00\x00\x00"
            "\x51\x01\x00\x00\x00\x00\x00\x00\x00"
            "\x5D\x00\x00\x80\x3f"
            "\x61\x00\x00\x00\x00\x00\x00\xf0\x3f"
            "\x68\x01"
            "\x70\x01",
            58) {}
  template <class Proto>
  void SetProto3PrimitiveRepeatedFields(Proto* message) {
    message->add_repeated_int32(1);
    message->add_repeated_int64(1);
    message->add_repeated_uint32(1);
    message->add_repeated_uint64(1);
    message->add_repeated_sint32(1);
    message->add_repeated_sint64(1);
    message->add_repeated_fixed32(1);
    message->add_repeated_fixed64(1);
    message->add_repeated_sfixed32(1);
    message->add_repeated_sfixed64(1);
    message->add_repeated_float(1.0);
    message->add_repeated_double(1.0);
    message->add_repeated_bool(true);
    message->add_repeated_nested_enum(
        proto3_arena_unittest::TestAllTypes_NestedEnum_FOO);
  }

  template <class Proto>
  void ExpectProto3PrimitiveRepeatedFieldsSet(const Proto& message) {
    EXPECT_EQ(1, message.repeated_int32(0));
    EXPECT_EQ(1, message.repeated_int64(0));
    EXPECT_EQ(1, message.repeated_uint32(0));
    EXPECT_EQ(1, message.repeated_uint64(0));
    EXPECT_EQ(1, message.repeated_sint32(0));
    EXPECT_EQ(1, message.repeated_sint64(0));
    EXPECT_EQ(1, message.repeated_fixed32(0));
    EXPECT_EQ(1, message.repeated_fixed64(0));
    EXPECT_EQ(1, message.repeated_sfixed32(0));
    EXPECT_EQ(1, message.repeated_sfixed64(0));
    EXPECT_EQ(1.0, message.repeated_float(0));
    EXPECT_EQ(1.0, message.repeated_double(0));
    EXPECT_EQ(true, message.repeated_bool(0));
    EXPECT_EQ(proto3_arena_unittest::TestAllTypes_NestedEnum_FOO,
              message.repeated_nested_enum(0));
  }

  template <class Proto>
  void TestSerialization(Proto* message, const std::string& expected) {
    SetProto3PrimitiveRepeatedFields(message);

    int size = message->ByteSize();

    // Serialize using the generated code.
    std::string generated_data;
    {
      io::StringOutputStream raw_output(&generated_data);
      io::CodedOutputStream output(&raw_output);
      message->SerializeWithCachedSizes(&output);
      ASSERT_FALSE(output.HadError());
    }
    EXPECT_TRUE(expected == generated_data);

    // Serialize using the dynamic code.
    std::string dynamic_data;
    {
      io::StringOutputStream raw_output(&dynamic_data);
      io::CodedOutputStream output(&raw_output);
      WireFormat::SerializeWithCachedSizes(*message, size, &output);
      ASSERT_FALSE(output.HadError());
    }
    EXPECT_TRUE(expected == dynamic_data);
  }

  template <class Proto>
  void TestParsing(Proto* message, const std::string& compatible_data) {
    message->Clear();
    message->ParseFromString(compatible_data);
    ExpectProto3PrimitiveRepeatedFieldsSet(*message);

    message->Clear();
    io::CodedInputStream input(
        reinterpret_cast<const uint8*>(compatible_data.data()),
        compatible_data.size());
    WireFormat::ParseAndMergePartial(&input, message);
    ExpectProto3PrimitiveRepeatedFieldsSet(*message);
  }

  const std::string packedTestAllTypes_;
  const std::string packedTestUnpackedTypes_;
  const std::string unpackedTestAllTypes_;
  const std::string unpackedTestUnpackedTypes_;
};

TEST_F(Proto3PrimitiveRepeatedWireFormatTest, Proto3PrimitiveRepeated) {
  proto3_arena_unittest::TestAllTypes packed_message;
  proto3_arena_unittest::TestUnpackedTypes unpacked_message;
  TestSerialization(&packed_message, packedTestAllTypes_);
  TestParsing(&packed_message, packedTestAllTypes_);
  TestParsing(&packed_message, unpackedTestAllTypes_);
  TestSerialization(&unpacked_message, unpackedTestUnpackedTypes_);
  TestParsing(&unpacked_message, packedTestUnpackedTypes_);
  TestParsing(&unpacked_message, unpackedTestUnpackedTypes_);
}

class WireFormatInvalidInputTest : public testing::Test {
 protected:
  // Make a serialized TestAllTypes in which the field optional_nested_message
  // contains exactly the given bytes, which may be invalid.
  std::string MakeInvalidEmbeddedMessage(const char* bytes, int size) {
    const FieldDescriptor* field =
        unittest::TestAllTypes::descriptor()->FindFieldByName(
            "optional_nested_message");
    GOOGLE_CHECK(field != NULL);

    std::string result;

    {
      io::StringOutputStream raw_output(&result);
      io::CodedOutputStream output(&raw_output);

      WireFormatLite::WriteBytes(field->number(), std::string(bytes, size),
                                 &output);
    }

    return result;
  }

  // Make a serialized TestAllTypes in which the field optionalgroup
  // contains exactly the given bytes -- which may be invalid -- and
  // possibly no end tag.
  std::string MakeInvalidGroup(const char* bytes, int size,
                               bool include_end_tag) {
    const FieldDescriptor* field =
        unittest::TestAllTypes::descriptor()->FindFieldByName("optionalgroup");
    GOOGLE_CHECK(field != NULL);

    std::string result;

    {
      io::StringOutputStream raw_output(&result);
      io::CodedOutputStream output(&raw_output);

      output.WriteVarint32(WireFormat::MakeTag(field));
      output.WriteString(std::string(bytes, size));
      if (include_end_tag) {
        output.WriteVarint32(WireFormatLite::MakeTag(
            field->number(), WireFormatLite::WIRETYPE_END_GROUP));
      }
    }

    return result;
  }
};

TEST_F(WireFormatInvalidInputTest, InvalidSubMessage) {
  unittest::TestAllTypes message;

  // Control case.
  EXPECT_TRUE(message.ParseFromString(MakeInvalidEmbeddedMessage("", 0)));

  // The byte is a valid varint, but not a valid tag (zero).
  EXPECT_FALSE(message.ParseFromString(MakeInvalidEmbeddedMessage("\0", 1)));

  // The byte is a malformed varint.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidEmbeddedMessage("\200", 1)));

  // The byte is an endgroup tag, but we aren't parsing a group.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidEmbeddedMessage("\014", 1)));

  // The byte is a valid varint but not a valid tag (bad wire type).
  EXPECT_FALSE(message.ParseFromString(MakeInvalidEmbeddedMessage("\017", 1)));
}

TEST_F(WireFormatInvalidInputTest, InvalidMessageWithExtraZero) {
  std::string data;
  {
    // Serialize a valid proto
    unittest::TestAllTypes message;
    message.set_optional_int32(1);
    message.SerializeToString(&data);
    data.push_back(0);  // Append invalid zero tag
  }

  // Control case.
  {
    io::ArrayInputStream ais(data.data(), data.size());
    io::CodedInputStream is(&ais);
    unittest::TestAllTypes message;
    // It should fail but currently passes.
    EXPECT_TRUE(message.MergePartialFromCodedStream(&is));
    // Parsing from the string should fail.
    EXPECT_FALSE(message.ParseFromString(data));
  }
}

TEST_F(WireFormatInvalidInputTest, InvalidGroup) {
  unittest::TestAllTypes message;

  // Control case.
  EXPECT_TRUE(message.ParseFromString(MakeInvalidGroup("", 0, true)));

  // Missing end tag.  Groups cannot end at EOF.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("", 0, false)));

  // The byte is a valid varint, but not a valid tag (zero).
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\0", 1, false)));

  // The byte is a malformed varint.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\200", 1, false)));

  // The byte is an endgroup tag, but not the right one for this group.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\014", 1, false)));

  // The byte is a valid varint but not a valid tag (bad wire type).
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\017", 1, true)));
}

TEST_F(WireFormatInvalidInputTest, InvalidUnknownGroup) {
  // Use TestEmptyMessage so that the group made by MakeInvalidGroup will not
  // be a known tag number.
  unittest::TestEmptyMessage message;

  // Control case.
  EXPECT_TRUE(message.ParseFromString(MakeInvalidGroup("", 0, true)));

  // Missing end tag.  Groups cannot end at EOF.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("", 0, false)));

  // The byte is a valid varint, but not a valid tag (zero).
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\0", 1, false)));

  // The byte is a malformed varint.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\200", 1, false)));

  // The byte is an endgroup tag, but not the right one for this group.
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\014", 1, false)));

  // The byte is a valid varint but not a valid tag (bad wire type).
  EXPECT_FALSE(message.ParseFromString(MakeInvalidGroup("\017", 1, true)));
}

TEST_F(WireFormatInvalidInputTest, InvalidStringInUnknownGroup) {
  // Test a bug fix:  SkipMessage should fail if the message contains a
  // string whose length would extend beyond the message end.

  unittest::TestAllTypes message;
  message.set_optional_string("foo foo foo foo");
  std::string data;
  message.SerializeToString(&data);

  // Chop some bytes off the end.
  data.resize(data.size() - 4);

  // Try to skip it.  Note that the bug was only present when parsing to an
  // UnknownFieldSet.
  io::ArrayInputStream raw_input(data.data(), data.size());
  io::CodedInputStream coded_input(&raw_input);
  UnknownFieldSet unknown_fields;
  EXPECT_FALSE(WireFormat::SkipMessage(&coded_input, &unknown_fields));
}

// Test differences between string and bytes.
// Value of a string type must be valid UTF-8 string.  When UTF-8
// validation is enabled (GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED):
// WriteInvalidUTF8String:  see error message.
// ReadInvalidUTF8String:  see error message.
// WriteValidUTF8String: fine.
// ReadValidUTF8String:  fine.
// WriteAnyBytes: fine.
// ReadAnyBytes: fine.
const char* kInvalidUTF8String = "Invalid UTF-8: \xA0\xB0\xC0\xD0";
// This used to be "Valid UTF-8: \x01\x02\u8C37\u6B4C", but MSVC seems to
// interpret \u differently from GCC.
const char* kValidUTF8String = "Valid UTF-8: \x01\x02\350\260\267\346\255\214";

template <typename T>
bool WriteMessage(const char* value, T* message, std::string* wire_buffer) {
  message->set_data(value);
  wire_buffer->clear();
  message->AppendToString(wire_buffer);
  return (wire_buffer->size() > 0);
}

template <typename T>
bool ReadMessage(const std::string& wire_buffer, T* message) {
  return message->ParseFromArray(wire_buffer.data(), wire_buffer.size());
}

class Utf8ValidationTest : public ::testing::Test {
 protected:
  Utf8ValidationTest() {}
  virtual ~Utf8ValidationTest() {}
  virtual void SetUp() {
  }

};

TEST_F(Utf8ValidationTest, WriteInvalidUTF8String) {
  std::string wire_buffer;
  protobuf_unittest::OneString input;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    WriteMessage(kInvalidUTF8String, &input, &wire_buffer);
    errors = log.GetMessages(ERROR);
  }
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  ASSERT_EQ(1, errors.size());
  EXPECT_TRUE(
      HasPrefixString(errors[0],
                       "String field 'protobuf_unittest.OneString.data' "
                       "contains invalid UTF-8 data when "
                       "serializing a protocol buffer. Use the "
                       "'bytes' type if you intend to send raw bytes."));
#else
  ASSERT_EQ(0, errors.size());
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
}


TEST_F(Utf8ValidationTest, ReadInvalidUTF8String) {
  std::string wire_buffer;
  protobuf_unittest::OneString input;
  WriteMessage(kInvalidUTF8String, &input, &wire_buffer);
  protobuf_unittest::OneString output;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    ReadMessage(wire_buffer, &output);
    errors = log.GetMessages(ERROR);
  }
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  ASSERT_EQ(1, errors.size());
  EXPECT_TRUE(
      HasPrefixString(errors[0],
                       "String field 'protobuf_unittest.OneString.data' "
                       "contains invalid UTF-8 data when "
                       "parsing a protocol buffer. Use the "
                       "'bytes' type if you intend to send raw bytes."));

#else
  ASSERT_EQ(0, errors.size());
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
}


TEST_F(Utf8ValidationTest, WriteValidUTF8String) {
  std::string wire_buffer;
  protobuf_unittest::OneString input;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    WriteMessage(kValidUTF8String, &input, &wire_buffer);
    errors = log.GetMessages(ERROR);
  }
  ASSERT_EQ(0, errors.size());
}

TEST_F(Utf8ValidationTest, ReadValidUTF8String) {
  std::string wire_buffer;
  protobuf_unittest::OneString input;
  WriteMessage(kValidUTF8String, &input, &wire_buffer);
  protobuf_unittest::OneString output;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    ReadMessage(wire_buffer, &output);
    errors = log.GetMessages(ERROR);
  }
  ASSERT_EQ(0, errors.size());
  EXPECT_EQ(input.data(), output.data());
}

// Bytes: anything can pass as bytes, use invalid UTF-8 string to test
TEST_F(Utf8ValidationTest, WriteArbitraryBytes) {
  std::string wire_buffer;
  protobuf_unittest::OneBytes input;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    WriteMessage(kInvalidUTF8String, &input, &wire_buffer);
    errors = log.GetMessages(ERROR);
  }
  ASSERT_EQ(0, errors.size());
}

TEST_F(Utf8ValidationTest, ReadArbitraryBytes) {
  std::string wire_buffer;
  protobuf_unittest::OneBytes input;
  WriteMessage(kInvalidUTF8String, &input, &wire_buffer);
  protobuf_unittest::OneBytes output;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    ReadMessage(wire_buffer, &output);
    errors = log.GetMessages(ERROR);
  }
  ASSERT_EQ(0, errors.size());
  EXPECT_EQ(input.data(), output.data());
}

TEST_F(Utf8ValidationTest, ParseRepeatedString) {
  protobuf_unittest::MoreBytes input;
  input.add_data(kValidUTF8String);
  input.add_data(kInvalidUTF8String);
  input.add_data(kInvalidUTF8String);
  std::string wire_buffer = input.SerializeAsString();

  protobuf_unittest::MoreString output;
  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    ReadMessage(wire_buffer, &output);
    errors = log.GetMessages(ERROR);
  }
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  ASSERT_EQ(2, errors.size());
#else
  ASSERT_EQ(0, errors.size());
#endif  // GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  EXPECT_EQ(wire_buffer, output.SerializeAsString());
}

// Test the old VerifyUTF8String() function, which may still be called by old
// generated code.
TEST_F(Utf8ValidationTest, OldVerifyUTF8String) {
  std::string data(kInvalidUTF8String);

  std::vector<std::string> errors;
  {
    ScopedMemoryLog log;
    WireFormat::VerifyUTF8String(data.data(), data.size(),
                                 WireFormat::SERIALIZE);
    errors = log.GetMessages(ERROR);
  }
#ifdef GOOGLE_PROTOBUF_UTF8_VALIDATION_ENABLED
  ASSERT_EQ(1, errors.size());
  EXPECT_TRUE(
      HasPrefixString(errors[0],
                       "String field contains invalid UTF-8 data when "
                       "serializing a protocol buffer. Use the "
                       "'bytes' type if you intend to send raw bytes."));
#else
  ASSERT_EQ(0, errors.size());
#endif
}


TEST(RepeatedVarint, Int32) {
  RepeatedField<int32> v;

  // Insert -2^n, 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(-(1 << n));
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar Int32Size.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::Int32Size(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::Int32Size(v));
}

TEST(RepeatedVarint, Int64) {
  RepeatedField<int64> v;

  // Insert -2^n, 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(-(1 << n));
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar Int64Size.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::Int64Size(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::Int64Size(v));
}

TEST(RepeatedVarint, SInt32) {
  RepeatedField<int32> v;

  // Insert -2^n, 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(-(1 << n));
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar SInt32Size.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::SInt32Size(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::SInt32Size(v));
}

TEST(RepeatedVarint, SInt64) {
  RepeatedField<int64> v;

  // Insert -2^n, 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(-(1 << n));
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar SInt64Size.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::SInt64Size(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::SInt64Size(v));
}

TEST(RepeatedVarint, UInt32) {
  RepeatedField<uint32> v;

  // Insert 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar UInt32Size.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::UInt32Size(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::UInt32Size(v));
}

TEST(RepeatedVarint, UInt64) {
  RepeatedField<uint64> v;

  // Insert 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar UInt64Size.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::UInt64Size(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::UInt64Size(v));
}

TEST(RepeatedVarint, Enum) {
  RepeatedField<int> v;

  // Insert 2^n and 2^n-1.
  for (int n = 0; n < 10; n++) {
    v.Add(1 << n);
    v.Add((1 << n) - 1);
  }

  // Check consistency with the scalar EnumSize.
  size_t expected = 0;
  for (int i = 0; i < v.size(); i++) {
    expected += WireFormatLite::EnumSize(v[i]);
  }

  EXPECT_EQ(expected, WireFormatLite::EnumSize(v));
}


}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
