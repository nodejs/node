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

#include <google/protobuf/any_test.pb.h>
#include <google/protobuf/unittest.pb.h>
#include <gtest/gtest.h>



namespace google {
namespace protobuf {
namespace {

TEST(AnyTest, TestPackAndUnpack) {
  protobuf_unittest::TestAny submessage;
  submessage.set_int32_value(12345);
  protobuf_unittest::TestAny message;
  message.mutable_any_value()->PackFrom(submessage);

  std::string data = message.SerializeAsString();

  ASSERT_TRUE(message.ParseFromString(data));
  EXPECT_TRUE(message.has_any_value());
  submessage.Clear();
  ASSERT_TRUE(message.any_value().UnpackTo(&submessage));
  EXPECT_EQ(12345, submessage.int32_value());
}

TEST(AnyTest, TestUnpackWithTypeMismatch) {
  protobuf_unittest::TestAny payload;
  payload.set_int32_value(13);
  google::protobuf::Any any;
  any.PackFrom(payload);

  // Attempt to unpack into the wrong type.
  protobuf_unittest::TestAllTypes dest;
  EXPECT_FALSE(any.UnpackTo(&dest));
}

TEST(AnyTest, TestPackAndUnpackAny) {
  // We can pack a Any message inside another Any message.
  protobuf_unittest::TestAny submessage;
  submessage.set_int32_value(12345);
  google::protobuf::Any any;
  any.PackFrom(submessage);
  protobuf_unittest::TestAny message;
  message.mutable_any_value()->PackFrom(any);

  std::string data = message.SerializeAsString();

  ASSERT_TRUE(message.ParseFromString(data));
  EXPECT_TRUE(message.has_any_value());
  any.Clear();
  submessage.Clear();
  ASSERT_TRUE(message.any_value().UnpackTo(&any));
  ASSERT_TRUE(any.UnpackTo(&submessage));
  EXPECT_EQ(12345, submessage.int32_value());
}

TEST(AnyTest, TestPackWithCustomTypeUrl) {
  protobuf_unittest::TestAny submessage;
  submessage.set_int32_value(12345);
  google::protobuf::Any any;
  // Pack with a custom type URL prefix.
  any.PackFrom(submessage, "type.myservice.com");
  EXPECT_EQ("type.myservice.com/protobuf_unittest.TestAny", any.type_url());
  // Pack with a custom type URL prefix ending with '/'.
  any.PackFrom(submessage, "type.myservice.com/");
  EXPECT_EQ("type.myservice.com/protobuf_unittest.TestAny", any.type_url());
  // Pack with an empty type URL prefix.
  any.PackFrom(submessage, "");
  EXPECT_EQ("/protobuf_unittest.TestAny", any.type_url());

  // Test unpacking the type.
  submessage.Clear();
  EXPECT_TRUE(any.UnpackTo(&submessage));
  EXPECT_EQ(12345, submessage.int32_value());
}

TEST(AnyTest, TestIs) {
  protobuf_unittest::TestAny submessage;
  submessage.set_int32_value(12345);
  google::protobuf::Any any;
  any.PackFrom(submessage);
  ASSERT_TRUE(any.ParseFromString(any.SerializeAsString()));
  EXPECT_TRUE(any.Is<protobuf_unittest::TestAny>());
  EXPECT_FALSE(any.Is<google::protobuf::Any>());

  protobuf_unittest::TestAny message;
  message.mutable_any_value()->PackFrom(any);
  ASSERT_TRUE(message.ParseFromString(message.SerializeAsString()));
  EXPECT_FALSE(message.any_value().Is<protobuf_unittest::TestAny>());
  EXPECT_TRUE(message.any_value().Is<google::protobuf::Any>());

  any.set_type_url("/protobuf_unittest.TestAny");
  EXPECT_TRUE(any.Is<protobuf_unittest::TestAny>());
  // The type URL must contain at least one "/".
  any.set_type_url("protobuf_unittest.TestAny");
  EXPECT_FALSE(any.Is<protobuf_unittest::TestAny>());
  // The type name after the slash must be fully qualified.
  any.set_type_url("/TestAny");
  EXPECT_FALSE(any.Is<protobuf_unittest::TestAny>());
}

TEST(AnyTest, MoveConstructor) {
  protobuf_unittest::TestAny payload;
  payload.set_int32_value(12345);

  google::protobuf::Any src;
  src.PackFrom(payload);

  const char* type_url = src.type_url().data();

  google::protobuf::Any dst(std::move(src));
  EXPECT_EQ(type_url, dst.type_url().data());
  payload.Clear();
  ASSERT_TRUE(dst.UnpackTo(&payload));
  EXPECT_EQ(12345, payload.int32_value());
}

TEST(AnyTest, MoveAssignment) {
  protobuf_unittest::TestAny payload;
  payload.set_int32_value(12345);

  google::protobuf::Any src;
  src.PackFrom(payload);

  const char* type_url = src.type_url().data();

  google::protobuf::Any dst;
  dst = std::move(src);
  EXPECT_EQ(type_url, dst.type_url().data());
  payload.Clear();
  ASSERT_TRUE(dst.UnpackTo(&payload));
  EXPECT_EQ(12345, payload.int32_value());
}


}  // namespace
}  // namespace protobuf
}  // namespace google
