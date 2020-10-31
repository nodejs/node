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

#include <cmath>
#include <limits>
#include <sstream>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/wire_format.h>

#include <google/protobuf/compiler/csharp/csharp_field_base.h>
#include <google/protobuf/compiler/csharp/csharp_helpers.h>
#include <google/protobuf/compiler/csharp/csharp_names.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

void FieldGeneratorBase::SetCommonFieldVariables(
    std::map<string, string>* variables) {
  // Note: this will be valid even though the tag emitted for packed and unpacked versions of
  // repeated fields varies by wire format. The wire format is encoded in the bottom 3 bits, which
  // never effects the tag size.
  int tag_size = internal::WireFormat::TagSize(descriptor_->number(), descriptor_->type());
  int part_tag_size = tag_size;
  if (descriptor_->type() == FieldDescriptor::TYPE_GROUP) {
    part_tag_size /= 2;
  }
  uint tag = internal::WireFormat::MakeTag(descriptor_);
  uint8 tag_array[5];
  io::CodedOutputStream::WriteTagToArray(tag, tag_array);
  string tag_bytes = StrCat(tag_array[0]);
  for (int i = 1; i < part_tag_size; i++) {
    tag_bytes += ", " + StrCat(tag_array[i]);
  }

  (*variables)["tag"] = StrCat(tag);
  (*variables)["tag_size"] = StrCat(tag_size);
  (*variables)["tag_bytes"] = tag_bytes;

  if (descriptor_->type() == FieldDescriptor::Type::TYPE_GROUP) {
    tag = internal::WireFormatLite::MakeTag(
        descriptor_->number(),
        internal::WireFormatLite::WIRETYPE_END_GROUP);
    io::CodedOutputStream::WriteTagToArray(tag, tag_array);
    tag_bytes = StrCat(tag_array[0]);
    for (int i = 1; i < part_tag_size; i++) {
        tag_bytes += ", " + StrCat(tag_array[i]);
    }

    variables_["end_tag"] = StrCat(tag);
    variables_["end_tag_bytes"] = tag_bytes;
  }

  (*variables)["access_level"] = "public";

  (*variables)["property_name"] = property_name();
  (*variables)["type_name"] = type_name();
  (*variables)["extended_type"] = GetClassName(descriptor_->containing_type());
  (*variables)["name"] = name();
  (*variables)["descriptor_name"] = descriptor_->name();
  (*variables)["default_value"] = default_value();
  (*variables)["capitalized_type_name"] = capitalized_type_name();
  (*variables)["number"] = number();
  if (has_default_value() && !IsProto2(descriptor_->file())) {
    (*variables)["name_def_message"] =
      (*variables)["name"] + "_ = " + (*variables)["default_value"];
  } else {
    (*variables)["name_def_message"] = (*variables)["name"] + "_";
  }
  if (IsProto2(descriptor_->file())) {
    (*variables)["has_property_check"] = "Has" + (*variables)["property_name"];
    (*variables)["other_has_property_check"] = "other.Has" + (*variables)["property_name"];
    (*variables)["has_not_property_check"] = "!" + (*variables)["has_property_check"];
    (*variables)["other_has_not_property_check"] = "!" + (*variables)["other_has_property_check"];
    if (presenceIndex_ != -1) {
      string hasBitsNumber = StrCat(presenceIndex_ / 32);
      string hasBitsMask = StrCat(1 << (presenceIndex_ % 32));
      (*variables)["has_field_check"] = "(_hasBits" + hasBitsNumber + " & " + hasBitsMask + ") != 0";
      (*variables)["set_has_field"] = "_hasBits" + hasBitsNumber + " |= " + hasBitsMask;
      (*variables)["clear_has_field"] = "_hasBits" + hasBitsNumber + " &= ~" + hasBitsMask;
    }
  } else {
    (*variables)["has_property_check"] =
      (*variables)["property_name"] + " != " + (*variables)["default_value"];
    (*variables)["other_has_property_check"] = "other." +
      (*variables)["property_name"] + " != " + (*variables)["default_value"];
  }
}

