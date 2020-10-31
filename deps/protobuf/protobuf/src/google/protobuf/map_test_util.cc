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

#include <google/protobuf/map_test_util.h>
#include <google/protobuf/map_test_util_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

namespace google {
namespace protobuf {

void MapTestUtil::SetMapFields(unittest::TestMap* message) {
  MapTestUtilImpl::SetMapFields<unittest::MapEnum, unittest::MAP_ENUM_BAR,
                                unittest::MAP_ENUM_BAZ>(message);
}

void MapTestUtil::SetArenaMapFields(unittest::TestArenaMap* message) {
  MapTestUtilImpl::SetArenaMapFields<unittest::MapEnum, unittest::MAP_ENUM_BAR,
                                     unittest::MAP_ENUM_BAZ>(message);
}

void MapTestUtil::SetMapFieldsInitialized(unittest::TestMap* message) {
  MapTestUtilImpl::SetMapFieldsInitialized(message);
}

void MapTestUtil::ModifyMapFields(unittest::TestMap* message) {
  MapTestUtilImpl::ModifyMapFields<unittest::MapEnum, unittest::MAP_ENUM_FOO>(
      message);
}

void MapTestUtil::ExpectClear(const unittest::TestMap& message) {
  MapTestUtilImpl::ExpectClear(message);
}

void MapTestUtil::ExpectMapFieldsSet(const unittest::TestMap& message) {
  MapTestUtilImpl::ExpectMapFieldsSet<unittest::MapEnum, unittest::MAP_ENUM_BAR,
                                      unittest::MAP_ENUM_BAZ>(message);
}

void MapTestUtil::ExpectArenaMapFieldsSet(
    const unittest::TestArenaMap& message) {
  MapTestUtilImpl::ExpectArenaMapFieldsSet<
      unittest::MapEnum, unittest::MAP_ENUM_BAR, unittest::MAP_ENUM_BAZ>(
      message);
}

void MapTestUtil::ExpectMapFieldsSetInitialized(
    const unittest::TestMap& message) {
  MapTestUtilImpl::ExpectMapFieldsSetInitialized<unittest::MapEnum,
                                                 unittest::MAP_ENUM_FOO>(
      message);
}

void MapTestUtil::ExpectMapFieldsModified(const unittest::TestMap& message) {
  MapTestUtilImpl::ExpectMapFieldsModified<
      unittest::MapEnum, unittest::MAP_ENUM_BAR, unittest::MAP_ENUM_FOO>(
      message);
}

void MapTestUtil::ExpectMapsSize(const unittest::TestMap& message, int size) {
  const Descriptor* descriptor = message.GetDescriptor();

  EXPECT_EQ(size, message.GetReflection()->FieldSize(
                      message, descriptor->FindFieldByName("map_int32_int32")));
  EXPECT_EQ(size, message.GetReflection()->FieldSize(
                      message, descriptor->FindFieldByName("map_int64_int64")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_uint32_uint32")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_uint64_uint64")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_sint32_sint32")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_sint64_sint64")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_fixed32_fixed32")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_fixed64_fixed64")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_sfixed32_sfixed32")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_sfixed64_sfixed64")));
  EXPECT_EQ(size, message.GetReflection()->FieldSize(
                      message, descriptor->FindFieldByName("map_int32_float")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_int32_double")));
  EXPECT_EQ(size, message.GetReflection()->FieldSize(
                      message, descriptor->FindFieldByName("map_bool_bool")));
  EXPECT_EQ(size,
            message.GetReflection()->FieldSize(
                message, descriptor->FindFieldByName("map_string_string")));
  EXPECT_EQ(size, message.GetReflection()->FieldSize(
                      message, descriptor->FindFieldByName("map_int32_bytes")));
  EXPECT_EQ(
      size,
      message.GetReflection()->FieldSize(
          message, descriptor->FindFieldByName("map_int32_foreign_message")));
}

std::vector<const Message*> MapTestUtil::GetMapEntries(
    const unittest::TestMap& message, int index) {
  const Descriptor* descriptor = message.GetDescriptor();
  std::vector<const Message*> result;

  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int32_int32"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int64_int64"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_uint32_uint32"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_uint64_uint64"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_sint32_sint32"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_sint64_sint64"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_fixed32_fixed32"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_fixed64_fixed64"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_sfixed32_sfixed32"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_sfixed64_sfixed64"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int32_float"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int32_double"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_bool_bool"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_string_string"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int32_bytes"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int32_enum"), index));
  result.push_back(&message.GetReflection()->GetRepeatedMessage(
      message, descriptor->FindFieldByName("map_int32_foreign_message"),
      index));

  return result;
}

std::vector<const Message*> MapTestUtil::GetMapEntriesFromRelease(
    unittest::TestMap* message) {
  const Descriptor* descriptor = message->GetDescriptor();
  std::vector<const Message*> result;

  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int32_int32")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int64_int64")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_uint32_uint32")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_uint64_uint64")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_sint32_sint32")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_sint64_sint64")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_fixed32_fixed32")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_fixed64_fixed64")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_sfixed32_sfixed32")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_sfixed64_sfixed64")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int32_float")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int32_double")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_bool_bool")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_string_string")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int32_bytes")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int32_enum")));
  result.push_back(message->GetReflection()->ReleaseLast(
      message, descriptor->FindFieldByName("map_int32_foreign_message")));

  return result;
}

