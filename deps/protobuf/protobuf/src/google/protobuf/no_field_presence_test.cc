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

#include <string>

#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_no_field_presence.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {

// Helper: checks that all fields have default (zero/empty) values.
void CheckDefaultValues(
    const proto2_nofieldpresence_unittest::TestAllTypes& m) {
  EXPECT_EQ(0, m.optional_int32());
  EXPECT_EQ(0, m.optional_int64());
  EXPECT_EQ(0, m.optional_uint32());
  EXPECT_EQ(0, m.optional_uint64());
  EXPECT_EQ(0, m.optional_sint32());
  EXPECT_EQ(0, m.optional_sint64());
  EXPECT_EQ(0, m.optional_fixed32());
  EXPECT_EQ(0, m.optional_fixed64());
  EXPECT_EQ(0, m.optional_sfixed32());
  EXPECT_EQ(0, m.optional_sfixed64());
  EXPECT_EQ(0, m.optional_float());
  EXPECT_EQ(0, m.optional_double());
  EXPECT_EQ(false, m.optional_bool());
  EXPECT_EQ(0, m.optional_string().size());
  EXPECT_EQ(0, m.optional_bytes().size());

  EXPECT_EQ(false, m.has_optional_nested_message());
  // accessor for message fields returns default instance when not present
  EXPECT_EQ(0, m.optional_nested_message().bb());
  EXPECT_EQ(false, m.has_optional_proto2_message());
  // Embedded proto2 messages still have proto2 semantics, e.g. non-zero default
  // values. Here the submessage is not present but its accessor returns the
  // default instance.
  EXPECT_EQ(41, m.optional_proto2_message().default_int32());
  EXPECT_EQ(false, m.has_optional_foreign_message());
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes_NestedEnum_FOO,
            m.optional_nested_enum());
  EXPECT_EQ(proto2_nofieldpresence_unittest::FOREIGN_FOO,
            m.optional_foreign_enum());


  EXPECT_EQ(0, m.repeated_int32_size());
  EXPECT_EQ(0, m.repeated_int64_size());
  EXPECT_EQ(0, m.repeated_uint32_size());
  EXPECT_EQ(0, m.repeated_uint64_size());
  EXPECT_EQ(0, m.repeated_sint32_size());
  EXPECT_EQ(0, m.repeated_sint64_size());
  EXPECT_EQ(0, m.repeated_fixed32_size());
  EXPECT_EQ(0, m.repeated_fixed64_size());
  EXPECT_EQ(0, m.repeated_sfixed32_size());
  EXPECT_EQ(0, m.repeated_sfixed64_size());
  EXPECT_EQ(0, m.repeated_float_size());
  EXPECT_EQ(0, m.repeated_double_size());
  EXPECT_EQ(0, m.repeated_bool_size());
  EXPECT_EQ(0, m.repeated_string_size());
  EXPECT_EQ(0, m.repeated_bytes_size());
  EXPECT_EQ(0, m.repeated_nested_message_size());
  EXPECT_EQ(0, m.repeated_foreign_message_size());
  EXPECT_EQ(0, m.repeated_proto2_message_size());
  EXPECT_EQ(0, m.repeated_nested_enum_size());
  EXPECT_EQ(0, m.repeated_foreign_enum_size());
  EXPECT_EQ(0, m.repeated_lazy_message_size());
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes::ONEOF_FIELD_NOT_SET,
            m.oneof_field_case());
}

