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

#ifndef GOOGLE_PROTOBUF_TEST_UTIL_H__
#define GOOGLE_PROTOBUF_TEST_UTIL_H__

#include <google/protobuf/unittest.pb.h>

#define UNITTEST ::protobuf_unittest
#define UNITTEST_IMPORT ::protobuf_unittest_import
// Must be included when the preprocessor symbols above are defined.
#include <google/protobuf/test_util.inc>
#undef UNITTEST
#undef UNITTEST_IMPORT

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
// This file doesn't use these declarations, but some .cc files do.
namespace unittest = ::protobuf_unittest;
namespace unittest_import = ::protobuf_unittest_import;

namespace TestUtil {

class ReflectionTester {
 public:
  // base_descriptor must be a descriptor for TestAllTypes or
  // TestAllExtensions.  In the former case, ReflectionTester fetches from
  // it the FieldDescriptors needed to use the reflection interface.  In
  // the latter case, ReflectionTester searches for extension fields in
  // its file.
  explicit ReflectionTester(const Descriptor* base_descriptor);

  void SetAllFieldsViaReflection(Message* message);
  void ModifyRepeatedFieldsViaReflection(Message* message);
  void ExpectAllFieldsSetViaReflection(const Message& message);
  void ExpectClearViaReflection(const Message& message);

  void SetPackedFieldsViaReflection(Message* message);
  void ExpectPackedFieldsSetViaReflection(const Message& message);

  void RemoveLastRepeatedsViaReflection(Message* message);
  void ReleaseLastRepeatedsViaReflection(Message* message,
                                         bool expect_extensions_notnull);
  void SwapRepeatedsViaReflection(Message* message);
  void SetAllocatedOptionalMessageFieldsToNullViaReflection(Message* message);
  static void SetAllocatedOptionalMessageFieldsToMessageViaReflection(
      Message* from_message, Message* to_message);

  enum MessageReleaseState {
    IS_NULL,
    CAN_BE_NULL,
    NOT_NULL,
  };
  void ExpectMessagesReleasedViaReflection(
      Message* message, MessageReleaseState expected_release_state);

  // Set and check functions for TestOneof2 messages. No need to construct
  // the ReflectionTester by TestAllTypes nor TestAllExtensions.
  static void SetOneofViaReflection(Message* message);
  static void ExpectOneofSetViaReflection(const Message& message);

 private:
  const FieldDescriptor* F(const std::string& name);

  const Descriptor* base_descriptor_;

  const FieldDescriptor* group_a_;
  const FieldDescriptor* repeated_group_a_;
  const FieldDescriptor* nested_b_;
  const FieldDescriptor* foreign_c_;
  const FieldDescriptor* import_d_;
  const FieldDescriptor* import_e_;

  const EnumValueDescriptor* nested_foo_;
  const EnumValueDescriptor* nested_bar_;
  const EnumValueDescriptor* nested_baz_;
  const EnumValueDescriptor* foreign_foo_;
  const EnumValueDescriptor* foreign_bar_;
  const EnumValueDescriptor* foreign_baz_;
  const EnumValueDescriptor* import_foo_;
  const EnumValueDescriptor* import_bar_;
  const EnumValueDescriptor* import_baz_;

  // We have to split this into three function otherwise it creates a stack
  // frame so large that it triggers a warning.
  void ExpectAllFieldsSetViaReflection1(const Message& message);
  void ExpectAllFieldsSetViaReflection2(const Message& message);
  void ExpectAllFieldsSetViaReflection3(const Message& message);

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ReflectionTester);
};

inline TestUtil::ReflectionTester::ReflectionTester(
    const Descriptor* base_descriptor)
    : base_descriptor_(base_descriptor) {
  const DescriptorPool* pool = base_descriptor->file()->pool();
  std::string package = base_descriptor->file()->package();
  const FieldDescriptor* import_descriptor =
      pool->FindFieldByName(package + ".TestAllTypes.optional_import_message");
  std::string import_package =
      import_descriptor->message_type()->file()->package();

  nested_b_ = pool->FindFieldByName(package + ".TestAllTypes.NestedMessage.bb");
  foreign_c_ = pool->FindFieldByName(package + ".ForeignMessage.c");
  import_d_ = pool->FindFieldByName(import_package + ".ImportMessage.d");
  import_e_ = pool->FindFieldByName(import_package + ".PublicImportMessage.e");
  nested_foo_ = pool->FindEnumValueByName(package + ".TestAllTypes.FOO");
  nested_bar_ = pool->FindEnumValueByName(package + ".TestAllTypes.BAR");
  nested_baz_ = pool->FindEnumValueByName(package + ".TestAllTypes.BAZ");
  foreign_foo_ = pool->FindEnumValueByName(package + ".FOREIGN_FOO");
  foreign_bar_ = pool->FindEnumValueByName(package + ".FOREIGN_BAR");
  foreign_baz_ = pool->FindEnumValueByName(package + ".FOREIGN_BAZ");
  import_foo_ = pool->FindEnumValueByName(import_package + ".IMPORT_FOO");
  import_bar_ = pool->FindEnumValueByName(import_package + ".IMPORT_BAR");
  import_baz_ = pool->FindEnumValueByName(import_package + ".IMPORT_BAZ");

  if (base_descriptor_->name() == "TestAllExtensions") {
    group_a_ = pool->FindFieldByName(package + ".OptionalGroup_extension.a");
    repeated_group_a_ =
        pool->FindFieldByName(package + ".RepeatedGroup_extension.a");
  } else {
    group_a_ = pool->FindFieldByName(package + ".TestAllTypes.OptionalGroup.a");
    repeated_group_a_ =
        pool->FindFieldByName(package + ".TestAllTypes.RepeatedGroup.a");
  }

  EXPECT_TRUE(group_a_ != nullptr);
  EXPECT_TRUE(repeated_group_a_ != nullptr);
  EXPECT_TRUE(nested_b_ != nullptr);
  EXPECT_TRUE(foreign_c_ != nullptr);
  EXPECT_TRUE(import_d_ != nullptr);
  EXPECT_TRUE(import_e_ != nullptr);
  EXPECT_TRUE(nested_foo_ != nullptr);
  EXPECT_TRUE(nested_bar_ != nullptr);
  EXPECT_TRUE(nested_baz_ != nullptr);
  EXPECT_TRUE(foreign_foo_ != nullptr);
  EXPECT_TRUE(foreign_bar_ != nullptr);
  EXPECT_TRUE(foreign_baz_ != nullptr);
  EXPECT_TRUE(import_foo_ != nullptr);
  EXPECT_TRUE(import_bar_ != nullptr);
  EXPECT_TRUE(import_baz_ != nullptr);
}

// Shorthand to get a FieldDescriptor for a field of TestAllTypes.
inline const FieldDescriptor* TestUtil::ReflectionTester::F(
    const std::string& name) {
  const FieldDescriptor* result = nullptr;
  if (base_descriptor_->name() == "TestAllExtensions" ||
      base_descriptor_->name() == "TestPackedExtensions") {
    result = base_descriptor_->file()->FindExtensionByName(name + "_extension");
  } else {
    result = base_descriptor_->FindFieldByName(name);
  }
  GOOGLE_CHECK(result != nullptr);
  return result;
}

// -------------------------------------------------------------------

