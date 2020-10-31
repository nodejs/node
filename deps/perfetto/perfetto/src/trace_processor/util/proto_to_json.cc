/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <vector>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/util/proto_to_json.h"

namespace perfetto {
namespace trace_processor {
namespace proto_to_json {

namespace {

std::string EscapeJsonString(const std::string& raw) {
  std::string ret;
  for (auto it = raw.cbegin(); it != raw.cend(); it++) {
    switch (*it) {
      case '\\':
        ret += "\\\\";
        break;
      case '"':
        ret += "\\\"";
        break;
      case '/':
        ret += "\\/";
        break;
      case '\b':
        ret += "\\b";
        break;
      case '\f':
        ret += "\\f";
        break;
      case '\n':
        ret += "\\n";
        break;
      case '\r':
        ret += "\\r";
        break;
      case '\t':
        ret += "\\t";
        break;
      default:
        ret += *it;
        break;
    }
  }
  return '"' + ret + '"';
}

std::string FieldToJson(const google::protobuf::Message& message,
                        const google::protobuf::FieldDescriptor* field_desc,
                        int idx,
                        uint32_t indent) {
  using google::protobuf::FieldDescriptor;

  const google::protobuf::Reflection* ref = message.GetReflection();
  bool is_repeated = field_desc->is_repeated();
  switch (field_desc->cpp_type()) {
    case FieldDescriptor::CppType::CPPTYPE_BOOL:
      return std::to_string(is_repeated
                                ? ref->GetRepeatedBool(message, field_desc, idx)
                                : ref->GetBool(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_ENUM:
      return EscapeJsonString(
          is_repeated ? ref->GetRepeatedEnum(message, field_desc, idx)->name()
                      : ref->GetEnum(message, field_desc)->name());
    case FieldDescriptor::CppType::CPPTYPE_FLOAT:
      return std::to_string(
          is_repeated
              ? static_cast<double>(
                    ref->GetRepeatedFloat(message, field_desc, idx))
              : static_cast<double>(ref->GetFloat(message, field_desc)));
    case FieldDescriptor::CppType::CPPTYPE_INT32:
      return std::to_string(
          is_repeated ? ref->GetRepeatedInt32(message, field_desc, idx)
                      : ref->GetInt32(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_INT64:
      return std::to_string(
          is_repeated ? ref->GetRepeatedInt64(message, field_desc, idx)
                      : ref->GetInt64(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_DOUBLE:
      return std::to_string(
          is_repeated ? ref->GetRepeatedDouble(message, field_desc, idx)
                      : ref->GetDouble(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_STRING:
      return EscapeJsonString(
          is_repeated ? ref->GetRepeatedString(message, field_desc, idx)
                      : ref->GetString(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_UINT32:
      return std::to_string(
          is_repeated ? ref->GetRepeatedUInt32(message, field_desc, idx)
                      : ref->GetUInt32(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_UINT64:
      return std::to_string(
          is_repeated ? ref->GetRepeatedInt64(message, field_desc, idx)
                      : ref->GetInt64(message, field_desc));
    case FieldDescriptor::CppType::CPPTYPE_MESSAGE:
      return MessageToJson(
          is_repeated ? ref->GetRepeatedMessage(message, field_desc, idx)
                      : ref->GetMessage(message, field_desc),
          indent);
  }
  PERFETTO_FATAL("For GCC");
}

std::string RepeatedFieldValuesToJson(
    const google::protobuf::Message& message,
    const google::protobuf::FieldDescriptor* field_desc,
    uint32_t indent) {
  const google::protobuf::Reflection* ref = message.GetReflection();
  std::string ret;
  for (int i = 0; i < ref->FieldSize(message, field_desc); ++i) {
    if (i != 0) {
      ret += ",";
    }
    ret += "\n" + std::string(indent, ' ') +
           FieldToJson(message, field_desc, i, indent);
  }
  return ret;
}

std::string MessageFieldsToJson(const google::protobuf::Message& message,
                                uint32_t indent) {
  const google::protobuf::Reflection* ref = message.GetReflection();
  std::vector<const google::protobuf::FieldDescriptor*> field_descs;
  ref->ListFields(message, &field_descs);

  std::string ret;
  uint32_t next_field_idx = 0;
  for (const google::protobuf::FieldDescriptor* field_desc : field_descs) {
    if (next_field_idx++ != 0) {
      ret += ",";
    }
    std::string value;
    if (field_desc->is_repeated()) {
      value = "[" + RepeatedFieldValuesToJson(message, field_desc, indent + 2) +
              "\n" + std::string(indent, ' ') + "]";
    } else {
      value = FieldToJson(message, field_desc, 0, indent);
    }
    const std::string& name = field_desc->is_extension()
                                  ? field_desc->full_name()
                                  : field_desc->name();
    ret += "\n" + std::string(indent, ' ') + "\"" + name + "\": " + value;
  }
  return ret;
}

// This is a class helps avoid the situation where every function has to take
// field_options_prototype as an argument, which becomes distracting.
class OptionsConverter {
 public:
  explicit OptionsConverter(
      const google::protobuf::Message* field_options_prototype)
      : field_options_prototype_(field_options_prototype) {}

  // Prints all field options for non-empty fields of a message. Example:
  // --- Message definitions ---
  // FooMessage {
  //   repeated int64 foo = 1 [op1 = val1, op2 = val2];
  //   optional BarMessage bar = 2 [op3 = val3];
  // }
  //
  // BarMessage {
  //   optional int64 baz = 1 [op4 = val4];
  // }
  // --- MessageInstance ---
  // foo_msg = {  // (As JSON)
  //   foo: [23, 24, 25],
  //   bar: {
  //     baz: 42
  //   }
  // }
  // --- Output of MessageFieldOptionsToJson(foo_msg) ---
  //   foo: {
  //     __field_options: {
  //       op1: val1,
  //       op2: val2,
  //     },
  //     __repeated: true
  //   }
  //   bar: {
  //     __field_options: {
  //       op3 = val3,
  //     },
  //     baz: {
  //       __field_options: {
  //         op4 = val4
  //       },
  //     }
  //   }
  // --- Notes ---
  // This function does not produce the surrounding braces for easier use in
  // recursive use cases. The caller needs to surround the output with braces.
  std::string MessageFieldOptionsToJson(
      const google::protobuf::Message& message,
      uint32_t indent) {
    using google::protobuf::FieldDescriptor;
    std::vector<const FieldDescriptor*> field_descs;
    message.GetReflection()->ListFields(message, &field_descs);
    std::vector<std::string> field_outputs;
    for (auto* field_desc : field_descs) {
      std::vector<std::string> field_entries;
      if (HasFieldOptions(field_desc)) {
        std::string options_entry;
        options_entry +=
            std::string(indent + 2, ' ') + R"("__field_options": )";
        options_entry += FieldOptionsToJson(field_desc, indent + 4);
        field_entries.push_back(std::move(options_entry));
      }
      std::string nested_fields =
          NestedMessageFieldOptionsToJson(message, field_desc, indent + 2);
      if (nested_fields != "") {
        field_entries.push_back(std::move(nested_fields));
      }
      // We don't output annotations for a field if that field and all its
      // descendants have no field options.
      if (field_entries.size() > 0) {
        if (field_desc->is_repeated()) {
          field_entries.push_back(std::string(indent, ' ') +
                                  R"("__repeated": true)");
        }
        std::string field_output;
        const std::string& name = field_desc->is_extension()
                                      ? field_desc->full_name()
                                      : field_desc->name();
        field_output += std::string(indent, ' ') + "\"" + name + "\": {\n";
        field_output += base::Join(field_entries, ",\n") + "\n";
        field_output += std::string(indent, ' ') + "}";
        field_outputs.push_back(std::move(field_output));
      }
    }
    return base::Join(field_outputs, ",\n");
  }

 private:
  static bool HasFieldOptions(
      const google::protobuf::FieldDescriptor* field_desc) {
    return field_desc->options().ByteSizeLong() > 0;
  }

  std::string NestedMessageFieldOptionsToJson(
      const google::protobuf::Message& message,
      const google::protobuf::FieldDescriptor* field_desc,
      uint32_t indent) {
    using google::protobuf::FieldDescriptor;
    if (field_desc->cpp_type() != FieldDescriptor::CppType::CPPTYPE_MESSAGE)
      return "";
    const auto* reflection = message.GetReflection();
    const google::protobuf::Message& nested_message =
        field_desc->is_repeated()
            ? reflection->GetRepeatedMessage(message, field_desc, 0)
            : reflection->GetMessage(message, field_desc);
    return MessageFieldOptionsToJson(nested_message, indent);
  }

  std::string FieldOptionsToJson(
      const google::protobuf::FieldDescriptor* field_desc,
      uint32_t indent) {
    PERFETTO_DCHECK(HasFieldOptions(field_desc));
    std::unique_ptr<google::protobuf::Message> options(
        field_options_prototype_->New());
    // Field option extensions are compiled at runtime as opposed to being
    // compiled in and being part of the generated pool, so the field option
    // must be re-parsed as a dynamic message for the extensions to show up. If
    // we do not do this, the extension fields remain "unknown fields" to the
    // reflection API.
    options->ParseFromString(field_desc->options().SerializeAsString());
    return MessageToJson(*options, indent);
  }

  const google::protobuf::Message* field_options_prototype_;
};

}  // namespace

std::string MessageToJson(const google::protobuf::Message& message,
                          uint32_t indent) {
  return "{" + MessageFieldsToJson(message, indent + 2) + '\n' +
         std::string(indent, ' ') + "}";
}

std::string MessageToJsonWithAnnotations(
    const google::protobuf::Message& message,
    const google::protobuf::Message* field_options_prototype,
    uint32_t indent) {
  std::string ret;
  OptionsConverter options_converter(field_options_prototype);

  ret = "{" + MessageFieldsToJson(message, indent + 2);
  std::string annotation_fields =
      options_converter.MessageFieldOptionsToJson(message, indent + 4);
  if (annotation_fields != "") {
    ret += ",\n";
    ret += std::string(indent + 2, ' ') + "\"__annotations\": {\n";
    ret += annotation_fields + "\n";
    ret += std::string(indent + 2, ' ') + "}\n";
  } else {
    ret += "\n";
  }
  ret += std::string(indent, ' ') + "}\n";
  return ret;
}

}  // namespace proto_to_json
}  // namespace trace_processor
}  // namespace perfetto
