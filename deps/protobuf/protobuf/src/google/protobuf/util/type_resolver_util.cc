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

#include <google/protobuf/type.pb.h>
#include <google/protobuf/wrappers.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/internal/utility.h>
#include <google/protobuf/util/type_resolver.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/stubs/status.h>

// clang-format off
#include <google/protobuf/port_def.inc>
// clang-format on

namespace google {
namespace protobuf {
namespace util {
namespace {
using google::protobuf::Any;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::Enum;
using google::protobuf::EnumValue;
using google::protobuf::Field;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::Option;
using google::protobuf::StringValue;
using google::protobuf::Type;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;

using util::Status;
using util::error::INVALID_ARGUMENT;
using util::error::NOT_FOUND;

class DescriptorPoolTypeResolver : public TypeResolver {
 public:
  DescriptorPoolTypeResolver(const std::string& url_prefix,
                             const DescriptorPool* pool)
      : url_prefix_(url_prefix), pool_(pool) {}

  Status ResolveMessageType(const std::string& type_url, Type* type) override {
    std::string type_name;
    Status status = ParseTypeUrl(type_url, &type_name);
    if (!status.ok()) {
      return status;
    }

    const Descriptor* descriptor = pool_->FindMessageTypeByName(type_name);
    if (descriptor == NULL) {
      return Status(util::error::NOT_FOUND,
                    "Invalid type URL, unknown type: " + type_name);
    }
    ConvertDescriptor(descriptor, type);
    return Status();
  }

  Status ResolveEnumType(const std::string& type_url,
                         Enum* enum_type) override {
    std::string type_name;
    Status status = ParseTypeUrl(type_url, &type_name);
    if (!status.ok()) {
      return status;
    }

    const EnumDescriptor* descriptor = pool_->FindEnumTypeByName(type_name);
    if (descriptor == NULL) {
      return Status(util::error::NOT_FOUND,
                    "Invalid type URL, unknown type: " + type_name);
    }
    ConvertEnumDescriptor(descriptor, enum_type);
    return Status();
  }

 private:
  void ConvertDescriptor(const Descriptor* descriptor, Type* type) {
    type->Clear();
    type->set_name(descriptor->full_name());
    for (int i = 0; i < descriptor->field_count(); ++i) {
      ConvertFieldDescriptor(descriptor->field(i), type->add_fields());
    }
    for (int i = 0; i < descriptor->oneof_decl_count(); ++i) {
      type->add_oneofs(descriptor->oneof_decl(i)->name());
    }
    type->mutable_source_context()->set_file_name(descriptor->file()->name());
    ConvertMessageOptions(descriptor->options(), type->mutable_options());
  }

  void ConvertMessageOptions(const MessageOptions& options,
                             RepeatedPtrField<Option>* output) {
    return ConvertOptionsInternal(options, output);
  }

  void ConvertFieldOptions(const FieldOptions& options,
                           RepeatedPtrField<Option>* output) {
    return ConvertOptionsInternal(options, output);
  }

  void ConvertEnumOptions(const EnumOptions& options,
                          RepeatedPtrField<Option>* output) {
    return ConvertOptionsInternal(options, output);
  }

  void ConvertEnumValueOptions(const EnumValueOptions& options,
                               RepeatedPtrField<Option>* output) {
    return ConvertOptionsInternal(options, output);
  }

  // Implementation details for Convert*Options.
  void ConvertOptionsInternal(const Message& options,
                              RepeatedPtrField<Option>* output) {
    const Reflection* reflection = options.GetReflection();
    std::vector<const FieldDescriptor*> fields;
    reflection->ListFields(options, &fields);
    for (const FieldDescriptor* field : fields) {
      if (field->is_repeated()) {
        const int size = reflection->FieldSize(options, field);
        for (int i = 0; i < size; i++) {
          ConvertOptionField(reflection, options, field, i, output->Add());
        }
      } else {
        ConvertOptionField(reflection, options, field, -1, output->Add());
      }
    }
  }