void FillValues(proto2_nofieldpresence_unittest::TestAllTypes* m) {
  m->set_optional_int32(100);
  m->set_optional_int64(101);
  m->set_optional_uint32(102);
  m->set_optional_uint64(103);
  m->set_optional_sint32(104);
  m->set_optional_sint64(105);
  m->set_optional_fixed32(106);
  m->set_optional_fixed64(107);
  m->set_optional_sfixed32(108);
  m->set_optional_sfixed64(109);
  m->set_optional_float(110.0);
  m->set_optional_double(111.0);
  m->set_optional_bool(true);
  m->set_optional_string("asdf");
  m->set_optional_bytes("jkl;");
  m->mutable_optional_nested_message()->set_bb(42);
  m->mutable_optional_foreign_message()->set_c(43);
  m->mutable_optional_proto2_message()->set_optional_int32(44);
  m->set_optional_nested_enum(
      proto2_nofieldpresence_unittest::TestAllTypes_NestedEnum_BAZ);
  m->set_optional_foreign_enum(proto2_nofieldpresence_unittest::FOREIGN_BAZ);
  m->mutable_optional_lazy_message()->set_bb(45);
  m->add_repeated_int32(100);
  m->add_repeated_int64(101);
  m->add_repeated_uint32(102);
  m->add_repeated_uint64(103);
  m->add_repeated_sint32(104);
  m->add_repeated_sint64(105);
  m->add_repeated_fixed32(106);
  m->add_repeated_fixed64(107);
  m->add_repeated_sfixed32(108);
  m->add_repeated_sfixed64(109);
  m->add_repeated_float(110.0);
  m->add_repeated_double(111.0);
  m->add_repeated_bool(true);
  m->add_repeated_string("asdf");
  m->add_repeated_bytes("jkl;");
  m->add_repeated_nested_message()->set_bb(46);
  m->add_repeated_foreign_message()->set_c(47);
  m->add_repeated_proto2_message()->set_optional_int32(48);
  m->add_repeated_nested_enum(
      proto2_nofieldpresence_unittest::TestAllTypes_NestedEnum_BAZ);
  m->add_repeated_foreign_enum(proto2_nofieldpresence_unittest::FOREIGN_BAZ);
  m->add_repeated_lazy_message()->set_bb(49);

  m->set_oneof_uint32(1);
  m->mutable_oneof_nested_message()->set_bb(50);
  m->set_oneof_string("test");  // only this one remains set
}

void CheckNonDefaultValues(
    const proto2_nofieldpresence_unittest::TestAllTypes& m) {
  EXPECT_EQ(100, m.optional_int32());
  EXPECT_EQ(101, m.optional_int64());
  EXPECT_EQ(102, m.optional_uint32());
  EXPECT_EQ(103, m.optional_uint64());
  EXPECT_EQ(104, m.optional_sint32());
  EXPECT_EQ(105, m.optional_sint64());
  EXPECT_EQ(106, m.optional_fixed32());
  EXPECT_EQ(107, m.optional_fixed64());
  EXPECT_EQ(108, m.optional_sfixed32());
  EXPECT_EQ(109, m.optional_sfixed64());
  EXPECT_EQ(110.0, m.optional_float());
  EXPECT_EQ(111.0, m.optional_double());
  EXPECT_EQ(true, m.optional_bool());
  EXPECT_EQ("asdf", m.optional_string());
  EXPECT_EQ("jkl;", m.optional_bytes());
  EXPECT_EQ(true, m.has_optional_nested_message());
  EXPECT_EQ(42, m.optional_nested_message().bb());
  EXPECT_EQ(true, m.has_optional_foreign_message());
  EXPECT_EQ(43, m.optional_foreign_message().c());
  EXPECT_EQ(true, m.has_optional_proto2_message());
  EXPECT_EQ(44, m.optional_proto2_message().optional_int32());
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes_NestedEnum_BAZ,
            m.optional_nested_enum());
  EXPECT_EQ(proto2_nofieldpresence_unittest::FOREIGN_BAZ,
            m.optional_foreign_enum());
  EXPECT_EQ(true, m.has_optional_lazy_message());
  EXPECT_EQ(45, m.optional_lazy_message().bb());

  EXPECT_EQ(1, m.repeated_int32_size());
  EXPECT_EQ(100, m.repeated_int32(0));
  EXPECT_EQ(1, m.repeated_int64_size());
  EXPECT_EQ(101, m.repeated_int64(0));
  EXPECT_EQ(1, m.repeated_uint32_size());
  EXPECT_EQ(102, m.repeated_uint32(0));
  EXPECT_EQ(1, m.repeated_uint64_size());
  EXPECT_EQ(103, m.repeated_uint64(0));
  EXPECT_EQ(1, m.repeated_sint32_size());
  EXPECT_EQ(104, m.repeated_sint32(0));
  EXPECT_EQ(1, m.repeated_sint64_size());
  EXPECT_EQ(105, m.repeated_sint64(0));
  EXPECT_EQ(1, m.repeated_fixed32_size());
  EXPECT_EQ(106, m.repeated_fixed32(0));
  EXPECT_EQ(1, m.repeated_fixed64_size());
  EXPECT_EQ(107, m.repeated_fixed64(0));
  EXPECT_EQ(1, m.repeated_sfixed32_size());
  EXPECT_EQ(108, m.repeated_sfixed32(0));
  EXPECT_EQ(1, m.repeated_sfixed64_size());
  EXPECT_EQ(109, m.repeated_sfixed64(0));
  EXPECT_EQ(1, m.repeated_float_size());
  EXPECT_EQ(110.0, m.repeated_float(0));
  EXPECT_EQ(1, m.repeated_double_size());
  EXPECT_EQ(111.0, m.repeated_double(0));
  EXPECT_EQ(1, m.repeated_bool_size());
  EXPECT_EQ(true, m.repeated_bool(0));
  EXPECT_EQ(1, m.repeated_string_size());
  EXPECT_EQ("asdf", m.repeated_string(0));
  EXPECT_EQ(1, m.repeated_bytes_size());
  EXPECT_EQ("jkl;", m.repeated_bytes(0));
  EXPECT_EQ(1, m.repeated_nested_message_size());
  EXPECT_EQ(46, m.repeated_nested_message(0).bb());
  EXPECT_EQ(1, m.repeated_foreign_message_size());
  EXPECT_EQ(47, m.repeated_foreign_message(0).c());
  EXPECT_EQ(1, m.repeated_proto2_message_size());
  EXPECT_EQ(48, m.repeated_proto2_message(0).optional_int32());
  EXPECT_EQ(1, m.repeated_nested_enum_size());
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes_NestedEnum_BAZ,
            m.repeated_nested_enum(0));
  EXPECT_EQ(1, m.repeated_foreign_enum_size());
  EXPECT_EQ(proto2_nofieldpresence_unittest::FOREIGN_BAZ,
            m.repeated_foreign_enum(0));
  EXPECT_EQ(1, m.repeated_lazy_message_size());
  EXPECT_EQ(49, m.repeated_lazy_message(0).bb());

  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes::kOneofString,
            m.oneof_field_case());
  EXPECT_EQ("test", m.oneof_string());
}