inline void TestUtil::ReflectionTester::SetAllFieldsViaReflection(
    Message* message) {
  const Reflection* reflection = message->GetReflection();
  Message* sub_message;

  reflection->SetInt32(message, F("optional_int32"), 101);
  reflection->SetInt64(message, F("optional_int64"), 102);
  reflection->SetUInt32(message, F("optional_uint32"), 103);
  reflection->SetUInt64(message, F("optional_uint64"), 104);
  reflection->SetInt32(message, F("optional_sint32"), 105);
  reflection->SetInt64(message, F("optional_sint64"), 106);
  reflection->SetUInt32(message, F("optional_fixed32"), 107);
  reflection->SetUInt64(message, F("optional_fixed64"), 108);
  reflection->SetInt32(message, F("optional_sfixed32"), 109);
  reflection->SetInt64(message, F("optional_sfixed64"), 110);
  reflection->SetFloat(message, F("optional_float"), 111);
  reflection->SetDouble(message, F("optional_double"), 112);
  reflection->SetBool(message, F("optional_bool"), true);
  reflection->SetString(message, F("optional_string"), "115");
  reflection->SetString(message, F("optional_bytes"), "116");

  sub_message = reflection->MutableMessage(message, F("optionalgroup"));
  sub_message->GetReflection()->SetInt32(sub_message, group_a_, 117);
  sub_message =
      reflection->MutableMessage(message, F("optional_nested_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 118);
  sub_message =
      reflection->MutableMessage(message, F("optional_foreign_message"));
  sub_message->GetReflection()->SetInt32(sub_message, foreign_c_, 119);
  sub_message =
      reflection->MutableMessage(message, F("optional_import_message"));
  sub_message->GetReflection()->SetInt32(sub_message, import_d_, 120);

  reflection->SetEnum(message, F("optional_nested_enum"), nested_baz_);
  reflection->SetEnum(message, F("optional_foreign_enum"), foreign_baz_);
  reflection->SetEnum(message, F("optional_import_enum"), import_baz_);

  reflection->SetString(message, F("optional_string_piece"), "124");
  reflection->SetString(message, F("optional_cord"), "125");

  sub_message =
      reflection->MutableMessage(message, F("optional_public_import_message"));
  sub_message->GetReflection()->SetInt32(sub_message, import_e_, 126);

  sub_message = reflection->MutableMessage(message, F("optional_lazy_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 127);

  // -----------------------------------------------------------------

  reflection->AddInt32(message, F("repeated_int32"), 201);
  reflection->AddInt64(message, F("repeated_int64"), 202);
  reflection->AddUInt32(message, F("repeated_uint32"), 203);
  reflection->AddUInt64(message, F("repeated_uint64"), 204);
  reflection->AddInt32(message, F("repeated_sint32"), 205);
  reflection->AddInt64(message, F("repeated_sint64"), 206);
  reflection->AddUInt32(message, F("repeated_fixed32"), 207);
  reflection->AddUInt64(message, F("repeated_fixed64"), 208);
  reflection->AddInt32(message, F("repeated_sfixed32"), 209);
  reflection->AddInt64(message, F("repeated_sfixed64"), 210);
  reflection->AddFloat(message, F("repeated_float"), 211);
  reflection->AddDouble(message, F("repeated_double"), 212);
  reflection->AddBool(message, F("repeated_bool"), true);
  reflection->AddString(message, F("repeated_string"), "215");
  reflection->AddString(message, F("repeated_bytes"), "216");

  sub_message = reflection->AddMessage(message, F("repeatedgroup"));
  sub_message->GetReflection()->SetInt32(sub_message, repeated_group_a_, 217);
  sub_message = reflection->AddMessage(message, F("repeated_nested_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 218);
  sub_message = reflection->AddMessage(message, F("repeated_foreign_message"));
  sub_message->GetReflection()->SetInt32(sub_message, foreign_c_, 219);
  sub_message = reflection->AddMessage(message, F("repeated_import_message"));
  sub_message->GetReflection()->SetInt32(sub_message, import_d_, 220);
  sub_message = reflection->AddMessage(message, F("repeated_lazy_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 227);

  reflection->AddEnum(message, F("repeated_nested_enum"), nested_bar_);
  reflection->AddEnum(message, F("repeated_foreign_enum"), foreign_bar_);
  reflection->AddEnum(message, F("repeated_import_enum"), import_bar_);

  reflection->AddString(message, F("repeated_string_piece"), "224");
  reflection->AddString(message, F("repeated_cord"), "225");

  // Add a second one of each field.
  reflection->AddInt32(message, F("repeated_int32"), 301);
  reflection->AddInt64(message, F("repeated_int64"), 302);
  reflection->AddUInt32(message, F("repeated_uint32"), 303);
  reflection->AddUInt64(message, F("repeated_uint64"), 304);
  reflection->AddInt32(message, F("repeated_sint32"), 305);
  reflection->AddInt64(message, F("repeated_sint64"), 306);
  reflection->AddUInt32(message, F("repeated_fixed32"), 307);
  reflection->AddUInt64(message, F("repeated_fixed64"), 308);
  reflection->AddInt32(message, F("repeated_sfixed32"), 309);
  reflection->AddInt64(message, F("repeated_sfixed64"), 310);
  reflection->AddFloat(message, F("repeated_float"), 311);
  reflection->AddDouble(message, F("repeated_double"), 312);
  reflection->AddBool(message, F("repeated_bool"), false);
  reflection->AddString(message, F("repeated_string"), "315");
  reflection->AddString(message, F("repeated_bytes"), "316");

  sub_message = reflection->AddMessage(message, F("repeatedgroup"));
  sub_message->GetReflection()->SetInt32(sub_message, repeated_group_a_, 317);
  sub_message = reflection->AddMessage(message, F("repeated_nested_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 318);
  sub_message = reflection->AddMessage(message, F("repeated_foreign_message"));
  sub_message->GetReflection()->SetInt32(sub_message, foreign_c_, 319);
  sub_message = reflection->AddMessage(message, F("repeated_import_message"));
  sub_message->GetReflection()->SetInt32(sub_message, import_d_, 320);
  sub_message = reflection->AddMessage(message, F("repeated_lazy_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 327);

  reflection->AddEnum(message, F("repeated_nested_enum"), nested_baz_);
  reflection->AddEnum(message, F("repeated_foreign_enum"), foreign_baz_);
  reflection->AddEnum(message, F("repeated_import_enum"), import_baz_);

  reflection->AddString(message, F("repeated_string_piece"), "324");
  reflection->AddString(message, F("repeated_cord"), "325");

  // -----------------------------------------------------------------

  reflection->SetInt32(message, F("default_int32"), 401);
  reflection->SetInt64(message, F("default_int64"), 402);
  reflection->SetUInt32(message, F("default_uint32"), 403);
  reflection->SetUInt64(message, F("default_uint64"), 404);
  reflection->SetInt32(message, F("default_sint32"), 405);
  reflection->SetInt64(message, F("default_sint64"), 406);
  reflection->SetUInt32(message, F("default_fixed32"), 407);
  reflection->SetUInt64(message, F("default_fixed64"), 408);
  reflection->SetInt32(message, F("default_sfixed32"), 409);
  reflection->SetInt64(message, F("default_sfixed64"), 410);
  reflection->SetFloat(message, F("default_float"), 411);
  reflection->SetDouble(message, F("default_double"), 412);
  reflection->SetBool(message, F("default_bool"), false);
  reflection->SetString(message, F("default_string"), "415");
  reflection->SetString(message, F("default_bytes"), "416");

  reflection->SetEnum(message, F("default_nested_enum"), nested_foo_);
  reflection->SetEnum(message, F("default_foreign_enum"), foreign_foo_);
  reflection->SetEnum(message, F("default_import_enum"), import_foo_);

  reflection->SetString(message, F("default_string_piece"), "424");
  reflection->SetString(message, F("default_cord"), "425");

  reflection->SetUInt32(message, F("oneof_uint32"), 601);
  sub_message = reflection->MutableMessage(message, F("oneof_nested_message"));
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 602);
  reflection->SetString(message, F("oneof_string"), "603");
  reflection->SetString(message, F("oneof_bytes"), "604");
}

inline void TestUtil::ReflectionTester::SetOneofViaReflection(
    Message* message) {
  const Descriptor* descriptor = message->GetDescriptor();
  const Reflection* reflection = message->GetReflection();
  Message* sub_message = reflection->MutableMessage(
      message, descriptor->FindFieldByName("foo_lazy_message"));
  sub_message->GetReflection()->SetInt64(
      sub_message, sub_message->GetDescriptor()->FindFieldByName("qux_int"),
      100);

  reflection->SetString(message, descriptor->FindFieldByName("bar_cord"),
                        "101");
  reflection->SetInt32(message, descriptor->FindFieldByName("baz_int"), 102);
  reflection->SetString(message, descriptor->FindFieldByName("baz_string"),
                        "103");
}

inline void TestUtil::ReflectionTester::ExpectOneofSetViaReflection(
    const Message& message) {
  const Descriptor* descriptor = message.GetDescriptor();
  const Reflection* reflection = message.GetReflection();
  std::string scratch;
  EXPECT_TRUE(reflection->HasField(
      message, descriptor->FindFieldByName("foo_lazy_message")));
  EXPECT_TRUE(
      reflection->HasField(message, descriptor->FindFieldByName("bar_cord")));
  EXPECT_TRUE(
      reflection->HasField(message, descriptor->FindFieldByName("baz_int")));
  EXPECT_TRUE(
      reflection->HasField(message, descriptor->FindFieldByName("baz_string")));

  const Message* sub_message = &reflection->GetMessage(
      message, descriptor->FindFieldByName("foo_lazy_message"));
  EXPECT_EQ(100, sub_message->GetReflection()->GetInt64(
                     *sub_message,
                     sub_message->GetDescriptor()->FindFieldByName("qux_int")));

  EXPECT_EQ("101", reflection->GetString(
                       message, descriptor->FindFieldByName("bar_cord")));
  EXPECT_EQ("101",
            reflection->GetStringReference(
                message, descriptor->FindFieldByName("bar_cord"), &scratch));

  EXPECT_EQ(102, reflection->GetInt32(message,
                                      descriptor->FindFieldByName("baz_int")));

  EXPECT_EQ("103", reflection->GetString(
                       message, descriptor->FindFieldByName("baz_string")));
  EXPECT_EQ("103",
            reflection->GetStringReference(
                message, descriptor->FindFieldByName("baz_string"), &scratch));
}

inline void TestUtil::ReflectionTester::SetPackedFieldsViaReflection(
    Message* message) {
  const Reflection* reflection = message->GetReflection();
  reflection->AddInt32(message, F("packed_int32"), 601);
  reflection->AddInt64(message, F("packed_int64"), 602);
  reflection->AddUInt32(message, F("packed_uint32"), 603);
  reflection->AddUInt64(message, F("packed_uint64"), 604);
  reflection->AddInt32(message, F("packed_sint32"), 605);
  reflection->AddInt64(message, F("packed_sint64"), 606);
  reflection->AddUInt32(message, F("packed_fixed32"), 607);
  reflection->AddUInt64(message, F("packed_fixed64"), 608);
  reflection->AddInt32(message, F("packed_sfixed32"), 609);
  reflection->AddInt64(message, F("packed_sfixed64"), 610);
  reflection->AddFloat(message, F("packed_float"), 611);
  reflection->AddDouble(message, F("packed_double"), 612);
  reflection->AddBool(message, F("packed_bool"), true);
  reflection->AddEnum(message, F("packed_enum"), foreign_bar_);

  reflection->AddInt32(message, F("packed_int32"), 701);
  reflection->AddInt64(message, F("packed_int64"), 702);
  reflection->AddUInt32(message, F("packed_uint32"), 703);
  reflection->AddUInt64(message, F("packed_uint64"), 704);
  reflection->AddInt32(message, F("packed_sint32"), 705);
  reflection->AddInt64(message, F("packed_sint64"), 706);
  reflection->AddUInt32(message, F("packed_fixed32"), 707);
  reflection->AddUInt64(message, F("packed_fixed64"), 708);
  reflection->AddInt32(message, F("packed_sfixed32"), 709);
  reflection->AddInt64(message, F("packed_sfixed64"), 710);
  reflection->AddFloat(message, F("packed_float"), 711);
  reflection->AddDouble(message, F("packed_double"), 712);
  reflection->AddBool(message, F("packed_bool"), false);
  reflection->AddEnum(message, F("packed_enum"), foreign_baz_);
}

// -------------------------------------------------------------------

inline void TestUtil::ReflectionTester::ExpectAllFieldsSetViaReflection(
    const Message& message) {
  // We have to split this into three function otherwise it creates a stack
  // frame so large that it triggers a warning.
  ExpectAllFieldsSetViaReflection1(message);
  ExpectAllFieldsSetViaReflection2(message);
  ExpectAllFieldsSetViaReflection3(message);
}

inline void TestUtil::ReflectionTester::ExpectAllFieldsSetViaReflection1(
    const Message& message) {
  const Reflection* reflection = message.GetReflection();
  std::string scratch;
  const Message* sub_message;

  EXPECT_TRUE(reflection->HasField(message, F("optional_int32")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_int64")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_uint32")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_uint64")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_sint32")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_sint64")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_fixed32")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_fixed64")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_sfixed32")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_sfixed64")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_float")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_double")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_bool")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_string")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_bytes")));

  EXPECT_TRUE(reflection->HasField(message, F("optionalgroup")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_nested_message")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_foreign_message")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_import_message")));
  EXPECT_TRUE(
      reflection->HasField(message, F("optional_public_import_message")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_lazy_message")));

  sub_message = &reflection->GetMessage(message, F("optionalgroup"));
  EXPECT_TRUE(sub_message->GetReflection()->HasField(*sub_message, group_a_));
  sub_message = &reflection->GetMessage(message, F("optional_nested_message"));
  EXPECT_TRUE(sub_message->GetReflection()->HasField(*sub_message, nested_b_));
  sub_message = &reflection->GetMessage(message, F("optional_foreign_message"));
  EXPECT_TRUE(sub_message->GetReflection()->HasField(*sub_message, foreign_c_));
  sub_message = &reflection->GetMessage(message, F("optional_import_message"));
  EXPECT_TRUE(sub_message->GetReflection()->HasField(*sub_message, import_d_));
  sub_message =
      &reflection->GetMessage(message, F("optional_public_import_message"));
  EXPECT_TRUE(sub_message->GetReflection()->HasField(*sub_message, import_e_));
  sub_message = &reflection->GetMessage(message, F("optional_lazy_message"));
  EXPECT_TRUE(sub_message->GetReflection()->HasField(*sub_message, nested_b_));

  EXPECT_TRUE(reflection->HasField(message, F("optional_nested_enum")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_foreign_enum")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_import_enum")));

  EXPECT_TRUE(reflection->HasField(message, F("optional_string_piece")));
  EXPECT_TRUE(reflection->HasField(message, F("optional_cord")));

  EXPECT_EQ(101, reflection->GetInt32(message, F("optional_int32")));
  EXPECT_EQ(102, reflection->GetInt64(message, F("optional_int64")));
  EXPECT_EQ(103, reflection->GetUInt32(message, F("optional_uint32")));
  EXPECT_EQ(104, reflection->GetUInt64(message, F("optional_uint64")));
  EXPECT_EQ(105, reflection->GetInt32(message, F("optional_sint32")));
  EXPECT_EQ(106, reflection->GetInt64(message, F("optional_sint64")));
  EXPECT_EQ(107, reflection->GetUInt32(message, F("optional_fixed32")));
  EXPECT_EQ(108, reflection->GetUInt64(message, F("optional_fixed64")));
  EXPECT_EQ(109, reflection->GetInt32(message, F("optional_sfixed32")));
  EXPECT_EQ(110, reflection->GetInt64(message, F("optional_sfixed64")));
  EXPECT_EQ(111, reflection->GetFloat(message, F("optional_float")));
  EXPECT_EQ(112, reflection->GetDouble(message, F("optional_double")));
  EXPECT_TRUE(reflection->GetBool(message, F("optional_bool")));
  EXPECT_EQ("115", reflection->GetString(message, F("optional_string")));
  EXPECT_EQ("116", reflection->GetString(message, F("optional_bytes")));

  EXPECT_EQ("115", reflection->GetStringReference(message, F("optional_string"),
                                                  &scratch));
  EXPECT_EQ("116", reflection->GetStringReference(message, F("optional_bytes"),
                                                  &scratch));

  sub_message = &reflection->GetMessage(message, F("optionalgroup"));
  EXPECT_EQ(117,
            sub_message->GetReflection()->GetInt32(*sub_message, group_a_));
  sub_message = &reflection->GetMessage(message, F("optional_nested_message"));
  EXPECT_EQ(118,
            sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));
  sub_message = &reflection->GetMessage(message, F("optional_foreign_message"));
  EXPECT_EQ(119,
            sub_message->GetReflection()->GetInt32(*sub_message, foreign_c_));
  sub_message = &reflection->GetMessage(message, F("optional_import_message"));
  EXPECT_EQ(120,
            sub_message->GetReflection()->GetInt32(*sub_message, import_d_));
  sub_message =
      &reflection->GetMessage(message, F("optional_public_import_message"));
  EXPECT_EQ(126,
            sub_message->GetReflection()->GetInt32(*sub_message, import_e_));
  sub_message = &reflection->GetMessage(message, F("optional_lazy_message"));
  EXPECT_EQ(127,
            sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));

  EXPECT_EQ(nested_baz_,
            reflection->GetEnum(message, F("optional_nested_enum")));
  EXPECT_EQ(foreign_baz_,
            reflection->GetEnum(message, F("optional_foreign_enum")));
  EXPECT_EQ(import_baz_,
            reflection->GetEnum(message, F("optional_import_enum")));

  EXPECT_EQ("124", reflection->GetString(message, F("optional_string_piece")));
  EXPECT_EQ("124", reflection->GetStringReference(
                       message, F("optional_string_piece"), &scratch));

  EXPECT_EQ("125", reflection->GetString(message, F("optional_cord")));
  EXPECT_EQ("125", reflection->GetStringReference(message, F("optional_cord"),
                                                  &scratch));

  EXPECT_TRUE(reflection->HasField(message, F("oneof_bytes")));
  EXPECT_EQ("604", reflection->GetString(message, F("oneof_bytes")));

  if (base_descriptor_->name() == "TestAllTypes") {
    EXPECT_FALSE(reflection->HasField(message, F("oneof_uint32")));
    EXPECT_FALSE(reflection->HasField(message, F("oneof_string")));
  } else {
    EXPECT_TRUE(reflection->HasField(message, F("oneof_uint32")));
    EXPECT_TRUE(reflection->HasField(message, F("oneof_string")));
    EXPECT_EQ(601, reflection->GetUInt32(message, F("oneof_uint32")));
    EXPECT_EQ("603", reflection->GetString(message, F("oneof_string")));
    sub_message = &reflection->GetMessage(message, F("oneof_nested_message"));
    EXPECT_EQ(602,
              sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));
  }
}

inline void TestUtil::ReflectionTester::ExpectAllFieldsSetViaReflection2(
    const Message& message) {
  const Reflection* reflection = message.GetReflection();
  std::string scratch;
  const Message* sub_message;

  // -----------------------------------------------------------------

  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_int32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_int64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_uint32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_uint64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_sint32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_sint64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_fixed32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_fixed64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_sfixed32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_sfixed64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_float")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_double")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_bool")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_string")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_bytes")));

  ASSERT_EQ(2, reflection->FieldSize(message, F("repeatedgroup")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_nested_message")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_foreign_message")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_import_message")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_lazy_message")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_nested_enum")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_foreign_enum")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_import_enum")));

  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_string_piece")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("repeated_cord")));

  EXPECT_EQ(201, reflection->GetRepeatedInt32(message, F("repeated_int32"), 0));
  EXPECT_EQ(202, reflection->GetRepeatedInt64(message, F("repeated_int64"), 0));
  EXPECT_EQ(203,
            reflection->GetRepeatedUInt32(message, F("repeated_uint32"), 0));
  EXPECT_EQ(204,
            reflection->GetRepeatedUInt64(message, F("repeated_uint64"), 0));
  EXPECT_EQ(205,
            reflection->GetRepeatedInt32(message, F("repeated_sint32"), 0));
  EXPECT_EQ(206,
            reflection->GetRepeatedInt64(message, F("repeated_sint64"), 0));
  EXPECT_EQ(207,
            reflection->GetRepeatedUInt32(message, F("repeated_fixed32"), 0));
  EXPECT_EQ(208,
            reflection->GetRepeatedUInt64(message, F("repeated_fixed64"), 0));
  EXPECT_EQ(209,
            reflection->GetRepeatedInt32(message, F("repeated_sfixed32"), 0));
  EXPECT_EQ(210,
            reflection->GetRepeatedInt64(message, F("repeated_sfixed64"), 0));
  EXPECT_EQ(211, reflection->GetRepeatedFloat(message, F("repeated_float"), 0));
  EXPECT_EQ(212,
            reflection->GetRepeatedDouble(message, F("repeated_double"), 0));
  EXPECT_TRUE(reflection->GetRepeatedBool(message, F("repeated_bool"), 0));
  EXPECT_EQ("215",
            reflection->GetRepeatedString(message, F("repeated_string"), 0));
  EXPECT_EQ("216",
            reflection->GetRepeatedString(message, F("repeated_bytes"), 0));

  EXPECT_EQ("215", reflection->GetRepeatedStringReference(
                       message, F("repeated_string"), 0, &scratch));
  EXPECT_EQ("216", reflection->GetRepeatedStringReference(
                       message, F("repeated_bytes"), 0, &scratch));

  sub_message = &reflection->GetRepeatedMessage(message, F("repeatedgroup"), 0);
  EXPECT_EQ(217, sub_message->GetReflection()->GetInt32(*sub_message,
                                                        repeated_group_a_));
  sub_message =
      &reflection->GetRepeatedMessage(message, F("repeated_nested_message"), 0);
  EXPECT_EQ(218,
            sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));
  sub_message = &reflection->GetRepeatedMessage(
      message, F("repeated_foreign_message"), 0);
  EXPECT_EQ(219,
            sub_message->GetReflection()->GetInt32(*sub_message, foreign_c_));
  sub_message =
      &reflection->GetRepeatedMessage(message, F("repeated_import_message"), 0);
  EXPECT_EQ(220,
            sub_message->GetReflection()->GetInt32(*sub_message, import_d_));
  sub_message =
      &reflection->GetRepeatedMessage(message, F("repeated_lazy_message"), 0);
  EXPECT_EQ(227,
            sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));

  EXPECT_EQ(nested_bar_,
            reflection->GetRepeatedEnum(message, F("repeated_nested_enum"), 0));
  EXPECT_EQ(foreign_bar_, reflection->GetRepeatedEnum(
                              message, F("repeated_foreign_enum"), 0));
  EXPECT_EQ(import_bar_,
            reflection->GetRepeatedEnum(message, F("repeated_import_enum"), 0));

  EXPECT_EQ("224", reflection->GetRepeatedString(
                       message, F("repeated_string_piece"), 0));
  EXPECT_EQ("224", reflection->GetRepeatedStringReference(
                       message, F("repeated_string_piece"), 0, &scratch));

  EXPECT_EQ("225",
            reflection->GetRepeatedString(message, F("repeated_cord"), 0));
  EXPECT_EQ("225", reflection->GetRepeatedStringReference(
                       message, F("repeated_cord"), 0, &scratch));

  EXPECT_EQ(301, reflection->GetRepeatedInt32(message, F("repeated_int32"), 1));
  EXPECT_EQ(302, reflection->GetRepeatedInt64(message, F("repeated_int64"), 1));
  EXPECT_EQ(303,
            reflection->GetRepeatedUInt32(message, F("repeated_uint32"), 1));
  EXPECT_EQ(304,
            reflection->GetRepeatedUInt64(message, F("repeated_uint64"), 1));
  EXPECT_EQ(305,
            reflection->GetRepeatedInt32(message, F("repeated_sint32"), 1));
  EXPECT_EQ(306,
            reflection->GetRepeatedInt64(message, F("repeated_sint64"), 1));
  EXPECT_EQ(307,
            reflection->GetRepeatedUInt32(message, F("repeated_fixed32"), 1));
  EXPECT_EQ(308,
            reflection->GetRepeatedUInt64(message, F("repeated_fixed64"), 1));
  EXPECT_EQ(309,
            reflection->GetRepeatedInt32(message, F("repeated_sfixed32"), 1));
  EXPECT_EQ(310,
            reflection->GetRepeatedInt64(message, F("repeated_sfixed64"), 1));
  EXPECT_EQ(311, reflection->GetRepeatedFloat(message, F("repeated_float"), 1));
  EXPECT_EQ(312,
            reflection->GetRepeatedDouble(message, F("repeated_double"), 1));
  EXPECT_FALSE(reflection->GetRepeatedBool(message, F("repeated_bool"), 1));
  EXPECT_EQ("315",
            reflection->GetRepeatedString(message, F("repeated_string"), 1));
  EXPECT_EQ("316",
            reflection->GetRepeatedString(message, F("repeated_bytes"), 1));

  EXPECT_EQ("315", reflection->GetRepeatedStringReference(
                       message, F("repeated_string"), 1, &scratch));
  EXPECT_EQ("316", reflection->GetRepeatedStringReference(
                       message, F("repeated_bytes"), 1, &scratch));

  sub_message = &reflection->GetRepeatedMessage(message, F("repeatedgroup"), 1);
  EXPECT_EQ(317, sub_message->GetReflection()->GetInt32(*sub_message,
                                                        repeated_group_a_));
  sub_message =
      &reflection->GetRepeatedMessage(message, F("repeated_nested_message"), 1);
  EXPECT_EQ(318,
            sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));
  sub_message = &reflection->GetRepeatedMessage(
      message, F("repeated_foreign_message"), 1);
  EXPECT_EQ(319,
            sub_message->GetReflection()->GetInt32(*sub_message, foreign_c_));
  sub_message =
      &reflection->GetRepeatedMessage(message, F("repeated_import_message"), 1);
  EXPECT_EQ(320,
            sub_message->GetReflection()->GetInt32(*sub_message, import_d_));
  sub_message =
      &reflection->GetRepeatedMessage(message, F("repeated_lazy_message"), 1);
  EXPECT_EQ(327,
            sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));

  EXPECT_EQ(nested_baz_,
            reflection->GetRepeatedEnum(message, F("repeated_nested_enum"), 1));
  EXPECT_EQ(foreign_baz_, reflection->GetRepeatedEnum(
                              message, F("repeated_foreign_enum"), 1));
  EXPECT_EQ(import_baz_,
            reflection->GetRepeatedEnum(message, F("repeated_import_enum"), 1));

  EXPECT_EQ("324", reflection->GetRepeatedString(
                       message, F("repeated_string_piece"), 1));
  EXPECT_EQ("324", reflection->GetRepeatedStringReference(
                       message, F("repeated_string_piece"), 1, &scratch));

  EXPECT_EQ("325",
            reflection->GetRepeatedString(message, F("repeated_cord"), 1));
  EXPECT_EQ("325", reflection->GetRepeatedStringReference(
                       message, F("repeated_cord"), 1, &scratch));
}

