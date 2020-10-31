/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/metrics/metrics.h"

#include <vector>

#include "protos/perfetto/common/descriptor.pbzero.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace metrics {

namespace {

std::string RunTemplateReplace(
    const std::string& str,
    std::unordered_map<std::string, std::string> subs) {
  std::string out;
  EXPECT_EQ(TemplateReplace(str, subs, &out), 0);
  return out;
}

TEST(MetricsTest, TemplateReplace) {
  auto res = RunTemplateReplace("no templates here", {});
  ASSERT_EQ(res, "no templates here");

  res = RunTemplateReplace("{{justtemplate}}", {{"justtemplate", "result"}});
  ASSERT_EQ(res, "result");

  res = RunTemplateReplace("{{temp1}} {{temp2}}!",
                           {{"temp1", "hello"}, {"temp2", "world"}});
  ASSERT_EQ(res, "hello world!");

  std::string unused;
  ASSERT_NE(TemplateReplace("{{missing}}", {{}}, &unused), 0);
}

class ProtoBuilderTest : public ::testing::Test {
 protected:
  template <bool repeated>
  protozero::TypedProtoDecoder<1, repeated> DecodeSingleFieldProto(
      const std::vector<uint8_t>& result_ser) {
    protos::pbzero::ProtoBuilderResult::Decoder result(result_ser.data(),
                                                       result_ser.size());
    protozero::ConstBytes single_ser = result.single();
    protos::pbzero::SingleBuilderResult::Decoder single(single_ser.data,
                                                        single_ser.size);

    protozero::ConstBytes proto_ser = single.protobuf();
    return protozero::TypedProtoDecoder<1, repeated>(proto_ser.data,
                                                     proto_ser.size);
  }
};

TEST_F(ProtoBuilderTest, AppendLong) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;

  // Create the descriptor version of the following message:
  // message TestProto {
  //   optional int64 int_value = 1;
  // }
  ProtoDescriptor descriptor(".perfetto.protos", ".perfetto.protos.TestProto",
                             ProtoDescriptor::Type::kMessage, base::nullopt);
  descriptor.AddField(FieldDescriptor(
      "int_value", 1, FieldDescriptorProto::TYPE_INT64, "", false));

  ProtoBuilder builder(&descriptor);
  ASSERT_TRUE(builder.AppendLong("int_value", 12345).ok());

  auto result_ser = builder.SerializeToProtoBuilderResult();
  auto proto = DecodeSingleFieldProto<false>(result_ser);
  const protozero::Field& int_field = proto.Get(1);
  ASSERT_EQ(int_field.as_int64(), 12345);
}

TEST_F(ProtoBuilderTest, AppendDouble) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;

  // Create the descriptor version of the following message:
  // message TestProto {
  //   optional double double_value = 1;
  // }
  ProtoDescriptor descriptor(".perfetto.protos", ".perfetto.protos.TestProto",
                             ProtoDescriptor::Type::kMessage, base::nullopt);
  descriptor.AddField(FieldDescriptor(
      "double_value", 1, FieldDescriptorProto::TYPE_DOUBLE, "", false));

  ProtoBuilder builder(&descriptor);
  ASSERT_TRUE(builder.AppendDouble("double_value", 1.2345).ok());

  auto result_ser = builder.SerializeToProtoBuilderResult();
  auto proto = DecodeSingleFieldProto<false>(result_ser);
  const protozero::Field& db_field = proto.Get(1);
  ASSERT_DOUBLE_EQ(db_field.as_double(), 1.2345);
}

TEST_F(ProtoBuilderTest, AppendString) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;

  // Create the descriptor version of the following message:
  // message TestProto {
  //   optional string string_value = 1;
  // }
  ProtoDescriptor descriptor(".perfetto.protos", ".perfetto.protos.TestProto",
                             ProtoDescriptor::Type::kMessage, base::nullopt);
  descriptor.AddField(FieldDescriptor(
      "string_value", 1, FieldDescriptorProto::TYPE_STRING, "", false));

  ProtoBuilder builder(&descriptor);
  ASSERT_TRUE(builder.AppendString("string_value", "hello world!").ok());

  auto result_ser = builder.SerializeToProtoBuilderResult();
  auto proto = DecodeSingleFieldProto<false>(result_ser);
  const protozero::Field& str_field = proto.Get(1);
  ASSERT_EQ(str_field.as_std_string(), "hello world!");
}