void FieldGeneratorBase::SetCommonOneofFieldVariables(
    std::map<string, string>* variables) {
  (*variables)["oneof_name"] = oneof_name();
  if (IsProto2(descriptor_->file())) {
    (*variables)["has_property_check"] = "Has" + property_name();
  } else {
    (*variables)["has_property_check"] =
      oneof_name() + "Case_ == " + oneof_property_name() +
      "OneofCase." + property_name();
  }
  (*variables)["oneof_property_name"] = oneof_property_name();
}

FieldGeneratorBase::FieldGeneratorBase(const FieldDescriptor* descriptor,
                                       int presenceIndex, const Options* options)
    : SourceGeneratorBase(descriptor->file(), options),
      descriptor_(descriptor),
      presenceIndex_(presenceIndex) {
  SetCommonFieldVariables(&variables_);
}

FieldGeneratorBase::~FieldGeneratorBase() {
}

void FieldGeneratorBase::GenerateFreezingCode(io::Printer* printer) {
  // No-op: only message fields and repeated fields need
  // special handling for freezing, so default to not generating any code.
}

void FieldGeneratorBase::GenerateCodecCode(io::Printer* printer) {
    // No-op: expect this to be overridden by appropriate types.
    // Could fail if we get called here though...
}

void FieldGeneratorBase::GenerateExtensionCode(io::Printer* printer) {
  // No-op: only message fields, enum fields, primitives, 
  // and repeated fields need this default is to not generate any code
}

void FieldGeneratorBase::AddDeprecatedFlag(io::Printer* printer) {
  if (descriptor_->options().deprecated()) {
    printer->Print("[global::System.ObsoleteAttribute]\n");
  } else if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE &&
           descriptor_->message_type()->options().deprecated()) {
    printer->Print("[global::System.ObsoleteAttribute]\n");
  }
}

void FieldGeneratorBase::AddPublicMemberAttributes(io::Printer* printer) {
  AddDeprecatedFlag(printer);
  WriteGeneratedCodeAttributes(printer);
}

std::string FieldGeneratorBase::oneof_property_name() {
  return UnderscoresToCamelCase(descriptor_->containing_oneof()->name(), true);
}

std::string FieldGeneratorBase::oneof_name() {
  return UnderscoresToCamelCase(descriptor_->containing_oneof()->name(), false);
}

std::string FieldGeneratorBase::property_name() {
  return GetPropertyName(descriptor_);
}

std::string FieldGeneratorBase::name() {
  return UnderscoresToCamelCase(GetFieldName(descriptor_), false);
}

std::string FieldGeneratorBase::type_name() {
  return type_name(descriptor_);
}

std::string FieldGeneratorBase::type_name(const FieldDescriptor* descriptor) {
  switch (descriptor->type()) {
    case FieldDescriptor::TYPE_ENUM:
      return GetClassName(descriptor->enum_type());
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      if (IsWrapperType(descriptor)) {
        const FieldDescriptor* wrapped_field =
            descriptor->message_type()->field(0);
        string wrapped_field_type_name = type_name(wrapped_field);
        // String and ByteString go to the same type; other wrapped types
        // go to the nullable equivalent.
        if (wrapped_field->type() == FieldDescriptor::TYPE_STRING ||
            wrapped_field->type() == FieldDescriptor::TYPE_BYTES) {
          return wrapped_field_type_name;
        } else {
          return wrapped_field_type_name + "?";
        }
      }
      return GetClassName(descriptor->message_type());
    case FieldDescriptor::TYPE_DOUBLE:
      return "double";
    case FieldDescriptor::TYPE_FLOAT:
      return "float";
    case FieldDescriptor::TYPE_INT64:
      return "long";
    case FieldDescriptor::TYPE_UINT64:
      return "ulong";
    case FieldDescriptor::TYPE_INT32:
      return "int";
    case FieldDescriptor::TYPE_FIXED64:
      return "ulong";
    case FieldDescriptor::TYPE_FIXED32:
      return "uint";
    case FieldDescriptor::TYPE_BOOL:
      return "bool";
    case FieldDescriptor::TYPE_STRING:
      return "string";
    case FieldDescriptor::TYPE_BYTES:
      return "pb::ByteString";
    case FieldDescriptor::TYPE_UINT32:
      return "uint";
    case FieldDescriptor::TYPE_SFIXED32:
      return "int";
    case FieldDescriptor::TYPE_SFIXED64:
      return "long";
    case FieldDescriptor::TYPE_SINT32:
      return "int";
    case FieldDescriptor::TYPE_SINT64:
      return "long";
    default:
      GOOGLE_LOG(FATAL)<< "Unknown field type.";
      return "";
  }
}

