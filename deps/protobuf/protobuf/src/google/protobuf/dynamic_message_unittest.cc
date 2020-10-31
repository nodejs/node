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
// Since the reflection interface for DynamicMessage is implemented by
// GenericMessageReflection, the only thing we really have to test is
// that DynamicMessage correctly sets up the information that
// GenericMessageReflection needs to use.  So, we focus on that in this
// test.  Other tests, such as generic_message_reflection_unittest and
// reflection_ops_unittest, cover the rest of the functionality used by
// DynamicMessage.

#include <memory>

#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_no_field_presence.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

class DynamicMessageTest : public ::testing::TestWithParam<bool> {
 protected:
  DescriptorPool pool_;
  DynamicMessageFactory factory_;
  const Descriptor* descriptor_;
  const Message* prototype_;
  const Descriptor* extensions_descriptor_;
  const Message* extensions_prototype_;
  const Descriptor* packed_descriptor_;
  const Message* packed_prototype_;
  const Descriptor* oneof_descriptor_;
  const Message* oneof_prototype_;
  const Descriptor* proto3_descriptor_;
  const Message* proto3_prototype_;

  DynamicMessageTest() : factory_(&pool_) {}

  virtual void SetUp() {
    // We want to make sure that DynamicMessage works (particularly with
    // extensions) even if we use descriptors that are *not* from compiled-in
    // types, so we make copies of the descriptors for unittest.proto and
    // unittest_import.proto.
    FileDescriptorProto unittest_file;
    FileDescriptorProto unittest_import_file;
    FileDescriptorProto unittest_import_public_file;
    FileDescriptorProto unittest_no_field_presence_file;

    unittest::TestAllTypes::descriptor()->file()->CopyTo(&unittest_file);
    unittest_import::ImportMessage::descriptor()->file()->CopyTo(
        &unittest_import_file);
    unittest_import::PublicImportMessage::descriptor()->file()->CopyTo(
        &unittest_import_public_file);
    proto2_nofieldpresence_unittest::TestAllTypes::descriptor()->file()->CopyTo(
        &unittest_no_field_presence_file);

    ASSERT_TRUE(pool_.BuildFile(unittest_import_public_file) != NULL);
    ASSERT_TRUE(pool_.BuildFile(unittest_import_file) != NULL);
    ASSERT_TRUE(pool_.BuildFile(unittest_file) != NULL);
    ASSERT_TRUE(pool_.BuildFile(unittest_no_field_presence_file) != NULL);

    descriptor_ = pool_.FindMessageTypeByName("protobuf_unittest.TestAllTypes");
    ASSERT_TRUE(descriptor_ != NULL);
    prototype_ = factory_.GetPrototype(descriptor_);

    extensions_descriptor_ =
        pool_.FindMessageTypeByName("protobuf_unittest.TestAllExtensions");
    ASSERT_TRUE(extensions_descriptor_ != NULL);
    extensions_prototype_ = factory_.GetPrototype(extensions_descriptor_);

    packed_descriptor_ =
        pool_.FindMessageTypeByName("protobuf_unittest.TestPackedTypes");
    ASSERT_TRUE(packed_descriptor_ != NULL);
    packed_prototype_ = factory_.GetPrototype(packed_descriptor_);

    oneof_descriptor_ =
        pool_.FindMessageTypeByName("protobuf_unittest.TestOneof2");
    ASSERT_TRUE(oneof_descriptor_ != NULL);
    oneof_prototype_ = factory_.GetPrototype(oneof_descriptor_);

    proto3_descriptor_ = pool_.FindMessageTypeByName(
        "proto2_nofieldpresence_unittest.TestAllTypes");
    ASSERT_TRUE(proto3_descriptor_ != NULL);
    proto3_prototype_ = factory_.GetPrototype(proto3_descriptor_);
  }
};

TEST_F(DynamicMessageTest, Descriptor) {
  // Check that the descriptor on the DynamicMessage matches the descriptor
  // passed to GetPrototype().
  EXPECT_EQ(prototype_->GetDescriptor(), descriptor_);
}

