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

#include <map>
#include <string>

#include <google/protobuf/compiler/objectivec/objectivec_enum_field.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

namespace {

void SetEnumVariables(const FieldDescriptor* descriptor,
                      std::map<string, string>* variables) {
  string type = EnumName(descriptor->enum_type());
  (*variables)["storage_type"] = type;
  // For non repeated fields, if it was defined in a different file, the
  // property decls need to use "enum NAME" rather than just "NAME" to support
  // the forward declaration of the enums.
  if (!descriptor->is_repeated() &&
      (descriptor->file() != descriptor->enum_type()->file())) {
    (*variables)["property_type"] = "enum " + type;
  }
  (*variables)["enum_verifier"] = type + "_IsValidValue";
  (*variables)["enum_desc_func"] = type + "_EnumDescriptor";

  (*variables)["dataTypeSpecific_name"] = "enumDescFunc";
  (*variables)["dataTypeSpecific_value"] = (*variables)["enum_desc_func"];

  const Descriptor* msg_descriptor = descriptor->containing_type();
  (*variables)["owning_message_class"] = ClassName(msg_descriptor);
}
}  // namespace

EnumFieldGenerator::EnumFieldGenerator(const FieldDescriptor* descriptor,
                                       const Options& options)
    : SingleFieldGenerator(descriptor, options) {
  SetEnumVariables(descriptor, &variables_);
}

EnumFieldGenerator::~EnumFieldGenerator() {}

void EnumFieldGenerator::GenerateCFunctionDeclarations(
    io::Printer* printer) const {
  if (!HasPreservingUnknownEnumSemantics(descriptor_->file())) {
    return;
  }

  printer->Print(
      variables_,
      "/**\n"
      " * Fetches the raw value of a @c $owning_message_class$'s @c $name$ property, even\n"
      " * if the value was not defined by the enum at the time the code was generated.\n"
      " **/\n"
      "int32_t $owning_message_class$_$capitalized_name$_RawValue($owning_message_class$ *message);\n"
      "/**\n"
      " * Sets the raw value of an @c $owning_message_class$'s @c $name$ property, allowing\n"
      " * it to be set to a value that was not defined by the enum at the time the code\n"
      " * was generated.\n"
      " **/\n"
      "void Set$owning_message_class$_$capitalized_name$_RawValue($owning_message_class$ *message, int32_t value);\n"
      "\n");
}

void EnumFieldGenerator::GenerateCFunctionImplementations(
    io::Printer* printer) const {
  if (!HasPreservingUnknownEnumSemantics(descriptor_->file())) return;

  printer->Print(
      variables_,
      "int32_t $owning_message_class$_$capitalized_name$_RawValue($owning_message_class$ *message) {\n"
      "  GPBDescriptor *descriptor = [$owning_message_class$ descriptor];\n"
      "  GPBFieldDescriptor *field = [descriptor fieldWithNumber:$field_number_name$];\n"
      "  return GPBGetMessageInt32Field(message, field);\n"
      "}\n"
      "\n"
      "void Set$owning_message_class$_$capitalized_name$_RawValue($owning_message_class$ *message, int32_t value) {\n"
      "  GPBDescriptor *descriptor = [$owning_message_class$ descriptor];\n"
      "  GPBFieldDescriptor *field = [descriptor fieldWithNumber:$field_number_name$];\n"
      "  GPBSetInt32IvarWithFieldInternal(message, field, value, descriptor.file.syntax);\n"
      "}\n"
      "\n");
}

void EnumFieldGenerator::DetermineForwardDeclarations(
    std::set<string>* fwd_decls) const {
  SingleFieldGenerator::DetermineForwardDeclarations(fwd_decls);
  // If it is an enum defined in a different file, then we'll need a forward
  // declaration for it.  When it is in our file, all the enums are output
  // before the message, so it will be declared before it is needed.
  if (descriptor_->file() != descriptor_->enum_type()->file()) {
    // Enum name is already in "storage_type".
    const string& name = variable("storage_type");
    fwd_decls->insert("GPB_ENUM_FWD_DECLARE(" + name + ")");
  }
}

RepeatedEnumFieldGenerator::RepeatedEnumFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : RepeatedFieldGenerator(descriptor, options) {
  SetEnumVariables(descriptor, &variables_);
  variables_["array_storage_type"] = "GPBEnumArray";
}

RepeatedEnumFieldGenerator::~RepeatedEnumFieldGenerator() {}

void RepeatedEnumFieldGenerator::FinishInitialization(void) {
  RepeatedFieldGenerator::FinishInitialization();
  variables_["array_comment"] =
      "// |" + variables_["name"] + "| contains |" + variables_["storage_type"] + "|\n";
}

}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
