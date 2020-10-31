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

#include <google/protobuf/util/type_resolver_util.h>

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/type.pb.h>
#include <google/protobuf/wrappers.pb.h>
#include <google/protobuf/map_unittest.pb.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_custom_options.pb.h>
#include <google/protobuf/util/json_format_proto3.pb.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace util {
namespace {
using google::protobuf::BoolValue;
using google::protobuf::Enum;
using google::protobuf::EnumValue;
using google::protobuf::Field;
using google::protobuf::Int32Value;
using google::protobuf::Option;
using google::protobuf::Type;
using google::protobuf::UInt64Value;

static const char kUrlPrefix[] = "type.googleapis.com";

class DescriptorPoolTypeResolverTest : public testing::Test {
 public:
  DescriptorPoolTypeResolverTest() {
    resolver_.reset(NewTypeResolverForDescriptorPool(
        kUrlPrefix, DescriptorPool::generated_pool()));
  }

  const Field* FindField(const Type& type, const std::string& name) {
    for (int i = 0; i < type.fields_size(); ++i) {
      const Field& field = type.fields(i);
      if (field.name() == name) {
        return &field;
      }
    }
    return NULL;
  }

  bool HasField(const Type& type, Field::Cardinality cardinality,
                Field::Kind kind, const std::string& name, int number) {
    const Field* field = FindField(type, name);
    if (field == NULL) {
      return false;
    }
    return field->cardinality() == cardinality && field->kind() == kind &&
           field->number() == number;
  }

  bool CheckFieldTypeUrl(const Type& type, const std::string& name,
                         const std::string& type_url) {
    const Field* field = FindField(type, name);
    if (field == NULL) {
      return false;
    }
    return field->type_url() == type_url;
  }

  bool FieldInOneof(const Type& type, const std::string& name,
                    const std::string& oneof_name) {
    const Field* field = FindField(type, name);
    if (field == NULL || field->oneof_index() <= 0 ||
        field->oneof_index() > type.oneofs_size()) {
      return false;
    }
    return type.oneofs(field->oneof_index() - 1) == oneof_name;
  }

  bool IsPacked(const Type& type, const std::string& name) {
    const Field* field = FindField(type, name);
    if (field == NULL) {
      return false;
    }
    return field->packed();
  }

  const EnumValue* FindEnumValue(const Enum& type, const std::string& name) {
    for (const EnumValue& value : type.enumvalue()) {
      if (value.name() == name) {
        return &value;
      }
    }
    return nullptr;
  }

  bool EnumHasValue(const Enum& type, const std::string& name, int number) {
    const EnumValue* value = FindEnumValue(type, name);
    return value != nullptr && value->number() == number;
  }

  bool HasBoolOption(const RepeatedPtrField<Option>& options,
                     const std::string& name, bool value) {
    return HasOption<BoolValue>(options, name, value);
  }

  bool HasInt32Option(const RepeatedPtrField<Option>& options,
                      const std::string& name, int32 value) {
    return HasOption<Int32Value>(options, name, value);
  }

  bool HasUInt64Option(const RepeatedPtrField<Option>& options,
                       const std::string& name, uint64 value) {
    return HasOption<UInt64Value>(options, name, value);
  }

  template <typename WrapperT, typename T>
  bool HasOption(const RepeatedPtrField<Option>& options,
                 const std::string& name, T value) {
    for (const Option& option : options) {
      if (option.name() == name) {
        WrapperT wrapper;
        if (option.value().UnpackTo(&wrapper) && wrapper.value() == value) {
          return true;
        }
      }
    }
    return false;
  }

  string GetTypeUrl(std::string full_name) {
    return kUrlPrefix + std::string("/") + full_name;
  }

  template <typename T>
  std::string GetTypeUrl() {
    return GetTypeUrl(T::descriptor()->full_name());
  }