MapReflectionTester::MapReflectionTester(const Descriptor* base_descriptor)
    : base_descriptor_(base_descriptor) {
  const DescriptorPool* pool = base_descriptor->file()->pool();

  map_enum_foo_ = pool->FindEnumValueByName("protobuf_unittest.MAP_ENUM_FOO");
  map_enum_bar_ = pool->FindEnumValueByName("protobuf_unittest.MAP_ENUM_BAR");
  map_enum_baz_ = pool->FindEnumValueByName("protobuf_unittest.MAP_ENUM_BAZ");

  foreign_c_ = pool->FindFieldByName("protobuf_unittest.ForeignMessage.c");
  map_int32_int32_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32Int32Entry.key");
  map_int32_int32_val_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32Int32Entry.value");
  map_int64_int64_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt64Int64Entry.key");
  map_int64_int64_val_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt64Int64Entry.value");
  map_uint32_uint32_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapUint32Uint32Entry.key");
  map_uint32_uint32_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapUint32Uint32Entry.value");
  map_uint64_uint64_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapUint64Uint64Entry.key");
  map_uint64_uint64_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapUint64Uint64Entry.value");
  map_sint32_sint32_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapSint32Sint32Entry.key");
  map_sint32_sint32_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapSint32Sint32Entry.value");
  map_sint64_sint64_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapSint64Sint64Entry.key");
  map_sint64_sint64_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapSint64Sint64Entry.value");
  map_fixed32_fixed32_key_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapFixed32Fixed32Entry.key");
  map_fixed32_fixed32_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapFixed32Fixed32Entry.value");
  map_fixed64_fixed64_key_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapFixed64Fixed64Entry.key");
  map_fixed64_fixed64_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapFixed64Fixed64Entry.value");
  map_sfixed32_sfixed32_key_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapSfixed32Sfixed32Entry.key");
  map_sfixed32_sfixed32_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapSfixed32Sfixed32Entry.value");
  map_sfixed64_sfixed64_key_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapSfixed64Sfixed64Entry.key");
  map_sfixed64_sfixed64_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapSfixed64Sfixed64Entry.value");
  map_int32_float_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32FloatEntry.key");
  map_int32_float_val_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32FloatEntry.value");
  map_int32_double_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32DoubleEntry.key");
  map_int32_double_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapInt32DoubleEntry.value");
  map_bool_bool_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapBoolBoolEntry.key");
  map_bool_bool_val_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapBoolBoolEntry.value");
  map_string_string_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapStringStringEntry.key");
  map_string_string_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapStringStringEntry.value");
  map_int32_bytes_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32BytesEntry.key");
  map_int32_bytes_val_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32BytesEntry.value");
  map_int32_enum_key_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32EnumEntry.key");
  map_int32_enum_val_ =
      pool->FindFieldByName("protobuf_unittest.TestMap.MapInt32EnumEntry.value");
  map_int32_foreign_message_key_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapInt32ForeignMessageEntry.key");
  map_int32_foreign_message_val_ = pool->FindFieldByName(
      "protobuf_unittest.TestMap.MapInt32ForeignMessageEntry.value");

  EXPECT_FALSE(map_enum_foo_ == NULL);
  EXPECT_FALSE(map_enum_bar_ == NULL);
  EXPECT_FALSE(map_enum_baz_ == NULL);
  EXPECT_FALSE(map_int32_int32_key_ == NULL);
  EXPECT_FALSE(map_int32_int32_val_ == NULL);
  EXPECT_FALSE(map_int64_int64_key_ == NULL);
  EXPECT_FALSE(map_int64_int64_val_ == NULL);
  EXPECT_FALSE(map_uint32_uint32_key_ == NULL);
  EXPECT_FALSE(map_uint32_uint32_val_ == NULL);
  EXPECT_FALSE(map_uint64_uint64_key_ == NULL);
  EXPECT_FALSE(map_uint64_uint64_val_ == NULL);
  EXPECT_FALSE(map_sint32_sint32_key_ == NULL);
  EXPECT_FALSE(map_sint32_sint32_val_ == NULL);
  EXPECT_FALSE(map_sint64_sint64_key_ == NULL);
  EXPECT_FALSE(map_sint64_sint64_val_ == NULL);
  EXPECT_FALSE(map_fixed32_fixed32_key_ == NULL);
  EXPECT_FALSE(map_fixed32_fixed32_val_ == NULL);
  EXPECT_FALSE(map_fixed64_fixed64_key_ == NULL);
  EXPECT_FALSE(map_fixed64_fixed64_val_ == NULL);
  EXPECT_FALSE(map_sfixed32_sfixed32_key_ == NULL);
  EXPECT_FALSE(map_sfixed32_sfixed32_val_ == NULL);
  EXPECT_FALSE(map_sfixed64_sfixed64_key_ == NULL);
  EXPECT_FALSE(map_sfixed64_sfixed64_val_ == NULL);
  EXPECT_FALSE(map_int32_float_key_ == NULL);
  EXPECT_FALSE(map_int32_float_val_ == NULL);
  EXPECT_FALSE(map_int32_double_key_ == NULL);
  EXPECT_FALSE(map_int32_double_val_ == NULL);
  EXPECT_FALSE(map_bool_bool_key_ == NULL);
  EXPECT_FALSE(map_bool_bool_val_ == NULL);
  EXPECT_FALSE(map_string_string_key_ == NULL);
  EXPECT_FALSE(map_string_string_val_ == NULL);
  EXPECT_FALSE(map_int32_bytes_key_ == NULL);
  EXPECT_FALSE(map_int32_bytes_val_ == NULL);
  EXPECT_FALSE(map_int32_enum_key_ == NULL);
  EXPECT_FALSE(map_int32_enum_val_ == NULL);
  EXPECT_FALSE(map_int32_foreign_message_key_ == NULL);
  EXPECT_FALSE(map_int32_foreign_message_val_ == NULL);
}

// Shorthand to get a FieldDescriptor for a field of unittest::TestMap.
const FieldDescriptor* MapReflectionTester::F(const std::string& name) {
  const FieldDescriptor* result = NULL;
  result = base_descriptor_->FindFieldByName(name);
  GOOGLE_CHECK(result != NULL);
  return result;
}

