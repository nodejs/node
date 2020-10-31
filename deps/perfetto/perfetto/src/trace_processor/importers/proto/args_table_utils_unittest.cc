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

#include "src/trace_processor/importers/proto/args_table_utils.h"

#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "protos/perfetto/common/descriptor.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "src/protozero/test/example_proto/test_messages.descriptor.h"
#include "src/protozero/test/example_proto/test_messages.pbzero.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

constexpr size_t kChunkSize = 42;

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NiceMock;

class ArgsTableUtilsTest : public ::testing::Test {
 protected:
  ArgsTableUtilsTest() {
    context_.storage.reset(new TraceStorage);
    storage_ = context_.storage.get();
    context_.global_args_tracker.reset(new GlobalArgsTracker(&context_));
    context_.args_tracker.reset(new ArgsTracker(&context_));
    sequence_state_.reset(new PacketSequenceState(&context_));
  }

  /**
   * Check whether the argument set contains the value with given flat_key and
   * key and is equal to the given value.
   */
  bool HasArg(ArgSetId set_id,
              const base::StringView& flat_key,
              const base::StringView& key,
              Variadic value) {
    const auto& args = storage_->arg_table();
    auto key_id = storage_->string_pool().GetId(key);
    EXPECT_TRUE(key_id);
    auto flat_key_id = storage_->string_pool().GetId(flat_key);
    EXPECT_TRUE(flat_key_id);

    RowMap rm = args.FilterToRowMap({args.arg_set_id().eq(set_id)});
    bool found = false;
    for (auto it = rm.IterateRows(); it; it.Next()) {
      if (args.key()[it.row()] == key_id) {
        EXPECT_EQ(args.flat_key()[it.row()], flat_key_id);
        if (storage_->GetArgValue(it.row()) == value) {
          found = true;
          break;
        }
      }
    }
    return found;
  }

  /**
   * Implementation of HasArg for a simple case when flat_key is equals to key,
   * so that two won't have to be repeated for each assertion.
   */
  bool HasArg(ArgSetId set_id, const base::StringView& key, Variadic value) {
    return HasArg(set_id, key, key, value);
  }

  uint32_t arg_set_id_ = 1;
  std::unique_ptr<PacketSequenceState> sequence_state_;
  TraceProcessorContext context_;
  TraceStorage* storage_;
};

TEST_F(ArgsTableUtilsTest, EnsureTestMessageProtoParses) {
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  EXPECT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();
}

TEST_F(ArgsTableUtilsTest, BasicSingleLayerProto) {
  using namespace protozero::test::protos::pbzero;
  protozero::HeapBuffered<EveryField> msg{kChunkSize, kChunkSize};
  msg->set_field_int32(-1);
  msg->set_field_int64(-333123456789ll);
  msg->set_field_uint32(600);
  msg->set_field_uint64(333123456789ll);
  msg->set_field_sint32(-5);
  msg->set_field_sint64(-9000);
  msg->set_field_fixed32(12345);
  msg->set_field_fixed64(444123450000ll);
  msg->set_field_sfixed32(-69999);
  msg->set_field_sfixed64(-200);
  msg->set_field_double(0.5555);
  msg->set_field_bool(true);
  msg->set_small_enum(SmallEnum::TO_BE);
  msg->set_signed_enum(SignedEnum::NEGATIVE);
  msg->set_big_enum(BigEnum::BEGIN);
  msg->set_nested_enum(EveryField::PONG);
  msg->set_field_float(3.14f);
  msg->set_field_string("FizzBuzz");
  msg->add_repeated_int32(1);
  msg->add_repeated_int32(-1);
  msg->add_repeated_int32(100);
  msg->add_repeated_int32(2000000);

  auto binary_proto = msg.SerializeAsArray();

  storage_->mutable_track_table()->Insert({});
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.EveryField", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "");

  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();

  context_.args_tracker->Flush();
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_int32", Variadic::Integer(-1)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "field_int64",
                     Variadic::Integer(-333123456789ll)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "field_uint32",
                     Variadic::UnsignedInteger(600)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "field_uint64",
                     Variadic::UnsignedInteger(333123456789ll)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_sint32", Variadic::Integer(-5)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_sint64", Variadic::Integer(-9000)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_fixed32", Variadic::Integer(12345)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "field_fixed64",
                     Variadic::Integer(444123450000ll)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "field_sfixed32",
                     Variadic::Integer(-69999)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_sfixed64", Variadic::Integer(-200)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_double", Variadic::Real(0.5555)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "field_bool", Variadic::Boolean(true)));
  EXPECT_TRUE(HasArg(
      ArgSetId(arg_set_id_), "small_enum",
      Variadic::String(*context_.storage->string_pool().GetId("TO_BE"))));
  EXPECT_TRUE(HasArg(
      ArgSetId(arg_set_id_), "signed_enum",
      Variadic::String(*context_.storage->string_pool().GetId("NEGATIVE"))));
  EXPECT_TRUE(HasArg(
      ArgSetId(arg_set_id_), "big_enum",
      Variadic::String(*context_.storage->string_pool().GetId("BEGIN"))));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "nested_enum",
             Variadic::String(*context_.storage->string_pool().GetId("PONG"))));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "field_float",
                     Variadic::Real(static_cast<double>(3.14f))));
  ASSERT_TRUE(context_.storage->string_pool().GetId("FizzBuzz"));
  EXPECT_TRUE(HasArg(
      ArgSetId(arg_set_id_), "field_string",
      Variadic::String(*context_.storage->string_pool().GetId("FizzBuzz"))));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "repeated_int32",
                     "repeated_int32[0]", Variadic::Integer(1)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "repeated_int32",
                     "repeated_int32[1]", Variadic::Integer(-1)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "repeated_int32",
                     "repeated_int32[2]", Variadic::Integer(100)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "repeated_int32",
                     "repeated_int32[3]", Variadic::Integer(2000000)));
}

