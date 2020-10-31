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

#include <map>
#include <string>

#include <google/protobuf/compiler/java/java_context.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_enum.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace java {

EnumGenerator::EnumGenerator(const EnumDescriptor* descriptor,
                             bool immutable_api, Context* context)
    : descriptor_(descriptor),
      immutable_api_(immutable_api),
      context_(context),
      name_resolver_(context->GetNameResolver()) {
  for (int i = 0; i < descriptor_->value_count(); i++) {
    const EnumValueDescriptor* value = descriptor_->value(i);
    const EnumValueDescriptor* canonical_value =
        descriptor_->FindValueByNumber(value->number());

    if (value == canonical_value) {
      canonical_values_.push_back(value);
    } else {
      Alias alias;
      alias.value = value;
      alias.canonical_value = canonical_value;
      aliases_.push_back(alias);
    }
  }
}

EnumGenerator::~EnumGenerator() {}

void EnumGenerator::Generate(io::Printer* printer) {
  WriteEnumDocComment(printer, descriptor_);
  MaybePrintGeneratedAnnotation(context_, printer, descriptor_, immutable_api_);
  printer->Print(
      "public enum $classname$\n"
      "    implements com.google.protobuf.ProtocolMessageEnum {\n",
      "classname", descriptor_->name());
  printer->Annotate("classname", descriptor_);
  printer->Indent();

  bool ordinal_is_index = true;
  std::string index_text = "ordinal()";
  for (int i = 0; i < canonical_values_.size(); i++) {
    if (canonical_values_[i]->index() != i) {
      ordinal_is_index = false;
      index_text = "index";
      break;
    }
  }

  for (int i = 0; i < canonical_values_.size(); i++) {
    std::map<std::string, std::string> vars;
    vars["name"] = canonical_values_[i]->name();
    vars["index"] = StrCat(canonical_values_[i]->index());
    vars["number"] = StrCat(canonical_values_[i]->number());
    WriteEnumValueDocComment(printer, canonical_values_[i]);
    if (canonical_values_[i]->options().deprecated()) {
      printer->Print("@java.lang.Deprecated\n");
    }
    if (ordinal_is_index) {
      printer->Print(vars, "$name$($number$),\n");
    } else {
      printer->Print(vars, "$name$($index$, $number$),\n");
    }
    printer->Annotate("name", canonical_values_[i]);
  }

  if (SupportUnknownEnumValue(descriptor_->file())) {
    if (ordinal_is_index) {
      printer->Print("${$UNRECOGNIZED$}$(-1),\n", "{", "", "}", "");
    } else {
      printer->Print("${$UNRECOGNIZED$}$(-1, -1),\n", "{", "", "}", "");
    }
    printer->Annotate("{", "}", descriptor_);
  }

  printer->Print(
      ";\n"
      "\n");

  // -----------------------------------------------------------------

  for (int i = 0; i < aliases_.size(); i++) {
    std::map<std::string, std::string> vars;
    vars["classname"] = descriptor_->name();
    vars["name"] = aliases_[i].value->name();
    vars["canonical_name"] = aliases_[i].canonical_value->name();
    WriteEnumValueDocComment(printer, aliases_[i].value);
    printer->Print(
        vars, "public static final $classname$ $name$ = $canonical_name$;\n");
    printer->Annotate("name", aliases_[i].value);
  }

  for (int i = 0; i < descriptor_->value_count(); i++) {
    std::map<std::string, std::string> vars;
    vars["name"] = descriptor_->value(i)->name();
    vars["number"] = StrCat(descriptor_->value(i)->number());
    vars["{"] = "";
    vars["}"] = "";
    WriteEnumValueDocComment(printer, descriptor_->value(i));
    printer->Print(vars,
                   "public static final int ${$$name$_VALUE$}$ = $number$;\n");
    printer->Annotate("{", "}", descriptor_->value(i));
  }
  printer->Print("\n");

  // -----------------------------------------------------------------

  printer->Print(
      "\n"
      "public final int getNumber() {\n");
  if (SupportUnknownEnumValue(descriptor_->file())) {
    if (ordinal_is_index) {
      printer->Print(
          "  if (this == UNRECOGNIZED) {\n"
          "    throw new java.lang.IllegalArgumentException(\n"
          "        \"Can't get the number of an unknown enum value.\");\n"
          "  }\n");
    } else {
      printer->Print(
          "  if (index == -1) {\n"
          "    throw new java.lang.IllegalArgumentException(\n"
          "        \"Can't get the number of an unknown enum value.\");\n"
          "  }\n");
    }
  }
  printer->Print(
      "  return value;\n"
      "}\n"
      "\n"
      "/**\n"
      " * @deprecated Use {@link #forNumber(int)} instead.\n"
      " */\n"
      "@java.lang.Deprecated\n"
      "public static $classname$ valueOf(int value) {\n"
      "  return forNumber(value);\n"
      "}\n"
      "\n"
      "public static $classname$ forNumber(int value) {\n"
      "  switch (value) {\n",
      "classname", descriptor_->name());
  printer->Indent();
  printer->Indent();

  for (int i = 0; i < canonical_values_.size(); i++) {
    printer->Print("case $number$: return $name$;\n", "name",
                   canonical_values_[i]->name(), "number",
                   StrCat(canonical_values_[i]->number()));
  }

  printer->Outdent();
  printer->Outdent();
  printer->Print(
      "    default: return null;\n"
      "  }\n"
      "}\n"
      "\n"
      "public static com.google.protobuf.Internal.EnumLiteMap<$classname$>\n"
      "    internalGetValueMap() {\n"
      "  return internalValueMap;\n"
      "}\n"
      "private static final com.google.protobuf.Internal.EnumLiteMap<\n"
      "    $classname$> internalValueMap =\n"
      "      new com.google.protobuf.Internal.EnumLiteMap<$classname$>() {\n"
      "        public $classname$ findValueByNumber(int number) {\n"
      "          return $classname$.forNumber(number);\n"
      "        }\n"
      "      };\n"
      "\n",
      "classname", descriptor_->name());

  // -----------------------------------------------------------------
  // Reflection

  if (HasDescriptorMethods(descriptor_, context_->EnforceLite())) {
    printer->Print(
        "public final com.google.protobuf.Descriptors.EnumValueDescriptor\n"
        "    getValueDescriptor() {\n"
        "  return getDescriptor().getValues().get($index_text$);\n"
        "}\n"
        "public final com.google.protobuf.Descriptors.EnumDescriptor\n"
        "    getDescriptorForType() {\n"
        "  return getDescriptor();\n"
        "}\n"
        "public static final com.google.protobuf.Descriptors.EnumDescriptor\n"
        "    getDescriptor() {\n",
        "index_text", index_text);

    // TODO(kenton):  Cache statically?  Note that we can't access descriptors
    //   at module init time because it wouldn't work with descriptor.proto, but
    //   we can cache the value the first time getDescriptor() is called.
    if (descriptor_->containing_type() == NULL) {
      // The class generated for the File fully populates the descriptor with
      // extensions in both the mutable and immutable cases. (In the mutable api
      // this is accomplished by attempting to load the immutable outer class).
      printer->Print(
          "  return $file$.getDescriptor().getEnumTypes().get($index$);\n",
          "file",
          name_resolver_->GetClassName(descriptor_->file(), immutable_api_),
          "index", StrCat(descriptor_->index()));
    } else {
      printer->Print(
          "  return $parent$.$descriptor$.getEnumTypes().get($index$);\n",
          "parent",
          name_resolver_->GetClassName(descriptor_->containing_type(),
                                       immutable_api_),
          "descriptor",
          descriptor_->containing_type()
                  ->options()
                  .no_standard_descriptor_accessor()
              ? "getDefaultInstance().getDescriptorForType()"
              : "getDescriptor()",
          "index", StrCat(descriptor_->index()));
    }

    printer->Print(
        "}\n"
        "\n"
        "private static final $classname$[] VALUES = ",
        "classname", descriptor_->name());

    if (CanUseEnumValues()) {
      // If the constants we are going to output are exactly the ones we
      // have declared in the Java enum in the same order, then we can use
      // the values() method that the Java compiler automatically generates
      // for every enum.
      printer->Print("values();\n");
    } else {
      printer->Print(
          "{\n"
          "  ");
      for (int i = 0; i < descriptor_->value_count(); i++) {
        printer->Print("$name$, ", "name", descriptor_->value(i)->name());
      }
      printer->Print(
          "\n"
          "};\n");
    }

    printer->Print(
        "\n"
        "public static $classname$ valueOf(\n"
        "    com.google.protobuf.Descriptors.EnumValueDescriptor desc) {\n"
        "  if (desc.getType() != getDescriptor()) {\n"
        "    throw new java.lang.IllegalArgumentException(\n"
        "      \"EnumValueDescriptor is not for this type.\");\n"
        "  }\n",
        "classname", descriptor_->name());
    if (SupportUnknownEnumValue(descriptor_->file())) {
      printer->Print(
          "  if (desc.getIndex() == -1) {\n"
          "    return UNRECOGNIZED;\n"
          "  }\n");
    }
    printer->Print(
        "  return VALUES[desc.getIndex()];\n"
        "}\n"
        "\n");

    if (!ordinal_is_index) {
      printer->Print("private final int index;\n");
    }
  }

  // -----------------------------------------------------------------

  printer->Print("private final int value;\n\n");

  if (ordinal_is_index) {
    printer->Print("private $classname$(int value) {\n", "classname",
                   descriptor_->name());
  } else {
    printer->Print("private $classname$(int index, int value) {\n", "classname",
                   descriptor_->name());
  }
  if (HasDescriptorMethods(descriptor_, context_->EnforceLite()) &&
      !ordinal_is_index) {
    printer->Print("  this.index = index;\n");
  }
  printer->Print(
      "  this.value = value;\n"
      "}\n");

  printer->Print(
      "\n"
      "// @@protoc_insertion_point(enum_scope:$full_name$)\n",
      "full_name", descriptor_->full_name());

  printer->Outdent();
  printer->Print("}\n\n");
}

bool EnumGenerator::CanUseEnumValues() {
  if (canonical_values_.size() != descriptor_->value_count()) {
    return false;
  }
  for (int i = 0; i < descriptor_->value_count(); i++) {
    if (descriptor_->value(i)->name() != canonical_values_[i]->name()) {
      return false;
    }
  }
  return true;
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
