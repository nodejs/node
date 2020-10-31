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
// To test the code generator, we actually use it to generate code for
// net/proto2/internal/unittest.proto, then test that.  This means that we
// are actually testing the parser and other parts of the system at the same
// time, and that problems in the generator may show up as compile-time errors
// rather than unittest failures, which may be surprising.  However, testing
// the output of the C++ generator directly would be very hard.  We can't very
// well just check it against golden files since those files would have to be
// updated for any small change; such a test would be very brittle and probably
// not very helpful.  What we really want to test is that the code compiles
// correctly and produces the interfaces we expect, which is why this test
// is written this way.

#include <google/protobuf/compiler/cpp/cpp_unittest.h>

#include <memory>
#include <vector>

#include <google/protobuf/unittest_no_arena.pb.h>
#include <google/protobuf/stubs/strutil.h>
#if !defined(GOOGLE_PROTOBUF_CMAKE_BUILD) && !defined(_MSC_VER)
// We exclude this large proto from cmake build because it's too large for
// visual studio to compile (report internal errors).
#include <google/protobuf/unittest_enormous_descriptor.pb.h>
#endif
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_test_bad_identifiers.pb.h>
#include <google/protobuf/compiler/scc.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/test_util2.h>
#include <google/protobuf/unittest_no_generic_services.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>

#include <google/protobuf/stubs/callback.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/stl_util.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// Can't use an anonymous namespace here due to brokenness of Tru64 compiler.
namespace cpp_unittest {


class MockErrorCollector : public MultiFileErrorCollector {
 public:
  MockErrorCollector() {}
  ~MockErrorCollector() {}

  std::string text_;

  // implements ErrorCollector ---------------------------------------
  void AddError(const std::string& filename, int line, int column,
                const std::string& message) {
    strings::SubstituteAndAppend(&text_, "$0:$1:$2: $3\n",
                                 filename, line, column, message);
  }
};

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS

// Test that generated code has proper descriptors:
// Parse a descriptor directly (using google::protobuf::compiler::Importer) and
// compare it to the one that was produced by generated code.
TEST(GENERATED_DESCRIPTOR_TEST_NAME, IdenticalDescriptors) {
  const FileDescriptor* generated_descriptor =
    UNITTEST::TestAllTypes::descriptor()->file();

  // Set up the Importer.
  MockErrorCollector error_collector;
  DiskSourceTree source_tree;
  source_tree.MapPath("", TestUtil::TestSourceDir());
  Importer importer(&source_tree, &error_collector);

  // Import (parse) unittest.proto.
  const FileDescriptor* parsed_descriptor =
      importer.Import(TestUtil::MaybeTranslatePath(UNITTEST_PROTO_PATH));
  EXPECT_EQ("", error_collector.text_);
  ASSERT_TRUE(parsed_descriptor != NULL);

  // Test that descriptors are generated correctly by converting them to
  // FileDescriptorProtos and comparing.
  FileDescriptorProto generated_descriptor_proto, parsed_descriptor_proto;
  generated_descriptor->CopyTo(&generated_descriptor_proto);
  parsed_descriptor->CopyTo(&parsed_descriptor_proto);

  EXPECT_EQ(parsed_descriptor_proto.DebugString(),
            generated_descriptor_proto.DebugString());
}

#if !defined(GOOGLE_PROTOBUF_CMAKE_BUILD) && !defined(_MSC_VER)
// Test that generated code has proper descriptors:
// Touch a descriptor generated from an enormous message to validate special
// handling for descriptors exceeding the C++ standard's recommended minimum
// limit for string literal size
TEST(GENERATED_DESCRIPTOR_TEST_NAME, EnormousDescriptor) {
  const Descriptor* generated_descriptor =
    ::protobuf_unittest::TestEnormousDescriptor::descriptor();

  EXPECT_TRUE(generated_descriptor != NULL);
}
#endif

#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS

// ===================================================================

TEST(GENERATED_MESSAGE_TEST_NAME, Defaults) {
  // Check that all default values are set correctly in the initial message.
  UNITTEST::TestAllTypes message;

  TestUtil::ExpectClear(message);

  // Messages should return pointers to default instances until first use.
  // (This is not checked by ExpectClear() since it is not actually true after
  // the fields have been set and then cleared.)
  EXPECT_EQ(&UNITTEST::TestAllTypes::OptionalGroup::default_instance(),
            &message.optionalgroup());
  EXPECT_EQ(&UNITTEST::TestAllTypes::NestedMessage::default_instance(),
            &message.optional_nested_message());
  EXPECT_EQ(&UNITTEST::ForeignMessage::default_instance(),
            &message.optional_foreign_message());
  EXPECT_EQ(&UNITTEST_IMPORT::ImportMessage::default_instance(),
            &message.optional_import_message());
}

TEST(GENERATED_MESSAGE_TEST_NAME, FloatingPointDefaults) {
  const UNITTEST::TestExtremeDefaultValues& extreme_default =
      UNITTEST::TestExtremeDefaultValues::default_instance();

  EXPECT_EQ(0.0f, extreme_default.zero_float());
  EXPECT_EQ(1.0f, extreme_default.one_float());
  EXPECT_EQ(1.5f, extreme_default.small_float());
  EXPECT_EQ(-1.0f, extreme_default.negative_one_float());
  EXPECT_EQ(-1.5f, extreme_default.negative_float());
  EXPECT_EQ(2.0e8f, extreme_default.large_float());
  EXPECT_EQ(-8e-28f, extreme_default.small_negative_float());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            extreme_default.inf_double());
  EXPECT_EQ(-std::numeric_limits<double>::infinity(),
            extreme_default.neg_inf_double());
  EXPECT_TRUE(extreme_default.nan_double() != extreme_default.nan_double());
  EXPECT_EQ(std::numeric_limits<float>::infinity(),
            extreme_default.inf_float());
  EXPECT_EQ(-std::numeric_limits<float>::infinity(),
            extreme_default.neg_inf_float());
  EXPECT_TRUE(extreme_default.nan_float() != extreme_default.nan_float());
}

TEST(GENERATED_MESSAGE_TEST_NAME, Trigraph) {
  const UNITTEST::TestExtremeDefaultValues& extreme_default =
      UNITTEST::TestExtremeDefaultValues::default_instance();

  EXPECT_EQ("? ? ?? ?? ??? ?\?/ ?\?-", extreme_default.cpp_trigraph());
}

TEST(GENERATED_MESSAGE_TEST_NAME, ExtremeSmallIntegerDefault) {
  const UNITTEST::TestExtremeDefaultValues& extreme_default =
      UNITTEST::TestExtremeDefaultValues::default_instance();
  EXPECT_EQ(~0x7fffffff, kint32min);
  EXPECT_EQ(PROTOBUF_LONGLONG(~0x7fffffffffffffff), kint64min);
  EXPECT_EQ(kint32min, extreme_default.really_small_int32());
  EXPECT_EQ(kint64min, extreme_default.really_small_int64());
}

TEST(GENERATED_MESSAGE_TEST_NAME, Accessors) {
  // Set every field to a unique value then go back and check all those
  // values.
  UNITTEST::TestAllTypes message;

  TestUtil::SetAllFields(&message);
  TestUtil::ExpectAllFieldsSet(message);

  TestUtil::ModifyRepeatedFields(&message);
  TestUtil::ExpectRepeatedFieldsModified(message);
}

TEST(GENERATED_MESSAGE_TEST_NAME, MutableStringDefault) {
  // mutable_foo() for a string should return a string initialized to its
  // default value.
  UNITTEST::TestAllTypes message;

  EXPECT_EQ("hello", *message.mutable_default_string());

  // Note that the first time we call mutable_foo(), we get a newly-allocated
  // string, but if we clear it and call it again, we get the same object again.
  // We should verify that it has its default value in both cases.
  message.set_default_string("blah");
  message.Clear();

  EXPECT_EQ("hello", *message.mutable_default_string());
}

TEST(GENERATED_MESSAGE_TEST_NAME, StringDefaults) {
  UNITTEST::TestExtremeDefaultValues message;
  // Check if '\000' can be used in default string value.
  EXPECT_EQ(std::string("hel\000lo", 6), message.string_with_zero());
  EXPECT_EQ(std::string("wor\000ld", 6), message.bytes_with_zero());
}

TEST(GENERATED_MESSAGE_TEST_NAME, ReleaseString) {
  // Check that release_foo() starts out NULL, and gives us a value
  // that we can delete after it's been set.
  UNITTEST::TestAllTypes message;

  EXPECT_EQ(NULL, message.release_default_string());
  EXPECT_FALSE(message.has_default_string());
  EXPECT_EQ("hello", message.default_string());

  message.set_default_string("blah");
  EXPECT_TRUE(message.has_default_string());
  std::unique_ptr<std::string> str(message.release_default_string());
  EXPECT_FALSE(message.has_default_string());
  ASSERT_TRUE(str != NULL);
  EXPECT_EQ("blah", *str);

  EXPECT_EQ(NULL, message.release_default_string());
  EXPECT_FALSE(message.has_default_string());
  EXPECT_EQ("hello", message.default_string());
}

TEST(GENERATED_MESSAGE_TEST_NAME, ReleaseMessage) {
  // Check that release_foo() starts out NULL, and gives us a value
  // that we can delete after it's been set.
  UNITTEST::TestAllTypes message;

  EXPECT_EQ(NULL, message.release_optional_nested_message());
  EXPECT_FALSE(message.has_optional_nested_message());

  message.mutable_optional_nested_message()->set_bb(1);
  std::unique_ptr<UNITTEST::TestAllTypes::NestedMessage> nest(
      message.release_optional_nested_message());
  EXPECT_FALSE(message.has_optional_nested_message());
  ASSERT_TRUE(nest != NULL);
  EXPECT_EQ(1, nest->bb());

  EXPECT_EQ(NULL, message.release_optional_nested_message());
  EXPECT_FALSE(message.has_optional_nested_message());
}

TEST(GENERATED_MESSAGE_TEST_NAME, SetAllocatedString) {
  // Check that set_allocated_foo() works for strings.
  UNITTEST::TestAllTypes message;

  EXPECT_FALSE(message.has_optional_string());
  const std::string kHello("hello");
  message.set_optional_string(kHello);
  EXPECT_TRUE(message.has_optional_string());

  message.set_allocated_optional_string(NULL);
  EXPECT_FALSE(message.has_optional_string());
  EXPECT_EQ("", message.optional_string());

  message.set_allocated_optional_string(new std::string(kHello));
  EXPECT_TRUE(message.has_optional_string());
  EXPECT_EQ(kHello, message.optional_string());
}