  static void ConvertOptionField(const Reflection* reflection,
                                 const Message& options,
                                 const FieldDescriptor* field, int index,
                                 Option* out) {
    out->set_name(field->is_extension() ? field->full_name() : field->name());
    Any* value = out->mutable_value();
    switch (field->cpp_type()) {
      case FieldDescriptor::CPPTYPE_MESSAGE:
        value->PackFrom(
            field->is_repeated()
                ? reflection->GetRepeatedMessage(options, field, index)
                : reflection->GetMessage(options, field));
        return;
      case FieldDescriptor::CPPTYPE_DOUBLE:
        value->PackFrom(WrapValue<DoubleValue>(
            field->is_repeated()
                ? reflection->GetRepeatedDouble(options, field, index)
                : reflection->GetDouble(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_FLOAT:
        value->PackFrom(WrapValue<FloatValue>(
            field->is_repeated()
                ? reflection->GetRepeatedFloat(options, field, index)
                : reflection->GetFloat(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_INT64:
        value->PackFrom(WrapValue<Int64Value>(
            field->is_repeated()
                ? reflection->GetRepeatedInt64(options, field, index)
                : reflection->GetInt64(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_UINT64:
        value->PackFrom(WrapValue<UInt64Value>(
            field->is_repeated()
                ? reflection->GetRepeatedUInt64(options, field, index)
                : reflection->GetUInt64(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_INT32:
        value->PackFrom(WrapValue<Int32Value>(
            field->is_repeated()
                ? reflection->GetRepeatedInt32(options, field, index)
                : reflection->GetInt32(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_UINT32:
        value->PackFrom(WrapValue<UInt32Value>(
            field->is_repeated()
                ? reflection->GetRepeatedUInt32(options, field, index)
                : reflection->GetUInt32(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_BOOL:
        value->PackFrom(WrapValue<BoolValue>(
            field->is_repeated()
                ? reflection->GetRepeatedBool(options, field, index)
                : reflection->GetBool(options, field)));
        return;
      case FieldDescriptor::CPPTYPE_STRING: {
        const std::string& val =
            field->is_repeated()
                ? reflection->GetRepeatedString(options, field, index)
                : reflection->GetString(options, field);
        if (field->type() == FieldDescriptor::TYPE_STRING) {
          value->PackFrom(WrapValue<StringValue>(val));
        } else {
          value->PackFrom(WrapValue<BytesValue>(val));
        }
        return;
      }
      case FieldDescriptor::CPPTYPE_ENUM: {
        const EnumValueDescriptor* val =
            field->is_repeated()
                ? reflection->GetRepeatedEnum(options, field, index)
                : reflection->GetEnum(options, field);
        value->PackFrom(WrapValue<Int32Value>(val->number()));
        return;
      }
    }
  }

  template <typename WrapperT, typename T>
  static WrapperT WrapValue(T value) {
    WrapperT wrapper;
    wrapper.set_value(value);
    return wrapper;
  }

  void ConvertFieldDescriptor(const FieldDescriptor* descriptor, Field* field) {
    field->set_kind(static_cast<Field::Kind>(descriptor->type()));
    switch (descriptor->label()) {
      case FieldDescriptor::LABEL_OPTIONAL:
        field->set_cardinality(Field::CARDINALITY_OPTIONAL);
        break;
      case FieldDescriptor::LABEL_REPEATED:
        field->set_cardinality(Field::CARDINALITY_REPEATED);
        break;
      case FieldDescriptor::LABEL_REQUIRED:
        field->set_cardinality(Field::CARDINALITY_REQUIRED);
        break;
    }
    field->set_number(descriptor->number());
    field->set_name(descriptor->name());
    field->set_json_name(descriptor->json_name());
    if (descriptor->has_default_value()) {
      field->set_default_value(DefaultValueAsString(descriptor));
    }
    if (descriptor->type() == FieldDescriptor::TYPE_MESSAGE ||
        descriptor->type() == FieldDescriptor::TYPE_GROUP) {
      field->set_type_url(GetTypeUrl(descriptor->message_type()));
    } else if (descriptor->type() == FieldDescriptor::TYPE_ENUM) {
      field->set_type_url(GetTypeUrl(descriptor->enum_type()));
    }
    if (descriptor->containing_oneof() != NULL) {
      field->set_oneof_index(descriptor->containing_oneof()->index() + 1);
    }
    if (descriptor->is_packed()) {
      field->set_packed(true);
    }

    ConvertFieldOptions(descriptor->options(), field->mutable_options());
  }

  void ConvertEnumDescriptor(const EnumDescriptor* descriptor,
                             Enum* enum_type) {
    enum_type->Clear();
    enum_type->set_name(descriptor->full_name());
    enum_type->mutable_source_context()->set_file_name(
        descriptor->file()->name());
    for (int i = 0; i < descriptor->value_count(); ++i) {
      const EnumValueDescriptor* value_descriptor = descriptor->value(i);
      EnumValue* value = enum_type->mutable_enumvalue()->Add();
      value->set_name(value_descriptor->name());
      value->set_number(value_descriptor->number());

      ConvertEnumValueOptions(value_descriptor->options(),
                              value->mutable_options());
    }

    ConvertEnumOptions(descriptor->options(), enum_type->mutable_options());
  }

  std::string GetTypeUrl(const Descriptor* descriptor) {
    return url_prefix_ + "/" + descriptor->full_name();
  }

  std::string GetTypeUrl(const EnumDescriptor* descriptor) {
    return url_prefix_ + "/" + descriptor->full_name();
  }

  Status ParseTypeUrl(const string& type_url, std::string* type_name) {
    if (type_url.substr(0, url_prefix_.size() + 1) != url_prefix_ + "/") {
      return Status(
          util::error::INVALID_ARGUMENT,
          StrCat("Invalid type URL, type URLs must be of the form '",
                       url_prefix_, "/<typename>', got: ", type_url));
    }
    *type_name = type_url.substr(url_prefix_.size() + 1);
    return Status();
  }

  std::string DefaultValueAsString(const FieldDescriptor* descriptor) {
    switch (descriptor->cpp_type()) {
      case FieldDescriptor::CPPTYPE_INT32:
        return StrCat(descriptor->default_value_int32());
        break;
      case FieldDescriptor::CPPTYPE_INT64:
        return StrCat(descriptor->default_value_int64());
        break;
      case FieldDescriptor::CPPTYPE_UINT32:
        return StrCat(descriptor->default_value_uint32());
        break;
      case FieldDescriptor::CPPTYPE_UINT64:
        return StrCat(descriptor->default_value_uint64());
        break;
      case FieldDescriptor::CPPTYPE_FLOAT:
        return SimpleFtoa(descriptor->default_value_float());
        break;
      case FieldDescriptor::CPPTYPE_DOUBLE:
        return SimpleDtoa(descriptor->default_value_double());
        break;
      case FieldDescriptor::CPPTYPE_BOOL:
        return descriptor->default_value_bool() ? "true" : "false";
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        if (descriptor->type() == FieldDescriptor::TYPE_BYTES) {
          return CEscape(descriptor->default_value_string());
        } else {
          return descriptor->default_value_string();
        }
        break;
      case FieldDescriptor::CPPTYPE_ENUM:
        return descriptor->default_value_enum()->name();
        break;
      case FieldDescriptor::CPPTYPE_MESSAGE:
        GOOGLE_LOG(DFATAL) << "Messages can't have default values!";
        break;
    }
    return "";
  }

  std::string url_prefix_;
  const DescriptorPool* pool_;
};

}  // namespace

TypeResolver* NewTypeResolverForDescriptorPool(const std::string& url_prefix,
                                               const DescriptorPool* pool) {
  return new DescriptorPoolTypeResolver(url_prefix, pool);
}

}  // namespace util
}  // namespace protobuf
}  // namespace google