TEST(NoFieldPresenceTest, BasicMessageTest) {
  proto2_nofieldpresence_unittest::TestAllTypes message;
  // Check default values, fill all fields, check values. We just want to
  // exercise the basic getters/setter paths here to make sure no
  // field-presence-related changes broke these.
  CheckDefaultValues(message);
  FillValues(&message);
  CheckNonDefaultValues(message);

  // Clear() should be equivalent to getting a freshly-constructed message.
  message.Clear();
  CheckDefaultValues(message);
}

TEST(NoFieldPresenceTest, MessageFieldPresenceTest) {
  // check that presence still works properly for message fields.
  proto2_nofieldpresence_unittest::TestAllTypes message;
  EXPECT_EQ(false, message.has_optional_nested_message());
  // Getter should fetch default instance, and not cause the field to become
  // present.
  EXPECT_EQ(0, message.optional_nested_message().bb());
  EXPECT_EQ(false, message.has_optional_nested_message());
  message.mutable_optional_nested_message()->set_bb(42);
  EXPECT_EQ(true, message.has_optional_nested_message());
  message.clear_optional_nested_message();
  EXPECT_EQ(false, message.has_optional_nested_message());

  // Likewise for a lazy message field.
  EXPECT_EQ(false, message.has_optional_lazy_message());
  // Getter should fetch default instance, and not cause the field to become
  // present.
  EXPECT_EQ(0, message.optional_lazy_message().bb());
  EXPECT_EQ(false, message.has_optional_lazy_message());
  message.mutable_optional_lazy_message()->set_bb(42);
  EXPECT_EQ(true, message.has_optional_lazy_message());
  message.clear_optional_lazy_message();
  EXPECT_EQ(false, message.has_optional_lazy_message());

  // Test field presence of a message field on the default instance.
  EXPECT_EQ(false,
            proto2_nofieldpresence_unittest::TestAllTypes::default_instance()
                .has_optional_nested_message());
}