inline void TestUtil::ReflectionTester::ExpectAllFieldsSetViaReflection3(
    const Message& message) {
  const Reflection* reflection = message.GetReflection();
  std::string scratch;

  // -----------------------------------------------------------------

  EXPECT_TRUE(reflection->HasField(message, F("default_int32")));
  EXPECT_TRUE(reflection->HasField(message, F("default_int64")));
  EXPECT_TRUE(reflection->HasField(message, F("default_uint32")));
  EXPECT_TRUE(reflection->HasField(message, F("default_uint64")));
  EXPECT_TRUE(reflection->HasField(message, F("default_sint32")));
  EXPECT_TRUE(reflection->HasField(message, F("default_sint64")));
  EXPECT_TRUE(reflection->HasField(message, F("default_fixed32")));
  EXPECT_TRUE(reflection->HasField(message, F("default_fixed64")));
  EXPECT_TRUE(reflection->HasField(message, F("default_sfixed32")));
  EXPECT_TRUE(reflection->HasField(message, F("default_sfixed64")));
  EXPECT_TRUE(reflection->HasField(message, F("default_float")));
  EXPECT_TRUE(reflection->HasField(message, F("default_double")));
  EXPECT_TRUE(reflection->HasField(message, F("default_bool")));
  EXPECT_TRUE(reflection->HasField(message, F("default_string")));
  EXPECT_TRUE(reflection->HasField(message, F("default_bytes")));

  EXPECT_TRUE(reflection->HasField(message, F("default_nested_enum")));
  EXPECT_TRUE(reflection->HasField(message, F("default_foreign_enum")));
  EXPECT_TRUE(reflection->HasField(message, F("default_import_enum")));

  EXPECT_TRUE(reflection->HasField(message, F("default_string_piece")));
  EXPECT_TRUE(reflection->HasField(message, F("default_cord")));

  EXPECT_EQ(401, reflection->GetInt32(message, F("default_int32")));
  EXPECT_EQ(402, reflection->GetInt64(message, F("default_int64")));
  EXPECT_EQ(403, reflection->GetUInt32(message, F("default_uint32")));
  EXPECT_EQ(404, reflection->GetUInt64(message, F("default_uint64")));
  EXPECT_EQ(405, reflection->GetInt32(message, F("default_sint32")));
  EXPECT_EQ(406, reflection->GetInt64(message, F("default_sint64")));
  EXPECT_EQ(407, reflection->GetUInt32(message, F("default_fixed32")));
  EXPECT_EQ(408, reflection->GetUInt64(message, F("default_fixed64")));
  EXPECT_EQ(409, reflection->GetInt32(message, F("default_sfixed32")));
  EXPECT_EQ(410, reflection->GetInt64(message, F("default_sfixed64")));
  EXPECT_EQ(411, reflection->GetFloat(message, F("default_float")));
  EXPECT_EQ(412, reflection->GetDouble(message, F("default_double")));
  EXPECT_FALSE(reflection->GetBool(message, F("default_bool")));
  EXPECT_EQ("415", reflection->GetString(message, F("default_string")));
  EXPECT_EQ("416", reflection->GetString(message, F("default_bytes")));

  EXPECT_EQ("415", reflection->GetStringReference(message, F("default_string"),
                                                  &scratch));
  EXPECT_EQ("416", reflection->GetStringReference(message, F("default_bytes"),
                                                  &scratch));

  EXPECT_EQ(nested_foo_,
            reflection->GetEnum(message, F("default_nested_enum")));
  EXPECT_EQ(foreign_foo_,
            reflection->GetEnum(message, F("default_foreign_enum")));
  EXPECT_EQ(import_foo_,
            reflection->GetEnum(message, F("default_import_enum")));

  EXPECT_EQ("424", reflection->GetString(message, F("default_string_piece")));
  EXPECT_EQ("424", reflection->GetStringReference(
                       message, F("default_string_piece"), &scratch));

  EXPECT_EQ("425", reflection->GetString(message, F("default_cord")));
  EXPECT_EQ("425", reflection->GetStringReference(message, F("default_cord"),
                                                  &scratch));
}

