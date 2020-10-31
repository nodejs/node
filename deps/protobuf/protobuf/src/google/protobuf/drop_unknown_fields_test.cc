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

#include <memory>

#include <google/protobuf/unittest_drop_unknown_fields.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message_lite.h>
#include <gtest/gtest.h>

using unittest_drop_unknown_fields::Foo;
using unittest_drop_unknown_fields::FooWithExtraFields;

namespace google {
namespace protobuf {

TEST(DropUnknownFieldsTest, GeneratedMessage) {
  FooWithExtraFields foo_with_extra_fields;
  foo_with_extra_fields.set_int32_value(1);
  foo_with_extra_fields.set_enum_value(FooWithExtraFields::QUX);
  foo_with_extra_fields.set_extra_int32_value(2);

  Foo foo;
  ASSERT_TRUE(foo.ParseFromString(foo_with_extra_fields.SerializeAsString()));
  EXPECT_EQ(1, foo.int32_value());
  EXPECT_EQ(static_cast<int>(FooWithExtraFields::QUX),
            static_cast<int>(foo.enum_value()));
  EXPECT_FALSE(foo.GetReflection()->GetUnknownFields(foo).empty());

  ASSERT_TRUE(foo_with_extra_fields.ParseFromString(foo.SerializeAsString()));
  EXPECT_EQ(1, foo_with_extra_fields.int32_value());
  EXPECT_EQ(FooWithExtraFields::QUX, foo_with_extra_fields.enum_value());
  // The "extra_int32_value" field should not be lost.
  EXPECT_EQ(2, foo_with_extra_fields.extra_int32_value());
}

TEST(DropUnknownFieldsTest, DynamicMessage) {
  FooWithExtraFields foo_with_extra_fields;
  foo_with_extra_fields.set_int32_value(1);
  foo_with_extra_fields.set_enum_value(FooWithExtraFields::QUX);
  foo_with_extra_fields.set_extra_int32_value(2);

  DynamicMessageFactory factory;
  std::unique_ptr<Message> foo(factory.GetPrototype(Foo::descriptor())->New());
  ASSERT_TRUE(foo->ParseFromString(foo_with_extra_fields.SerializeAsString()));
  EXPECT_FALSE(foo->GetReflection()->GetUnknownFields(*foo).empty());

  ASSERT_TRUE(foo_with_extra_fields.ParseFromString(foo->SerializeAsString()));
  EXPECT_EQ(1, foo_with_extra_fields.int32_value());
  EXPECT_EQ(FooWithExtraFields::QUX, foo_with_extra_fields.enum_value());
  // The "extra_int32_value" field should not be lost.
  EXPECT_EQ(2, foo_with_extra_fields.extra_int32_value());
}

}  // namespace protobuf
}  // namespace google
