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
//
// This file needs to be included as .inc as it depends on the namespaces
// (unittest and unittest_import) being set up properly. It is also included
// within an enclosing namespace and requires header files to be included
// out of this file.

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace TestUtil {

// Set every field in the message to a unique value.
inline void SetAllFields(UNITTEST::TestAllTypes* message);
inline void SetOptionalFields(UNITTEST::TestAllTypes* message);
inline void AddRepeatedFields1(UNITTEST::TestAllTypes* message);
inline void AddRepeatedFields2(UNITTEST::TestAllTypes* message);
inline void SetDefaultFields(UNITTEST::TestAllTypes* message);
inline void SetOneofFields(UNITTEST::TestAllTypes* message);
inline void SetAllExtensions(UNITTEST::TestAllExtensions* message);
inline void SetOneofFields(UNITTEST::TestAllExtensions* message);
inline void SetAllFieldsAndExtensions(UNITTEST::TestFieldOrderings* message);
inline void SetPackedFields(UNITTEST::TestPackedTypes* message);
inline void SetPackedExtensions(UNITTEST::TestPackedExtensions* message);
inline void SetUnpackedFields(UNITTEST::TestUnpackedTypes* message);
inline void SetOneof1(UNITTEST::TestOneof2* message);
inline void SetOneof2(UNITTEST::TestOneof2* message);

// Use the repeated versions of the set_*() accessors to modify all the
// repeated fields of the message (which should already have been
// initialized with Set*Fields()).  Set*Fields() itself only tests
// the add_*() accessors.
inline void ModifyRepeatedFields(UNITTEST::TestAllTypes* message);
inline void ModifyRepeatedExtensions(UNITTEST::TestAllExtensions* message);

// Check that all fields have the values that they should have after
// Set*Fields() is called.
inline void ExpectAllFieldsSet(const UNITTEST::TestAllTypes& message);
inline void ExpectAllExtensionsSet(const UNITTEST::TestAllExtensions& message);
inline void ExpectPackedFieldsSet(const UNITTEST::TestPackedTypes& message);
inline void ExpectPackedExtensionsSet(
    const UNITTEST::TestPackedExtensions& message);
inline void ExpectUnpackedFieldsSet(const UNITTEST::TestUnpackedTypes& message);
inline void ExpectUnpackedExtensionsSet(
    const UNITTEST::TestUnpackedExtensions& message);
inline void ExpectOneofSet1(const UNITTEST::TestOneof2& message);
inline void ExpectOneofSet2(const UNITTEST::TestOneof2& message);

// Expect that the message is modified as would be expected from
// Modify*Fields().
inline void ExpectRepeatedFieldsModified(const UNITTEST::TestAllTypes& message);
inline void ExpectRepeatedExtensionsModified(
    const UNITTEST::TestAllExtensions& message);

// Check that all fields have their default values.
inline void ExpectClear(const UNITTEST::TestAllTypes& message);
inline void ExpectExtensionsClear(const UNITTEST::TestAllExtensions& message);
inline void ExpectOneofClear(const UNITTEST::TestOneof2& message);

// Check that all repeated fields have had their last elements removed.
inline void ExpectLastRepeatedsRemoved(const UNITTEST::TestAllTypes& message);
inline void ExpectLastRepeatedExtensionsRemoved(
    const UNITTEST::TestAllExtensions& message);
inline void ExpectLastRepeatedsReleased(const UNITTEST::TestAllTypes& message);
inline void ExpectLastRepeatedExtensionsReleased(
    const UNITTEST::TestAllExtensions& message);

// Check that all repeated fields have had their first and last elements
// swapped.
inline void ExpectRepeatedsSwapped(const UNITTEST::TestAllTypes& message);
inline void ExpectRepeatedExtensionsSwapped(
    const UNITTEST::TestAllExtensions& message);

inline void ExpectAtMostOneFieldSetInOneof(const UNITTEST::TestOneof2& message);

}  // namespace TestUtil

inline void TestUtil::SetAllFields(UNITTEST::TestAllTypes* message) {
  SetOptionalFields(message);
  AddRepeatedFields1(message);
  AddRepeatedFields2(message);
  SetDefaultFields(message);
  SetOneofFields(message);
}

inline void TestUtil::SetOptionalFields(UNITTEST::TestAllTypes* message) {
  message->set_optional_int32(101);
  message->set_optional_int64(102);
  message->set_optional_uint32(103);
  message->set_optional_uint64(104);
  message->set_optional_sint32(105);
  message->set_optional_sint64(106);
  message->set_optional_fixed32(107);
  message->set_optional_fixed64(108);
  message->set_optional_sfixed32(109);
  message->set_optional_sfixed64(110);
  message->set_optional_float(111);
  message->set_optional_double(112);
  message->set_optional_bool(true);
  message->set_optional_string("115");
  message->set_optional_bytes("116");

  message->mutable_optionalgroup()->set_a(117);
  message->mutable_optional_nested_message()->set_bb(118);
  message->mutable_optional_foreign_message()->set_c(119);
  message->mutable_optional_import_message()->set_d(120);
  message->mutable_optional_public_import_message()->set_e(126);
  message->mutable_optional_lazy_message()->set_bb(127);

  message->set_optional_nested_enum(UNITTEST::TestAllTypes::BAZ);
  message->set_optional_foreign_enum(UNITTEST::FOREIGN_BAZ);
  message->set_optional_import_enum(UNITTEST_IMPORT::IMPORT_BAZ);

  // StringPiece and Cord fields are only accessible via reflection in the
  // open source release; see comments in compiler/cpp/string_field.cc.
#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  message->GetReflection()->SetString(
      message,
      message->GetDescriptor()->FindFieldByName("optional_string_piece"),
      "124");
  message->GetReflection()->SetString(
      message, message->GetDescriptor()->FindFieldByName("optional_cord"),
      "125");
#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS
}

// -------------------------------------------------------------------

inline void TestUtil::AddRepeatedFields1(UNITTEST::TestAllTypes* message) {
  message->add_repeated_int32(201);
  message->add_repeated_int64(202);
  message->add_repeated_uint32(203);
  message->add_repeated_uint64(204);
  message->add_repeated_sint32(205);
  message->add_repeated_sint64(206);
  message->add_repeated_fixed32(207);
  message->add_repeated_fixed64(208);
  message->add_repeated_sfixed32(209);
  message->add_repeated_sfixed64(210);
  message->add_repeated_float(211);
  message->add_repeated_double(212);
  message->add_repeated_bool(true);
  message->add_repeated_string("215");
  message->add_repeated_bytes("216");

  message->add_repeatedgroup()->set_a(217);
  message->add_repeated_nested_message()->set_bb(218);
  message->add_repeated_foreign_message()->set_c(219);
  message->add_repeated_import_message()->set_d(220);
  message->add_repeated_lazy_message()->set_bb(227);

  message->add_repeated_nested_enum(UNITTEST::TestAllTypes::BAR);
  message->add_repeated_foreign_enum(UNITTEST::FOREIGN_BAR);
  message->add_repeated_import_enum(UNITTEST_IMPORT::IMPORT_BAR);

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  message->GetReflection()->AddString(
      message,
      message->GetDescriptor()->FindFieldByName("repeated_string_piece"),
      "224");
  message->GetReflection()->AddString(
      message, message->GetDescriptor()->FindFieldByName("repeated_cord"),
      "225");
#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS
}

inline void TestUtil::AddRepeatedFields2(UNITTEST::TestAllTypes* message) {
  // Add a second one of each field.
  message->add_repeated_int32(301);
  message->add_repeated_int64(302);
  message->add_repeated_uint32(303);
  message->add_repeated_uint64(304);
  message->add_repeated_sint32(305);
  message->add_repeated_sint64(306);
  message->add_repeated_fixed32(307);
  message->add_repeated_fixed64(308);
  message->add_repeated_sfixed32(309);
  message->add_repeated_sfixed64(310);
  message->add_repeated_float(311);
  message->add_repeated_double(312);
  message->add_repeated_bool(false);
  message->add_repeated_string("315");
  message->add_repeated_bytes("316");

  message->add_repeatedgroup()->set_a(317);
  message->add_repeated_nested_message()->set_bb(318);
  message->add_repeated_foreign_message()->set_c(319);
  message->add_repeated_import_message()->set_d(320);
  message->add_repeated_lazy_message()->set_bb(327);

  message->add_repeated_nested_enum(UNITTEST::TestAllTypes::BAZ);
  message->add_repeated_foreign_enum(UNITTEST::FOREIGN_BAZ);
  message->add_repeated_import_enum(UNITTEST_IMPORT::IMPORT_BAZ);

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  message->GetReflection()->AddString(
      message,
      message->GetDescriptor()->FindFieldByName("repeated_string_piece"),
      "324");
  message->GetReflection()->AddString(
      message, message->GetDescriptor()->FindFieldByName("repeated_cord"),
      "325");
#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS
}

// -------------------------------------------------------------------

inline void TestUtil::SetDefaultFields(UNITTEST::TestAllTypes* message) {
  message->set_default_int32(401);
  message->set_default_int64(402);
  message->set_default_uint32(403);
  message->set_default_uint64(404);
  message->set_default_sint32(405);
  message->set_default_sint64(406);
  message->set_default_fixed32(407);
  message->set_default_fixed64(408);
  message->set_default_sfixed32(409);
  message->set_default_sfixed64(410);
  message->set_default_float(411);
  message->set_default_double(412);
  message->set_default_bool(false);
  message->set_default_string("415");
  message->set_default_bytes("416");

  message->set_default_nested_enum(UNITTEST::TestAllTypes::FOO);
  message->set_default_foreign_enum(UNITTEST::FOREIGN_FOO);
  message->set_default_import_enum(UNITTEST_IMPORT::IMPORT_FOO);

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  message->GetReflection()->SetString(
      message,
      message->GetDescriptor()->FindFieldByName("default_string_piece"), "424");
  message->GetReflection()->SetString(
      message, message->GetDescriptor()->FindFieldByName("default_cord"),
      "425");
#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS
}

// -------------------------------------------------------------------

inline void TestUtil::ModifyRepeatedFields(UNITTEST::TestAllTypes* message) {
  message->set_repeated_int32(1, 501);
  message->set_repeated_int64(1, 502);
  message->set_repeated_uint32(1, 503);
  message->set_repeated_uint64(1, 504);
  message->set_repeated_sint32(1, 505);
  message->set_repeated_sint64(1, 506);
  message->set_repeated_fixed32(1, 507);
  message->set_repeated_fixed64(1, 508);
  message->set_repeated_sfixed32(1, 509);
  message->set_repeated_sfixed64(1, 510);
  message->set_repeated_float(1, 511);
  message->set_repeated_double(1, 512);
  message->set_repeated_bool(1, true);
  message->set_repeated_string(1, "515");
  message->set_repeated_bytes(1, "516");

  message->mutable_repeatedgroup(1)->set_a(517);
  message->mutable_repeated_nested_message(1)->set_bb(518);
  message->mutable_repeated_foreign_message(1)->set_c(519);
  message->mutable_repeated_import_message(1)->set_d(520);
  message->mutable_repeated_lazy_message(1)->set_bb(527);

  message->set_repeated_nested_enum(1, UNITTEST::TestAllTypes::FOO);
  message->set_repeated_foreign_enum(1, UNITTEST::FOREIGN_FOO);
  message->set_repeated_import_enum(1, UNITTEST_IMPORT::IMPORT_FOO);

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  message->GetReflection()->SetRepeatedString(
      message,
      message->GetDescriptor()->FindFieldByName("repeated_string_piece"), 1,
      "524");
  message->GetReflection()->SetRepeatedString(
      message, message->GetDescriptor()->FindFieldByName("repeated_cord"), 1,
      "525");
#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS
}