TEST_F(ArgsTableUtilsTest, NestedProto) {
  using namespace protozero::test::protos::pbzero;
  protozero::HeapBuffered<NestedA> msg{kChunkSize, kChunkSize};
  msg->set_super_nested()->set_value_c(3);

  auto binary_proto = msg.SerializeAsArray();

  storage_->mutable_track_table()->Insert({});
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.NestedA", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "");
  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();
  context_.args_tracker->Flush();
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "super_nested.value_c",
                     Variadic::Integer(3)));
}

TEST_F(ArgsTableUtilsTest, CamelCaseFieldsProto) {
  using namespace protozero::test::protos::pbzero;
  protozero::HeapBuffered<CamelCaseFields> msg{kChunkSize, kChunkSize};
  msg->set_barbaz(true);
  msg->set_moomoo(true);
  msg->set___bigbang(true);

  auto binary_proto = msg.SerializeAsArray();

  storage_->mutable_track_table()->Insert({});
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.CamelCaseFields", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "");
  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();
  context_.args_tracker->Flush();
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "barBaz", Variadic::Boolean(true)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "MooMoo", Variadic::Boolean(true)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "__bigBang", Variadic::Boolean(true)));
}

TEST_F(ArgsTableUtilsTest, NestedProtoParsingOverrideHandled) {
  using namespace protozero::test::protos::pbzero;
  protozero::HeapBuffered<NestedA> msg{kChunkSize, kChunkSize};
  msg->set_super_nested()->set_value_c(3);

  auto binary_proto = msg.SerializeAsArray();

  storage_->mutable_track_table()->Insert({});
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  helper.AddParsingOverride(
      "super_nested.value_c",
      [](const ProtoToArgsTable::ParsingOverrideState& state,
         const protozero::Field& field, ArgsTracker::BoundInserter* inserter) {
        EXPECT_EQ(field.type(), protozero::proto_utils::ProtoWireType::kVarInt);
        EXPECT_TRUE(state.context);
        EXPECT_TRUE(state.sequence_state);
        auto id = state.context->storage->InternString(
            "super_nested.value_b.replaced");
        int32_t val = field.as_int32();
        inserter->AddArg(id, id, Variadic::Integer(val + 1));
        // We've handled this field by adding the desired args.
        return true;
      });

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.NestedA", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "");
  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();
  context_.args_tracker->Flush();
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "super_nested.value_b.replaced",
                     Variadic::Integer(4)));
}

TEST_F(ArgsTableUtilsTest, NestedProtoParsingOverrideSkipped) {
  using namespace protozero::test::protos::pbzero;
  protozero::HeapBuffered<NestedA> msg{kChunkSize, kChunkSize};
  msg->set_super_nested()->set_value_c(3);

  auto binary_proto = msg.SerializeAsArray();

  storage_->mutable_track_table()->Insert({});
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  helper.AddParsingOverride(
      "super_nested.value_c",
      [](const ProtoToArgsTable::ParsingOverrideState& state,
         const protozero::Field& field, ArgsTracker::BoundInserter*) {
        static int val = 0;
        ++val;
        EXPECT_EQ(1, val);
        EXPECT_EQ(field.type(), protozero::proto_utils::ProtoWireType::kVarInt);
        EXPECT_TRUE(state.sequence_state);
        EXPECT_TRUE(state.context);
        // By returning false we expect this field to be handled like regular.
        return false;
      });

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.NestedA", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "");
  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();
  context_.args_tracker->Flush();
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "super_nested.value_c",
                     Variadic::Integer(3)));
}