void MapReflectionTester::SetMapFieldsViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();
  Message* sub_message = NULL;
  Message* sub_foreign_message = NULL;

  // Add first element.
  sub_message = reflection->AddMessage(message, F("map_int32_int32"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_int32_key_, 0);
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_int32_val_, 0);

  sub_message = reflection->AddMessage(message, F("map_int64_int64"));
  sub_message->GetReflection()->SetInt64(sub_message, map_int64_int64_key_, 0);
  sub_message->GetReflection()->SetInt64(sub_message, map_int64_int64_val_, 0);

  sub_message = reflection->AddMessage(message, F("map_uint32_uint32"));
  sub_message->GetReflection()->SetUInt32(sub_message, map_uint32_uint32_key_,
                                          0);
  sub_message->GetReflection()->SetUInt32(sub_message, map_uint32_uint32_val_,
                                          0);

  sub_message = reflection->AddMessage(message, F("map_uint64_uint64"));
  sub_message->GetReflection()->SetUInt64(sub_message, map_uint64_uint64_key_,
                                          0);
  sub_message->GetReflection()->SetUInt64(sub_message, map_uint64_uint64_val_,
                                          0);

  sub_message = reflection->AddMessage(message, F("map_sint32_sint32"));
  sub_message->GetReflection()->SetInt32(sub_message, map_sint32_sint32_key_,
                                         0);
  sub_message->GetReflection()->SetInt32(sub_message, map_sint32_sint32_val_,
                                         0);

  sub_message = reflection->AddMessage(message, F("map_sint64_sint64"));
  sub_message->GetReflection()->SetInt64(sub_message, map_sint64_sint64_key_,
                                         0);
  sub_message->GetReflection()->SetInt64(sub_message, map_sint64_sint64_val_,
                                         0);

  sub_message = reflection->AddMessage(message, F("map_fixed32_fixed32"));
  sub_message->GetReflection()->SetUInt32(sub_message, map_fixed32_fixed32_key_,
                                          0);
  sub_message->GetReflection()->SetUInt32(sub_message, map_fixed32_fixed32_val_,
                                          0);

  sub_message = reflection->AddMessage(message, F("map_fixed64_fixed64"));
  sub_message->GetReflection()->SetUInt64(sub_message, map_fixed64_fixed64_key_,
                                          0);
  sub_message->GetReflection()->SetUInt64(sub_message, map_fixed64_fixed64_val_,
                                          0);

  sub_message = reflection->AddMessage(message, F("map_sfixed32_sfixed32"));
  sub_message->GetReflection()->SetInt32(sub_message,
                                         map_sfixed32_sfixed32_key_, 0);
  sub_message->GetReflection()->SetInt32(sub_message,
                                         map_sfixed32_sfixed32_val_, 0);

  sub_message = reflection->AddMessage(message, F("map_sfixed64_sfixed64"));
  sub_message->GetReflection()->SetInt64(sub_message,
                                         map_sfixed64_sfixed64_key_, 0);
  sub_message->GetReflection()->SetInt64(sub_message,
                                         map_sfixed64_sfixed64_val_, 0);

  sub_message = reflection->AddMessage(message, F("map_int32_float"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_float_key_, 0);
  sub_message->GetReflection()->SetFloat(sub_message, map_int32_float_val_,
                                         0.0);

  sub_message = reflection->AddMessage(message, F("map_int32_double"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_double_key_, 0);
  sub_message->GetReflection()->SetDouble(sub_message, map_int32_double_val_,
                                          0.0);

  sub_message = reflection->AddMessage(message, F("map_bool_bool"));
  sub_message->GetReflection()->SetBool(sub_message, map_bool_bool_key_, false);
  sub_message->GetReflection()->SetBool(sub_message, map_bool_bool_val_, false);

  sub_message = reflection->AddMessage(message, F("map_string_string"));
  sub_message->GetReflection()->SetString(sub_message, map_string_string_key_,
                                          "0");
  sub_message->GetReflection()->SetString(sub_message, map_string_string_val_,
                                          "0");

  sub_message = reflection->AddMessage(message, F("map_int32_bytes"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_bytes_key_, 0);
  sub_message->GetReflection()->SetString(sub_message, map_int32_bytes_val_,
                                          "0");

  sub_message = reflection->AddMessage(message, F("map_int32_enum"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_enum_key_, 0);
  sub_message->GetReflection()->SetEnum(sub_message, map_int32_enum_val_,
                                        map_enum_bar_);

  sub_message = reflection->AddMessage(message, F("map_int32_foreign_message"));
  sub_message->GetReflection()->SetInt32(sub_message,
                                         map_int32_foreign_message_key_, 0);
  sub_foreign_message = sub_message->GetReflection()->MutableMessage(
      sub_message, map_int32_foreign_message_val_, NULL);
  sub_foreign_message->GetReflection()->SetInt32(sub_foreign_message,
                                                 foreign_c_, 0);

  // Add second element
  sub_message = reflection->AddMessage(message, F("map_int32_int32"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_int32_key_, 1);
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_int32_val_, 1);

  sub_message = reflection->AddMessage(message, F("map_int64_int64"));
  sub_message->GetReflection()->SetInt64(sub_message, map_int64_int64_key_, 1);
  sub_message->GetReflection()->SetInt64(sub_message, map_int64_int64_val_, 1);

  sub_message = reflection->AddMessage(message, F("map_uint32_uint32"));
  sub_message->GetReflection()->SetUInt32(sub_message, map_uint32_uint32_key_,
                                          1);
  sub_message->GetReflection()->SetUInt32(sub_message, map_uint32_uint32_val_,
                                          1);

  sub_message = reflection->AddMessage(message, F("map_uint64_uint64"));
  sub_message->GetReflection()->SetUInt64(sub_message, map_uint64_uint64_key_,
                                          1);
  sub_message->GetReflection()->SetUInt64(sub_message, map_uint64_uint64_val_,
                                          1);

  sub_message = reflection->AddMessage(message, F("map_sint32_sint32"));
  sub_message->GetReflection()->SetInt32(sub_message, map_sint32_sint32_key_,
                                         1);
  sub_message->GetReflection()->SetInt32(sub_message, map_sint32_sint32_val_,
                                         1);

  sub_message = reflection->AddMessage(message, F("map_sint64_sint64"));
  sub_message->GetReflection()->SetInt64(sub_message, map_sint64_sint64_key_,
                                         1);
  sub_message->GetReflection()->SetInt64(sub_message, map_sint64_sint64_val_,
                                         1);

  sub_message = reflection->AddMessage(message, F("map_fixed32_fixed32"));
  sub_message->GetReflection()->SetUInt32(sub_message, map_fixed32_fixed32_key_,
                                          1);
  sub_message->GetReflection()->SetUInt32(sub_message, map_fixed32_fixed32_val_,
                                          1);

  sub_message = reflection->AddMessage(message, F("map_fixed64_fixed64"));
  sub_message->GetReflection()->SetUInt64(sub_message, map_fixed64_fixed64_key_,
                                          1);
  sub_message->GetReflection()->SetUInt64(sub_message, map_fixed64_fixed64_val_,
                                          1);

  sub_message = reflection->AddMessage(message, F("map_sfixed32_sfixed32"));
  sub_message->GetReflection()->SetInt32(sub_message,
                                         map_sfixed32_sfixed32_key_, 1);
  sub_message->GetReflection()->SetInt32(sub_message,
                                         map_sfixed32_sfixed32_val_, 1);

  sub_message = reflection->AddMessage(message, F("map_sfixed64_sfixed64"));
  sub_message->GetReflection()->SetInt64(sub_message,
                                         map_sfixed64_sfixed64_key_, 1);
  sub_message->GetReflection()->SetInt64(sub_message,
                                         map_sfixed64_sfixed64_val_, 1);

  sub_message = reflection->AddMessage(message, F("map_int32_float"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_float_key_, 1);
  sub_message->GetReflection()->SetFloat(sub_message, map_int32_float_val_,
                                         1.0);

  sub_message = reflection->AddMessage(message, F("map_int32_double"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_double_key_, 1);
  sub_message->GetReflection()->SetDouble(sub_message, map_int32_double_val_,
                                          1.0);

  sub_message = reflection->AddMessage(message, F("map_bool_bool"));
  sub_message->GetReflection()->SetBool(sub_message, map_bool_bool_key_, true);
  sub_message->GetReflection()->SetBool(sub_message, map_bool_bool_val_, true);

  sub_message = reflection->AddMessage(message, F("map_string_string"));
  sub_message->GetReflection()->SetString(sub_message, map_string_string_key_,
                                          "1");
  sub_message->GetReflection()->SetString(sub_message, map_string_string_val_,
                                          "1");

  sub_message = reflection->AddMessage(message, F("map_int32_bytes"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_bytes_key_, 1);
  sub_message->GetReflection()->SetString(sub_message, map_int32_bytes_val_,
                                          "1");

  sub_message = reflection->AddMessage(message, F("map_int32_enum"));
  sub_message->GetReflection()->SetInt32(sub_message, map_int32_enum_key_, 1);
  sub_message->GetReflection()->SetEnum(sub_message, map_int32_enum_val_,
                                        map_enum_baz_);

  sub_message = reflection->AddMessage(message, F("map_int32_foreign_message"));
  sub_message->GetReflection()->SetInt32(sub_message,
                                         map_int32_foreign_message_key_, 1);
  sub_foreign_message = sub_message->GetReflection()->MutableMessage(
      sub_message, map_int32_foreign_message_val_, NULL);
  sub_foreign_message->GetReflection()->SetInt32(sub_foreign_message,
                                                 foreign_c_, 1);
}

void MapReflectionTester::SetMapFieldsViaMapReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();

  Message* sub_foreign_message = NULL;
  MapValueRef map_val;

  // Add first element.
  MapKey map_key;
  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int32_int32"),
                                                 map_key, &map_val));
  map_val.SetInt32Value(0);

  map_key.SetInt64Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int64_int64"),
                                                 map_key, &map_val));
  map_val.SetInt64Value(0);

  map_key.SetUInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_uint32_uint32"), map_key, &map_val));
  map_val.SetUInt32Value(0);

  map_key.SetUInt64Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_uint64_uint64"), map_key, &map_val));
  map_val.SetUInt64Value(0);

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_sint32_sint32"), map_key, &map_val));
  map_val.SetInt32Value(0);

  map_key.SetInt64Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_sint64_sint64"), map_key, &map_val));
  map_val.SetInt64Value(0);

  map_key.SetUInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_fixed32_fixed32"), map_key, &map_val));
  map_val.SetUInt32Value(0);

  map_key.SetUInt64Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_fixed64_fixed64"), map_key, &map_val));
  map_val.SetUInt64Value(0);

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_sfixed32_sfixed32"), map_key, &map_val));
  map_val.SetInt32Value(0);

  map_key.SetInt64Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_sfixed64_sfixed64"), map_key, &map_val));
  map_val.SetInt64Value(0);

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int32_float"),
                                                 map_key, &map_val));
  map_val.SetFloatValue(0.0);

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int32_double"),
                                                 map_key, &map_val));
  map_val.SetDoubleValue(0.0);

  map_key.SetBoolValue(false);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_bool_bool"),
                                                 map_key, &map_val));
  map_val.SetBoolValue(false);

  map_key.SetStringValue("0");
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_string_string"), map_key, &map_val));
  map_val.SetStringValue("0");

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int32_bytes"),
                                                 map_key, &map_val));
  map_val.SetStringValue("0");

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int32_enum"),
                                                 map_key, &map_val));
  map_val.SetEnumValue(map_enum_bar_->number());

  map_key.SetInt32Value(0);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_int32_foreign_message"), map_key, &map_val));
  sub_foreign_message = map_val.MutableMessageValue();
  sub_foreign_message->GetReflection()->SetInt32(sub_foreign_message,
                                                 foreign_c_, 0);

  // Add second element
  map_key.SetInt32Value(1);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int32_int32"),
                                                 map_key, &map_val));
  map_val.SetInt32Value(1);
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(message, F("map_int32_int32"),
                                                  map_key, &map_val));

  map_key.SetInt64Value(1);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(message, F("map_int64_int64"),
                                                 map_key, &map_val));
  map_val.SetInt64Value(1);
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(message, F("map_int64_int64"),
                                                  map_key, &map_val));

  map_key.SetUInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_uint32_uint32"), map_key,
                                     &map_val);
  map_val.SetUInt32Value(1);

  map_key.SetUInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_uint64_uint64"), map_key,
                                     &map_val);
  map_val.SetUInt64Value(1);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sint32_sint32"), map_key,
                                     &map_val);
  map_val.SetInt32Value(1);

  map_key.SetInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sint64_sint64"), map_key,
                                     &map_val);
  map_val.SetInt64Value(1);

  map_key.SetUInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_fixed32_fixed32"), map_key,
                                     &map_val);
  map_val.SetUInt32Value(1);

  map_key.SetUInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_fixed64_fixed64"), map_key,
                                     &map_val);
  map_val.SetUInt64Value(1);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sfixed32_sfixed32"),
                                     map_key, &map_val);
  map_val.SetInt32Value(1);

  map_key.SetInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sfixed64_sfixed64"),
                                     map_key, &map_val);
  map_val.SetInt64Value(1);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_float"), map_key,
                                     &map_val);
  map_val.SetFloatValue(1.0);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_double"), map_key,
                                     &map_val);
  map_val.SetDoubleValue(1.0);

  map_key.SetBoolValue(true);
  reflection->InsertOrLookupMapValue(message, F("map_bool_bool"), map_key,
                                     &map_val);
  map_val.SetBoolValue(true);

  map_key.SetStringValue("1");
  reflection->InsertOrLookupMapValue(message, F("map_string_string"), map_key,
                                     &map_val);
  map_val.SetStringValue("1");

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_bytes"), map_key,
                                     &map_val);
  map_val.SetStringValue("1");

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_enum"), map_key,
                                     &map_val);
  map_val.SetEnumValue(map_enum_baz_->number());

  map_key.SetInt32Value(1);
  EXPECT_TRUE(reflection->InsertOrLookupMapValue(
      message, F("map_int32_foreign_message"), map_key, &map_val));
  sub_foreign_message = map_val.MutableMessageValue();
  sub_foreign_message->GetReflection()->SetInt32(sub_foreign_message,
                                                 foreign_c_, 1);
}

