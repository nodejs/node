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

#include <sstream>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite.h>

#include <google/protobuf/compiler/csharp/csharp_doc_comment.h>
#include <google/protobuf/compiler/csharp/csharp_helpers.h>
#include <google/protobuf/compiler/csharp/csharp_message_field.h>
#include <google/protobuf/compiler/csharp/csharp_options.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

MessageFieldGenerator::MessageFieldGenerator(const FieldDescriptor* descriptor,
                                             int presenceIndex,
                                             const Options *options)
    : FieldGeneratorBase(descriptor, presenceIndex, options) {
  if (!IsProto2(descriptor_->file())) {
    variables_["has_property_check"] = name() + "_ != null";
    variables_["has_not_property_check"] = name() + "_ == null";
  }
}

MessageFieldGenerator::~MessageFieldGenerator() {

}

void MessageFieldGenerator::GenerateMembers(io::Printer* printer) {
  printer->Print(
    variables_,
    "private $type_name$ $name$_;\n");
  WritePropertyDocComment(printer, descriptor_);
  AddPublicMemberAttributes(printer);
  printer->Print(
    variables_,
    "$access_level$ $type_name$ $property_name$ {\n"
    "  get { return $name$_; }\n"
    "  set {\n"
    "    $name$_ = value;\n"
    "  }\n"
    "}\n");
  if (IsProto2(descriptor_->file())) {
    printer->Print(
      variables_,
      "/// <summary>Gets whether the $descriptor_name$ field is set</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "$access_level$ bool Has$property_name$ {\n"
      "  get { return $name$_ != null; }\n"
      "}\n");
    printer->Print(
      variables_,
      "/// <summary>Clears the value of the $descriptor_name$ field</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "$access_level$ void Clear$property_name$() {\n"
      "  $name$_ = null;\n"
      "}\n");
  }
}

void MessageFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if (other.$has_property_check$) {\n"
    "  if ($has_not_property_check$) {\n"
    "    $property_name$ = new $type_name$();\n"
    "  }\n"
    "  $property_name$.MergeFrom(other.$property_name$);\n"
    "}\n");
}

void MessageFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  printer->Print(
    variables_,
    "if ($has_not_property_check$) {\n"
    "  $property_name$ = new $type_name$();\n"
    "}\n");
  if (descriptor_->type() == FieldDescriptor::Type::TYPE_MESSAGE) {
    printer->Print(variables_, "input.ReadMessage($property_name$);\n");
  } else {
    printer->Print(variables_, "input.ReadGroup($property_name$);\n");
  }
}

void MessageFieldGenerator::GenerateSerializationCode(io::Printer* printer) {
  if (descriptor_->type() == FieldDescriptor::Type::TYPE_MESSAGE) {
    printer->Print(
      variables_,
      "if ($has_property_check$) {\n"
      "  output.WriteRawTag($tag_bytes$);\n"
      "  output.WriteMessage($property_name$);\n"
      "}\n");
  } else {
    printer->Print(
      variables_,
      "if ($has_property_check$) {\n"
      "  output.WriteRawTag($tag_bytes$);\n"
      "  output.WriteGroup($property_name$);\n"
      "  output.WriteRawTag($end_tag_bytes$);\n"
      "}\n");
  }
}

void MessageFieldGenerator::GenerateSerializedSizeCode(io::Printer* printer) {
  if (descriptor_->type() == FieldDescriptor::Type::TYPE_MESSAGE) {
    printer->Print(
      variables_,
      "if ($has_property_check$) {\n"
      "  size += $tag_size$ + pb::CodedOutputStream.ComputeMessageSize($property_name$);\n"
      "}\n");
  } else {
    printer->Print(
      variables_,
      "if ($has_property_check$) {\n"
      "  size += $tag_size$ + pb::CodedOutputStream.ComputeGroupSize($property_name$);\n"
      "}\n");
  }
}