TEST_F(DynamicMessageTest, OnePrototype) {
  // Check that requesting the same prototype twice produces the same object.
  EXPECT_EQ(prototype_, factory_.GetPrototype(descriptor_));
}

TEST_F(DynamicMessageTest, Defaults) {
  // Check that all default values are set correctly in the initial message.
  TestUtil::ReflectionTester reflection_tester(descriptor_);
  reflection_tester.ExpectClearViaReflection(*prototype_);
}

TEST_P(DynamicMessageTest, IndependentOffsets) {
  // Check that all fields have independent offsets by setting each
  // one to a unique value then checking that they all still have those
  // unique values (i.e. they don't stomp each other).
  Arena arena;
  Message* message = prototype_->New(GetParam() ? &arena : NULL);
  TestUtil::ReflectionTester reflection_tester(descriptor_);

  reflection_tester.SetAllFieldsViaReflection(message);
  reflection_tester.ExpectAllFieldsSetViaReflection(*message);

  if (!GetParam()) {
    delete message;
  }
}

TEST_P(DynamicMessageTest, Extensions) {
  // Check that extensions work.
  Arena arena;
  Message* message = extensions_prototype_->New(GetParam() ? &arena : NULL);
  TestUtil::ReflectionTester reflection_tester(extensions_descriptor_);

  reflection_tester.SetAllFieldsViaReflection(message);
  reflection_tester.ExpectAllFieldsSetViaReflection(*message);

  if (!GetParam()) {
    delete message;
  }
}

TEST_P(DynamicMessageTest, PackedFields) {
  // Check that packed fields work properly.
  Arena arena;
  Message* message = packed_prototype_->New(GetParam() ? &arena : NULL);
  TestUtil::ReflectionTester reflection_tester(packed_descriptor_);

  reflection_tester.SetPackedFieldsViaReflection(message);
  reflection_tester.ExpectPackedFieldsSetViaReflection(*message);

  if (!GetParam()) {
    delete message;
  }
}

TEST_P(DynamicMessageTest, Oneof) {
  // Check that oneof fields work properly.
  Arena arena;
  Message* message = oneof_prototype_->New(GetParam() ? &arena : NULL);

  // Check default values.
  const Descriptor* descriptor = message->GetDescriptor();
  const Reflection* reflection = message->GetReflection();
  EXPECT_EQ(0, reflection->GetInt32(*message,
                                    descriptor->FindFieldByName("foo_int")));
  EXPECT_EQ("", reflection->GetString(
                    *message, descriptor->FindFieldByName("foo_string")));
  EXPECT_EQ("", reflection->GetString(*message,
                                      descriptor->FindFieldByName("foo_cord")));
  EXPECT_EQ("", reflection->GetString(
                    *message, descriptor->FindFieldByName("foo_string_piece")));
  EXPECT_EQ("", reflection->GetString(
                    *message, descriptor->FindFieldByName("foo_bytes")));
  EXPECT_EQ(
      unittest::TestOneof2::FOO,
      reflection->GetEnum(*message, descriptor->FindFieldByName("foo_enum"))
          ->number());
  const Descriptor* nested_descriptor;
  const Message* nested_prototype;
  nested_descriptor =
      pool_.FindMessageTypeByName("protobuf_unittest.TestOneof2.NestedMessage");
  nested_prototype = factory_.GetPrototype(nested_descriptor);
  EXPECT_EQ(nested_prototype,
            &reflection->GetMessage(
                *message, descriptor->FindFieldByName("foo_message")));
  const Descriptor* foogroup_descriptor;
  const Message* foogroup_prototype;
  foogroup_descriptor =
      pool_.FindMessageTypeByName("protobuf_unittest.TestOneof2.FooGroup");
  foogroup_prototype = factory_.GetPrototype(foogroup_descriptor);
  EXPECT_EQ(foogroup_prototype,
            &reflection->GetMessage(*message,
                                    descriptor->FindFieldByName("foogroup")));
  EXPECT_NE(foogroup_prototype,
            &reflection->GetMessage(
                *message, descriptor->FindFieldByName("foo_lazy_message")));
  EXPECT_EQ(5, reflection->GetInt32(*message,
                                    descriptor->FindFieldByName("bar_int")));
  EXPECT_EQ("STRING", reflection->GetString(
                          *message, descriptor->FindFieldByName("bar_string")));
  EXPECT_EQ("CORD", reflection->GetString(
                        *message, descriptor->FindFieldByName("bar_cord")));
  EXPECT_EQ("SPIECE",
            reflection->GetString(
                *message, descriptor->FindFieldByName("bar_string_piece")));
  EXPECT_EQ("BYTES", reflection->GetString(
                         *message, descriptor->FindFieldByName("bar_bytes")));
  EXPECT_EQ(
      unittest::TestOneof2::BAR,
      reflection->GetEnum(*message, descriptor->FindFieldByName("bar_enum"))
          ->number());

  // Check set functions.
  TestUtil::ReflectionTester reflection_tester(oneof_descriptor_);
  reflection_tester.SetOneofViaReflection(message);
  reflection_tester.ExpectOneofSetViaReflection(*message);

  if (!GetParam()) {
    delete message;
  }
}

