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

#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_embed_optimize_for.pb.h>
#include <google/protobuf/unittest_optimize_for.pb.h>

#include <google/protobuf/test_util.h>

#define MESSAGE_TEST_NAME MessageTest
#define GENERATED_DESCRIPTOR_TEST_NAME GeneratedDescriptorTest
#define GENERATED_MESSAGE_TEST_NAME GeneratedMessageTest
#define GENERATED_ENUM_TEST_NAME GeneratedEnumTest
#define GENERATED_SERVICE_TEST_NAME GeneratedServiceTest
#define HELPERS_TEST_NAME HelpersTest
#define DESCRIPTOR_INIT_TEST_NAME DescriptorInitializationTest

#define UNITTEST_PROTO_PATH "net/proto2/internal/unittest.proto"
#define UNITTEST ::protobuf_unittest
#define UNITTEST_IMPORT ::protobuf_unittest_import

// Must include after the above macros.
#include <google/protobuf/compiler/cpp/cpp_unittest.inc>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// Can't use an anonymous namespace here due to brokenness of Tru64 compiler.
namespace cpp_unittest {

namespace protobuf_unittest = ::protobuf_unittest;

TEST(GENERATED_MESSAGE_TEST_NAME, TestConflictingSymbolNames) {
  // test_bad_identifiers.proto successfully compiled, then it works.  The
  // following is just a token usage to insure that the code is, in fact,
  // being compiled and linked.

  protobuf_unittest::TestConflictingSymbolNames message;
  message.set_uint32(1);
  EXPECT_EQ(3, message.ByteSizeLong());

  message.set_friend_(5);
  EXPECT_EQ(5, message.friend_());

  message.set_class_(6);
  EXPECT_EQ(6, message.class_());

  // Instantiate extension template functions to test conflicting template
  // parameter names.
  typedef protobuf_unittest::TestConflictingSymbolNamesExtension ExtensionMessage;
  message.AddExtension(ExtensionMessage::repeated_int32_ext, 123);
  EXPECT_EQ(123, message.GetExtension(ExtensionMessage::repeated_int32_ext, 0));
}

TEST(GENERATED_MESSAGE_TEST_NAME, TestConflictingEnumNames) {
  protobuf_unittest::TestConflictingEnumNames message;
  message.set_conflicting_enum(
      protobuf_unittest::TestConflictingEnumNames_while_and_);
  EXPECT_EQ(1, message.conflicting_enum());
  message.set_conflicting_enum(
      protobuf_unittest::TestConflictingEnumNames_while_XOR);
  EXPECT_EQ(5, message.conflicting_enum());

  protobuf_unittest::bool_ conflicting_enum;
  conflicting_enum = protobuf_unittest::NOT_EQ;
  EXPECT_EQ(1, conflicting_enum);
  conflicting_enum = protobuf_unittest::return_;
  EXPECT_EQ(3, conflicting_enum);
}

TEST(GENERATED_MESSAGE_TEST_NAME, TestConflictingMessageNames) {
  protobuf_unittest::NULL_ message;
  message.set_int_(123);
  EXPECT_EQ(message.int_(), 123);
}

TEST(GENERATED_MESSAGE_TEST_NAME, TestConflictingExtension) {
  protobuf_unittest::TestConflictingSymbolNames message;
  message.SetExtension(protobuf_unittest::void_, 123);
  EXPECT_EQ(123, message.GetExtension(protobuf_unittest::void_));
}

}  // namespace cpp_unittest
}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