bool FieldGeneratorBase::has_default_value() {
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_ENUM:
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      return true;
    case FieldDescriptor::TYPE_DOUBLE:
      return descriptor_->default_value_double() != 0.0;
    case FieldDescriptor::TYPE_FLOAT:
      return descriptor_->default_value_float() != 0.0;
    case FieldDescriptor::TYPE_INT64:
      return descriptor_->default_value_int64() != 0L;
    case FieldDescriptor::TYPE_UINT64:
      return descriptor_->default_value_uint64() != 0L;
    case FieldDescriptor::TYPE_INT32:
      return descriptor_->default_value_int32() != 0;
    case FieldDescriptor::TYPE_FIXED64:
      return descriptor_->default_value_uint64() != 0L;
    case FieldDescriptor::TYPE_FIXED32:
      return descriptor_->default_value_uint32() != 0;
    case FieldDescriptor::TYPE_BOOL:
      return descriptor_->default_value_bool();
    case FieldDescriptor::TYPE_STRING:
      return true;
    case FieldDescriptor::TYPE_BYTES:
      return true;
    case FieldDescriptor::TYPE_UINT32:
      return descriptor_->default_value_uint32() != 0;
    case FieldDescriptor::TYPE_SFIXED32:
      return descriptor_->default_value_int32() != 0;
    case FieldDescriptor::TYPE_SFIXED64:
      return descriptor_->default_value_int64() != 0L;
    case FieldDescriptor::TYPE_SINT32:
      return descriptor_->default_value_int32() != 0;
    case FieldDescriptor::TYPE_SINT64:
      return descriptor_->default_value_int64() != 0L;
    default:
      GOOGLE_LOG(FATAL)<< "Unknown field type.";
      return true;
  }
}

bool AllPrintableAscii(const std::string& text) {
  for(int i = 0; i < text.size(); i++) {
    if (text[i] < 0x20 || text[i] > 0x7e) {
      return false;
    }
  }
  return true;
}

std::string FieldGeneratorBase::GetStringDefaultValueInternal(const FieldDescriptor* descriptor) {
    if (descriptor->default_value_string().empty())
        return "\"\"";
    else
        return "global::System.Encoding.UTF8.GetString(global::System.Convert.FromBase64String(\" +" + StringToBase64(descriptor->default_value_string()) + " +\"))";
}

std::string FieldGeneratorBase::GetBytesDefaultValueInternal(const FieldDescriptor* descriptor) {
    if (descriptor->default_value_string().empty())
        return "pb::ByteString.Empty";
    else
        return "pb::ByteString.FromBase64(\"" + StringToBase64(descriptor->default_value_string()) + "\")";
}

std::string FieldGeneratorBase::default_value() {
    return default_value(descriptor_);
}