void MapReflectionTester::GetMapValueViaMapReflection(
    Message* message, const std::string& field_name, const MapKey& map_key,
    MapValueRef* map_val) {
  const Reflection* reflection = message->GetReflection();
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(message, F(field_name),
                                                  map_key, map_val));
}

Message* MapReflectionTester::GetMapEntryViaReflection(
    Message* message, const std::string& field_name, int index) {
  const Reflection* reflection = message->GetReflection();
  return reflection->MutableRepeatedMessage(message, F(field_name), index);
}

MapIterator MapReflectionTester::MapBegin(Message* message,
                                          const std::string& field_name) {
  const Reflection* reflection = message->GetReflection();
  return reflection->MapBegin(message, F(field_name));
}

MapIterator MapReflectionTester::MapEnd(Message* message,
                                        const std::string& field_name) {
  const Reflection* reflection = message->GetReflection();
  return reflection->MapEnd(message, F(field_name));
}

void MapReflectionTester::ClearMapFieldsViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();

  reflection->ClearField(message, F("map_int32_int32"));
  reflection->ClearField(message, F("map_int64_int64"));
  reflection->ClearField(message, F("map_uint32_uint32"));
  reflection->ClearField(message, F("map_uint64_uint64"));
  reflection->ClearField(message, F("map_sint32_sint32"));
  reflection->ClearField(message, F("map_sint64_sint64"));
  reflection->ClearField(message, F("map_fixed32_fixed32"));
  reflection->ClearField(message, F("map_fixed64_fixed64"));
  reflection->ClearField(message, F("map_sfixed32_sfixed32"));
  reflection->ClearField(message, F("map_sfixed64_sfixed64"));
  reflection->ClearField(message, F("map_int32_float"));
  reflection->ClearField(message, F("map_int32_double"));
  reflection->ClearField(message, F("map_bool_bool"));
  reflection->ClearField(message, F("map_string_string"));
  reflection->ClearField(message, F("map_int32_bytes"));
  reflection->ClearField(message, F("map_int32_enum"));
  reflection->ClearField(message, F("map_int32_foreign_message"));
}

