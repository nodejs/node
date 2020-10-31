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

#ifndef GOOGLE_PROTOBUF_MAP_TEST_UTIL_IMPL_H__
#define GOOGLE_PROTOBUF_MAP_TEST_UTIL_IMPL_H__

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <gtest/gtest.h>


namespace protobuf_unittest {}  // namespace protobuf_unittest

namespace google {
namespace protobuf {

namespace unittest = ::protobuf_unittest;

class MapTestUtilImpl {
 public:
  // Set every field in the TestMap message to a unique value.
  template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
            typename MapMessage>
  static void SetMapFields(MapMessage* message);

  // Set every field in the TestArenaMap message to a unique value.
  template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
            typename MapMessage>
  static void SetArenaMapFields(MapMessage* message);

  // Set every field in the message to a default value.
  template <typename MapMessage>
  static void SetMapFieldsInitialized(MapMessage* message);

  // Modify all the map fields of the message (which should already have been
  // initialized with SetMapFields()).
  template <typename EnumType, EnumType enum_value, typename MapMessage>
  static void ModifyMapFields(MapMessage* message);

  // Check that all fields have the values that they should have after
  // SetMapFields() is called.
  template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
            typename MapMessage>
  static void ExpectMapFieldsSet(const MapMessage& message);

  // Check that all fields have the values that they should have after
  // SetMapFields() is called for TestArenaMap.
  template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
            typename MapMessage>
  static void ExpectArenaMapFieldsSet(const MapMessage& message);

  // Check that all fields have the values that they should have after
  // SetMapFieldsInitialized() is called.
  template <typename EnumType, EnumType enum_value, typename MapMessage>
  static void ExpectMapFieldsSetInitialized(const MapMessage& message);

  // Expect that the message is modified as would be expected from
  // ModifyMapFields().
  template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
            typename MapMessage>
  static void ExpectMapFieldsModified(const MapMessage& message);

  // Check that all fields are empty.
  template <typename MapMessage>
  static void ExpectClear(const MapMessage& message);

  // // Check that all map fields have the given size.
  // template <typename MapMessage>
  // static void ExpectMapsSize(const MapMessage& message, int size);

  // // Get pointers of map entries at given index.
  // static std::vector<const Message*> GetMapEntries(
  //     const MapMessage& message, int index);

