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

#include <google/protobuf/compiler/java/java_extension.h>

#include <google/protobuf/compiler/java/java_context.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace java {

ImmutableExtensionGenerator::ImmutableExtensionGenerator(
    const FieldDescriptor* descriptor, Context* context)
    : descriptor_(descriptor), name_resolver_(context->GetNameResolver()) {
  if (descriptor_->extension_scope() != NULL) {
    scope_ =
        name_resolver_->GetImmutableClassName(descriptor_->extension_scope());
  } else {
    scope_ = name_resolver_->GetImmutableClassName(descriptor_->file());
  }
}

ImmutableExtensionGenerator::~ImmutableExtensionGenerator() {}

// Initializes the vars referenced in the generated code templates.
void ExtensionGenerator::InitTemplateVars(
    const FieldDescriptor* descriptor, const std::string& scope, bool immutable,
    ClassNameResolver* name_resolver,
    std::map<std::string, std::string>* vars_pointer) {
  std::map<std::string, std::string>& vars = *vars_pointer;
  vars["scope"] = scope;
  vars["name"] = UnderscoresToCamelCaseCheckReserved(descriptor);
  vars["containing_type"] =
      name_resolver->GetClassName(descriptor->containing_type(), immutable);
  vars["number"] = StrCat(descriptor->number());
  vars["constant_name"] = FieldConstantName(descriptor);
  vars["index"] = StrCat(descriptor->index());
  vars["default"] = descriptor->is_repeated()
                        ? ""
                        : DefaultValue(descriptor, immutable, name_resolver);
  vars["type_constant"] = FieldTypeName(GetType(descriptor));
  vars["packed"] = descriptor->is_packed() ? "true" : "false";
  vars["enum_map"] = "null";
  vars["prototype"] = "null";

  JavaType java_type = GetJavaType(descriptor);
  std::string singular_type;
  switch (java_type) {
    case JAVATYPE_MESSAGE:
      singular_type =
          name_resolver->GetClassName(descriptor->message_type(), immutable);
      vars["prototype"] = singular_type + ".getDefaultInstance()";
      break;
    case JAVATYPE_ENUM:
      singular_type =
          name_resolver->GetClassName(descriptor->enum_type(), immutable);
      vars["enum_map"] = singular_type + ".internalGetValueMap()";
      break;
    case JAVATYPE_STRING:
      singular_type = "java.lang.String";
      break;
    case JAVATYPE_BYTES:
      singular_type = immutable ? "com.google.protobuf.ByteString" : "byte[]";
      break;
    default:
      singular_type = BoxedPrimitiveTypeName(java_type);
      break;
  }
  vars["type"] = descriptor->is_repeated()
                     ? "java.util.List<" + singular_type + ">"
                     : singular_type;
  vars["singular_type"] = singular_type;
}

void ImmutableExtensionGenerator::Generate(io::Printer* printer) {
  std::map<std::string, std::string> vars;
  const bool kUseImmutableNames = true;
  InitTemplateVars(descriptor_, scope_, kUseImmutableNames, name_resolver_,
                   &vars);
  printer->Print(vars, "public static final int $constant_name$ = $number$;\n");

  WriteFieldDocComment(printer, descriptor_);
  if (descriptor_->extension_scope() == NULL) {
    // Non-nested
    printer->Print(
        vars,
        "public static final\n"
        "  com.google.protobuf.GeneratedMessage.GeneratedExtension<\n"
        "    $containing_type$,\n"
        "    $type$> $name$ = com.google.protobuf.GeneratedMessage\n"
        "        .newFileScopedGeneratedExtension(\n"
        "      $singular_type$.class,\n"
        "      $prototype$);\n");
  } else {
    // Nested
    printer->Print(
        vars,
        "public static final\n"
        "  com.google.protobuf.GeneratedMessage.GeneratedExtension<\n"
        "    $containing_type$,\n"
        "    $type$> $name$ = com.google.protobuf.GeneratedMessage\n"
        "        .newMessageScopedGeneratedExtension(\n"
        "      $scope$.getDefaultInstance(),\n"
        "      $index$,\n"
        "      $singular_type$.class,\n"
        "      $prototype$);\n");
  }
  printer->Annotate("name", descriptor_);
}

int ImmutableExtensionGenerator::GenerateNonNestedInitializationCode(
    io::Printer* printer) {
  int bytecode_estimate = 0;
  if (descriptor_->extension_scope() == NULL) {
    // Only applies to non-nested extensions.
    printer->Print(
        "$name$.internalInit(descriptor.getExtensions().get($index$));\n",
        "name", UnderscoresToCamelCaseCheckReserved(descriptor_), "index",
        StrCat(descriptor_->index()));
    bytecode_estimate += 21;
  }
  return bytecode_estimate;
}

int ImmutableExtensionGenerator::GenerateRegistrationCode(
    io::Printer* printer) {
  printer->Print("registry.add($scope$.$name$);\n", "scope", scope_, "name",
                 UnderscoresToCamelCaseCheckReserved(descriptor_));
  return 7;
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