void MapReflectionTester::ModifyMapFieldsViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();
  MapValueRef map_val;
  Message* sub_foreign_message;

  // Modify the second element
  MapKey map_key;
  map_key.SetInt32Value(1);
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(message, F("map_int32_int32"),
                                                  map_key, &map_val));
  map_val.SetInt32Value(2);

  map_key.SetInt64Value(1);
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(message, F("map_int64_int64"),
                                                  map_key, &map_val));
  map_val.SetInt64Value(2);

  map_key.SetUInt32Value(1);
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(
      message, F("map_uint32_uint32"), map_key, &map_val));
  map_val.SetUInt32Value(2);

  map_key.SetUInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_uint64_uint64"), map_key,
                                     &map_val);
  map_val.SetUInt64Value(2);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sint32_sint32"), map_key,
                                     &map_val);
  map_val.SetInt32Value(2);

  map_key.SetInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sint64_sint64"), map_key,
                                     &map_val);
  map_val.SetInt64Value(2);

  map_key.SetUInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_fixed32_fixed32"), map_key,
                                     &map_val);
  map_val.SetUInt32Value(2);

  map_key.SetUInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_fixed64_fixed64"), map_key,
                                     &map_val);
  map_val.SetUInt64Value(2);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sfixed32_sfixed32"),
                                     map_key, &map_val);
  map_val.SetInt32Value(2);

  map_key.SetInt64Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_sfixed64_sfixed64"),
                                     map_key, &map_val);
  map_val.SetInt64Value(2);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_float"), map_key,
                                     &map_val);
  map_val.SetFloatValue(2.0);

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_double"), map_key,
                                     &map_val);
  map_val.SetDoubleValue(2.0);

  map_key.SetBoolValue(true);
  reflection->InsertOrLookupMapValue(message, F("map_bool_bool"), map_key,
                                     &map_val);
  map_val.SetBoolValue(false);

  map_key.SetStringValue("1");
  reflection->InsertOrLookupMapValue(message, F("map_string_string"), map_key,
                                     &map_val);
  map_val.SetStringValue("2");

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_bytes"), map_key,
                                     &map_val);
  map_val.SetStringValue("2");

  map_key.SetInt32Value(1);
  reflection->InsertOrLookupMapValue(message, F("map_int32_enum"), map_key,
                                     &map_val);
  map_val.SetEnumValue(map_enum_foo_->number());

  map_key.SetInt32Value(1);
  EXPECT_FALSE(reflection->InsertOrLookupMapValue(
      message, F("map_int32_foreign_message"), map_key, &map_val));
  sub_foreign_message = map_val.MutableMessageValue();
  sub_foreign_message->GetReflection()->SetInt32(sub_foreign_message,
                                                 foreign_c_, 2);
}

void MapReflectionTester::RemoveLastMapsViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();

  std::vector<const FieldDescriptor*> output;
  reflection->ListFields(*message, &output);
  for (int i = 0; i < output.size(); ++i) {
    const FieldDescriptor* field = output[i];
    if (!field->is_repeated()) continue;
    reflection->RemoveLast(message, field);
  }
}