TEST(GENERATED_MESSAGE_TEST_NAME, SetAllocatedMessage) {
  // Check that set_allocated_foo() can be called in all cases.
  UNITTEST::TestAllTypes message;

  EXPECT_FALSE(message.has_optional_nested_message());

  message.mutable_optional_nested_message()->set_bb(1);
  EXPECT_TRUE(message.has_optional_nested_message());

  message.set_allocated_optional_nested_message(NULL);
  EXPECT_FALSE(message.has_optional_nested_message());
  EXPECT_EQ(&UNITTEST::TestAllTypes::NestedMessage::default_instance(),
            &message.optional_nested_message());

  message.mutable_optional_nested_message()->set_bb(1);
  UNITTEST::TestAllTypes::NestedMessage* nest =
      message.release_optional_nested_message();
  ASSERT_TRUE(nest != NULL);
  EXPECT_FALSE(message.has_optional_nested_message());

  message.set_allocated_optional_nested_message(nest);
  EXPECT_TRUE(message.has_optional_nested_message());
  EXPECT_EQ(1, message.optional_nested_message().bb());
}

TEST(GENERATED_MESSAGE_TEST_NAME, Clear) {
  // Set every field to a unique value, clear the message, then check that
  // it is cleared.
  UNITTEST::TestAllTypes message;

  TestUtil::SetAllFields(&message);
  message.Clear();
  TestUtil::ExpectClear(message);

  // Unlike with the defaults test, we do NOT expect that requesting embedded
  // messages will return a pointer to the default instance.  Instead, they
  // should return the objects that were created when mutable_blah() was
  // called.
  EXPECT_NE(&UNITTEST::TestAllTypes::OptionalGroup::default_instance(),
            &message.optionalgroup());
  EXPECT_NE(&UNITTEST::TestAllTypes::NestedMessage::default_instance(),
            &message.optional_nested_message());
  EXPECT_NE(&UNITTEST::ForeignMessage::default_instance(),
            &message.optional_foreign_message());
  EXPECT_NE(&UNITTEST_IMPORT::ImportMessage::default_instance(),
            &message.optional_import_message());
}

TEST(GENERATED_MESSAGE_TEST_NAME, EmbeddedNullsInBytesCharStar) {
  UNITTEST::TestAllTypes message;

  const char* value = "\0lalala\0\0";
  message.set_optional_bytes(value, 9);
  ASSERT_EQ(9, message.optional_bytes().size());
  EXPECT_EQ(0, memcmp(value, message.optional_bytes().data(), 9));

  message.add_repeated_bytes(value, 9);
  ASSERT_EQ(9, message.repeated_bytes(0).size());
  EXPECT_EQ(0, memcmp(value, message.repeated_bytes(0).data(), 9));
}

TEST(GENERATED_MESSAGE_TEST_NAME, ClearOneField) {
  // Set every field to a unique value, then clear one value and insure that
  // only that one value is cleared.
  UNITTEST::TestAllTypes message;

  TestUtil::SetAllFields(&message);
  int64 original_value = message.optional_int64();

  // Clear the field and make sure it shows up as cleared.
  message.clear_optional_int64();
  EXPECT_FALSE(message.has_optional_int64());
  EXPECT_EQ(0, message.optional_int64());

  // Other adjacent fields should not be cleared.
  EXPECT_TRUE(message.has_optional_int32());
  EXPECT_TRUE(message.has_optional_uint32());

  // Make sure if we set it again, then all fields are set.
  message.set_optional_int64(original_value);
  TestUtil::ExpectAllFieldsSet(message);
}

TEST(GENERATED_MESSAGE_TEST_NAME, StringCharStarLength) {
  // Verify that we can use a char*,length to set one of the string fields.
  UNITTEST::TestAllTypes message;
  message.set_optional_string("abcdef", 3);
  EXPECT_EQ("abc", message.optional_string());

  // Verify that we can use a char*,length to add to a repeated string field.
  message.add_repeated_string("abcdef", 3);
  EXPECT_EQ(1, message.repeated_string_size());
  EXPECT_EQ("abc", message.repeated_string(0));

  // Verify that we can use a char*,length to set a repeated string field.
  message.set_repeated_string(0, "wxyz", 2);
  EXPECT_EQ("wx", message.repeated_string(0));
}

TEST(GENERATED_MESSAGE_TEST_NAME, StringMove) {
  // Verify that we trigger the move behavior on a scalar setter.
  protobuf_unittest_no_arena::TestAllTypes message;
  {
    std::string tmp(32, 'a');

    const char* old_data = tmp.data();
    message.set_optional_string(std::move(tmp));
    const char* new_data = message.optional_string().data();

    EXPECT_EQ(old_data, new_data);
    EXPECT_EQ(std::string(32, 'a'), message.optional_string());

    std::string tmp2(32, 'b');
    old_data = tmp2.data();
    message.set_optional_string(std::move(tmp2));
    new_data = message.optional_string().data();

    EXPECT_EQ(old_data, new_data);
    EXPECT_EQ(std::string(32, 'b'), message.optional_string());
  }

  // Verify that we trigger the move behavior on a oneof setter.
  {
    std::string tmp(32, 'a');

    const char* old_data = tmp.data();
    message.set_oneof_string(std::move(tmp));
    const char* new_data = message.oneof_string().data();

    EXPECT_EQ(old_data, new_data);
    EXPECT_EQ(std::string(32, 'a'), message.oneof_string());

    std::string tmp2(32, 'b');
    old_data = tmp2.data();
    message.set_oneof_string(std::move(tmp2));
    new_data = message.oneof_string().data();

    EXPECT_EQ(old_data, new_data);
    EXPECT_EQ(std::string(32, 'b'), message.oneof_string());
  }

  // Verify that we trigger the move behavior on a repeated setter.
  {
    std::string tmp(32, 'a');

    const char* old_data = tmp.data();
    message.add_repeated_string(std::move(tmp));
    const char* new_data = message.repeated_string(0).data();

    EXPECT_EQ(old_data, new_data);
    EXPECT_EQ(std::string(32, 'a'), message.repeated_string(0));

    std::string tmp2(32, 'b');
    old_data = tmp2.data();
    message.set_repeated_string(0, std::move(tmp2));
    new_data = message.repeated_string(0).data();

    EXPECT_EQ(old_data, new_data);
    EXPECT_EQ(std::string(32, 'b'), message.repeated_string(0));
  }
}


TEST(GENERATED_MESSAGE_TEST_NAME, CopyFrom) {
  UNITTEST::TestAllTypes message1, message2;

  TestUtil::SetAllFields(&message1);
  message2.CopyFrom(message1);
  TestUtil::ExpectAllFieldsSet(message2);

  // Copying from self should be a no-op.
  message2.CopyFrom(message2);
  TestUtil::ExpectAllFieldsSet(message2);
}


TEST(GENERATED_MESSAGE_TEST_NAME, SwapWithEmpty) {
  UNITTEST::TestAllTypes message1, message2;
  TestUtil::SetAllFields(&message1);

  TestUtil::ExpectAllFieldsSet(message1);
  TestUtil::ExpectClear(message2);
  message1.Swap(&message2);
  TestUtil::ExpectAllFieldsSet(message2);
  TestUtil::ExpectClear(message1);
}

TEST(GENERATED_MESSAGE_TEST_NAME, SwapWithSelf) {
  UNITTEST::TestAllTypes message;
  TestUtil::SetAllFields(&message);
  TestUtil::ExpectAllFieldsSet(message);
  message.Swap(&message);
  TestUtil::ExpectAllFieldsSet(message);
}

TEST(GENERATED_MESSAGE_TEST_NAME, SwapWithOther) {
  UNITTEST::TestAllTypes message1, message2;

  message1.set_optional_int32(123);
  message1.set_optional_string("abc");
  message1.mutable_optional_nested_message()->set_bb(1);
  message1.set_optional_nested_enum(UNITTEST::TestAllTypes::FOO);
  message1.add_repeated_int32(1);
  message1.add_repeated_int32(2);
  message1.add_repeated_string("a");
  message1.add_repeated_string("b");
  message1.add_repeated_nested_message()->set_bb(7);
  message1.add_repeated_nested_message()->set_bb(8);
  message1.add_repeated_nested_enum(UNITTEST::TestAllTypes::FOO);
  message1.add_repeated_nested_enum(UNITTEST::TestAllTypes::BAR);

  message2.set_optional_int32(456);
  message2.set_optional_string("def");
  message2.mutable_optional_nested_message()->set_bb(2);
  message2.set_optional_nested_enum(UNITTEST::TestAllTypes::BAR);
  message2.add_repeated_int32(3);
  message2.add_repeated_string("c");
  message2.add_repeated_nested_message()->set_bb(9);
  message2.add_repeated_nested_enum(UNITTEST::TestAllTypes::BAZ);

  message1.Swap(&message2);

  EXPECT_EQ(456, message1.optional_int32());
  EXPECT_EQ("def", message1.optional_string());
  EXPECT_EQ(2, message1.optional_nested_message().bb());
  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message1.optional_nested_enum());
  ASSERT_EQ(1, message1.repeated_int32_size());
  EXPECT_EQ(3, message1.repeated_int32(0));
  ASSERT_EQ(1, message1.repeated_string_size());
  EXPECT_EQ("c", message1.repeated_string(0));
  ASSERT_EQ(1, message1.repeated_nested_message_size());
  EXPECT_EQ(9, message1.repeated_nested_message(0).bb());
  ASSERT_EQ(1, message1.repeated_nested_enum_size());
  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ, message1.repeated_nested_enum(0));

  EXPECT_EQ(123, message2.optional_int32());
  EXPECT_EQ("abc", message2.optional_string());
  EXPECT_EQ(1, message2.optional_nested_message().bb());
  EXPECT_EQ(UNITTEST::TestAllTypes::FOO, message2.optional_nested_enum());
  ASSERT_EQ(2, message2.repeated_int32_size());
  EXPECT_EQ(1, message2.repeated_int32(0));
  EXPECT_EQ(2, message2.repeated_int32(1));
  ASSERT_EQ(2, message2.repeated_string_size());
  EXPECT_EQ("a", message2.repeated_string(0));
  EXPECT_EQ("b", message2.repeated_string(1));
  ASSERT_EQ(2, message2.repeated_nested_message_size());
  EXPECT_EQ(7, message2.repeated_nested_message(0).bb());
  EXPECT_EQ(8, message2.repeated_nested_message(1).bb());
  ASSERT_EQ(2, message2.repeated_nested_enum_size());
  EXPECT_EQ(UNITTEST::TestAllTypes::FOO, message2.repeated_nested_enum(0));
  EXPECT_EQ(UNITTEST::TestAllTypes::BAR, message2.repeated_nested_enum(1));
}

