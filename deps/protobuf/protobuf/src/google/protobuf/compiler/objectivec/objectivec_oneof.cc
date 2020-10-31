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

#include <google/protobuf/compiler/objectivec/objectivec_oneof.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

OneofGenerator::OneofGenerator(const OneofDescriptor* descriptor)
    : descriptor_(descriptor) {
  variables_["enum_name"] = OneofEnumName(descriptor_);
  variables_["name"] = OneofName(descriptor_);
  variables_["capitalized_name"] = OneofNameCapitalized(descriptor_);
  variables_["raw_index"] = StrCat(descriptor_->index());
  const Descriptor* msg_descriptor = descriptor_->containing_type();
  variables_["owning_message_class"] = ClassName(msg_descriptor);

  string comments;
  SourceLocation location;
  if (descriptor_->GetSourceLocation(&location)) {
    comments = BuildCommentsString(location, true);
  } else {
    comments = "";
  }
  variables_["comments"] = comments;
}

OneofGenerator::~OneofGenerator() {}

void OneofGenerator::SetOneofIndexBase(int index_base) {
  int index = descriptor_->index() + index_base;
  // Flip the sign to mark it as a oneof.
  variables_["index"] = StrCat(-index);
}

void OneofGenerator::GenerateCaseEnum(io::Printer* printer) {
  printer->Print(
      variables_,
      "typedef GPB_ENUM($enum_name$) {\n");
  printer->Indent();
  printer->Print(
      variables_,
      "$enum_name$_GPBUnsetOneOfCase = 0,\n");
  string enum_name = variables_["enum_name"];
  for (int j = 0; j < descriptor_->field_count(); j++) {
    const FieldDescriptor* field = descriptor_->field(j);
    string field_name = FieldNameCapitalized(field);
    printer->Print(
        "$enum_name$_$field_name$ = $field_number$,\n",
        "enum_name", enum_name,
        "field_name", field_name,
        "field_number", StrCat(field->number()));
  }
  printer->Outdent();
  printer->Print(
      "};\n"
      "\n");
}

void OneofGenerator::GeneratePublicCasePropertyDeclaration(
    io::Printer* printer) {
  printer->Print(
      variables_,
      "$comments$"
      "@property(nonatomic, readonly) $enum_name$ $name$OneOfCase;\n"
      "\n");
}

void OneofGenerator::GenerateClearFunctionDeclaration(io::Printer* printer) {
  printer->Print(
      variables_,
      "/**\n"
      " * Clears whatever value was set for the oneof '$name$'.\n"
      " **/\n"
      "void $owning_message_class$_Clear$capitalized_name$OneOfCase($owning_message_class$ *message);\n");
}

void OneofGenerator::GeneratePropertyImplementation(io::Printer* printer) {
  printer->Print(
      variables_,
      "@dynamic $name$OneOfCase;\n");
}

void OneofGenerator::GenerateClearFunctionImplementation(io::Printer* printer) {
  printer->Print(
      variables_,
      "void $owning_message_class$_Clear$capitalized_name$OneOfCase($owning_message_class$ *message) {\n"
      "  GPBDescriptor *descriptor = [message descriptor];\n"
      "  GPBOneofDescriptor *oneof = [descriptor.oneofs objectAtIndex:$raw_index$];\n"
      "  GPBMaybeClearOneof(message, oneof, $index$, 0);\n"
      "}\n");
}

string OneofGenerator::DescriptorName(void) const {
  return variables_.find("name")->second;
}

string OneofGenerator::HasIndexAsString(void) const {
  return variables_.find("index")->second;
}

}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