std::string FieldGeneratorBase::default_value(const FieldDescriptor* descriptor) {
  switch (descriptor->type()) {
    case FieldDescriptor::TYPE_ENUM:
      return GetClassName(descriptor->default_value_enum()->type()) + "." +
        GetEnumValueName(descriptor->default_value_enum()->type()->name(), descriptor->default_value_enum()->name());
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP:
      if (IsWrapperType(descriptor)) {
        const FieldDescriptor* wrapped_field = descriptor->message_type()->field(0);
        return default_value(wrapped_field);
      } else {
        return "null";
      }
    case FieldDescriptor::TYPE_DOUBLE: {
      double value = descriptor->default_value_double();
      if (value == std::numeric_limits<double>::infinity()) {
        return "double.PositiveInfinity";
      } else if (value == -std::numeric_limits<double>::infinity()) {
        return "double.NegativeInfinity";
      } else if (std::isnan(value)) {
        return "double.NaN";
      }
      return StrCat(value) + "D";
    }
    case FieldDescriptor::TYPE_FLOAT: {
      float value = descriptor->default_value_float();
      if (value == std::numeric_limits<float>::infinity()) {
        return "float.PositiveInfinity";
      } else if (value == -std::numeric_limits<float>::infinity()) {
        return "float.NegativeInfinity";
      } else if (std::isnan(value)) {
        return "float.NaN";
      }
      return StrCat(value) + "F";
    }
    case FieldDescriptor::TYPE_INT64:
      return StrCat(descriptor->default_value_int64()) + "L";
    case FieldDescriptor::TYPE_UINT64:
      return StrCat(descriptor->default_value_uint64()) + "UL";
    case FieldDescriptor::TYPE_INT32:
      return StrCat(descriptor->default_value_int32());
    case FieldDescriptor::TYPE_FIXED64:
      return StrCat(descriptor->default_value_uint64()) + "UL";
    case FieldDescriptor::TYPE_FIXED32:
      return StrCat(descriptor->default_value_uint32());
    case FieldDescriptor::TYPE_BOOL:
      if (descriptor->default_value_bool()) {
        return "true";
      } else {
        return "false";
      }
    case FieldDescriptor::TYPE_STRING:
      return GetStringDefaultValueInternal(descriptor);
    case FieldDescriptor::TYPE_BYTES:
      return GetBytesDefaultValueInternal(descriptor);
    case FieldDescriptor::TYPE_UINT32:
      return StrCat(descriptor->default_value_uint32());
    case FieldDescriptor::TYPE_SFIXED32:
      return StrCat(descriptor->default_value_int32());
    case FieldDescriptor::TYPE_SFIXED64:
      return StrCat(descriptor->default_value_int64()) + "L";
    case FieldDescriptor::TYPE_SINT32:
      return StrCat(descriptor->default_value_int32());
    case FieldDescriptor::TYPE_SINT64:
      return StrCat(descriptor->default_value_int64()) + "L";
    default:
      GOOGLE_LOG(FATAL)<< "Unknown field type.";
      return "";
  }
}

std::string FieldGeneratorBase::number() {
  return StrCat(descriptor_->number());
}

std::string FieldGeneratorBase::capitalized_type_name() {
  switch (descriptor_->type()) {
    case FieldDescriptor::TYPE_ENUM:
      return "Enum";
    case FieldDescriptor::TYPE_MESSAGE:
      return "Message";
    case FieldDescriptor::TYPE_GROUP:
      return "Group";
    case FieldDescriptor::TYPE_DOUBLE:
      return "Double";
    case FieldDescriptor::TYPE_FLOAT:
      return "Float";
    case FieldDescriptor::TYPE_INT64:
      return "Int64";
    case FieldDescriptor::TYPE_UINT64:
      return "UInt64";
    case FieldDescriptor::TYPE_INT32:
      return "Int32";
    case FieldDescriptor::TYPE_FIXED64:
      return "Fixed64";
    case FieldDescriptor::TYPE_FIXED32:
      return "Fixed32";
    case FieldDescriptor::TYPE_BOOL:
      return "Bool";
    case FieldDescriptor::TYPE_STRING:
      return "String";
    case FieldDescriptor::TYPE_BYTES:
      return "Bytes";
    case FieldDescriptor::TYPE_UINT32:
      return "UInt32";
    case FieldDescriptor::TYPE_SFIXED32:
      return "SFixed32";
    case FieldDescriptor::TYPE_SFIXED64:
      return "SFixed64";
    case FieldDescriptor::TYPE_SINT32:
      return "SInt32";
    case FieldDescriptor::TYPE_SINT64:
      return "SInt64";
    default:
      GOOGLE_LOG(FATAL)<< "Unknown field type.";
      return "";
  }
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