// ------------------------------------------------------------------
inline void TestUtil::SetOneofFields(UNITTEST::TestAllTypes* message) {
  message->set_oneof_uint32(601);
  message->mutable_oneof_nested_message()->set_bb(602);
  message->set_oneof_string("603");
  message->set_oneof_bytes("604");
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectAllFieldsSet(
    const UNITTEST::TestAllTypes& message) {
  EXPECT_TRUE(message.has_optional_int32());
  EXPECT_TRUE(message.has_optional_int64());
  EXPECT_TRUE(message.has_optional_uint32());
  EXPECT_TRUE(message.has_optional_uint64());
  EXPECT_TRUE(message.has_optional_sint32());
  EXPECT_TRUE(message.has_optional_sint64());
  EXPECT_TRUE(message.has_optional_fixed32());
  EXPECT_TRUE(message.has_optional_fixed64());
  EXPECT_TRUE(message.has_optional_sfixed32());
  EXPECT_TRUE(message.has_optional_sfixed64());
  EXPECT_TRUE(message.has_optional_float());
  EXPECT_TRUE(message.has_optional_double());
  EXPECT_TRUE(message.has_optional_bool());
  EXPECT_TRUE(message.has_optional_string());
  EXPECT_TRUE(message.has_optional_bytes());

  EXPECT_TRUE(message.has_optionalgroup());
  EXPECT_TRUE(message.has_optional_nested_message());
  EXPECT_TRUE(message.has_optional_foreign_message());
  EXPECT_TRUE(message.has_optional_import_message());
  EXPECT_TRUE(message.has_optional_public_import_message());
  EXPECT_TRUE(message.has_optional_lazy_message());

  EXPECT_TRUE(message.optionalgroup().has_a());
  EXPECT_TRUE(message.optional_nested_message().has_bb());
  EXPECT_TRUE(message.optional_foreign_message().has_c());
  EXPECT_TRUE(message.optional_import_message().has_d());
  EXPECT_TRUE(message.optional_public_import_message().has_e());
  EXPECT_TRUE(message.optional_lazy_message().has_bb());

  EXPECT_TRUE(message.has_optional_nested_enum());
  EXPECT_TRUE(message.has_optional_foreign_enum());
  EXPECT_TRUE(message.has_optional_import_enum());

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  EXPECT_TRUE(message.has_optional_string_piece());
  EXPECT_TRUE(message.has_optional_cord());
#endif

  EXPECT_EQ(101, message.optional_int32());
  EXPECT_EQ(102, message.optional_int64());
  EXPECT_EQ(103, message.optional_uint32());
  EXPECT_EQ(104, message.optional_uint64());
  EXPECT_EQ(105, message.optional_sint32());
  EXPECT_EQ(106, message.optional_sint64());
  EXPECT_EQ(107, message.optional_fixed32());
  EXPECT_EQ(108, message.optional_fixed64());
  EXPECT_EQ(109, message.optional_sfixed32());
  EXPECT_EQ(110, message.optional_sfixed64());
  EXPECT_EQ(111, message.optional_float());
  EXPECT_EQ(112, message.optional_double());
  EXPECT_TRUE(message.optional_bool());
  EXPECT_EQ("115", message.optional_string());
  EXPECT_EQ("116", message.optional_bytes());

  EXPECT_EQ(117, message.optionalgroup().a());
  EXPECT_EQ(118, message.optional_nested_message().bb());
  EXPECT_EQ(119, message.optional_foreign_message().c());
  EXPECT_EQ(120, message.optional_import_message().d());
  EXPECT_EQ(126, message.optional_public_import_message().e());
  EXPECT_EQ(127, message.optional_lazy_message().bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ, message.optional_nested_enum());
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ, message.optional_foreign_enum());
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAZ, message.optional_import_enum());


  // -----------------------------------------------------------------

  ASSERT_EQ(2, message.repeated_int32_size());
  ASSERT_EQ(2, message.repeated_int64_size());
  ASSERT_EQ(2, message.repeated_uint32_size());
  ASSERT_EQ(2, message.repeated_uint64_size());
  ASSERT_EQ(2, message.repeated_sint32_size());
  ASSERT_EQ(2, message.repeated_sint64_size());
  ASSERT_EQ(2, message.repeated_fixed32_size());
  ASSERT_EQ(2, message.repeated_fixed64_size());
  ASSERT_EQ(2, message.repeated_sfixed32_size());
  ASSERT_EQ(2, message.repeated_sfixed64_size());
  ASSERT_EQ(2, message.repeated_float_size());
  ASSERT_EQ(2, message.repeated_double_size());
  ASSERT_EQ(2, message.repeated_bool_size());
  ASSERT_EQ(2, message.repeated_string_size());
  ASSERT_EQ(2, message.repeated_bytes_size());

  ASSERT_EQ(2, message.repeatedgroup_size());
  ASSERT_EQ(2, message.repeated_nested_message_size());
  ASSERT_EQ(2, message.repeated_foreign_message_size());
  ASSERT_EQ(2, message.repeated_import_message_size());
  ASSERT_EQ(2, message.repeated_lazy_message_size());
  ASSERT_EQ(2, message.repeated_nested_enum_size());
  ASSERT_EQ(2, message.repeated_foreign_enum_size());
  ASSERT_EQ(2, message.repeated_import_enum_size());

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  ASSERT_EQ(2, message.repeated_string_piece_size());
  ASSERT_EQ(2, message.repeated_cord_size());
