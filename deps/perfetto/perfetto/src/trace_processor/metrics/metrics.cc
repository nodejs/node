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

#include <regex>
#include <unordered_map>
#include <vector>

#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/trace_processor/metrics/sql_metrics.h"

#include "protos/perfetto/common/descriptor.pbzero.h"
#include "protos/perfetto/trace_processor/metrics_impl.pbzero.h"

namespace perfetto {
namespace trace_processor {
namespace metrics {

namespace {

// TODO(lalitm): delete this and use sqlite_utils when that is cleaned up of
// trace processor dependencies.
const char* ExtractSqliteValue(sqlite3_value* value) {
  auto type = sqlite3_value_type(value);
  PERFETTO_DCHECK(type == SQLITE_TEXT);
  return reinterpret_cast<const char*>(sqlite3_value_text(value));
}

SqlValue SqlValueFromSqliteValue(sqlite3_value* value) {
  SqlValue sql_value;
  switch (sqlite3_value_type(value)) {
    case SQLITE_INTEGER:
      sql_value.type = SqlValue::Type::kLong;
      sql_value.long_value = sqlite3_value_int64(value);
      break;
    case SQLITE_FLOAT:
      sql_value.type = SqlValue::Type::kDouble;
      sql_value.double_value = sqlite3_value_double(value);
      break;
    case SQLITE_TEXT:
      sql_value.type = SqlValue::Type::kString;
      sql_value.string_value =
          reinterpret_cast<const char*>(sqlite3_value_text(value));
      break;
    case SQLITE_BLOB:
      sql_value.type = SqlValue::Type::kBytes;
      sql_value.bytes_value = sqlite3_value_blob(value);
      sql_value.bytes_count = static_cast<size_t>(sqlite3_value_bytes(value));
      break;
  }
  return sql_value;
}

}  // namespace

ProtoBuilder::ProtoBuilder(const ProtoDescriptor* descriptor)
    : descriptor_(descriptor) {}

util::Status ProtoBuilder::AppendSqlValue(const std::string& field_name,
                                          const SqlValue& value) {
  switch (value.type) {
    case SqlValue::kLong:
      return AppendLong(field_name, value.long_value);
    case SqlValue::kDouble:
      return AppendDouble(field_name, value.double_value);
    case SqlValue::kString:
      return AppendString(field_name, value.string_value);
    case SqlValue::kBytes:
      return AppendBytes(field_name,
                         static_cast<const uint8_t*>(value.bytes_value),
                         value.bytes_count);
    case SqlValue::kNull:
      // If the value is null, it's treated as the field being absent so we
      // don't append anything.
      return util::OkStatus();
  }
  PERFETTO_FATAL("For GCC");
}

util::Status ProtoBuilder::AppendLong(const std::string& field_name,
                                      int64_t value,
                                      bool is_inside_repeated) {
  auto field_idx = descriptor_->FindFieldIdxByName(field_name);
  if (!field_idx.has_value()) {
    return util::ErrStatus("Field with name %s not found in proto type %s",
                           field_name.c_str(),
                           descriptor_->full_name().c_str());
  }

  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  const auto& field = descriptor_->fields()[field_idx.value()];
  if (field.is_repeated() && !is_inside_repeated) {
    return util::ErrStatus(
        "Unexpected long value for repeated field %s in proto type %s",
        field_name.c_str(), descriptor_->full_name().c_str());
  }

  switch (field.type()) {
    case FieldDescriptorProto::TYPE_INT32:
    case FieldDescriptorProto::TYPE_INT64:
    case FieldDescriptorProto::TYPE_UINT32:
    case FieldDescriptorProto::TYPE_BOOL:
      message_->AppendVarInt(field.number(), value);
      break;
    case FieldDescriptorProto::TYPE_SINT32:
    case FieldDescriptorProto::TYPE_SINT64:
      message_->AppendSignedVarInt(field.number(), value);
      break;
    case FieldDescriptorProto::TYPE_FIXED32:
    case FieldDescriptorProto::TYPE_SFIXED32:
    case FieldDescriptorProto::TYPE_FIXED64:
    case FieldDescriptorProto::TYPE_SFIXED64:
      message_->AppendFixed(field.number(), value);
      break;
    default: {
      return util::ErrStatus(
          "Tried to write value of type long into field %s (in proto type %s) "
          "which has type %d",
          field.name().c_str(), descriptor_->full_name().c_str(), field.type());
    }
  }
  return util::OkStatus();
}

util::Status ProtoBuilder::AppendDouble(const std::string& field_name,
                                        double value,
                                        bool is_inside_repeated) {
  auto field_idx = descriptor_->FindFieldIdxByName(field_name);
  if (!field_idx.has_value()) {
    return util::ErrStatus("Field with name %s not found in proto type %s",
                           field_name.c_str(),
                           descriptor_->full_name().c_str());
  }

  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  const auto& field = descriptor_->fields()[field_idx.value()];
  if (field.is_repeated() && !is_inside_repeated) {
    return util::ErrStatus(
        "Unexpected double value for repeated field %s in proto type %s",
        field_name.c_str(), descriptor_->full_name().c_str());
  }

  switch (field.type()) {
    case FieldDescriptorProto::TYPE_FLOAT:
    case FieldDescriptorProto::TYPE_DOUBLE: {
      if (field.type() == FieldDescriptorProto::TYPE_FLOAT) {
        message_->AppendFixed(field.number(), static_cast<float>(value));
      } else {
        message_->AppendFixed(field.number(), value);
      }
      break;
    }
    default: {
      return util::ErrStatus(
          "Tried to write value of type double into field %s (in proto type "
          "%s) which has type %d",
          field.name().c_str(), descriptor_->full_name().c_str(), field.type());
    }
  }
  return util::OkStatus();
}

util::Status ProtoBuilder::AppendString(const std::string& field_name,
                                        base::StringView data,
                                        bool is_inside_repeated) {
  auto field_idx = descriptor_->FindFieldIdxByName(field_name);
  if (!field_idx.has_value()) {
    return util::ErrStatus("Field with name %s not found in proto type %s",
                           field_name.c_str(),
                           descriptor_->full_name().c_str());
  }

  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  const auto& field = descriptor_->fields()[field_idx.value()];
  if (field.is_repeated() && !is_inside_repeated) {
    return util::ErrStatus(
        "Unexpected string value for repeated field %s in proto type %s",
        field_name.c_str(), descriptor_->full_name().c_str());
  }

  switch (field.type()) {
    case FieldDescriptorProto::TYPE_STRING: {
      message_->AppendBytes(field.number(), data.data(), data.size());
      break;
    }
    default: {
      return util::ErrStatus(
          "Tried to write value of type string into field %s (in proto type "
          "%s) which has type %d",
          field.name().c_str(), descriptor_->full_name().c_str(), field.type());
    }
  }
  return util::OkStatus();
}

util::Status ProtoBuilder::AppendBytes(const std::string& field_name,
                                       const uint8_t* ptr,
                                       size_t size,
                                       bool is_inside_repeated) {
  auto field_idx = descriptor_->FindFieldIdxByName(field_name);
  if (!field_idx.has_value()) {
    return util::ErrStatus("Field with name %s not found in proto type %s",
                           field_name.c_str(),
                           descriptor_->full_name().c_str());
  }

  using FieldDescriptorProto = protos::pbzero::FieldDescriptorProto;
  const auto& field = descriptor_->fields()[field_idx.value()];
  if (field.is_repeated() && !is_inside_repeated)
    return AppendRepeated(field, ptr, size);

  if (field.type() == FieldDescriptorProto::TYPE_MESSAGE)
    return AppendSingleMessage(field, ptr, size);

  if (size == 0) {
    return util::ErrStatus(
        "Tried to write null value into field %s (in proto type %s). "
        "Nulls are only supported for message protos; all other types should"
        "ensure that nulls are not passed to proto builder functions by using"
        "the SQLite IFNULL/COALESCE functions.",
        field.name().c_str(), descriptor_->full_name().c_str());
  }

  return util::ErrStatus(
      "Tried to write value of type bytes into field %s (in proto type %s) "
      "which has type %d",
      field.name().c_str(), descriptor_->full_name().c_str(), field.type());
}

util::Status ProtoBuilder::AppendSingleMessage(const FieldDescriptor& field,
                                               const uint8_t* ptr,
                                               size_t size) {
  if (size == 0) {
    // If we have an zero sized bytes, we still want to propogate that it the
    // message was set but empty. Just set the field with that id to an
    // empty bytes.
    message_->AppendBytes(field.number(), ptr, size);
    return util::OkStatus();
  }

  protos::pbzero::ProtoBuilderResult::Decoder decoder(ptr, size);
  if (decoder.is_repeated()) {
    return util::ErrStatus("Cannot handle nested repeated messages in field %s",
                           field.name().c_str());
  }

  const auto& single_field = decoder.single();
  protos::pbzero::SingleBuilderResult::Decoder single(single_field.data,
                                                      single_field.size);

  if (single.type() != field.type()) {
    return util::ErrStatus("Field %s has wrong type (expected %u, was %u)",
                           field.name().c_str(), field.type(), single.type());
  }

  auto actual_type_name = single.type_name().ToStdString();
  if (actual_type_name != field.resolved_type_name()) {
    return util::ErrStatus("Field %s has wrong type (expected %s, was %s)",
                           field.name().c_str(), actual_type_name.c_str(),
                           field.resolved_type_name().c_str());
  }

  if (!single.has_protobuf()) {
    return util::ErrStatus("Field %s has no proto bytes", field.name().c_str());
  }

  // We disallow 0 size fields here as they should have been reported as null
  // one layer down.
  auto bytes = single.protobuf();
  if (bytes.size == 0) {
    return util::ErrStatus("Unexpected to see field %s with zero size",
                           field.name().c_str());
  }

  message_->AppendBytes(field.number(), bytes.data, bytes.size);
  return util::OkStatus();
}

util::Status ProtoBuilder::AppendRepeated(const FieldDescriptor& field,
                                          const uint8_t* ptr,
                                          size_t size) {
  protos::pbzero::ProtoBuilderResult::Decoder decoder(ptr, size);
  if (!decoder.is_repeated()) {
    return util::ErrStatus(
        "Unexpected message value for repeated field %s in proto type %s",
        field.name().c_str(), descriptor_->full_name().c_str());
  }

  const auto& rep = decoder.repeated();
  protos::pbzero::RepeatedBuilderResult::Decoder repeated(rep.data, rep.size);

  for (auto it = repeated.value(); it; ++it) {
    protos::pbzero::RepeatedBuilderResult::Value::Decoder value(*it);
    util::Status status;
    if (value.has_int_value()) {
      status = AppendLong(field.name(), value.int_value(), true);
    } else if (value.has_double_value()) {
      status = AppendDouble(field.name(), value.double_value(), true);
    } else if (value.has_string_value()) {
      status = AppendString(field.name(),
                            base::StringView(value.string_value()), true);
    } else if (value.has_bytes_value()) {
      const auto& bytes = value.bytes_value();
      status = AppendBytes(field.name(), bytes.data, bytes.size, true);
    } else {
      status = util::ErrStatus("Unknown type in repeated field");
    }

    if (!status.ok())
      return status;
  }
  return util::OkStatus();
}

std::vector<uint8_t> ProtoBuilder::SerializeToProtoBuilderResult() {
  std::vector<uint8_t> serialized = SerializeRaw();
  if (serialized.empty())
    return serialized;

  const auto& type_name = descriptor_->full_name();

  protozero::HeapBuffered<protos::pbzero::ProtoBuilderResult> result;
  result->set_is_repeated(false);

  auto* single = result->set_single();
  single->set_type(protos::pbzero::FieldDescriptorProto_Type_TYPE_MESSAGE);
  single->set_type_name(type_name.c_str(), type_name.size());
  single->set_protobuf(serialized.data(), serialized.size());
  return result.SerializeAsArray();
}

std::vector<uint8_t> ProtoBuilder::SerializeRaw() {
  return message_.SerializeAsArray();
}

RepeatedFieldBuilder::RepeatedFieldBuilder() {
  repeated_ = message_->set_repeated();
}

util::Status RepeatedFieldBuilder::AddSqlValue(SqlValue value) {
  switch (value.type) {
    case SqlValue::kLong:
      AddLong(value.long_value);
      break;
    case SqlValue::kDouble:
      AddDouble(value.double_value);
      break;
    case SqlValue::kString:
      AddString(value.string_value);
      break;
    case SqlValue::kBytes:
      AddBytes(static_cast<const uint8_t*>(value.bytes_value),
               value.bytes_count);
      break;
    case SqlValue::kNull:
      AddBytes(nullptr, 0);
      break;
  }
  return util::OkStatus();
}

void RepeatedFieldBuilder::AddLong(int64_t value) {
  has_data_ = true;
  repeated_->add_value()->set_int_value(value);
}

void RepeatedFieldBuilder::AddDouble(double value) {
  has_data_ = true;
  repeated_->add_value()->set_double_value(value);
}

void RepeatedFieldBuilder::AddString(base::StringView value) {
  has_data_ = true;
  repeated_->add_value()->set_string_value(value.data(), value.size());
}

void RepeatedFieldBuilder::AddBytes(const uint8_t* data, size_t size) {
  has_data_ = true;
  repeated_->add_value()->set_bytes_value(data, size);
}

std::vector<uint8_t> RepeatedFieldBuilder::SerializeToProtoBuilderResult() {
  repeated_ = nullptr;
  if (!has_data_)
    return std::vector<uint8_t>();

  message_->set_is_repeated(true);
  return message_.SerializeAsArray();
}

int TemplateReplace(
    const std::string& raw_text,
    const std::unordered_map<std::string, std::string>& substitutions,
    std::string* out) {
  std::regex re(R"(\{\{\s*(\w*)\s*\}\})", std::regex_constants::ECMAScript);

  auto it = std::sregex_iterator(raw_text.begin(), raw_text.end(), re);
  auto regex_end = std::sregex_iterator();
  auto start = raw_text.begin();
  for (; it != regex_end; ++it) {
    out->insert(out->end(), start, raw_text.begin() + it->position(0));

    auto value_it = substitutions.find(it->str(1));
    if (value_it == substitutions.end())
      return 1;

    const auto& value = value_it->second;
    std::copy(value.begin(), value.end(), std::back_inserter(*out));
    start = raw_text.begin() + it->position(0) + it->length(0);
  }
  out->insert(out->end(), start, raw_text.end());
  return 0;
}

void RepeatedFieldStep(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  if (argc != 1) {
    sqlite3_result_error(ctx, "RepeatedField: only expected one arg", -1);
    return;
  }

  // We use a double indirection here so we can use new and delete without
  // needing to do dangerous dances with placement new and checking
  // initalization.
  auto** builder_ptr_ptr = static_cast<RepeatedFieldBuilder**>(
      sqlite3_aggregate_context(ctx, sizeof(RepeatedFieldBuilder*)));

  // The memory returned from sqlite3_aggregate_context is zeroed on its first
  // invocation so *builder_ptr_ptr will be nullptr on the first invocation of
  // RepeatedFieldStep.
  bool needs_init = *builder_ptr_ptr == nullptr;
  if (needs_init) {
    *builder_ptr_ptr = new RepeatedFieldBuilder();
  }

  auto value = SqlValueFromSqliteValue(argv[0]);
  RepeatedFieldBuilder* builder = *builder_ptr_ptr;
  auto status = builder->AddSqlValue(value);
  if (!status.ok()) {
    sqlite3_result_error(ctx, status.c_message(), -1);
  }
}

void RepeatedFieldFinal(sqlite3_context* ctx) {
  // Note: we choose the size intentionally to be zero because we don't want to
  // allocate if the Step has never been called.
  auto** builder_ptr_ptr =
      static_cast<RepeatedFieldBuilder**>(sqlite3_aggregate_context(ctx, 0));

  // If Step has never been called, |builder_ptr_ptr| will be null.
  if (builder_ptr_ptr == nullptr) {
    sqlite3_result_null(ctx);
    return;
  }

  // Capture the context pointer so that it will be freed at the end of this
  // function.
  std::unique_ptr<RepeatedFieldBuilder> builder(*builder_ptr_ptr);
  std::vector<uint8_t> raw = builder->SerializeToProtoBuilderResult();
  if (raw.empty()) {
    sqlite3_result_null(ctx);
    return;
  }

  std::unique_ptr<uint8_t[], base::FreeDeleter> data(
      static_cast<uint8_t*>(malloc(raw.size())));
  memcpy(data.get(), raw.data(), raw.size());
  sqlite3_result_blob(ctx, data.release(), static_cast<int>(raw.size()), free);
}

// SQLite function implementation used to build a proto directly in SQL. The
// proto to be built is given by the descriptor which is given as a context
// parameter to this function and chosen when this function is first registed
// with SQLite. The args of this function are key value pairs specifying the
// name of the field and its value. Nested messages are expected to be passed
// as byte blobs (as they were built recursively using this function).
// The return value is the built proto or an error about why the proto could
// not be built.
void BuildProto(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  const auto* fn_ctx =
      static_cast<const BuildProtoContext*>(sqlite3_user_data(ctx));
  if (argc % 2 != 0) {
    util::Status error =
        util::ErrStatus("Invalid number of args to %s BuildProto (got %d)",
                        fn_ctx->desc->full_name().c_str(), argc);
    sqlite3_result_error(ctx, error.c_message(), -1);
    return;
  }

  ProtoBuilder builder(fn_ctx->desc);
  for (int i = 0; i < argc; i += 2) {
    if (sqlite3_value_type(argv[i]) != SQLITE_TEXT) {
      sqlite3_result_error(ctx, "BuildProto: Invalid args", -1);
      return;
    }

    auto* key = reinterpret_cast<const char*>(sqlite3_value_text(argv[i]));
    auto value = SqlValueFromSqliteValue(argv[i + 1]);
    auto status = builder.AppendSqlValue(key, value);
    if (!status.ok()) {
      sqlite3_result_error(ctx, status.c_message(), -1);
      return;
    }
  }

  std::vector<uint8_t> raw = builder.SerializeToProtoBuilderResult();
  if (raw.empty()) {
    sqlite3_result_null(ctx);
    return;
  }

  std::unique_ptr<uint8_t[], base::FreeDeleter> data(
      static_cast<uint8_t*>(malloc(raw.size())));
  memcpy(data.get(), raw.data(), raw.size());
  sqlite3_result_blob(ctx, data.release(), static_cast<int>(raw.size()), free);
}

void RunMetric(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
  auto* fn_ctx = static_cast<RunMetricContext*>(sqlite3_user_data(ctx));
  if (argc == 0 || sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
    sqlite3_result_error(ctx, "RUN_METRIC: Invalid arguments", -1);
    return;
  }

  const char* path = reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
  auto metric_it = std::find_if(
      fn_ctx->metrics->begin(), fn_ctx->metrics->end(),
      [path](const SqlMetricFile& metric) { return metric.path == path; });
  if (metric_it == fn_ctx->metrics->end()) {
    sqlite3_result_error(ctx, "RUN_METRIC: Unknown filename provided", -1);
    return;
  }
  const auto& sql = metric_it->sql;

  std::unordered_map<std::string, std::string> substitutions;
  for (int i = 1; i < argc; i += 2) {
    if (sqlite3_value_type(argv[i]) != SQLITE_TEXT) {
      sqlite3_result_error(ctx, "RUN_METRIC: Invalid args", -1);
      return;
    }

    auto* key_str = ExtractSqliteValue(argv[i]);
    auto* value_str = ExtractSqliteValue(argv[i + 1]);
    substitutions[key_str] = value_str;
  }

  for (const auto& query : base::SplitString(sql, ";\n")) {
    std::string buffer;
    int ret = TemplateReplace(query, substitutions, &buffer);
    if (ret) {
      sqlite3_result_error(
          ctx, "RUN_METRIC: Error when performing substitution", -1);
      return;
    }

    PERFETTO_DLOG("RUN_METRIC: Executing query: %s", buffer.c_str());
    auto it = fn_ctx->tp->ExecuteQuery(buffer);
    it.Next();

    util::Status status = it.Status();
    if (!status.ok()) {
      char* error =
          sqlite3_mprintf("RUN_METRIC: Error when running file %s: %s", path,
                          status.c_message());
      sqlite3_result_error(ctx, error, -1);
      sqlite3_free(error);
      return;
    }
  }
}

util::Status ComputeMetrics(TraceProcessor* tp,
                            const std::vector<std::string> metrics_to_compute,
                            const std::vector<SqlMetricFile>& sql_metrics,
                            const ProtoDescriptor& root_descriptor,
                            std::vector<uint8_t>* metrics_proto) {
  ProtoBuilder metric_builder(&root_descriptor);
  for (const auto& name : metrics_to_compute) {
    auto metric_it =
        std::find_if(sql_metrics.begin(), sql_metrics.end(),
                     [&name](const SqlMetricFile& metric) {
                       return metric.proto_field_name.has_value() &&
                              name == metric.proto_field_name.value();
                     });
    if (metric_it == sql_metrics.end())
      return util::ErrStatus("Unknown metric %s", name.c_str());

    const auto& sql_metric = *metric_it;
    auto queries = base::SplitString(sql_metric.sql, ";\n");
    for (const auto& query : queries) {
      PERFETTO_DLOG("Executing query: %s", query.c_str());
      auto prep_it = tp->ExecuteQuery(query);
      prep_it.Next();

      util::Status status = prep_it.Status();
      if (!status.ok())
        return status;
    }

    auto output_query =
        "SELECT * FROM " + sql_metric.output_table_name.value() + ";";
    PERFETTO_DLOG("Executing output query: %s", output_query.c_str());

    auto it = tp->ExecuteQuery(output_query.c_str());
    auto has_next = it.Next();
    util::Status status = it.Status();
    if (!status.ok()) {
      return status;
    } else if (!has_next) {
      return util::ErrStatus("Output table %s should have at least one row",
                             sql_metric.output_table_name.value().c_str());
    } else if (it.ColumnCount() != 1) {
      return util::ErrStatus("Output table %s should have exactly one column",
                             sql_metric.output_table_name.value().c_str());
    }

    if (it.Get(0).type == SqlValue::kBytes) {
      const auto& field_name = sql_metric.proto_field_name.value();
      const auto& col = it.Get(0);
      status = metric_builder.AppendSqlValue(field_name, col);
      if (!status.ok())
        return status;
    } else if (it.Get(0).type != SqlValue::kNull) {
      return util::ErrStatus("Output table %s column has invalid type",
                             sql_metric.output_table_name.value().c_str());
    }

    has_next = it.Next();
    if (has_next)
      return util::ErrStatus("Output table %s should only have one row",
                             sql_metric.output_table_name.value().c_str());

    status = it.Status();
    if (!status.ok())
      return status;
  }
  *metrics_proto = metric_builder.SerializeRaw();
  return util::OkStatus();
}

}  // namespace metrics
}  // namespace trace_processor
}  // namespace perfetto