inline void TestUtil::ReflectionTester::ExpectPackedFieldsSetViaReflection(
    const Message& message) {
  const Reflection* reflection = message.GetReflection();

  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_int32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_int64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_uint32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_uint64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_sint32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_sint64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_fixed32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_fixed64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_sfixed32")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_sfixed64")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_float")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_double")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_bool")));
  ASSERT_EQ(2, reflection->FieldSize(message, F("packed_enum")));

  EXPECT_EQ(601, reflection->GetRepeatedInt32(message, F("packed_int32"), 0));
  EXPECT_EQ(602, reflection->GetRepeatedInt64(message, F("packed_int64"), 0));
  EXPECT_EQ(603, reflection->GetRepeatedUInt32(message, F("packed_uint32"), 0));
  EXPECT_EQ(604, reflection->GetRepeatedUInt64(message, F("packed_uint64"), 0));
  EXPECT_EQ(605, reflection->GetRepeatedInt32(message, F("packed_sint32"), 0));
  EXPECT_EQ(606, reflection->GetRepeatedInt64(message, F("packed_sint64"), 0));
  EXPECT_EQ(607,
            reflection->GetRepeatedUInt32(message, F("packed_fixed32"), 0));
  EXPECT_EQ(608,
            reflection->GetRepeatedUInt64(message, F("packed_fixed64"), 0));
  EXPECT_EQ(609,
            reflection->GetRepeatedInt32(message, F("packed_sfixed32"), 0));
  EXPECT_EQ(610,
            reflection->GetRepeatedInt64(message, F("packed_sfixed64"), 0));
  EXPECT_EQ(611, reflection->GetRepeatedFloat(message, F("packed_float"), 0));
  EXPECT_EQ(612, reflection->GetRepeatedDouble(message, F("packed_double"), 0));
  EXPECT_TRUE(reflection->GetRepeatedBool(message, F("packed_bool"), 0));
  EXPECT_EQ(foreign_bar_,
            reflection->GetRepeatedEnum(message, F("packed_enum"), 0));

  EXPECT_EQ(701, reflection->GetRepeatedInt32(message, F("packed_int32"), 1));
  EXPECT_EQ(702, reflection->GetRepeatedInt64(message, F("packed_int64"), 1));
  EXPECT_EQ(703, reflection->GetRepeatedUInt32(message, F("packed_uint32"), 1));
  EXPECT_EQ(704, reflection->GetRepeatedUInt64(message, F("packed_uint64"), 1));
  EXPECT_EQ(705, reflection->GetRepeatedInt32(message, F("packed_sint32"), 1));
  EXPECT_EQ(706, reflection->GetRepeatedInt64(message, F("packed_sint64"), 1));
  EXPECT_EQ(707,
            reflection->GetRepeatedUInt32(message, F("packed_fixed32"), 1));
  EXPECT_EQ(708,
            reflection->GetRepeatedUInt64(message, F("packed_fixed64"), 1));
  EXPECT_EQ(709,
            reflection->GetRepeatedInt32(message, F("packed_sfixed32"), 1));
  EXPECT_EQ(710,
            reflection->GetRepeatedInt64(message, F("packed_sfixed64"), 1));
  EXPECT_EQ(711, reflection->GetRepeatedFloat(message, F("packed_float"), 1));
  EXPECT_EQ(712, reflection->GetRepeatedDouble(message, F("packed_double"), 1));
  EXPECT_FALSE(reflection->GetRepeatedBool(message, F("packed_bool"), 1));
  EXPECT_EQ(foreign_baz_,
            reflection->GetRepeatedEnum(message, F("packed_enum"), 1));
}