TEST(GENERATED_MESSAGE_TEST_NAME, ADLSwap) {
  UNITTEST::TestAllTypes message1, message2;
  TestUtil::SetAllFields(&message1);

  // Note the address of one of the repeated fields, to verify it was swapped
  // rather than copied.
  const int32* addr = &message1.repeated_int32().Get(0);

  using std::swap;
  swap(message1, message2);

  TestUtil::ExpectAllFieldsSet(message2);
  TestUtil::ExpectClear(message1);

  EXPECT_EQ(addr, &message2.repeated_int32().Get(0));
}

TEST(GENERATED_MESSAGE_TEST_NAME, CopyConstructor) {
  // All set.
  {
    UNITTEST::TestAllTypes message1;
    TestUtil::SetAllFields(&message1);

    UNITTEST::TestAllTypes message2(message1);
    TestUtil::ExpectAllFieldsSet(message2);
  }

  // None set.
  {
    UNITTEST::TestAllTypes message1;
    UNITTEST::TestAllTypes message2(message1);

    EXPECT_FALSE(message1.has_optional_string());
    EXPECT_FALSE(message2.has_optional_string());
    EXPECT_EQ(message1.optional_string(), message2.optional_string());

    EXPECT_FALSE(message1.has_optional_bytes());
    EXPECT_FALSE(message2.has_optional_bytes());
    EXPECT_EQ(message1.optional_bytes(), message2.optional_bytes());

    EXPECT_FALSE(message1.has_optional_nested_message());
    EXPECT_FALSE(message2.has_optional_nested_message());
    EXPECT_EQ(&message1.optional_nested_message(),
              &message2.optional_nested_message());

    EXPECT_FALSE(message1.has_optional_foreign_message());
    EXPECT_FALSE(message2.has_optional_foreign_message());
    EXPECT_EQ(&message1.optional_foreign_message(),
              &message2.optional_foreign_message());

    EXPECT_FALSE(message1.has_optional_import_message());
    EXPECT_FALSE(message2.has_optional_import_message());
    EXPECT_EQ(&message1.optional_import_message(),
              &message2.optional_import_message());

    EXPECT_FALSE(message1.has_optional_public_import_message());
    EXPECT_FALSE(message2.has_optional_public_import_message());
    EXPECT_EQ(&message1.optional_public_import_message(),
              &message2.optional_public_import_message());

    EXPECT_FALSE(message1.has_optional_lazy_message());
    EXPECT_FALSE(message2.has_optional_lazy_message());
    EXPECT_EQ(&message1.optional_lazy_message(),
              &message2.optional_lazy_message());
  }
}

TEST(GENERATED_MESSAGE_TEST_NAME, CopyConstructorWithArenas) {
  Arena arena;
  UNITTEST::TestAllTypes* message1 =
      Arena::CreateMessage<UNITTEST::TestAllTypes>(&arena);
  TestUtil::SetAllFields(message1);

  UNITTEST::TestAllTypes message2_stack(*message1);
  TestUtil::ExpectAllFieldsSet(message2_stack);

  std::unique_ptr<UNITTEST::TestAllTypes> message2_heap(
      new UNITTEST::TestAllTypes(*message1));
  TestUtil::ExpectAllFieldsSet(*message2_heap);

  arena.Reset();

  // Verify that the copies are still intact.
  TestUtil::ExpectAllFieldsSet(message2_stack);
  TestUtil::ExpectAllFieldsSet(*message2_heap);
}

TEST(GENERATED_MESSAGE_TEST_NAME, CopyAssignmentOperator) {
  UNITTEST::TestAllTypes message1;
  TestUtil::SetAllFields(&message1);

  UNITTEST::TestAllTypes message2;
  message2 = message1;
  TestUtil::ExpectAllFieldsSet(message2);

  // Make sure that self-assignment does something sane.
  message2.operator=(message2);
  TestUtil::ExpectAllFieldsSet(message2);
}

#if !defined(PROTOBUF_TEST_NO_DESCRIPTORS) || PROTOBUF_RTTI
TEST(GENERATED_MESSAGE_TEST_NAME, UpcastCopyFrom) {
  // Test the CopyFrom method that takes in the generic const Message&
  // parameter.
  UNITTEST::TestAllTypes message1, message2;

  TestUtil::SetAllFields(&message1);

  const Message* source = implicit_cast<const Message*>(&message1);
  message2.CopyFrom(*source);

  TestUtil::ExpectAllFieldsSet(message2);
}
#endif

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS

TEST(GENERATED_MESSAGE_TEST_NAME, DynamicMessageCopyFrom) {
  // Test copying from a DynamicMessage, which must fall back to using
  // reflection.
  UNITTEST::TestAllTypes message2;

  // Construct a new version of the dynamic message via the factory.
  DynamicMessageFactory factory;
  std::unique_ptr<Message> message1;
  message1.reset(factory.GetPrototype(
                     UNITTEST::TestAllTypes::descriptor())->New());

  TestUtil::ReflectionTester reflection_tester(
    UNITTEST::TestAllTypes::descriptor());
  reflection_tester.SetAllFieldsViaReflection(message1.get());

  message2.CopyFrom(*message1);

  TestUtil::ExpectAllFieldsSet(message2);
}

#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS

TEST(GENERATED_MESSAGE_TEST_NAME, NonEmptyMergeFrom) {
  // Test merging with a non-empty message. Code is a modified form
  // of that found in net/proto2/internal/reflection_ops_unittest.cc.
  UNITTEST::TestAllTypes message1, message2;

  TestUtil::SetAllFields(&message1);

  // This field will test merging into an empty spot.
  message2.set_optional_int32(message1.optional_int32());
  message1.clear_optional_int32();

  // This tests overwriting.
  message2.set_optional_string(message1.optional_string());
  message1.set_optional_string("something else");

  // This tests concatenating.
  message2.add_repeated_int32(message1.repeated_int32(1));
  int32 i = message1.repeated_int32(0);
  message1.clear_repeated_int32();
  message1.add_repeated_int32(i);

  message1.MergeFrom(message2);

  TestUtil::ExpectAllFieldsSet(message1);
}


// Test the generated SerializeWithCachedSizesToArray(),
TEST(GENERATED_MESSAGE_TEST_NAME, SerializationToArray) {
  UNITTEST::TestAllTypes message1, message2;
  std::string data;
  TestUtil::SetAllFields(&message1);
  int size = message1.ByteSizeLong();
  data.resize(size);
  uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
  uint8* end = message1.SerializeWithCachedSizesToArray(start);
  EXPECT_EQ(size, end - start);
  EXPECT_TRUE(message2.ParseFromString(data));
  TestUtil::ExpectAllFieldsSet(message2);

}

TEST(GENERATED_MESSAGE_TEST_NAME, PackedFieldsSerializationToArray) {
  UNITTEST::TestPackedTypes packed_message1, packed_message2;
  std::string packed_data;
  TestUtil::SetPackedFields(&packed_message1);
  int packed_size = packed_message1.ByteSizeLong();
  packed_data.resize(packed_size);
  uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&packed_data));
  uint8* end = packed_message1.SerializeWithCachedSizesToArray(start);
  EXPECT_EQ(packed_size, end - start);
  EXPECT_TRUE(packed_message2.ParseFromString(packed_data));
  TestUtil::ExpectPackedFieldsSet(packed_message2);
}

// Test the generated SerializeWithCachedSizes() by forcing the buffer to write
// one byte at a time.
TEST(GENERATED_MESSAGE_TEST_NAME, SerializationToStream) {
  UNITTEST::TestAllTypes message1, message2;
  TestUtil::SetAllFields(&message1);
  int size = message1.ByteSizeLong();
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
  TestUtil::ExpectAllFieldsSet(message2);

}

TEST(GENERATED_MESSAGE_TEST_NAME, PackedFieldsSerializationToStream) {
  UNITTEST::TestPackedTypes message1, message2;
  TestUtil::SetPackedFields(&message1);
  int size = message1.ByteSizeLong();
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
  TestUtil::ExpectPackedFieldsSet(message2);
}


TEST(GENERATED_MESSAGE_TEST_NAME, Required) {
  // Test that IsInitialized() returns false if required fields are missing.
  UNITTEST::TestRequired message;

  EXPECT_FALSE(message.IsInitialized());
  message.set_a(1);
  EXPECT_FALSE(message.IsInitialized());
  message.set_b(2);
  EXPECT_FALSE(message.IsInitialized());
  message.set_c(3);
  EXPECT_TRUE(message.IsInitialized());
}

TEST(GENERATED_MESSAGE_TEST_NAME, RequiredForeign) {
  // Test that IsInitialized() returns false if required fields in nested
  // messages are missing.
  UNITTEST::TestRequiredForeign message;

  EXPECT_TRUE(message.IsInitialized());

  message.mutable_optional_message();
  EXPECT_FALSE(message.IsInitialized());

  message.mutable_optional_message()->set_a(1);
  message.mutable_optional_message()->set_b(2);
  message.mutable_optional_message()->set_c(3);
  EXPECT_TRUE(message.IsInitialized());

  message.add_repeated_message();
  EXPECT_FALSE(message.IsInitialized());

  message.mutable_repeated_message(0)->set_a(1);
  message.mutable_repeated_message(0)->set_b(2);
  message.mutable_repeated_message(0)->set_c(3);
  EXPECT_TRUE(message.IsInitialized());
}

TEST(GENERATED_MESSAGE_TEST_NAME, ForeignNested) {
  // Test that TestAllTypes::NestedMessage can be embedded directly into
  // another message.
  UNITTEST::TestForeignNested message;

  // If this compiles and runs without crashing, it must work.  We have
  // nothing more to test.
  UNITTEST::TestAllTypes::NestedMessage* nested =
    message.mutable_foreign_nested();
  nested->set_bb(1);
}