TEST_F(ProtoBuilderTest, AppendNested) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;

  // Create the descriptor version of the following message:
  // message TestProto {
  //   message NestedProto {
  //     optional int64 nested_int_value = 1;
  //   }
  //   optional NestedProto nested_value = 1;
  // }
  ProtoDescriptor nested(".perfetto.protos",
                         ".perfetto.protos.TestProto.NestedProto",
                         ProtoDescriptor::Type::kMessage, base::nullopt);
  nested.AddField(FieldDescriptor("nested_int_value", 1,
                                  FieldDescriptorProto::TYPE_INT64, "", false));

  ProtoDescriptor descriptor(".perfetto.protos", ".perfetto.protos.TestProto",
                             ProtoDescriptor::Type::kMessage, base::nullopt);
  auto field =
      FieldDescriptor("nested_value", 1, FieldDescriptorProto::TYPE_MESSAGE,
                      ".perfetto.protos.TestProto.NestedProto", false);
  field.set_resolved_type_name(".perfetto.protos.TestProto.NestedProto");
  descriptor.AddField(field);

  ProtoBuilder nest_builder(&nested);
  ASSERT_TRUE(nest_builder.AppendLong("nested_int_value", 789).ok());

  auto nest_ser = nest_builder.SerializeToProtoBuilderResult();

  ProtoBuilder builder(&descriptor);
  ASSERT_TRUE(
      builder.AppendBytes("nested_value", nest_ser.data(), nest_ser.size())
          .ok());

  auto result_ser = builder.SerializeToProtoBuilderResult();
  auto proto = DecodeSingleFieldProto<false>(result_ser);
  const protozero::Field& nest_field = proto.Get(1);
  ASSERT_EQ(nest_field.type(),
            protozero::proto_utils::ProtoWireType::kLengthDelimited);

  protozero::ConstBytes nest_bytes = nest_field.as_bytes();
  protozero::TypedProtoDecoder<1, false> nest(nest_bytes.data, nest_bytes.size);

  const protozero::Field& nest_int_field = nest.Get(1);
  ASSERT_EQ(nest_int_field.type(),
            protozero::proto_utils::ProtoWireType::kVarInt);
  ASSERT_EQ(nest_int_field.as_int64(), 789);
}

TEST_F(ProtoBuilderTest, AppendRepeatedPrimitive) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;

  // Create the descriptor version of the following message:
  // message TestProto {
  //   repeated int64 int_value = 1;
  // }
  ProtoDescriptor descriptor(".perfetto.protos", ".perfetto.protos.TestProto",
                             ProtoDescriptor::Type::kMessage, base::nullopt);
  descriptor.AddField(FieldDescriptor(
      "rep_int_value", 1, FieldDescriptorProto::TYPE_INT64, "", true));

  RepeatedFieldBuilder rep_builder;
  rep_builder.AddLong(1234);
  rep_builder.AddLong(5678);

  std::vector<uint8_t> rep_ser = rep_builder.SerializeToProtoBuilderResult();

  ProtoBuilder builder(&descriptor);
  ASSERT_TRUE(
      builder.AppendBytes("rep_int_value", rep_ser.data(), rep_ser.size())
          .ok());

  auto result_ser = builder.SerializeToProtoBuilderResult();
  auto proto = DecodeSingleFieldProto<true>(result_ser);
  auto it = proto.GetRepeated<int64_t>(1);
  ASSERT_EQ(*it, 1234);
  ASSERT_EQ(*++it, 5678);
  ASSERT_FALSE(++it);
}

}  // namespace

}  // namespace metrics
}  // namespace trace_processor
}  // namespace perfetto