// -------------------------------------------------------------------

inline void TestUtil::ReflectionTester::ExpectClearViaReflection(
    const Message& message) {
  const Reflection* reflection = message.GetReflection();
  std::string scratch;
  const Message* sub_message;

  // has_blah() should initially be false for all optional fields.
  EXPECT_FALSE(reflection->HasField(message, F("optional_int32")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_int64")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_uint32")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_uint64")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_sint32")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_sint64")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_fixed32")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_fixed64")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_sfixed32")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_sfixed64")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_float")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_double")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_bool")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_string")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_bytes")));

  EXPECT_FALSE(reflection->HasField(message, F("optionalgroup")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_nested_message")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_foreign_message")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_import_message")));
  EXPECT_FALSE(
      reflection->HasField(message, F("optional_public_import_message")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_lazy_message")));

  EXPECT_FALSE(reflection->HasField(message, F("optional_nested_enum")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_foreign_enum")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_import_enum")));

  EXPECT_FALSE(reflection->HasField(message, F("optional_string_piece")));
  EXPECT_FALSE(reflection->HasField(message, F("optional_cord")));

  // Optional fields without defaults are set to zero or something like it.
  EXPECT_EQ(0, reflection->GetInt32(message, F("optional_int32")));
  EXPECT_EQ(0, reflection->GetInt64(message, F("optional_int64")));
  EXPECT_EQ(0, reflection->GetUInt32(message, F("optional_uint32")));
  EXPECT_EQ(0, reflection->GetUInt64(message, F("optional_uint64")));
  EXPECT_EQ(0, reflection->GetInt32(message, F("optional_sint32")));
  EXPECT_EQ(0, reflection->GetInt64(message, F("optional_sint64")));
  EXPECT_EQ(0, reflection->GetUInt32(message, F("optional_fixed32")));
  EXPECT_EQ(0, reflection->GetUInt64(message, F("optional_fixed64")));
  EXPECT_EQ(0, reflection->GetInt32(message, F("optional_sfixed32")));
  EXPECT_EQ(0, reflection->GetInt64(message, F("optional_sfixed64")));
  EXPECT_EQ(0, reflection->GetFloat(message, F("optional_float")));
  EXPECT_EQ(0, reflection->GetDouble(message, F("optional_double")));
  EXPECT_FALSE(reflection->GetBool(message, F("optional_bool")));
  EXPECT_EQ("", reflection->GetString(message, F("optional_string")));
  EXPECT_EQ("", reflection->GetString(message, F("optional_bytes")));

  EXPECT_EQ("", reflection->GetStringReference(message, F("optional_string"),
                                               &scratch));
  EXPECT_EQ("", reflection->GetStringReference(message, F("optional_bytes"),
                                               &scratch));

  // Embedded messages should also be clear.
  sub_message = &reflection->GetMessage(message, F("optionalgroup"));
  EXPECT_FALSE(sub_message->GetReflection()->HasField(*sub_message, group_a_));
  EXPECT_EQ(0, sub_message->GetReflection()->GetInt32(*sub_message, group_a_));
  sub_message = &reflection->GetMessage(message, F("optional_nested_message"));
  EXPECT_FALSE(sub_message->GetReflection()->HasField(*sub_message, nested_b_));
  EXPECT_EQ(0, sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));
  sub_message = &reflection->GetMessage(message, F("optional_foreign_message"));
  EXPECT_FALSE(
      sub_message->GetReflection()->HasField(*sub_message, foreign_c_));
  EXPECT_EQ(0,
            sub_message->GetReflection()->GetInt32(*sub_message, foreign_c_));
  sub_message = &reflection->GetMessage(message, F("optional_import_message"));
  EXPECT_FALSE(sub_message->GetReflection()->HasField(*sub_message, import_d_));
  EXPECT_EQ(0, sub_message->GetReflection()->GetInt32(*sub_message, import_d_));
  sub_message =
      &reflection->GetMessage(message, F("optional_public_import_message"));
  EXPECT_FALSE(sub_message->GetReflection()->HasField(*sub_message, import_e_));
  EXPECT_EQ(0, sub_message->GetReflection()->GetInt32(*sub_message, import_e_));
  sub_message = &reflection->GetMessage(message, F("optional_lazy_message"));
  EXPECT_FALSE(sub_message->GetReflection()->HasField(*sub_message, nested_b_));
  EXPECT_EQ(0, sub_message->GetReflection()->GetInt32(*sub_message, nested_b_));

  // Enums without defaults are set to the first value in the enum.
  EXPECT_EQ(nested_foo_,
            reflection->GetEnum(message, F("optional_nested_enum")));
  EXPECT_EQ(foreign_foo_,
            reflection->GetEnum(message, F("optional_foreign_enum")));
  EXPECT_EQ(import_foo_,
            reflection->GetEnum(message, F("optional_import_enum")));

  EXPECT_EQ("", reflection->GetString(message, F("optional_string_piece")));
  EXPECT_EQ("", reflection->GetStringReference(
                    message, F("optional_string_piece"), &scratch));

  EXPECT_EQ("", reflection->GetString(message, F("optional_cord")));
  EXPECT_EQ("", reflection->GetStringReference(message, F("optional_cord"),
                                               &scratch));

  // Repeated fields are empty.
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_int32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_int64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_uint32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_uint64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_sint32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_sint64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_fixed32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_fixed64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_sfixed32")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_sfixed64")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_float")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_double")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_bool")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_string")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_bytes")));

  EXPECT_EQ(0, reflection->FieldSize(message, F("repeatedgroup")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_nested_message")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_foreign_message")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_import_message")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_lazy_message")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_nested_enum")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_foreign_enum")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_import_enum")));

  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_string_piece")));
  EXPECT_EQ(0, reflection->FieldSize(message, F("repeated_cord")));

  // has_blah() should also be false for all default fields.
  EXPECT_FALSE(reflection->HasField(message, F("default_int32")));
  EXPECT_FALSE(reflection->HasField(message, F("default_int64")));
  EXPECT_FALSE(reflection->HasField(message, F("default_uint32")));
  EXPECT_FALSE(reflection->HasField(message, F("default_uint64")));
  EXPECT_FALSE(reflection->HasField(message, F("default_sint32")));
  EXPECT_FALSE(reflection->HasField(message, F("default_sint64")));
  EXPECT_FALSE(reflection->HasField(message, F("default_fixed32")));
  EXPECT_FALSE(reflection->HasField(message, F("default_fixed64")));
  EXPECT_FALSE(reflection->HasField(message, F("default_sfixed32")));
  EXPECT_FALSE(reflection->HasField(message, F("default_sfixed64")));
  EXPECT_FALSE(reflection->HasField(message, F("default_float")));
  EXPECT_FALSE(reflection->HasField(message, F("default_double")));
  EXPECT_FALSE(reflection->HasField(message, F("default_bool")));
  EXPECT_FALSE(reflection->HasField(message, F("default_string")));
  EXPECT_FALSE(reflection->HasField(message, F("default_bytes")));

  EXPECT_FALSE(reflection->HasField(message, F("default_nested_enum")));
  EXPECT_FALSE(reflection->HasField(message, F("default_foreign_enum")));
  EXPECT_FALSE(reflection->HasField(message, F("default_import_enum")));

  EXPECT_FALSE(reflection->HasField(message, F("default_string_piece")));
  EXPECT_FALSE(reflection->HasField(message, F("default_cord")));

  // Fields with defaults have their default values (duh).
  EXPECT_EQ(41, reflection->GetInt32(message, F("default_int32")));
  EXPECT_EQ(42, reflection->GetInt64(message, F("default_int64")));
  EXPECT_EQ(43, reflection->GetUInt32(message, F("default_uint32")));
  EXPECT_EQ(44, reflection->GetUInt64(message, F("default_uint64")));
  EXPECT_EQ(-45, reflection->GetInt32(message, F("default_sint32")));
  EXPECT_EQ(46, reflection->GetInt64(message, F("default_sint64")));
  EXPECT_EQ(47, reflection->GetUInt32(message, F("default_fixed32")));
  EXPECT_EQ(48, reflection->GetUInt64(message, F("default_fixed64")));
  EXPECT_EQ(49, reflection->GetInt32(message, F("default_sfixed32")));
  EXPECT_EQ(-50, reflection->GetInt64(message, F("default_sfixed64")));
  EXPECT_EQ(51.5, reflection->GetFloat(message, F("default_float")));
  EXPECT_EQ(52e3, reflection->GetDouble(message, F("default_double")));
  EXPECT_TRUE(reflection->GetBool(message, F("default_bool")));
  EXPECT_EQ("hello", reflection->GetString(message, F("default_string")));
  EXPECT_EQ("world", reflection->GetString(message, F("default_bytes")));

  EXPECT_EQ("hello", reflection->GetStringReference(
                         message, F("default_string"), &scratch));
  EXPECT_EQ("world", reflection->GetStringReference(message, F("default_bytes"),
                                                    &scratch));

  EXPECT_EQ(nested_bar_,
            reflection->GetEnum(message, F("default_nested_enum")));
  EXPECT_EQ(foreign_bar_,
            reflection->GetEnum(message, F("default_foreign_enum")));
  EXPECT_EQ(import_bar_,
            reflection->GetEnum(message, F("default_import_enum")));

  EXPECT_EQ("abc", reflection->GetString(message, F("default_string_piece")));
  EXPECT_EQ("abc", reflection->GetStringReference(
                       message, F("default_string_piece"), &scratch));

  EXPECT_EQ("123", reflection->GetString(message, F("default_cord")));
  EXPECT_EQ("123", reflection->GetStringReference(message, F("default_cord"),
                                                  &scratch));
}