 protected:
  std::unique_ptr<TypeResolver> resolver_;
};

TEST_F(DescriptorPoolTypeResolverTest, TestAllTypes) {
  Type type;
  ASSERT_TRUE(resolver_
                  ->ResolveMessageType(
                      GetTypeUrl<protobuf_unittest::TestAllTypes>(), &type)
                  .ok());
  // Check all optional fields.
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_INT32,
                       "optional_int32", 1));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_INT64,
                       "optional_int64", 2));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_UINT32,
                       "optional_uint32", 3));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_UINT64,
                       "optional_uint64", 4));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_SINT32,
                       "optional_sint32", 5));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_SINT64,
                       "optional_sint64", 6));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_FIXED32,
                       "optional_fixed32", 7));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_FIXED64,
                       "optional_fixed64", 8));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_SFIXED32,
                       "optional_sfixed32", 9));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_SFIXED64,
                       "optional_sfixed64", 10));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_FLOAT,
                       "optional_float", 11));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_DOUBLE,
                       "optional_double", 12));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_BOOL,
                       "optional_bool", 13));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_STRING,
                       "optional_string", 14));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_BYTES,
                       "optional_bytes", 15));

  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_GROUP,
                       "optionalgroup", 16));

  EXPECT_TRUE(CheckFieldTypeUrl(
      type, "optionalgroup",
      GetTypeUrl<protobuf_unittest::TestAllTypes::OptionalGroup>()));

  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_MESSAGE,
                       "optional_nested_message", 18));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_MESSAGE,
                       "optional_foreign_message", 19));

  EXPECT_TRUE(CheckFieldTypeUrl(
      type, "optional_nested_message",
      GetTypeUrl<protobuf_unittest::TestAllTypes::NestedMessage>()));
  EXPECT_TRUE(CheckFieldTypeUrl(type, "optional_foreign_message",
                                GetTypeUrl<protobuf_unittest::ForeignMessage>()));

  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_ENUM,
                       "optional_nested_enum", 21));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_ENUM,
                       "optional_foreign_enum", 22));

  EXPECT_TRUE(
      CheckFieldTypeUrl(type, "optional_nested_enum",
                        GetTypeUrl("protobuf_unittest.TestAllTypes.NestedEnum")));
  EXPECT_TRUE(CheckFieldTypeUrl(type, "optional_foreign_enum",
                                GetTypeUrl("protobuf_unittest.ForeignEnum")));

  // Check all repeated fields.
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_INT32,
                       "repeated_int32", 31));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_INT64,
                       "repeated_int64", 32));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_UINT32,
                       "repeated_uint32", 33));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_UINT64,
                       "repeated_uint64", 34));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_SINT32,
                       "repeated_sint32", 35));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_SINT64,
                       "repeated_sint64", 36));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_FIXED32,
                       "repeated_fixed32", 37));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_FIXED64,
                       "repeated_fixed64", 38));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_SFIXED32,
                       "repeated_sfixed32", 39));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_SFIXED64,
                       "repeated_sfixed64", 40));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_FLOAT,
                       "repeated_float", 41));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_DOUBLE,
                       "repeated_double", 42));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_BOOL,
                       "repeated_bool", 43));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_STRING,
                       "repeated_string", 44));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_BYTES,
                       "repeated_bytes", 45));

  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_GROUP,
                       "repeatedgroup", 46));

  EXPECT_TRUE(CheckFieldTypeUrl(
      type, "repeatedgroup",
      GetTypeUrl<protobuf_unittest::TestAllTypes::RepeatedGroup>()));

  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_MESSAGE,
                       "repeated_nested_message", 48));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_MESSAGE,
                       "repeated_foreign_message", 49));

  EXPECT_TRUE(CheckFieldTypeUrl(
      type, "repeated_nested_message",
      GetTypeUrl<protobuf_unittest::TestAllTypes::NestedMessage>()));
  EXPECT_TRUE(CheckFieldTypeUrl(type, "repeated_foreign_message",
                                GetTypeUrl<protobuf_unittest::ForeignMessage>()));

  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_ENUM,
                       "repeated_nested_enum", 51));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_ENUM,
                       "repeated_foreign_enum", 52));

  EXPECT_TRUE(
      CheckFieldTypeUrl(type, "repeated_nested_enum",
                        GetTypeUrl("protobuf_unittest.TestAllTypes.NestedEnum")));
  EXPECT_TRUE(CheckFieldTypeUrl(type, "repeated_foreign_enum",
                                GetTypeUrl("protobuf_unittest.ForeignEnum")));
}

TEST_F(DescriptorPoolTypeResolverTest, TestPackedField) {
  Type type;
  ASSERT_TRUE(resolver_
                  ->ResolveMessageType(
                      GetTypeUrl<protobuf_unittest::TestPackedTypes>(), &type)
                  .ok());
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_INT32,
                       "packed_int32", 90));
  EXPECT_TRUE(IsPacked(type, "packed_int32"));
}