void MessageFieldGenerator::WriteHash(io::Printer* printer) {
  printer->Print(
    variables_,
    "if ($has_property_check$) hash ^= $property_name$.GetHashCode();\n");
}
void MessageFieldGenerator::WriteEquals(io::Printer* printer) {
  printer->Print(
    variables_,
    "if (!object.Equals($property_name$, other.$property_name$)) return false;\n");
}
void MessageFieldGenerator::WriteToString(io::Printer* printer) {
  variables_["field_name"] = GetFieldName(descriptor_);
  printer->Print(
    variables_,
    "PrintField(\"$field_name$\", has$property_name$, $name$_, writer);\n");
}
void MessageFieldGenerator::GenerateExtensionCode(io::Printer* printer) {
  WritePropertyDocComment(printer, descriptor_);
  AddDeprecatedFlag(printer);
  printer->Print(
    variables_,
    "$access_level$ static readonly pb::Extension<$extended_type$, $type_name$> $property_name$ =\n"
    "  new pb::Extension<$extended_type$, $type_name$>($number$, ");
  GenerateCodecCode(printer);
  printer->Print(");\n");
}
void MessageFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  printer->Print(variables_,
    "$name$_ = other.$has_property_check$ ? other.$name$_.Clone() : null;\n");
}

void MessageFieldGenerator::GenerateFreezingCode(io::Printer* printer) {
}

void MessageFieldGenerator::GenerateCodecCode(io::Printer* printer) {
  if (descriptor_->type() == FieldDescriptor::Type::TYPE_MESSAGE) {
    printer->Print(
      variables_,
      "pb::FieldCodec.ForMessage($tag$, $type_name$.Parser)");
  } else {
    printer->Print(
      variables_,
      "pb::FieldCodec.ForGroup($tag$, $end_tag$, $type_name$.Parser)");
  }
}

MessageOneofFieldGenerator::MessageOneofFieldGenerator(
    const FieldDescriptor* descriptor,
	  int presenceIndex,
    const Options *options)
    : MessageFieldGenerator(descriptor, presenceIndex, options) {
  SetCommonOneofFieldVariables(&variables_);
}

MessageOneofFieldGenerator::~MessageOneofFieldGenerator() {

}

void MessageOneofFieldGenerator::GenerateMembers(io::Printer* printer) {
  WritePropertyDocComment(printer, descriptor_);
  AddPublicMemberAttributes(printer);
  printer->Print(
    variables_,
    "$access_level$ $type_name$ $property_name$ {\n"
    "  get { return $has_property_check$ ? ($type_name$) $oneof_name$_ : null; }\n"
    "  set {\n"
    "    $oneof_name$_ = value;\n"
    "    $oneof_name$Case_ = value == null ? $oneof_property_name$OneofCase.None : $oneof_property_name$OneofCase.$property_name$;\n"
    "  }\n"
    "}\n");
  if (IsProto2(descriptor_->file())) {
    printer->Print(
      variables_,
      "/// <summary>Gets whether the \"$descriptor_name$\" field is set</summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "$access_level$ bool Has$property_name$ {\n"
      "  get { return $oneof_name$Case_ == $oneof_property_name$OneofCase.$property_name$; }\n"
      "}\n");
    printer->Print(
      variables_,
      "/// <summary> Clears the value of the oneof if it's currently set to \"$descriptor_name$\" </summary>\n");
    AddPublicMemberAttributes(printer);
    printer->Print(
      variables_,
      "$access_level$ void Clear$property_name$() {\n"
      "  if ($has_property_check$) {\n"
      "    Clear$oneof_property_name$();\n"
      "  }\n"
      "}\n");
  }
}

void MessageOneofFieldGenerator::GenerateMergingCode(io::Printer* printer) {
  printer->Print(variables_,
    "if ($property_name$ == null) {\n"
    "  $property_name$ = new $type_name$();\n"
    "}\n"
    "$property_name$.MergeFrom(other.$property_name$);\n");
}

void MessageOneofFieldGenerator::GenerateParsingCode(io::Printer* printer) {
  // TODO(jonskeet): We may be able to do better than this
  printer->Print(
    variables_,
    "$type_name$ subBuilder = new $type_name$();\n"
    "if ($has_property_check$) {\n"
    "  subBuilder.MergeFrom($property_name$);\n"
    "}\n");
  if (descriptor_->type() == FieldDescriptor::Type::TYPE_MESSAGE) {
    printer->Print("input.ReadMessage(subBuilder);\n");
  } else {
    printer->Print("input.ReadGroup(subBuilder);\n");
  }
  printer->Print(variables_, "$property_name$ = subBuilder;\n");
}

void MessageOneofFieldGenerator::WriteToString(io::Printer* printer) {
  printer->Print(
    variables_,
    "PrintField(\"$descriptor_name$\", $has_property_check$, $oneof_name$_, writer);\n");
}

void MessageOneofFieldGenerator::GenerateCloningCode(io::Printer* printer) {
  printer->Print(variables_,
    "$property_name$ = other.$property_name$.Clone();\n");
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