// -------------------------------------------------------------------

inline void TestUtil::ReflectionTester::ModifyRepeatedFieldsViaReflection(
    Message* message) {
  const Reflection* reflection = message->GetReflection();
  Message* sub_message;

  reflection->SetRepeatedInt32(message, F("repeated_int32"), 1, 501);
  reflection->SetRepeatedInt64(message, F("repeated_int64"), 1, 502);
  reflection->SetRepeatedUInt32(message, F("repeated_uint32"), 1, 503);
  reflection->SetRepeatedUInt64(message, F("repeated_uint64"), 1, 504);
  reflection->SetRepeatedInt32(message, F("repeated_sint32"), 1, 505);
  reflection->SetRepeatedInt64(message, F("repeated_sint64"), 1, 506);
  reflection->SetRepeatedUInt32(message, F("repeated_fixed32"), 1, 507);
  reflection->SetRepeatedUInt64(message, F("repeated_fixed64"), 1, 508);
  reflection->SetRepeatedInt32(message, F("repeated_sfixed32"), 1, 509);
  reflection->SetRepeatedInt64(message, F("repeated_sfixed64"), 1, 510);
  reflection->SetRepeatedFloat(message, F("repeated_float"), 1, 511);
  reflection->SetRepeatedDouble(message, F("repeated_double"), 1, 512);
  reflection->SetRepeatedBool(message, F("repeated_bool"), 1, true);
  reflection->SetRepeatedString(message, F("repeated_string"), 1, "515");
  reflection->SetRepeatedString(message, F("repeated_bytes"), 1, "516");

  sub_message =
      reflection->MutableRepeatedMessage(message, F("repeatedgroup"), 1);
  sub_message->GetReflection()->SetInt32(sub_message, repeated_group_a_, 517);
  sub_message = reflection->MutableRepeatedMessage(
      message, F("repeated_nested_message"), 1);
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 518);
  sub_message = reflection->MutableRepeatedMessage(
      message, F("repeated_foreign_message"), 1);
  sub_message->GetReflection()->SetInt32(sub_message, foreign_c_, 519);
  sub_message = reflection->MutableRepeatedMessage(
      message, F("repeated_import_message"), 1);
  sub_message->GetReflection()->SetInt32(sub_message, import_d_, 520);
  sub_message = reflection->MutableRepeatedMessage(
      message, F("repeated_lazy_message"), 1);
  sub_message->GetReflection()->SetInt32(sub_message, nested_b_, 527);

  reflection->SetRepeatedEnum(message, F("repeated_nested_enum"), 1,
                              nested_foo_);
  reflection->SetRepeatedEnum(message, F("repeated_foreign_enum"), 1,
                              foreign_foo_);
  reflection->SetRepeatedEnum(message, F("repeated_import_enum"), 1,
                              import_foo_);

  reflection->SetRepeatedString(message, F("repeated_string_piece"), 1, "524");
  reflection->SetRepeatedString(message, F("repeated_cord"), 1, "525");
}