  // // Get pointers of map entries from release.
  // static std::vector<const Message*> GetMapEntriesFromRelease(
  //     MapMessage* message);
};

template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
          typename MapMessage>
void MapTestUtilImpl::SetMapFields(MapMessage* message) {
  // Add first element.
  (*message->mutable_map_int32_int32())[0] = 0;
  (*message->mutable_map_int64_int64())[0] = 0;
  (*message->mutable_map_uint32_uint32())[0] = 0;
  (*message->mutable_map_uint64_uint64())[0] = 0;
  (*message->mutable_map_sint32_sint32())[0] = 0;
  (*message->mutable_map_sint64_sint64())[0] = 0;
  (*message->mutable_map_fixed32_fixed32())[0] = 0;
  (*message->mutable_map_fixed64_fixed64())[0] = 0;
  (*message->mutable_map_sfixed32_sfixed32())[0] = 0;
  (*message->mutable_map_sfixed64_sfixed64())[0] = 0;
  (*message->mutable_map_int32_float())[0] = 0.0;
  (*message->mutable_map_int32_double())[0] = 0.0;
  (*message->mutable_map_bool_bool())[0] = false;
  (*message->mutable_map_string_string())["0"] = "0";
  (*message->mutable_map_int32_bytes())[0] = "0";
  (*message->mutable_map_int32_enum())[0] = enum_value0;
  (*message->mutable_map_int32_foreign_message())[0].set_c(0);

  // Add second element
  (*message->mutable_map_int32_int32())[1] = 1;
  (*message->mutable_map_int64_int64())[1] = 1;
  (*message->mutable_map_uint32_uint32())[1] = 1;
  (*message->mutable_map_uint64_uint64())[1] = 1;
  (*message->mutable_map_sint32_sint32())[1] = 1;
  (*message->mutable_map_sint64_sint64())[1] = 1;
  (*message->mutable_map_fixed32_fixed32())[1] = 1;
  (*message->mutable_map_fixed64_fixed64())[1] = 1;
  (*message->mutable_map_sfixed32_sfixed32())[1] = 1;
  (*message->mutable_map_sfixed64_sfixed64())[1] = 1;
  (*message->mutable_map_int32_float())[1] = 1.0;
  (*message->mutable_map_int32_double())[1] = 1.0;
  (*message->mutable_map_bool_bool())[1] = true;
  (*message->mutable_map_string_string())["1"] = "1";
  (*message->mutable_map_int32_bytes())[1] = "1";
  (*message->mutable_map_int32_enum())[1] = enum_value1;
  (*message->mutable_map_int32_foreign_message())[1].set_c(1);
}

template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
          typename MapMessage>
void MapTestUtilImpl::SetArenaMapFields(MapMessage* message) {
  // Add first element.
  (*message->mutable_map_int32_int32())[0] = 0;
  (*message->mutable_map_int64_int64())[0] = 0;
  (*message->mutable_map_uint32_uint32())[0] = 0;
  (*message->mutable_map_uint64_uint64())[0] = 0;
  (*message->mutable_map_sint32_sint32())[0] = 0;
  (*message->mutable_map_sint64_sint64())[0] = 0;
  (*message->mutable_map_fixed32_fixed32())[0] = 0;
  (*message->mutable_map_fixed64_fixed64())[0] = 0;
  (*message->mutable_map_sfixed32_sfixed32())[0] = 0;
  (*message->mutable_map_sfixed64_sfixed64())[0] = 0;
  (*message->mutable_map_int32_float())[0] = 0.0;
  (*message->mutable_map_int32_double())[0] = 0.0;
  (*message->mutable_map_bool_bool())[0] = false;
  (*message->mutable_map_string_string())["0"] = "0";
  (*message->mutable_map_int32_bytes())[0] = "0";
  (*message->mutable_map_int32_enum())[0] = enum_value0;
  (*message->mutable_map_int32_foreign_message())[0].set_c(0);
  (*message->mutable_map_int32_foreign_message_no_arena())[0].set_c(0);

  // Add second element
  (*message->mutable_map_int32_int32())[1] = 1;
  (*message->mutable_map_int64_int64())[1] = 1;
  (*message->mutable_map_uint32_uint32())[1] = 1;
  (*message->mutable_map_uint64_uint64())[1] = 1;
  (*message->mutable_map_sint32_sint32())[1] = 1;
  (*message->mutable_map_sint64_sint64())[1] = 1;
  (*message->mutable_map_fixed32_fixed32())[1] = 1;
  (*message->mutable_map_fixed64_fixed64())[1] = 1;
  (*message->mutable_map_sfixed32_sfixed32())[1] = 1;
  (*message->mutable_map_sfixed64_sfixed64())[1] = 1;
  (*message->mutable_map_int32_float())[1] = 1.0;
  (*message->mutable_map_int32_double())[1] = 1.0;
  (*message->mutable_map_bool_bool())[1] = true;
  (*message->mutable_map_string_string())["1"] = "1";
  (*message->mutable_map_int32_bytes())[1] = "1";
  (*message->mutable_map_int32_enum())[1] = enum_value1;
  (*message->mutable_map_int32_foreign_message())[1].set_c(1);
  (*message->mutable_map_int32_foreign_message_no_arena())[1].set_c(1);
}

template <typename MapMessage>
void MapTestUtilImpl::SetMapFieldsInitialized(MapMessage* message) {
  // Add first element using bracket operator, which should assign default
  // value automatically.
  (*message->mutable_map_int32_int32())[0];
  (*message->mutable_map_int64_int64())[0];
  (*message->mutable_map_uint32_uint32())[0];
  (*message->mutable_map_uint64_uint64())[0];
  (*message->mutable_map_sint32_sint32())[0];
  (*message->mutable_map_sint64_sint64())[0];
  (*message->mutable_map_fixed32_fixed32())[0];
  (*message->mutable_map_fixed64_fixed64())[0];
  (*message->mutable_map_sfixed32_sfixed32())[0];
  (*message->mutable_map_sfixed64_sfixed64())[0];
  (*message->mutable_map_int32_float())[0];
  (*message->mutable_map_int32_double())[0];
  (*message->mutable_map_bool_bool())[0];
  (*message->mutable_map_string_string())["0"];
  (*message->mutable_map_int32_bytes())[0];
  (*message->mutable_map_int32_enum())[0];
  (*message->mutable_map_int32_foreign_message())[0];
}

template <typename EnumType, EnumType enum_value, typename MapMessage>
void MapTestUtilImpl::ModifyMapFields(MapMessage* message) {
  (*message->mutable_map_int32_int32())[1] = 2;
  (*message->mutable_map_int64_int64())[1] = 2;
  (*message->mutable_map_uint32_uint32())[1] = 2;
  (*message->mutable_map_uint64_uint64())[1] = 2;
  (*message->mutable_map_sint32_sint32())[1] = 2;
  (*message->mutable_map_sint64_sint64())[1] = 2;
  (*message->mutable_map_fixed32_fixed32())[1] = 2;
  (*message->mutable_map_fixed64_fixed64())[1] = 2;
  (*message->mutable_map_sfixed32_sfixed32())[1] = 2;
  (*message->mutable_map_sfixed64_sfixed64())[1] = 2;
  (*message->mutable_map_int32_float())[1] = 2.0;
  (*message->mutable_map_int32_double())[1] = 2.0;
  (*message->mutable_map_bool_bool())[1] = false;
  (*message->mutable_map_string_string())["1"] = "2";
  (*message->mutable_map_int32_bytes())[1] = "2";
  (*message->mutable_map_int32_enum())[1] = enum_value;
  (*message->mutable_map_int32_foreign_message())[1].set_c(2);
}

template <typename MapMessage>
void MapTestUtilImpl::ExpectClear(const MapMessage& message) {
  EXPECT_EQ(0, message.map_int32_int32().size());
  EXPECT_EQ(0, message.map_int64_int64().size());
  EXPECT_EQ(0, message.map_uint32_uint32().size());
  EXPECT_EQ(0, message.map_uint64_uint64().size());
  EXPECT_EQ(0, message.map_sint32_sint32().size());
  EXPECT_EQ(0, message.map_sint64_sint64().size());
  EXPECT_EQ(0, message.map_fixed32_fixed32().size());
  EXPECT_EQ(0, message.map_fixed64_fixed64().size());
  EXPECT_EQ(0, message.map_sfixed32_sfixed32().size());
  EXPECT_EQ(0, message.map_sfixed64_sfixed64().size());
  EXPECT_EQ(0, message.map_int32_float().size());
  EXPECT_EQ(0, message.map_int32_double().size());
  EXPECT_EQ(0, message.map_bool_bool().size());
  EXPECT_EQ(0, message.map_string_string().size());
  EXPECT_EQ(0, message.map_int32_bytes().size());
  EXPECT_EQ(0, message.map_int32_enum().size());
  EXPECT_EQ(0, message.map_int32_foreign_message().size());
}

template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
          typename MapMessage>
void MapTestUtilImpl::ExpectMapFieldsSet(const MapMessage& message) {
  ASSERT_EQ(2, message.map_int32_int32().size());
  ASSERT_EQ(2, message.map_int64_int64().size());
  ASSERT_EQ(2, message.map_uint32_uint32().size());
  ASSERT_EQ(2, message.map_uint64_uint64().size());
  ASSERT_EQ(2, message.map_sint32_sint32().size());
  ASSERT_EQ(2, message.map_sint64_sint64().size());
  ASSERT_EQ(2, message.map_fixed32_fixed32().size());
  ASSERT_EQ(2, message.map_fixed64_fixed64().size());
  ASSERT_EQ(2, message.map_sfixed32_sfixed32().size());
  ASSERT_EQ(2, message.map_sfixed64_sfixed64().size());
  ASSERT_EQ(2, message.map_int32_float().size());
  ASSERT_EQ(2, message.map_int32_double().size());
  ASSERT_EQ(2, message.map_bool_bool().size());
  ASSERT_EQ(2, message.map_string_string().size());
  ASSERT_EQ(2, message.map_int32_bytes().size());
  ASSERT_EQ(2, message.map_int32_enum().size());
  ASSERT_EQ(2, message.map_int32_foreign_message().size());

  EXPECT_EQ(0, message.map_int32_int32().at(0));
  EXPECT_EQ(0, message.map_int64_int64().at(0));
  EXPECT_EQ(0, message.map_uint32_uint32().at(0));
  EXPECT_EQ(0, message.map_uint64_uint64().at(0));
  EXPECT_EQ(0, message.map_sint32_sint32().at(0));
  EXPECT_EQ(0, message.map_sint64_sint64().at(0));
  EXPECT_EQ(0, message.map_fixed32_fixed32().at(0));
  EXPECT_EQ(0, message.map_fixed64_fixed64().at(0));
  EXPECT_EQ(0, message.map_sfixed32_sfixed32().at(0));
  EXPECT_EQ(0, message.map_sfixed64_sfixed64().at(0));
  EXPECT_EQ(0, message.map_int32_float().at(0));
  EXPECT_EQ(0, message.map_int32_double().at(0));
  EXPECT_EQ(false, message.map_bool_bool().at(0));
  EXPECT_EQ("0", message.map_string_string().at("0"));
  EXPECT_EQ("0", message.map_int32_bytes().at(0));
  EXPECT_EQ(enum_value0, message.map_int32_enum().at(0));
  EXPECT_EQ(0, message.map_int32_foreign_message().at(0).c());

  EXPECT_EQ(1, message.map_int32_int32().at(1));
  EXPECT_EQ(1, message.map_int64_int64().at(1));
  EXPECT_EQ(1, message.map_uint32_uint32().at(1));
  EXPECT_EQ(1, message.map_uint64_uint64().at(1));
  EXPECT_EQ(1, message.map_sint32_sint32().at(1));
  EXPECT_EQ(1, message.map_sint64_sint64().at(1));
  EXPECT_EQ(1, message.map_fixed32_fixed32().at(1));
  EXPECT_EQ(1, message.map_fixed64_fixed64().at(1));
  EXPECT_EQ(1, message.map_sfixed32_sfixed32().at(1));
  EXPECT_EQ(1, message.map_sfixed64_sfixed64().at(1));
  EXPECT_EQ(1, message.map_int32_float().at(1));
  EXPECT_EQ(1, message.map_int32_double().at(1));
  EXPECT_EQ(true, message.map_bool_bool().at(1));
  EXPECT_EQ("1", message.map_string_string().at("1"));
  EXPECT_EQ("1", message.map_int32_bytes().at(1));
  EXPECT_EQ(enum_value1, message.map_int32_enum().at(1));
  EXPECT_EQ(1, message.map_int32_foreign_message().at(1).c());
}

template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
          typename MapMessage>
void MapTestUtilImpl::ExpectArenaMapFieldsSet(const MapMessage& message) {
  EXPECT_EQ(2, message.map_int32_int32().size());
  EXPECT_EQ(2, message.map_int64_int64().size());
  EXPECT_EQ(2, message.map_uint32_uint32().size());
  EXPECT_EQ(2, message.map_uint64_uint64().size());
  EXPECT_EQ(2, message.map_sint32_sint32().size());
  EXPECT_EQ(2, message.map_sint64_sint64().size());
  EXPECT_EQ(2, message.map_fixed32_fixed32().size());
  EXPECT_EQ(2, message.map_fixed64_fixed64().size());
  EXPECT_EQ(2, message.map_sfixed32_sfixed32().size());
  EXPECT_EQ(2, message.map_sfixed64_sfixed64().size());
  EXPECT_EQ(2, message.map_int32_float().size());
  EXPECT_EQ(2, message.map_int32_double().size());
  EXPECT_EQ(2, message.map_bool_bool().size());
  EXPECT_EQ(2, message.map_string_string().size());
  EXPECT_EQ(2, message.map_int32_bytes().size());
  EXPECT_EQ(2, message.map_int32_enum().size());
  EXPECT_EQ(2, message.map_int32_foreign_message().size());
  EXPECT_EQ(2, message.map_int32_foreign_message_no_arena().size());

  EXPECT_EQ(0, message.map_int32_int32().at(0));
  EXPECT_EQ(0, message.map_int64_int64().at(0));
  EXPECT_EQ(0, message.map_uint32_uint32().at(0));
  EXPECT_EQ(0, message.map_uint64_uint64().at(0));
  EXPECT_EQ(0, message.map_sint32_sint32().at(0));
  EXPECT_EQ(0, message.map_sint64_sint64().at(0));
  EXPECT_EQ(0, message.map_fixed32_fixed32().at(0));
  EXPECT_EQ(0, message.map_fixed64_fixed64().at(0));
  EXPECT_EQ(0, message.map_sfixed32_sfixed32().at(0));
  EXPECT_EQ(0, message.map_sfixed64_sfixed64().at(0));
  EXPECT_EQ(0, message.map_int32_float().at(0));
  EXPECT_EQ(0, message.map_int32_double().at(0));
  EXPECT_EQ(false, message.map_bool_bool().at(0));
  EXPECT_EQ("0", message.map_string_string().at("0"));
  EXPECT_EQ("0", message.map_int32_bytes().at(0));
  EXPECT_EQ(enum_value0, message.map_int32_enum().at(0));
  EXPECT_EQ(0, message.map_int32_foreign_message().at(0).c());
  EXPECT_EQ(0, message.map_int32_foreign_message_no_arena().at(0).c());

  EXPECT_EQ(1, message.map_int32_int32().at(1));
  EXPECT_EQ(1, message.map_int64_int64().at(1));
  EXPECT_EQ(1, message.map_uint32_uint32().at(1));
  EXPECT_EQ(1, message.map_uint64_uint64().at(1));
  EXPECT_EQ(1, message.map_sint32_sint32().at(1));
  EXPECT_EQ(1, message.map_sint64_sint64().at(1));
  EXPECT_EQ(1, message.map_fixed32_fixed32().at(1));
  EXPECT_EQ(1, message.map_fixed64_fixed64().at(1));
  EXPECT_EQ(1, message.map_sfixed32_sfixed32().at(1));
  EXPECT_EQ(1, message.map_sfixed64_sfixed64().at(1));
  EXPECT_EQ(1, message.map_int32_float().at(1));
  EXPECT_EQ(1, message.map_int32_double().at(1));
  EXPECT_EQ(true, message.map_bool_bool().at(1));
  EXPECT_EQ("1", message.map_string_string().at("1"));
  EXPECT_EQ("1", message.map_int32_bytes().at(1));
  EXPECT_EQ(enum_value1, message.map_int32_enum().at(1));
  EXPECT_EQ(1, message.map_int32_foreign_message().at(1).c());
  EXPECT_EQ(1, message.map_int32_foreign_message_no_arena().at(1).c());
}

template <typename EnumType, EnumType enum_value, typename MapMessage>
void MapTestUtilImpl::ExpectMapFieldsSetInitialized(const MapMessage& message) {
  EXPECT_EQ(1, message.map_int32_int32().size());
  EXPECT_EQ(1, message.map_int64_int64().size());
  EXPECT_EQ(1, message.map_uint32_uint32().size());
  EXPECT_EQ(1, message.map_uint64_uint64().size());
  EXPECT_EQ(1, message.map_sint32_sint32().size());
  EXPECT_EQ(1, message.map_sint64_sint64().size());
  EXPECT_EQ(1, message.map_fixed32_fixed32().size());
  EXPECT_EQ(1, message.map_fixed64_fixed64().size());
  EXPECT_EQ(1, message.map_sfixed32_sfixed32().size());
  EXPECT_EQ(1, message.map_sfixed64_sfixed64().size());
  EXPECT_EQ(1, message.map_int32_float().size());
  EXPECT_EQ(1, message.map_int32_double().size());
  EXPECT_EQ(1, message.map_bool_bool().size());
  EXPECT_EQ(1, message.map_string_string().size());
  EXPECT_EQ(1, message.map_int32_bytes().size());
  EXPECT_EQ(1, message.map_int32_enum().size());
  EXPECT_EQ(1, message.map_int32_foreign_message().size());

  EXPECT_EQ(0, message.map_int32_int32().at(0));
  EXPECT_EQ(0, message.map_int64_int64().at(0));
  EXPECT_EQ(0, message.map_uint32_uint32().at(0));
  EXPECT_EQ(0, message.map_uint64_uint64().at(0));
  EXPECT_EQ(0, message.map_sint32_sint32().at(0));
  EXPECT_EQ(0, message.map_sint64_sint64().at(0));
  EXPECT_EQ(0, message.map_fixed32_fixed32().at(0));
  EXPECT_EQ(0, message.map_fixed64_fixed64().at(0));
  EXPECT_EQ(0, message.map_sfixed32_sfixed32().at(0));
  EXPECT_EQ(0, message.map_sfixed64_sfixed64().at(0));
  EXPECT_EQ(0, message.map_int32_float().at(0));
  EXPECT_EQ(0, message.map_int32_double().at(0));
  EXPECT_EQ(false, message.map_bool_bool().at(0));
  EXPECT_EQ("", message.map_string_string().at("0"));
  EXPECT_EQ("", message.map_int32_bytes().at(0));
  EXPECT_EQ(enum_value, message.map_int32_enum().at(0));
  EXPECT_EQ(0, message.map_int32_foreign_message().at(0).ByteSize());
}

template <typename EnumType, EnumType enum_value0, EnumType enum_value1,
          typename MapMessage>
void MapTestUtilImpl::ExpectMapFieldsModified(const MapMessage& message) {
  // ModifyMapFields only sets the second element of each field.  In addition to
  // verifying this, we also verify that the first element and size were *not*
  // modified.
  EXPECT_EQ(2, message.map_int32_int32().size());
  EXPECT_EQ(2, message.map_int64_int64().size());
  EXPECT_EQ(2, message.map_uint32_uint32().size());
  EXPECT_EQ(2, message.map_uint64_uint64().size());
  EXPECT_EQ(2, message.map_sint32_sint32().size());
  EXPECT_EQ(2, message.map_sint64_sint64().size());
  EXPECT_EQ(2, message.map_fixed32_fixed32().size());
  EXPECT_EQ(2, message.map_fixed64_fixed64().size());
  EXPECT_EQ(2, message.map_sfixed32_sfixed32().size());
  EXPECT_EQ(2, message.map_sfixed64_sfixed64().size());
  EXPECT_EQ(2, message.map_int32_float().size());
  EXPECT_EQ(2, message.map_int32_double().size());
  EXPECT_EQ(2, message.map_bool_bool().size());
  EXPECT_EQ(2, message.map_string_string().size());
  EXPECT_EQ(2, message.map_int32_bytes().size());
  EXPECT_EQ(2, message.map_int32_enum().size());
  EXPECT_EQ(2, message.map_int32_foreign_message().size());

  EXPECT_EQ(0, message.map_int32_int32().at(0));
  EXPECT_EQ(0, message.map_int64_int64().at(0));
  EXPECT_EQ(0, message.map_uint32_uint32().at(0));
  EXPECT_EQ(0, message.map_uint64_uint64().at(0));
  EXPECT_EQ(0, message.map_sint32_sint32().at(0));
  EXPECT_EQ(0, message.map_sint64_sint64().at(0));
  EXPECT_EQ(0, message.map_fixed32_fixed32().at(0));
  EXPECT_EQ(0, message.map_fixed64_fixed64().at(0));
  EXPECT_EQ(0, message.map_sfixed32_sfixed32().at(0));
  EXPECT_EQ(0, message.map_sfixed64_sfixed64().at(0));
  EXPECT_EQ(0, message.map_int32_float().at(0));
  EXPECT_EQ(0, message.map_int32_double().at(0));
  EXPECT_EQ(false, message.map_bool_bool().at(0));
  EXPECT_EQ("0", message.map_string_string().at("0"));
  EXPECT_EQ("0", message.map_int32_bytes().at(0));
  EXPECT_EQ(enum_value0, message.map_int32_enum().at(0));
  EXPECT_EQ(0, message.map_int32_foreign_message().at(0).c());

  // Actually verify the second (modified) elements now.
  EXPECT_EQ(2, message.map_int32_int32().at(1));
  EXPECT_EQ(2, message.map_int64_int64().at(1));
  EXPECT_EQ(2, message.map_uint32_uint32().at(1));
  EXPECT_EQ(2, message.map_uint64_uint64().at(1));
  EXPECT_EQ(2, message.map_sint32_sint32().at(1));
  EXPECT_EQ(2, message.map_sint64_sint64().at(1));
  EXPECT_EQ(2, message.map_fixed32_fixed32().at(1));
  EXPECT_EQ(2, message.map_fixed64_fixed64().at(1));
  EXPECT_EQ(2, message.map_sfixed32_sfixed32().at(1));
  EXPECT_EQ(2, message.map_sfixed64_sfixed64().at(1));
  EXPECT_EQ(2, message.map_int32_float().at(1));
  EXPECT_EQ(2, message.map_int32_double().at(1));
  EXPECT_EQ(false, message.map_bool_bool().at(1));
  EXPECT_EQ("2", message.map_string_string().at("1"));
  EXPECT_EQ("2", message.map_int32_bytes().at(1));
  EXPECT_EQ(enum_value1, message.map_int32_enum().at(1));
  EXPECT_EQ(2, message.map_int32_foreign_message().at(1).c());
}

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_MAP_TEST_UTIL_IMPL_H__