#endif

  EXPECT_EQ(201, message.repeated_int32(0));
  EXPECT_EQ(202, message.repeated_int64(0));
  EXPECT_EQ(203, message.repeated_uint32(0));
  EXPECT_EQ(204, message.repeated_uint64(0));
  EXPECT_EQ(205, message.repeated_sint32(0));
  EXPECT_EQ(206, message.repeated_sint64(0));
  EXPECT_EQ(207, message.repeated_fixed32(0));
  EXPECT_EQ(208, message.repeated_fixed64(0));
  EXPECT_EQ(209, message.repeated_sfixed32(0));
  EXPECT_EQ(210, message.repeated_sfixed64(0));
  EXPECT_EQ(211, message.repeated_float(0));
  EXPECT_EQ(212, message.repeated_double(0));
  EXPECT_TRUE(message.repeated_bool(0));
  EXPECT_EQ("215", message.repeated_string(0));
  EXPECT_EQ("216", message.repeated_bytes(0));

  EXPECT_EQ(217, message.repeatedgroup(0).a());
  EXPECT_EQ(218, message.repeated_nested_message(0).bb());
  EXPECT_EQ(219, message.repeated_foreign_message(0).c());
  EXPECT_EQ(220, message.repeated_import_message(0).d());
  EXPECT_EQ(227, message.repeated_lazy_message(0).bb());


  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message.repeated_nested_enum(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.repeated_foreign_enum(0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR, message.repeated_import_enum(0));

  EXPECT_EQ(301, message.repeated_int32(1));
  EXPECT_EQ(302, message.repeated_int64(1));
  EXPECT_EQ(303, message.repeated_uint32(1));
  EXPECT_EQ(304, message.repeated_uint64(1));
  EXPECT_EQ(305, message.repeated_sint32(1));
  EXPECT_EQ(306, message.repeated_sint64(1));
  EXPECT_EQ(307, message.repeated_fixed32(1));
  EXPECT_EQ(308, message.repeated_fixed64(1));
  EXPECT_EQ(309, message.repeated_sfixed32(1));
  EXPECT_EQ(310, message.repeated_sfixed64(1));
  EXPECT_EQ(311, message.repeated_float(1));
  EXPECT_EQ(312, message.repeated_double(1));
  EXPECT_FALSE(message.repeated_bool(1));
  EXPECT_EQ("315", message.repeated_string(1));
  EXPECT_EQ("316", message.repeated_bytes(1));

  EXPECT_EQ(317, message.repeatedgroup(1).a());
  EXPECT_EQ(318, message.repeated_nested_message(1).bb());
  EXPECT_EQ(319, message.repeated_foreign_message(1).c());
  EXPECT_EQ(320, message.repeated_import_message(1).d());
  EXPECT_EQ(327, message.repeated_lazy_message(1).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ, message.repeated_nested_enum(1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ, message.repeated_foreign_enum(1));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAZ, message.repeated_import_enum(1));


  // -----------------------------------------------------------------

  EXPECT_TRUE(message.has_default_int32());
  EXPECT_TRUE(message.has_default_int64());
  EXPECT_TRUE(message.has_default_uint32());
  EXPECT_TRUE(message.has_default_uint64());
  EXPECT_TRUE(message.has_default_sint32());
  EXPECT_TRUE(message.has_default_sint64());
  EXPECT_TRUE(message.has_default_fixed32());
  EXPECT_TRUE(message.has_default_fixed64());
  EXPECT_TRUE(message.has_default_sfixed32());
  EXPECT_TRUE(message.has_default_sfixed64());
  EXPECT_TRUE(message.has_default_float());
  EXPECT_TRUE(message.has_default_double());
  EXPECT_TRUE(message.has_default_bool());
  EXPECT_TRUE(message.has_default_string());
  EXPECT_TRUE(message.has_default_bytes());

  EXPECT_TRUE(message.has_default_nested_enum());
  EXPECT_TRUE(message.has_default_foreign_enum());
  EXPECT_TRUE(message.has_default_import_enum());


  EXPECT_EQ(401, message.default_int32());
  EXPECT_EQ(402, message.default_int64());
  EXPECT_EQ(403, message.default_uint32());
  EXPECT_EQ(404, message.default_uint64());
  EXPECT_EQ(405, message.default_sint32());
  EXPECT_EQ(406, message.default_sint64());
  EXPECT_EQ(407, message.default_fixed32());
  EXPECT_EQ(408, message.default_fixed64());
  EXPECT_EQ(409, message.default_sfixed32());
  EXPECT_EQ(410, message.default_sfixed64());
  EXPECT_EQ(411, message.default_float());
  EXPECT_EQ(412, message.default_double());
  EXPECT_FALSE(message.default_bool());
  EXPECT_EQ("415", message.default_string());
  EXPECT_EQ("416", message.default_bytes());

  EXPECT_EQ(UNITTEST::TestAllTypes::FOO, message.default_nested_enum());
  EXPECT_EQ(UNITTEST::FOREIGN_FOO, message.default_foreign_enum());
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_FOO, message.default_import_enum());


  EXPECT_FALSE(message.has_oneof_uint32());
  EXPECT_FALSE(message.has_oneof_nested_message());
  EXPECT_FALSE(message.has_oneof_string());
  EXPECT_TRUE(message.has_oneof_bytes());

  EXPECT_EQ("604", message.oneof_bytes());
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectClear(const UNITTEST::TestAllTypes& message) {
  // has_blah() should initially be false for all optional fields.
  EXPECT_FALSE(message.has_optional_int32());
  EXPECT_FALSE(message.has_optional_int64());
  EXPECT_FALSE(message.has_optional_uint32());
  EXPECT_FALSE(message.has_optional_uint64());
  EXPECT_FALSE(message.has_optional_sint32());
  EXPECT_FALSE(message.has_optional_sint64());
  EXPECT_FALSE(message.has_optional_fixed32());
  EXPECT_FALSE(message.has_optional_fixed64());
  EXPECT_FALSE(message.has_optional_sfixed32());
  EXPECT_FALSE(message.has_optional_sfixed64());
  EXPECT_FALSE(message.has_optional_float());
  EXPECT_FALSE(message.has_optional_double());
  EXPECT_FALSE(message.has_optional_bool());
  EXPECT_FALSE(message.has_optional_string());
  EXPECT_FALSE(message.has_optional_bytes());

  EXPECT_FALSE(message.has_optionalgroup());
  EXPECT_FALSE(message.has_optional_nested_message());
  EXPECT_FALSE(message.has_optional_foreign_message());
  EXPECT_FALSE(message.has_optional_import_message());
  EXPECT_FALSE(message.has_optional_public_import_message());
  EXPECT_FALSE(message.has_optional_lazy_message());

  EXPECT_FALSE(message.has_optional_nested_enum());
  EXPECT_FALSE(message.has_optional_foreign_enum());
  EXPECT_FALSE(message.has_optional_import_enum());

  EXPECT_FALSE(message.has_optional_string_piece());
  EXPECT_FALSE(message.has_optional_cord());

  // Optional fields without defaults are set to zero or something like it.
  EXPECT_EQ(0, message.optional_int32());
  EXPECT_EQ(0, message.optional_int64());
  EXPECT_EQ(0, message.optional_uint32());
  EXPECT_EQ(0, message.optional_uint64());
  EXPECT_EQ(0, message.optional_sint32());
  EXPECT_EQ(0, message.optional_sint64());
  EXPECT_EQ(0, message.optional_fixed32());
  EXPECT_EQ(0, message.optional_fixed64());
  EXPECT_EQ(0, message.optional_sfixed32());
  EXPECT_EQ(0, message.optional_sfixed64());
  EXPECT_EQ(0, message.optional_float());
  EXPECT_EQ(0, message.optional_double());
  EXPECT_FALSE(message.optional_bool());
  EXPECT_EQ("", message.optional_string());
  EXPECT_EQ("", message.optional_bytes());

  // Embedded messages should also be clear.
  EXPECT_FALSE(message.optionalgroup().has_a());
  EXPECT_FALSE(message.optional_nested_message().has_bb());
  EXPECT_FALSE(message.optional_foreign_message().has_c());
  EXPECT_FALSE(message.optional_import_message().has_d());
  EXPECT_FALSE(message.optional_public_import_message().has_e());
  EXPECT_FALSE(message.optional_lazy_message().has_bb());

  EXPECT_EQ(0, message.optionalgroup().a());
  EXPECT_EQ(0, message.optional_nested_message().bb());
  EXPECT_EQ(0, message.optional_foreign_message().c());
  EXPECT_EQ(0, message.optional_import_message().d());
  EXPECT_EQ(0, message.optional_public_import_message().e());
  EXPECT_EQ(0, message.optional_lazy_message().bb());

  // Enums without defaults are set to the first value in the enum.
  EXPECT_EQ(UNITTEST::TestAllTypes::FOO, message.optional_nested_enum());
  EXPECT_EQ(UNITTEST::FOREIGN_FOO, message.optional_foreign_enum());
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_FOO, message.optional_import_enum());


  // Repeated fields are empty.
  EXPECT_EQ(0, message.repeated_int32_size());
  EXPECT_EQ(0, message.repeated_int64_size());
  EXPECT_EQ(0, message.repeated_uint32_size());
  EXPECT_EQ(0, message.repeated_uint64_size());
  EXPECT_EQ(0, message.repeated_sint32_size());
  EXPECT_EQ(0, message.repeated_sint64_size());
  EXPECT_EQ(0, message.repeated_fixed32_size());
  EXPECT_EQ(0, message.repeated_fixed64_size());
  EXPECT_EQ(0, message.repeated_sfixed32_size());
  EXPECT_EQ(0, message.repeated_sfixed64_size());
  EXPECT_EQ(0, message.repeated_float_size());
  EXPECT_EQ(0, message.repeated_double_size());
  EXPECT_EQ(0, message.repeated_bool_size());
  EXPECT_EQ(0, message.repeated_string_size());
  EXPECT_EQ(0, message.repeated_bytes_size());

  EXPECT_EQ(0, message.repeatedgroup_size());
  EXPECT_EQ(0, message.repeated_nested_message_size());
  EXPECT_EQ(0, message.repeated_foreign_message_size());
  EXPECT_EQ(0, message.repeated_import_message_size());
  EXPECT_EQ(0, message.repeated_lazy_message_size());
  EXPECT_EQ(0, message.repeated_nested_enum_size());
  EXPECT_EQ(0, message.repeated_foreign_enum_size());
  EXPECT_EQ(0, message.repeated_import_enum_size());

  EXPECT_EQ(0, message.repeated_string_piece_size());
  EXPECT_EQ(0, message.repeated_cord_size());

  // has_blah() should also be false for all default fields.
  EXPECT_FALSE(message.has_default_int32());
  EXPECT_FALSE(message.has_default_int64());
  EXPECT_FALSE(message.has_default_uint32());
  EXPECT_FALSE(message.has_default_uint64());
  EXPECT_FALSE(message.has_default_sint32());
  EXPECT_FALSE(message.has_default_sint64());
  EXPECT_FALSE(message.has_default_fixed32());
  EXPECT_FALSE(message.has_default_fixed64());
  EXPECT_FALSE(message.has_default_sfixed32());
  EXPECT_FALSE(message.has_default_sfixed64());
  EXPECT_FALSE(message.has_default_float());
  EXPECT_FALSE(message.has_default_double());
  EXPECT_FALSE(message.has_default_bool());
  EXPECT_FALSE(message.has_default_string());
  EXPECT_FALSE(message.has_default_bytes());

  EXPECT_FALSE(message.has_default_nested_enum());
  EXPECT_FALSE(message.has_default_foreign_enum());
  EXPECT_FALSE(message.has_default_import_enum());


  // Fields with defaults have their default values (duh).
  EXPECT_EQ(41, message.default_int32());
  EXPECT_EQ(42, message.default_int64());
  EXPECT_EQ(43, message.default_uint32());
  EXPECT_EQ(44, message.default_uint64());
  EXPECT_EQ(-45, message.default_sint32());
  EXPECT_EQ(46, message.default_sint64());
  EXPECT_EQ(47, message.default_fixed32());
  EXPECT_EQ(48, message.default_fixed64());
  EXPECT_EQ(49, message.default_sfixed32());
  EXPECT_EQ(-50, message.default_sfixed64());
  EXPECT_EQ(51.5, message.default_float());
  EXPECT_EQ(52e3, message.default_double());
  EXPECT_TRUE(message.default_bool());
  EXPECT_EQ("hello", message.default_string());
  EXPECT_EQ("world", message.default_bytes());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message.default_nested_enum());
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.default_foreign_enum());
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR, message.default_import_enum());


  EXPECT_FALSE(message.has_oneof_uint32());
  EXPECT_FALSE(message.has_oneof_nested_message());
  EXPECT_FALSE(message.has_oneof_string());
  EXPECT_FALSE(message.has_oneof_bytes());
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectRepeatedFieldsModified(
    const UNITTEST::TestAllTypes& message) {
  // ModifyRepeatedFields only sets the second repeated element of each
  // field.  In addition to verifying this, we also verify that the first
  // element and size were *not* modified.
  ASSERT_EQ(2, message.repeated_int32_size());
  ASSERT_EQ(2, message.repeated_int64_size());
  ASSERT_EQ(2, message.repeated_uint32_size());
  ASSERT_EQ(2, message.repeated_uint64_size());
  ASSERT_EQ(2, message.repeated_sint32_size());
  ASSERT_EQ(2, message.repeated_sint64_size());
  ASSERT_EQ(2, message.repeated_fixed32_size());
  ASSERT_EQ(2, message.repeated_fixed64_size());
  ASSERT_EQ(2, message.repeated_sfixed32_size());
  ASSERT_EQ(2, message.repeated_sfixed64_size());
  ASSERT_EQ(2, message.repeated_float_size());
  ASSERT_EQ(2, message.repeated_double_size());
  ASSERT_EQ(2, message.repeated_bool_size());
  ASSERT_EQ(2, message.repeated_string_size());
  ASSERT_EQ(2, message.repeated_bytes_size());

  ASSERT_EQ(2, message.repeatedgroup_size());
  ASSERT_EQ(2, message.repeated_nested_message_size());
  ASSERT_EQ(2, message.repeated_foreign_message_size());
  ASSERT_EQ(2, message.repeated_import_message_size());
  ASSERT_EQ(2, message.repeated_lazy_message_size());
  ASSERT_EQ(2, message.repeated_nested_enum_size());
  ASSERT_EQ(2, message.repeated_foreign_enum_size());
  ASSERT_EQ(2, message.repeated_import_enum_size());

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  ASSERT_EQ(2, message.repeated_string_piece_size());
  ASSERT_EQ(2, message.repeated_cord_size());
#endif

  EXPECT_EQ(201, message.repeated_int32(0));
  EXPECT_EQ(202, message.repeated_int64(0));
  EXPECT_EQ(203, message.repeated_uint32(0));
  EXPECT_EQ(204, message.repeated_uint64(0));
  EXPECT_EQ(205, message.repeated_sint32(0));
  EXPECT_EQ(206, message.repeated_sint64(0));
  EXPECT_EQ(207, message.repeated_fixed32(0));
  EXPECT_EQ(208, message.repeated_fixed64(0));
  EXPECT_EQ(209, message.repeated_sfixed32(0));
  EXPECT_EQ(210, message.repeated_sfixed64(0));
  EXPECT_EQ(211, message.repeated_float(0));
  EXPECT_EQ(212, message.repeated_double(0));
  EXPECT_TRUE(message.repeated_bool(0));
  EXPECT_EQ("215", message.repeated_string(0));
  EXPECT_EQ("216", message.repeated_bytes(0));

  EXPECT_EQ(217, message.repeatedgroup(0).a());
  EXPECT_EQ(218, message.repeated_nested_message(0).bb());
  EXPECT_EQ(219, message.repeated_foreign_message(0).c());
  EXPECT_EQ(220, message.repeated_import_message(0).d());
  EXPECT_EQ(227, message.repeated_lazy_message(0).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message.repeated_nested_enum(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.repeated_foreign_enum(0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR, message.repeated_import_enum(0));


  // Actually verify the second (modified) elements now.
  EXPECT_EQ(501, message.repeated_int32(1));
  EXPECT_EQ(502, message.repeated_int64(1));
  EXPECT_EQ(503, message.repeated_uint32(1));
  EXPECT_EQ(504, message.repeated_uint64(1));
  EXPECT_EQ(505, message.repeated_sint32(1));
  EXPECT_EQ(506, message.repeated_sint64(1));
  EXPECT_EQ(507, message.repeated_fixed32(1));
  EXPECT_EQ(508, message.repeated_fixed64(1));
  EXPECT_EQ(509, message.repeated_sfixed32(1));
  EXPECT_EQ(510, message.repeated_sfixed64(1));
  EXPECT_EQ(511, message.repeated_float(1));
  EXPECT_EQ(512, message.repeated_double(1));
  EXPECT_TRUE(message.repeated_bool(1));
  EXPECT_EQ("515", message.repeated_string(1));
  EXPECT_EQ("516", message.repeated_bytes(1));

  EXPECT_EQ(517, message.repeatedgroup(1).a());
  EXPECT_EQ(518, message.repeated_nested_message(1).bb());
  EXPECT_EQ(519, message.repeated_foreign_message(1).c());
  EXPECT_EQ(520, message.repeated_import_message(1).d());
  EXPECT_EQ(527, message.repeated_lazy_message(1).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::FOO, message.repeated_nested_enum(1));
  EXPECT_EQ(UNITTEST::FOREIGN_FOO, message.repeated_foreign_enum(1));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_FOO, message.repeated_import_enum(1));

}

// -------------------------------------------------------------------

inline void TestUtil::SetPackedFields(UNITTEST::TestPackedTypes* message) {
  message->add_packed_int32(601);
  message->add_packed_int64(602);
  message->add_packed_uint32(603);
  message->add_packed_uint64(604);
  message->add_packed_sint32(605);
  message->add_packed_sint64(606);
  message->add_packed_fixed32(607);
  message->add_packed_fixed64(608);
  message->add_packed_sfixed32(609);
  message->add_packed_sfixed64(610);
  message->add_packed_float(611);
  message->add_packed_double(612);
  message->add_packed_bool(true);
  message->add_packed_enum(UNITTEST::FOREIGN_BAR);
  // add a second one of each field
  message->add_packed_int32(701);
  message->add_packed_int64(702);
  message->add_packed_uint32(703);
  message->add_packed_uint64(704);
  message->add_packed_sint32(705);
  message->add_packed_sint64(706);
  message->add_packed_fixed32(707);
  message->add_packed_fixed64(708);
  message->add_packed_sfixed32(709);
  message->add_packed_sfixed64(710);
  message->add_packed_float(711);
  message->add_packed_double(712);
  message->add_packed_bool(false);
  message->add_packed_enum(UNITTEST::FOREIGN_BAZ);
}

inline void TestUtil::SetUnpackedFields(UNITTEST::TestUnpackedTypes* message) {
  // The values applied here must match those of SetPackedFields.

  message->add_unpacked_int32(601);
  message->add_unpacked_int64(602);
  message->add_unpacked_uint32(603);
  message->add_unpacked_uint64(604);
  message->add_unpacked_sint32(605);
  message->add_unpacked_sint64(606);
  message->add_unpacked_fixed32(607);
  message->add_unpacked_fixed64(608);
  message->add_unpacked_sfixed32(609);
  message->add_unpacked_sfixed64(610);
  message->add_unpacked_float(611);
  message->add_unpacked_double(612);
  message->add_unpacked_bool(true);
  message->add_unpacked_enum(UNITTEST::FOREIGN_BAR);
  // add a second one of each field
  message->add_unpacked_int32(701);
  message->add_unpacked_int64(702);
  message->add_unpacked_uint32(703);
  message->add_unpacked_uint64(704);
  message->add_unpacked_sint32(705);
  message->add_unpacked_sint64(706);
  message->add_unpacked_fixed32(707);
  message->add_unpacked_fixed64(708);
  message->add_unpacked_sfixed32(709);
  message->add_unpacked_sfixed64(710);
  message->add_unpacked_float(711);
  message->add_unpacked_double(712);
  message->add_unpacked_bool(false);
  message->add_unpacked_enum(UNITTEST::FOREIGN_BAZ);
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectPackedFieldsSet(
    const UNITTEST::TestPackedTypes& message) {
  ASSERT_EQ(2, message.packed_int32_size());
  ASSERT_EQ(2, message.packed_int64_size());
  ASSERT_EQ(2, message.packed_uint32_size());
  ASSERT_EQ(2, message.packed_uint64_size());
  ASSERT_EQ(2, message.packed_sint32_size());
  ASSERT_EQ(2, message.packed_sint64_size());
  ASSERT_EQ(2, message.packed_fixed32_size());
  ASSERT_EQ(2, message.packed_fixed64_size());
  ASSERT_EQ(2, message.packed_sfixed32_size());
  ASSERT_EQ(2, message.packed_sfixed64_size());
  ASSERT_EQ(2, message.packed_float_size());
  ASSERT_EQ(2, message.packed_double_size());
  ASSERT_EQ(2, message.packed_bool_size());
  ASSERT_EQ(2, message.packed_enum_size());

  EXPECT_EQ(601, message.packed_int32(0));
  EXPECT_EQ(602, message.packed_int64(0));
  EXPECT_EQ(603, message.packed_uint32(0));
  EXPECT_EQ(604, message.packed_uint64(0));
  EXPECT_EQ(605, message.packed_sint32(0));
  EXPECT_EQ(606, message.packed_sint64(0));
  EXPECT_EQ(607, message.packed_fixed32(0));
  EXPECT_EQ(608, message.packed_fixed64(0));
  EXPECT_EQ(609, message.packed_sfixed32(0));
  EXPECT_EQ(610, message.packed_sfixed64(0));
  EXPECT_EQ(611, message.packed_float(0));
  EXPECT_EQ(612, message.packed_double(0));
  EXPECT_TRUE(message.packed_bool(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.packed_enum(0));

  EXPECT_EQ(701, message.packed_int32(1));
  EXPECT_EQ(702, message.packed_int64(1));
  EXPECT_EQ(703, message.packed_uint32(1));
  EXPECT_EQ(704, message.packed_uint64(1));
  EXPECT_EQ(705, message.packed_sint32(1));
  EXPECT_EQ(706, message.packed_sint64(1));
  EXPECT_EQ(707, message.packed_fixed32(1));
  EXPECT_EQ(708, message.packed_fixed64(1));
  EXPECT_EQ(709, message.packed_sfixed32(1));
  EXPECT_EQ(710, message.packed_sfixed64(1));
  EXPECT_EQ(711, message.packed_float(1));
  EXPECT_EQ(712, message.packed_double(1));
  EXPECT_FALSE(message.packed_bool(1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ, message.packed_enum(1));
}

inline void TestUtil::ExpectUnpackedFieldsSet(
    const UNITTEST::TestUnpackedTypes& message) {
  // The values expected here must match those of ExpectPackedFieldsSet.

  ASSERT_EQ(2, message.unpacked_int32_size());
  ASSERT_EQ(2, message.unpacked_int64_size());
  ASSERT_EQ(2, message.unpacked_uint32_size());
  ASSERT_EQ(2, message.unpacked_uint64_size());
  ASSERT_EQ(2, message.unpacked_sint32_size());
  ASSERT_EQ(2, message.unpacked_sint64_size());
  ASSERT_EQ(2, message.unpacked_fixed32_size());
  ASSERT_EQ(2, message.unpacked_fixed64_size());
  ASSERT_EQ(2, message.unpacked_sfixed32_size());
  ASSERT_EQ(2, message.unpacked_sfixed64_size());
  ASSERT_EQ(2, message.unpacked_float_size());
  ASSERT_EQ(2, message.unpacked_double_size());
  ASSERT_EQ(2, message.unpacked_bool_size());
  ASSERT_EQ(2, message.unpacked_enum_size());

  EXPECT_EQ(601, message.unpacked_int32(0));
  EXPECT_EQ(602, message.unpacked_int64(0));
  EXPECT_EQ(603, message.unpacked_uint32(0));
  EXPECT_EQ(604, message.unpacked_uint64(0));
  EXPECT_EQ(605, message.unpacked_sint32(0));
  EXPECT_EQ(606, message.unpacked_sint64(0));
  EXPECT_EQ(607, message.unpacked_fixed32(0));
  EXPECT_EQ(608, message.unpacked_fixed64(0));
  EXPECT_EQ(609, message.unpacked_sfixed32(0));
  EXPECT_EQ(610, message.unpacked_sfixed64(0));
  EXPECT_EQ(611, message.unpacked_float(0));
  EXPECT_EQ(612, message.unpacked_double(0));
  EXPECT_TRUE(message.unpacked_bool(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.unpacked_enum(0));

  EXPECT_EQ(701, message.unpacked_int32(1));
  EXPECT_EQ(702, message.unpacked_int64(1));
  EXPECT_EQ(703, message.unpacked_uint32(1));
  EXPECT_EQ(704, message.unpacked_uint64(1));
  EXPECT_EQ(705, message.unpacked_sint32(1));
  EXPECT_EQ(706, message.unpacked_sint64(1));
  EXPECT_EQ(707, message.unpacked_fixed32(1));
  EXPECT_EQ(708, message.unpacked_fixed64(1));
  EXPECT_EQ(709, message.unpacked_sfixed32(1));
  EXPECT_EQ(710, message.unpacked_sfixed64(1));
  EXPECT_EQ(711, message.unpacked_float(1));
  EXPECT_EQ(712, message.unpacked_double(1));
  EXPECT_FALSE(message.unpacked_bool(1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ, message.unpacked_enum(1));
}

// ===================================================================
// Extensions
//
// All this code is exactly equivalent to the above code except that it's
// manipulating extension fields instead of normal ones.

inline void TestUtil::SetAllExtensions(UNITTEST::TestAllExtensions* message) {
  message->SetExtension(UNITTEST::optional_int32_extension, 101);
  message->SetExtension(UNITTEST::optional_int64_extension, 102);
  message->SetExtension(UNITTEST::optional_uint32_extension, 103);
  message->SetExtension(UNITTEST::optional_uint64_extension, 104);
  message->SetExtension(UNITTEST::optional_sint32_extension, 105);
  message->SetExtension(UNITTEST::optional_sint64_extension, 106);
  message->SetExtension(UNITTEST::optional_fixed32_extension, 107);
  message->SetExtension(UNITTEST::optional_fixed64_extension, 108);
  message->SetExtension(UNITTEST::optional_sfixed32_extension, 109);
  message->SetExtension(UNITTEST::optional_sfixed64_extension, 110);
  message->SetExtension(UNITTEST::optional_float_extension, 111);
  message->SetExtension(UNITTEST::optional_double_extension, 112);
  message->SetExtension(UNITTEST::optional_bool_extension, true);
  message->SetExtension(UNITTEST::optional_string_extension, "115");
  message->SetExtension(UNITTEST::optional_bytes_extension, "116");

  message->MutableExtension(UNITTEST::optionalgroup_extension)->set_a(117);
  message->MutableExtension(UNITTEST::optional_nested_message_extension)
      ->set_bb(118);
  message->MutableExtension(UNITTEST::optional_foreign_message_extension)
      ->set_c(119);
  message->MutableExtension(UNITTEST::optional_import_message_extension)
      ->set_d(120);

  message->SetExtension(UNITTEST::optional_nested_enum_extension,
                        UNITTEST::TestAllTypes::BAZ);
  message->SetExtension(UNITTEST::optional_foreign_enum_extension,
                        UNITTEST::FOREIGN_BAZ);
  message->SetExtension(UNITTEST::optional_import_enum_extension,
                        UNITTEST_IMPORT::IMPORT_BAZ);

  message->SetExtension(UNITTEST::optional_string_piece_extension, "124");
  message->SetExtension(UNITTEST::optional_cord_extension, "125");

  message->MutableExtension(UNITTEST::optional_public_import_message_extension)
      ->set_e(126);
  message->MutableExtension(UNITTEST::optional_lazy_message_extension)
      ->set_bb(127);

  // -----------------------------------------------------------------

  message->AddExtension(UNITTEST::repeated_int32_extension, 201);
  message->AddExtension(UNITTEST::repeated_int64_extension, 202);
  message->AddExtension(UNITTEST::repeated_uint32_extension, 203);
  message->AddExtension(UNITTEST::repeated_uint64_extension, 204);
  message->AddExtension(UNITTEST::repeated_sint32_extension, 205);
  message->AddExtension(UNITTEST::repeated_sint64_extension, 206);
  message->AddExtension(UNITTEST::repeated_fixed32_extension, 207);
  message->AddExtension(UNITTEST::repeated_fixed64_extension, 208);
  message->AddExtension(UNITTEST::repeated_sfixed32_extension, 209);
  message->AddExtension(UNITTEST::repeated_sfixed64_extension, 210);
  message->AddExtension(UNITTEST::repeated_float_extension, 211);
  message->AddExtension(UNITTEST::repeated_double_extension, 212);
  message->AddExtension(UNITTEST::repeated_bool_extension, true);
  message->AddExtension(UNITTEST::repeated_string_extension, "215");
  message->AddExtension(UNITTEST::repeated_bytes_extension, "216");

  message->AddExtension(UNITTEST::repeatedgroup_extension)->set_a(217);
  message->AddExtension(UNITTEST::repeated_nested_message_extension)
      ->set_bb(218);
  message->AddExtension(UNITTEST::repeated_foreign_message_extension)
      ->set_c(219);
  message->AddExtension(UNITTEST::repeated_import_message_extension)
      ->set_d(220);
  message->AddExtension(UNITTEST::repeated_lazy_message_extension)->set_bb(227);

  message->AddExtension(UNITTEST::repeated_nested_enum_extension,
                        UNITTEST::TestAllTypes::BAR);
  message->AddExtension(UNITTEST::repeated_foreign_enum_extension,
                        UNITTEST::FOREIGN_BAR);
  message->AddExtension(UNITTEST::repeated_import_enum_extension,
                        UNITTEST_IMPORT::IMPORT_BAR);

  message->AddExtension(UNITTEST::repeated_string_piece_extension, "224");
  message->AddExtension(UNITTEST::repeated_cord_extension, "225");

  // Add a second one of each field.
  message->AddExtension(UNITTEST::repeated_int32_extension, 301);
  message->AddExtension(UNITTEST::repeated_int64_extension, 302);
  message->AddExtension(UNITTEST::repeated_uint32_extension, 303);
  message->AddExtension(UNITTEST::repeated_uint64_extension, 304);
  message->AddExtension(UNITTEST::repeated_sint32_extension, 305);
  message->AddExtension(UNITTEST::repeated_sint64_extension, 306);
  message->AddExtension(UNITTEST::repeated_fixed32_extension, 307);
  message->AddExtension(UNITTEST::repeated_fixed64_extension, 308);
  message->AddExtension(UNITTEST::repeated_sfixed32_extension, 309);
  message->AddExtension(UNITTEST::repeated_sfixed64_extension, 310);
  message->AddExtension(UNITTEST::repeated_float_extension, 311);
  message->AddExtension(UNITTEST::repeated_double_extension, 312);
  message->AddExtension(UNITTEST::repeated_bool_extension, false);
  message->AddExtension(UNITTEST::repeated_string_extension, "315");
  message->AddExtension(UNITTEST::repeated_bytes_extension, "316");

  message->AddExtension(UNITTEST::repeatedgroup_extension)->set_a(317);
  message->AddExtension(UNITTEST::repeated_nested_message_extension)
      ->set_bb(318);
  message->AddExtension(UNITTEST::repeated_foreign_message_extension)
      ->set_c(319);
  message->AddExtension(UNITTEST::repeated_import_message_extension)
      ->set_d(320);
  message->AddExtension(UNITTEST::repeated_lazy_message_extension)->set_bb(327);

  message->AddExtension(UNITTEST::repeated_nested_enum_extension,
                        UNITTEST::TestAllTypes::BAZ);
  message->AddExtension(UNITTEST::repeated_foreign_enum_extension,
                        UNITTEST::FOREIGN_BAZ);
  message->AddExtension(UNITTEST::repeated_import_enum_extension,
                        UNITTEST_IMPORT::IMPORT_BAZ);

  message->AddExtension(UNITTEST::repeated_string_piece_extension, "324");
  message->AddExtension(UNITTEST::repeated_cord_extension, "325");

  // -----------------------------------------------------------------

  message->SetExtension(UNITTEST::default_int32_extension, 401);
  message->SetExtension(UNITTEST::default_int64_extension, 402);
  message->SetExtension(UNITTEST::default_uint32_extension, 403);
  message->SetExtension(UNITTEST::default_uint64_extension, 404);
  message->SetExtension(UNITTEST::default_sint32_extension, 405);
  message->SetExtension(UNITTEST::default_sint64_extension, 406);
  message->SetExtension(UNITTEST::default_fixed32_extension, 407);
  message->SetExtension(UNITTEST::default_fixed64_extension, 408);
  message->SetExtension(UNITTEST::default_sfixed32_extension, 409);
  message->SetExtension(UNITTEST::default_sfixed64_extension, 410);
  message->SetExtension(UNITTEST::default_float_extension, 411);
  message->SetExtension(UNITTEST::default_double_extension, 412);
  message->SetExtension(UNITTEST::default_bool_extension, false);
  message->SetExtension(UNITTEST::default_string_extension, "415");
  message->SetExtension(UNITTEST::default_bytes_extension, "416");

  message->SetExtension(UNITTEST::default_nested_enum_extension,
                        UNITTEST::TestAllTypes::FOO);
  message->SetExtension(UNITTEST::default_foreign_enum_extension,
                        UNITTEST::FOREIGN_FOO);
  message->SetExtension(UNITTEST::default_import_enum_extension,
                        UNITTEST_IMPORT::IMPORT_FOO);

  message->SetExtension(UNITTEST::default_string_piece_extension, "424");
  message->SetExtension(UNITTEST::default_cord_extension, "425");

  SetOneofFields(message);
}

inline void TestUtil::SetOneofFields(UNITTEST::TestAllExtensions* message) {
  message->SetExtension(UNITTEST::oneof_uint32_extension, 601);
  message->MutableExtension(UNITTEST::oneof_nested_message_extension)
      ->set_bb(602);
  message->SetExtension(UNITTEST::oneof_string_extension, "603");
  message->SetExtension(UNITTEST::oneof_bytes_extension, "604");
}

// -------------------------------------------------------------------

inline void TestUtil::SetAllFieldsAndExtensions(
    UNITTEST::TestFieldOrderings* message) {
  GOOGLE_CHECK(message);
  message->set_my_int(1);
  message->set_my_string("foo");
  message->set_my_float(1.0);
  message->SetExtension(UNITTEST::my_extension_int, 23);
  message->SetExtension(UNITTEST::my_extension_string, "bar");
}

// -------------------------------------------------------------------

inline void TestUtil::ModifyRepeatedExtensions(
    UNITTEST::TestAllExtensions* message) {
  message->SetExtension(UNITTEST::repeated_int32_extension, 1, 501);
  message->SetExtension(UNITTEST::repeated_int64_extension, 1, 502);
  message->SetExtension(UNITTEST::repeated_uint32_extension, 1, 503);
  message->SetExtension(UNITTEST::repeated_uint64_extension, 1, 504);
  message->SetExtension(UNITTEST::repeated_sint32_extension, 1, 505);
  message->SetExtension(UNITTEST::repeated_sint64_extension, 1, 506);
  message->SetExtension(UNITTEST::repeated_fixed32_extension, 1, 507);
  message->SetExtension(UNITTEST::repeated_fixed64_extension, 1, 508);
  message->SetExtension(UNITTEST::repeated_sfixed32_extension, 1, 509);
  message->SetExtension(UNITTEST::repeated_sfixed64_extension, 1, 510);
  message->SetExtension(UNITTEST::repeated_float_extension, 1, 511);
  message->SetExtension(UNITTEST::repeated_double_extension, 1, 512);
  message->SetExtension(UNITTEST::repeated_bool_extension, 1, true);
  message->SetExtension(UNITTEST::repeated_string_extension, 1, "515");
  message->SetExtension(UNITTEST::repeated_bytes_extension, 1, "516");

  message->MutableExtension(UNITTEST::repeatedgroup_extension, 1)->set_a(517);
  message->MutableExtension(UNITTEST::repeated_nested_message_extension, 1)
      ->set_bb(518);
  message->MutableExtension(UNITTEST::repeated_foreign_message_extension, 1)
      ->set_c(519);
  message->MutableExtension(UNITTEST::repeated_import_message_extension, 1)
      ->set_d(520);
  message->MutableExtension(UNITTEST::repeated_lazy_message_extension, 1)
      ->set_bb(527);

  message->SetExtension(UNITTEST::repeated_nested_enum_extension, 1,
                        UNITTEST::TestAllTypes::FOO);
  message->SetExtension(UNITTEST::repeated_foreign_enum_extension, 1,
                        UNITTEST::FOREIGN_FOO);
  message->SetExtension(UNITTEST::repeated_import_enum_extension, 1,
                        UNITTEST_IMPORT::IMPORT_FOO);

  message->SetExtension(UNITTEST::repeated_string_piece_extension, 1, "524");
  message->SetExtension(UNITTEST::repeated_cord_extension, 1, "525");
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectAllExtensionsSet(
    const UNITTEST::TestAllExtensions& message) {
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_int32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_int64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_uint32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_uint64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_sint32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_sint64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_fixed32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_fixed64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_sfixed32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_sfixed64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_float_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_double_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_bool_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_string_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_bytes_extension));

  EXPECT_TRUE(message.HasExtension(UNITTEST::optionalgroup_extension));
  EXPECT_TRUE(
      message.HasExtension(UNITTEST::optional_nested_message_extension));
  EXPECT_TRUE(
      message.HasExtension(UNITTEST::optional_foreign_message_extension));
  EXPECT_TRUE(
      message.HasExtension(UNITTEST::optional_import_message_extension));
  EXPECT_TRUE(
      message.HasExtension(UNITTEST::optional_public_import_message_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_lazy_message_extension));

  EXPECT_TRUE(message.GetExtension(UNITTEST::optionalgroup_extension).has_a());
  EXPECT_TRUE(message.GetExtension(UNITTEST::optional_nested_message_extension)
                  .has_bb());
  EXPECT_TRUE(message.GetExtension(UNITTEST::optional_foreign_message_extension)
                  .has_c());
  EXPECT_TRUE(message.GetExtension(UNITTEST::optional_import_message_extension)
                  .has_d());
  EXPECT_TRUE(
      message.GetExtension(UNITTEST::optional_public_import_message_extension)
          .has_e());
  EXPECT_TRUE(
      message.GetExtension(UNITTEST::optional_lazy_message_extension).has_bb());

  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_nested_enum_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_foreign_enum_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_import_enum_extension));

  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_string_piece_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::optional_cord_extension));

  EXPECT_EQ(101, message.GetExtension(UNITTEST::optional_int32_extension));
  EXPECT_EQ(102, message.GetExtension(UNITTEST::optional_int64_extension));
  EXPECT_EQ(103, message.GetExtension(UNITTEST::optional_uint32_extension));
  EXPECT_EQ(104, message.GetExtension(UNITTEST::optional_uint64_extension));
  EXPECT_EQ(105, message.GetExtension(UNITTEST::optional_sint32_extension));
  EXPECT_EQ(106, message.GetExtension(UNITTEST::optional_sint64_extension));
  EXPECT_EQ(107, message.GetExtension(UNITTEST::optional_fixed32_extension));
  EXPECT_EQ(108, message.GetExtension(UNITTEST::optional_fixed64_extension));
  EXPECT_EQ(109, message.GetExtension(UNITTEST::optional_sfixed32_extension));
  EXPECT_EQ(110, message.GetExtension(UNITTEST::optional_sfixed64_extension));
  EXPECT_EQ(111, message.GetExtension(UNITTEST::optional_float_extension));
  EXPECT_EQ(112, message.GetExtension(UNITTEST::optional_double_extension));
  EXPECT_TRUE(message.GetExtension(UNITTEST::optional_bool_extension));
  EXPECT_EQ("115", message.GetExtension(UNITTEST::optional_string_extension));
  EXPECT_EQ("116", message.GetExtension(UNITTEST::optional_bytes_extension));

  EXPECT_EQ(117, message.GetExtension(UNITTEST::optionalgroup_extension).a());
  EXPECT_EQ(
      118,
      message.GetExtension(UNITTEST::optional_nested_message_extension).bb());
  EXPECT_EQ(
      119,
      message.GetExtension(UNITTEST::optional_foreign_message_extension).c());
  EXPECT_EQ(
      120,
      message.GetExtension(UNITTEST::optional_import_message_extension).d());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ,
            message.GetExtension(UNITTEST::optional_nested_enum_extension));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ,
            message.GetExtension(UNITTEST::optional_foreign_enum_extension));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAZ,
            message.GetExtension(UNITTEST::optional_import_enum_extension));

  EXPECT_EQ("124",
            message.GetExtension(UNITTEST::optional_string_piece_extension));
  EXPECT_EQ("125", message.GetExtension(UNITTEST::optional_cord_extension));
  EXPECT_EQ(
      126,
      message.GetExtension(UNITTEST::optional_public_import_message_extension)
          .e());
  EXPECT_EQ(
      127,
      message.GetExtension(UNITTEST::optional_lazy_message_extension).bb());

  // -----------------------------------------------------------------

  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_int32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_int64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_uint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_uint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_fixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_fixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sfixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sfixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_float_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_double_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_bool_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_string_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_bytes_extension));

  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeatedgroup_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_nested_message_extension));
  ASSERT_EQ(
      2, message.ExtensionSize(UNITTEST::repeated_foreign_message_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_import_message_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_lazy_message_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_nested_enum_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_foreign_enum_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_import_enum_extension));

  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_string_piece_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_cord_extension));

  EXPECT_EQ(201, message.GetExtension(UNITTEST::repeated_int32_extension, 0));
  EXPECT_EQ(202, message.GetExtension(UNITTEST::repeated_int64_extension, 0));
  EXPECT_EQ(203, message.GetExtension(UNITTEST::repeated_uint32_extension, 0));
  EXPECT_EQ(204, message.GetExtension(UNITTEST::repeated_uint64_extension, 0));
  EXPECT_EQ(205, message.GetExtension(UNITTEST::repeated_sint32_extension, 0));
  EXPECT_EQ(206, message.GetExtension(UNITTEST::repeated_sint64_extension, 0));
  EXPECT_EQ(207, message.GetExtension(UNITTEST::repeated_fixed32_extension, 0));
  EXPECT_EQ(208, message.GetExtension(UNITTEST::repeated_fixed64_extension, 0));
  EXPECT_EQ(209,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 0));
  EXPECT_EQ(210,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 0));
  EXPECT_EQ(211, message.GetExtension(UNITTEST::repeated_float_extension, 0));
  EXPECT_EQ(212, message.GetExtension(UNITTEST::repeated_double_extension, 0));
  EXPECT_TRUE(message.GetExtension(UNITTEST::repeated_bool_extension, 0));
  EXPECT_EQ("215",
            message.GetExtension(UNITTEST::repeated_string_extension, 0));
  EXPECT_EQ("216", message.GetExtension(UNITTEST::repeated_bytes_extension, 0));

  EXPECT_EQ(217,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 0).a());
  EXPECT_EQ(218,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 0)
                .bb());
  EXPECT_EQ(
      219, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 0)
               .c());
  EXPECT_EQ(
      220,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 0).d());
  EXPECT_EQ(
      227,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 0).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 0));

  EXPECT_EQ("224",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 0));
  EXPECT_EQ("225", message.GetExtension(UNITTEST::repeated_cord_extension, 0));

  EXPECT_EQ(301, message.GetExtension(UNITTEST::repeated_int32_extension, 1));
  EXPECT_EQ(302, message.GetExtension(UNITTEST::repeated_int64_extension, 1));
  EXPECT_EQ(303, message.GetExtension(UNITTEST::repeated_uint32_extension, 1));
  EXPECT_EQ(304, message.GetExtension(UNITTEST::repeated_uint64_extension, 1));
  EXPECT_EQ(305, message.GetExtension(UNITTEST::repeated_sint32_extension, 1));
  EXPECT_EQ(306, message.GetExtension(UNITTEST::repeated_sint64_extension, 1));
  EXPECT_EQ(307, message.GetExtension(UNITTEST::repeated_fixed32_extension, 1));
  EXPECT_EQ(308, message.GetExtension(UNITTEST::repeated_fixed64_extension, 1));
  EXPECT_EQ(309,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 1));
  EXPECT_EQ(310,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 1));
  EXPECT_EQ(311, message.GetExtension(UNITTEST::repeated_float_extension, 1));
  EXPECT_EQ(312, message.GetExtension(UNITTEST::repeated_double_extension, 1));
  EXPECT_FALSE(message.GetExtension(UNITTEST::repeated_bool_extension, 1));
  EXPECT_EQ("315",
            message.GetExtension(UNITTEST::repeated_string_extension, 1));
  EXPECT_EQ("316", message.GetExtension(UNITTEST::repeated_bytes_extension, 1));

  EXPECT_EQ(317,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 1).a());
  EXPECT_EQ(318,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 1)
                .bb());
  EXPECT_EQ(
      319, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 1)
               .c());
  EXPECT_EQ(
      320,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 1).d());
  EXPECT_EQ(
      327,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 1).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 1));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAZ,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 1));

  EXPECT_EQ("324",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 1));
  EXPECT_EQ("325", message.GetExtension(UNITTEST::repeated_cord_extension, 1));

  // -----------------------------------------------------------------

  EXPECT_TRUE(message.HasExtension(UNITTEST::default_int32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_int64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_uint32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_uint64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_sint32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_sint64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_fixed32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_fixed64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_sfixed32_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_sfixed64_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_float_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_double_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_bool_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_string_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_bytes_extension));

  EXPECT_TRUE(message.HasExtension(UNITTEST::default_nested_enum_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_foreign_enum_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_import_enum_extension));

  EXPECT_TRUE(message.HasExtension(UNITTEST::default_string_piece_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::default_cord_extension));

  EXPECT_EQ(401, message.GetExtension(UNITTEST::default_int32_extension));
  EXPECT_EQ(402, message.GetExtension(UNITTEST::default_int64_extension));
  EXPECT_EQ(403, message.GetExtension(UNITTEST::default_uint32_extension));
  EXPECT_EQ(404, message.GetExtension(UNITTEST::default_uint64_extension));
  EXPECT_EQ(405, message.GetExtension(UNITTEST::default_sint32_extension));
  EXPECT_EQ(406, message.GetExtension(UNITTEST::default_sint64_extension));
  EXPECT_EQ(407, message.GetExtension(UNITTEST::default_fixed32_extension));
  EXPECT_EQ(408, message.GetExtension(UNITTEST::default_fixed64_extension));
  EXPECT_EQ(409, message.GetExtension(UNITTEST::default_sfixed32_extension));
  EXPECT_EQ(410, message.GetExtension(UNITTEST::default_sfixed64_extension));
  EXPECT_EQ(411, message.GetExtension(UNITTEST::default_float_extension));
  EXPECT_EQ(412, message.GetExtension(UNITTEST::default_double_extension));
  EXPECT_FALSE(message.GetExtension(UNITTEST::default_bool_extension));
  EXPECT_EQ("415", message.GetExtension(UNITTEST::default_string_extension));
  EXPECT_EQ("416", message.GetExtension(UNITTEST::default_bytes_extension));

  EXPECT_EQ(UNITTEST::TestAllTypes::FOO,
            message.GetExtension(UNITTEST::default_nested_enum_extension));
  EXPECT_EQ(UNITTEST::FOREIGN_FOO,
            message.GetExtension(UNITTEST::default_foreign_enum_extension));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_FOO,
            message.GetExtension(UNITTEST::default_import_enum_extension));

  EXPECT_EQ("424",
            message.GetExtension(UNITTEST::default_string_piece_extension));
  EXPECT_EQ("425", message.GetExtension(UNITTEST::default_cord_extension));

  EXPECT_TRUE(message.HasExtension(UNITTEST::oneof_uint32_extension));
  EXPECT_TRUE(
      message.GetExtension(UNITTEST::oneof_nested_message_extension).has_bb());
  EXPECT_TRUE(message.HasExtension(UNITTEST::oneof_string_extension));
  EXPECT_TRUE(message.HasExtension(UNITTEST::oneof_bytes_extension));

  EXPECT_EQ(601, message.GetExtension(UNITTEST::oneof_uint32_extension));
  EXPECT_EQ(
      602, message.GetExtension(UNITTEST::oneof_nested_message_extension).bb());
  EXPECT_EQ("603", message.GetExtension(UNITTEST::oneof_string_extension));
  EXPECT_EQ("604", message.GetExtension(UNITTEST::oneof_bytes_extension));
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectExtensionsClear(
    const UNITTEST::TestAllExtensions& message) {
  std::string serialized;
  ASSERT_TRUE(message.SerializeToString(&serialized));
  EXPECT_EQ("", serialized);
  EXPECT_EQ(0, message.ByteSizeLong());

  // has_blah() should initially be false for all optional fields.
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_int32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_int64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_uint32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_uint64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_sint32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_sint64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_fixed32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_fixed64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_sfixed32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_sfixed64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_float_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_double_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_bool_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_string_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_bytes_extension));

  EXPECT_FALSE(message.HasExtension(UNITTEST::optionalgroup_extension));
  EXPECT_FALSE(
      message.HasExtension(UNITTEST::optional_nested_message_extension));
  EXPECT_FALSE(
      message.HasExtension(UNITTEST::optional_foreign_message_extension));
  EXPECT_FALSE(
      message.HasExtension(UNITTEST::optional_import_message_extension));
  EXPECT_FALSE(
      message.HasExtension(UNITTEST::optional_public_import_message_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_lazy_message_extension));

  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_nested_enum_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_foreign_enum_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_import_enum_extension));

  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_string_piece_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::optional_cord_extension));

  // Optional fields without defaults are set to zero or something like it.
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_int32_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_int64_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_uint32_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_uint64_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_sint32_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_sint64_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_fixed32_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_fixed64_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_sfixed32_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_sfixed64_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_float_extension));
  EXPECT_EQ(0, message.GetExtension(UNITTEST::optional_double_extension));
  EXPECT_FALSE(message.GetExtension(UNITTEST::optional_bool_extension));
  EXPECT_EQ("", message.GetExtension(UNITTEST::optional_string_extension));
  EXPECT_EQ("", message.GetExtension(UNITTEST::optional_bytes_extension));

  // Embedded messages should also be clear.
  EXPECT_FALSE(message.GetExtension(UNITTEST::optionalgroup_extension).has_a());
  EXPECT_FALSE(message.GetExtension(UNITTEST::optional_nested_message_extension)
                   .has_bb());
  EXPECT_FALSE(
      message.GetExtension(UNITTEST::optional_foreign_message_extension)
          .has_c());
  EXPECT_FALSE(message.GetExtension(UNITTEST::optional_import_message_extension)
                   .has_d());
  EXPECT_FALSE(
      message.GetExtension(UNITTEST::optional_public_import_message_extension)
          .has_e());
  EXPECT_FALSE(
      message.GetExtension(UNITTEST::optional_lazy_message_extension).has_bb());

  EXPECT_EQ(0, message.GetExtension(UNITTEST::optionalgroup_extension).a());
  EXPECT_EQ(
      0,
      message.GetExtension(UNITTEST::optional_nested_message_extension).bb());
  EXPECT_EQ(
      0,
      message.GetExtension(UNITTEST::optional_foreign_message_extension).c());
  EXPECT_EQ(
      0, message.GetExtension(UNITTEST::optional_import_message_extension).d());
  EXPECT_EQ(
      0,
      message.GetExtension(UNITTEST::optional_public_import_message_extension)
          .e());
  EXPECT_EQ(
      0, message.GetExtension(UNITTEST::optional_lazy_message_extension).bb());

  // Enums without defaults are set to the first value in the enum.
  EXPECT_EQ(UNITTEST::TestAllTypes::FOO,
            message.GetExtension(UNITTEST::optional_nested_enum_extension));
  EXPECT_EQ(UNITTEST::FOREIGN_FOO,
            message.GetExtension(UNITTEST::optional_foreign_enum_extension));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_FOO,
            message.GetExtension(UNITTEST::optional_import_enum_extension));

  EXPECT_EQ("",
            message.GetExtension(UNITTEST::optional_string_piece_extension));
  EXPECT_EQ("", message.GetExtension(UNITTEST::optional_cord_extension));

  // Repeated fields are empty.
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_int32_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_int64_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_uint32_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_uint64_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_sint32_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_sint64_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_fixed32_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_fixed64_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_sfixed32_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_sfixed64_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_float_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_double_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_bool_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_string_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_bytes_extension));

  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeatedgroup_extension));
  EXPECT_EQ(0,
            message.ExtensionSize(UNITTEST::repeated_nested_message_extension));
  EXPECT_EQ(
      0, message.ExtensionSize(UNITTEST::repeated_foreign_message_extension));
  EXPECT_EQ(0,
            message.ExtensionSize(UNITTEST::repeated_import_message_extension));
  EXPECT_EQ(0,
            message.ExtensionSize(UNITTEST::repeated_lazy_message_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_nested_enum_extension));
  EXPECT_EQ(0,
            message.ExtensionSize(UNITTEST::repeated_foreign_enum_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_import_enum_extension));

  EXPECT_EQ(0,
            message.ExtensionSize(UNITTEST::repeated_string_piece_extension));
  EXPECT_EQ(0, message.ExtensionSize(UNITTEST::repeated_cord_extension));

  // has_blah() should also be false for all default fields.
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_int32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_int64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_uint32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_uint64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_sint32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_sint64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_fixed32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_fixed64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_sfixed32_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_sfixed64_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_float_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_double_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_bool_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_string_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_bytes_extension));

  EXPECT_FALSE(message.HasExtension(UNITTEST::default_nested_enum_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_foreign_enum_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_import_enum_extension));

  EXPECT_FALSE(message.HasExtension(UNITTEST::default_string_piece_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::default_cord_extension));

  // Fields with defaults have their default values (duh).
  EXPECT_EQ(41, message.GetExtension(UNITTEST::default_int32_extension));
  EXPECT_EQ(42, message.GetExtension(UNITTEST::default_int64_extension));
  EXPECT_EQ(43, message.GetExtension(UNITTEST::default_uint32_extension));
  EXPECT_EQ(44, message.GetExtension(UNITTEST::default_uint64_extension));
  EXPECT_EQ(-45, message.GetExtension(UNITTEST::default_sint32_extension));
  EXPECT_EQ(46, message.GetExtension(UNITTEST::default_sint64_extension));
  EXPECT_EQ(47, message.GetExtension(UNITTEST::default_fixed32_extension));
  EXPECT_EQ(48, message.GetExtension(UNITTEST::default_fixed64_extension));
  EXPECT_EQ(49, message.GetExtension(UNITTEST::default_sfixed32_extension));
  EXPECT_EQ(-50, message.GetExtension(UNITTEST::default_sfixed64_extension));
  EXPECT_EQ(51.5, message.GetExtension(UNITTEST::default_float_extension));
  EXPECT_EQ(52e3, message.GetExtension(UNITTEST::default_double_extension));
  EXPECT_TRUE(message.GetExtension(UNITTEST::default_bool_extension));
  EXPECT_EQ("hello", message.GetExtension(UNITTEST::default_string_extension));
  EXPECT_EQ("world", message.GetExtension(UNITTEST::default_bytes_extension));

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR,
            message.GetExtension(UNITTEST::default_nested_enum_extension));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::default_foreign_enum_extension));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR,
            message.GetExtension(UNITTEST::default_import_enum_extension));

  EXPECT_EQ("abc",
            message.GetExtension(UNITTEST::default_string_piece_extension));
  EXPECT_EQ("123", message.GetExtension(UNITTEST::default_cord_extension));

  EXPECT_FALSE(message.HasExtension(UNITTEST::oneof_uint32_extension));
  EXPECT_FALSE(
      message.GetExtension(UNITTEST::oneof_nested_message_extension).has_bb());
  EXPECT_FALSE(message.HasExtension(UNITTEST::oneof_string_extension));
  EXPECT_FALSE(message.HasExtension(UNITTEST::oneof_bytes_extension));
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectRepeatedExtensionsModified(
    const UNITTEST::TestAllExtensions& message) {
  // ModifyRepeatedFields only sets the second repeated element of each
  // field.  In addition to verifying this, we also verify that the first
  // element and size were *not* modified.
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_int32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_int64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_uint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_uint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_fixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_fixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sfixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sfixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_float_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_double_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_bool_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_string_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_bytes_extension));

  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeatedgroup_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_nested_message_extension));
  ASSERT_EQ(
      2, message.ExtensionSize(UNITTEST::repeated_foreign_message_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_import_message_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_lazy_message_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_nested_enum_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_foreign_enum_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_import_enum_extension));

  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_string_piece_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_cord_extension));

  EXPECT_EQ(201, message.GetExtension(UNITTEST::repeated_int32_extension, 0));
  EXPECT_EQ(202, message.GetExtension(UNITTEST::repeated_int64_extension, 0));
  EXPECT_EQ(203, message.GetExtension(UNITTEST::repeated_uint32_extension, 0));
  EXPECT_EQ(204, message.GetExtension(UNITTEST::repeated_uint64_extension, 0));
  EXPECT_EQ(205, message.GetExtension(UNITTEST::repeated_sint32_extension, 0));
  EXPECT_EQ(206, message.GetExtension(UNITTEST::repeated_sint64_extension, 0));
  EXPECT_EQ(207, message.GetExtension(UNITTEST::repeated_fixed32_extension, 0));
  EXPECT_EQ(208, message.GetExtension(UNITTEST::repeated_fixed64_extension, 0));
  EXPECT_EQ(209,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 0));
  EXPECT_EQ(210,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 0));
  EXPECT_EQ(211, message.GetExtension(UNITTEST::repeated_float_extension, 0));
  EXPECT_EQ(212, message.GetExtension(UNITTEST::repeated_double_extension, 0));
  EXPECT_TRUE(message.GetExtension(UNITTEST::repeated_bool_extension, 0));
  EXPECT_EQ("215",
            message.GetExtension(UNITTEST::repeated_string_extension, 0));
  EXPECT_EQ("216", message.GetExtension(UNITTEST::repeated_bytes_extension, 0));

  EXPECT_EQ(217,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 0).a());
  EXPECT_EQ(218,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 0)
                .bb());
  EXPECT_EQ(
      219, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 0)
               .c());
  EXPECT_EQ(
      220,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 0).d());
  EXPECT_EQ(
      227,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 0).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 0));

  EXPECT_EQ("224",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 0));
  EXPECT_EQ("225", message.GetExtension(UNITTEST::repeated_cord_extension, 0));

  // Actually verify the second (modified) elements now.
  EXPECT_EQ(501, message.GetExtension(UNITTEST::repeated_int32_extension, 1));
  EXPECT_EQ(502, message.GetExtension(UNITTEST::repeated_int64_extension, 1));
  EXPECT_EQ(503, message.GetExtension(UNITTEST::repeated_uint32_extension, 1));
  EXPECT_EQ(504, message.GetExtension(UNITTEST::repeated_uint64_extension, 1));
  EXPECT_EQ(505, message.GetExtension(UNITTEST::repeated_sint32_extension, 1));
  EXPECT_EQ(506, message.GetExtension(UNITTEST::repeated_sint64_extension, 1));
  EXPECT_EQ(507, message.GetExtension(UNITTEST::repeated_fixed32_extension, 1));
  EXPECT_EQ(508, message.GetExtension(UNITTEST::repeated_fixed64_extension, 1));
  EXPECT_EQ(509,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 1));
  EXPECT_EQ(510,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 1));
  EXPECT_EQ(511, message.GetExtension(UNITTEST::repeated_float_extension, 1));
  EXPECT_EQ(512, message.GetExtension(UNITTEST::repeated_double_extension, 1));
  EXPECT_TRUE(message.GetExtension(UNITTEST::repeated_bool_extension, 1));
  EXPECT_EQ("515",
            message.GetExtension(UNITTEST::repeated_string_extension, 1));
  EXPECT_EQ("516", message.GetExtension(UNITTEST::repeated_bytes_extension, 1));

  EXPECT_EQ(517,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 1).a());
  EXPECT_EQ(518,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 1)
                .bb());
  EXPECT_EQ(
      519, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 1)
               .c());
  EXPECT_EQ(
      520,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 1).d());
  EXPECT_EQ(
      527,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 1).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::FOO,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 1));
  EXPECT_EQ(UNITTEST::FOREIGN_FOO,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 1));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_FOO,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 1));

  EXPECT_EQ("524",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 1));
  EXPECT_EQ("525", message.GetExtension(UNITTEST::repeated_cord_extension, 1));
}