inline void TestUtil::ReflectionTester::RemoveLastRepeatedsViaReflection(
    Message* message) {
  const Reflection* reflection = message->GetReflection();

  std::vector<const FieldDescriptor*> output;
  reflection->ListFields(*message, &output);
  for (int i = 0; i < output.size(); ++i) {
    const FieldDescriptor* field = output[i];
    if (!field->is_repeated()) continue;

    reflection->RemoveLast(message, field);
  }
}

inline void TestUtil::ReflectionTester::ReleaseLastRepeatedsViaReflection(
    Message* message, bool expect_extensions_notnull) {
  const Reflection* reflection = message->GetReflection();

  std::vector<const FieldDescriptor*> output;
  reflection->ListFields(*message, &output);
  for (int i = 0; i < output.size(); ++i) {
    const FieldDescriptor* field = output[i];
    if (!field->is_repeated()) continue;
    if (field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE) continue;

    Message* released = reflection->ReleaseLast(message, field);
    if (!field->is_extension() || expect_extensions_notnull) {
      ASSERT_TRUE(released != nullptr)
          << "ReleaseLast returned nullptr for: " << field->name();
    }
    delete released;
  }
}

inline void TestUtil::ReflectionTester::SwapRepeatedsViaReflection(
    Message* message) {
  const Reflection* reflection = message->GetReflection();

  std::vector<const FieldDescriptor*> output;
  reflection->ListFields(*message, &output);
  for (int i = 0; i < output.size(); ++i) {
    const FieldDescriptor* field = output[i];
    if (!field->is_repeated()) continue;

    reflection->SwapElements(message, field, 0, 1);
  }
}