TEST_F(DescriptorPoolTypeResolverTest, TestOneof) {
  Type type;
  ASSERT_TRUE(resolver_
                  ->ResolveMessageType(
                      GetTypeUrl<protobuf_unittest::TestAllTypes>(), &type)
                  .ok());
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_UINT32,
                       "oneof_uint32", 111));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_MESSAGE,
                       "oneof_nested_message", 112));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_STRING,
                       "oneof_string", 113));
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_OPTIONAL, Field::TYPE_BYTES,
                       "oneof_bytes", 114));
  EXPECT_TRUE(FieldInOneof(type, "oneof_uint32", "oneof_field"));
  EXPECT_TRUE(FieldInOneof(type, "oneof_nested_message", "oneof_field"));
  EXPECT_TRUE(FieldInOneof(type, "oneof_string", "oneof_field"));
  EXPECT_TRUE(FieldInOneof(type, "oneof_bytes", "oneof_field"));
}

TEST_F(DescriptorPoolTypeResolverTest, TestMap) {
  Type type;
  ASSERT_TRUE(
      resolver_
          ->ResolveMessageType(GetTypeUrl<protobuf_unittest::TestMap>(), &type)
          .ok());
  EXPECT_TRUE(HasField(type, Field::CARDINALITY_REPEATED, Field::TYPE_MESSAGE,
                       "map_int32_int32", 1));
  EXPECT_TRUE(CheckFieldTypeUrl(
      type, "map_int32_int32",
      GetTypeUrl("protobuf_unittest.TestMap.MapInt32Int32Entry")));

  ASSERT_TRUE(
      resolver_
          ->ResolveMessageType(
              GetTypeUrl("protobuf_unittest.TestMap.MapInt32Int32Entry"), &type)
          .ok());
  EXPECT_TRUE(HasBoolOption(type.options(), "map_entry", true));
}

TEST_F(DescriptorPoolTypeResolverTest, TestCustomMessageOptions) {
  Type type;
  ASSERT_TRUE(
      resolver_
          ->ResolveMessageType(
              GetTypeUrl<protobuf_unittest::TestMessageWithCustomOptions>(),
              &type)
          .ok());
  EXPECT_TRUE(
      HasInt32Option(type.options(), "protobuf_unittest.message_opt1", -56));
}

TEST_F(DescriptorPoolTypeResolverTest, TestCustomFieldOptions) {
  Type type;
  ASSERT_TRUE(
      resolver_
          ->ResolveMessageType(
              GetTypeUrl<protobuf_unittest::TestMessageWithCustomOptions>(),
              &type)
          .ok());
  const Field* field = FindField(type, "field1");
  ASSERT_TRUE(field != nullptr);
  EXPECT_TRUE(HasUInt64Option(field->options(), "protobuf_unittest.field_opt1",
                              8765432109));
}

TEST_F(DescriptorPoolTypeResolverTest, TestEnum) {
  Enum type;
  ASSERT_TRUE(
      resolver_
          ->ResolveEnumType(
              GetTypeUrl("protobuf_unittest.TestAllTypes.NestedEnum"), &type)
          .ok());
  EnumHasValue(type, "FOO", 1);
  EnumHasValue(type, "BAR", 2);
  EnumHasValue(type, "BAZ", 3);
  EnumHasValue(type, "NEG", -1);
}

TEST_F(DescriptorPoolTypeResolverTest, TestCustomEnumOptions) {
  Enum type;
  ASSERT_TRUE(
      resolver_
          ->ResolveEnumType(
              GetTypeUrl("protobuf_unittest.TestMessageWithCustomOptions.AnEnum"),
              &type)
          .ok());
  ASSERT_TRUE(
      HasInt32Option(type.options(), "protobuf_unittest.enum_opt1", -789));
}

TEST_F(DescriptorPoolTypeResolverTest, TestCustomValueOptions) {
  Enum type;
  ASSERT_TRUE(
      resolver_
          ->ResolveEnumType(
              GetTypeUrl("protobuf_unittest.TestMessageWithCustomOptions.AnEnum"),
              &type)
          .ok());
  const EnumValue* value = FindEnumValue(type, "ANENUM_VAL2");
  ASSERT_TRUE(value != nullptr);
  ASSERT_TRUE(
      HasInt32Option(value->options(), "protobuf_unittest.enum_value_opt1", 123));
}

TEST_F(DescriptorPoolTypeResolverTest, TestJsonName) {
  Type type;
  ASSERT_TRUE(resolver_
                  ->ResolveMessageType(
                      GetTypeUrl<protobuf_unittest::TestAllTypes>(), &type)
                  .ok());
  EXPECT_EQ("optionalInt32", FindField(type, "optional_int32")->json_name());

  ASSERT_TRUE(
      resolver_
          ->ResolveMessageType(GetTypeUrl<proto3::TestCustomJsonName>(), &type)
          .ok());
  EXPECT_EQ("@value", FindField(type, "value")->json_name());
}

}  // namespace
}  // namespace util
}  // namespace protobuf
}  // namespace google