// -------------------------------------------------------------------

inline void TestUtil::SetPackedExtensions(
    UNITTEST::TestPackedExtensions* message) {
  message->AddExtension(UNITTEST::packed_int32_extension, 601);
  message->AddExtension(UNITTEST::packed_int64_extension, 602);
  message->AddExtension(UNITTEST::packed_uint32_extension, 603);
  message->AddExtension(UNITTEST::packed_uint64_extension, 604);
  message->AddExtension(UNITTEST::packed_sint32_extension, 605);
  message->AddExtension(UNITTEST::packed_sint64_extension, 606);
  message->AddExtension(UNITTEST::packed_fixed32_extension, 607);
  message->AddExtension(UNITTEST::packed_fixed64_extension, 608);
  message->AddExtension(UNITTEST::packed_sfixed32_extension, 609);
  message->AddExtension(UNITTEST::packed_sfixed64_extension, 610);
  message->AddExtension(UNITTEST::packed_float_extension, 611);
  message->AddExtension(UNITTEST::packed_double_extension, 612);
  message->AddExtension(UNITTEST::packed_bool_extension, true);
  message->AddExtension(UNITTEST::packed_enum_extension, UNITTEST::FOREIGN_BAR);
  // add a second one of each field
  message->AddExtension(UNITTEST::packed_int32_extension, 701);
  message->AddExtension(UNITTEST::packed_int64_extension, 702);
  message->AddExtension(UNITTEST::packed_uint32_extension, 703);
  message->AddExtension(UNITTEST::packed_uint64_extension, 704);
  message->AddExtension(UNITTEST::packed_sint32_extension, 705);
  message->AddExtension(UNITTEST::packed_sint64_extension, 706);
  message->AddExtension(UNITTEST::packed_fixed32_extension, 707);
  message->AddExtension(UNITTEST::packed_fixed64_extension, 708);
  message->AddExtension(UNITTEST::packed_sfixed32_extension, 709);
  message->AddExtension(UNITTEST::packed_sfixed64_extension, 710);
  message->AddExtension(UNITTEST::packed_float_extension, 711);
  message->AddExtension(UNITTEST::packed_double_extension, 712);
  message->AddExtension(UNITTEST::packed_bool_extension, false);
  message->AddExtension(UNITTEST::packed_enum_extension, UNITTEST::FOREIGN_BAZ);
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectPackedExtensionsSet(
    const UNITTEST::TestPackedExtensions& message) {
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_int32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_int64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_uint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_uint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_sint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_sint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_fixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_fixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_sfixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_sfixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_float_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_double_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_bool_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::packed_enum_extension));

  EXPECT_EQ(601, message.GetExtension(UNITTEST::packed_int32_extension, 0));
  EXPECT_EQ(602, message.GetExtension(UNITTEST::packed_int64_extension, 0));
  EXPECT_EQ(603, message.GetExtension(UNITTEST::packed_uint32_extension, 0));
  EXPECT_EQ(604, message.GetExtension(UNITTEST::packed_uint64_extension, 0));
  EXPECT_EQ(605, message.GetExtension(UNITTEST::packed_sint32_extension, 0));
  EXPECT_EQ(606, message.GetExtension(UNITTEST::packed_sint64_extension, 0));
  EXPECT_EQ(607, message.GetExtension(UNITTEST::packed_fixed32_extension, 0));
  EXPECT_EQ(608, message.GetExtension(UNITTEST::packed_fixed64_extension, 0));
  EXPECT_EQ(609, message.GetExtension(UNITTEST::packed_sfixed32_extension, 0));
  EXPECT_EQ(610, message.GetExtension(UNITTEST::packed_sfixed64_extension, 0));
  EXPECT_EQ(611, message.GetExtension(UNITTEST::packed_float_extension, 0));
  EXPECT_EQ(612, message.GetExtension(UNITTEST::packed_double_extension, 0));
  EXPECT_TRUE(message.GetExtension(UNITTEST::packed_bool_extension, 0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::packed_enum_extension, 0));
  EXPECT_EQ(701, message.GetExtension(UNITTEST::packed_int32_extension, 1));
  EXPECT_EQ(702, message.GetExtension(UNITTEST::packed_int64_extension, 1));
  EXPECT_EQ(703, message.GetExtension(UNITTEST::packed_uint32_extension, 1));
  EXPECT_EQ(704, message.GetExtension(UNITTEST::packed_uint64_extension, 1));
  EXPECT_EQ(705, message.GetExtension(UNITTEST::packed_sint32_extension, 1));
  EXPECT_EQ(706, message.GetExtension(UNITTEST::packed_sint64_extension, 1));
  EXPECT_EQ(707, message.GetExtension(UNITTEST::packed_fixed32_extension, 1));
  EXPECT_EQ(708, message.GetExtension(UNITTEST::packed_fixed64_extension, 1));
  EXPECT_EQ(709, message.GetExtension(UNITTEST::packed_sfixed32_extension, 1));
  EXPECT_EQ(710, message.GetExtension(UNITTEST::packed_sfixed64_extension, 1));
  EXPECT_EQ(711, message.GetExtension(UNITTEST::packed_float_extension, 1));
  EXPECT_EQ(712, message.GetExtension(UNITTEST::packed_double_extension, 1));
  EXPECT_FALSE(message.GetExtension(UNITTEST::packed_bool_extension, 1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ,
            message.GetExtension(UNITTEST::packed_enum_extension, 1));
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectUnpackedExtensionsSet(
    const UNITTEST::TestUnpackedExtensions& message) {
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_int32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_int64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_uint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_uint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_sint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_sint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_fixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_fixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_sfixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_sfixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_float_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_double_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_bool_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::unpacked_enum_extension));

  EXPECT_EQ(601, message.GetExtension(UNITTEST::unpacked_int32_extension, 0));
  EXPECT_EQ(602, message.GetExtension(UNITTEST::unpacked_int64_extension, 0));
  EXPECT_EQ(603, message.GetExtension(UNITTEST::unpacked_uint32_extension, 0));
  EXPECT_EQ(604, message.GetExtension(UNITTEST::unpacked_uint64_extension, 0));
  EXPECT_EQ(605, message.GetExtension(UNITTEST::unpacked_sint32_extension, 0));
  EXPECT_EQ(606, message.GetExtension(UNITTEST::unpacked_sint64_extension, 0));
  EXPECT_EQ(607, message.GetExtension(UNITTEST::unpacked_fixed32_extension, 0));
  EXPECT_EQ(608, message.GetExtension(UNITTEST::unpacked_fixed64_extension, 0));
  EXPECT_EQ(609,
            message.GetExtension(UNITTEST::unpacked_sfixed32_extension, 0));
  EXPECT_EQ(610,
            message.GetExtension(UNITTEST::unpacked_sfixed64_extension, 0));
  EXPECT_EQ(611, message.GetExtension(UNITTEST::unpacked_float_extension, 0));
  EXPECT_EQ(612, message.GetExtension(UNITTEST::unpacked_double_extension, 0));
  EXPECT_EQ(true, message.GetExtension(UNITTEST::unpacked_bool_extension, 0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::unpacked_enum_extension, 0));
  EXPECT_EQ(701, message.GetExtension(UNITTEST::unpacked_int32_extension, 1));
  EXPECT_EQ(702, message.GetExtension(UNITTEST::unpacked_int64_extension, 1));
  EXPECT_EQ(703, message.GetExtension(UNITTEST::unpacked_uint32_extension, 1));
  EXPECT_EQ(704, message.GetExtension(UNITTEST::unpacked_uint64_extension, 1));
  EXPECT_EQ(705, message.GetExtension(UNITTEST::unpacked_sint32_extension, 1));
  EXPECT_EQ(706, message.GetExtension(UNITTEST::unpacked_sint64_extension, 1));
  EXPECT_EQ(707, message.GetExtension(UNITTEST::unpacked_fixed32_extension, 1));
  EXPECT_EQ(708, message.GetExtension(UNITTEST::unpacked_fixed64_extension, 1));
  EXPECT_EQ(709,
            message.GetExtension(UNITTEST::unpacked_sfixed32_extension, 1));
  EXPECT_EQ(710,
            message.GetExtension(UNITTEST::unpacked_sfixed64_extension, 1));
  EXPECT_EQ(711, message.GetExtension(UNITTEST::unpacked_float_extension, 1));
  EXPECT_EQ(712, message.GetExtension(UNITTEST::unpacked_double_extension, 1));
  EXPECT_EQ(false, message.GetExtension(UNITTEST::unpacked_bool_extension, 1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ,
            message.GetExtension(UNITTEST::unpacked_enum_extension, 1));
}