TEST(NoFieldPresenceTest, ReflectionHasFieldTest) {
  // check that HasField reports true on all scalar fields. Check that it
  // behaves properly for message fields.

  proto2_nofieldpresence_unittest::TestAllTypes message;
  const Reflection* r = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();

  // Check initial state: scalars not present (due to need to be consistent with
  // MergeFrom()), message fields not present, oneofs not present.
  for (int i = 0; i < desc->field_count(); i++) {
    const FieldDescriptor* field = desc->field(i);
    if (field->is_repeated()) continue;
    EXPECT_EQ(false, r->HasField(message, field));
  }

  // Test field presence of a message field on the default instance.
  const FieldDescriptor* msg_field =
      desc->FindFieldByName("optional_nested_message");
  EXPECT_EQ(
      false,
      r->HasField(
          proto2_nofieldpresence_unittest::TestAllTypes::default_instance(),
          msg_field));

  // Fill all fields, expect everything to report true (check oneofs below).
  FillValues(&message);
  for (int i = 0; i < desc->field_count(); i++) {
    const FieldDescriptor* field = desc->field(i);
    if (field->is_repeated() || field->containing_oneof()) {
      continue;
    }
    if (field->options().ctype() != FieldOptions::STRING) {
      continue;
    }
    EXPECT_EQ(true, r->HasField(message, field));
  }

  message.Clear();

  // Check zero/empty-means-not-present semantics.
  const FieldDescriptor* field_int32 = desc->FindFieldByName("optional_int32");
  const FieldDescriptor* field_double =
      desc->FindFieldByName("optional_double");
  const FieldDescriptor* field_string =
      desc->FindFieldByName("optional_string");

  EXPECT_EQ(false, r->HasField(message, field_int32));
  EXPECT_EQ(false, r->HasField(message, field_double));
  EXPECT_EQ(false, r->HasField(message, field_string));

  message.set_optional_int32(42);
  EXPECT_EQ(true, r->HasField(message, field_int32));
  message.set_optional_int32(0);
  EXPECT_EQ(false, r->HasField(message, field_int32));

  message.set_optional_double(42.0);
  EXPECT_EQ(true, r->HasField(message, field_double));
  message.set_optional_double(0.0);
  EXPECT_EQ(false, r->HasField(message, field_double));

  message.set_optional_string("test");
  EXPECT_EQ(true, r->HasField(message, field_string));
  message.set_optional_string("");
  EXPECT_EQ(false, r->HasField(message, field_string));
}

TEST(NoFieldPresenceTest, ReflectionClearFieldTest) {
  proto2_nofieldpresence_unittest::TestAllTypes message;

  const Reflection* r = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();

  const FieldDescriptor* field_int32 = desc->FindFieldByName("optional_int32");
  const FieldDescriptor* field_double =
      desc->FindFieldByName("optional_double");
  const FieldDescriptor* field_string =
      desc->FindFieldByName("optional_string");
  const FieldDescriptor* field_message =
      desc->FindFieldByName("optional_nested_message");
  const FieldDescriptor* field_lazy =
      desc->FindFieldByName("optional_lazy_message");

  message.set_optional_int32(42);
  r->ClearField(&message, field_int32);
  EXPECT_EQ(0, message.optional_int32());

  message.set_optional_double(42.0);
  r->ClearField(&message, field_double);
  EXPECT_EQ(0.0, message.optional_double());

  message.set_optional_string("test");
  r->ClearField(&message, field_string);
  EXPECT_EQ("", message.optional_string());

  message.mutable_optional_nested_message()->set_bb(1234);
  r->ClearField(&message, field_message);
  EXPECT_FALSE(message.has_optional_nested_message());
  EXPECT_EQ(0, message.optional_nested_message().bb());

  message.mutable_optional_lazy_message()->set_bb(42);
  r->ClearField(&message, field_lazy);
  EXPECT_FALSE(message.has_optional_lazy_message());
  EXPECT_EQ(0, message.optional_lazy_message().bb());
}