TEST_P(DynamicMessageTest, SpaceUsed) {
  // Test that SpaceUsed() works properly

  // Since we share the implementation with generated messages, we don't need
  // to test very much here.  Just make sure it appears to be working.

  Arena arena;
  Message* message = prototype_->New(GetParam() ? &arena : NULL);
  TestUtil::ReflectionTester reflection_tester(descriptor_);

  int initial_space_used = message->SpaceUsed();

  reflection_tester.SetAllFieldsViaReflection(message);
  EXPECT_LT(initial_space_used, message->SpaceUsed());

  if (!GetParam()) {
    delete message;
  }
}

TEST_F(DynamicMessageTest, Arena) {
  Arena arena;
  Message* message = prototype_->New(&arena);
  Message* extension_message = extensions_prototype_->New(&arena);
  Message* packed_message = packed_prototype_->New(&arena);
  Message* oneof_message = oneof_prototype_->New(&arena);

  // avoid unused-variable error.
  (void)message;
  (void)extension_message;
  (void)packed_message;
  (void)oneof_message;
  // Return without freeing: should not leak.
}

TEST_F(DynamicMessageTest, Proto3) {
  Message* message = proto3_prototype_->New();
  const Reflection* refl = message->GetReflection();
  const Descriptor* desc = message->GetDescriptor();

  // Just test a single primtive and single message field here to make sure we
  // are getting the no-field-presence semantics elsewhere. DynamicMessage uses
  // GeneratedMessageReflection under the hood, so the rest should be fine as
  // long as GMR recognizes that we're using a proto3 message.
  const FieldDescriptor* optional_int32 =
      desc->FindFieldByName("optional_int32");
  const FieldDescriptor* optional_msg =
      desc->FindFieldByName("optional_nested_message");
  EXPECT_TRUE(optional_int32 != NULL);
  EXPECT_TRUE(optional_msg != NULL);

  EXPECT_EQ(false, refl->HasField(*message, optional_int32));
  refl->SetInt32(message, optional_int32, 42);
  EXPECT_EQ(true, refl->HasField(*message, optional_int32));
  refl->SetInt32(message, optional_int32, 0);
  EXPECT_EQ(false, refl->HasField(*message, optional_int32));

  EXPECT_EQ(false, refl->HasField(*message, optional_msg));
  refl->MutableMessage(message, optional_msg);
  EXPECT_EQ(true, refl->HasField(*message, optional_msg));
  delete refl->ReleaseMessage(message, optional_msg);
  EXPECT_EQ(false, refl->HasField(*message, optional_msg));

  // Also ensure that the default instance handles field presence properly.
  EXPECT_EQ(false, refl->HasField(*proto3_prototype_, optional_msg));

  delete message;
}

INSTANTIATE_TEST_SUITE_P(UseArena, DynamicMessageTest, ::testing::Bool());

}  // namespace protobuf
}  // namespace google