// -------------------------------------------------------------------

inline void TestUtil::ExpectLastRepeatedsRemoved(
    const UNITTEST::TestAllTypes& message) {
  ASSERT_EQ(1, message.repeated_int32_size());
  ASSERT_EQ(1, message.repeated_int64_size());
  ASSERT_EQ(1, message.repeated_uint32_size());
  ASSERT_EQ(1, message.repeated_uint64_size());
  ASSERT_EQ(1, message.repeated_sint32_size());
  ASSERT_EQ(1, message.repeated_sint64_size());
  ASSERT_EQ(1, message.repeated_fixed32_size());
  ASSERT_EQ(1, message.repeated_fixed64_size());
  ASSERT_EQ(1, message.repeated_sfixed32_size());
  ASSERT_EQ(1, message.repeated_sfixed64_size());
  ASSERT_EQ(1, message.repeated_float_size());
  ASSERT_EQ(1, message.repeated_double_size());
  ASSERT_EQ(1, message.repeated_bool_size());
  ASSERT_EQ(1, message.repeated_string_size());
  ASSERT_EQ(1, message.repeated_bytes_size());

  ASSERT_EQ(1, message.repeatedgroup_size());
  ASSERT_EQ(1, message.repeated_nested_message_size());
  ASSERT_EQ(1, message.repeated_foreign_message_size());
  ASSERT_EQ(1, message.repeated_import_message_size());
  ASSERT_EQ(1, message.repeated_import_message_size());
  ASSERT_EQ(1, message.repeated_nested_enum_size());
  ASSERT_EQ(1, message.repeated_foreign_enum_size());
  ASSERT_EQ(1, message.repeated_import_enum_size());

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  ASSERT_EQ(1, message.repeated_string_piece_size());
  ASSERT_EQ(1, message.repeated_cord_size());
#endif

  // Test that the remaining element is the correct one.
  EXPECT_EQ(201, message.repeated_int32(0));
  EXPECT_EQ(202, message.repeated_int64(0));
  EXPECT_EQ(203, message.repeated_uint32(0));
  EXPECT_EQ(204, message.repeated_uint64(0));
  EXPECT_EQ(205, message.repeated_sint32(0));
  EXPECT_EQ(206, message.repeated_sint64(0));
  EXPECT_EQ(207, message.repeated_fixed32(0));
  EXPECT_EQ(208, message.repeated_fixed64(0));
  EXPECT_EQ(209, message.repeated_sfixed32(0));
  EXPECT_EQ(210, message.repeated_sfixed64(0));
  EXPECT_EQ(211, message.repeated_float(0));
  EXPECT_EQ(212, message.repeated_double(0));
  EXPECT_TRUE(message.repeated_bool(0));
  EXPECT_EQ("215", message.repeated_string(0));
  EXPECT_EQ("216", message.repeated_bytes(0));

  EXPECT_EQ(217, message.repeatedgroup(0).a());
  EXPECT_EQ(218, message.repeated_nested_message(0).bb());
  EXPECT_EQ(219, message.repeated_foreign_message(0).c());
  EXPECT_EQ(220, message.repeated_import_message(0).d());
  EXPECT_EQ(220, message.repeated_import_message(0).d());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message.repeated_nested_enum(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.repeated_foreign_enum(0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR, message.repeated_import_enum(0));
}

inline void TestUtil::ExpectLastRepeatedExtensionsRemoved(
    const UNITTEST::TestAllExtensions& message) {
  // Test that one element was removed.
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_int32_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_int64_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_uint32_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_uint64_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_sint32_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_sint64_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_fixed32_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_fixed64_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_sfixed32_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_sfixed64_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_float_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_double_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_bool_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_string_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_bytes_extension));

  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeatedgroup_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_nested_message_extension));
  ASSERT_EQ(
      1, message.ExtensionSize(UNITTEST::repeated_foreign_message_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_import_message_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_lazy_message_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_nested_enum_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_foreign_enum_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_import_enum_extension));

  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_string_piece_extension));
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeated_cord_extension));

  // Test that the remaining element is the correct one.
  EXPECT_EQ(201, message.GetExtension(UNITTEST::repeated_int32_extension, 0));
  EXPECT_EQ(202, message.GetExtension(UNITTEST::repeated_int64_extension, 0));
  EXPECT_EQ(203, message.GetExtension(UNITTEST::repeated_uint32_extension, 0));
  EXPECT_EQ(204, message.GetExtension(UNITTEST::repeated_uint64_extension, 0));
  EXPECT_EQ(205, message.GetExtension(UNITTEST::repeated_sint32_extension, 0));
  EXPECT_EQ(206, message.GetExtension(UNITTEST::repeated_sint64_extension, 0));
  EXPECT_EQ(207, message.GetExtension(UNITTEST::repeated_fixed32_extension, 0));
  EXPECT_EQ(208, message.GetExtension(UNITTEST::repeated_fixed64_extension, 0));
  EXPECT_EQ(209,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 0));
  EXPECT_EQ(210,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 0));
  EXPECT_EQ(211, message.GetExtension(UNITTEST::repeated_float_extension, 0));
  EXPECT_EQ(212, message.GetExtension(UNITTEST::repeated_double_extension, 0));
  EXPECT_TRUE(message.GetExtension(UNITTEST::repeated_bool_extension, 0));
  EXPECT_EQ("215",
            message.GetExtension(UNITTEST::repeated_string_extension, 0));
  EXPECT_EQ("216", message.GetExtension(UNITTEST::repeated_bytes_extension, 0));

  EXPECT_EQ(217,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 0).a());
  EXPECT_EQ(218,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 0)
                .bb());
  EXPECT_EQ(
      219, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 0)
               .c());
  EXPECT_EQ(
      220,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 0).d());
  EXPECT_EQ(
      227,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 0).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 0));

  EXPECT_EQ("224",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 0));
  EXPECT_EQ("225", message.GetExtension(UNITTEST::repeated_cord_extension, 0));
}