TEST(NoFieldPresenceTest, HasFieldOneofsTest) {
  // check that HasField behaves properly for oneofs.
  proto2_nofieldpresence_unittest::TestAllTypes message;

  const Reflection* r = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();
  const FieldDescriptor* desc_oneof_uint32 =
      desc->FindFieldByName("oneof_uint32");
  const FieldDescriptor* desc_oneof_nested_message =
      desc->FindFieldByName("oneof_nested_message");
  const FieldDescriptor* desc_oneof_string =
      desc->FindFieldByName("oneof_string");
  GOOGLE_CHECK(desc_oneof_uint32 != nullptr);
  GOOGLE_CHECK(desc_oneof_nested_message != nullptr);
  GOOGLE_CHECK(desc_oneof_string != nullptr);

  EXPECT_EQ(false, r->HasField(message, desc_oneof_uint32));
  EXPECT_EQ(false, r->HasField(message, desc_oneof_nested_message));
  EXPECT_EQ(false, r->HasField(message, desc_oneof_string));

  message.set_oneof_string("test");
  EXPECT_EQ(false, r->HasField(message, desc_oneof_uint32));
  EXPECT_EQ(false, r->HasField(message, desc_oneof_nested_message));
  EXPECT_EQ(true, r->HasField(message, desc_oneof_string));
  message.mutable_oneof_nested_message()->set_bb(42);
  EXPECT_EQ(false, r->HasField(message, desc_oneof_uint32));
  EXPECT_EQ(true, r->HasField(message, desc_oneof_nested_message));
  EXPECT_EQ(false, r->HasField(message, desc_oneof_string));

  message.Clear();
  EXPECT_EQ(false, r->HasField(message, desc_oneof_uint32));
  EXPECT_EQ(false, r->HasField(message, desc_oneof_nested_message));
  EXPECT_EQ(false, r->HasField(message, desc_oneof_string));
}

TEST(NoFieldPresenceTest, DontSerializeDefaultValuesTest) {
  // check that serialized data contains only non-zero numeric fields/non-empty
  // string/byte fields.
  proto2_nofieldpresence_unittest::TestAllTypes message;
  std::string output;

  // All default values -> no output.
  message.SerializeToString(&output);
  EXPECT_EQ(0, output.size());

  // Zero values -> still no output.
  message.set_optional_int32(0);
  message.set_optional_int64(0);
  message.set_optional_uint32(0);
  message.set_optional_uint64(0);
  message.set_optional_sint32(0);
  message.set_optional_sint64(0);
  message.set_optional_fixed32(0);
  message.set_optional_fixed64(0);
  message.set_optional_sfixed32(0);
  message.set_optional_sfixed64(0);
  message.set_optional_float(0);
  message.set_optional_double(0);
  message.set_optional_bool(0);
  message.set_optional_string("");
  message.set_optional_bytes("");
  message.set_optional_nested_enum(
      proto2_nofieldpresence_unittest::
          TestAllTypes_NestedEnum_FOO);  // first enum entry
  message.set_optional_foreign_enum(
      proto2_nofieldpresence_unittest::FOREIGN_FOO);  // first enum entry

  message.SerializeToString(&output);
  EXPECT_EQ(0, output.size());

  message.set_optional_int32(1);
  message.SerializeToString(&output);
  EXPECT_EQ(2, output.size());
  EXPECT_EQ("\x08\x01", output);

  message.set_optional_int32(0);
  message.SerializeToString(&output);
  EXPECT_EQ(0, output.size());
}

TEST(NoFieldPresenceTest, MergeFromIfNonzeroTest) {
  // check that MergeFrom copies if nonzero/nondefault only.
  proto2_nofieldpresence_unittest::TestAllTypes source;
  proto2_nofieldpresence_unittest::TestAllTypes dest;

  dest.set_optional_int32(42);
  dest.set_optional_string("test");
  source.set_optional_int32(0);
  source.set_optional_string("");
  // MergeFrom() copies only if present in serialization, i.e., non-zero.
  dest.MergeFrom(source);
  EXPECT_EQ(42, dest.optional_int32());
  EXPECT_EQ("test", dest.optional_string());

  source.set_optional_int32(84);
  source.set_optional_string("test2");
  dest.MergeFrom(source);
  EXPECT_EQ(84, dest.optional_int32());
  EXPECT_EQ("test2", dest.optional_string());
}

