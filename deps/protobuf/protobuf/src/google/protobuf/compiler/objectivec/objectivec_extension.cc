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

#include <iostream>

#include <google/protobuf/compiler/objectivec/objectivec_extension.h>
#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/io/printer.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

ExtensionGenerator::ExtensionGenerator(const string& root_class_name,
                                       const FieldDescriptor* descriptor)
    : method_name_(ExtensionMethodName(descriptor)),
      root_class_and_method_name_(root_class_name + "_" + method_name_),
      descriptor_(descriptor) {
  if (descriptor->is_map()) {
    // NOTE: src/google/protobuf/compiler/plugin.cc makes use of cerr for some
    // error cases, so it seems to be ok to use as a back door for errors.
    std::cerr << "error: Extension is a map<>!"
         << " That used to be blocked by the compiler." << std::endl;
    std::cerr.flush();
    abort();
  }
}

ExtensionGenerator::~ExtensionGenerator() {}

void ExtensionGenerator::GenerateMembersHeader(io::Printer* printer) {
  std::map<string, string> vars;
  vars["method_name"] = method_name_;
  if (IsRetainedName(method_name_)) {
    vars["storage_attribute"] = " NS_RETURNS_NOT_RETAINED";
  } else {
    vars["storage_attribute"] = "";
  }
  SourceLocation location;
  if (descriptor_->GetSourceLocation(&location)) {
    vars["comments"] = BuildCommentsString(location, true);
  } else {
    vars["comments"] = "";
  }
  // Unlike normal message fields, check if the file for the extension was
  // deprecated.
  vars["deprecated_attribute"] = GetOptionalDeprecatedAttribute(descriptor_, descriptor_->file());
  printer->Print(vars,
                 "$comments$"
                 "+ (GPBExtensionDescriptor *)$method_name$$storage_attribute$$deprecated_attribute$;\n");
}

void ExtensionGenerator::GenerateStaticVariablesInitialization(
    io::Printer* printer) {
  std::map<string, string> vars;
  vars["root_class_and_method_name"] = root_class_and_method_name_;
  vars["extended_type"] = ClassName(descriptor_->containing_type());
  vars["number"] = StrCat(descriptor_->number());

  std::vector<string> options;
  if (descriptor_->is_repeated()) options.push_back("GPBExtensionRepeated");
  if (descriptor_->is_packed()) options.push_back("GPBExtensionPacked");
  if (descriptor_->containing_type()->options().message_set_wire_format())
    options.push_back("GPBExtensionSetWireFormat");

  vars["options"] = BuildFlagsString(FLAGTYPE_EXTENSION, options);

  ObjectiveCType objc_type = GetObjectiveCType(descriptor_);
  string singular_type;
  if (objc_type == OBJECTIVECTYPE_MESSAGE) {
    vars["type"] = string("GPBStringifySymbol(") +
                   ClassName(descriptor_->message_type()) + ")";
  } else {
    vars["type"] = "NULL";
  }

  vars["default_name"] = GPBGenericValueFieldName(descriptor_);
  if (descriptor_->is_repeated()) {
    vars["default"] = "nil";
  } else {
    vars["default"] = DefaultValue(descriptor_);
  }
  string type = GetCapitalizedType(descriptor_);
  vars["extension_type"] = string("GPBDataType") + type;

  if (objc_type == OBJECTIVECTYPE_ENUM) {
    vars["enum_desc_func_name"] =
         EnumName(descriptor_->enum_type()) + "_EnumDescriptor";
  } else {
    vars["enum_desc_func_name"] = "NULL";
  }

  printer->Print(vars,
                 "{\n"
                 "  .defaultValue.$default_name$ = $default$,\n"
                 "  .singletonName = GPBStringifySymbol($root_class_and_method_name$),\n"
                 "  .extendedClass = GPBStringifySymbol($extended_type$),\n"
                 "  .messageOrGroupClassName = $type$,\n"
                 "  .enumDescriptorFunc = $enum_desc_func_name$,\n"
                 "  .fieldNumber = $number$,\n"
                 "  .dataType = $extension_type$,\n"
                 "  .options = $options$,\n"
                 "},\n");
}

void ExtensionGenerator::GenerateRegistrationSource(io::Printer* printer) {
  printer->Print(
      "[registry addExtension:$root_class_and_method_name$];\n",
      "root_class_and_method_name", root_class_and_method_name_);
}
}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