TEST(GENERATED_MESSAGE_TEST_NAME, ReallyLargeTagNumber) {
  // Test that really large tag numbers don't break anything.
  UNITTEST::TestReallyLargeTagNumber message1, message2;
  std::string data;

  // For the most part, if this compiles and runs then we're probably good.
  // (The most likely cause for failure would be if something were attempting
  // to allocate a lookup table of some sort using tag numbers as the index.)
  // We'll try serializing just for fun.
  message1.set_a(1234);
  message1.set_bb(5678);
  message1.SerializeToString(&data);
  EXPECT_TRUE(message2.ParseFromString(data));
  EXPECT_EQ(1234, message2.a());
  EXPECT_EQ(5678, message2.bb());
}

TEST(GENERATED_MESSAGE_TEST_NAME, MutualRecursion) {
  // Test that mutually-recursive message types work.
  UNITTEST::TestMutualRecursionA message;
  UNITTEST::TestMutualRecursionA* nested = message.mutable_bb()->mutable_a();
  UNITTEST::TestMutualRecursionA* nested2 = nested->mutable_bb()->mutable_a();

  // Again, if the above compiles and runs, that's all we really have to
  // test, but just for run we'll check that the system didn't somehow come
  // up with a pointer loop...
  EXPECT_NE(&message, nested);
  EXPECT_NE(&message, nested2);
  EXPECT_NE(nested, nested2);
}

TEST(GENERATED_MESSAGE_TEST_NAME, CamelCaseFieldNames) {
  // This test is mainly checking that the following compiles, which verifies
  // that the field names were coerced to lower-case.
  //
  // Protocol buffers standard style is to use lowercase-with-underscores for
  // field names.  Some old proto1 .protos unfortunately used camel-case field
  // names.  In proto1, these names were forced to lower-case.  So, we do the
  // same thing in proto2.

  UNITTEST::TestCamelCaseFieldNames message;

  message.set_primitivefield(2);
  message.set_stringfield("foo");
  message.set_enumfield(UNITTEST::FOREIGN_FOO);
  message.mutable_messagefield()->set_c(6);

  message.add_repeatedprimitivefield(8);
  message.add_repeatedstringfield("qux");
  message.add_repeatedenumfield(UNITTEST::FOREIGN_BAR);
  message.add_repeatedmessagefield()->set_c(15);

  EXPECT_EQ(2, message.primitivefield());
  EXPECT_EQ("foo", message.stringfield());
  EXPECT_EQ(UNITTEST::FOREIGN_FOO, message.enumfield());
  EXPECT_EQ(6, message.messagefield().c());

  EXPECT_EQ(8, message.repeatedprimitivefield(0));
  EXPECT_EQ("qux", message.repeatedstringfield(0));
  EXPECT_EQ(UNITTEST::FOREIGN_BAR, message.repeatedenumfield(0));
  EXPECT_EQ(15, message.repeatedmessagefield(0).c());
}

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS

TEST(GENERATED_MESSAGE_TEST_NAME, TestOptimizedForSize) {
  // We rely on the tests in reflection_ops_unittest and wire_format_unittest
  // to really test that reflection-based methods work.  Here we are mostly
  // just making sure that TestOptimizedForSize actually builds and seems to
  // function.

  UNITTEST::TestOptimizedForSize message, message2;
  message.set_i(1);
  message.mutable_msg()->set_c(2);
  message2.CopyFrom(message);
  EXPECT_EQ(1, message2.i());
  EXPECT_EQ(2, message2.msg().c());
}

TEST(GENERATED_MESSAGE_TEST_NAME, TestEmbedOptimizedForSize) {
  // Verifies that something optimized for speed can contain something optimized
  // for size.

  UNITTEST::TestEmbedOptimizedForSize message, message2;
  message.mutable_optional_message()->set_i(1);
  message.add_repeated_message()->mutable_msg()->set_c(2);
  std::string data;
  message.SerializeToString(&data);
  ASSERT_TRUE(message2.ParseFromString(data));
  EXPECT_EQ(1, message2.optional_message().i());
  EXPECT_EQ(2, message2.repeated_message(0).msg().c());
}

TEST(GENERATED_MESSAGE_TEST_NAME, TestSpaceUsed) {
  UNITTEST::TestAllTypes message1;
  // sizeof provides a lower bound on SpaceUsedLong().
  EXPECT_LE(sizeof(UNITTEST::TestAllTypes), message1.SpaceUsedLong());
  const size_t empty_message_size = message1.SpaceUsedLong();

  // Setting primitive types shouldn't affect the space used.
  message1.set_optional_int32(123);
  message1.set_optional_int64(12345);
  message1.set_optional_uint32(123);
  message1.set_optional_uint64(12345);
  EXPECT_EQ(empty_message_size, message1.SpaceUsedLong());

  // On some STL implementations, setting the string to a small value should
  // only increase SpaceUsedLong() by the size of a string object, though this
  // is not true everywhere.
  message1.set_optional_string("abc");
  EXPECT_LE(empty_message_size + message1.optional_string().size(),
            message1.SpaceUsedLong());

  // Setting a string to a value larger than the string object itself should
  // increase SpaceUsedLong(), because it cannot store the value internally.
  message1.set_optional_string(std::string(sizeof(std::string) + 1, 'x'));
  int min_expected_increase = message1.optional_string().capacity();
  EXPECT_LE(empty_message_size + min_expected_increase,
            message1.SpaceUsedLong());

  size_t previous_size = message1.SpaceUsedLong();
  // Adding an optional message should increase the size by the size of the
  // nested message type. NestedMessage is simple enough (1 int field) that it
  // is equal to sizeof(NestedMessage)
  message1.mutable_optional_nested_message();
  ASSERT_EQ(sizeof(UNITTEST::TestAllTypes::NestedMessage),
            message1.optional_nested_message().SpaceUsedLong());
  EXPECT_EQ(previous_size +
            sizeof(UNITTEST::TestAllTypes::NestedMessage),
            message1.SpaceUsedLong());
}

TEST(GENERATED_MESSAGE_TEST_NAME, TestOneofSpaceUsed) {
  UNITTEST::TestOneof2 message1;
  EXPECT_LE(sizeof(UNITTEST::TestOneof2), message1.SpaceUsedLong());

  const size_t empty_message_size = message1.SpaceUsedLong();
  // Setting primitive types shouldn't affect the space used.
  message1.set_foo_int(123);
  message1.set_bar_int(12345);
  EXPECT_EQ(empty_message_size, message1.SpaceUsedLong());

  // Setting a string in oneof to a small value should only increase
  // SpaceUsedLong() by the size of a string object.
  message1.set_foo_string("abc");
  EXPECT_LE(empty_message_size + sizeof(std::string), message1.SpaceUsedLong());

  // Setting a string in oneof to a value larger than the string object itself
  // should increase SpaceUsedLong(), because it cannot store the value
  // internally.
  message1.set_foo_string(std::string(sizeof(std::string) + 1, 'x'));
  int min_expected_increase =
      message1.foo_string().capacity() + sizeof(std::string);
  EXPECT_LE(empty_message_size + min_expected_increase,
            message1.SpaceUsedLong());

  // Setting a message in oneof should delete the other fields and increase the
  // size by the size of the nested message type. NestedMessage is simple enough
  // that it is equal to sizeof(NestedMessage)
  message1.mutable_foo_message();
  ASSERT_EQ(sizeof(UNITTEST::TestOneof2::NestedMessage),
            message1.foo_message().SpaceUsedLong());
  EXPECT_EQ(empty_message_size +
            sizeof(UNITTEST::TestOneof2::NestedMessage),
            message1.SpaceUsedLong());
}

#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS


TEST(GENERATED_MESSAGE_TEST_NAME, FieldConstantValues) {
  UNITTEST::TestRequired message;
  EXPECT_EQ(UNITTEST::TestAllTypes_NestedMessage::kBbFieldNumber, 1);
  EXPECT_EQ(UNITTEST::TestAllTypes::kOptionalInt32FieldNumber, 1);
  EXPECT_EQ(UNITTEST::TestAllTypes::kOptionalgroupFieldNumber, 16);
  EXPECT_EQ(UNITTEST::TestAllTypes::kOptionalNestedMessageFieldNumber, 18);
  EXPECT_EQ(UNITTEST::TestAllTypes::kOptionalNestedEnumFieldNumber, 21);
  EXPECT_EQ(UNITTEST::TestAllTypes::kRepeatedInt32FieldNumber, 31);
  EXPECT_EQ(UNITTEST::TestAllTypes::kRepeatedgroupFieldNumber, 46);
  EXPECT_EQ(UNITTEST::TestAllTypes::kRepeatedNestedMessageFieldNumber, 48);
  EXPECT_EQ(UNITTEST::TestAllTypes::kRepeatedNestedEnumFieldNumber, 51);
}

TEST(GENERATED_MESSAGE_TEST_NAME, ExtensionConstantValues) {
  EXPECT_EQ(UNITTEST::TestRequired::kSingleFieldNumber, 1000);
  EXPECT_EQ(UNITTEST::TestRequired::kMultiFieldNumber, 1001);
  EXPECT_EQ(UNITTEST::kOptionalInt32ExtensionFieldNumber, 1);
  EXPECT_EQ(UNITTEST::kOptionalgroupExtensionFieldNumber, 16);
  EXPECT_EQ(UNITTEST::kOptionalNestedMessageExtensionFieldNumber, 18);
  EXPECT_EQ(UNITTEST::kOptionalNestedEnumExtensionFieldNumber, 21);
  EXPECT_EQ(UNITTEST::kRepeatedInt32ExtensionFieldNumber, 31);
  EXPECT_EQ(UNITTEST::kRepeatedgroupExtensionFieldNumber, 46);
  EXPECT_EQ(UNITTEST::kRepeatedNestedMessageExtensionFieldNumber, 48);
  EXPECT_EQ(UNITTEST::kRepeatedNestedEnumExtensionFieldNumber, 51);
}

TEST(GENERATED_MESSAGE_TEST_NAME, ParseFromTruncated) {
  const std::string long_string = std::string(128, 'q');
  FileDescriptorProto p;
  p.add_extension()->set_name(long_string);
  const std::string msg = p.SerializeAsString();
  int successful_count = 0;
  for (int i = 0; i <= msg.size(); i++) {
    if (p.ParseFromArray(msg.c_str(), i)) {
      ++successful_count;
    }
  }
  // We don't really care about how often we succeeded.
  // As long as we didn't crash, we're happy.
  EXPECT_GE(successful_count, 1);
}

// ===================================================================