void MapReflectionTester::ReleaseLastMapsViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();

  std::vector<const FieldDescriptor*> output;
  reflection->ListFields(*message, &output);
  for (int i = 0; i < output.size(); ++i) {
    const FieldDescriptor* field = output[i];
    if (!field->is_repeated()) continue;
    if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) continue;

    Message* released = reflection->ReleaseLast(message, field);
    ASSERT_TRUE(released != NULL)
        << "ReleaseLast returned NULL for: " << field->name();
    delete released;
  }
}

void MapReflectionTester::SwapMapsViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();
  std::vector<const FieldDescriptor*> output;
  reflection->ListFields(*message, &output);
  for (int i = 0; i < output.size(); ++i) {
    const FieldDescriptor* field = output[i];
    if (!field->is_repeated()) continue;
    reflection->SwapElements(message, field, 0, 1);
  }
}

void MapReflectionTester::MutableUnknownFieldsOfMapFieldsViaReflection(
    Message* message) {
  const Reflection* reflection = message->GetReflection();
  Message* sub_message = NULL;

  sub_message = reflection->AddMessage(message, F("map_int32_int32"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_int64_int64"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_uint32_uint32"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_uint64_uint64"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_sint32_sint32"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_sint64_sint64"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_fixed32_fixed32"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_fixed64_fixed64"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_sfixed32_sfixed32"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_sfixed64_sfixed64"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_int32_float"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_int32_double"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_bool_bool"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_string_string"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_int32_bytes"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_int32_enum"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
  sub_message = reflection->AddMessage(message, F("map_int32_foreign_message"));
  EXPECT_TRUE(sub_message->GetReflection()->MutableUnknownFields(sub_message) !=
              NULL);
}

void MapReflectionTester::ExpectMapFieldsSetViaReflection(
    const Message& message) {
  std::string scratch;
  const Reflection* reflection = message.GetReflection();
  const Message* sub_message;
  MapKey map_key;

  // -----------------------------------------------------------------

  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int32_int32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int64_int64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_uint32_uint32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_uint64_uint64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_sint32_sint32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_sint64_sint64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_fixed32_fixed32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_fixed64_fixed64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_sfixed32_sfixed32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_sfixed64_sfixed64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int32_float")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int32_double")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_bool_bool")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_string_string")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int32_bytes")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int32_enum")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("map_int32_foreign_message")));

  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      // Check with RepeatedField Reflection
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_int32_int32"), i);
      int32 key = sub_message->GetReflection()->GetInt32(*sub_message,
                                                         map_int32_int32_key_);
      int32 val = sub_message->GetReflection()->GetInt32(*sub_message,
                                                         map_int32_int32_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_TRUE(
          reflection->ContainsMapKey(message, F("map_int32_int32"), map_key));
    }
  }
  {
    std::map<int64, int64> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      // Check with RepeatedField Reflection
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_int64_int64"), i);
      int64 key = sub_message->GetReflection()->GetInt64(*sub_message,
                                                         map_int64_int64_key_);
      int64 val = sub_message->GetReflection()->GetInt64(*sub_message,
                                                         map_int64_int64_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt64Value(key);
      EXPECT_TRUE(
          reflection->ContainsMapKey(message, F("map_int64_int64"), map_key));
    }
  }
  {
    std::map<uint32, uint32> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      // Check with RepeatedField Reflection
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_uint32_uint32"), i);
      uint32 key = sub_message->GetReflection()->GetUInt32(
          *sub_message, map_uint32_uint32_key_);
      uint32 val = sub_message->GetReflection()->GetUInt32(
          *sub_message, map_uint32_uint32_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetUInt32Value(key);
      EXPECT_TRUE(
          reflection->ContainsMapKey(message, F("map_uint32_uint32"), map_key));
    }
  }
  {
    std::map<uint64, uint64> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_uint64_uint64"), i);
      uint64 key = sub_message->GetReflection()->GetUInt64(
          *sub_message, map_uint64_uint64_key_);
      uint64 val = sub_message->GetReflection()->GetUInt64(
          *sub_message, map_uint64_uint64_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetUInt64Value(key);
      EXPECT_TRUE(
          reflection->ContainsMapKey(message, F("map_uint64_uint64"), map_key));
    }
  }
  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_sint32_sint32"), i);
      int32 key = sub_message->GetReflection()->GetInt32(
          *sub_message, map_sint32_sint32_key_);
      int32 val = sub_message->GetReflection()->GetInt32(
          *sub_message, map_sint32_sint32_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_sint32_sint32"), map_key));
    }
  }
  {
    std::map<int64, int64> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_sint64_sint64"), i);
      int64 key = sub_message->GetReflection()->GetInt64(
          *sub_message, map_sint64_sint64_key_);
      int64 val = sub_message->GetReflection()->GetInt64(
          *sub_message, map_sint64_sint64_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt64Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_sint64_sint64"), map_key));
    }
  }
  {
    std::map<uint32, uint32> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_fixed32_fixed32"), i);
      uint32 key = sub_message->GetReflection()->GetUInt32(
          *sub_message, map_fixed32_fixed32_key_);
      uint32 val = sub_message->GetReflection()->GetUInt32(
          *sub_message, map_fixed32_fixed32_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetUInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_fixed32_fixed32"), map_key));
    }
  }
  {
    std::map<uint64, uint64> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_fixed64_fixed64"), i);
      uint64 key = sub_message->GetReflection()->GetUInt64(
          *sub_message, map_fixed64_fixed64_key_);
      uint64 val = sub_message->GetReflection()->GetUInt64(
          *sub_message, map_fixed64_fixed64_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetUInt64Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_fixed64_fixed64"), map_key));
    }
  }
  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message = &reflection->GetRepeatedMessage(
          message, F("map_sfixed32_sfixed32"), i);
      int32 key = sub_message->GetReflection()->GetInt32(
          *sub_message, map_sfixed32_sfixed32_key_);
      int32 val = sub_message->GetReflection()->GetInt32(
          *sub_message, map_sfixed32_sfixed32_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_sfixed32_sfixed32"), map_key));
    }
  }
  {
    std::map<int64, int64> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message = &reflection->GetRepeatedMessage(
          message, F("map_sfixed64_sfixed64"), i);
      int64 key = sub_message->GetReflection()->GetInt64(
          *sub_message, map_sfixed64_sfixed64_key_);
      int64 val = sub_message->GetReflection()->GetInt64(
          *sub_message, map_sfixed64_sfixed64_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt64Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_sfixed64_sfixed64"), map_key));
    }
  }
  {
    std::map<int32, float> map;
    map[0] = 0.0;
    map[1] = 1.0;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_int32_float"), i);
      int32 key = sub_message->GetReflection()->GetInt32(*sub_message,
                                                         map_int32_float_key_);
      float val = sub_message->GetReflection()->GetFloat(*sub_message,
                                                         map_int32_float_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(message, F("map_int32_float"),
                                                 map_key));
    }
  }
  {
    std::map<int32, double> map;
    map[0] = 0.0;
    map[1] = 1.0;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_int32_double"), i);
      int32 key = sub_message->GetReflection()->GetInt32(*sub_message,
                                                         map_int32_double_key_);
      double val = sub_message->GetReflection()->GetDouble(
          *sub_message, map_int32_double_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(message, F("map_int32_double"),
                                                 map_key));
    }
  }
  {
    std::map<bool, bool> map;
    map[false] = false;
    map[true] = true;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_bool_bool"), i);
      bool key = sub_message->GetReflection()->GetBool(*sub_message,
                                                       map_bool_bool_key_);
      bool val = sub_message->GetReflection()->GetBool(*sub_message,
                                                       map_bool_bool_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetBoolValue(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(message, F("map_bool_bool"),
                                                 map_key));
    }
  }
  {
    std::map<std::string, std::string> map;
    map["0"] = "0";
    map["1"] = "1";
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_string_string"), i);
      std::string key = sub_message->GetReflection()->GetString(
          *sub_message, map_string_string_key_);
      std::string val = sub_message->GetReflection()->GetString(
          *sub_message, map_string_string_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetStringValue(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_string_string"), map_key));
    }
  }
  {
    std::map<int32, std::string> map;
    map[0] = "0";
    map[1] = "1";
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_int32_bytes"), i);
      int32 key = sub_message->GetReflection()->GetInt32(*sub_message,
                                                         map_int32_bytes_key_);
      std::string val = sub_message->GetReflection()->GetString(
          *sub_message, map_int32_bytes_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(message, F("map_int32_bytes"),
                                                 map_key));
    }
  }
  {
    std::map<int32, const EnumValueDescriptor*> map;
    map[0] = map_enum_bar_;
    map[1] = map_enum_baz_;
    for (int i = 0; i < 2; i++) {
      sub_message =
          &reflection->GetRepeatedMessage(message, F("map_int32_enum"), i);
      int32 key = sub_message->GetReflection()->GetInt32(*sub_message,
                                                         map_int32_enum_key_);
      const EnumValueDescriptor* val = sub_message->GetReflection()->GetEnum(
          *sub_message, map_int32_enum_val_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(message, F("map_int32_enum"),
                                                 map_key));
    }
  }
  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    for (int i = 0; i < 2; i++) {
      sub_message = &reflection->GetRepeatedMessage(
          message, F("map_int32_foreign_message"), i);
      int32 key = sub_message->GetReflection()->GetInt32(
          *sub_message, map_int32_foreign_message_key_);
      const Message& foreign_message = sub_message->GetReflection()->GetMessage(
          *sub_message, map_int32_foreign_message_val_);
      int32 val = foreign_message.GetReflection()->GetInt32(foreign_message,
                                                            foreign_c_);
      EXPECT_EQ(map[key], val);
      // Check with Map Reflection
      map_key.SetInt32Value(key);
      EXPECT_EQ(true, reflection->ContainsMapKey(
                          message, F("map_int32_foreign_message"), map_key));
    }
  }
}