TEST_F(ArgsTableUtilsTest, LookingUpInternedStateParsingOverride) {
  using namespace protozero::test::protos::pbzero;
  // The test proto, we will use |value_c| as the source_location iid.
  protozero::HeapBuffered<NestedA> msg{kChunkSize, kChunkSize};
  msg->set_super_nested()->set_value_c(3);
  auto binary_proto = msg.SerializeAsArray();

  // The interned source location.
  protozero::HeapBuffered<protos::pbzero::SourceLocation> src_loc{kChunkSize,
                                                                  kChunkSize};
  src_loc->set_iid(3);
  src_loc->set_file_name("test_file_name");
  // We need to update sequence_state to point to it.
  auto binary_data = src_loc.SerializeAsArray();
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[binary_data.size()]);
  for (size_t i = 0; i < binary_data.size(); ++i) {
    buffer.get()[i] = binary_data[i];
  }
  TraceBlobView blob(std::move(buffer), 0, binary_data.size());
  sequence_state_->InternMessage(
      protos::pbzero::InternedData::kSourceLocationsFieldNumber,
      std::move(blob));

  ProtoToArgsTable helper(&context_);
  // Now we override the behaviour of |value_c| so we can expand the iid into
  // multiple args rows.
  helper.AddParsingOverride(
      "super_nested.value_c",
      [](const ProtoToArgsTable::ParsingOverrideState& state,
         const protozero::Field& field, ArgsTracker::BoundInserter* inserter) {
        EXPECT_EQ(field.type(), protozero::proto_utils::ProtoWireType::kVarInt);
        auto* decoder = state.sequence_state->LookupInternedMessage<
            protos::pbzero::InternedData::kSourceLocationsFieldNumber,
            protos::pbzero::SourceLocation>(field.as_uint64());
        if (!decoder) {
          // Lookup failed fall back on default behaviour.
          return false;
        }
        TraceStorage* storage = state.context->storage.get();
        auto file_name_id = storage->InternString("file_name");
        auto line_number_id = storage->InternString("line_number");
        auto file_id = storage->InternString(decoder->file_name());
        inserter->AddArg(file_name_id, file_name_id, Variadic::String(file_id));
        inserter->AddArg(line_number_id, line_number_id, Variadic::Integer(2));
        return true;
      });

  storage_->mutable_track_table()->Insert({});
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.NestedA", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "");
  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();
  auto file_name_id = storage_->string_pool().GetId("test_file_name");
  ASSERT_TRUE(file_name_id);
  context_.args_tracker->Flush();
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "file_name",
                     Variadic::String(*file_name_id)));
  EXPECT_TRUE(
      HasArg(ArgSetId(arg_set_id_), "line_number", Variadic::Integer(2)));
}

TEST_F(ArgsTableUtilsTest, NonEmptyPrefix) {
  using namespace protozero::test::protos::pbzero;
  protozero::HeapBuffered<EveryField> msg{kChunkSize, kChunkSize};
  msg->add_repeated_int32(1);
  msg->add_repeated_int32(-1);
  msg->add_repeated_int32(100);
  msg->add_repeated_int32(2000000);

  auto binary_proto = msg.SerializeAsArray();

  storage_->mutable_track_table()->Insert({});
  ProtoToArgsTable helper(&context_);
  auto status = helper.AddProtoFileDescriptor(kTestMessagesDescriptor.data(),
                                              kTestMessagesDescriptor.size());
  ASSERT_TRUE(status.ok()) << "Failed to parse kTestMessagesDescriptor: "
                           << status.message();

  auto inserter = context_.args_tracker->AddArgsTo(TrackId(0));
  status = helper.InternProtoIntoArgsTable(
      protozero::ConstBytes{binary_proto.data(), binary_proto.size()},
      ".protozero.test.protos.EveryField", &inserter,
      sequence_state_->current_generation(), /* key_prefix= */ "prefix");

  EXPECT_TRUE(status.ok()) << "InternProtoIntoArgsTable failed with error: "
                           << status.message();

  context_.args_tracker->Flush();
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "prefix.repeated_int32",
                     "prefix.repeated_int32[0]", Variadic::Integer(1)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "prefix.repeated_int32",
                     "prefix.repeated_int32[1]", Variadic::Integer(-1)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "prefix.repeated_int32",
                     "prefix.repeated_int32[2]", Variadic::Integer(100)));
  EXPECT_TRUE(HasArg(ArgSetId(arg_set_id_), "prefix.repeated_int32",
                     "prefix.repeated_int32[3]", Variadic::Integer(2000000)));
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