TEST(GENERATED_ENUM_TEST_NAME, EnumValuesAsSwitchCases) {
  // Test that our nested enum values can be used as switch cases.  This test
  // doesn't actually do anything, the proof that it works is that it
  // compiles.
  int i =0;
  UNITTEST::TestAllTypes::NestedEnum a = UNITTEST::TestAllTypes::BAR;
  switch (a) {
    case UNITTEST::TestAllTypes::FOO:
      i = 1;
      break;
    case UNITTEST::TestAllTypes::BAR:
      i = 2;
      break;
    case UNITTEST::TestAllTypes::BAZ:
      i = 3;
      break;
    case UNITTEST::TestAllTypes::NEG:
      i = -1;
      break;
    // no default case:  We want to make sure the compiler recognizes that
    //   all cases are covered.  (GCC warns if you do not cover all cases of
    //   an enum in a switch.)
  }

  // Token check just for fun.
  EXPECT_EQ(2, i);
}

TEST(GENERATED_ENUM_TEST_NAME, IsValidValue) {
  // Test enum IsValidValue.
  EXPECT_TRUE(UNITTEST::TestAllTypes::NestedEnum_IsValid(1));
  EXPECT_TRUE(UNITTEST::TestAllTypes::NestedEnum_IsValid(2));
  EXPECT_TRUE(UNITTEST::TestAllTypes::NestedEnum_IsValid(3));

  EXPECT_FALSE(UNITTEST::TestAllTypes::NestedEnum_IsValid(0));
  EXPECT_FALSE(UNITTEST::TestAllTypes::NestedEnum_IsValid(4));

  // Make sure it also works when there are dups.
  EXPECT_TRUE(UNITTEST::TestEnumWithDupValue_IsValid(1));
  EXPECT_TRUE(UNITTEST::TestEnumWithDupValue_IsValid(2));
  EXPECT_TRUE(UNITTEST::TestEnumWithDupValue_IsValid(3));

  EXPECT_FALSE(UNITTEST::TestEnumWithDupValue_IsValid(0));
  EXPECT_FALSE(UNITTEST::TestEnumWithDupValue_IsValid(4));
}

TEST(GENERATED_ENUM_TEST_NAME, MinAndMax) {
  EXPECT_EQ(UNITTEST::TestAllTypes::NEG,
            UNITTEST::TestAllTypes::NestedEnum_MIN);
  EXPECT_EQ(UNITTEST::TestAllTypes::BAZ,
            UNITTEST::TestAllTypes::NestedEnum_MAX);
  EXPECT_EQ(4, UNITTEST::TestAllTypes::NestedEnum_ARRAYSIZE);

  EXPECT_EQ(UNITTEST::FOREIGN_FOO, UNITTEST::ForeignEnum_MIN);
  EXPECT_EQ(UNITTEST::FOREIGN_BAZ, UNITTEST::ForeignEnum_MAX);
  EXPECT_EQ(7, UNITTEST::ForeignEnum_ARRAYSIZE);

  EXPECT_EQ(1, UNITTEST::TestEnumWithDupValue_MIN);
  EXPECT_EQ(3, UNITTEST::TestEnumWithDupValue_MAX);
  EXPECT_EQ(4, UNITTEST::TestEnumWithDupValue_ARRAYSIZE);

  EXPECT_EQ(UNITTEST::SPARSE_E, UNITTEST::TestSparseEnum_MIN);
  EXPECT_EQ(UNITTEST::SPARSE_C, UNITTEST::TestSparseEnum_MAX);
  EXPECT_EQ(12589235, UNITTEST::TestSparseEnum_ARRAYSIZE);

  // Make sure we can take the address of _MIN, _MAX and _ARRAYSIZE.
  void* null_pointer = 0;  // NULL may be integer-type, not pointer-type.
  EXPECT_NE(null_pointer, &UNITTEST::TestAllTypes::NestedEnum_MIN);
  EXPECT_NE(null_pointer, &UNITTEST::TestAllTypes::NestedEnum_MAX);
  EXPECT_NE(null_pointer, &UNITTEST::TestAllTypes::NestedEnum_ARRAYSIZE);

  EXPECT_NE(null_pointer, &UNITTEST::ForeignEnum_MIN);
  EXPECT_NE(null_pointer, &UNITTEST::ForeignEnum_MAX);
  EXPECT_NE(null_pointer, &UNITTEST::ForeignEnum_ARRAYSIZE);

  // Make sure we can use _MIN and _MAX as switch cases.
  switch (UNITTEST::SPARSE_A) {
    case UNITTEST::TestSparseEnum_MIN:
    case UNITTEST::TestSparseEnum_MAX:
      break;
    default:
      break;
  }
}

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS

TEST(GENERATED_ENUM_TEST_NAME, Name) {
  // "Names" in the presence of dup values map to the first alias.
  EXPECT_EQ("FOO1", UNITTEST::TestEnumWithDupValue_Name(UNITTEST::FOO1));
  EXPECT_EQ("FOO1", UNITTEST::TestEnumWithDupValue_Name(UNITTEST::FOO2));

  EXPECT_EQ("SPARSE_A", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_A));
  EXPECT_EQ("SPARSE_B", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_B));
  EXPECT_EQ("SPARSE_C", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_C));
  EXPECT_EQ("SPARSE_D", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_D));
  EXPECT_EQ("SPARSE_E", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_E));
  EXPECT_EQ("SPARSE_F", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_F));
  EXPECT_EQ("SPARSE_G", UNITTEST::TestSparseEnum_Name(UNITTEST::SPARSE_G));
}

TEST(GENERATED_ENUM_TEST_NAME, Parse) {
  UNITTEST::TestEnumWithDupValue dup_value = UNITTEST::FOO1;
  EXPECT_TRUE(UNITTEST::TestEnumWithDupValue_Parse("FOO1", &dup_value));
  EXPECT_EQ(UNITTEST::FOO1, dup_value);
  EXPECT_TRUE(UNITTEST::TestEnumWithDupValue_Parse("FOO2", &dup_value));
  EXPECT_EQ(UNITTEST::FOO2, dup_value);
  EXPECT_FALSE(UNITTEST::TestEnumWithDupValue_Parse("FOO", &dup_value));
}

TEST(GENERATED_ENUM_TEST_NAME, GetEnumDescriptor) {
  EXPECT_EQ(UNITTEST::TestAllTypes::NestedEnum_descriptor(),
            GetEnumDescriptor<UNITTEST::TestAllTypes::NestedEnum>());
  EXPECT_EQ(UNITTEST::ForeignEnum_descriptor(),
            GetEnumDescriptor<UNITTEST::ForeignEnum>());
  EXPECT_EQ(UNITTEST::TestEnumWithDupValue_descriptor(),
            GetEnumDescriptor<UNITTEST::TestEnumWithDupValue>());
  EXPECT_EQ(UNITTEST::TestSparseEnum_descriptor(),
            GetEnumDescriptor<UNITTEST::TestSparseEnum>());
}

enum NonProtoEnum {
  kFoo = 1,
};

TEST(GENERATED_ENUM_TEST_NAME, IsProtoEnumTypeTrait) {
  EXPECT_TRUE(is_proto_enum<UNITTEST::TestAllTypes::NestedEnum>::value);
  EXPECT_TRUE(is_proto_enum<UNITTEST::ForeignEnum>::value);
  EXPECT_TRUE(is_proto_enum<UNITTEST::TestEnumWithDupValue>::value);
  EXPECT_TRUE(is_proto_enum<UNITTEST::TestSparseEnum>::value);

  EXPECT_FALSE(is_proto_enum<int>::value);
  EXPECT_FALSE(is_proto_enum<NonProtoEnum>::value);
}

#endif  // PROTOBUF_TEST_NO_DESCRIPTORS

// ===================================================================

#ifndef PROTOBUF_TEST_NO_DESCRIPTORS

// Support code for testing services.
class GENERATED_SERVICE_TEST_NAME : public testing::Test {
 protected:
  class MockTestService : public UNITTEST::TestService {
   public:
    MockTestService()
      : called_(false),
        method_(""),
        controller_(NULL),
        request_(NULL),
        response_(NULL),
        done_(NULL) {}

    ~MockTestService() {}

    void Reset() { called_ = false; }

    // implements TestService ----------------------------------------

    void Foo(RpcController* controller,
             const UNITTEST::FooRequest* request,
             UNITTEST::FooResponse* response,
             Closure* done) {
      ASSERT_FALSE(called_);
      called_ = true;
      method_ = "Foo";
      controller_ = controller;
      request_ = request;
      response_ = response;
      done_ = done;
    }

    void Bar(RpcController* controller,
             const UNITTEST::BarRequest* request,
             UNITTEST::BarResponse* response,
             Closure* done) {
      ASSERT_FALSE(called_);
      called_ = true;
      method_ = "Bar";
      controller_ = controller;
      request_ = request;
      response_ = response;
      done_ = done;
    }

    // ---------------------------------------------------------------

    bool called_;
    std::string method_;
    RpcController* controller_;
    const Message* request_;
    Message* response_;
    Closure* done_;
  };

  class MockRpcChannel : public RpcChannel {
   public:
    MockRpcChannel()
      : called_(false),
        method_(NULL),
        controller_(NULL),
        request_(NULL),
        response_(NULL),
        done_(NULL),
        destroyed_(NULL) {}

    ~MockRpcChannel() {
      if (destroyed_ != NULL) *destroyed_ = true;
    }

    void Reset() { called_ = false; }

    // implements TestService ----------------------------------------

    void CallMethod(const MethodDescriptor* method,
                    RpcController* controller,
                    const Message* request,
                    Message* response,
                    Closure* done) {
      ASSERT_FALSE(called_);
      called_ = true;
      method_ = method;
      controller_ = controller;
      request_ = request;
      response_ = response;
      done_ = done;
    }

    // ---------------------------------------------------------------

    bool called_;
    const MethodDescriptor* method_;
    RpcController* controller_;
    const Message* request_;
    Message* response_;
    Closure* done_;
    bool* destroyed_;
  };

  class MockController : public RpcController {
   public:
    void Reset() {
      ADD_FAILURE() << "Reset() not expected during this test.";
    }
    bool Failed() const {
      ADD_FAILURE() << "Failed() not expected during this test.";
      return false;
    }
    std::string ErrorText() const {
      ADD_FAILURE() << "ErrorText() not expected during this test.";
      return "";
    }
    void StartCancel() {
      ADD_FAILURE() << "StartCancel() not expected during this test.";
    }
    void SetFailed(const std::string& reason) {
      ADD_FAILURE() << "SetFailed() not expected during this test.";
    }
    bool IsCanceled() const {
      ADD_FAILURE() << "IsCanceled() not expected during this test.";
      return false;
    }
    void NotifyOnCancel(Closure* callback) {
      ADD_FAILURE() << "NotifyOnCancel() not expected during this test.";
    }
  };