void MapReflectionTester::ExpectMapFieldsSetViaReflectionIterator(
    Message* message) {
  std::string scratch;
  std::string serialized;
  const Reflection* reflection = message->GetReflection();

  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int32_int32")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int64_int64")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_uint32_uint32")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_uint64_uint64")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_sint32_sint32")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_sint64_sint64")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_fixed32_fixed32")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_fixed64_fixed64")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_sfixed32_sfixed32")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_sfixed64_sfixed64")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int32_float")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int32_double")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_bool_bool")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_string_string")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int32_bytes")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int32_enum")));
  ASSERT_EQ(2, reflection->FieldSize(*message, F("map_int32_foreign_message")));

  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    int size = 0;
    for (MapIterator iter = reflection->MapBegin(message, F("map_int32_int32"));
         iter != reflection->MapEnd(message, F("map_int32_int32"));
         ++iter, ++size) {
      // Check const methods do not invalidate map.
      message->DebugString();
      message->ShortDebugString();
      message->SerializeToString(&serialized);
      message->SpaceUsed();
      message->ByteSize();
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                iter.GetValueRef().GetInt32Value());
    }
    EXPECT_EQ(size, 2);
  }
  {
    std::map<int64, int64> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter = reflection->MapBegin(message, F("map_int64_int64"));
         iter != reflection->MapEnd(message, F("map_int64_int64")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt64Value()],
                iter.GetValueRef().GetInt64Value());
    }
  }
  {
    std::map<uint32, uint32> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_uint32_uint32"));
         iter != reflection->MapEnd(message, F("map_uint32_uint32")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetUInt32Value()],
                iter.GetValueRef().GetUInt32Value());
    }
  }
  {
    std::map<uint64, uint64> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_uint64_uint64"));
         iter != reflection->MapEnd(message, F("map_uint64_uint64")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetUInt64Value()],
                iter.GetValueRef().GetUInt64Value());
    }
  }
  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_sint32_sint32"));
         iter != reflection->MapEnd(message, F("map_sint32_sint32")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                iter.GetValueRef().GetInt32Value());
    }
  }
  {
    std::map<int64, int64> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_sint64_sint64"));
         iter != reflection->MapEnd(message, F("map_sint64_sint64")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt64Value()],
                iter.GetValueRef().GetInt64Value());
    }
  }
  {
    std::map<uint32, uint32> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_fixed32_fixed32"));
         iter != reflection->MapEnd(message, F("map_fixed32_fixed32"));
         ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetUInt32Value()],
                iter.GetValueRef().GetUInt32Value());
    }
  }
  {
    std::map<uint64, uint64> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_fixed64_fixed64"));
         iter != reflection->MapEnd(message, F("map_fixed64_fixed64"));
         ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetUInt64Value()],
                iter.GetValueRef().GetUInt64Value());
    }
  }
  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_sfixed32_sfixed32"));
         iter != reflection->MapEnd(message, F("map_sfixed32_sfixed32"));
         ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                iter.GetValueRef().GetInt32Value());
    }
  }
  {
    std::map<int32, float> map;
    map[0] = 0.0;
    map[1] = 1.0;
    for (MapIterator iter = reflection->MapBegin(message, F("map_int32_float"));
         iter != reflection->MapEnd(message, F("map_int32_float")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                iter.GetValueRef().GetFloatValue());
    }
  }
  {
    std::map<int32, double> map;
    map[0] = 0.0;
    map[1] = 1.0;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_int32_double"));
         iter != reflection->MapEnd(message, F("map_int32_double")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                iter.GetValueRef().GetDoubleValue());
    }
  }
  {
    std::map<bool, bool> map;
    map[false] = false;
    map[true] = true;
    for (MapIterator iter = reflection->MapBegin(message, F("map_bool_bool"));
         iter != reflection->MapEnd(message, F("map_bool_bool")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetBoolValue()],
                iter.GetValueRef().GetBoolValue());
    }
  }
  {
    std::map<std::string, std::string> map;
    map["0"] = "0";
    map["1"] = "1";
    int size = 0;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_string_string"));
         iter != reflection->MapEnd(message, F("map_string_string"));
         ++iter, ++size) {
      // Check const methods do not invalidate map.
      message->DebugString();
      message->ShortDebugString();
      message->SerializeToString(&serialized);
      message->SpaceUsed();
      message->ByteSize();
      EXPECT_EQ(map[iter.GetKey().GetStringValue()],
                iter.GetValueRef().GetStringValue());
    }
    EXPECT_EQ(size, 2);
  }
  {
    std::map<int32, std::string> map;
    map[0] = "0";
    map[1] = "1";
    for (MapIterator iter = reflection->MapBegin(message, F("map_int32_bytes"));
         iter != reflection->MapEnd(message, F("map_int32_bytes")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                iter.GetValueRef().GetStringValue());
    }
  }
  {
    std::map<int32, const EnumValueDescriptor*> map;
    map[0] = map_enum_bar_;
    map[1] = map_enum_baz_;
    for (MapIterator iter = reflection->MapBegin(message, F("map_int32_enum"));
         iter != reflection->MapEnd(message, F("map_int32_enum")); ++iter) {
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()]->number(),
                iter.GetValueRef().GetEnumValue());
    }
  }
  {
    std::map<int32, int32> map;
    map[0] = 0;
    map[1] = 1;
    int size = 0;
    for (MapIterator iter =
             reflection->MapBegin(message, F("map_int32_foreign_message"));
         iter != reflection->MapEnd(message, F("map_int32_foreign_message"));
         ++iter, ++size) {
      // Check const methods do not invalidate map.
      message->DebugString();
      message->ShortDebugString();
      message->SerializeToString(&serialized);
      message->SpaceUsed();
      message->ByteSize();
      const Message& sub_message = iter.GetValueRef().GetMessageValue();
      EXPECT_EQ(map[iter.GetKey().GetInt32Value()],
                sub_message.GetReflection()->GetInt32(sub_message, foreign_c_));
    }
    EXPECT_EQ(size, 2);
  }
}