inline void TestUtil::ReflectionTester::
    SetAllocatedOptionalMessageFieldsToNullViaReflection(Message* message) {
  const Reflection* reflection = message->GetReflection();

  std::vector<const FieldDescriptor*> fields;
  reflection->ListFields(*message, &fields);

  for (int i = 0; i < fields.size(); ++i) {
    const FieldDescriptor* field = fields[i];
    if (!field->is_optional() ||
        field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE)
      continue;

    reflection->SetAllocatedMessage(message, nullptr, field);
  }
}

inline void TestUtil::ReflectionTester::
    SetAllocatedOptionalMessageFieldsToMessageViaReflection(
        Message* from_message, Message* to_message) {
  EXPECT_EQ(from_message->GetDescriptor(), to_message->GetDescriptor());
  const Reflection* from_reflection = from_message->GetReflection();
  const Reflection* to_reflection = to_message->GetReflection();

  std::vector<const FieldDescriptor*> fields;
  from_reflection->ListFields(*from_message, &fields);

  for (int i = 0; i < fields.size(); ++i) {
    const FieldDescriptor* field = fields[i];
    if (!field->is_optional() ||
        field->cpp_type() != FieldDescriptor::CPPTYPE_MESSAGE)
      continue;

    Message* sub_message = from_reflection->ReleaseMessage(from_message, field);
    to_reflection->SetAllocatedMessage(to_message, sub_message, field);
  }
}

inline void TestUtil::ReflectionTester::ExpectMessagesReleasedViaReflection(
    Message* message,
    TestUtil::ReflectionTester::MessageReleaseState expected_release_state) {
  const Reflection* reflection = message->GetReflection();

  static const char* fields[] = {
      "optionalgroup",
      "optional_nested_message",
      "optional_foreign_message",
      "optional_import_message",
  };
  for (int i = 0; i < GOOGLE_ARRAYSIZE(fields); i++) {
    const Message& sub_message = reflection->GetMessage(*message, F(fields[i]));
    Message* released = reflection->ReleaseMessage(message, F(fields[i]));
    switch (expected_release_state) {
      case IS_NULL:
        EXPECT_TRUE(released == nullptr);
        break;
      case NOT_NULL:
        EXPECT_TRUE(released != nullptr);
        if (message->GetArena() == nullptr) {
          // released message must be same as sub_message if source message is
          // not on arena.
          EXPECT_EQ(&sub_message, released);
        }
        break;
      case CAN_BE_NULL:
        break;
    }
    delete released;
    EXPECT_FALSE(reflection->HasField(*message, F(fields[i])));
  }
}

// Check that the passed-in serialization is the canonical serialization we
// expect for a TestFieldOrderings message filled in by
// SetAllFieldsAndExtensions().
inline void ExpectAllFieldsAndExtensionsInOrder(const std::string& serialized) {
  // We set each field individually, serialize separately, and concatenate all
  // the strings in canonical order to determine the expected serialization.
  std::string expected;
  unittest::TestFieldOrderings message;
  message.set_my_int(1);  // Field 1.
  message.AppendToString(&expected);
  message.Clear();
  message.SetExtension(unittest::my_extension_int, 23);  // Field 5.
  message.AppendToString(&expected);
  message.Clear();
  message.set_my_string("foo");  // Field 11.
  message.AppendToString(&expected);
  message.Clear();
  message.SetExtension(unittest::my_extension_string, "bar");  // Field 50.
  message.AppendToString(&expected);
  message.Clear();
  message.set_my_float(1.0);  // Field 101.
  message.AppendToString(&expected);
  message.Clear();

  // We don't EXPECT_EQ() since we don't want to print raw bytes to stdout.
  EXPECT_TRUE(serialized == expected);
}

}  // namespace TestUtil
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_TEST_UTIL_H__