  GENERATED_SERVICE_TEST_NAME()
    : descriptor_(UNITTEST::TestService::descriptor()),
      foo_(descriptor_->FindMethodByName("Foo")),
      bar_(descriptor_->FindMethodByName("Bar")),
      stub_(&mock_channel_),
      done_(::google::protobuf::NewPermanentCallback(&DoNothing)) {}

  virtual void SetUp() {
    ASSERT_TRUE(foo_ != NULL);
    ASSERT_TRUE(bar_ != NULL);
  }

  const ServiceDescriptor* descriptor_;
  const MethodDescriptor* foo_;
  const MethodDescriptor* bar_;

  MockTestService mock_service_;
  MockController mock_controller_;

  MockRpcChannel mock_channel_;
  UNITTEST::TestService::Stub stub_;

  // Just so we don't have to re-define these with every test.
  UNITTEST::FooRequest foo_request_;
  UNITTEST::FooResponse foo_response_;
  UNITTEST::BarRequest bar_request_;
  UNITTEST::BarResponse bar_response_;
  std::unique_ptr<Closure> done_;
};

TEST_F(GENERATED_SERVICE_TEST_NAME, GetDescriptor) {
  // Test that GetDescriptor() works.

  EXPECT_EQ(descriptor_, mock_service_.GetDescriptor());
}

TEST_F(GENERATED_SERVICE_TEST_NAME, GetChannel) {
  EXPECT_EQ(&mock_channel_, stub_.channel());
}

TEST_F(GENERATED_SERVICE_TEST_NAME, OwnsChannel) {
  MockRpcChannel* channel = new MockRpcChannel;
  bool destroyed = false;
  channel->destroyed_ = &destroyed;

  {
    UNITTEST::TestService::Stub owning_stub(channel,
                                            Service::STUB_OWNS_CHANNEL);
    EXPECT_FALSE(destroyed);
  }

  EXPECT_TRUE(destroyed);
}

TEST_F(GENERATED_SERVICE_TEST_NAME, CallMethod) {
  // Test that CallMethod() works.

  // Call Foo() via CallMethod().
  mock_service_.CallMethod(foo_, &mock_controller_,
                           &foo_request_, &foo_response_, done_.get());

  ASSERT_TRUE(mock_service_.called_);

  EXPECT_EQ("Foo"            , mock_service_.method_    );
  EXPECT_EQ(&mock_controller_, mock_service_.controller_);
  EXPECT_EQ(&foo_request_    , mock_service_.request_   );
  EXPECT_EQ(&foo_response_   , mock_service_.response_  );
  EXPECT_EQ(done_.get()      , mock_service_.done_      );

  // Try again, but call Bar() instead.
  mock_service_.Reset();
  mock_service_.CallMethod(bar_, &mock_controller_,
                           &bar_request_, &bar_response_, done_.get());

  ASSERT_TRUE(mock_service_.called_);
  EXPECT_EQ("Bar", mock_service_.method_);
}

TEST_F(GENERATED_SERVICE_TEST_NAME, CallMethodTypeFailure) {
  // Verify death if we call Foo() with Bar's message types.

#ifdef PROTOBUF_HAS_DEATH_TEST  // death tests do not work on Windows yet
  EXPECT_DEBUG_DEATH(
    mock_service_.CallMethod(foo_, &mock_controller_,
                             &foo_request_, &bar_response_, done_.get()),
    "dynamic_cast");

  mock_service_.Reset();
  EXPECT_DEBUG_DEATH(
    mock_service_.CallMethod(foo_, &mock_controller_,
                             &bar_request_, &foo_response_, done_.get()),
    "dynamic_cast");
#endif  // PROTOBUF_HAS_DEATH_TEST
}

TEST_F(GENERATED_SERVICE_TEST_NAME, GetPrototypes) {
  // Test Get{Request,Response}Prototype() methods.

  EXPECT_EQ(&UNITTEST::FooRequest::default_instance(),
            &mock_service_.GetRequestPrototype(foo_));
  EXPECT_EQ(&UNITTEST::BarRequest::default_instance(),
            &mock_service_.GetRequestPrototype(bar_));

  EXPECT_EQ(&UNITTEST::FooResponse::default_instance(),
            &mock_service_.GetResponsePrototype(foo_));
  EXPECT_EQ(&UNITTEST::BarResponse::default_instance(),
            &mock_service_.GetResponsePrototype(bar_));
}

TEST_F(GENERATED_SERVICE_TEST_NAME, Stub) {
  // Test that the stub class works.

  // Call Foo() via the stub.
  stub_.Foo(&mock_controller_, &foo_request_, &foo_response_, done_.get());

  ASSERT_TRUE(mock_channel_.called_);

  EXPECT_EQ(foo_             , mock_channel_.method_    );
  EXPECT_EQ(&mock_controller_, mock_channel_.controller_);
  EXPECT_EQ(&foo_request_    , mock_channel_.request_   );
  EXPECT_EQ(&foo_response_   , mock_channel_.response_  );
  EXPECT_EQ(done_.get()      , mock_channel_.done_      );

  // Call Bar() via the stub.
  mock_channel_.Reset();
  stub_.Bar(&mock_controller_, &bar_request_, &bar_response_, done_.get());

  ASSERT_TRUE(mock_channel_.called_);
  EXPECT_EQ(bar_, mock_channel_.method_);
}

TEST_F(GENERATED_SERVICE_TEST_NAME, NotImplemented) {
  // Test that failing to implement a method of a service causes it to fail
  // with a "not implemented" error message.

  // A service which doesn't implement any methods.
  class UnimplementedService : public UNITTEST::TestService {
   public:
    UnimplementedService() {}
  };

  UnimplementedService unimplemented_service;

  // And a controller which expects to get a "not implemented" error.
  class ExpectUnimplementedController : public MockController {
   public:
    ExpectUnimplementedController() : called_(false) {}

    void SetFailed(const std::string& reason) {
      EXPECT_FALSE(called_);
      called_ = true;
      EXPECT_EQ("Method Foo() not implemented.", reason);
    }

    bool called_;
  };

  ExpectUnimplementedController controller;

  // Call Foo.
  unimplemented_service.Foo(&controller, &foo_request_, &foo_response_,
                            done_.get());

  EXPECT_TRUE(controller.called_);
}

// ===================================================================

class OneofTest : public testing::Test {
 protected:
  virtual void SetUp() {
  }

  void ExpectEnumCasesWork(const UNITTEST::TestOneof2 &message) {
    switch (message.foo_case()) {
      case UNITTEST::TestOneof2::kFooInt:
        EXPECT_TRUE(message.has_foo_int());
        break;
      case UNITTEST::TestOneof2::kFooString:
        EXPECT_TRUE(message.has_foo_string());
        break;
      case UNITTEST::TestOneof2::kFooCord:
        EXPECT_TRUE(message.has_foo_cord());
        break;
      case UNITTEST::TestOneof2::kFooStringPiece:
        EXPECT_TRUE(message.has_foo_string_piece());
        break;
      case UNITTEST::TestOneof2::kFooBytes:
        EXPECT_TRUE(message.has_foo_bytes());
        break;
      case UNITTEST::TestOneof2::kFooEnum:
        EXPECT_TRUE(message.has_foo_enum());
        break;
      case UNITTEST::TestOneof2::kFooMessage:
        EXPECT_TRUE(message.has_foo_message());
        break;
      case UNITTEST::TestOneof2::kFoogroup:
        EXPECT_TRUE(message.has_foogroup());
        break;
      case UNITTEST::TestOneof2::kFooLazyMessage:
        EXPECT_TRUE(message.has_foo_lazy_message());
        break;
      case UNITTEST::TestOneof2::FOO_NOT_SET:
        break;
    }
  }
};

TEST_F(OneofTest, SettingOneFieldClearsOthers) {
  UNITTEST::TestOneof2 message;

  message.set_foo_int(123);
  EXPECT_TRUE(message.has_foo_int());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);

  message.set_foo_string("foo");
  EXPECT_TRUE(message.has_foo_string());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);


  message.set_foo_bytes("qux");
  EXPECT_TRUE(message.has_foo_bytes());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);

  message.set_foo_enum(UNITTEST::TestOneof2::FOO);
  EXPECT_TRUE(message.has_foo_enum());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);

  message.mutable_foo_message()->set_qux_int(234);
  EXPECT_TRUE(message.has_foo_message());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);

  message.mutable_foogroup()->set_a(345);
  EXPECT_TRUE(message.has_foogroup());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);


  // we repeat this because we didn't test if this properly clears other fields
  // at the beginning.
  message.set_foo_int(123);
  EXPECT_TRUE(message.has_foo_int());
  TestUtil::ExpectAtMostOneFieldSetInOneof(message);
}

TEST_F(OneofTest, EnumCases) {
  UNITTEST::TestOneof2 message;

  message.set_foo_int(123);
  ExpectEnumCasesWork(message);
  message.set_foo_string("foo");
  ExpectEnumCasesWork(message);
  message.set_foo_bytes("qux");
  ExpectEnumCasesWork(message);
  message.set_foo_enum(UNITTEST::TestOneof2::FOO);
  ExpectEnumCasesWork(message);
  message.mutable_foo_message()->set_qux_int(234);
  ExpectEnumCasesWork(message);
  message.mutable_foogroup()->set_a(345);
  ExpectEnumCasesWork(message);
}

TEST_F(OneofTest, PrimitiveType) {
  UNITTEST::TestOneof2 message;
  // Unset field returns default value
  EXPECT_EQ(message.foo_int(), 0);

  message.set_foo_int(123);
  EXPECT_TRUE(message.has_foo_int());
  EXPECT_EQ(message.foo_int(), 123);
  message.clear_foo_int();
  EXPECT_FALSE(message.has_foo_int());
}

TEST_F(OneofTest, EnumType) {
  UNITTEST::TestOneof2 message;
  // Unset field returns default value
  EXPECT_EQ(message.foo_enum(), 1);

  message.set_foo_enum(UNITTEST::TestOneof2::FOO);
  EXPECT_TRUE(message.has_foo_enum());
  EXPECT_EQ(message.foo_enum(), UNITTEST::TestOneof2::FOO);
  message.clear_foo_enum();
  EXPECT_FALSE(message.has_foo_enum());
}