void MapReflectionTester::ExpectClearViaReflection(const Message& message) {
  const Reflection* reflection = message.GetReflection();
  // Map fields are empty.
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int32_int32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int64_int64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_uint32_uint32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_uint64_uint64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_sint32_sint32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_sint64_sint64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_fixed32_fixed32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_fixed64_fixed64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_sfixed32_sfixed32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_sfixed64_sfixed64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int32_float")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int32_double")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_bool_bool")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_string_string")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int32_bytes")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int32_enum")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("map_int32_foreign_message")));
  EXPECT_TRUE(reflection->GetMapData(message, F("map_int32_foreign_message"))
                  ->IsMapValid());
}

void MapReflectionTester::ExpectClearViaReflectionIterator(Message* message) {
  const Reflection* reflection = message->GetReflection();
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int32_int32")) ==
              reflection->MapEnd(message, F("map_int32_int32")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int64_int64")) ==
              reflection->MapEnd(message, F("map_int64_int64")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_uint32_uint32")) ==
              reflection->MapEnd(message, F("map_uint32_uint32")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_uint64_uint64")) ==
              reflection->MapEnd(message, F("map_uint64_uint64")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_sint32_sint32")) ==
              reflection->MapEnd(message, F("map_sint32_sint32")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_sint64_sint64")) ==
              reflection->MapEnd(message, F("map_sint64_sint64")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_fixed32_fixed32")) ==
              reflection->MapEnd(message, F("map_fixed32_fixed32")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_fixed64_fixed64")) ==
              reflection->MapEnd(message, F("map_fixed64_fixed64")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_sfixed32_sfixed32")) ==
              reflection->MapEnd(message, F("map_sfixed32_sfixed32")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_sfixed64_sfixed64")) ==
              reflection->MapEnd(message, F("map_sfixed64_sfixed64")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int32_float")) ==
              reflection->MapEnd(message, F("map_int32_float")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int32_double")) ==
              reflection->MapEnd(message, F("map_int32_double")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_bool_bool")) ==
              reflection->MapEnd(message, F("map_bool_bool")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_string_string")) ==
              reflection->MapEnd(message, F("map_string_string")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int32_bytes")) ==
              reflection->MapEnd(message, F("map_int32_bytes")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int32_enum")) ==
              reflection->MapEnd(message, F("map_int32_enum")));
  EXPECT_TRUE(reflection->MapBegin(message, F("map_int32_foreign_message")) ==
              reflection->MapEnd(message, F("map_int32_foreign_message")));
}

}  // namespace protobuf
}  // namespace google