inline void TestUtil::ExpectLastRepeatedsReleased(
    const UNITTEST::TestAllTypes& message) {
  ASSERT_EQ(1, message.repeatedgroup_size());
  ASSERT_EQ(1, message.repeated_nested_message_size());
  ASSERT_EQ(1, message.repeated_foreign_message_size());
  ASSERT_EQ(1, message.repeated_import_message_size());
  ASSERT_EQ(1, message.repeated_import_message_size());

  EXPECT_EQ(217, message.repeatedgroup(0).a());
  EXPECT_EQ(218, message.repeated_nested_message(0).bb());
  EXPECT_EQ(219, message.repeated_foreign_message(0).c());
  EXPECT_EQ(220, message.repeated_import_message(0).d());
  EXPECT_EQ(220, message.repeated_import_message(0).d());
}

inline void TestUtil::ExpectLastRepeatedExtensionsReleased(
    const UNITTEST::TestAllExtensions& message) {
  ASSERT_EQ(1, message.ExtensionSize(UNITTEST::repeatedgroup_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_nested_message_extension));
  ASSERT_EQ(
      1, message.ExtensionSize(UNITTEST::repeated_foreign_message_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_import_message_extension));
  ASSERT_EQ(1,
            message.ExtensionSize(UNITTEST::repeated_lazy_message_extension));

  EXPECT_EQ(217,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 0).a());
  EXPECT_EQ(218,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 0)
                .bb());
  EXPECT_EQ(
      219, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 0)
               .c());
  EXPECT_EQ(
      220,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 0).d());
  EXPECT_EQ(
      227,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 0).bb());
}