TEST_F(OneofTest, SetString) {
  // Check that setting a string field in various ways works
  UNITTEST::TestOneof2 message;

  // Unset field returns default value
  EXPECT_EQ(message.foo_string(), "");

  message.set_foo_string("foo");
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "foo");
  message.clear_foo_string();
  EXPECT_FALSE(message.has_foo_string());

  message.set_foo_string(std::string("bar"));
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "bar");
  message.clear_foo_string();
  EXPECT_FALSE(message.has_foo_string());


  message.set_foo_string("qux", 3);
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "qux");
  message.clear_foo_string();
  EXPECT_FALSE(message.has_foo_string());

  message.mutable_foo_string()->assign("quux");
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "quux");
  message.clear_foo_string();
  EXPECT_FALSE(message.has_foo_string());

  message.set_foo_string("corge");
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "corge");
  message.clear_foo_string();
  EXPECT_FALSE(message.has_foo_string());
}

TEST_F(OneofTest, ReleaseString) {
  // Check that release_foo() starts out NULL, and gives us a value
  // that we can delete after it's been set.
  UNITTEST::TestOneof2 message;

  EXPECT_EQ(NULL, message.release_foo_string());
  EXPECT_FALSE(message.has_foo_string());

  message.set_foo_string("blah");
  EXPECT_TRUE(message.has_foo_string());
  std::unique_ptr<std::string> str(message.release_foo_string());
  EXPECT_FALSE(message.has_foo_string());
  ASSERT_TRUE(str != NULL);
  EXPECT_EQ("blah", *str);

  EXPECT_EQ(NULL, message.release_foo_string());
  EXPECT_FALSE(message.has_foo_string());
}

TEST_F(OneofTest, SetAllocatedString) {
  // Check that set_allocated_foo() works for strings.
  UNITTEST::TestOneof2 message;

  EXPECT_FALSE(message.has_foo_string());
  const std::string kHello("hello");
  message.set_foo_string(kHello);
  EXPECT_TRUE(message.has_foo_string());

  message.set_allocated_foo_string(NULL);
  EXPECT_FALSE(message.has_foo_string());
  EXPECT_EQ("", message.foo_string());

  message.set_allocated_foo_string(new std::string(kHello));
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(kHello, message.foo_string());
}


TEST_F(OneofTest, SetMessage) {
  // Check that setting a message field works
  UNITTEST::TestOneof2 message;

  // Unset field returns default instance
  EXPECT_EQ(&message.foo_message(),
            &UNITTEST::TestOneof2_NestedMessage::default_instance());
  EXPECT_EQ(message.foo_message().qux_int(), 0);

  message.mutable_foo_message()->set_qux_int(234);
  EXPECT_TRUE(message.has_foo_message());
  EXPECT_EQ(message.foo_message().qux_int(), 234);
  message.clear_foo_message();
  EXPECT_FALSE(message.has_foo_message());
}

TEST_F(OneofTest, ReleaseMessage) {
  // Check that release_foo() starts out NULL, and gives us a value
  // that we can delete after it's been set.
  UNITTEST::TestOneof2 message;

  EXPECT_EQ(NULL, message.release_foo_message());
  EXPECT_FALSE(message.has_foo_message());

  message.mutable_foo_message()->set_qux_int(1);
  EXPECT_TRUE(message.has_foo_message());
  std::unique_ptr<UNITTEST::TestOneof2_NestedMessage> mes(
      message.release_foo_message());
  EXPECT_FALSE(message.has_foo_message());
  ASSERT_TRUE(mes != NULL);
  EXPECT_EQ(1, mes->qux_int());

  EXPECT_EQ(NULL, message.release_foo_message());
  EXPECT_FALSE(message.has_foo_message());
}

TEST_F(OneofTest, SetAllocatedMessage) {
  // Check that set_allocated_foo() works for messages.
  UNITTEST::TestOneof2 message;

  EXPECT_FALSE(message.has_foo_message());

  message.mutable_foo_message()->set_qux_int(1);
  EXPECT_TRUE(message.has_foo_message());

  message.set_allocated_foo_message(NULL);
  EXPECT_FALSE(message.has_foo_message());
  EXPECT_EQ(&message.foo_message(),
            &UNITTEST::TestOneof2_NestedMessage::default_instance());

  message.mutable_foo_message()->set_qux_int(1);
  UNITTEST::TestOneof2_NestedMessage* mes = message.release_foo_message();
  ASSERT_TRUE(mes != NULL);
  EXPECT_FALSE(message.has_foo_message());

  message.set_allocated_foo_message(mes);
  EXPECT_TRUE(message.has_foo_message());
  EXPECT_EQ(1, message.foo_message().qux_int());
}


TEST_F(OneofTest, Clear) {
  UNITTEST::TestOneof2 message;

  message.set_foo_int(1);
  EXPECT_TRUE(message.has_foo_int());
  message.clear_foo_int();
  EXPECT_FALSE(message.has_foo_int());
}

TEST_F(OneofTest, Defaults) {
  UNITTEST::TestOneof2 message;

  EXPECT_FALSE(message.has_foo_int());
  EXPECT_EQ(message.foo_int(), 0);

  EXPECT_FALSE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "");


  EXPECT_FALSE(message.has_foo_bytes());
  EXPECT_EQ(message.foo_bytes(), "");

  EXPECT_FALSE(message.has_foo_enum());
  EXPECT_EQ(message.foo_enum(), 1);

  EXPECT_FALSE(message.has_foo_message());
  EXPECT_EQ(message.foo_message().qux_int(), 0);

  EXPECT_FALSE(message.has_foogroup());
  EXPECT_EQ(message.foogroup().a(), 0);


  EXPECT_FALSE(message.has_bar_int());
  EXPECT_EQ(message.bar_int(), 5);

  EXPECT_FALSE(message.has_bar_string());
  EXPECT_EQ(message.bar_string(), "STRING");


  EXPECT_FALSE(message.has_bar_bytes());
  EXPECT_EQ(message.bar_bytes(), "BYTES");

  EXPECT_FALSE(message.has_bar_enum());
  EXPECT_EQ(message.bar_enum(), 2);
}

TEST_F(OneofTest, SwapWithEmpty) {
  UNITTEST::TestOneof2 message1, message2;
  message1.set_foo_string("FOO");
  EXPECT_TRUE(message1.has_foo_string());
  message1.Swap(&message2);
  EXPECT_FALSE(message1.has_foo_string());
  EXPECT_TRUE(message2.has_foo_string());
  EXPECT_EQ(message2.foo_string(), "FOO");
}

TEST_F(OneofTest, SwapWithSelf) {
  UNITTEST::TestOneof2 message;
  message.set_foo_string("FOO");
  EXPECT_TRUE(message.has_foo_string());
  message.Swap(&message);
  EXPECT_TRUE(message.has_foo_string());
  EXPECT_EQ(message.foo_string(), "FOO");
}

TEST_F(OneofTest, SwapBothHasFields) {
  UNITTEST::TestOneof2 message1, message2;

  message1.set_foo_string("FOO");
  EXPECT_TRUE(message1.has_foo_string());
  message2.mutable_foo_message()->set_qux_int(1);
  EXPECT_TRUE(message2.has_foo_message());

  message1.Swap(&message2);
  EXPECT_FALSE(message1.has_foo_string());
  EXPECT_FALSE(message2.has_foo_message());
  EXPECT_TRUE(message1.has_foo_message());
  EXPECT_EQ(message1.foo_message().qux_int(), 1);
  EXPECT_TRUE(message2.has_foo_string());
  EXPECT_EQ(message2.foo_string(), "FOO");
}

TEST_F(OneofTest, CopyConstructor) {
  UNITTEST::TestOneof2 message1;
  message1.set_foo_bytes("FOO");

  UNITTEST::TestOneof2 message2(message1);
  EXPECT_TRUE(message2.has_foo_bytes());
  EXPECT_EQ(message2.foo_bytes(), "FOO");
}

TEST_F(OneofTest, CopyFrom) {
  UNITTEST::TestOneof2 message1, message2;
  message1.set_foo_enum(UNITTEST::TestOneof2::BAR);
  EXPECT_TRUE(message1.has_foo_enum());

  message2.CopyFrom(message1);
  EXPECT_TRUE(message2.has_foo_enum());
  EXPECT_EQ(message2.foo_enum(), UNITTEST::TestOneof2::BAR);

  // Copying from self should be a no-op.
  message2.CopyFrom(message2);
  EXPECT_TRUE(message2.has_foo_enum());
  EXPECT_EQ(message2.foo_enum(), UNITTEST::TestOneof2::BAR);
}

TEST_F(OneofTest, CopyAssignmentOperator) {
  UNITTEST::TestOneof2 message1;
  message1.mutable_foo_message()->set_qux_int(123);
  EXPECT_TRUE(message1.has_foo_message());

  UNITTEST::TestOneof2 message2;
  message2 = message1;
  EXPECT_EQ(message2.foo_message().qux_int(), 123);

  // Make sure that self-assignment does something sane.
  message2 = *&message2;  // Avoid -Wself-assign.
  EXPECT_EQ(message2.foo_message().qux_int(), 123);
}

TEST_F(OneofTest, UpcastCopyFrom) {
  // Test the CopyFrom method that takes in the generic const Message&
  // parameter.
  UNITTEST::TestOneof2 message1, message2;
  message1.mutable_foogroup()->set_a(123);
  EXPECT_TRUE(message1.has_foogroup());

  const Message* source = implicit_cast<const Message*>(&message1);
  message2.CopyFrom(*source);

  EXPECT_TRUE(message2.has_foogroup());
  EXPECT_EQ(message2.foogroup().a(), 123);
}

