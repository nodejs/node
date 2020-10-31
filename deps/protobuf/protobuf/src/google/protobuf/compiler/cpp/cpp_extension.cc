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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/compiler/cpp/cpp_extension.h>
#include <map>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>



namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

// Returns the fully-qualified class name of the message that this field
// extends. This function is used in the Google-internal code to handle some
// legacy cases.
std::string ExtendeeClassName(const FieldDescriptor* descriptor) {
  const Descriptor* extendee = descriptor->containing_type();
  return ClassName(extendee, true);
}

}  // anonymous namespace

ExtensionGenerator::ExtensionGenerator(const FieldDescriptor* descriptor,
                                       const Options& options)
    : descriptor_(descriptor), options_(options) {
  // Construct type_traits_.
  if (descriptor_->is_repeated()) {
    type_traits_ = "Repeated";
  }

  switch (descriptor_->cpp_type()) {
    case FieldDescriptor::CPPTYPE_ENUM:
      type_traits_.append("EnumTypeTraits< ");
      type_traits_.append(ClassName(descriptor_->enum_type(), true));
      type_traits_.append(", ");
      type_traits_.append(ClassName(descriptor_->enum_type(), true));
      type_traits_.append("_IsValid>");
      break;
    case FieldDescriptor::CPPTYPE_STRING:
      type_traits_.append("StringTypeTraits");
      break;
    case FieldDescriptor::CPPTYPE_MESSAGE:
      type_traits_.append("MessageTypeTraits< ");
      type_traits_.append(ClassName(descriptor_->message_type(), true));
      type_traits_.append(" >");
      break;
    default:
      type_traits_.append("PrimitiveTypeTraits< ");
      type_traits_.append(PrimitiveTypeName(options_, descriptor_->cpp_type()));
      type_traits_.append(" >");
      break;
  }
  SetCommonVars(options, &variables_);
  variables_["extendee"] = ExtendeeClassName(descriptor_);
  variables_["type_traits"] = type_traits_;
  std::string name = descriptor_->name();
  variables_["name"] = ResolveKeyword(name);
  variables_["constant_name"] = FieldConstantName(descriptor_);
  variables_["field_type"] =
      StrCat(static_cast<int>(descriptor_->type()));
  variables_["packed"] = descriptor_->options().packed() ? "true" : "false";

  std::string scope =
      IsScoped() ? ClassName(descriptor_->extension_scope(), false) + "::" : "";
  variables_["scope"] = scope;
  std::string scoped_name = scope + ResolveKeyword(name);
  variables_["scoped_name"] = scoped_name;
  variables_["number"] = StrCat(descriptor_->number());
}

ExtensionGenerator::~ExtensionGenerator() {}

bool ExtensionGenerator::IsScoped() const {
  return descriptor_->extension_scope() != nullptr;
}

void ExtensionGenerator::GenerateDeclaration(io::Printer* printer) const {
  Formatter format(printer, variables_);

  // If this is a class member, it needs to be declared "static".  Otherwise,
  // it needs to be "extern".  In the latter case, it also needs the DLL
  // export/import specifier.
  std::string qualifier;
  if (!IsScoped()) {
    qualifier = "extern";
    if (!options_.dllexport_decl.empty()) {
      qualifier = options_.dllexport_decl + " " + qualifier;
    }
  } else {
    qualifier = "static";
  }

  format(
      "static const int $constant_name$ = $number$;\n"
      "$1$ ::$proto_ns$::internal::ExtensionIdentifier< $extendee$,\n"
      "    ::$proto_ns$::internal::$type_traits$, $field_type$, $packed$ >\n"
      "  ${2$$name$$}$;\n",
      qualifier, descriptor_);
}

void ExtensionGenerator::GenerateDefinition(io::Printer* printer) {
  // If we are building for lite with implicit weak fields, we want to skip over
  // any custom options (i.e. extensions of messages from descriptor.proto).
  // This prevents the creation of any unnecessary linker references to the
  // descriptor messages.
  if (options_.lite_implicit_weak_fields &&
      descriptor_->containing_type()->file()->name() ==
          "net/proto2/proto/descriptor.proto") {
    return;
  }

  Formatter format(printer, variables_);
  std::string default_str;
  // If this is a class member, it needs to be declared in its class scope.
  if (descriptor_->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
    // We need to declare a global string which will contain the default value.
    // We cannot declare it at class scope because that would require exposing
    // it in the header which would be annoying for other reasons.  So we
    // replace :: with _ in the name and declare it as a global.
    default_str =
        StringReplace(variables_["scoped_name"], "::", "_", true) + "_default";
    format("const std::string $1$($2$);\n", default_str,
           DefaultValue(options_, descriptor_));
  } else {
    default_str = DefaultValue(options_, descriptor_);
  }

  // Likewise, class members need to declare the field constant variable.
  if (IsScoped()) {
    format(
        "#if !defined(_MSC_VER) || _MSC_VER >= 1900\n"
        "const int $scope$$constant_name$;\n"
        "#endif\n");
  }

  format(
      "::$proto_ns$::internal::ExtensionIdentifier< $extendee$,\n"
      "    ::$proto_ns$::internal::$type_traits$, $field_type$, $packed$ >\n"
      "  $scoped_name$($constant_name$, $1$);\n",
      default_str);
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