inline void TestUtil::ExpectRepeatedsSwapped(
    const UNITTEST::TestAllTypes& message) {
  ASSERT_EQ(2, message.repeated_int32_size());
  ASSERT_EQ(2, message.repeated_int64_size());
  ASSERT_EQ(2, message.repeated_uint32_size());
  ASSERT_EQ(2, message.repeated_uint64_size());
  ASSERT_EQ(2, message.repeated_sint32_size());
  ASSERT_EQ(2, message.repeated_sint64_size());
  ASSERT_EQ(2, message.repeated_fixed32_size());
  ASSERT_EQ(2, message.repeated_fixed64_size());
  ASSERT_EQ(2, message.repeated_sfixed32_size());
  ASSERT_EQ(2, message.repeated_sfixed64_size());
  ASSERT_EQ(2, message.repeated_float_size());
  ASSERT_EQ(2, message.repeated_double_size());
  ASSERT_EQ(2, message.repeated_bool_size());
  ASSERT_EQ(2, message.repeated_string_size());
  ASSERT_EQ(2, message.repeated_bytes_size());

  ASSERT_EQ(2, message.repeatedgroup_size());
  ASSERT_EQ(2, message.repeated_nested_message_size());
  ASSERT_EQ(2, message.repeated_foreign_message_size());
  ASSERT_EQ(2, message.repeated_import_message_size());
  ASSERT_EQ(2, message.repeated_import_message_size());
  ASSERT_EQ(2, message.repeated_nested_enum_size());
  ASSERT_EQ(2, message.repeated_foreign_enum_size());
  ASSERT_EQ(2, message.repeated_import_enum_size());

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS
  ASSERT_EQ(2, message.repeated_string_piece_size());
  ASSERT_EQ(2, message.repeated_cord_size());
#endif

  // Test that the first element and second element are flipped.
  EXPECT_EQ(201, message.repeated_int32(1));
  EXPECT_EQ(202, message.repeated_int64(1));
  EXPECT_EQ(203, message.repeated_uint32(1));
  EXPECT_EQ(204, message.repeated_uint64(1));
  EXPECT_EQ(205, message.repeated_sint32(1));
  EXPECT_EQ(206, message.repeated_sint64(1));
  EXPECT_EQ(207, message.repeated_fixed32(1));
  EXPECT_EQ(208, message.repeated_fixed64(1));
  EXPECT_EQ(209, message.repeated_sfixed32(1));
  EXPECT_EQ(210, message.repeated_sfixed64(1));
  EXPECT_EQ(211, message.repeated_float(1));
  EXPECT_EQ(212, message.repeated_double(1));
  EXPECT_TRUE(message.repeated_bool(1));
  EXPECT_EQ("215", message.repeated_string(1));
  EXPECT_EQ("216", message.repeated_bytes(1));

  EXPECT_EQ(217, message.repeatedgroup(1).a());
  EXPECT_EQ(218, message.repeated_nested_message(1).bb());
  EXPECT_EQ(219, message.repeated_foreign_message(1).c());
  EXPECT_EQ(220, message.repeated_import_message(1).d());
  EXPECT_EQ(220, message.repeated_import_message(1).d());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message.repeated_nested_enum(1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.repeated_foreign_enum(1));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR, message.repeated_import_enum(1));

  EXPECT_EQ(301, message.repeated_int32(0));
  EXPECT_EQ(302, message.repeated_int64(0));
  EXPECT_EQ(303, message.repeated_uint32(0));
  EXPECT_EQ(304, message.repeated_uint64(0));
  EXPECT_EQ(305, message.repeated_sint32(0));
  EXPECT_EQ(306, message.repeated_sint64(0));
  EXPECT_EQ(307, message.repeated_fixed32(0));
  EXPECT_EQ(308, message.repeated_fixed64(0));
  EXPECT_EQ(309, message.repeated_sfixed32(0));
  EXPECT_EQ(310, message.repeated_sfixed64(0));
  EXPECT_EQ(311, message.repeated_float(0));
  EXPECT_EQ(312, message.repeated_double(0));
  EXPECT_FALSE(message.repeated_bool(0));
  EXPECT_EQ("315", message.repeated_string(0));
  EXPECT_EQ("316", message.repeated_bytes(0));

  EXPECT_EQ(317, message.repeatedgroup(0).a());
  EXPECT_EQ(318, message.repeated_nested_message(0).bb());
  EXPECT_EQ(319, message.repeated_foreign_message(0).c());
  EXPECT_EQ(320, message.repeated_import_message(0).d());
  EXPECT_EQ(320, message.repeated_import_message(0).d());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ, message.repeated_nested_enum(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ, message.repeated_foreign_enum(0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAZ, message.repeated_import_enum(0));
}