// Test the generated SerializeWithCachedSizesToArray(),
// This indirectly tests MergePartialFromCodedStream()
// We have to test each field type separately because we cannot set them at the
// same time
TEST_F(OneofTest, SerializationToArray) {
  // Primitive type
  {
    UNITTEST::TestOneof2 message1, message2;
std::string data;
message1.set_foo_int(123);
int size = message1.ByteSizeLong();
data.resize(size);
uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
uint8* end = message1.SerializeWithCachedSizesToArray(start);
EXPECT_EQ(size, end - start);
EXPECT_TRUE(message2.ParseFromString(data));
EXPECT_EQ(message2.foo_int(), 123);
  }

  // String
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.set_foo_string("foo");
    int size = message1.ByteSizeLong();
    data.resize(size);
    uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
    uint8* end = message1.SerializeWithCachedSizesToArray(start);
    EXPECT_EQ(size, end - start);
    EXPECT_TRUE(message2.ParseFromString(data));
    EXPECT_EQ(message2.foo_string(), "foo");
  }


  // Bytes
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.set_foo_bytes("qux");
    int size = message1.ByteSizeLong();
    data.resize(size);
    uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
    uint8* end = message1.SerializeWithCachedSizesToArray(start);
    EXPECT_EQ(size, end - start);
    EXPECT_TRUE(message2.ParseFromString(data));
    EXPECT_EQ(message2.foo_bytes(), "qux");
  }

  // Enum
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.set_foo_enum(UNITTEST::TestOneof2::FOO);
    int size = message1.ByteSizeLong();
    data.resize(size);
    uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
    uint8* end = message1.SerializeWithCachedSizesToArray(start);
    EXPECT_EQ(size, end - start);
    EXPECT_TRUE(message2.ParseFromString(data));
    EXPECT_EQ(message2.foo_enum(), UNITTEST::TestOneof2::FOO);
  }

  // Message
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.mutable_foo_message()->set_qux_int(234);
    int size = message1.ByteSizeLong();
    data.resize(size);
    uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
    uint8* end = message1.SerializeWithCachedSizesToArray(start);
    EXPECT_EQ(size, end - start);
    EXPECT_TRUE(message2.ParseFromString(data));
    EXPECT_EQ(message2.foo_message().qux_int(), 234);
  }

  // Group
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.mutable_foogroup()->set_a(345);
    int size = message1.ByteSizeLong();
    data.resize(size);
    uint8* start = reinterpret_cast<uint8*>(::google::protobuf::string_as_array(&data));
    uint8* end = message1.SerializeWithCachedSizesToArray(start);
    EXPECT_EQ(size, end - start);
    EXPECT_TRUE(message2.ParseFromString(data));
    EXPECT_EQ(message2.foogroup().a(), 345);
  }

}

// Test the generated SerializeWithCachedSizes() by forcing the buffer to write
// one byte at a time.
// This indirectly tests MergePartialFromCodedStream()
// We have to test each field type separately because we cannot set them at the
// same time
TEST_F(OneofTest, SerializationToStream) {
  // Primitive type
  {
    UNITTEST::TestOneof2 message1, message2;
std::string data;
message1.set_foo_int(123);
int size = message1.ByteSizeLong();
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
    EXPECT_EQ(message2.foo_int(), 123);
  }

  // String
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.set_foo_string("foo");
    int size = message1.ByteSizeLong();
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
    EXPECT_EQ(message2.foo_string(), "foo");
  }


  // Bytes
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.set_foo_bytes("qux");
    int size = message1.ByteSizeLong();
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
    EXPECT_EQ(message2.foo_bytes(), "qux");
  }

  // Enum
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.set_foo_enum(UNITTEST::TestOneof2::FOO);
    int size = message1.ByteSizeLong();
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
    EXPECT_EQ(message2.foo_enum(), UNITTEST::TestOneof2::FOO);
  }

  // Message
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.mutable_foo_message()->set_qux_int(234);
    int size = message1.ByteSizeLong();
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
    EXPECT_EQ(message2.foo_message().qux_int(), 234);
  }

  // Group
  {
    UNITTEST::TestOneof2 message1, message2;
    std::string data;
    message1.mutable_foogroup()->set_a(345);
    int size = message1.ByteSizeLong();
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
    EXPECT_EQ(message2.foogroup().a(), 345);
  }

}

TEST_F(OneofTest, MergeFrom) {
  UNITTEST::TestOneof2 message1, message2;

  message1.set_foo_int(123);
  message2.MergeFrom(message1);
  TestUtil::ExpectAtMostOneFieldSetInOneof(message2);
  EXPECT_TRUE(message2.has_foo_int());
  EXPECT_EQ(message2.foo_int(), 123);

  message1.set_foo_string("foo");
  message2.MergeFrom(message1);
  TestUtil::ExpectAtMostOneFieldSetInOneof(message2);
  EXPECT_TRUE(message2.has_foo_string());
  EXPECT_EQ(message2.foo_string(), "foo");


  message1.set_foo_bytes("qux");
  message2.MergeFrom(message1);
  TestUtil::ExpectAtMostOneFieldSetInOneof(message2);
  EXPECT_TRUE(message2.has_foo_bytes());
  EXPECT_EQ(message2.foo_bytes(), "qux");

  message1.set_foo_enum(UNITTEST::TestOneof2::FOO);
  message2.MergeFrom(message1);
  TestUtil::ExpectAtMostOneFieldSetInOneof(message2);
  EXPECT_TRUE(message2.has_foo_enum());
  EXPECT_EQ(message2.foo_enum(), UNITTEST::TestOneof2::FOO);

  message1.mutable_foo_message()->set_qux_int(234);
  message2.MergeFrom(message1);
  TestUtil::ExpectAtMostOneFieldSetInOneof(message2);
  EXPECT_TRUE(message2.has_foo_message());
  EXPECT_EQ(message2.foo_message().qux_int(), 234);

  message1.mutable_foogroup()->set_a(345);
  message2.MergeFrom(message1);
  TestUtil::ExpectAtMostOneFieldSetInOneof(message2);
  EXPECT_TRUE(message2.has_foogroup());
  EXPECT_EQ(message2.foogroup().a(), 345);

}

TEST(HELPERS_TEST_NAME, TestSCC) {
  UNITTEST::TestMutualRecursionA a;
  MessageSCCAnalyzer scc_analyzer((Options()));
  const SCC* scc = scc_analyzer.GetSCC(a.GetDescriptor());
  std::vector<std::string> names;
  names.reserve(scc->descriptors.size());
  for (int i = 0; i < scc->descriptors.size(); i++) {
    names.push_back(scc->descriptors[i]->full_name());
  }
  std::string package = a.GetDescriptor()->file()->package();
  ASSERT_EQ(names.size(), 4);
  std::sort(names.begin(), names.end());
  EXPECT_EQ(names[0], package + ".TestMutualRecursionA");
  EXPECT_EQ(names[1], package + ".TestMutualRecursionA.SubGroup");
  EXPECT_EQ(names[2], package + ".TestMutualRecursionA.SubMessage");
  EXPECT_EQ(names[3], package + ".TestMutualRecursionB");

  MessageAnalysis result = scc_analyzer.GetSCCAnalysis(scc);
  EXPECT_EQ(result.is_recursive, true);
  EXPECT_EQ(result.contains_required, false);
  EXPECT_EQ(result.contains_cord, true);  // TestAllTypes
  EXPECT_EQ(result.contains_extension, false);  // TestAllTypes
}

TEST(HELPERS_TEST_NAME, TestSCCAnalysis) {
  {
    UNITTEST::TestRecursiveMessage msg;
    MessageSCCAnalyzer scc_analyzer((Options()));
    const SCC* scc = scc_analyzer.GetSCC(msg.GetDescriptor());
    MessageAnalysis result = scc_analyzer.GetSCCAnalysis(scc);
    EXPECT_EQ(result.is_recursive, true);
    EXPECT_EQ(result.contains_required, false);
    EXPECT_EQ(result.contains_cord, false);
    EXPECT_EQ(result.contains_extension, false);
  }
  {
    UNITTEST::TestAllExtensions msg;
    MessageSCCAnalyzer scc_analyzer((Options()));
    const SCC* scc = scc_analyzer.GetSCC(msg.GetDescriptor());
    MessageAnalysis result = scc_analyzer.GetSCCAnalysis(scc);
    EXPECT_EQ(result.is_recursive, false);
    EXPECT_EQ(result.contains_required, false);
    EXPECT_EQ(result.contains_cord, false);
    EXPECT_EQ(result.contains_extension, true);
  }
  {
    UNITTEST::TestRequired msg;
    MessageSCCAnalyzer scc_analyzer((Options()));
    const SCC* scc = scc_analyzer.GetSCC(msg.GetDescriptor());
    MessageAnalysis result = scc_analyzer.GetSCCAnalysis(scc);
    EXPECT_EQ(result.is_recursive, false);
    EXPECT_EQ(result.contains_required, true);
    EXPECT_EQ(result.contains_cord, false);
    EXPECT_EQ(result.contains_extension, false);
  }
}

}  // namespace cpp_unittest
}  // namespace cpp
}  // namespace compiler

namespace no_generic_services_test {
  // Verify that no class called "TestService" was defined in
  // unittest_no_generic_services.pb.h by defining a different type by the same
  // name.  If such a service was generated, this will not compile.
  struct TestService {
    int i;
  };
}

namespace compiler {
namespace cpp {
namespace cpp_unittest {

TEST_F(GENERATED_SERVICE_TEST_NAME, NoGenericServices) {
  // Verify that non-services in unittest_no_generic_services.proto were
  // generated.
  ::protobuf_unittest::no_generic_services_test::TestMessage message;
  message.set_a(1);
  message.SetExtension(
      ::protobuf_unittest::no_generic_services_test::test_extension, 123);
  ::protobuf_unittest::no_generic_services_test::TestEnum e =
      ::protobuf_unittest::no_generic_services_test::FOO;
  EXPECT_EQ(e, 1);

  // Verify that a ServiceDescriptor is generated for the service even if the
  // class itself is not.
  const FileDescriptor* file =
      ::google::protobuf::unittest::no_generic_services_test::TestMessage::descriptor()
          ->file();

  ASSERT_EQ(1, file->service_count());
  EXPECT_EQ("TestService", file->service(0)->name());
  ASSERT_EQ(1, file->service(0)->method_count());
  EXPECT_EQ("Foo", file->service(0)->method(0)->name());
}

#endif  // !PROTOBUF_TEST_NO_DESCRIPTORS

// ===================================================================

// This test must run last.  It verifies that descriptors were or were not
// initialized depending on whether PROTOBUF_TEST_NO_DESCRIPTORS was defined.
// When this is defined, we skip all tests which are expected to trigger
// descriptor initialization.  This verifies that everything else still works
// if descriptors are not initialized.
TEST(DESCRIPTOR_INIT_TEST_NAME, Initialized) {
#ifdef PROTOBUF_TEST_NO_DESCRIPTORS
  bool should_have_descriptors = false;
#else
  bool should_have_descriptors = true;
#endif

  EXPECT_EQ(should_have_descriptors,
            DescriptorPool::generated_pool()->InternalIsFileLoaded(
                TestUtil::MaybeTranslatePath(UNITTEST_PROTO_PATH)));
}

}  // namespace cpp_unittest

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>
