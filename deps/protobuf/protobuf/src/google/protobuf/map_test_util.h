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

#ifndef GOOGLE_PROTOBUF_MAP_TEST_UTIL_H__
#define GOOGLE_PROTOBUF_MAP_TEST_UTIL_H__

#include <google/protobuf/map_unittest.pb.h>

namespace google {
namespace protobuf {

namespace unittest = ::protobuf_unittest;

class MapTestUtil {
 public:
  // Set every field in the TestMap message to a unique value.
  static void SetMapFields(unittest::TestMap* message);

  // Set every field in the TestArenaMap message to a unique value.
  static void SetArenaMapFields(unittest::TestArenaMap* message);

  // Set every field in the message to a default value.
  static void SetMapFieldsInitialized(unittest::TestMap* message);

  // Modify all the map fields of the message (which should already have been
  // initialized with SetMapFields()).
  static void ModifyMapFields(unittest::TestMap* message);

  // Check that all fields have the values that they should have after
  // SetMapFields() is called.
  static void ExpectMapFieldsSet(const unittest::TestMap& message);

  // Check that all fields have the values that they should have after
  // SetMapFields() is called for TestArenaMap.
  static void ExpectArenaMapFieldsSet(const unittest::TestArenaMap& message);

  // Check that all fields have the values that they should have after
  // SetMapFieldsInitialized() is called.
  static void ExpectMapFieldsSetInitialized(const unittest::TestMap& message);

  // Expect that the message is modified as would be expected from
  // ModifyMapFields().
  static void ExpectMapFieldsModified(const unittest::TestMap& message);

  // Check that all fields are empty.
  static void ExpectClear(const unittest::TestMap& message);

  // Check that all map fields have the given size.
  static void ExpectMapsSize(const unittest::TestMap& message, int size);

  // Get pointers of map entries at given index.
  static std::vector<const Message*> GetMapEntries(
      const unittest::TestMap& message, int index);

  // Get pointers of map entries from release.
  static std::vector<const Message*> GetMapEntriesFromRelease(
      unittest::TestMap* message);
};

// Like above, but use the reflection interface.
class MapReflectionTester {
 public:
  // base_descriptor must be a descriptor for TestMap, which is used for
  // MapReflectionTester to fetch the FieldDescriptors needed to use the
  // reflection interface.
  explicit MapReflectionTester(const Descriptor* base_descriptor);

  void SetMapFieldsViaReflection(Message* message);
  void SetMapFieldsViaMapReflection(Message* message);
  void ClearMapFieldsViaReflection(Message* message);
  void ModifyMapFieldsViaReflection(Message* message);
  void RemoveLastMapsViaReflection(Message* message);
  void ReleaseLastMapsViaReflection(Message* message);
  void SwapMapsViaReflection(Message* message);
  void MutableUnknownFieldsOfMapFieldsViaReflection(Message* message);
  void ExpectMapFieldsSetViaReflection(const Message& message);
  void ExpectMapFieldsSetViaReflectionIterator(Message* message);
  void ExpectClearViaReflection(const Message& message);
  void ExpectClearViaReflectionIterator(Message* message);
  void GetMapValueViaMapReflection(Message* message,
                                   const std::string& field_name,
                                   const MapKey& map_key, MapValueRef* map_val);
  Message* GetMapEntryViaReflection(Message* message,
                                    const std::string& field_name, int index);
  MapIterator MapBegin(Message* message, const std::string& field_name);
  MapIterator MapEnd(Message* message, const std::string& field_name);

 private:
  const FieldDescriptor* F(const std::string& name);

  const Descriptor* base_descriptor_;

  const EnumValueDescriptor* map_enum_bar_;
  const EnumValueDescriptor* map_enum_baz_;
  const EnumValueDescriptor* map_enum_foo_;

  const FieldDescriptor* foreign_c_;
  const FieldDescriptor* map_int32_int32_key_;
  const FieldDescriptor* map_int32_int32_val_;
  const FieldDescriptor* map_int64_int64_key_;
  const FieldDescriptor* map_int64_int64_val_;
  const FieldDescriptor* map_uint32_uint32_key_;
  const FieldDescriptor* map_uint32_uint32_val_;
  const FieldDescriptor* map_uint64_uint64_key_;
  const FieldDescriptor* map_uint64_uint64_val_;
  const FieldDescriptor* map_sint32_sint32_key_;
  const FieldDescriptor* map_sint32_sint32_val_;
  const FieldDescriptor* map_sint64_sint64_key_;
  const FieldDescriptor* map_sint64_sint64_val_;
  const FieldDescriptor* map_fixed32_fixed32_key_;
  const FieldDescriptor* map_fixed32_fixed32_val_;
  const FieldDescriptor* map_fixed64_fixed64_key_;
  const FieldDescriptor* map_fixed64_fixed64_val_;
  const FieldDescriptor* map_sfixed32_sfixed32_key_;
  const FieldDescriptor* map_sfixed32_sfixed32_val_;
  const FieldDescriptor* map_sfixed64_sfixed64_key_;
  const FieldDescriptor* map_sfixed64_sfixed64_val_;
  const FieldDescriptor* map_int32_float_key_;
  const FieldDescriptor* map_int32_float_val_;
  const FieldDescriptor* map_int32_double_key_;
  const FieldDescriptor* map_int32_double_val_;
  const FieldDescriptor* map_bool_bool_key_;
  const FieldDescriptor* map_bool_bool_val_;
  const FieldDescriptor* map_string_string_key_;
  const FieldDescriptor* map_string_string_val_;
  const FieldDescriptor* map_int32_bytes_key_;
  const FieldDescriptor* map_int32_bytes_val_;
  const FieldDescriptor* map_int32_enum_key_;
  const FieldDescriptor* map_int32_enum_val_;
  const FieldDescriptor* map_int32_foreign_message_key_;
  const FieldDescriptor* map_int32_foreign_message_val_;
};

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_MAP_TEST_UTIL_H__
