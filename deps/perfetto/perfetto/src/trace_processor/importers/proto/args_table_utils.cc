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

#include "protos/perfetto/common/descriptor.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_compositor_scheduler_state.pbzero.h"
#include "protos/perfetto/trace/track_event/source_location.pbzero.h"

namespace perfetto {
namespace trace_processor {

ProtoToArgsTable::ProtoToArgsTable(TraceProcessorContext* context)
    : context_(context) {
  constexpr int kDefaultSize = 64;
  key_prefix_.reserve(kDefaultSize);
  flat_key_prefix_.reserve(kDefaultSize);
}

util::Status ProtoToArgsTable::AddProtoFileDescriptor(
    const uint8_t* proto_descriptor_array,
    size_t proto_descriptor_array_size) {
  return pool_.AddFromFileDescriptorSet(proto_descriptor_array,
                                        proto_descriptor_array_size);
}

util::Status ProtoToArgsTable::InternProtoFieldsIntoArgsTable(
    const protozero::ConstBytes& cb,
    const std::string& type,
    const std::vector<uint16_t>& fields,
    ArgsTracker::BoundInserter* inserter,
    PacketSequenceStateGeneration* sequence_state) {
  auto idx = pool_.FindDescriptorIdx(type);
  if (!idx) {
    return util::Status("Failed to find proto descriptor");
  }

  auto descriptor = pool_.descriptors()[*idx];

  protozero::ProtoDecoder decoder(cb);
  for (protozero::Field f = decoder.ReadField(); f.valid();
       f = decoder.ReadField()) {
    auto it = std::find(fields.begin(), fields.end(), f.id());
    if (it == fields.end()) {
      continue;
    }

    auto field_idx = descriptor.FindFieldIdxByTag(*it);
    if (!field_idx) {
      return util::Status("Failed to find proto descriptor");
    }
    auto field = descriptor.fields()[*field_idx];
    auto status =
        InternProtoIntoArgsTable(f.as_bytes(), field.resolved_type_name(),
                                 inserter, sequence_state, field.name());

    if (!status.ok()) {
      return status;
    }
  }

  return util::OkStatus();
}

util::Status ProtoToArgsTable::InternProtoIntoArgsTable(
    const protozero::ConstBytes& cb,
    const std::string& type,
    ArgsTracker::BoundInserter* inserter,
    PacketSequenceStateGeneration* sequence_state,
    const std::string& key_prefix) {
  key_prefix_.assign(key_prefix);
  flat_key_prefix_.assign(key_prefix);

  ParsingOverrideState state{context_, sequence_state};
  return InternProtoIntoArgsTableInternal(cb, type, inserter, state);
}

util::Status ProtoToArgsTable::InternProtoIntoArgsTableInternal(
    const protozero::ConstBytes& cb,
    const std::string& type,
    ArgsTracker::BoundInserter* inserter,
    ParsingOverrideState state) {
  // Given |type| field the proto descriptor for this proto message.
  auto opt_proto_descriptor_idx = pool_.FindDescriptorIdx(type);
  if (!opt_proto_descriptor_idx) {
    return util::Status("Failed to find proto descriptor");
  }
  auto proto_descriptor = pool_.descriptors()[*opt_proto_descriptor_idx];

  // For repeated fields, contains mapping from field descriptor index to
  // current count of how many fields have been serialized with this field.
  std::unordered_map<size_t, int> repeated_field_index;

  // Parse this message field by field until there are no bytes left.
  protozero::ProtoDecoder decoder(cb.data, cb.size);
  for (auto field = decoder.ReadField(); field.valid();
       field = decoder.ReadField()) {
    auto opt_field_descriptor_idx =
        proto_descriptor.FindFieldIdxByTag(field.id());
    if (!opt_field_descriptor_idx) {
      // Since the binary descriptor is compiled in it is possible we're seeing
      // a new message that our descriptors don't have. Just skip the field.
      continue;
    }
    const auto& field_descriptor =
        proto_descriptor.fields()[*opt_field_descriptor_idx];

    std::string prefix_part = field_descriptor.name();
    if (field_descriptor.is_repeated()) {
      std::string number =
          std::to_string(repeated_field_index[*opt_field_descriptor_idx]);
      prefix_part.reserve(prefix_part.length() + number.length() + 2);
      prefix_part.append("[");
      prefix_part.append(number);
      prefix_part.append("]");
      repeated_field_index[*opt_field_descriptor_idx]++;
    }

    // In the args table we build up message1.message2.field1 as the column
    // name. This will append the ".field1" suffix to |key_prefix| and then
    // remove it when it goes out of scope.
    ScopedStringAppender scoped_prefix(prefix_part, &key_prefix_);
    ScopedStringAppender scoped_flat_key_prefix(field_descriptor.name(),
                                                &flat_key_prefix_);

    // If we have an override parser then use that instead and move onto the
    // next loop.
    auto it = FindOverride(key_prefix_);
    if (it != overrides_.end()) {
      if (it->second(state, field, inserter)) {
        continue;
      }
    }

    // If this is not a message we can just immediately add the column name and
    // get the value out of |field|. However if it is a message we need to
    // recurse into it.
    if (field_descriptor.type() ==
        protos::pbzero::FieldDescriptorProto::TYPE_MESSAGE) {
      auto status = InternProtoIntoArgsTableInternal(
          field.as_bytes(), field_descriptor.resolved_type_name(), inserter,
          state);
      if (!status.ok()) {
        return status;
      }
    } else {
      const StringId key_id =
          state.context->storage->InternString(base::StringView(key_prefix_));
      const StringId flat_key_id = state.context->storage->InternString(
          base::StringView(flat_key_prefix_));
      inserter->AddArg(
          flat_key_id, key_id,
          ConvertProtoTypeToVariadic(field_descriptor, field, state));
    }
  }
  PERFETTO_DCHECK(decoder.bytes_left() == 0);
  return util::OkStatus();
}

void ProtoToArgsTable::AddParsingOverride(std::string field,
                                          ParsingOverride func) {
  overrides_.emplace_back(std::move(field), func);
}

ProtoToArgsTable::OverrideIterator ProtoToArgsTable::FindOverride(
    const std::string& field) {
  return std::find_if(
      overrides_.begin(), overrides_.end(),
      [&field](const std::pair<std::string, ParsingOverride>& overrides) {
        return overrides.first == field;
      });
}

Variadic ProtoToArgsTable::ConvertProtoTypeToVariadic(
    const FieldDescriptor& descriptor,
    const protozero::Field& field,
    ParsingOverrideState state) {
  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  switch (descriptor.type()) {
    case FieldDescriptorProto::TYPE_INT32:
    case FieldDescriptorProto::TYPE_SFIXED32:
    case FieldDescriptorProto::TYPE_FIXED32:
      return Variadic::Integer(field.as_int32());
    case FieldDescriptorProto::TYPE_SINT32:
      return Variadic::Integer(field.as_sint32());
    case FieldDescriptorProto::TYPE_INT64:
    case FieldDescriptorProto::TYPE_SFIXED64:
    case FieldDescriptorProto::TYPE_FIXED64:
      return Variadic::Integer(field.as_int64());
    case FieldDescriptorProto::TYPE_SINT64:
      return Variadic::Integer(field.as_sint64());
    case FieldDescriptorProto::TYPE_UINT32:
      return Variadic::UnsignedInteger(field.as_uint32());
    case FieldDescriptorProto::TYPE_UINT64:
      return Variadic::UnsignedInteger(field.as_uint64());
    case FieldDescriptorProto::TYPE_BOOL:
      return Variadic::Boolean(field.as_bool());
    case FieldDescriptorProto::TYPE_DOUBLE:
      return Variadic::Real(field.as_double());
    case FieldDescriptorProto::TYPE_FLOAT:
      return Variadic::Real(static_cast<double>(field.as_float()));
    case FieldDescriptorProto::TYPE_STRING:
      return Variadic::String(
          state.context->storage->InternString(field.as_string()));
    case FieldDescriptorProto::TYPE_ENUM: {
      auto opt_enum_descriptor_idx =
          pool_.FindDescriptorIdx(descriptor.resolved_type_name());
      if (!opt_enum_descriptor_idx) {
        // Fall back to the integer representation of the field.
        return Variadic::Integer(field.as_int32());
      }
      auto opt_enum_string =
          pool_.descriptors()[*opt_enum_descriptor_idx].FindEnumString(
              field.as_int32());
      if (!opt_enum_string) {
        // Fall back to the integer representation of the field.
        return Variadic::Integer(field.as_int32());
      }
      return Variadic::String(state.context->storage->InternString(
          base::StringView(*opt_enum_string)));
    }
    default: {
      PERFETTO_FATAL(
          "Tried to write value of type field %s (in proto type "
          "%s) which has type enum %d",
          descriptor.name().c_str(), descriptor.resolved_type_name().c_str(),
          descriptor.type());
    }
  }
  return Variadic{};
}

ProtoToArgsTable::ScopedStringAppender::ScopedStringAppender(
    const std::string& append,
    std::string* dest)
    : old_size_(dest->size()), str_(dest) {
  if (dest->empty()) {
    str_->reserve(append.size());
  } else {
    str_->reserve(old_size_ + 1 + append.size());
    str_->append(".");
  }
  str_->append(append);
}

ProtoToArgsTable::ScopedStringAppender::~ScopedStringAppender() {
  str_->erase(old_size_);
}

}  // namespace trace_processor
}  // namespace perfetto