TEST(NoFieldPresenceTest, IsInitializedTest) {
  // Check that IsInitialized works properly.
  proto2_nofieldpresence_unittest::TestProto2Required message;

  EXPECT_EQ(true, message.IsInitialized());
  message.mutable_proto2()->set_a(1);
  EXPECT_EQ(false, message.IsInitialized());
  message.mutable_proto2()->set_b(1);
  EXPECT_EQ(false, message.IsInitialized());
  message.mutable_proto2()->set_c(1);
  EXPECT_EQ(true, message.IsInitialized());
}

TEST(NoFieldPresenceTest, LazyMessageFieldHasBit) {
  // Check that has-bit interaction with lazy message works (has-bit before and
  // after lazy decode).
  proto2_nofieldpresence_unittest::TestAllTypes message;
  const Reflection* r = message.GetReflection();
  const Descriptor* desc = message.GetDescriptor();
  const FieldDescriptor* field = desc->FindFieldByName("optional_lazy_message");
  GOOGLE_CHECK(field != nullptr);

  EXPECT_EQ(false, message.has_optional_lazy_message());
  EXPECT_EQ(false, r->HasField(message, field));

  message.mutable_optional_lazy_message()->set_bb(42);
  EXPECT_EQ(true, message.has_optional_lazy_message());
  EXPECT_EQ(true, r->HasField(message, field));

  // Serialize and parse with a new message object so that lazy field on new
  // object is in unparsed state.
  std::string output;
  message.SerializeToString(&output);
  proto2_nofieldpresence_unittest::TestAllTypes message2;
  message2.ParseFromString(output);

  EXPECT_EQ(true, message2.has_optional_lazy_message());
  EXPECT_EQ(true, r->HasField(message2, field));

  // Access field to force lazy parse.
  EXPECT_EQ(42, message.optional_lazy_message().bb());
  EXPECT_EQ(true, message2.has_optional_lazy_message());
  EXPECT_EQ(true, r->HasField(message2, field));
}

TEST(NoFieldPresenceTest, OneofPresence) {
  proto2_nofieldpresence_unittest::TestAllTypes message;
  // oneof fields still have field presence -- ensure that this goes on the wire
  // even though its value is the empty string.
  message.set_oneof_string("");
  std::string serialized;
  message.SerializeToString(&serialized);
  // Tag: 113 --> tag is (113 << 3) | 2 (length delimited) = 906
  // varint: 0x8a 0x07
  // Length: 0x00
  EXPECT_EQ(3, serialized.size());
  EXPECT_EQ(static_cast<char>(0x8a), serialized.at(0));
  EXPECT_EQ(static_cast<char>(0x07), serialized.at(1));
  EXPECT_EQ(static_cast<char>(0x00), serialized.at(2));

  message.Clear();
  EXPECT_TRUE(message.ParseFromString(serialized));
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes::kOneofString,
            message.oneof_field_case());

  // Also test int32 and enum fields.
  message.Clear();
  message.set_oneof_uint32(0);  // would not go on wire if ordinary field.
  message.SerializeToString(&serialized);
  EXPECT_EQ(3, serialized.size());
  EXPECT_TRUE(message.ParseFromString(serialized));
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes::kOneofUint32,
            message.oneof_field_case());

  message.Clear();
  message.set_oneof_enum(
      proto2_nofieldpresence_unittest::TestAllTypes_NestedEnum_FOO);  // default
                                                                      // value.
  message.SerializeToString(&serialized);
  EXPECT_EQ(3, serialized.size());
  EXPECT_TRUE(message.ParseFromString(serialized));
  EXPECT_EQ(proto2_nofieldpresence_unittest::TestAllTypes::kOneofEnum,
            message.oneof_field_case());

  message.Clear();
  message.set_oneof_string("test");
  message.clear_oneof_string();
  EXPECT_EQ(0, message.ByteSize());
}

}  // namespace
}  // namespace protobuf
}  // namespace google