inline void TestUtil::ExpectRepeatedExtensionsSwapped(
    const UNITTEST::TestAllExtensions& message) {
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_int32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_int64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_uint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_uint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sint32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sint64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_fixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_fixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sfixed32_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_sfixed64_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_float_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_double_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_bool_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_string_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_bytes_extension));

  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeatedgroup_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_nested_message_extension));
  ASSERT_EQ(
      2, message.ExtensionSize(UNITTEST::repeated_foreign_message_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_import_message_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_lazy_message_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_nested_enum_extension));
  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_foreign_enum_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_import_enum_extension));

  ASSERT_EQ(2,
            message.ExtensionSize(UNITTEST::repeated_string_piece_extension));
  ASSERT_EQ(2, message.ExtensionSize(UNITTEST::repeated_cord_extension));

  EXPECT_EQ(201, message.GetExtension(UNITTEST::repeated_int32_extension, 1));
  EXPECT_EQ(202, message.GetExtension(UNITTEST::repeated_int64_extension, 1));
  EXPECT_EQ(203, message.GetExtension(UNITTEST::repeated_uint32_extension, 1));
  EXPECT_EQ(204, message.GetExtension(UNITTEST::repeated_uint64_extension, 1));
  EXPECT_EQ(205, message.GetExtension(UNITTEST::repeated_sint32_extension, 1));
  EXPECT_EQ(206, message.GetExtension(UNITTEST::repeated_sint64_extension, 1));
  EXPECT_EQ(207, message.GetExtension(UNITTEST::repeated_fixed32_extension, 1));
  EXPECT_EQ(208, message.GetExtension(UNITTEST::repeated_fixed64_extension, 1));
  EXPECT_EQ(209,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 1));
  EXPECT_EQ(210,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 1));
  EXPECT_EQ(211, message.GetExtension(UNITTEST::repeated_float_extension, 1));
  EXPECT_EQ(212, message.GetExtension(UNITTEST::repeated_double_extension, 1));
  EXPECT_TRUE(message.GetExtension(UNITTEST::repeated_bool_extension, 1));
  EXPECT_EQ("215",
            message.GetExtension(UNITTEST::repeated_string_extension, 1));
  EXPECT_EQ("216", message.GetExtension(UNITTEST::repeated_bytes_extension, 1));

  EXPECT_EQ(217,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 1).a());
  EXPECT_EQ(218,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 1)
                .bb());
  EXPECT_EQ(
      219, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 1)
               .c());
  EXPECT_EQ(
      220,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 1).d());
  EXPECT_EQ(
      227,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 1).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAR,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 1));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 1));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAR,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 1));

  EXPECT_EQ("224",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 1));
  EXPECT_EQ("225", message.GetExtension(UNITTEST::repeated_cord_extension, 1));

  EXPECT_EQ(301, message.GetExtension(UNITTEST::repeated_int32_extension, 0));
  EXPECT_EQ(302, message.GetExtension(UNITTEST::repeated_int64_extension, 0));
  EXPECT_EQ(303, message.GetExtension(UNITTEST::repeated_uint32_extension, 0));
  EXPECT_EQ(304, message.GetExtension(UNITTEST::repeated_uint64_extension, 0));
  EXPECT_EQ(305, message.GetExtension(UNITTEST::repeated_sint32_extension, 0));
  EXPECT_EQ(306, message.GetExtension(UNITTEST::repeated_sint64_extension, 0));
  EXPECT_EQ(307, message.GetExtension(UNITTEST::repeated_fixed32_extension, 0));
  EXPECT_EQ(308, message.GetExtension(UNITTEST::repeated_fixed64_extension, 0));
  EXPECT_EQ(309,
            message.GetExtension(UNITTEST::repeated_sfixed32_extension, 0));
  EXPECT_EQ(310,
            message.GetExtension(UNITTEST::repeated_sfixed64_extension, 0));
  EXPECT_EQ(311, message.GetExtension(UNITTEST::repeated_float_extension, 0));
  EXPECT_EQ(312, message.GetExtension(UNITTEST::repeated_double_extension, 0));
  EXPECT_FALSE(message.GetExtension(UNITTEST::repeated_bool_extension, 0));
  EXPECT_EQ("315",
            message.GetExtension(UNITTEST::repeated_string_extension, 0));
  EXPECT_EQ("316", message.GetExtension(UNITTEST::repeated_bytes_extension, 0));

  EXPECT_EQ(317,
            message.GetExtension(UNITTEST::repeatedgroup_extension, 0).a());
  EXPECT_EQ(318,
            message.GetExtension(UNITTEST::repeated_nested_message_extension, 0)
                .bb());
  EXPECT_EQ(
      319, message.GetExtension(UNITTEST::repeated_foreign_message_extension, 0)
               .c());
  EXPECT_EQ(
      320,
      message.GetExtension(UNITTEST::repeated_import_message_extension, 0).d());
  EXPECT_EQ(
      327,
      message.GetExtension(UNITTEST::repeated_lazy_message_extension, 0).bb());

  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ,
            message.GetExtension(UNITTEST::repeated_nested_enum_extension, 0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ,
            message.GetExtension(UNITTEST::repeated_foreign_enum_extension, 0));
  EXPECT_EQ(UNITTEST_IMPORT::IMPORT_BAZ,
            message.GetExtension(UNITTEST::repeated_import_enum_extension, 0));

  EXPECT_EQ("324",
            message.GetExtension(UNITTEST::repeated_string_piece_extension, 0));
  EXPECT_EQ("325", message.GetExtension(UNITTEST::repeated_cord_extension, 0));
}

inline void TestUtil::SetOneof1(UNITTEST::TestOneof2* message) {
  message->mutable_foo_lazy_message()->set_qux_int(100);
  message->set_bar_string("101");
  message->set_baz_int(102);
  message->set_baz_string("103");
}

inline void TestUtil::SetOneof2(UNITTEST::TestOneof2* message) {
  message->set_foo_int(200);
  message->set_bar_enum(UNITTEST::TestOneof2::BAZ);
  message->set_baz_int(202);
  message->set_baz_string("203");
}

inline void TestUtil::ExpectOneofSet1(const UNITTEST::TestOneof2& message) {
  ExpectAtMostOneFieldSetInOneof(message);

  EXPECT_TRUE(message.has_foo_lazy_message());
  EXPECT_TRUE(message.foo_lazy_message().has_qux_int());

  EXPECT_TRUE(message.has_bar_string());
  EXPECT_TRUE(message.has_baz_int());
  EXPECT_TRUE(message.has_baz_string());

  ASSERT_EQ(0, message.foo_lazy_message().corge_int_size());

  EXPECT_EQ(100, message.foo_lazy_message().qux_int());
  EXPECT_EQ("101", message.bar_string());
  EXPECT_EQ(102, message.baz_int());
  EXPECT_EQ("103", message.baz_string());
}

inline void TestUtil::ExpectOneofSet2(const UNITTEST::TestOneof2& message) {
  ExpectAtMostOneFieldSetInOneof(message);

  EXPECT_TRUE(message.has_foo_int());
  EXPECT_TRUE(message.has_bar_enum());
  EXPECT_TRUE(message.has_baz_int());
  EXPECT_TRUE(message.has_baz_string());

  EXPECT_EQ(200, message.foo_int());
  EXPECT_EQ(UNITTEST::TestOneof2::BAZ, message.bar_enum());
  EXPECT_EQ(202, message.baz_int());
  EXPECT_EQ("203", message.baz_string());
}

inline void TestUtil::ExpectOneofClear(const UNITTEST::TestOneof2& message) {
  EXPECT_FALSE(message.has_foo_int());
  EXPECT_FALSE(message.has_foo_string());
  EXPECT_FALSE(message.has_foo_bytes());
  EXPECT_FALSE(message.has_foo_enum());
  EXPECT_FALSE(message.has_foo_message());
  EXPECT_FALSE(message.has_foogroup());
  EXPECT_FALSE(message.has_foo_lazy_message());

  EXPECT_FALSE(message.has_bar_int());
  EXPECT_FALSE(message.has_bar_string());
  EXPECT_FALSE(message.has_bar_bytes());
  EXPECT_FALSE(message.has_bar_enum());

  EXPECT_FALSE(message.has_baz_int());
  EXPECT_FALSE(message.has_baz_string());

  EXPECT_EQ(UNITTEST::TestOneof2::FOO_NOT_SET, message.foo_case());
  EXPECT_EQ(UNITTEST::TestOneof2::BAR_NOT_SET, message.bar_case());
}

inline void TestUtil::ExpectAtMostOneFieldSetInOneof(
    const UNITTEST::TestOneof2& message) {
  int count = 0;
  if (message.has_foo_int()) count++;
  if (message.has_foo_string()) count++;
  if (message.has_foo_bytes()) count++;
  if (message.has_foo_enum()) count++;
  if (message.has_foo_message()) count++;
  if (message.has_foogroup()) count++;
  if (message.has_foo_lazy_message()) count++;
  EXPECT_LE(count, 1);
  count = 0;
  if (message.has_bar_int()) count++;
  if (message.has_bar_string()) count++;
  if (message.has_bar_bytes()) count++;
  if (message.has_bar_enum()) count++;
  EXPECT_TRUE(count == 0 || count == 1);
}

// ===================================================================

}  // namespace protobuf
}  // namespace google
